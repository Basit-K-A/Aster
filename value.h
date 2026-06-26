#ifndef ASTER_VALUE_H
#define ASTER_VALUE_H

#include <stdbool.h>

#include "ast.h"
#include "object.h"

struct Chunk;
typedef enum {
    VAL_NUMBER, VAL_STRING, VAL_BOOL, VAL_NULL,
    VAL_FUNCTION, VAL_CLOSURE, VAL_CLASS, VAL_INSTANCE
} ValueType;

typedef struct AsterFunction AsterFunction;
typedef struct AsterClosure AsterClosure;
typedef struct AsterClass AsterClass;
typedef struct AsterInstance AsterInstance;
typedef struct Upvalue Upvalue;
typedef struct VM VM;

/* Compiled user-defined function object */
struct AsterFunction {
    AsterObject obj;
    char* name;
    int arity;
    int upvalueCount;
    char** params;
    AstNode* body;
    struct Chunk* chunk;
    bool hasBytecode;
};

/* Function plus captured upvalues */
struct AsterClosure {
    AsterObject obj;
    AsterFunction* function;
    Upvalue** upvalues;
    int upvalueCount;
};

/* Class object with a method table */
struct AsterClass {
    AsterObject obj;
    char* name;
    char** methodNames;
    AsterFunction** methods;
    int methodCount;
};

/* A dynamically-typed runtime value */
typedef struct {
    ValueType type;
    bool isGcString;
    union {
        double number;
        char* string;
        ObjString* stringObj;
        bool boolean;
        AsterFunction* function;
        AsterClosure* closure;
        AsterClass* klass;
        AsterInstance* instance;
    } as;
} Value;

/* Instance object with per-object fields */
struct AsterInstance {
    AsterObject obj;
    AsterClass* klass;
    char** fieldKeys;
    Value* fieldVals;
    int fieldCount;
};

/* An open or closed captured variable slot */
struct Upvalue {
    AsterObject obj;
    Value* location;
    Value closed;
    struct Upvalue* next;
};

/* Returns a null runtime value */
Value valueNull(void);

/* Returns a numeric runtime value */
Value valueNumber(double n);

/* Returns a boolean runtime value */
Value valueBool(bool b);

/* Returns a heap-owned string runtime value */
Value valueStringOwned(char* s);

/* Returns a function runtime value without taking ownership of the struct */
Value valueFunction(AsterFunction* fn);

/* Returns a closure runtime value without taking ownership of the struct */
Value valueClosure(AsterClosure* closure);

/* Returns a class runtime value without taking ownership of the struct */
Value valueClass(AsterClass* klass);

/* Returns an instance runtime value without taking ownership of the struct */
Value valueInstance(AsterInstance* instance);

/* Returns a heap-owned string runtime value backed by a GC object */
Value valueStringObj(ObjString* string);

/* Returns the C string payload for a string value (compile-time or GC-managed) */
const char* valueStringChars(Value v);

/* Deep-copies heap-owned parts of a value for independent storage */
Value valueCopy(Value v);

/* Deep-copies a value using the VM allocator for GC-managed strings */
Value valueCopyVm(VM* vm, Value v);

/* Frees heap-owned resources inside a runtime value (strings only) */
void valueFree(Value v);

/* Frees heap-owned resources including functions and closures when releasing stored values */
void valueRelease(Value v);

/* Frees a user-defined function and its owned name/param strings (non-GC sweep) */
void functionFreeBody(AsterFunction* fn);

/* Frees a closure shell without freeing its underlying function */
void closureFreeShell(AsterClosure* closure);

/* Frees class-owned tables without freeing the class header (non-GC sweep) */
void classFreeBody(AsterClass* klass);

/* Frees instance field storage without freeing the header (non-GC sweep) */
void instanceFreeBody(AsterInstance* instance);

/* Frees a user-defined function (interpreter / compile-time allocations) */
void functionFree(AsterFunction* fn);

/* Frees a closure shell (interpreter / compile-time allocations) */
void closureFree(AsterClosure* closure);

/* Frees a class object (interpreter / compile-time allocations) */
void classFree(AsterClass* klass);

/* Frees an instance object (interpreter / compile-time allocations) */
void instanceFree(AsterInstance* instance);

/* Builds an AsterFunction object from a function declaration AST node */
AsterFunction* functionFromNode(AstNode* node);

/* Prints a runtime value to stdout with a trailing newline */
void printValue(Value v);

/* Returns whether a runtime value is truthy in boolean context */
bool isTruthy(Value v);

/* Returns whether two runtime values are equal */
bool valuesEqual(Value a, Value b);

/* Converts any value to a newly allocated printable string */
char* valueToAllocatedString(Value v);

#endif /* ASTER_VALUE_H */
