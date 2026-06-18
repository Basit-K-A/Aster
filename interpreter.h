#ifndef ASTER_INTERPRETER_H
#define ASTER_INTERPRETER_H

#include <stdbool.h>

#include "ast.h"
#include "env.h"
#include "value.h"

/* Tree-walk interpreter state with globals and return control */
typedef struct {
    Env* globals;
    bool hadError;
    bool hasReturn;
    Value returnValue;
} Interpreter;

/* Evaluates any AST node, executing statements or returning expression values */
Value interpret(AstNode* node, Interpreter* interp, Env* env);

/* Parses and executes source code, printing a labeled test header */
bool runSource(const char* source, const char* label);

#endif /* ASTER_INTERPRETER_H */
