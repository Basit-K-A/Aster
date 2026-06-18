#include "parser.h"

#include "lexer.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Parser state holding lexer position and error flag */
typedef struct {
    Lexer lexer;
    Token current;
    Token previous;
    bool hadError;
} Parser;

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
            case TOKEN_GREATER:       op = BINOP_GT; break;
            case TOKEN_GREATER_EQUAL: op = BINOP_GTE; break;
            case TOKEN_LESS:          op = BINOP_LT; break;
            case TOKEN_LESS_EQUAL:    op = BINOP_LTE; break;
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
