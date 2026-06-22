#ifndef ASTER_CHUNK_H
#define ASTER_CHUNK_H

#include <stdint.h>

#include "value.h"

/* Bytecode instruction opcodes for the Aster VM */
typedef enum {
    OP_CONST,
    OP_NULL, OP_TRUE, OP_FALSE,
    OP_ADD, OP_SUB, OP_MUL, OP_DIV, OP_MOD,
    OP_NEG, OP_NOT,
    OP_EQ, OP_NEQ, OP_LT, OP_LTE, OP_GT, OP_GTE,
    OP_AND, OP_OR,
    OP_PRINT,
    OP_POP,
    OP_DEF_GLOBAL, OP_GET_GLOBAL, OP_SET_GLOBAL,
    OP_DEF_LOCAL, OP_GET_LOCAL, OP_SET_LOCAL,
    OP_JUMP,
    OP_JUMP_IF_FALSE,
    OP_LOOP,
    OP_CALL,
    OP_RETURN,
    OP_HALT,
} OpCode;

/* Bytecode container with instruction stream, constant pool, and line info */
typedef struct Chunk {
    uint8_t* code;
    int count;
    int capacity;
    Value* constants;
    int constCount;
    int constCapacity;
    int* lines;
} Chunk;

/* Initializes an empty bytecode chunk */
void initChunk(Chunk* chunk);

/* Appends a single byte to the chunk with source line information */
void writeChunk(Chunk* chunk, uint8_t byte, int line);

/* Adds a constant value to the pool and returns its index */
int addConstant(Chunk* chunk, Value value);

/* Frees all memory owned by a bytecode chunk */
void freeChunk(Chunk* chunk);

#endif /* ASTER_CHUNK_H */
