#include "object.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "chunk.h"
#include "vm.h"

static AsterObject* grayStack[GC_GRAY_MAX];
static int grayCount = 0;

static void markValue(VM* vm, Value value);
static void markObject(VM* vm, AsterObject* object);
static void blackenObject(VM* vm, AsterObject* object);
static void freeObject(VM* vm, AsterObject* object);

/* Returns whether an object header belongs to the VM heap list */
static bool objectOnVm(VM* vm, AsterObject* object) {
    for (AsterObject* current = vm->objects; current != NULL; current = current->next) {
        if (current == object) return true;
    }
    return false;
}

/* Allocates a GC-tracked object and prepends it to the VM object list */
void* allocateObject(VM* vm, size_t size, ObjType type) {
    if (vm->bytesAllocated + size > vm->nextGC) {
        collectGarbage(vm);
    }

    AsterObject* object = (AsterObject*)calloc(1, size);
    if (!object) return NULL;

    object->isMarked = false;
    object->type = type;
    object->next = vm->objects;
    vm->objects = object;
    vm->bytesAllocated += size;

    return object;
}

/* Copies a C string into a new GC-managed string object */
ObjString* copyString(VM* vm, const char* chars) {
    size_t charSize = chars ? strlen(chars) + 1 : 1;
    if (vm->bytesAllocated + sizeof(ObjString) + charSize > vm->nextGC) {
        collectGarbage(vm);
    }

    ObjString* string = (ObjString*)allocateObject(vm, sizeof(ObjString), OBJ_STRING);
    if (!string) return NULL;

    if (chars == NULL) chars = "";
    string->chars = strdup(chars);
    if (!string->chars) return NULL;

    vm->bytesAllocated += charSize;
    return string;
}

/* Returns the number of bytes currently tracked by the VM allocator */
size_t vmBytesAllocated(const VM* vm) {
    return vm->bytesAllocated;
}

/* Adds an object to the gray worklist for tracing */
static void grayObject(AsterObject* object) {
    if (object == NULL || object->isMarked) return;
    if (grayCount >= GC_GRAY_MAX) return;

    object->isMarked = true;
    grayStack[grayCount++] = object;
}

/* Marks a heap object and enqueues it for reference tracing */
static void markObject(VM* vm, AsterObject* object) {
    (void)vm;
    grayObject(object);
}

/* Marks a chunk constant pool and all values it references */
static void markChunk(VM* vm, Chunk* chunk) {
    if (!chunk) return;
    for (int i = 0; i < chunk->constCount; i++) {
        markValue(vm, chunk->constants[i]);
    }
}

/* Marks a function prototype and its bytecode constants */
static void markFunction(VM* vm, AsterFunction* function) {
    if (!function) return;
    if (objectOnVm(vm, &function->obj)) {
        markObject(vm, &function->obj);
    }
    markChunk(vm, function->chunk);
}

/* Traces outgoing references from a marked heap object */
static void blackenObject(VM* vm, AsterObject* object) {
    switch (object->type) {
        case OBJ_STRING:
            break;
        case OBJ_FUNCTION: {
            AsterFunction* function = (AsterFunction*)object;
            markChunk(vm, function->chunk);
            break;
        }
        case OBJ_CLOSURE: {
            AsterClosure* closure = (AsterClosure*)object;
            markFunction(vm, closure->function);
            for (int i = 0; i < closure->upvalueCount; i++) {
                Upvalue* upvalue = closure->upvalues[i];
                if (upvalue) markObject(vm, &upvalue->obj);
            }
            break;
        }
        case OBJ_CLASS: {
            AsterClass* klass = (AsterClass*)object;
            for (int i = 0; i < klass->methodCount; i++) {
                markFunction(vm, klass->methods[i]);
            }
            break;
        }
        case OBJ_INSTANCE: {
            AsterInstance* instance = (AsterInstance*)object;
            if (instance->klass && objectOnVm(vm, &instance->klass->obj)) {
                markObject(vm, &instance->klass->obj);
            }
            for (int i = 0; i < instance->fieldCount; i++) {
                markValue(vm, instance->fieldVals[i]);
            }
            break;
        }
        case OBJ_UPVALUE: {
            Upvalue* upvalue = (Upvalue*)object;
            if (upvalue->location) {
                markValue(vm, *upvalue->location);
            } else {
                markValue(vm, upvalue->closed);
            }
            break;
        }
    }
}

/* Marks a runtime value and everything it references */
static void markValue(VM* vm, Value value) {
    switch (value.type) {
        case VAL_STRING:
            if (value.isGcString && value.as.stringObj) {
                markObject(vm, &value.as.stringObj->obj);
            }
            break;
        case VAL_FUNCTION:
            markFunction(vm, value.as.function);
            break;
        case VAL_CLOSURE:
            markObject(vm, &value.as.closure->obj);
            break;
        case VAL_CLASS:
            markObject(vm, &value.as.klass->obj);
            break;
        case VAL_INSTANCE:
            markObject(vm, &value.as.instance->obj);
            break;
        default:
            break;
    }
}

/* Drains the gray worklist until all reachable objects are black */
static void traceReferences(VM* vm) {
    while (grayCount > 0) {
        AsterObject* object = grayStack[--grayCount];
        blackenObject(vm, object);
    }
}

/* Marks call frames, stack slots, globals, and open upvalues as GC roots */
static void markRoots(VM* vm) {
    for (Value* slot = vm->stack; slot < vm->stackTop; slot++) {
        markValue(vm, *slot);
    }

    for (int i = 0; i < vm->globalCount; i++) {
        markValue(vm, vm->globalVals[i]);
    }

    for (Upvalue* upvalue = vm->openUpvalues; upvalue != NULL; upvalue = upvalue->next) {
        markObject(vm, &upvalue->obj);
    }

    for (int i = 0; i < vm->frameCount; i++) {
        CallFrame* frame = &vm->frames[i];
        if (frame->closure) {
            markObject(vm, &frame->closure->obj);
        }
        markChunk(vm, frame->chunk);
    }
}

/* Frees an unreachable GC object and updates allocation accounting */
static void freeObject(VM* vm, AsterObject* object) {
    size_t objectSize = sizeof(ObjString);
    switch (object->type) {
        case OBJ_STRING:
            objectSize = sizeof(ObjString);
            break;
        case OBJ_FUNCTION:
            objectSize = sizeof(AsterFunction);
            break;
        case OBJ_CLOSURE:
            objectSize = sizeof(AsterClosure);
            break;
        case OBJ_CLASS:
            objectSize = sizeof(AsterClass);
            break;
        case OBJ_INSTANCE:
            objectSize = sizeof(AsterInstance);
            break;
        case OBJ_UPVALUE:
            objectSize = sizeof(Upvalue);
            break;
    }
    vm->bytesAllocated -= objectSize;

    switch (object->type) {
        case OBJ_STRING: {
            ObjString* string = (ObjString*)object;
            if (string->chars) {
                vm->bytesAllocated -= strlen(string->chars) + 1;
                free(string->chars);
            }
            free(string);
            break;
        }
        case OBJ_FUNCTION: {
            AsterFunction* function = (AsterFunction*)object;
            functionFreeBody(function);
            free(function);
            break;
        }
        case OBJ_CLOSURE: {
            AsterClosure* closure = (AsterClosure*)object;
            closureFreeShell(closure);
            free(closure);
            break;
        }
        case OBJ_CLASS: {
            AsterClass* klass = (AsterClass*)object;
            classFreeBody(klass);
            free(klass);
            break;
        }
        case OBJ_INSTANCE: {
            AsterInstance* instance = (AsterInstance*)object;
            instanceFreeBody(instance);
            free(instance);
            break;
        }
        case OBJ_UPVALUE:
            free(object);
            break;
    }
}

/* Runs mark-and-sweep garbage collection on the VM heap */
void collectGarbage(VM* vm) {
    grayCount = 0;
    markRoots(vm);
    traceReferences(vm);

    AsterObject** cursor = &vm->objects;
    while (*cursor) {
        if (!(*cursor)->isMarked) {
            AsterObject* unreached = *cursor;
            *cursor = unreached->next;
            freeObject(vm, unreached);
        } else {
            (*cursor)->isMarked = false;
            cursor = &(*cursor)->next;
        }
    }

    vm->nextGC = vm->bytesAllocated * 2;
    if (vm->nextGC < GC_INITIAL_THRESHOLD) {
        vm->nextGC = GC_INITIAL_THRESHOLD;
    }
}

/* Frees every object on the VM heap without running collection */
void freeObjects(VM* vm) {
    AsterObject* object = vm->objects;
    while (object != NULL) {
        AsterObject* next = object->next;
        freeObject(vm, object);
        object = next;
    }
    vm->objects = NULL;
    vm->bytesAllocated = 0;
    vm->nextGC = GC_INITIAL_THRESHOLD;
}
