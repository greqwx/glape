// (c) Glape Contributors

#ifndef GLAPE_AST_H
#define GLAPE_AST_H

#include <stddef.h>
#include <stdio.h>

#include "parse/lexer.h"

typedef enum {
    AST_PROGRAM,
    AST_LOAD,
    AST_DEFINE,
    AST_IF,
    AST_LOOP,
    AST_ASSIGN,
    AST_EXPR,
    AST_CATCH,
    AST_RETURN,
    AST_SKIP,
    AST_BREAK,
    AST_CONTINUE,
    AST_NUMBER,
    AST_STRING,
    AST_BOOL,
    AST_NIL,
    AST_IDENT,
    AST_LIST,
    AST_MAP,
    AST_UNARY,
    AST_BINARY,
    AST_CALL,
    AST_FIELD,
    AST_INDEX
} AstKind;

typedef struct AstNode AstNode;

typedef struct AstName {
    const char *name;
    struct AstName *next;
} AstName;

typedef struct AstNodeList {
    AstNode *node;
    struct AstNodeList *next;
} AstNodeList;

typedef struct AstMapEntry {
    AstNode *key;
    AstNode *value;
    struct AstMapEntry *next;
} AstMapEntry;

typedef struct {
    AstNode *head;
} AstBlock;

struct AstNode {
    AstKind kind;
    int line;
    int column;
    AstNode *next;
    union {
        struct {
            AstBlock body;
        } program;
        struct {
            AstName *names;
        } load;
        struct {
            const char *name;
            AstName *params;
            int may_fail;
            AstBlock body;
        } define;
        struct {
            AstNode *condition;
            AstBlock then_block;
            AstBlock else_block;
            int has_else;
        } if_stmt;
        struct {
            const char *var;
            AstNode *iterable;
            AstBlock body;
        } loop;
        struct {
            AstNode *target;
            AstNode *value;
        } assign;
        struct {
            AstNode *expr;
        } expr_stmt;
        struct {
            const char *name;
            AstNode *expr;
            AstBlock body;
        } catch_stmt;
        struct {
            int has_value;
            AstNode *value;
        } ret;
        struct {
            int is_float;
            long long integer;
            double floating;
        } number;
        struct {
            const char *value;
        } string;
        struct {
            int value;
        } boolean;
        struct {
            const char *name;
        } ident;
        struct {
            AstNodeList *items;
        } list;
        struct {
            AstMapEntry *entries;
        } map;
        struct {
            TokenType op;
            AstNode *operand;
        } unary;
        struct {
            TokenType op;
            AstNode *left;
            AstNode *right;
        } binary;
        struct {
            AstNode *callee;
            AstNodeList *args;
        } call;
        struct {
            AstNode *object;
            const char *name;
        } field;
        struct {
            AstNode *object;
            AstNode *index;
        } index;
    } as;
};

void ast_dump(const AstNode *program, FILE *out);

#endif
