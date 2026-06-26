#include "value.h"

#include "chunk.h"
#include "object.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Returns the C string payload for a string value */
const char* valueStringChars(Value v) {
    if (v.type != VAL_STRING) return "";
    if (v.isGcString) {
        return v.as.stringObj && v.as.stringObj->chars ? v.as.stringObj->chars : "";
    }
    return v.as.string ? v.as.string : "";
}

/* Returns a null runtime value */
Value valueNull(void) {
    Value v;
    memset(&v, 0, sizeof(Value));
    v.type = VAL_NULL;
    return v;
}

/* Returns a numeric runtime value */
Value valueNumber(double n) {
    Value v;
    memset(&v, 0, sizeof(Value));
    v.type = VAL_NUMBER;
    v.as.number = n;
    return v;
}

/* Returns a boolean runtime value */
Value valueBool(bool b) {
    Value v;
    memset(&v, 0, sizeof(Value));
    v.type = VAL_BOOL;
    v.as.boolean = b;
    return v;
}

/* Returns a compile-time heap-owned string runtime value */
Value valueStringOwned(char* s) {
    Value v;
    memset(&v, 0, sizeof(Value));
    v.type = VAL_STRING;
    v.isGcString = false;
    v.as.string = s;
    return v;
}

/* Returns a GC-managed string runtime value */
Value valueStringObj(ObjString* string) {
    Value v;
    memset(&v, 0, sizeof(Value));
    v.type = VAL_STRING;
    v.isGcString = true;
    v.as.stringObj = string;
    return v;
}

/* Returns a function runtime value without taking ownership of the struct */
Value valueFunction(AsterFunction* fn) {
    Value v;
    memset(&v, 0, sizeof(Value));
    v.type = VAL_FUNCTION;
    v.as.function = fn;
    return v;
}

/* Returns a closure runtime value without taking ownership of the struct */
Value valueClosure(AsterClosure* closure) {
    Value v;
    memset(&v, 0, sizeof(Value));
    v.type = VAL_CLOSURE;
    v.as.closure = closure;
    return v;
}

/* Returns a class runtime value without taking ownership of the struct */
Value valueClass(AsterClass* klass) {
    Value v;
    memset(&v, 0, sizeof(Value));
    v.type = VAL_CLASS;
    v.as.klass = klass;
    return v;
}

/* Returns an instance runtime value without taking ownership of the struct */
Value valueInstance(AsterInstance* instance) {
    Value v;
    memset(&v, 0, sizeof(Value));
    v.type = VAL_INSTANCE;
    v.as.instance = instance;
    return v;
}

/* Deep-copies compile-time string values for the constant pool */
Value valueCopy(Value v) {
    if (v.type == VAL_STRING) {
        return valueStringOwned(strdup(valueStringChars(v)));
    }
    return v;
}

/* Deep-copies a value using the VM allocator for GC-managed strings */
Value valueCopyVm(VM* vm, Value v) {
    if (v.type == VAL_STRING) {
        ObjString* copied = copyString(vm, valueStringChars(v));
        if (!copied) return valueNull();
        return valueStringObj(copied);
    }
    return v;
}

/* Frees compile-time string buffers held directly by a value */
void valueFree(Value v) {
    if (v.type == VAL_STRING && !v.isGcString && v.as.string) {
        free(v.as.string);
    }
}

/* Releases compile-time owned resources when replacing stored values */
void valueRelease(Value v) {
    if (v.type == VAL_STRING && !v.isGcString && v.as.string) {
        free(v.as.string);
    } else if (v.type == VAL_FUNCTION) {
        functionFree(v.as.function);
    } else if (v.type == VAL_CLOSURE) {
        closureFree(v.as.closure);
    } else if (v.type == VAL_CLASS) {
        classFree(v.as.klass);
    } else if (v.type == VAL_INSTANCE) {
        instanceFree(v.as.instance);
    }
}

/* Frees bytecode and metadata owned by a function body */
void functionFreeBody(AsterFunction* fn) {
    if (!fn) return;
    if (fn->hasBytecode && fn->chunk) {
        freeChunk(fn->chunk);
        free(fn->chunk);
        fn->chunk = NULL;
    }
    free(fn->name);
    if (fn->params) {
        for (int i = 0; i < fn->arity; i++) {
            free(fn->params[i]);
        }
    }
    free(fn->params);
    fn->name = NULL;
    fn->params = NULL;
}

/* Frees a compile-time or interpreter-owned function object */
void functionFree(AsterFunction* fn) {
    if (!fn) return;
    functionFreeBody(fn);
    free(fn);
}

/* Frees closure-owned tables without freeing the closure header */
void closureFreeShell(AsterClosure* closure) {
    if (!closure) return;
    free(closure->upvalues);
    closure->upvalues = NULL;
}

/* Frees a compile-time or interpreter-owned closure object */
void closureFree(AsterClosure* closure) {
    if (!closure) return;
    closureFreeShell(closure);
    free(closure);
}

/* Frees class-owned tables without freeing the class header */
void classFreeBody(AsterClass* klass) {
    if (!klass) return;
    free(klass->name);
    for (int i = 0; i < klass->methodCount; i++) {
        free(klass->methodNames[i]);
    }
    free(klass->methodNames);
    free(klass->methods);
    klass->name = NULL;
    klass->methodNames = NULL;
    klass->methods = NULL;
    klass->methodCount = 0;
}

/* Frees a compile-time or interpreter-owned class object */
void classFree(AsterClass* klass) {
    if (!klass) return;
    classFreeBody(klass);
    free(klass);
}

/* Frees instance-owned field storage without freeing the header */
void instanceFreeBody(AsterInstance* instance) {
    if (!instance) return;
    for (int i = 0; i < instance->fieldCount; i++) {
        free(instance->fieldKeys[i]);
        valueFree(instance->fieldVals[i]);
    }
    free(instance->fieldKeys);
    free(instance->fieldVals);
    instance->fieldKeys = NULL;
    instance->fieldVals = NULL;
    instance->fieldCount = 0;
}

/* Frees a compile-time or interpreter-owned instance object */
void instanceFree(AsterInstance* instance) {
    if (!instance) return;
    instanceFreeBody(instance);
    free(instance);
}

/* Converts a number to a newly allocated string */
static char* numberToString(double n) {
    char buffer[64];
    snprintf(buffer, sizeof(buffer), "%.14g", n);
    return strdup(buffer);
}

/* Converts any value to a newly allocated printable string */
static char* valueToString(Value v) {
    switch (v.type) {
        case VAL_NUMBER: return numberToString(v.as.number);
        case VAL_STRING: return strdup(valueStringChars(v));
        case VAL_BOOL:   return strdup(v.as.boolean ? "true" : "false");
        case VAL_NULL:   return strdup("null");
        case VAL_FUNCTION:
            return strdup(v.as.function && v.as.function->name ? v.as.function->name : "<fn>");
        case VAL_CLOSURE:
            return strdup(v.as.closure && v.as.closure->function && v.as.closure->function->name
                ? v.as.closure->function->name : "<closure>");
        case VAL_CLASS:
            return strdup(v.as.klass && v.as.klass->name ? v.as.klass->name : "<class>");
        case VAL_INSTANCE:
            return strdup(v.as.instance && v.as.instance->klass && v.as.instance->klass->name
                ? v.as.instance->klass->name : "<instance>");
    }
    return strdup("");
}

/* Builds an AsterFunction object from a function declaration AST node */
AsterFunction* functionFromNode(AstNode* node) {
    AsterFunction* fn = (AsterFunction*)calloc(1, sizeof(AsterFunction));
    if (!fn) return NULL;

    fn->name = strdup(node->as.funcDecl.name);
    fn->arity = node->as.funcDecl.paramCount;
    fn->body = node->as.funcDecl.body;

    if (fn->arity > 0) {
        fn->params = (char**)calloc((size_t)fn->arity, sizeof(char*));
        if (!fn->params) {
            functionFree(fn);
            return NULL;
        }
        for (int i = 0; i < fn->arity; i++) {
            fn->params[i] = strdup(node->as.funcDecl.params[i]);
        }
    }

    return fn;
}

/* Prints a runtime value to stdout with a trailing newline */
void printValue(Value v) {
    switch (v.type) {
        case VAL_NUMBER:
            printf("%g\n", v.as.number);
            break;
        case VAL_STRING:
            printf("%s\n", valueStringChars(v));
            break;
        case VAL_BOOL:
            printf("%s\n", v.as.boolean ? "true" : "false");
            break;
        case VAL_NULL:
            printf("null\n");
            break;
        case VAL_FUNCTION:
            printf("<fn %s>\n", v.as.function && v.as.function->name ? v.as.function->name : "");
            break;
        case VAL_CLOSURE:
            printf("<closure %s>\n",
                v.as.closure && v.as.closure->function && v.as.closure->function->name
                    ? v.as.closure->function->name : "");
            break;
        case VAL_CLASS:
            printf("<class %s>\n", v.as.klass && v.as.klass->name ? v.as.klass->name : "");
            break;
        case VAL_INSTANCE:
            printf("<instance %s>\n",
                v.as.instance && v.as.instance->klass && v.as.instance->klass->name
                    ? v.as.instance->klass->name : "");
            break;
    }
}

/* Returns whether two runtime values are equal */
bool valuesEqual(Value a, Value b) {
    if (a.type != b.type) return false;

    switch (a.type) {
        case VAL_NULL:     return true;
        case VAL_BOOL:     return a.as.boolean == b.as.boolean;
        case VAL_NUMBER:   return a.as.number == b.as.number;
        case VAL_STRING:   return strcmp(valueStringChars(a), valueStringChars(b)) == 0;
        case VAL_FUNCTION: return a.as.function == b.as.function;
        case VAL_CLOSURE:  return a.as.closure == b.as.closure;
        case VAL_CLASS:    return a.as.klass == b.as.klass;
        case VAL_INSTANCE: return a.as.instance == b.as.instance;
    }
    return false;
}

/* Returns whether a runtime value is truthy in boolean context */
bool isTruthy(Value v) {
    switch (v.type) {
        case VAL_NULL:     return false;
        case VAL_BOOL:     return v.as.boolean;
        case VAL_NUMBER:   return v.as.number != 0.0;
        case VAL_STRING:   return valueStringChars(v)[0] != '\0';
        case VAL_FUNCTION: return true;
        case VAL_CLOSURE:  return true;
        case VAL_CLASS:    return true;
        case VAL_INSTANCE: return true;
    }
    return false;
}

/* Converts any value to a newly allocated string for concatenation */
char* valueToAllocatedString(Value v) {
    return valueToString(v);
}
