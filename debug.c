#include "debug.h"

#include <stdio.h>

/* Returns a human-readable name for an opcode */
static const char* opcodeName(OpCode op) {
    switch (op) {
        case OP_CONST:          return "OP_CONST";
        case OP_NULL:           return "OP_NULL";
        case OP_TRUE:           return "OP_TRUE";
        case OP_FALSE:          return "OP_FALSE";
        case OP_ADD:            return "OP_ADD";
        case OP_SUB:            return "OP_SUB";
        case OP_MUL:            return "OP_MUL";
        case OP_DIV:            return "OP_DIV";
        case OP_MOD:            return "OP_MOD";
        case OP_NEG:            return "OP_NEG";
        case OP_NOT:            return "OP_NOT";
        case OP_EQ:             return "OP_EQ";
        case OP_NEQ:            return "OP_NEQ";
        case OP_LT:             return "OP_LT";
        case OP_LTE:            return "OP_LTE";
        case OP_GT:             return "OP_GT";
        case OP_GTE:            return "OP_GTE";
        case OP_AND:            return "OP_AND";
        case OP_OR:             return "OP_OR";
        case OP_PRINT:          return "OP_PRINT";
        case OP_POP:            return "OP_POP";
        case OP_DEF_GLOBAL:     return "OP_DEF_GLOBAL";
        case OP_GET_GLOBAL:     return "OP_GET_GLOBAL";
        case OP_SET_GLOBAL:     return "OP_SET_GLOBAL";
        case OP_DEF_LOCAL:      return "OP_DEF_LOCAL";
        case OP_GET_LOCAL:      return "OP_GET_LOCAL";
        case OP_SET_LOCAL:      return "OP_SET_LOCAL";
        case OP_JUMP:           return "OP_JUMP";
        case OP_JUMP_IF_FALSE:  return "OP_JUMP_IF_FALSE";
        case OP_LOOP:           return "OP_LOOP";
        case OP_CALL:           return "OP_CALL";
        case OP_RETURN:         return "OP_RETURN";
        case OP_CLOSURE:        return "OP_CLOSURE";
        case OP_GET_UPVALUE:    return "OP_GET_UPVALUE";
        case OP_SET_UPVALUE:    return "OP_SET_UPVALUE";
        case OP_CLOSE_UPVALUE:  return "OP_CLOSE_UPVALUE";
        case OP_CLASS:          return "OP_CLASS";
        case OP_GET_PROPERTY:   return "OP_GET_PROPERTY";
        case OP_SET_PROPERTY:   return "OP_SET_PROPERTY";
        case OP_METHOD:         return "OP_METHOD";
        case OP_INVOKE:         return "OP_INVOKE";
        case OP_HALT:           return "OP_HALT";
    }
    return "OP_UNKNOWN";
}

/* Formats a constant pool entry for disassembly output */
static void printConstant(Chunk* chunk, int index) {
    Value value = chunk->constants[index];
    switch (value.type) {
        case VAL_NUMBER:
            printf("%g", value.as.number);
            break;
        case VAL_STRING:
            printf("\"%s\"", valueStringChars(value));
            break;
        case VAL_BOOL:
            printf("%s", value.as.boolean ? "true" : "false");
            break;
        case VAL_NULL:
            printf("null");
            break;
        case VAL_FUNCTION:
            printf("<fn %s>", value.as.function && value.as.function->name ? value.as.function->name : "");
            break;
        case VAL_CLOSURE:
            printf("<closure>");
            break;
        case VAL_CLASS:
            printf("<class>");
            break;
        case VAL_INSTANCE:
            printf("<instance>");
            break;
    }
}

/* Disassembles a single instruction and returns the next instruction offset */
static int disassembleInstruction(Chunk* chunk, int offset) {
    printf("%04d ", offset);

    if (offset > 0 && chunk->lines[offset] == chunk->lines[offset - 1]) {
        printf("   | ");
    } else {
        printf("%4d ", chunk->lines[offset]);
    }

    OpCode instruction = (OpCode)chunk->code[offset];
    printf("%-20s", opcodeName(instruction));

    int next = offset + 1;
    switch (instruction) {
        case OP_CONST: {
            uint8_t constant = chunk->code[offset + 1];
            printf("%3d '", constant);
            printConstant(chunk, constant);
            printf("'");
            next = offset + 2;
            break;
        }
        case OP_DEF_GLOBAL:
        case OP_GET_GLOBAL:
        case OP_SET_GLOBAL: {
            uint8_t constant = chunk->code[offset + 1];
            printf("%3d '", constant);
            printConstant(chunk, constant);
            printf("'");
            next = offset + 2;
            break;
        }
        case OP_DEF_LOCAL:
        case OP_GET_LOCAL:
        case OP_SET_LOCAL: {
            uint8_t slot = chunk->code[offset + 1];
            printf("%3d", slot);
            next = offset + 2;
            break;
        }
        case OP_JUMP:
        case OP_JUMP_IF_FALSE: {
            uint16_t jump = (uint16_t)((chunk->code[offset + 1] << 8) | chunk->code[offset + 2]);
            printf("%4d -> %d", jump, offset + 3 + jump);
            next = offset + 3;
            break;
        }
        case OP_LOOP: {
            uint16_t jump = (uint16_t)((chunk->code[offset + 1] << 8) | chunk->code[offset + 2]);
            printf("%4d -> %d", jump, offset + 3 - jump);
            next = offset + 3;
            break;
        }
        case OP_CALL: {
            uint8_t argc = chunk->code[offset + 1];
            printf("(%d args)", argc);
            next = offset + 2;
            break;
        }
        case OP_CLOSURE: {
            uint8_t constant = chunk->code[offset + 1];
            printf("%3d '", constant);
            printConstant(chunk, constant);
            printf("'");
            next = offset + 2;
            Value fnValue = chunk->constants[constant];
            if (fnValue.type == VAL_FUNCTION && fnValue.as.function) {
                for (int i = 0; i < fnValue.as.function->upvalueCount; i++) {
                    uint8_t isLocal = chunk->code[next];
                    uint8_t index = chunk->code[next + 1];
                    printf(" %s:%d", isLocal ? "local" : "upvalue", index);
                    next += 2;
                }
            }
            break;
        }
        case OP_GET_UPVALUE:
        case OP_SET_UPVALUE:
        case OP_CLASS:
        case OP_GET_PROPERTY:
        case OP_SET_PROPERTY:
        case OP_METHOD: {
            uint8_t slot = chunk->code[offset + 1];
            printf("%3d '", slot);
            printConstant(chunk, slot);
            printf("'");
            next = offset + 2;
            break;
        }
        case OP_INVOKE: {
            uint8_t name = chunk->code[offset + 1];
            uint8_t argc = chunk->code[offset + 2];
            printf("%3d '", name);
            printConstant(chunk, name);
            printf("' (%d args)", argc);
            next = offset + 3;
            break;
        }
        default:
            break;
    }

    printf("\n");
    return next;
}

/* Prints a human-readable disassembly of a bytecode chunk */
void disassemble(Chunk* chunk, const char* name) {
    printf("== %s ==\n", name);
    for (int offset = 0; offset < chunk->count;) {
        offset = disassembleInstruction(chunk, offset);
    }
}

/* Disassembles a chunk and any nested function chunks in its constant pool */
void disassembleAll(Chunk* chunk, const char* name) {
    disassemble(chunk, name);
    for (int i = 0; i < chunk->constCount; i++) {
        Value value = chunk->constants[i];
        if (value.type == VAL_FUNCTION && value.as.function && value.as.function->chunk) {
            char label[256];
            const char* fnName = value.as.function->name ? value.as.function->name : "?";
            snprintf(label, sizeof(label), "%s/<fn %s>", name, fnName);
            disassembleAll(value.as.function->chunk, label);
        }
    }
}
