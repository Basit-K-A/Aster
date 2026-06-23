#include "vm.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ast.h"
#include "compiler.h"
#include "parser.h"

/* Pushes a value onto the VM operand stack */
static void push(VM* vm, Value value) {
    if (vm->stackTop >= vm->stack + STACK_MAX) {
        vm->hadError = true;
        fprintf(stderr, "Stack overflow.\n");
        return;
    }
    *vm->stackTop++ = value;
}

/* Pops a value from the VM operand stack */
static Value pop(VM* vm) {
    return *--vm->stackTop;
}

/* Returns a stack value at the given distance from the top */
static Value peek(VM* vm, int distance) {
    return vm->stackTop[-1 - distance];
}

/* Reports a runtime error at the current instruction's source line */
static void runtimeError(VM* vm, const char* message) {
    if (vm->hadError) return;
    vm->hadError = true;

    CallFrame* frame = &vm->frames[vm->frameCount - 1];
    int offset = (int)(frame->ip - frame->chunk->code) - 1;
    int line = frame->chunk->lines[offset];
    fprintf(stderr, "[line %d] Runtime error: %s\n", line, message);
}

/* Reads a single-byte operand from the current instruction stream */
static uint8_t readByte(CallFrame* frame) {
    return *frame->ip++;
}

/* Reads a two-byte big-endian operand from the current instruction stream */
static uint16_t readShort(CallFrame* frame) {
    uint16_t value = (uint16_t)((frame->ip[0] << 8) | frame->ip[1]);
    frame->ip += 2;
    return value;
}

/* Defines a new global variable in the VM */
static bool globalDefine(VM* vm, const char* name, Value value) {
    if (vm->globalCount >= 256) {
        runtimeError(vm, "Too many global variables.");
        return false;
    }

    for (int i = 0; i < vm->globalCount; i++) {
        if (strcmp(vm->globalKeys[i], name) == 0) {
            runtimeError(vm, "Global variable already defined.");
            return false;
        }
    }

    vm->globalKeys[vm->globalCount] = strdup(name);
    vm->globalVals[vm->globalCount] = valueCopy(value);
    vm->globalCount++;
    return true;
}

/* Assigns a value to an existing global variable */
static bool globalSet(VM* vm, const char* name, Value value) {
    for (int i = 0; i < vm->globalCount; i++) {
        if (strcmp(vm->globalKeys[i], name) == 0) {
            if (vm->globalVals[i].type == VAL_FUNCTION) {
                functionFree(vm->globalVals[i].as.function);
            } else {
                valueFree(vm->globalVals[i]);
            }
            vm->globalVals[i] = valueCopy(value);
            return true;
        }
    }
    runtimeError(vm, "Undefined global variable.");
    return false;
}

/* Looks up a global variable by name and pushes a copy onto the stack */
static bool globalGet(VM* vm, const char* name) {
    for (int i = 0; i < vm->globalCount; i++) {
        if (strcmp(vm->globalKeys[i], name) == 0) {
            push(vm, valueCopy(vm->globalVals[i]));
            return true;
        }
    }
    runtimeError(vm, "Undefined global variable.");
    return false;
}

/* Begins executing a user-defined function with arguments already on the stack */
static bool callFunction(VM* vm, AsterFunction* fn, int argCount) {
    if (!fn || !fn->hasBytecode || !fn->chunk) {
        runtimeError(vm, "Invalid function.");
        return false;
    }
    if (argCount != fn->arity) {
        runtimeError(vm, "Wrong number of arguments.");
        return false;
    }
    if (vm->frameCount >= FRAMES_MAX) {
        runtimeError(vm, "Stack overflow.");
        return false;
    }

    pop(vm);

    CallFrame* frame = &vm->frames[vm->frameCount++];
    frame->function = fn;
    frame->chunk = fn->chunk;
    frame->ip = fn->chunk->code;
    frame->slots = vm->stackTop - argCount;
    vm->stackTop = frame->slots + argCount;
    return true;
}

/* Applies a binary operator to two stack values and pushes the result */
static bool binaryOp(VM* vm, BinOp op) {
    Value b = pop(vm);
    Value a = pop(vm);

    if (op == BINOP_ADD) {
        if (a.type == VAL_STRING || b.type == VAL_STRING) {
            char* as = valueToAllocatedString(a);
            char* bs = valueToAllocatedString(b);
            size_t len = strlen(as) + strlen(bs) + 1;
            char* combined = (char*)malloc(len);
            if (combined) {
                snprintf(combined, len, "%s%s", as, bs);
            }
            free(as);
            free(bs);
            valueFree(a);
            valueFree(b);
            push(vm, valueStringOwned(combined));
            return true;
        }
        if (a.type == VAL_NUMBER && b.type == VAL_NUMBER) {
            push(vm, valueNumber(a.as.number + b.as.number));
            return true;
        }
        runtimeError(vm, "Operands must be numbers or strings for '+'.");
        valueFree(a);
        valueFree(b);
        return false;
    }

    if (a.type != VAL_NUMBER || b.type != VAL_NUMBER) {
        runtimeError(vm, "Operands must be numbers.");
        valueFree(a);
        valueFree(b);
        return false;
    }

    double x = a.as.number;
    double y = b.as.number;
    Value result = valueNull();

    switch (op) {
        case BINOP_SUB: result = valueNumber(x - y); break;
        case BINOP_MUL: result = valueNumber(x * y); break;
        case BINOP_DIV:
            if (y == 0.0) {
                runtimeError(vm, "Division by zero.");
                valueFree(a);
                valueFree(b);
                return false;
            }
            result = valueNumber(x / y);
            break;
        case BINOP_MOD:
            if (y == 0.0) {
                runtimeError(vm, "Modulo by zero.");
                valueFree(a);
                valueFree(b);
                return false;
            }
            result = valueNumber(fmod(x, y));
            break;
        case BINOP_EQ:  result = valueBool(valuesEqual(a, b)); break;
        case BINOP_NEQ: result = valueBool(!valuesEqual(a, b)); break;
        case BINOP_LT:  result = valueBool(x < y); break;
        case BINOP_LTE: result = valueBool(x <= y); break;
        case BINOP_GT:  result = valueBool(x > y); break;
        case BINOP_GTE: result = valueBool(x >= y); break;
        default:
            runtimeError(vm, "Unknown binary operator.");
            valueFree(a);
            valueFree(b);
            return false;
    }

    valueFree(a);
    valueFree(b);
    push(vm, result);
    return true;
}

/* Initializes a virtual machine to a clean state */
void initVM(VM* vm) {
    memset(vm, 0, sizeof(VM));
    vm->stackTop = vm->stack;
}

/* Frees all globals owned by the virtual machine */
void freeVM(VM* vm) {
    for (int i = 0; i < vm->globalCount; i++) {
        free(vm->globalKeys[i]);
        if (vm->globalVals[i].type == VAL_FUNCTION) {
            functionFree(vm->globalVals[i].as.function);
        } else {
            valueFree(vm->globalVals[i]);
        }
    }
    initVM(vm);
}

/* Executes a compiled bytecode chunk in the virtual machine */
InterpretResult run(VM* vm, Chunk* chunk) {
    vm->frameCount = 1;
    CallFrame* frame = &vm->frames[0];
    frame->function = NULL;
    frame->chunk = chunk;
    frame->ip = chunk->code;
    frame->slots = vm->stack;
    vm->stackTop = vm->stack;

    for (;;) {
        if (vm->hadError) return INTERPRET_RUNTIME_ERROR;

        uint8_t instruction = readByte(frame);

        switch ((OpCode)instruction) {
            case OP_CONST: {
                uint8_t constant = readByte(frame);
                push(vm, valueCopy(frame->chunk->constants[constant]));
                break;
            }
            case OP_NULL:
                push(vm, valueNull());
                break;
            case OP_TRUE:
                push(vm, valueBool(true));
                break;
            case OP_FALSE:
                push(vm, valueBool(false));
                break;
            case OP_ADD:
                if (!binaryOp(vm, BINOP_ADD)) return INTERPRET_RUNTIME_ERROR;
                break;
            case OP_SUB:
                if (!binaryOp(vm, BINOP_SUB)) return INTERPRET_RUNTIME_ERROR;
                break;
            case OP_MUL:
                if (!binaryOp(vm, BINOP_MUL)) return INTERPRET_RUNTIME_ERROR;
                break;
            case OP_DIV:
                if (!binaryOp(vm, BINOP_DIV)) return INTERPRET_RUNTIME_ERROR;
                break;
            case OP_MOD:
                if (!binaryOp(vm, BINOP_MOD)) return INTERPRET_RUNTIME_ERROR;
                break;
            case OP_NEG: {
                Value operand = pop(vm);
                if (operand.type != VAL_NUMBER) {
                    runtimeError(vm, "Operand must be a number.");
                    valueFree(operand);
                    return INTERPRET_RUNTIME_ERROR;
                }
                push(vm, valueNumber(-operand.as.number));
                break;
            }
            case OP_NOT: {
                Value operand = pop(vm);
                push(vm, valueBool(!isTruthy(operand)));
                valueFree(operand);
                break;
            }
            case OP_EQ:
                if (!binaryOp(vm, BINOP_EQ)) return INTERPRET_RUNTIME_ERROR;
                break;
            case OP_NEQ:
                if (!binaryOp(vm, BINOP_NEQ)) return INTERPRET_RUNTIME_ERROR;
                break;
            case OP_LT:
                if (!binaryOp(vm, BINOP_LT)) return INTERPRET_RUNTIME_ERROR;
                break;
            case OP_LTE:
                if (!binaryOp(vm, BINOP_LTE)) return INTERPRET_RUNTIME_ERROR;
                break;
            case OP_GT:
                if (!binaryOp(vm, BINOP_GT)) return INTERPRET_RUNTIME_ERROR;
                break;
            case OP_GTE:
                if (!binaryOp(vm, BINOP_GTE)) return INTERPRET_RUNTIME_ERROR;
                break;
            case OP_PRINT: {
                Value value = pop(vm);
                printValue(value);
                valueFree(value);
                break;
            }
            case OP_POP:
                valueFree(pop(vm));
                break;
            case OP_DEF_GLOBAL: {
                uint8_t nameIndex = readByte(frame);
                const char* name = frame->chunk->constants[nameIndex].as.string;
                Value value = pop(vm);
                if (!globalDefine(vm, name, value)) {
                    valueFree(value);
                    return INTERPRET_RUNTIME_ERROR;
                }
                valueFree(value);
                break;
            }
            case OP_GET_GLOBAL: {
                uint8_t nameIndex = readByte(frame);
                const char* name = frame->chunk->constants[nameIndex].as.string;
                if (!globalGet(vm, name)) return INTERPRET_RUNTIME_ERROR;
                break;
            }
            case OP_SET_GLOBAL: {
                uint8_t nameIndex = readByte(frame);
                const char* name = frame->chunk->constants[nameIndex].as.string;
                Value value = peek(vm, 0);
                if (!globalSet(vm, name, value)) return INTERPRET_RUNTIME_ERROR;
                break;
            }
            case OP_DEF_LOCAL: {
                uint8_t slot = readByte(frame);
                frame->slots[slot] = pop(vm);
                break;
            }
            case OP_GET_LOCAL: {
                uint8_t slot = readByte(frame);
                push(vm, valueCopy(frame->slots[slot]));
                break;
            }
            case OP_SET_LOCAL: {
                uint8_t slot = readByte(frame);
                frame->slots[slot] = peek(vm, 0);
                break;
            }
            case OP_JUMP:
                frame->ip += readShort(frame);
                break;
            case OP_JUMP_IF_FALSE: {
                uint16_t offset = readShort(frame);
                if (!isTruthy(peek(vm, 0))) {
                    pop(vm);
                    frame->ip += offset;
                } else {
                    pop(vm);
                }
                break;
            }
            case OP_LOOP:
                frame->ip -= readShort(frame);
                break;
            case OP_CALL: {
                uint8_t argCount = readByte(frame);
                Value callee = peek(vm, 0);
                if (callee.type != VAL_FUNCTION) {
                    runtimeError(vm, "Can only call functions.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                if (!callFunction(vm, callee.as.function, argCount)) {
                    return INTERPRET_RUNTIME_ERROR;
                }
                frame = &vm->frames[vm->frameCount - 1];
                break;
            }
            case OP_RETURN: {
                Value result = pop(vm);
                vm->frameCount--;
                if (vm->frameCount == 0) {
                    push(vm, result);
                    return INTERPRET_OK;
                }
                vm->stackTop = frame->slots;
                push(vm, result);
                frame = &vm->frames[vm->frameCount - 1];
                break;
            }
            case OP_HALT:
                return INTERPRET_OK;
            default:
                runtimeError(vm, "Unknown opcode.");
                return INTERPRET_RUNTIME_ERROR;
        }
    }
}

/* Parses, compiles, and runs source code in the VM */
bool runSourceVM(const char* source, const char* label) {
    printf("--- %s ---\n", label);

    AstNode* program = parse(source);
    if (!program) {
        fprintf(stderr, "Parse failed.\n");
        return false;
    }

    Chunk chunk;
    initChunk(&chunk);

    Compiler compiler;
    compiler.chunk = &chunk;
    compiler.hadError = false;

    compile(program, &compiler);
    freeAst(program);

    if (compiler.hadError) {
        fprintf(stderr, "Compile failed.\n");
        freeChunk(&chunk);
        return false;
    }

    VM vm;
    initVM(&vm);
    InterpretResult result = run(&vm, &chunk);
    freeVM(&vm);
    freeChunk(&chunk);

    if (result != INTERPRET_OK) {
        fprintf(stderr, "Execution failed.\n");
        return false;
    }

    printf("\n");
    return true;
}
