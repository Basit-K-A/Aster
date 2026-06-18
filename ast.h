#ifndef ASTER_AST_H
#define ASTER_AST_H

#include <stdbool.h>

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

/* Pretty-prints an AST node with indentation for debugging */
void printAst(AstNode* node, int indent);

/* Recursively frees an AST node and all owned heap memory */
void freeAst(AstNode* node);

#endif /* ASTER_AST_H */
