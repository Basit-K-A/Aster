#include "vm.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ast.h"
#include "compiler.h"
#include "object.h"
#include "parser.h"

static bool callClosure(VM* vm, AsterClosure* closure, int argCount);

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

/* Returns the value stored in an upvalue, whether open or closed */
static Value upvalueGet(Upvalue* upvalue) {
    return upvalue->location ? *upvalue->location : upvalue->closed;
}

/* Assigns a value into an upvalue, whether open or closed */
static void upvalueSet(VM* vm, Upvalue* upvalue, Value value) {
    if (upvalue->location) {
        *upvalue->location = value;
    } else {
        valueFree(upvalue->closed);
        upvalue->closed = valueCopyVm(vm, value);
    }
}

/* Captures a local variable slot as an open upvalue */
static Upvalue* captureUpvalue(VM* vm, Value* local) {
    Upvalue* prev = NULL;
    Upvalue* upvalue = vm->openUpvalues;

    while (upvalue != NULL && upvalue->location > local) {
        prev = upvalue;
        upvalue = upvalue->next;
    }

    if (upvalue != NULL && upvalue->location == local) {
        return upvalue;
    }

    Upvalue* created = (Upvalue*)allocateObject(vm, sizeof(Upvalue), OBJ_UPVALUE);
    if (!created) return NULL;
    created->location = local;
    created->next = upvalue;

    if (prev == NULL) {
        vm->openUpvalues = created;
    } else {
        prev->next = created;
    }

    return created;
}

/* Closes open upvalues at or above the given stack location */
static void closeUpvalues(VM* vm, Value* last) {
    while (vm->openUpvalues != NULL && vm->openUpvalues->location >= last) {
        Upvalue* upvalue = vm->openUpvalues;
        upvalue->closed = valueCopyVm(vm, *upvalue->location);
        upvalue->location = NULL;
        vm->openUpvalues = upvalue->next;
    }
}

/* Creates a closure from a function prototype and captured upvalues */
static AsterClosure* closureNew(VM* vm, AsterFunction* function, int upvalueCount) {
    AsterClosure* closure = (AsterClosure*)allocateObject(vm, sizeof(AsterClosure), OBJ_CLOSURE);
    if (!closure) return NULL;

    closure->function = function;
    closure->upvalueCount = upvalueCount;
    if (upvalueCount > 0) {
        closure->upvalues = (Upvalue**)calloc((size_t)upvalueCount, sizeof(Upvalue*));
        if (!closure->upvalues) {
            return NULL;
        }
    }
    return closure;
}

/* Creates a new class object with the given name */
static AsterClass* classNew(VM* vm, const char* name) {
    AsterClass* klass = (AsterClass*)allocateObject(vm, sizeof(AsterClass), OBJ_CLASS);
    if (!klass) return NULL;
    klass->name = strdup(name);
    if (!klass->name) return NULL;
    vm->bytesAllocated += strlen(name) + 1;
    return klass;
}

/* Adds a method function to a class method table */
static void classAddMethod(AsterClass* klass, const char* name, AsterFunction* method) {
    int count = klass->methodCount;
    char** names = (char**)realloc(klass->methodNames, (size_t)(count + 1) * sizeof(char*));
    AsterFunction** methods = (AsterFunction**)realloc(klass->methods, (size_t)(count + 1) * sizeof(AsterFunction*));
    if (!names || !methods) {
        free(names);
        free(methods);
        return;
    }
    klass->methodNames = names;
    klass->methods = methods;
    klass->methodNames[count] = strdup(name);
    klass->methods[count] = method;
    klass->methodCount++;
}

/* Looks up a method on a class by name */
static AsterFunction* classGetMethod(AsterClass* klass, const char* name) {
    for (int i = 0; i < klass->methodCount; i++) {
        if (strcmp(klass->methodNames[i], name) == 0) {
            return klass->methods[i];
        }
    }
    return NULL;
}

/* Creates a new instance of the given class */
static AsterInstance* instanceNew(VM* vm, AsterClass* klass) {
    AsterInstance* instance = (AsterInstance*)allocateObject(vm, sizeof(AsterInstance), OBJ_INSTANCE);
    if (!instance) return NULL;
    instance->klass = klass;
    return instance;
}

/* Reads a field value from an instance */
static bool instanceGetField(VM* vm, AsterInstance* instance, const char* name, Value* out) {
    for (int i = 0; i < instance->fieldCount; i++) {
        if (strcmp(instance->fieldKeys[i], name) == 0) {
            *out = valueCopyVm(vm, instance->fieldVals[i]);
            return true;
        }
    }
    return false;
}

/* Assigns a field value on an instance, creating the field if needed */
static bool instanceSetField(VM* vm, AsterInstance* instance, const char* name, Value value) {
    for (int i = 0; i < instance->fieldCount; i++) {
        if (strcmp(instance->fieldKeys[i], name) == 0) {
            valueFree(instance->fieldVals[i]);
            instance->fieldVals[i] = valueCopyVm(vm, value);
            return true;
        }
    }

    if (instance->fieldCount >= 256) return false;

    int count = instance->fieldCount;
    char** keys = (char**)realloc(instance->fieldKeys, (size_t)(count + 1) * sizeof(char*));
    Value* vals = (Value*)realloc(instance->fieldVals, (size_t)(count + 1) * sizeof(Value));
    if (!keys || !vals) {
        free(keys);
        free(vals);
        return false;
    }
    instance->fieldKeys = keys;
    instance->fieldVals = vals;
    instance->fieldKeys[count] = strdup(name);
    vm->bytesAllocated += strlen(name) + 1;
    instance->fieldVals[count] = valueCopyVm(vm, value);
    instance->fieldCount++;
    return true;
}

/* Invokes a method on an instance with arguments already on the stack */
static bool invokeMethod(VM* vm, AsterInstance* instance, const char* name, int argCount) {
    AsterFunction* method = classGetMethod(instance->klass, name);
    if (!method) {
        runtimeError(vm, "Undefined property.");
        return false;
    }

    AsterClosure* closure = closureNew(vm, method, method->upvalueCount);
    if (!closure) {
        runtimeError(vm, "Out of memory.");
        return false;
    }

    push(vm, valueClosure(closure));
    return callClosure(vm, closure, argCount);
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
    vm->globalVals[vm->globalCount] = valueCopyVm(vm, value);
    vm->globalCount++;
    return true;
}

/* Releases a stored global compile-time value without freeing GC objects */
static void globalReleaseValue(Value value) {
    valueFree(value);
}

/* Assigns a value to an existing global variable */
static bool globalSet(VM* vm, const char* name, Value value) {
    for (int i = 0; i < vm->globalCount; i++) {
        if (strcmp(vm->globalKeys[i], name) == 0) {
            globalReleaseValue(vm->globalVals[i]);
            vm->globalVals[i] = valueCopyVm(vm, value);
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
            push(vm, valueCopyVm(vm, vm->globalVals[i]));
            return true;
        }
    }
    runtimeError(vm, "Undefined global variable.");
    return false;
}

/* Begins executing a closure with arguments already on the stack */
static bool callClosure(VM* vm, AsterClosure* closure, int argCount) {
    AsterFunction* fn = closure->function;
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
    frame->closure = closure;
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
            ObjString* result = copyString(vm, combined ? combined : "");
            free(combined);
            free(as);
            free(bs);
            valueFree(a);
            valueFree(b);
            if (!result) {
                runtimeError(vm, "Out of memory.");
                return false;
            }
            push(vm, valueStringObj(result));
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
    vm->nextGC = GC_INITIAL_THRESHOLD;
}

/* Frees all globals and open upvalues owned by the virtual machine */
void freeVM(VM* vm) {
    closeUpvalues(vm, vm->stack);

    for (int i = 0; i < vm->globalCount; i++) {
        free(vm->globalKeys[i]);
        valueFree(vm->globalVals[i]);
    }

    freeObjects(vm);
    initVM(vm);
}

/* Executes a compiled bytecode chunk in the virtual machine */
InterpretResult run(VM* vm, Chunk* chunk) {
    vm->frameCount = 1;
    CallFrame* frame = &vm->frames[0];
    frame->closure = NULL;
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
                push(vm, valueCopyVm(vm, frame->chunk->constants[constant]));
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
                const char* name = valueStringChars(frame->chunk->constants[nameIndex]);
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
                const char* name = valueStringChars(frame->chunk->constants[nameIndex]);
                if (!globalGet(vm, name)) return INTERPRET_RUNTIME_ERROR;
                break;
            }
            case OP_SET_GLOBAL: {
                uint8_t nameIndex = readByte(frame);
                const char* name = valueStringChars(frame->chunk->constants[nameIndex]);
                Value value = peek(vm, 0);
                if (!globalSet(vm, name, value)) return INTERPRET_RUNTIME_ERROR;
                break;
            }
            case OP_DEF_LOCAL: {
                uint8_t slot = readByte(frame);
                frame->slots[slot] = pop(vm);
                vm->stackTop = frame->slots + slot + 1;
                break;
            }
            case OP_GET_LOCAL: {
                uint8_t slot = readByte(frame);
                push(vm, valueCopyVm(vm, frame->slots[slot]));
                break;
            }
            case OP_SET_LOCAL: {
                uint8_t slot = readByte(frame);
                frame->slots[slot] = peek(vm, 0);
                break;
            }
            case OP_GET_UPVALUE: {
                uint8_t slot = readByte(frame);
                push(vm, valueCopyVm(vm, upvalueGet(frame->closure->upvalues[slot])));
                break;
            }
            case OP_SET_UPVALUE: {
                uint8_t slot = readByte(frame);
                upvalueSet(vm, frame->closure->upvalues[slot], peek(vm, 0));
                break;
            }
            case OP_CLOSE_UPVALUE:
                closeUpvalues(vm, vm->stackTop - 1);
                pop(vm);
                break;
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
            case OP_CLOSURE: {
                uint8_t constant = readByte(frame);
                AsterFunction* function = frame->chunk->constants[constant].as.function;
                AsterClosure* closure = closureNew(vm, function, function->upvalueCount);
                if (!closure) {
                    runtimeError(vm, "Out of memory.");
                    return INTERPRET_RUNTIME_ERROR;
                }

                for (int i = 0; i < function->upvalueCount; i++) {
                    uint8_t isLocal = readByte(frame);
                    uint8_t index = readByte(frame);
                    if (isLocal) {
                        closure->upvalues[i] = captureUpvalue(vm, frame->slots + index);
                    } else {
                        closure->upvalues[i] = frame->closure->upvalues[index];
                    }
                }

                push(vm, valueClosure(closure));
                break;
            }
            case OP_CALL: {
                uint8_t argCount = readByte(frame);
                Value callee = peek(vm, 0);
                if (callee.type == VAL_CLASS) {
                    pop(vm);
                    AsterInstance* instance = instanceNew(vm, callee.as.klass);
                    if (!instance) {
                        runtimeError(vm, "Out of memory.");
                        return INTERPRET_RUNTIME_ERROR;
                    }
                    push(vm, valueInstance(instance));
                } else if (callee.type == VAL_CLOSURE) {
                    if (!callClosure(vm, callee.as.closure, argCount)) {
                        return INTERPRET_RUNTIME_ERROR;
                    }
                } else if (callee.type == VAL_FUNCTION) {
                    runtimeError(vm, "Functions must be wrapped in a closure before calling.");
                    return INTERPRET_RUNTIME_ERROR;
                } else {
                    runtimeError(vm, "Can only call functions and classes.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                frame = &vm->frames[vm->frameCount - 1];
                break;
            }
            case OP_CLASS: {
                uint8_t nameIndex = readByte(frame);
                const char* name = valueStringChars(frame->chunk->constants[nameIndex]);
                AsterClass* klass = classNew(vm, name);
                if (!klass) {
                    runtimeError(vm, "Out of memory.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                push(vm, valueClass(klass));
                break;
            }
            case OP_METHOD: {
                uint8_t nameIndex = readByte(frame);
                const char* name = valueStringChars(frame->chunk->constants[nameIndex]);
                Value method = pop(vm);
                if (method.type != VAL_CLOSURE) {
                    runtimeError(vm, "Expected method closure.");
                    valueFree(method);
                    return INTERPRET_RUNTIME_ERROR;
                }
                Value classValue = peek(vm, 0);
                if (classValue.type != VAL_CLASS) {
                    runtimeError(vm, "Expected class.");
                    valueFree(method);
                    return INTERPRET_RUNTIME_ERROR;
                }
                classAddMethod(classValue.as.klass, name, method.as.closure->function);
                valueFree(method);
                break;
            }
            case OP_GET_PROPERTY: {
                uint8_t nameIndex = readByte(frame);
                const char* name = valueStringChars(frame->chunk->constants[nameIndex]);
                Value receiver = pop(vm);
                if (receiver.type != VAL_INSTANCE) {
                    runtimeError(vm, "Only instances have properties.");
                    valueFree(receiver);
                    return INTERPRET_RUNTIME_ERROR;
                }
                Value field;
                if (instanceGetField(vm, receiver.as.instance, name, &field)) {
                    valueFree(receiver);
                    push(vm, field);
                    break;
                }
                if (classGetMethod(receiver.as.instance->klass, name)) {
                    runtimeError(vm, "Use method invocation for methods.");
                    valueFree(receiver);
                    return INTERPRET_RUNTIME_ERROR;
                }
                runtimeError(vm, "Undefined property.");
                valueFree(receiver);
                return INTERPRET_RUNTIME_ERROR;
            }
            case OP_SET_PROPERTY: {
                uint8_t nameIndex = readByte(frame);
                const char* name = valueStringChars(frame->chunk->constants[nameIndex]);
                Value receiver = pop(vm);
                Value value = pop(vm);
                if (receiver.type != VAL_INSTANCE) {
                    runtimeError(vm, "Only instances have properties.");
                    valueFree(receiver);
                    valueFree(value);
                    return INTERPRET_RUNTIME_ERROR;
                }
                if (!instanceSetField(vm, receiver.as.instance, name, value)) {
                    runtimeError(vm, "Too many fields on instance.");
                    valueFree(receiver);
                    valueFree(value);
                    return INTERPRET_RUNTIME_ERROR;
                }
                valueFree(receiver);
                push(vm, value);
                break;
            }
            case OP_INVOKE: {
                uint8_t nameIndex = readByte(frame);
                uint8_t argCount = readByte(frame);
                const char* name = valueStringChars(frame->chunk->constants[nameIndex]);
                Value receiver = peek(vm, argCount);
                if (receiver.type != VAL_INSTANCE) {
                    runtimeError(vm, "Only instances have methods.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                if (!invokeMethod(vm, receiver.as.instance, name, argCount)) {
                    return INTERPRET_RUNTIME_ERROR;
                }
                frame = &vm->frames[vm->frameCount - 1];
                break;
            }
            case OP_RETURN: {
                Value result = pop(vm);
                closeUpvalues(vm, frame->slots);
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
bool runSourceVMEx(const char* source, const char* label, size_t* bytesAfter) {
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

    if (bytesAfter) {
        *bytesAfter = vmBytesAllocated(&vm);
    }

    freeVM(&vm);
    freeChunk(&chunk);

    if (result != INTERPRET_OK) {
        fprintf(stderr, "Execution failed.\n");
        return false;
    }

    printf("\n");
    return true;
}

/* Parses, compiles, and runs source code in the VM */
bool runSourceVM(const char* source, const char* label) {
    return runSourceVMEx(source, label, NULL);
}
