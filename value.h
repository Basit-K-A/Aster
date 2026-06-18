#ifndef ASTER_VALUE_H
#define ASTER_VALUE_H

#include <stdbool.h>

#include "ast.h"

/* Runtime value type enumeration */
typedef enum {
    VAL_NUMBER, VAL_STRING, VAL_BOOL, VAL_NULL, VAL_FUNCTION
} ValueType;

typedef struct AsterFunction AsterFunction;

/* User-defined function for tree-walk execution */
struct AsterFunction {
    char* name;
    char** params;
    int paramCount;
    AstNode* body;
};

/* A dynamically-typed runtime value */
typedef struct {
    ValueType type;
    union {
        double number;
        char* string;
        bool boolean;
        AsterFunction* function;
    } as;
} Value;

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

/* Deep-copies heap-owned parts of a value for independent storage */
Value valueCopy(Value v);

/* Frees heap-owned resources inside a runtime value */
void valueFree(Value v);

/* Frees a user-defined function and its owned name/param strings */
void functionFree(AsterFunction* fn);

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
