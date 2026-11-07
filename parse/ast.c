// (c) Glape Contributors

#include "ast.h"

#include <string.h>

static void print_indent(FILE *out, int depth)
{
    int i;

    for (i = 0; i < depth; i++)
        fputs("  ", out);
}

static const char *operator_name(TokenType type)
{
    switch (type) {
    case TOK_PLUS:
        return "+";
    case TOK_MINUS:
        return "-";
    case TOK_STAR:
        return "*";
    case TOK_SLASH:
        return "/";
    case TOK_PERCENT:
        return "%";
    case TOK_EQ:
        return "==";
    case TOK_NE:
        return "!=";
    case TOK_LT:
        return "<";
    case TOK_LE:
        return "<=";
    case TOK_GT:
        return ">";
    case TOK_GE:
        return ">=";
    case TOK_AND:
        return "and";
    case TOK_OR:
        return "or";
    case TOK_NOT:
        return "not";
    default:
        return "?";
    }
}

static void print_string(const char *text, FILE *out)
{
    const unsigned char *p = (const unsigned char *)text;

    fputc('"', out);

    while (*p != '\0') {
        unsigned char c = *p++;

        if (c == '\\') {
            fputs("\\\\", out);
        } else if (c == '"') {
            fputs("\\\"", out);
        } else if (c == '\n') {
            fputs("\\n", out);
        } else if (c == '\r') {
            fputs("\\r", out);
        } else if (c == '\t') {
            fputs("\\t", out);
        } else if (c < 32 || c >= 127) {
            fprintf(out, "\\x%02x", (unsigned int)c);
        } else {
            fputc((int)c, out);
        }
    }

    fputc('"', out);
}

static void dump_block(const AstBlock *block, FILE *out, int depth);

static void dump_names(const AstName *names, FILE *out)
{
    int first = 1;

    while (names != NULL) {
        if (!first)
            fputs(", ", out);
        fputs(names->name, out);
        first = 0;
        names = names->next;
    }
}

static void dump_node(const AstNode *node, FILE *out, int depth)
{
    if (node == NULL)
        return;

    print_indent(out, depth);

    switch (node->kind) {
    case AST_PROGRAM:
        fputs("program\n", out);
        dump_block(&node->as.program.body, out, depth + 1);
        break;

    case AST_LOAD:
        fputs("load ", out);
        dump_names(node->as.load.names, out);
        fputc('\n', out);
        break;

    case AST_DEFINE:
        fprintf(out, "define %s(", node->as.define.name);
        dump_names(node->as.define.params, out);
        fprintf(out, ")%s\n", node->as.define.may_fail ? "!" : "");
        dump_block(&node->as.define.body, out, depth + 1);
        break;

    case AST_IF:
        fputs("if\n", out);
        print_indent(out, depth + 1);
        fputs("cond\n", out);
        dump_node(node->as.if_stmt.condition, out, depth + 2);
        print_indent(out, depth + 1);
        fputs("then\n", out);
        dump_block(&node->as.if_stmt.then_block, out, depth + 2);
        if (node->as.if_stmt.has_else) {
            print_indent(out, depth + 1);
            fputs("else\n", out);
            dump_block(&node->as.if_stmt.else_block, out, depth + 2);
        }
        break;

    case AST_LOOP:
        fprintf(out, "loop %s\n", node->as.loop.var);
        print_indent(out, depth + 1);
        fputs("in\n", out);
        dump_node(node->as.loop.iterable, out, depth + 2);
        print_indent(out, depth + 1);
        fputs("body\n", out);
        dump_block(&node->as.loop.body, out, depth + 2);
        break;

    case AST_ASSIGN:
        fputs("assign\n", out);
        print_indent(out, depth + 1);
        fputs("target\n", out);
        dump_node(node->as.assign.target, out, depth + 2);
        print_indent(out, depth + 1);
        fputs("value\n", out);
        dump_node(node->as.assign.value, out, depth + 2);
        break;

    case AST_EXPR:
        fputs("expr\n", out);
        dump_node(node->as.expr_stmt.expr, out, depth + 1);
        break;

    case AST_CATCH:
        fprintf(out, "catch %s\n", node->as.catch_stmt.name);
        print_indent(out, depth + 1);
        fputs("expr\n", out);
        dump_node(node->as.catch_stmt.expr, out, depth + 2);
        print_indent(out, depth + 1);
        fputs("body\n", out);
        dump_block(&node->as.catch_stmt.body, out, depth + 2);
        break;

    case AST_RETURN:
        if (node->as.ret.has_value) {
            fputs("return\n", out);
            print_indent(out, depth + 1);
            fputs("value\n", out);
            dump_node(node->as.ret.value, out, depth + 2);
        } else {
            fputs("return\n", out);
        }
        break;

    case AST_SKIP:
        fputs("skip\n", out);
        break;

    case AST_BREAK:
        fputs("break\n", out);
        break;

    case AST_CONTINUE:
        fputs("continue\n", out);
        break;

    case AST_NUMBER:
        if (node->as.number.is_float)
            fprintf(out, "float %g\n", node->as.number.floating);
        else
            fprintf(out, "int %lld\n", node->as.number.integer);
        break;

    case AST_STRING:
        fputs("string ", out);
        print_string(node->as.string.value, out);
        fputc('\n', out);
        break;

    case AST_BOOL:
        fprintf(out, "bool %s\n", node->as.boolean.value ? "true" : "false");
        break;

    case AST_NIL:
        fputs("nil\n", out);
        break;

    case AST_IDENT:
        fprintf(out, "ident %s\n", node->as.ident.name);
        break;

    case AST_LIST: {
        const AstNodeList *item = node->as.list.items;

        fputs("list\n", out);

        while (item != NULL) {
            print_indent(out, depth + 1);
            fputs("item\n", out);
            dump_node(item->node, out, depth + 2);
            item = item->next;
        }
        break;
    }

    case AST_MAP: {
        const AstMapEntry *entry = node->as.map.entries;

        fputs("map\n", out);

        while (entry != NULL) {
            print_indent(out, depth + 1);
            fputs("entry\n", out);
            print_indent(out, depth + 2);
            fputs("key\n", out);
            dump_node(entry->key, out, depth + 3);
            print_indent(out, depth + 2);
            fputs("value\n", out);
            dump_node(entry->value, out, depth + 3);
            entry = entry->next;
        }
        break;
    }

    case AST_UNARY:
        fprintf(out, "unary %s\n", operator_name(node->as.unary.op));
        print_indent(out, depth + 1);
        fputs("operand\n", out);
        dump_node(node->as.unary.operand, out, depth + 2);
        break;

    case AST_BINARY:
        fprintf(out, "binary %s\n", operator_name(node->as.binary.op));
        print_indent(out, depth + 1);
        fputs("left\n", out);
        dump_node(node->as.binary.left, out, depth + 2);
        print_indent(out, depth + 1);
        fputs("right\n", out);
        dump_node(node->as.binary.right, out, depth + 2);
        break;

    case AST_CALL: {
        const AstNodeList *arg = node->as.call.args;

        fputs("call\n", out);
        print_indent(out, depth + 1);
        fputs("callee\n", out);
        dump_node(node->as.call.callee, out, depth + 2);

        while (arg != NULL) {
            print_indent(out, depth + 1);
            fputs("arg\n", out);
            dump_node(arg->node, out, depth + 2);
            arg = arg->next;
        }
        break;
    }

    case AST_FIELD:
        fputs("field\n", out);
        print_indent(out, depth + 1);
        fputs("object\n", out);
        dump_node(node->as.field.object, out, depth + 2);
        print_indent(out, depth + 1);
        fprintf(out, "name %s\n", node->as.field.name);
        break;

    case AST_INDEX:
        fputs("index\n", out);
        print_indent(out, depth + 1);
        fputs("object\n", out);
        dump_node(node->as.index.object, out, depth + 2);
        print_indent(out, depth + 1);
        fputs("index\n", out);
        dump_node(node->as.index.index, out, depth + 2);
        break;

    default:
        fputs("unknown\n", out);
        break;
    }
}

static void dump_block(const AstBlock *block, FILE *out, int depth)
{
    const AstNode *node = block->head;

    while (node != NULL) {
        dump_node(node, out, depth);
        node = node->next;
    }
}

void ast_dump(const AstNode *program, FILE *out)
{
    dump_node(program, out, 0);
}
