#ifndef ASTER_OBJECT_H
#define ASTER_OBJECT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct VM VM;

/* Heap object kinds tracked by the garbage collector */
typedef enum {
    OBJ_STRING,
    OBJ_FUNCTION,
    OBJ_CLOSURE,
    OBJ_CLASS,
    OBJ_INSTANCE,
    OBJ_UPVALUE,
} ObjType;

/* Intrusive header for all GC-managed heap objects */
typedef struct AsterObject {
    bool isMarked;
    struct AsterObject* next;
    ObjType type;
} AsterObject;

/* GC-managed string object */
typedef struct ObjString {
    AsterObject obj;
    char* chars;
} ObjString;

/* Allocates a GC-tracked object and prepends it to the VM object list */
void* allocateObject(VM* vm, size_t size, ObjType type);

/* Copies a C string into a new GC-managed string object */
ObjString* copyString(VM* vm, const char* chars);

/* Returns the number of bytes currently tracked by the VM allocator */
size_t vmBytesAllocated(const VM* vm);

/* Runs mark-and-sweep garbage collection on the VM heap */
void collectGarbage(VM* vm);

/* Frees every object on the VM heap without running collection */
void freeObjects(VM* vm);

#endif /* ASTER_OBJECT_H */
