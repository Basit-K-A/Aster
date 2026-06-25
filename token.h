#ifndef ASTER_TOKEN_H
#define ASTER_TOKEN_H

/* Token type enumeration */
typedef enum {
    /* Literals */
    TOKEN_NUMBER, TOKEN_STRING, TOKEN_IDENTIFIER,
    /* Keywords */
    TOKEN_LET, TOKEN_IF, TOKEN_ELSE, TOKEN_WHILE,
    TOKEN_FUNCTION, TOKEN_RETURN, TOKEN_PRINT, TOKEN_TRUE, TOKEN_FALSE, TOKEN_NULL,
    TOKEN_CLASS, TOKEN_THIS,
    /* Operators */
    TOKEN_PLUS, TOKEN_MINUS, TOKEN_STAR, TOKEN_SLASH, TOKEN_PERCENT,
    TOKEN_BANG, TOKEN_BANG_EQUAL,
    TOKEN_EQUAL, TOKEN_EQUAL_EQUAL,
    TOKEN_LESS, TOKEN_LESS_EQUAL,
    TOKEN_GREATER, TOKEN_GREATER_EQUAL,
    TOKEN_AND, TOKEN_OR,
    /* Delimiters */
    TOKEN_LPAREN, TOKEN_RPAREN,
    TOKEN_LBRACE, TOKEN_RBRACE,
    TOKEN_LBRACKET, TOKEN_RBRACKET,
    TOKEN_COMMA, TOKEN_SEMICOLON, TOKEN_DOT,
    /* Control */
    TOKEN_EOF, TOKEN_ERROR
} TokenType;

/* A single lexical token with source span and line info */
typedef struct {
    TokenType type;
    const char* start;
    int length;
    int line;
} Token;

#endif /* ASTER_TOKEN_H */
