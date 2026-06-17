/* Aster Language — Phase 2: Lexer + Parser & AST */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

/* Token type enumeration */
typedef enum {
    /* Literals */
    TOKEN_NUMBER, TOKEN_STRING, TOKEN_IDENTIFIER,
    /* Keywords */
    TOKEN_LET, TOKEN_IF, TOKEN_ELSE, TOKEN_WHILE,
    TOKEN_FUNCTION, TOKEN_RETURN, TOKEN_PRINT, TOKEN_TRUE, TOKEN_FALSE, TOKEN_NULL,
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

/* Lexer state for scanning source text */
typedef struct {
    const char* start;
    const char* current;
    int line;
} Lexer;

/* AST node type enumeration */
typedef enum {
    NODE_NUMBER, NODE_STRING, NODE_BOOL, NODE_NULL,
    NODE_IDENTIFIER,
    NODE_BINARY, NODE_UNARY,
    NODE_ASSIGNMENT,
    NODE_VAR_DECL,
    NODE_BLOCK,
    NODE_IF,
    NODE_WHILE,
    NODE_FUNCTION_DECL,
    NODE_CALL,
    NODE_RETURN,
    NODE_PRINT,
} NodeType;

/* Binary operator kinds for NODE_BINARY nodes */
typedef enum {
    BINOP_ADD, BINOP_SUB, BINOP_MUL, BINOP_DIV, BINOP_MOD,
    BINOP_EQ, BINOP_NEQ, BINOP_LT, BINOP_LTE, BINOP_GT, BINOP_GTE,
    BINOP_AND, BINOP_OR,
} BinOp;

/* Unary operator kinds for NODE_UNARY nodes */
typedef enum {
    UNOP_NEG, UNOP_NOT,
} UnOp;

typedef struct AstNode AstNode;

/* Parser state holding lexer position and error flag */
typedef struct {
    Lexer lexer;
    Token current;
    Token previous;
    bool hadError;
} Parser;

/* Recursively frees an AST node and all owned heap memory */
void freeAst(AstNode* node);

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

/* Tagged union AST node with pointer-based children */
struct AstNode {
    NodeType type;
    int line;
    union {
        double number;
        char* string;
        char* name;
        bool boolean;
        struct {
            BinOp op;
            AstNode* left;
            AstNode* right;
        } binary;
        struct {
            UnOp op;
            AstNode* operand;
        } unary;
        struct {
            char* name;
            AstNode* value;
        } assignment;
        struct {
            char* name;
            AstNode* initializer;
        } varDecl;
        struct {
            AstNode** statements;
            int count;
            int capacity;
        } block;
        struct {
            AstNode* condition;
            AstNode* thenBranch;
            AstNode* elseBranch;
        } ifStmt;
        struct {
            AstNode* condition;
            AstNode* body;
        } whileStmt;
        struct {
            char* name;
            char** params;
            int paramCount;
            AstNode* body;
        } funcDecl;
        struct {
            AstNode* callee;
            AstNode** args;
            int argCount;
        } call;
        struct {
            AstNode* value;
        } returnStmt;
        struct {
            AstNode* value;
        } printStmt;
    } as;
};

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

/* Allocates a heap copy of a token's lexeme bytes */
static char* copyTokenText(Token token) {
    char* copy = (char*)malloc((size_t)token.length + 1);
    if (!copy) return NULL;
    memcpy(copy, token.start, (size_t)token.length);
    copy[token.length] = '\0';
    return copy;
}

/* Parses a numeric token lexeme into a double value */
static double tokenToNumber(Token token) {
    char buffer[64];
    int len = token.length < 63 ? token.length : 63;
    memcpy(buffer, token.start, (size_t)len);
    buffer[len] = '\0';
    return strtod(buffer, NULL);
}

/* Allocates a heap copy of a string literal without surrounding quotes */
static char* copyStringLiteral(Token token) {
    if (token.length < 2) return copyTokenText(token);
    char* copy = (char*)malloc((size_t)token.length - 1);
    if (!copy) return NULL;
    memcpy(copy, token.start + 1, (size_t)token.length - 2);
    copy[token.length - 2] = '\0';
    return copy;
}

/* Allocates a new empty NODE_BLOCK AST node */
static AstNode* newBlockNode(int line) {
    AstNode* node = (AstNode*)calloc(1, sizeof(AstNode));
    if (!node) return NULL;
    node->type = NODE_BLOCK;
    node->line = line;
    return node;
}

/* Appends a statement node to a block, growing the array as needed */
static void blockAppend(AstNode* block, AstNode* stmt) {
    if (!block || block->type != NODE_BLOCK || !stmt) return;

    if (block->as.block.count >= block->as.block.capacity) {
        int newCap = block->as.block.capacity < 8 ? 8 : block->as.block.capacity * 2;
        AstNode** stmts = (AstNode**)realloc(
            block->as.block.statements,
            (size_t)newCap * sizeof(AstNode*));
        if (!stmts) return;
        block->as.block.statements = stmts;
        block->as.block.capacity = newCap;
    }

    block->as.block.statements[block->as.block.count++] = stmt;
}

/* Records a parse error at the previous token location */
static void parserErrorAt(Parser* p, Token token, const char* message) {
    if (p->hadError) return;
    p->hadError = true;
    fprintf(stderr, "[line %d] Error", token.line);
    if (token.type == TOKEN_EOF) {
        fprintf(stderr, " at end");
    } else if (token.type == TOKEN_ERROR) {
        /* Lexer already reported position via token span */
    } else {
        fprintf(stderr, " at '");
        for (int i = 0; i < token.length; i++) putc(token.start[i], stderr);
        fprintf(stderr, "'");
    }
    fprintf(stderr, ": %s\n", message);
}

/* Records a parse error at the current token */
static void parserErrorAtCurrent(Parser* p, const char* message) {
    parserErrorAt(p, p->current, message);
}

/* Advances the parser to the next token */
static void parserAdvance(Parser* p) {
    p->previous = p->current;
    p->current = nextToken(&p->lexer);
    if (p->current.type == TOKEN_ERROR) {
        parserErrorAt(p, p->current, "Invalid token.");
    }
}

/* Returns true when the current token matches the given type */
static bool parserCheck(Parser* p, TokenType type) {
    return p->current.type == type;
}

/* Consumes the current token when it matches the expected type */
static bool parserMatch(Parser* p, TokenType type) {
    if (!parserCheck(p, type)) return false;
    parserAdvance(p);
    return true;
}

/* Requires the current token to be of the given type or reports an error */
static void parserConsume(Parser* p, TokenType type, const char* message) {
    if (parserCheck(p, type)) {
        parserAdvance(p);
        return;
    }
    parserErrorAtCurrent(p, message);
}

/* Skips tokens until a likely statement or declaration boundary is found */
static void parserSynchronize(Parser* p) {
    p->hadError = false;

    while (p->current.type != TOKEN_EOF) {
        if (p->previous.type == TOKEN_SEMICOLON) return;

        switch (p->current.type) {
            case TOKEN_LET:
            case TOKEN_FUNCTION:
            case TOKEN_IF:
            case TOKEN_WHILE:
            case TOKEN_RETURN:
            case TOKEN_PRINT:
                return;
            default:
                break;
        }

        parserAdvance(p);
    }
}

static AstNode* expression(Parser* p);
static AstNode* declaration(Parser* p);
static AstNode* statement(Parser* p);

/* Parses a primary expression literal or parenthesized sub-expression */
static AstNode* primary(Parser* p) {
    if (parserMatch(p, TOKEN_NUMBER)) {
        AstNode* node = (AstNode*)calloc(1, sizeof(AstNode));
        if (!node) return NULL;
        node->type = NODE_NUMBER;
        node->line = p->previous.line;
        node->as.number = tokenToNumber(p->previous);
        return node;
    }

    if (parserMatch(p, TOKEN_STRING)) {
        AstNode* node = (AstNode*)calloc(1, sizeof(AstNode));
        if (!node) return NULL;
        node->type = NODE_STRING;
        node->line = p->previous.line;
        node->as.string = copyStringLiteral(p->previous);
        return node;
    }

    if (parserMatch(p, TOKEN_TRUE)) {
        AstNode* node = (AstNode*)calloc(1, sizeof(AstNode));
        if (!node) return NULL;
        node->type = NODE_BOOL;
        node->line = p->previous.line;
        node->as.boolean = true;
        return node;
    }

    if (parserMatch(p, TOKEN_FALSE)) {
        AstNode* node = (AstNode*)calloc(1, sizeof(AstNode));
        if (!node) return NULL;
        node->type = NODE_BOOL;
        node->line = p->previous.line;
        node->as.boolean = false;
        return node;
    }

    if (parserMatch(p, TOKEN_NULL)) {
        AstNode* node = (AstNode*)calloc(1, sizeof(AstNode));
        if (!node) return NULL;
        node->type = NODE_NULL;
        node->line = p->previous.line;
        return node;
    }

    if (parserMatch(p, TOKEN_IDENTIFIER)) {
        AstNode* node = (AstNode*)calloc(1, sizeof(AstNode));
        if (!node) return NULL;
        node->type = NODE_IDENTIFIER;
        node->line = p->previous.line;
        node->as.name = copyTokenText(p->previous);
        return node;
    }

    if (parserMatch(p, TOKEN_LPAREN)) {
        AstNode* expr = expression(p);
        parserConsume(p, TOKEN_RPAREN, "Expected ')' after expression.");
        return expr;
    }

    parserErrorAtCurrent(p, "Expected expression.");
    return NULL;
}

/* Finishes parsing a call chain starting from an already-parsed callee node */
static AstNode* finishCall(Parser* p, AstNode* callee) {
    AstNode** args = NULL;
    int argCount = 0;
    int argCapacity = 0;

    if (!parserCheck(p, TOKEN_RPAREN)) {
        do {
            if (argCount >= argCapacity) {
                int newCap = argCapacity < 4 ? 4 : argCapacity * 2;
                AstNode** newArgs = (AstNode**)realloc(args, (size_t)newCap * sizeof(AstNode*));
                if (!newArgs) {
                    free(args);
                    return NULL;
                }
                args = newArgs;
                argCapacity = newCap;
            }
            args[argCount++] = expression(p);
        } while (parserMatch(p, TOKEN_COMMA));
    }

    parserConsume(p, TOKEN_RPAREN, "Expected ')' after arguments.");

    AstNode* node = (AstNode*)calloc(1, sizeof(AstNode));
    if (!node) {
        free(args);
        return NULL;
    }
    node->type = NODE_CALL;
    node->line = callee ? callee->line : p->previous.line;
    node->as.call.callee = callee;
    node->as.call.args = args;
    node->as.call.argCount = argCount;
    return node;
}

/* Parses call expressions by wrapping primaries with zero or more calls */
static AstNode* call(Parser* p) {
    AstNode* expr = primary(p);

    for (;;) {
        if (parserMatch(p, TOKEN_LPAREN)) {
            expr = finishCall(p, expr);
        } else {
            break;
        }
    }

    return expr;
}

/* Parses unary prefix operators or delegates to call expressions */
static AstNode* unary(Parser* p) {
    if (parserMatch(p, TOKEN_BANG)) {
        AstNode* node = (AstNode*)calloc(1, sizeof(AstNode));
        if (!node) return NULL;
        node->type = NODE_UNARY;
        node->line = p->previous.line;
        node->as.unary.op = UNOP_NOT;
        node->as.unary.operand = unary(p);
        return node;
    }

    if (parserMatch(p, TOKEN_MINUS)) {
        AstNode* node = (AstNode*)calloc(1, sizeof(AstNode));
        if (!node) return NULL;
        node->type = NODE_UNARY;
        node->line = p->previous.line;
        node->as.unary.op = UNOP_NEG;
        node->as.unary.operand = unary(p);
        return node;
    }

    return call(p);
}

/* Parses multiplicative * / % expressions */
static AstNode* factor(Parser* p) {
    AstNode* left = unary(p);

    while (parserMatch(p, TOKEN_STAR) || parserMatch(p, TOKEN_SLASH) || parserMatch(p, TOKEN_PERCENT)) {
        BinOp op = BINOP_MUL;
        if (p->previous.type == TOKEN_SLASH) op = BINOP_DIV;
        if (p->previous.type == TOKEN_PERCENT) op = BINOP_MOD;

        AstNode* node = (AstNode*)calloc(1, sizeof(AstNode));
        if (!node) return left;
        node->type = NODE_BINARY;
        node->line = p->previous.line;
        node->as.binary.op = op;
        node->as.binary.left = left;
        node->as.binary.right = unary(p);
        left = node;
    }

    return left;
}

/* Parses additive + - expressions */
static AstNode* term(Parser* p) {
    AstNode* left = factor(p);

    while (parserMatch(p, TOKEN_PLUS) || parserMatch(p, TOKEN_MINUS)) {
        BinOp op = p->previous.type == TOKEN_PLUS ? BINOP_ADD : BINOP_SUB;

        AstNode* node = (AstNode*)calloc(1, sizeof(AstNode));
        if (!node) return left;
        node->type = NODE_BINARY;
        node->line = p->previous.line;
        node->as.binary.op = op;
        node->as.binary.left = left;
        node->as.binary.right = factor(p);
        left = node;
    }

    return left;
}

/* Parses comparison < <= > >= expressions */
static AstNode* comparison(Parser* p) {
    AstNode* left = term(p);

    while (parserMatch(p, TOKEN_GREATER) || parserMatch(p, TOKEN_GREATER_EQUAL) ||
           parserMatch(p, TOKEN_LESS) || parserMatch(p, TOKEN_LESS_EQUAL)) {
        BinOp op = BINOP_GT;
        switch (p->previous.type) {
            case TOKEN_GREATER:      op = BINOP_GT; break;
            case TOKEN_GREATER_EQUAL: op = BINOP_GTE; break;
            case TOKEN_LESS:         op = BINOP_LT; break;
            case TOKEN_LESS_EQUAL:   op = BINOP_LTE; break;
            default: break;
        }

        AstNode* node = (AstNode*)calloc(1, sizeof(AstNode));
        if (!node) return left;
        node->type = NODE_BINARY;
        node->line = p->previous.line;
        node->as.binary.op = op;
        node->as.binary.left = left;
        node->as.binary.right = term(p);
        left = node;
    }

    return left;
}

/* Parses equality == != expressions */
static AstNode* equality(Parser* p) {
    AstNode* left = comparison(p);

    while (parserMatch(p, TOKEN_BANG_EQUAL) || parserMatch(p, TOKEN_EQUAL_EQUAL)) {
        BinOp op = p->previous.type == TOKEN_EQUAL_EQUAL ? BINOP_EQ : BINOP_NEQ;

        AstNode* node = (AstNode*)calloc(1, sizeof(AstNode));
        if (!node) return left;
        node->type = NODE_BINARY;
        node->line = p->previous.line;
        node->as.binary.op = op;
        node->as.binary.left = left;
        node->as.binary.right = comparison(p);
        left = node;
    }

    return left;
}

/* Parses logical && expressions */
static AstNode* logicAnd(Parser* p) {
    AstNode* left = equality(p);

    while (parserMatch(p, TOKEN_AND)) {
        AstNode* node = (AstNode*)calloc(1, sizeof(AstNode));
        if (!node) return left;
        node->type = NODE_BINARY;
        node->line = p->previous.line;
        node->as.binary.op = BINOP_AND;
        node->as.binary.left = left;
        node->as.binary.right = equality(p);
        left = node;
    }

    return left;
}

/* Parses logical || expressions */
static AstNode* logicOr(Parser* p) {
    AstNode* left = logicAnd(p);

    while (parserMatch(p, TOKEN_OR)) {
        AstNode* node = (AstNode*)calloc(1, sizeof(AstNode));
        if (!node) return left;
        node->type = NODE_BINARY;
        node->line = p->previous.line;
        node->as.binary.op = BINOP_OR;
        node->as.binary.left = left;
        node->as.binary.right = logicAnd(p);
        left = node;
    }

    return left;
}

/* Parses assignment expressions or delegates to logic_or */
static AstNode* assignment(Parser* p) {
    AstNode* expr = logicOr(p);

    if (parserMatch(p, TOKEN_EQUAL)) {
        if (expr && expr->type == NODE_IDENTIFIER) {
            AstNode* node = (AstNode*)calloc(1, sizeof(AstNode));
            if (!node) return expr;
            node->type = NODE_ASSIGNMENT;
            node->line = p->previous.line;
            node->as.assignment.name = expr->as.name;
            expr->as.name = NULL;
            freeAst(expr);
            node->as.assignment.value = assignment(p);
            return node;
        }
        parserErrorAt(p, p->previous, "Invalid assignment target.");
    }

    return expr;
}

/* Parses a top-level or nested expression */
static AstNode* expression(Parser* p) {
    return assignment(p);
}

/* Parses a braced block of statements */
static AstNode* block(Parser* p) {
    AstNode* node = newBlockNode(p->previous.line);
    if (!node) return NULL;

    while (!parserCheck(p, TOKEN_RBRACE) && !parserCheck(p, TOKEN_EOF)) {
        AstNode* stmt = declaration(p);
        if (stmt) blockAppend(node, stmt);
        if (p->hadError) parserSynchronize(p);
    }

    parserConsume(p, TOKEN_RBRACE, "Expected '}' after block.");
    return node;
}

/* Parses an if statement with an optional else branch */
static AstNode* ifStatement(Parser* p) {
    parserConsume(p, TOKEN_LPAREN, "Expected '(' after 'if'.");
    AstNode* condition = expression(p);
    parserConsume(p, TOKEN_RPAREN, "Expected ')' after if condition.");

    AstNode* thenBranch = statement(p);
    AstNode* elseBranch = NULL;

    if (parserMatch(p, TOKEN_ELSE)) {
        elseBranch = statement(p);
    }

    AstNode* node = (AstNode*)calloc(1, sizeof(AstNode));
    if (!node) return NULL;
    node->type = NODE_IF;
    node->line = p->previous.line;
    node->as.ifStmt.condition = condition;
    node->as.ifStmt.thenBranch = thenBranch;
    node->as.ifStmt.elseBranch = elseBranch;
    return node;
}

/* Parses a while loop statement */
static AstNode* whileStatement(Parser* p) {
    parserConsume(p, TOKEN_LPAREN, "Expected '(' after 'while'.");
    AstNode* condition = expression(p);
    parserConsume(p, TOKEN_RPAREN, "Expected ')' after while condition.");

    AstNode* body = statement(p);

    AstNode* node = (AstNode*)calloc(1, sizeof(AstNode));
    if (!node) return NULL;
    node->type = NODE_WHILE;
    node->line = p->previous.line;
    node->as.whileStmt.condition = condition;
    node->as.whileStmt.body = body;
    return node;
}

/* Parses a return statement with an optional value expression */
static AstNode* returnStatement(Parser* p) {
    AstNode* value = NULL;
    if (!parserCheck(p, TOKEN_SEMICOLON)) {
        value = expression(p);
    }
    parserConsume(p, TOKEN_SEMICOLON, "Expected ';' after return value.");

    AstNode* node = (AstNode*)calloc(1, sizeof(AstNode));
    if (!node) return NULL;
    node->type = NODE_RETURN;
    node->line = p->previous.line;
    node->as.returnStmt.value = value;
    return node;
}

/* Parses a print statement */
static AstNode* printStatement(Parser* p) {
    parserConsume(p, TOKEN_LPAREN, "Expected '(' after 'print'.");
    AstNode* value = expression(p);
    parserConsume(p, TOKEN_RPAREN, "Expected ')' after print value.");
    parserConsume(p, TOKEN_SEMICOLON, "Expected ';' after print statement.");

    AstNode* node = (AstNode*)calloc(1, sizeof(AstNode));
    if (!node) return NULL;
    node->type = NODE_PRINT;
    node->line = p->previous.line;
    node->as.printStmt.value = value;
    return node;
}

/* Parses a single statement of any supported kind */
static AstNode* statement(Parser* p) {
    if (parserMatch(p, TOKEN_IF)) return ifStatement(p);
    if (parserMatch(p, TOKEN_WHILE)) return whileStatement(p);
    if (parserMatch(p, TOKEN_RETURN)) return returnStatement(p);
    if (parserMatch(p, TOKEN_PRINT)) return printStatement(p);
    if (parserMatch(p, TOKEN_LBRACE)) return block(p);

    AstNode* expr = expression(p);
    parserConsume(p, TOKEN_SEMICOLON, "Expected ';' after expression.");
    return expr;
}

/* Parses a let variable declaration */
static AstNode* varDeclaration(Parser* p) {
    parserConsume(p, TOKEN_IDENTIFIER, "Expected variable name.");
    Token name = p->previous;

    parserConsume(p, TOKEN_EQUAL, "Expected '=' after variable name.");
    AstNode* initializer = expression(p);
    parserConsume(p, TOKEN_SEMICOLON, "Expected ';' after variable declaration.");

    AstNode* node = (AstNode*)calloc(1, sizeof(AstNode));
    if (!node) return NULL;
    node->type = NODE_VAR_DECL;
    node->line = name.line;
    node->as.varDecl.name = copyTokenText(name);
    node->as.varDecl.initializer = initializer;
    return node;
}

/* Parses a function declaration with parameters and body block */
static AstNode* funcDeclaration(Parser* p) {
    parserConsume(p, TOKEN_IDENTIFIER, "Expected function name.");
    Token name = p->previous;

    parserConsume(p, TOKEN_LPAREN, "Expected '(' after function name.");

    char** params = NULL;
    int paramCount = 0;
    int paramCapacity = 0;

    if (!parserCheck(p, TOKEN_RPAREN)) {
        do {
            parserConsume(p, TOKEN_IDENTIFIER, "Expected parameter name.");
            if (paramCount >= paramCapacity) {
                int newCap = paramCapacity < 4 ? 4 : paramCapacity * 2;
                char** newParams = (char**)realloc(params, (size_t)newCap * sizeof(char*));
                if (!newParams) {
                    for (int i = 0; i < paramCount; i++) free(params[i]);
                    free(params);
                    return NULL;
                }
                params = newParams;
                paramCapacity = newCap;
            }
            params[paramCount++] = copyTokenText(p->previous);
        } while (parserMatch(p, TOKEN_COMMA));
    }

    parserConsume(p, TOKEN_RPAREN, "Expected ')' after parameters.");
    parserConsume(p, TOKEN_LBRACE, "Expected '{' before function body.");
    AstNode* body = block(p);

    AstNode* node = (AstNode*)calloc(1, sizeof(AstNode));
    if (!node) return NULL;
    node->type = NODE_FUNCTION_DECL;
    node->line = name.line;
    node->as.funcDecl.name = copyTokenText(name);
    node->as.funcDecl.params = params;
    node->as.funcDecl.paramCount = paramCount;
    node->as.funcDecl.body = body;
    return node;
}

/* Parses a top-level declaration or statement */
static AstNode* declaration(Parser* p) {
    if (parserMatch(p, TOKEN_LET)) return varDeclaration(p);
    if (parserMatch(p, TOKEN_FUNCTION)) return funcDeclaration(p);
    return statement(p);
}

/* Parses a full program into a block of top-level declarations */
static AstNode* parseProgram(Parser* p) {
    AstNode* program = newBlockNode(1);
    if (!program) return NULL;

    while (!parserCheck(p, TOKEN_EOF)) {
        AstNode* decl = declaration(p);
        if (decl) blockAppend(program, decl);
        if (p->hadError) parserSynchronize(p);
    }

    return program;
}

/* Parses source text and returns the program AST root node */
AstNode* parse(const char* source) {
    Parser parser;
    parser.hadError = false;
    initLexer(&parser.lexer, source);
    parserAdvance(&parser);

    AstNode* program = parseProgram(&parser);
    if (parser.hadError) {
        freeAst(program);
        return NULL;
    }
    return program;
}

/* Returns a human-readable name for a binary operator */
static const char* binOpName(BinOp op) {
    switch (op) {
        case BINOP_ADD: return "+";
        case BINOP_SUB: return "-";
        case BINOP_MUL: return "*";
        case BINOP_DIV: return "/";
        case BINOP_MOD: return "%";
        case BINOP_EQ:  return "==";
        case BINOP_NEQ: return "!=";
        case BINOP_LT:  return "<";
        case BINOP_LTE: return "<=";
        case BINOP_GT:  return ">";
        case BINOP_GTE: return ">=";
        case BINOP_AND: return "&&";
        case BINOP_OR:  return "||";
    }
    return "?";
}

/* Returns a human-readable name for a unary operator */
static const char* unOpName(UnOp op) {
    return op == UNOP_NEG ? "-" : "!";
}

/* Returns a human-readable name for an AST node type */
static const char* nodeTypeName(NodeType type) {
    switch (type) {
        case NODE_NUMBER:         return "Number";
        case NODE_STRING:         return "String";
        case NODE_BOOL:           return "Bool";
        case NODE_NULL:           return "Null";
        case NODE_IDENTIFIER:     return "Identifier";
        case NODE_BINARY:         return "Binary";
        case NODE_UNARY:          return "Unary";
        case NODE_ASSIGNMENT:     return "Assignment";
        case NODE_VAR_DECL:       return "VarDecl";
        case NODE_BLOCK:          return "Block";
        case NODE_IF:             return "If";
        case NODE_WHILE:          return "While";
        case NODE_FUNCTION_DECL:  return "FunctionDecl";
        case NODE_CALL:           return "Call";
        case NODE_RETURN:         return "Return";
        case NODE_PRINT:          return "Print";
    }
    return "Unknown";
}

/* Pretty-prints an AST node with indentation for debugging */
void printAst(AstNode* node, int indent) {
    if (!node) {
        for (int i = 0; i < indent; i++) printf("  ");
        printf("(null)\n");
        return;
    }

    for (int i = 0; i < indent; i++) printf("  ");

    switch (node->type) {
        case NODE_NUMBER:
            printf("%s %g\n", nodeTypeName(node->type), node->as.number);
            break;
        case NODE_STRING:
            printf("%s \"%s\"\n", nodeTypeName(node->type), node->as.string ? node->as.string : "");
            break;
        case NODE_BOOL:
            printf("%s %s\n", nodeTypeName(node->type), node->as.boolean ? "true" : "false");
            break;
        case NODE_NULL:
            printf("%s\n", nodeTypeName(node->type));
            break;
        case NODE_IDENTIFIER:
            printf("%s %s\n", nodeTypeName(node->type), node->as.name ? node->as.name : "");
            break;
        case NODE_BINARY:
            printf("%s %s\n", nodeTypeName(node->type), binOpName(node->as.binary.op));
            printAst(node->as.binary.left, indent + 1);
            printAst(node->as.binary.right, indent + 1);
            break;
        case NODE_UNARY:
            printf("%s %s\n", nodeTypeName(node->type), unOpName(node->as.unary.op));
            printAst(node->as.unary.operand, indent + 1);
            break;
        case NODE_ASSIGNMENT:
            printf("%s %s\n", nodeTypeName(node->type), node->as.assignment.name ? node->as.assignment.name : "");
            printAst(node->as.assignment.value, indent + 1);
            break;
        case NODE_VAR_DECL:
            printf("%s %s\n", nodeTypeName(node->type), node->as.varDecl.name ? node->as.varDecl.name : "");
            printAst(node->as.varDecl.initializer, indent + 1);
            break;
        case NODE_BLOCK:
            printf("%s\n", nodeTypeName(node->type));
            for (int i = 0; i < node->as.block.count; i++) {
                printAst(node->as.block.statements[i], indent + 1);
            }
            break;
        case NODE_IF:
            printf("%s\n", nodeTypeName(node->type));
            printAst(node->as.ifStmt.condition, indent + 1);
            printAst(node->as.ifStmt.thenBranch, indent + 1);
            if (node->as.ifStmt.elseBranch) {
                for (int i = 0; i < indent + 1; i++) printf("  ");
                printf("Else\n");
                printAst(node->as.ifStmt.elseBranch, indent + 1);
            }
            break;
        case NODE_WHILE:
            printf("%s\n", nodeTypeName(node->type));
            printAst(node->as.whileStmt.condition, indent + 1);
            printAst(node->as.whileStmt.body, indent + 1);
            break;
        case NODE_FUNCTION_DECL:
            printf("%s %s(", nodeTypeName(node->type), node->as.funcDecl.name ? node->as.funcDecl.name : "");
            for (int i = 0; i < node->as.funcDecl.paramCount; i++) {
                if (i > 0) printf(", ");
                printf("%s", node->as.funcDecl.params[i]);
            }
            printf(")\n");
            printAst(node->as.funcDecl.body, indent + 1);
            break;
        case NODE_CALL:
            printf("%s\n", nodeTypeName(node->type));
            printAst(node->as.call.callee, indent + 1);
            for (int i = 0; i < node->as.call.argCount; i++) {
                printAst(node->as.call.args[i], indent + 1);
            }
            break;
        case NODE_RETURN:
            printf("%s\n", nodeTypeName(node->type));
            printAst(node->as.returnStmt.value, indent + 1);
            break;
        case NODE_PRINT:
            printf("%s\n", nodeTypeName(node->type));
            printAst(node->as.printStmt.value, indent + 1);
            break;
    }
}

/* Recursively frees an AST node and all owned heap memory */
void freeAst(AstNode* node) {
    if (!node) return;

    switch (node->type) {
        case NODE_STRING:
            free(node->as.string);
            break;
        case NODE_IDENTIFIER:
            free(node->as.name);
            break;
        case NODE_BINARY:
            freeAst(node->as.binary.left);
            freeAst(node->as.binary.right);
            break;
        case NODE_UNARY:
            freeAst(node->as.unary.operand);
            break;
        case NODE_ASSIGNMENT:
            free(node->as.assignment.name);
            freeAst(node->as.assignment.value);
            break;
        case NODE_VAR_DECL:
            free(node->as.varDecl.name);
            freeAst(node->as.varDecl.initializer);
            break;
        case NODE_BLOCK:
            for (int i = 0; i < node->as.block.count; i++) {
                freeAst(node->as.block.statements[i]);
            }
            free(node->as.block.statements);
            break;
        case NODE_IF:
            freeAst(node->as.ifStmt.condition);
            freeAst(node->as.ifStmt.thenBranch);
            freeAst(node->as.ifStmt.elseBranch);
            break;
        case NODE_WHILE:
            freeAst(node->as.whileStmt.condition);
            freeAst(node->as.whileStmt.body);
            break;
        case NODE_FUNCTION_DECL:
            free(node->as.funcDecl.name);
            for (int i = 0; i < node->as.funcDecl.paramCount; i++) {
                free(node->as.funcDecl.params[i]);
            }
            free(node->as.funcDecl.params);
            freeAst(node->as.funcDecl.body);
            break;
        case NODE_CALL:
            freeAst(node->as.call.callee);
            for (int i = 0; i < node->as.call.argCount; i++) {
                freeAst(node->as.call.args[i]);
            }
            free(node->as.call.args);
            break;
        case NODE_RETURN:
            freeAst(node->as.returnStmt.value);
            break;
        case NODE_PRINT:
            freeAst(node->as.printStmt.value);
            break;
        default:
            break;
    }

    free(node);
}

int main(void) {
    const char* source =
        "let result = 5 + 10 * 2;\n"
        "\n"
        "function add(a, b) { return a + b; }\n"
        "\n"
        "print(add(3, 4));\n";

    printf("=== Phase 2 Parser Test ===\n\n");

    AstNode* program = parse(source);
    if (!program) {
        fprintf(stderr, "Parse failed.\n");
        return 1;
    }

    printAst(program, 0);
    freeAst(program);

    printf("\n=== Phase 2 complete. Run tests before continuing. ===\n");
    return 0;
}
