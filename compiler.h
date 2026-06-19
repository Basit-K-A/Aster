#ifndef ASTER_COMPILER_H
#define ASTER_COMPILER_H

#include <stdbool.h>

#include "ast.h"
#include "chunk.h"

/* Bytecode compiler state with target chunk and error flag */
typedef struct {
    Chunk* chunk;
    bool hadError;
} Compiler;

/* Compiles an AST node into bytecode in the given compiler's chunk */
void compile(AstNode* node, Compiler* compiler);

#endif /* ASTER_COMPILER_H */
