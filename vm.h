#ifndef ASTER_VM_H
#define ASTER_VM_H

#include <stdbool.h>

#include "chunk.h"

#define STACK_MAX 256
#define FRAMES_MAX 64

typedef struct VM VM;

/* A single call frame with instruction pointer and stack slot base */
typedef struct {
    Chunk* chunk;
    uint8_t* ip;
    Value* slots;
} CallFrame;

/* Stack-based virtual machine with globals and call frames */
struct VM {
    CallFrame frames[FRAMES_MAX];
    int frameCount;
    Value stack[STACK_MAX];
    Value* stackTop;
    char* globalKeys[256];
    Value globalVals[256];
    int globalCount;
    bool hadError;
};

/* Result of executing bytecode in the VM */
typedef enum {
    INTERPRET_OK,
    INTERPRET_COMPILE_ERROR,
    INTERPRET_RUNTIME_ERROR,
} InterpretResult;

/* Initializes a virtual machine to a clean state */
void initVM(VM* vm);

/* Frees all globals owned by the virtual machine */
void freeVM(VM* vm);

/* Executes a compiled bytecode chunk in the virtual machine */
InterpretResult run(VM* vm, Chunk* chunk);

/* Parses, compiles, and runs source code in the VM */
bool runSourceVM(const char* source, const char* label);

#endif /* ASTER_VM_H */
