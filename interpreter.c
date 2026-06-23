#include "interpreter.h"

#include "parser.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Reports a runtime error at the given source line */
static void interpError(Interpreter* interp, int line, const char* message) {
    if (interp->hadError) return;
    interp->hadError = true;
    fprintf(stderr, "[line %d] Runtime error: %s\n", line, message);
}

static Value interpretExpr(AstNode* node, Interpreter* interp, Env* env);
static void interpretStmt(AstNode* node, Interpreter* interp, Env* env);
static void interpretBlock(AstNode* node, Interpreter* interp, Env* env, bool newScope);

/* Applies a binary operator to two evaluated operand values */
static Value applyBinary(BinOp op, Value left, Value right, int line, Interpreter* interp) {
    if (op == BINOP_ADD) {
        if (left.type == VAL_STRING || right.type == VAL_STRING) {
            char* ls = valueToAllocatedString(left);
            char* rs = valueToAllocatedString(right);
            size_t len = strlen(ls) + strlen(rs) + 1;
            char* combined = (char*)malloc(len);
            if (combined) {
                snprintf(combined, len, "%s%s", ls, rs);
            }
            free(ls);
            free(rs);
            return valueStringOwned(combined);
        }
        if (left.type == VAL_NUMBER && right.type == VAL_NUMBER) {
            return valueNumber(left.as.number + right.as.number);
        }
        interpError(interp, line, "Operands must be numbers or strings for '+'.");
        return valueNull();
    }

    if (op == BINOP_AND) {
        return valueBool(isTruthy(left) && isTruthy(right));
    }
    if (op == BINOP_OR) {
        return valueBool(isTruthy(left) || isTruthy(right));
    }

    if (left.type != VAL_NUMBER || right.type != VAL_NUMBER) {
        interpError(interp, line, "Operands must be numbers.");
        return valueNull();
    }

    double a = left.as.number;
    double b = right.as.number;

    switch (op) {
        case BINOP_SUB: return valueNumber(a - b);
        case BINOP_MUL: return valueNumber(a * b);
        case BINOP_DIV:
            if (b == 0.0) {
                interpError(interp, line, "Division by zero.");
                return valueNull();
            }
            return valueNumber(a / b);
        case BINOP_MOD:
            if (b == 0.0) {
                interpError(interp, line, "Modulo by zero.");
                return valueNull();
            }
            return valueNumber(fmod(a, b));
        case BINOP_EQ:  return valueBool(valuesEqual(left, right));
        case BINOP_NEQ: return valueBool(!valuesEqual(left, right));
        case BINOP_LT:  return valueBool(a < b);
        case BINOP_LTE: return valueBool(a <= b);
        case BINOP_GT:  return valueBool(a > b);
        case BINOP_GTE: return valueBool(a >= b);
        default:
            interpError(interp, line, "Unknown binary operator.");
            return valueNull();
    }
}

/* Applies a unary operator to an evaluated operand value */
static Value applyUnary(UnOp op, Value operand, int line, Interpreter* interp) {
    if (op == UNOP_NOT) {
        return valueBool(!isTruthy(operand));
    }

    if (operand.type != VAL_NUMBER) {
        interpError(interp, line, "Operand must be a number.");
        return valueNull();
    }

    return valueNumber(-operand.as.number);
}

/* Invokes a user-defined function with evaluated argument values */
static Value callFunction(AsterFunction* fn, Value* args, int argCount, Interpreter* interp, Env* env) {
    if (!fn) {
        interpError(interp, 0, "Called null function.");
        return valueNull();
    }

    if (argCount != fn->arity) {
        interpError(interp, 0, "Wrong number of arguments.");
        return valueNull();
    }

    Env* callEnv = envCreate(env);
    if (!callEnv) return valueNull();

    for (int i = 0; i < fn->arity; i++) {
        if (!envDefine(callEnv, fn->params[i], args[i])) {
            envFree(callEnv);
            interpError(interp, 0, "Could not bind parameter.");
            return valueNull();
        }
    }

    bool savedHasReturn = interp->hasReturn;
    Value savedReturn = savedHasReturn ? valueCopy(interp->returnValue) : valueNull();
    interp->hasReturn = false;

    interpretBlock(fn->body, interp, callEnv, false);

    Value result = interp->hasReturn ? valueCopy(interp->returnValue) : valueNull();
    if (interp->hasReturn) valueFree(interp->returnValue);
    interp->hasReturn = savedHasReturn;
    if (savedHasReturn) {
        interp->returnValue = savedReturn;
    } else {
        valueFree(savedReturn);
    }

    envFree(callEnv);
    return result;
}

/* Evaluates an expression AST node and returns the resulting value */
static Value interpretExpr(AstNode* node, Interpreter* interp, Env* env) {
    if (!node || interp->hadError) return valueNull();

    switch (node->type) {
        case NODE_NUMBER:
            return valueNumber(node->as.number);
        case NODE_STRING:
            return valueStringOwned(strdup(node->as.string));
        case NODE_BOOL:
            return valueBool(node->as.boolean);
        case NODE_NULL:
            return valueNull();
        case NODE_IDENTIFIER: {
            Value value;
            if (!envGet(env, node->as.name, &value)) {
                interpError(interp, node->line, "Undefined variable.");
                return valueNull();
            }
            return value;
        }
        case NODE_BINARY: {
            Value left = interpretExpr(node->as.binary.left, interp, env);
            Value right = interpretExpr(node->as.binary.right, interp, env);
            Value result = applyBinary(node->as.binary.op, left, right, node->line, interp);
            valueFree(left);
            valueFree(right);
            return result;
        }
        case NODE_UNARY: {
            Value operand = interpretExpr(node->as.unary.operand, interp, env);
            Value result = applyUnary(node->as.unary.op, operand, node->line, interp);
            valueFree(operand);
            return result;
        }
        case NODE_ASSIGNMENT: {
            Value value = interpretExpr(node->as.assignment.value, interp, env);
            if (!envSet(env, node->as.assignment.name, value)) {
                interpError(interp, node->line, "Undefined variable.");
                valueFree(value);
                return valueNull();
            }
            return value;
        }
        case NODE_CALL: {
            Value callee = interpretExpr(node->as.call.callee, interp, env);
            if (callee.type != VAL_FUNCTION) {
                interpError(interp, node->line, "Can only call functions.");
                valueFree(callee);
                return valueNull();
            }

            Value* args = NULL;
            if (node->as.call.argCount > 0) {
                args = (Value*)calloc((size_t)node->as.call.argCount, sizeof(Value));
                if (!args) {
                    valueFree(callee);
                    return valueNull();
                }
            }

            for (int i = 0; i < node->as.call.argCount; i++) {
                args[i] = interpretExpr(node->as.call.args[i], interp, env);
            }

            Value result = callFunction(callee.as.function, args, node->as.call.argCount, interp, env);

            for (int i = 0; i < node->as.call.argCount; i++) {
                valueFree(args[i]);
            }
            free(args);
            valueFree(callee);
            return result;
        }
        default:
            interpError(interp, node->line, "Invalid expression.");
            return valueNull();
    }
}

/* Executes a statement AST node for side effects */
static void interpretStmt(AstNode* node, Interpreter* interp, Env* env) {
    if (!node || interp->hadError) return;

    switch (node->type) {
        case NODE_VAR_DECL: {
            Value value = interpretExpr(node->as.varDecl.initializer, interp, env);
            if (!envDefine(env, node->as.varDecl.name, value)) {
                interpError(interp, node->line, "Variable already defined.");
                valueFree(value);
            }
            break;
        }
        case NODE_FUNCTION_DECL: {
            AsterFunction* fn = functionFromNode(node);
            if (!fn) {
                interpError(interp, node->line, "Could not create function.");
                break;
            }
            if (!envDefine(env, fn->name, valueFunction(fn))) {
                functionFree(fn);
                interpError(interp, node->line, "Function already defined.");
            }
            break;
        }
        case NODE_PRINT: {
            Value value = interpretExpr(node->as.printStmt.value, interp, env);
            printValue(value);
            valueFree(value);
            break;
        }
        case NODE_RETURN:
            interp->returnValue = node->as.returnStmt.value
                ? interpretExpr(node->as.returnStmt.value, interp, env)
                : valueNull();
            interp->hasReturn = true;
            break;
        case NODE_IF: {
            Value condition = interpretExpr(node->as.ifStmt.condition, interp, env);
            bool takeThen = isTruthy(condition);
            valueFree(condition);
            if (takeThen) {
                interpretStmt(node->as.ifStmt.thenBranch, interp, env);
            } else if (node->as.ifStmt.elseBranch) {
                interpretStmt(node->as.ifStmt.elseBranch, interp, env);
            }
            break;
        }
        case NODE_WHILE:
            while (!interp->hadError) {
                Value condition = interpretExpr(node->as.whileStmt.condition, interp, env);
                bool continueLoop = isTruthy(condition);
                valueFree(condition);
                if (!continueLoop) break;

                interpretStmt(node->as.whileStmt.body, interp, env);
                if (interp->hasReturn) break;
            }
            break;
        case NODE_BLOCK:
            interpretBlock(node, interp, env, true);
            break;
        default: {
            Value value = interpretExpr(node, interp, env);
            valueFree(value);
            break;
        }
    }
}

/* Executes all statements in a block, optionally in a new child scope */
static void interpretBlock(AstNode* node, Interpreter* interp, Env* env, bool newScope) {
    if (!node || node->type != NODE_BLOCK || interp->hadError) return;

    Env* scope = env;
    Env* child = NULL;
    if (newScope) {
        child = envCreate(env);
        scope = child;
    }

    for (int i = 0; i < node->as.block.count; i++) {
        interpretStmt(node->as.block.statements[i], interp, scope);
        if (interp->hasReturn) break;
    }

    if (child) envFree(child);
}

/* Evaluates any AST node, executing statements or returning expression values */
Value interpret(AstNode* node, Interpreter* interp, Env* env) {
    if (!node) return valueNull();

    switch (node->type) {
        case NODE_VAR_DECL:
        case NODE_FUNCTION_DECL:
        case NODE_PRINT:
        case NODE_RETURN:
        case NODE_IF:
        case NODE_WHILE:
        case NODE_BLOCK:
            interpretStmt(node, interp, env);
            return valueNull();
        default:
            return interpretExpr(node, interp, env);
    }
}

/* Parses and executes source code, printing a labeled test header */
bool runSource(const char* source, const char* label) {
    printf("--- %s ---\n", label);

    AstNode* program = parse(source);
    if (!program) {
        fprintf(stderr, "Parse failed.\n");
        return false;
    }

    Interpreter interp;
    interp.globals = envCreate(NULL);
    interp.hadError = false;
    interp.hasReturn = false;
    interp.returnValue = valueNull();

    if (program->type == NODE_BLOCK) {
        interpretBlock(program, &interp, interp.globals, false);
    } else {
        interpretStmt(program, &interp, interp.globals);
    }

    if (interp.hasReturn) valueFree(interp.returnValue);
    envFree(interp.globals);
    freeAst(program);

    if (interp.hadError) {
        fprintf(stderr, "Execution failed.\n");
        return false;
    }

    printf("\n");
    return true;
}
