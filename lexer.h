#ifndef ASTER_LEXER_H
#define ASTER_LEXER_H

#include "token.h"

/* Lexer state for scanning source text */
typedef struct {
    const char* start;
    const char* current;
    int line;
} Lexer;

/* Initializes a lexer to scan the given source string */
void initLexer(Lexer* l, const char* source);

/* Returns the next token from the source */
Token nextToken(Lexer* l);

/* Prints a token's type, lexeme, and line number for debugging */
void printToken(Token t);

#endif /* ASTER_LEXER_H */
