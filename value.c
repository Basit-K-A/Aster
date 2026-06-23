#include "value.h"

#include "chunk.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Returns a null runtime value */
Value valueNull(void) {
    Value v;
    v.type = VAL_NULL;
    return v;
}

/* Returns a numeric runtime value */
Value valueNumber(double n) {
    Value v;
    v.type = VAL_NUMBER;
    v.as.number = n;
    return v;
}

/* Returns a boolean runtime value */
Value valueBool(bool b) {
    Value v;
    v.type = VAL_BOOL;
    v.as.boolean = b;
    return v;
}

/* Returns a heap-owned string runtime value */
Value valueStringOwned(char* s) {
    Value v;
    v.type = VAL_STRING;
    v.as.string = s;
    return v;
}

/* Returns a function runtime value without taking ownership of the struct */
Value valueFunction(AsterFunction* fn) {
    Value v;
    v.type = VAL_FUNCTION;
    v.as.function = fn;
    return v;
}

/* Deep-copies heap-owned parts of a value for independent storage */
Value valueCopy(Value v) {
    if (v.type == VAL_STRING && v.as.string) {
        return valueStringOwned(strdup(v.as.string));
    }
    return v;
}

/* Frees heap-owned resources inside a runtime value (strings only) */
void valueFree(Value v) {
    if (v.type == VAL_STRING) {
        free(v.as.string);
    }
}

/* Frees heap-owned resources including functions when releasing stored values */
void valueRelease(Value v) {
    if (v.type == VAL_STRING) {
        free(v.as.string);
    } else if (v.type == VAL_FUNCTION) {
        functionFree(v.as.function);
    }
}

/* Frees a user-defined function and its owned name/param strings */
void functionFree(AsterFunction* fn) {
    if (!fn) return;
    if (fn->hasBytecode && fn->chunk) {
        freeChunk(fn->chunk);
        free(fn->chunk);
    }
    free(fn->name);
    for (int i = 0; i < fn->arity; i++) {
        free(fn->params[i]);
    }
    free(fn->params);
    free(fn);
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
        case VAL_STRING: return v.as.string ? strdup(v.as.string) : strdup("");
        case VAL_BOOL:   return strdup(v.as.boolean ? "true" : "false");
        case VAL_NULL:   return strdup("null");
        case VAL_FUNCTION:
            return strdup(v.as.function && v.as.function->name ? v.as.function->name : "<fn>");
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
            printf("%s\n", v.as.string ? v.as.string : "");
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
    }
}

/* Returns whether two runtime values are equal */
bool valuesEqual(Value a, Value b) {
    if (a.type != b.type) return false;

    switch (a.type) {
        case VAL_NULL:     return true;
        case VAL_BOOL:     return a.as.boolean == b.as.boolean;
        case VAL_NUMBER:   return a.as.number == b.as.number;
        case VAL_STRING:
            if (!a.as.string || !b.as.string) return a.as.string == b.as.string;
            return strcmp(a.as.string, b.as.string) == 0;
        case VAL_FUNCTION: return a.as.function == b.as.function;
    }
    return false;
}

/* Returns whether a runtime value is truthy in boolean context */
bool isTruthy(Value v) {
    switch (v.type) {
        case VAL_NULL:     return false;
        case VAL_BOOL:     return v.as.boolean;
        case VAL_NUMBER:   return v.as.number != 0.0;
        case VAL_STRING:   return v.as.string != NULL && v.as.string[0] != '\0';
        case VAL_FUNCTION: return true;
    }
    return false;
}

/* Converts any value to a newly allocated string for concatenation */
char* valueToAllocatedString(Value v) {
    return valueToString(v);
}
