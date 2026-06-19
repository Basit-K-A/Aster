#include "compiler.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_LOCALS 256

/* Local variable slot tracked during compilation */
typedef struct {
    char* name;
    int depth;
} Local;

/* Extended compiler state with scope and local variable tracking */
typedef struct {
    Chunk* chunk;
    bool hadError;
    Local locals[MAX_LOCALS];
    int localCount;
    int scopeDepth;
} CompilerState;

/* Reports a compile error at the given source line */
static void compileError(CompilerState* c, int line, const char* message) {
    if (c->hadError) return;
    c->hadError = true;
    fprintf(stderr, "[line %d] Compile error: %s\n", line, message);
}

/* Appends a single bytecode byte with line information */
static void emitByte(CompilerState* c, uint8_t byte, int line) {
    writeChunk(c->chunk, byte, line);
}

/* Appends two bytecode bytes with line information */
static void emitBytes(CompilerState* c, uint8_t b1, uint8_t b2, int line) {
    emitByte(c, b1, line);
    emitByte(c, b2, line);
}

/* Adds a constant to the chunk pool and emits OP_CONST with its index */
static void emitConstant(CompilerState* c, Value value, int line) {
    int constant = addConstant(c->chunk, valueCopy(value));
    if (constant > 255) {
        compileError(c, line, "Too many constants in one chunk.");
        return;
    }
    emitBytes(c, OP_CONST, (uint8_t)constant, line);
}

/* Adds an identifier name to the constant pool and returns its index */
static int identifierConstant(CompilerState* c, const char* name, int line) {
    int constant = addConstant(c->chunk, valueStringOwned(strdup(name)));
    if (constant > 255) {
        compileError(c, line, "Too many constants in one chunk.");
    }
    return constant;
}

/* Emits a jump instruction with a placeholder two-byte offset */
static int emitJump(CompilerState* c, OpCode instruction, int line) {
    emitByte(c, (uint8_t)instruction, line);
    emitByte(c, 0xff, line);
    emitByte(c, 0xff, line);
    return c->chunk->count - 2;
}

/* Patches a previously emitted jump to target the current code position */
static void patchJump(CompilerState* c, int offset) {
    int jump = c->chunk->count - offset - 2;
    if (jump < 0 || jump > UINT16_MAX) {
        compileError(c, 0, "Too much code to jump over.");
        return;
    }
    c->chunk->code[offset] = (uint8_t)((jump >> 8) & 0xff);
    c->chunk->code[offset + 1] = (uint8_t)(jump & 0xff);
}

/* Emits a backward loop jump to a previously recorded loop start */
static void emitLoop(CompilerState* c, int loopStart, int line) {
    emitByte(c, OP_LOOP, line);
    int offset = c->chunk->count - loopStart + 2;
    if (offset > UINT16_MAX) {
        compileError(c, line, "Loop body too large.");
        return;
    }
    emitByte(c, (uint8_t)((offset >> 8) & 0xff), line);
    emitByte(c, (uint8_t)(offset & 0xff), line);
}

/* Enters a new lexical scope during compilation */
static void beginScope(CompilerState* c) {
    c->scopeDepth++;
}

/* Exits the current scope and discards locals declared within it */
static void endScope(CompilerState* c) {
    c->scopeDepth--;
    while (c->localCount > 0 && c->locals[c->localCount - 1].depth > c->scopeDepth) {
        free(c->locals[c->localCount - 1].name);
        c->localCount--;
    }
}

/* Registers a new local variable in the current scope */
static void addLocal(CompilerState* c, const char* name, int line) {
    if (c->localCount >= MAX_LOCALS) {
        compileError(c, line, "Too many local variables.");
        return;
    }
    c->locals[c->localCount].name = strdup(name);
    c->locals[c->localCount].depth = c->scopeDepth;
    c->localCount++;
}

/* Resolves a local variable by name, returning its slot index or -1 */
static int resolveLocal(CompilerState* c, const char* name) {
    for (int i = c->localCount - 1; i >= 0; i--) {
        if (strcmp(c->locals[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

static void compileExpression(AstNode* node, CompilerState* c);
static void compileStatement(AstNode* node, CompilerState* c);

/* Compiles a binary AST node to stack-based arithmetic/logic opcodes */
static void compileBinary(AstNode* node, CompilerState* c) {
    if (node->as.binary.op == BINOP_AND) {
        compileExpression(node->as.binary.left, c);
        int endJump = emitJump(c, OP_JUMP_IF_FALSE, node->line);
        emitByte(c, OP_POP, node->line);
        compileExpression(node->as.binary.right, c);
        patchJump(c, endJump);
        return;
    }

    if (node->as.binary.op == BINOP_OR) {
        compileExpression(node->as.binary.left, c);
        int elseJump = emitJump(c, OP_JUMP_IF_FALSE, node->line);
        int endJump = emitJump(c, OP_JUMP, node->line);
        patchJump(c, elseJump);
        emitByte(c, OP_POP, node->line);
        compileExpression(node->as.binary.right, c);
        patchJump(c, endJump);
        return;
    }

    compileExpression(node->as.binary.left, c);
    compileExpression(node->as.binary.right, c);

    OpCode op = OP_ADD;
    switch (node->as.binary.op) {
        case BINOP_ADD: op = OP_ADD; break;
        case BINOP_SUB: op = OP_SUB; break;
        case BINOP_MUL: op = OP_MUL; break;
        case BINOP_DIV: op = OP_DIV; break;
        case BINOP_MOD: op = OP_MOD; break;
        case BINOP_EQ:  op = OP_EQ; break;
        case BINOP_NEQ: op = OP_NEQ; break;
        case BINOP_LT:  op = OP_LT; break;
        case BINOP_LTE: op = OP_LTE; break;
        case BINOP_GT:  op = OP_GT; break;
        case BINOP_GTE: op = OP_GTE; break;
        default: break;
    }
    emitByte(c, (uint8_t)op, node->line);
}

/* Compiles an expression AST node to bytecode */
static void compileExpression(AstNode* node, CompilerState* c) {
    if (!node || c->hadError) return;

    switch (node->type) {
        case NODE_NUMBER:
            emitConstant(c, valueNumber(node->as.number), node->line);
            break;
        case NODE_STRING:
            emitConstant(c, valueStringOwned(strdup(node->as.string)), node->line);
            break;
        case NODE_BOOL:
            emitByte(c, node->as.boolean ? OP_TRUE : OP_FALSE, node->line);
            break;
        case NODE_NULL:
            emitByte(c, OP_NULL, node->line);
            break;
        case NODE_IDENTIFIER: {
            int local = resolveLocal(c, node->as.name);
            if (local != -1) {
                emitBytes(c, OP_GET_LOCAL, (uint8_t)local, node->line);
            } else {
                int global = identifierConstant(c, node->as.name, node->line);
                emitBytes(c, OP_GET_GLOBAL, (uint8_t)global, node->line);
            }
            break;
        }
        case NODE_BINARY:
            compileBinary(node, c);
            break;
        case NODE_UNARY:
            compileExpression(node->as.unary.operand, c);
            emitByte(c, node->as.unary.op == UNOP_NEG ? OP_NEG : OP_NOT, node->line);
            break;
        case NODE_ASSIGNMENT: {
            compileExpression(node->as.assignment.value, c);
            int local = resolveLocal(c, node->as.assignment.name);
            if (local != -1) {
                emitBytes(c, OP_SET_LOCAL, (uint8_t)local, node->line);
            } else {
                int global = identifierConstant(c, node->as.assignment.name, node->line);
                emitBytes(c, OP_SET_GLOBAL, (uint8_t)global, node->line);
            }
            break;
        }
        case NODE_CALL:
            for (int i = 0; i < node->as.call.argCount; i++) {
                compileExpression(node->as.call.args[i], c);
            }
            compileExpression(node->as.call.callee, c);
            emitBytes(c, OP_CALL, (uint8_t)node->as.call.argCount, node->line);
            break;
        default:
            compileError(c, node->line, "Invalid expression in compiler.");
            break;
    }
}

/* Compiles a block AST node, optionally creating a new local scope */
static void compileBlock(AstNode* node, CompilerState* c, bool createScope) {
    if (!node || node->type != NODE_BLOCK) return;

    if (createScope) beginScope(c);

    for (int i = 0; i < node->as.block.count; i++) {
        compileStatement(node->as.block.statements[i], c);
        if (c->hadError) break;
    }

    if (createScope) endScope(c);
}

/* Compiles a statement AST node to bytecode */
static void compileStatement(AstNode* node, CompilerState* c) {
    if (!node || c->hadError) return;

    switch (node->type) {
        case NODE_VAR_DECL:
            compileExpression(node->as.varDecl.initializer, c);
            if (c->scopeDepth == 0) {
                int global = identifierConstant(c, node->as.varDecl.name, node->line);
                emitBytes(c, OP_DEF_GLOBAL, (uint8_t)global, node->line);
            } else {
                addLocal(c, node->as.varDecl.name, node->line);
                emitBytes(c, OP_DEF_LOCAL, (uint8_t)(c->localCount - 1), node->line);
            }
            break;
        case NODE_FUNCTION_DECL:
            compileError(c, node->line, "Function compilation deferred to Phase 6.");
            break;
        case NODE_PRINT:
            compileExpression(node->as.printStmt.value, c);
            emitByte(c, OP_PRINT, node->line);
            break;
        case NODE_RETURN:
            if (node->as.returnStmt.value) {
                compileExpression(node->as.returnStmt.value, c);
            } else {
                emitByte(c, OP_NULL, node->line);
            }
            emitByte(c, OP_RETURN, node->line);
            break;
        case NODE_IF: {
            compileExpression(node->as.ifStmt.condition, c);
            int elseJump = emitJump(c, OP_JUMP_IF_FALSE, node->line);
            compileStatement(node->as.ifStmt.thenBranch, c);
            int endJump = emitJump(c, OP_JUMP, node->line);
            patchJump(c, elseJump);
            if (node->as.ifStmt.elseBranch) {
                compileStatement(node->as.ifStmt.elseBranch, c);
            }
            patchJump(c, endJump);
            break;
        }
        case NODE_WHILE: {
            int loopStart = c->chunk->count;
            compileExpression(node->as.whileStmt.condition, c);
            int exitJump = emitJump(c, OP_JUMP_IF_FALSE, node->line);
            compileStatement(node->as.whileStmt.body, c);
            emitLoop(c, loopStart, node->line);
            patchJump(c, exitJump);
            break;
        }
        case NODE_BLOCK:
            compileBlock(node, c, true);
            break;
        default:
            compileExpression(node, c);
            emitByte(c, OP_POP, node->line);
            break;
    }
}

/* Compiles a program AST root and appends OP_HALT */
void compileProgram(AstNode* program, CompilerState* c) {
    if (!program || program->type != NODE_BLOCK) {
        compileError(c, 0, "Expected program block.");
        return;
    }

    compileBlock(program, c, false);
    emitByte(c, OP_HALT, program->line);
}

/* Compiles an AST node into bytecode in the given compiler's chunk */
void compile(AstNode* node, Compiler* compiler) {
    CompilerState state;
    state.chunk = compiler->chunk;
    state.hadError = compiler->hadError;
    state.localCount = 0;
    state.scopeDepth = 0;

    if (node && node->type == NODE_BLOCK) {
        compileProgram(node, &state);
    } else if (node) {
        compileStatement(node, &state);
        emitByte(&state, OP_HALT, node->line);
    }

    compiler->hadError = state.hadError;
}
