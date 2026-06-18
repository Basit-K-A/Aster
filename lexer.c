#include "lexer.h"

#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

/* Keyword table entry mapping lexeme to token type */
typedef struct {
    const char* name;
    int length;
    TokenType type;
} Keyword;

static const Keyword keywords[] = {
    {"let",      3, TOKEN_LET},
    {"if",       2, TOKEN_IF},
    {"else",     4, TOKEN_ELSE},
    {"while",    5, TOKEN_WHILE},
    {"function", 8, TOKEN_FUNCTION},
    {"return",   6, TOKEN_RETURN},
    {"print",    5, TOKEN_PRINT},
    {"true",     4, TOKEN_TRUE},
    {"false",    5, TOKEN_FALSE},
    {"null",     4, TOKEN_NULL},
};

static const int keywordCount = sizeof(keywords) / sizeof(keywords[0]);

/* Returns true if the lexer has reached the end of the source */
static bool isAtEnd(Lexer* l) {
    return *l->current == '\0';
}

/* Advances the lexer one character and returns the consumed character */
static char advance(Lexer* l) {
    return *l->current++;
}

/* Returns the current character without consuming it */
static char peek(Lexer* l) {
    return *l->current;
}

/* Returns the next character without consuming it */
static char peekNext(Lexer* l) {
    if (isAtEnd(l)) return '\0';
    return l->current[1];
}

/* Consumes the current character if it matches the expected one */
static bool match(Lexer* l, char expected) {
    if (isAtEnd(l)) return false;
    if (*l->current != expected) return false;
    l->current++;
    return true;
}

/* Builds an error token spanning from start to current position */
static Token makeErrorToken(Lexer* l, const char* message) {
    (void)message;
    Token token;
    token.type = TOKEN_ERROR;
    token.start = l->start;
    token.length = (int)(l->current - l->start);
    token.line = l->line;
    return token;
}

/* Builds a token of the given type spanning from start to current position */
static Token makeToken(Lexer* l, TokenType type) {
    Token token;
    token.type = type;
    token.start = l->start;
    token.length = (int)(l->current - l->start);
    token.line = l->line;
    return token;
}

/* Skips whitespace and comments until a significant character is found */
static void skipWhitespace(Lexer* l) {
    for (;;) {
        char c = peek(l);
        switch (c) {
            case ' ':
            case '\r':
            case '\t':
                advance(l);
                break;
            case '\n':
                l->line++;
                advance(l);
                break;
            case '/':
                if (peekNext(l) == '/') {
                    while (peek(l) != '\n' && !isAtEnd(l)) advance(l);
                } else {
                    return;
                }
                break;
            default:
                return;
        }
    }
}

/* Checks whether a character is valid inside an identifier */
static bool isIdentifierChar(char c) {
    return isalnum((unsigned char)c) || c == '_';
}

/* Looks up an identifier lexeme in the keyword table */
static TokenType identifierType(Lexer* l) {
    for (int i = 0; i < keywordCount; i++) {
        if (l->current - l->start == keywords[i].length &&
            memcmp(l->start, keywords[i].name, (size_t)keywords[i].length) == 0) {
            return keywords[i].type;
        }
    }
    return TOKEN_IDENTIFIER;
}

/* Scans a numeric literal (integer or decimal) */
static Token number(Lexer* l) {
    while (isdigit((unsigned char)peek(l))) advance(l);

    if (peek(l) == '.' && isdigit((unsigned char)peekNext(l))) {
        advance(l);
        while (isdigit((unsigned char)peek(l))) advance(l);
    }

    return makeToken(l, TOKEN_NUMBER);
}

/* Scans a string literal enclosed in double quotes */
static Token string(Lexer* l) {
    while (peek(l) != '"' && !isAtEnd(l)) {
        if (peek(l) == '\n') l->line++;
        advance(l);
    }

    if (isAtEnd(l)) return makeErrorToken(l, "Unterminated string.");

    advance(l);
    return makeToken(l, TOKEN_STRING);
}

/* Scans an identifier or keyword */
static Token identifier(Lexer* l) {
    while (isIdentifierChar(peek(l))) advance(l);
    return makeToken(l, identifierType(l));
}

/* Initializes a lexer to scan the given source string */
void initLexer(Lexer* l, const char* source) {
    l->start = source;
    l->current = source;
    l->line = 1;
}

/* Returns the next token from the source */
Token nextToken(Lexer* l) {
    skipWhitespace(l);
    l->start = l->current;

    if (isAtEnd(l)) return makeToken(l, TOKEN_EOF);

    char c = advance(l);

    if (isdigit((unsigned char)c)) return number(l);

    if (isalpha((unsigned char)c) || c == '_') return identifier(l);

    switch (c) {
        case '(': return makeToken(l, TOKEN_LPAREN);
        case ')': return makeToken(l, TOKEN_RPAREN);
        case '{': return makeToken(l, TOKEN_LBRACE);
        case '}': return makeToken(l, TOKEN_RBRACE);
        case '[': return makeToken(l, TOKEN_LBRACKET);
        case ']': return makeToken(l, TOKEN_RBRACKET);
        case ',': return makeToken(l, TOKEN_COMMA);
        case '.': return makeToken(l, TOKEN_DOT);
        case '-': return makeToken(l, TOKEN_MINUS);
        case '+': return makeToken(l, TOKEN_PLUS);
        case ';': return makeToken(l, TOKEN_SEMICOLON);
        case '%': return makeToken(l, TOKEN_PERCENT);
        case '*': return makeToken(l, TOKEN_STAR);
        case '/': return makeToken(l, TOKEN_SLASH);
        case '!': return makeToken(l, match(l, '=') ? TOKEN_BANG_EQUAL : TOKEN_BANG);
        case '=': return makeToken(l, match(l, '=') ? TOKEN_EQUAL_EQUAL : TOKEN_EQUAL);
        case '<': return makeToken(l, match(l, '=') ? TOKEN_LESS_EQUAL : TOKEN_LESS);
        case '>': return makeToken(l, match(l, '=') ? TOKEN_GREATER_EQUAL : TOKEN_GREATER);
        case '&': return makeToken(l, match(l, '&') ? TOKEN_AND : TOKEN_ERROR);
        case '|': return makeToken(l, match(l, '|') ? TOKEN_OR : TOKEN_ERROR);
        case '"': return string(l);
    }

    return makeErrorToken(l, "Unexpected character.");
}

/* Returns a human-readable name for a token type */
static const char* tokenTypeName(TokenType type) {
    switch (type) {
        case TOKEN_NUMBER:      return "NUMBER";
        case TOKEN_STRING:      return "STRING";
        case TOKEN_IDENTIFIER:  return "IDENTIFIER";
        case TOKEN_LET:         return "LET";
        case TOKEN_IF:          return "IF";
        case TOKEN_ELSE:        return "ELSE";
        case TOKEN_WHILE:       return "WHILE";
        case TOKEN_FUNCTION:    return "FUNCTION";
        case TOKEN_RETURN:      return "RETURN";
        case TOKEN_PRINT:       return "PRINT";
        case TOKEN_TRUE:        return "TRUE";
        case TOKEN_FALSE:       return "FALSE";
        case TOKEN_NULL:        return "NULL";
        case TOKEN_PLUS:        return "PLUS";
        case TOKEN_MINUS:       return "MINUS";
        case TOKEN_STAR:        return "STAR";
        case TOKEN_SLASH:       return "SLASH";
        case TOKEN_PERCENT:     return "PERCENT";
        case TOKEN_BANG:        return "BANG";
        case TOKEN_BANG_EQUAL:  return "BANG_EQUAL";
        case TOKEN_EQUAL:       return "EQUAL";
        case TOKEN_EQUAL_EQUAL: return "EQUAL_EQUAL";
        case TOKEN_LESS:        return "LESS";
        case TOKEN_LESS_EQUAL:  return "LESS_EQUAL";
        case TOKEN_GREATER:     return "GREATER";
        case TOKEN_GREATER_EQUAL: return "GREATER_EQUAL";
        case TOKEN_AND:         return "AND";
        case TOKEN_OR:          return "OR";
        case TOKEN_LPAREN:      return "LPAREN";
        case TOKEN_RPAREN:      return "RPAREN";
        case TOKEN_LBRACE:      return "LBRACE";
        case TOKEN_RBRACE:      return "RBRACE";
        case TOKEN_LBRACKET:    return "LBRACKET";
        case TOKEN_RBRACKET:    return "RBRACKET";
        case TOKEN_COMMA:       return "COMMA";
        case TOKEN_SEMICOLON:   return "SEMICOLON";
        case TOKEN_DOT:         return "DOT";
        case TOKEN_EOF:         return "EOF";
        case TOKEN_ERROR:       return "ERROR";
    }
    return "UNKNOWN";
}

/* Prints a token's type, lexeme, and line number for debugging */
void printToken(Token t) {
    printf("%-16s '", tokenTypeName(t.type));
    for (int i = 0; i < t.length; i++) {
        char c = t.start[i];
        if (c == '\n') {
            printf("\\n");
        } else if (c == '\t') {
            printf("\\t");
        } else if (c == '\r') {
            printf("\\r");
        } else {
            putchar(c);
        }
    }
    printf("' (line %d)\n", t.line);
}
