#include "chunk.h"

#include <stdlib.h>
#include <string.h>

/* Grows the bytecode instruction buffer when capacity is exceeded */
static void growCode(Chunk* chunk) {
    int capacity = chunk->capacity < 8 ? 8 : chunk->capacity * 2;
    uint8_t* code = (uint8_t*)realloc(chunk->code, (size_t)capacity);
    int* lines = (int*)realloc(chunk->lines, (size_t)capacity * sizeof(int));
    if (!code || !lines) return;
    chunk->code = code;
    chunk->lines = lines;
    chunk->capacity = capacity;
}

/* Grows the constant pool when capacity is exceeded */
static void growConstants(Chunk* chunk) {
    int capacity = chunk->constCapacity < 8 ? 8 : chunk->constCapacity * 2;
    Value* constants = (Value*)realloc(chunk->constants, (size_t)capacity * sizeof(Value));
    if (!constants) return;
    chunk->constants = constants;
    chunk->constCapacity = capacity;
}

/* Initializes an empty bytecode chunk */
void initChunk(Chunk* chunk) {
    memset(chunk, 0, sizeof(Chunk));
}

/* Appends a single byte to the chunk with source line information */
void writeChunk(Chunk* chunk, uint8_t byte, int line) {
    if (chunk->count >= chunk->capacity) {
        growCode(chunk);
    }
    chunk->code[chunk->count] = byte;
    chunk->lines[chunk->count] = line;
    chunk->count++;
}

/* Adds a constant value to the pool and returns its index */
int addConstant(Chunk* chunk, Value value) {
    if (chunk->constCount >= chunk->constCapacity) {
        growConstants(chunk);
    }
    chunk->constants[chunk->constCount] = value;
    return chunk->constCount++;
}

/* Frees all memory owned by a bytecode chunk */
void freeChunk(Chunk* chunk) {
    free(chunk->code);
    free(chunk->lines);
    for (int i = 0; i < chunk->constCount; i++) {
        if (chunk->constants[i].type == VAL_FUNCTION ||
            chunk->constants[i].type == VAL_CLOSURE ||
            chunk->constants[i].type == VAL_CLASS) continue;
        valueFree(chunk->constants[i]);
    }
    free(chunk->constants);
    initChunk(chunk);
}
