#ifndef ASTER_VM_H
#define ASTER_VM_H

#include <stdbool.h>
#include <stddef.h>

#include "chunk.h"
#include "value.h"

#define STACK_MAX 256
#define FRAMES_MAX 64

typedef struct AsterObject AsterObject;
typedef struct VM VM;

/* A single call frame referencing a closure and its stack slot base */
typedef struct {
    AsterClosure* closure;
    Chunk* chunk;
    uint8_t* ip;
    Value* slots;
} CallFrame;

#define GC_INITIAL_THRESHOLD (1024 * 64)
#define GC_GRAY_MAX 1024

/* Stack-based virtual machine with globals, call frames, and open upvalues */
struct VM {
    CallFrame frames[FRAMES_MAX];
    int frameCount;
    Value stack[STACK_MAX];
    Value* stackTop;
    char* globalKeys[256];
    Value globalVals[256];
    int globalCount;
    Upvalue* openUpvalues;
    bool hadError;
    AsterObject* objects;
    size_t bytesAllocated;
    size_t nextGC;
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

/* Like runSourceVM but writes final tracked heap bytes to bytesAfter when non-null */
bool runSourceVMEx(const char* source, const char* label, size_t* bytesAfter);

#endif /* ASTER_VM_H */
