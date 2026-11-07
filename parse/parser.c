// (c) Glape Contributors

#include "parser.h"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static AstNode *parse_expression(Parser *parser);
static AstNode *parse_statement(Parser *parser);

static void set_error(Parser *parser, int line, int column, const char *message)
{
    if (parser->failed)
        return;

    parser->failed = 1;
    parser->error_line = line;
    parser->error_column = column;
    snprintf(parser->error, sizeof parser->error, "%s", message);
}

static void set_errorf(Parser *parser, int line, int column, const char *format, ...)
{
    va_list args;

    if (parser->failed)
        return;

    parser->failed = 1;
    parser->error_line = line;
    parser->error_column = column;

    va_start(args, format);
    vsnprintf(parser->error, sizeof parser->error, format, args);
    va_end(args);
}

static void advance(Parser *parser)
{
    if (parser->failed)
        return;

    parser->current = lexer_next(&parser->lexer);

    if (parser->current.type == TOK_ERROR)
        set_error(parser, parser->current.line, parser->current.column, lexer_error(&parser->lexer));
}

static int check(const Parser *parser, TokenType type)
{
    return parser->current.type == type;
}

static int match(Parser *parser, TokenType type)
{
    if (!check(parser, type))
        return 0;

    advance(parser);
    return 1;
}

static int expect(Parser *parser, TokenType type, const char *what)
{
    if (parser->failed)
        return 0;

    if (!check(parser, type)) {
        set_errorf(parser, parser->current.line, parser->current.column, "expected %s, got %s", what,
                   token_type_name(parser->current.type));
        return 0;
    }

    advance(parser);
    return 1;
}

static AstNode *new_node_at(Parser *parser, AstKind kind, int line, int column)
{
    AstNode *node;

    if (parser->failed)
        return NULL;

    node = (AstNode *)arena_alloc(parser->arena, sizeof *node);
    if (node == NULL) {
        set_error(parser, line, column, "out of memory");
        return NULL;
    }

    memset(node, 0, sizeof *node);
    node->kind = kind;
    node->line = line;
    node->column = column;
    return node;
}

static AstNode *new_node(Parser *parser, AstKind kind, Token token)
{
    return new_node_at(parser, kind, token.line, token.column);
}

static const char *copy_text(Parser *parser, const char *text, int length, int line, int column)
{
    char *copy;

    if (parser->failed)
        return NULL;

    copy = (char *)arena_alloc(parser->arena, (size_t)length + 1);
    if (copy == NULL) {
        set_error(parser, line, column, "out of memory");
        return NULL;
    }

    memcpy(copy, text, (size_t)length);
    copy[length] = '\0';
    return copy;
}

static const char *copy_token(Parser *parser, Token token)
{
    return copy_text(parser, token.start, token.length, token.line, token.column);
}

static void append_node(AstNode **head, AstNode **tail, AstNode *node)
{
    if (*tail != NULL)
        (*tail)->next = node;
    else
        *head = node;

    *tail = node;
}

static void append_name(AstName **head, AstName **tail, AstName *name)
{
    if (*tail != NULL)
        (*tail)->next = name;
    else
        *head = name;

    *tail = name;
}

static void append_arg(AstNodeList **head, AstNodeList **tail, AstNodeList *arg)
{
    if (*tail != NULL)
        (*tail)->next = arg;
    else
        *head = arg;

    *tail = arg;
}

static void append_entry(AstMapEntry **head, AstMapEntry **tail, AstMapEntry *entry)
{
    if (*tail != NULL)
        (*tail)->next = entry;
    else
        *head = entry;

    *tail = entry;
}

static AstName *new_name_item(Parser *parser, Token token)
{
    AstName *item = (AstName *)arena_alloc(parser->arena, sizeof *item);

    if (item == NULL) {
        set_error(parser, token.line, token.column, "out of memory");
        return NULL;
    }

    item->name = copy_token(parser, token);
    item->next = NULL;

    if (item->name == NULL)
        return NULL;

    return item;
}

static AstNodeList *new_arg_item(Parser *parser, AstNode *node)
{
    AstNodeList *item = (AstNodeList *)arena_alloc(parser->arena, sizeof *item);

    if (item == NULL) {
        set_error(parser, node->line, node->column, "out of memory");
        return NULL;
    }

    item->node = node;
    item->next = NULL;
    return item;
}

static AstMapEntry *new_map_entry(Parser *parser, AstNode *key, AstNode *value)
{
    AstMapEntry *entry = (AstMapEntry *)arena_alloc(parser->arena, sizeof *entry);

    if (entry == NULL) {
        set_error(parser, key->line, key->column, "out of memory");
        return NULL;
    }

    entry->key = key;
    entry->value = value;
    entry->next = NULL;
    return entry;
}

static AstNode *parse_number(Parser *parser, Token token)
{
    char *text;
    char *end;
    size_t i;
    size_t length = 0;
    int is_float = 0;
    int is_hex = 0;
    AstNode *node;

    text = (char *)arena_alloc(parser->arena, (size_t)token.length + 1);
    if (text == NULL) {
        set_error(parser, token.line, token.column, "out of memory");
        return NULL;
    }

    for (i = 0; i < (size_t)token.length; i++) {
        if (token.start[i] != '_')
            text[length++] = token.start[i];
    }
    text[length] = '\0';

    if (length >= 2 && text[0] == '0' && (text[1] == 'x' || text[1] == 'X'))
        is_hex = 1;

    if (!is_hex) {
        for (i = 0; i < length; i++) {
            if (text[i] == '.' || text[i] == 'e' || text[i] == 'E') {
                is_float = 1;
                break;
            }
        }
    }

    node = new_node(parser, AST_NUMBER, token);
    if (node == NULL)
        return NULL;

    errno = 0;

    if (is_float) {
        double value = strtod(text, &end);

        if (end == text || *end != '\0' || errno == ERANGE) {
            set_errorf(parser, token.line, token.column, "invalid number: %s", text);
            return NULL;
        }

        node->as.number.is_float = 1;
        node->as.number.floating = value;
    } else {
        long long value = strtoll(text, &end, is_hex ? 16 : 10);

        if (end == text || *end != '\0' || errno == ERANGE) {
            set_errorf(parser, token.line, token.column, "invalid number: %s", text);
            return NULL;
        }

        node->as.number.integer = value;
    }

    return node;
}

static AstNode *parse_primary(Parser *parser)
{
    Token token = parser->current;
    AstNode *node;

    switch (token.type) {
    case TOK_NUMBER:
        advance(parser);
        return parse_number(parser, token);

    case TOK_STRING:
        advance(parser);
        node = new_node(parser, AST_STRING, token);
        if (node != NULL)
            node->as.string.value = copy_token(parser, token);
        return node;

    case TOK_TRUE:
    case TOK_FALSE:
        advance(parser);
        node = new_node(parser, AST_BOOL, token);
        if (node != NULL)
            node->as.boolean.value = token.type == TOK_TRUE;
        return node;

    case TOK_NIL:
        advance(parser);
        return new_node(parser, AST_NIL, token);

    case TOK_IDENT:
        advance(parser);
        node = new_node(parser, AST_IDENT, token);
        if (node != NULL)
            node->as.ident.name = copy_token(parser, token);
        return node;

    case TOK_LPAREN:
        advance(parser);
        node = parse_expression(parser);
        if (node == NULL)
            return NULL;
        if (!expect(parser, TOK_RPAREN, "')'"))
            return NULL;
        return node;

    case TOK_LBRACKET:
        advance(parser);
        node = new_node(parser, AST_LIST, token);
        if (node == NULL)
            return NULL;

        if (!check(parser, TOK_RBRACKET)) {
            AstNodeList *head = NULL;
            AstNodeList *tail = NULL;

            for (;;) {
                AstNode *item = parse_expression(parser);
                AstNodeList *arg;

                if (item == NULL)
                    return NULL;

                arg = new_arg_item(parser, item);
                if (arg == NULL)
                    return NULL;

                append_arg(&head, &tail, arg);

                if (!match(parser, TOK_COMMA))
                    break;

                if (check(parser, TOK_RBRACKET))
                    break;
            }

            node->as.list.items = head;
        }

        if (!expect(parser, TOK_RBRACKET, "']'"))
            return NULL;

        return node;

    case TOK_LBRACE:
        advance(parser);
        node = new_node(parser, AST_MAP, token);
        if (node == NULL)
            return NULL;

        if (!check(parser, TOK_RBRACE)) {
            AstMapEntry *head = NULL;
            AstMapEntry *tail = NULL;

            for (;;) {
                AstNode *key = parse_expression(parser);
                AstNode *value;
                AstMapEntry *entry;

                if (key == NULL)
                    return NULL;

                if (!expect(parser, TOK_COLON, "':'"))
                    return NULL;

                value = parse_expression(parser);
                if (value == NULL)
                    return NULL;

                entry = new_map_entry(parser, key, value);
                if (entry == NULL)
                    return NULL;

                append_entry(&head, &tail, entry);

                if (!match(parser, TOK_COMMA))
                    break;

                if (check(parser, TOK_RBRACE))
                    break;
            }

            node->as.map.entries = head;
        }

        if (!expect(parser, TOK_RBRACE, "'}'"))
            return NULL;

        return node;

    default:
        set_errorf(parser, token.line, token.column, "expected expression, got %s",
                   token_type_name(token.type));
        return NULL;
    }
}

static AstNode *parse_postfix(Parser *parser)
{
    AstNode *expr = parse_primary(parser);

    while (!parser->failed) {
        if (match(parser, TOK_LPAREN)) {
            AstNode *call = new_node_at(parser, AST_CALL, expr->line, expr->column);

            if (call == NULL)
                return NULL;

            call->as.call.callee = expr;

            if (!check(parser, TOK_RPAREN)) {
                AstNodeList *head = NULL;
                AstNodeList *tail = NULL;

                for (;;) {
                    AstNode *arg_value = parse_expression(parser);
                    AstNodeList *arg;

                    if (arg_value == NULL)
                        return NULL;

                    arg = new_arg_item(parser, arg_value);
                    if (arg == NULL)
                        return NULL;

                    append_arg(&head, &tail, arg);

                    if (!match(parser, TOK_COMMA))
                        break;

                    if (check(parser, TOK_RPAREN))
                        break;
                }

                call->as.call.args = head;
            }

            if (!expect(parser, TOK_RPAREN, "')'"))
                return NULL;

            expr = call;
            continue;
        }

        if (match(parser, TOK_DOT)) {
            Token name_token = parser->current;
            AstNode *field;

            if (!expect(parser, TOK_IDENT, "field name"))
                return NULL;

            field = new_node_at(parser, AST_FIELD, name_token.line, name_token.column);
            if (field == NULL)
                return NULL;

            field->as.field.object = expr;
            field->as.field.name = copy_token(parser, name_token);
            expr = field;
            continue;
        }

        if (check(parser, TOK_LBRACKET)) {
            Token bracket = parser->current;
            AstNode *index;

            advance(parser);
            index = new_node_at(parser, AST_INDEX, bracket.line, bracket.column);

            if (index == NULL)
                return NULL;

            index->as.index.object = expr;
            index->as.index.index = parse_expression(parser);

            if (index->as.index.index == NULL)
                return NULL;

            if (!expect(parser, TOK_RBRACKET, "']'"))
                return NULL;

            expr = index;
            continue;
        }

        break;
    }

    return expr;
}

static AstNode *parse_unary(Parser *parser)
{
    if (check(parser, TOK_MINUS) || check(parser, TOK_PLUS) || check(parser, TOK_NOT)) {
        Token op = parser->current;
        AstNode *operand;
        AstNode *node;

        advance(parser);
        operand = parse_unary(parser);
        if (operand == NULL)
            return NULL;

        node = new_node(parser, AST_UNARY, op);
        if (node == NULL)
            return NULL;

        node->as.unary.op = op.type;
        node->as.unary.operand = operand;
        return node;
    }

    return parse_postfix(parser);
}

static AstNode *make_binary(Parser *parser, AstNode *left, Token op, AstNode *right)
{
    AstNode *node = new_node(parser, AST_BINARY, op);

    if (node == NULL)
        return NULL;

    node->as.binary.op = op.type;
    node->as.binary.left = left;
    node->as.binary.right = right;
    return node;
}

static AstNode *parse_factor(Parser *parser)
{
    AstNode *left = parse_unary(parser);

    while (!parser->failed &&
           (check(parser, TOK_STAR) || check(parser, TOK_SLASH) || check(parser, TOK_PERCENT))) {
        Token op = parser->current;
        AstNode *right;

        advance(parser);
        right = parse_unary(parser);
        if (right == NULL)
            return NULL;

        left = make_binary(parser, left, op, right);
        if (left == NULL)
            return NULL;
    }

    return left;
}

static AstNode *parse_term(Parser *parser)
{
    AstNode *left = parse_factor(parser);

    while (!parser->failed && (check(parser, TOK_PLUS) || check(parser, TOK_MINUS))) {
        Token op = parser->current;
        AstNode *right;

        advance(parser);
        right = parse_factor(parser);
        if (right == NULL)
            return NULL;

        left = make_binary(parser, left, op, right);
        if (left == NULL)
            return NULL;
    }

    return left;
}

static AstNode *parse_comparison(Parser *parser)
{
    AstNode *left = parse_term(parser);

    while (!parser->failed &&
           (check(parser, TOK_LT) || check(parser, TOK_LE) || check(parser, TOK_GT) ||
            check(parser, TOK_GE))) {
        Token op = parser->current;
        AstNode *right;

        advance(parser);
        right = parse_term(parser);
        if (right == NULL)
            return NULL;

        left = make_binary(parser, left, op, right);
        if (left == NULL)
            return NULL;
    }

    return left;
}

static AstNode *parse_equality(Parser *parser)
{
    AstNode *left = parse_comparison(parser);

    while (!parser->failed && (check(parser, TOK_EQ) || check(parser, TOK_NE))) {
        Token op = parser->current;
        AstNode *right;

        advance(parser);
        right = parse_comparison(parser);
        if (right == NULL)
            return NULL;

        left = make_binary(parser, left, op, right);
        if (left == NULL)
            return NULL;
    }

    return left;
}

static AstNode *parse_and(Parser *parser)
{
    AstNode *left = parse_equality(parser);

    while (!parser->failed && check(parser, TOK_AND)) {
        Token op = parser->current;
        AstNode *right;

        advance(parser);
        right = parse_equality(parser);
        if (right == NULL)
            return NULL;

        left = make_binary(parser, left, op, right);
        if (left == NULL)
            return NULL;
    }

    return left;
}

static AstNode *parse_or(Parser *parser)
{
    AstNode *left = parse_and(parser);

    while (!parser->failed && check(parser, TOK_OR)) {
        Token op = parser->current;
        AstNode *right;

        advance(parser);
        right = parse_and(parser);
        if (right == NULL)
            return NULL;

        left = make_binary(parser, left, op, right);
        if (left == NULL)
            return NULL;
    }

    return left;
}

static AstNode *parse_expression(Parser *parser)
{
    return parse_or(parser);
}

static AstNode *parse_load(Parser *parser)
{
    Token start = parser->current;
    AstNode *node = new_node(parser, AST_LOAD, start);
    AstName *head = NULL;
    AstName *tail = NULL;

    if (node == NULL)
        return NULL;

    advance(parser);

    for (;;) {
        Token name_token = parser->current;
        AstName *name;

        if (!check(parser, TOK_IDENT)) {
            set_errorf(parser, parser->current.line, parser->current.column,
                       "expected module name, got %s", token_type_name(parser->current.type));
            return NULL;
        }

        name = new_name_item(parser, name_token);
        if (name == NULL)
            return NULL;

        append_name(&head, &tail, name);
        advance(parser);

        if (!match(parser, TOK_COMMA))
            break;
    }

    if (!expect(parser, TOK_NEWLINE, "end of line"))
        return NULL;

    node->as.load.names = head;
    return node;
}

static int parse_block(Parser *parser, AstBlock *block)
{
    AstNode *head = NULL;
    AstNode *tail = NULL;

    if (!expect(parser, TOK_NEWLINE, "end of line"))
        return 0;

    if (!expect(parser, TOK_INDENT, "indented block"))
        return 0;

    while (!parser->failed && !check(parser, TOK_DEDENT) && !check(parser, TOK_EOF)) {
        AstNode *statement = parse_statement(parser);

        if (statement == NULL)
            return 0;

        append_node(&head, &tail, statement);
    }

    if (parser->failed)
        return 0;

    if (!expect(parser, TOK_DEDENT, "end of block"))
        return 0;

    block->head = head;
    return 1;
}

static AstNode *parse_define(Parser *parser)
{
    Token start = parser->current;
    Token name_token;
    AstNode *node;
    AstName *params = NULL;
    AstName *param_tail = NULL;

    advance(parser);

    if (!check(parser, TOK_IDENT)) {
        set_errorf(parser, parser->current.line, parser->current.column,
                   "expected function name, got %s", token_type_name(parser->current.type));
        return NULL;
    }

    name_token = parser->current;
    advance(parser);

    if (!expect(parser, TOK_LPAREN, "'('"))
        return NULL;

    if (!check(parser, TOK_RPAREN)) {
        for (;;) {
            Token param_token = parser->current;
            AstName *param;

            if (!check(parser, TOK_IDENT)) {
                set_errorf(parser, parser->current.line, parser->current.column,
                           "expected parameter name, got %s",
                           token_type_name(parser->current.type));
                return NULL;
            }

            param = new_name_item(parser, param_token);
            if (param == NULL)
                return NULL;

            append_name(&params, &param_tail, param);
            advance(parser);

            if (!match(parser, TOK_COMMA))
                break;

            if (check(parser, TOK_RPAREN))
                break;
        }
    }

    if (!expect(parser, TOK_RPAREN, "')'"))
        return NULL;

    node = new_node(parser, AST_DEFINE, start);
    if (node == NULL)
        return NULL;

    node->as.define.name = copy_token(parser, name_token);
    if (node->as.define.name == NULL)
        return NULL;

    node->as.define.may_fail = match(parser, TOK_BANG);
    node->as.define.params = params;

    if (!expect(parser, TOK_COLON, "':'"))
        return NULL;

    if (!parse_block(parser, &node->as.define.body))
        return NULL;

    return node;
}

static AstNode *parse_if(Parser *parser)
{
    Token start = parser->current;
    AstNode *node;

    advance(parser);

    node = new_node(parser, AST_IF, start);
    if (node == NULL)
        return NULL;

    node->as.if_stmt.condition = parse_expression(parser);
    if (node->as.if_stmt.condition == NULL)
        return NULL;

    if (!expect(parser, TOK_COLON, "':'"))
        return NULL;

    if (!parse_block(parser, &node->as.if_stmt.then_block))
        return NULL;

    if (match(parser, TOK_ELSE)) {
        node->as.if_stmt.has_else = 1;

        if (!expect(parser, TOK_COLON, "':'"))
            return NULL;

        if (!parse_block(parser, &node->as.if_stmt.else_block))
            return NULL;
    }

    return node;
}

static AstNode *parse_loop(Parser *parser)
{
    Token start = parser->current;
    Token var_token;
    AstNode *node;

    advance(parser);

    if (!check(parser, TOK_IDENT)) {
        set_errorf(parser, parser->current.line, parser->current.column,
                   "expected loop variable, got %s", token_type_name(parser->current.type));
        return NULL;
    }

    var_token = parser->current;
    advance(parser);

    if (!expect(parser, TOK_IN, "'in'"))
        return NULL;

    node = new_node(parser, AST_LOOP, start);
    if (node == NULL)
        return NULL;

    node->as.loop.var = copy_token(parser, var_token);
    if (node->as.loop.var == NULL)
        return NULL;

    node->as.loop.iterable = parse_expression(parser);
    if (node->as.loop.iterable == NULL)
        return NULL;

    if (!expect(parser, TOK_COLON, "':'"))
        return NULL;

    if (!parse_block(parser, &node->as.loop.body))
        return NULL;

    return node;
}

static AstNode *parse_return(Parser *parser)
{
    Token start = parser->current;
    AstNode *node;

    advance(parser);

    node = new_node(parser, AST_RETURN, start);
    if (node == NULL)
        return NULL;

    if (!check(parser, TOK_NEWLINE)) {
        node->as.ret.has_value = 1;
        node->as.ret.value = parse_expression(parser);
        if (node->as.ret.value == NULL)
            return NULL;
    }

    if (!expect(parser, TOK_NEWLINE, "end of line"))
        return NULL;

    return node;
}

static int is_lvalue(const AstNode *node)
{
    return node->kind == AST_IDENT || node->kind == AST_FIELD || node->kind == AST_INDEX;
}

static AstNode *parse_statement(Parser *parser)
{
    Token start = parser->current;

    switch (start.type) {
    case TOK_LOAD:
        return parse_load(parser);

    case TOK_DEFINE:
        return parse_define(parser);

    case TOK_IF:
        return parse_if(parser);

    case TOK_LOOP:
        return parse_loop(parser);

    case TOK_RETURN:
        return parse_return(parser);

    case TOK_SKIP:
        advance(parser);
        if (!expect(parser, TOK_NEWLINE, "end of line"))
            return NULL;
        return new_node(parser, AST_SKIP, start);

    case TOK_BREAK:
        advance(parser);
        if (!expect(parser, TOK_NEWLINE, "end of line"))
            return NULL;
        return new_node(parser, AST_BREAK, start);

    case TOK_CONTINUE:
        advance(parser);
        if (!expect(parser, TOK_NEWLINE, "end of line"))
            return NULL;
        return new_node(parser, AST_CONTINUE, start);

    default: {
        AstNode *expr = parse_expression(parser);

        if (expr == NULL)
            return NULL;

        if (match(parser, TOK_ASSIGN)) {
            AstNode *node;

            if (!is_lvalue(expr)) {
                set_error(parser, expr->line, expr->column, "cannot assign to this expression");
                return NULL;
            }

            node = new_node(parser, AST_ASSIGN, start);
            if (node == NULL)
                return NULL;

            node->as.assign.target = expr;
            node->as.assign.value = parse_expression(parser);
            if (node->as.assign.value == NULL)
                return NULL;

            if (check(parser, TOK_CATCH)) {
                set_error(parser, parser->current.line, parser->current.column,
                          "catch can only follow a call");
                return NULL;
            }

            if (!expect(parser, TOK_NEWLINE, "end of line"))
                return NULL;

            return node;
        }

        if (check(parser, TOK_CATCH)) {
            Token catch_token = parser->current;
            Token name_token;
            AstNode *node;
            AstBlock body;

            if (expr->kind != AST_CALL) {
                set_error(parser, catch_token.line, catch_token.column,
                          "catch can only follow a call");
                return NULL;
            }

            advance(parser);

            if (!check(parser, TOK_IDENT)) {
                set_errorf(parser, parser->current.line, parser->current.column,
                           "expected error name, got %s", token_type_name(parser->current.type));
                return NULL;
            }

            name_token = parser->current;
            advance(parser);

            if (!expect(parser, TOK_COLON, "':'"))
                return NULL;

            if (!parse_block(parser, &body))
                return NULL;

            node = new_node(parser, AST_CATCH, catch_token);
            if (node == NULL)
                return NULL;

            node->as.catch_stmt.name = copy_token(parser, name_token);
            if (node->as.catch_stmt.name == NULL)
                return NULL;

            node->as.catch_stmt.expr = expr;
            node->as.catch_stmt.body = body;
            return node;
        }

        {
            AstNode *node;

            if (!expect(parser, TOK_NEWLINE, "end of line"))
                return NULL;

            node = new_node(parser, AST_EXPR, start);
            if (node == NULL)
                return NULL;

            node->as.expr_stmt.expr = expr;
            return node;
        }
    }
    }
}

void parser_init(Parser *parser, Arena *arena, const char *source, size_t length)
{
    memset(parser, 0, sizeof *parser);
    parser->arena = arena;
    lexer_init(&parser->lexer, source, length);
    parser->current = lexer_next(&parser->lexer);

    if (parser->current.type == TOK_ERROR)
        set_error(parser, parser->current.line, parser->current.column, lexer_error(&parser->lexer));
}

AstNode *parser_parse_program(Parser *parser)
{
    AstNode *program;
    AstNode *head = NULL;
    AstNode *tail = NULL;

    program = new_node(parser, AST_PROGRAM, parser->current);
    if (program == NULL)
        return NULL;

    while (!parser->failed && !check(parser, TOK_EOF)) {
        AstNode *statement = parse_statement(parser);

        if (statement == NULL)
            return NULL;

        append_node(&head, &tail, statement);
    }

    if (parser->failed)
        return NULL;

    program->as.program.body.head = head;
    return program;
}

AstNode *parser_parse_expression_text(Arena *arena, const char *source, size_t length,
                                      char *error, size_t error_size)
{
    Parser parser;
    AstNode *expression;

    parser_init(&parser, arena, source, length);
    expression = parse_expression(&parser);

    if (expression == NULL) {
        snprintf(error, error_size, "%d:%d: %s", parser_error_line(&parser),
                 parser_error_column(&parser), parser_error(&parser));
        return NULL;
    }

    while (check(&parser, TOK_NEWLINE))
        advance(&parser);

    if (parser.failed) {
        snprintf(error, error_size, "%d:%d: %s", parser_error_line(&parser),
                 parser_error_column(&parser), parser_error(&parser));
        return NULL;
    }

    if (!check(&parser, TOK_EOF)) {
        snprintf(error, error_size, "%d:%d: unexpected %s", parser.current.line,
                 parser.current.column, token_type_name(parser.current.type));
        return NULL;
    }

    error[0] = '\0';
    return expression;
}

const char *parser_error(const Parser *parser)
{
    return parser->error;
}

int parser_error_line(const Parser *parser)
{
    return parser->error_line;
}

int parser_error_column(const Parser *parser)
{
    return parser->error_column;
}
