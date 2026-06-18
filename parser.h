#ifndef ASTER_PARSER_H
#define ASTER_PARSER_H

#include "ast.h"

/* Parses source text and returns the program AST root node */
AstNode* parse(const char* source);

#endif /* ASTER_PARSER_H */
