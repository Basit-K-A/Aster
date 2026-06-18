#include "ast.h"

#include <stdio.h>
#include <stdlib.h>

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
