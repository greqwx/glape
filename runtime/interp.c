// (c) Glape Contributors

#include "interp.h"

#include <limits.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parse/parser.h"

#include "libs/registry.h"

static ExecStatus eval_expr(Interpreter *interp, AstNode *node, Value *out);
static ExecStatus exec_block(Interpreter *interp, const AstBlock *block, Env *parent);
static ExecStatus exec_stmt(Interpreter *interp, AstNode *node);

void interp_init(Interpreter *interp, Arena *arena, FILE *out, const char *file)
{
    memset(interp, 0, sizeof *interp);
    gc_init(&interp->gc);
    interp->arena = arena;
    interp->out = out != NULL ? out : stdout;
    interp->file = file;
    interp->globals = env_new(&interp->gc, NULL);
    interp->env = interp->globals;
    interp->return_value = value_nil();
    interp->frame_count = 0;
    runtime_error_init(&interp->error);

    /*
     * These four fields stay live for the whole run, so they are rooted once
     * and never popped. Temporaries are rooted where they are created.
     */
    gc_push_root(&interp->gc, GC_ROOT_ENV, &interp->globals);
    gc_push_root(&interp->gc, GC_ROOT_ENV, &interp->env);
    gc_push_root(&interp->gc, GC_ROOT_VALUE, &interp->return_value);
    gc_push_root(&interp->gc, GC_ROOT_VALUE, &interp->error.value);
}

void interp_free(Interpreter *interp)
{
    gc_free_all(&interp->gc);
}

static void update_top_frame(Interpreter *interp, int line, int column)
{
    if (interp->frame_count > 0) {
        CallFrame *frame = &interp->frames[interp->frame_count - 1];

        frame->line = line;
        frame->column = column;
    }
}

static void capture_stack(Interpreter *interp)
{
    if (interp->error.frame_count > 0 || interp->frame_count <= 0)
        return;

    runtime_error_capture(&interp->error, interp->frames, interp->frame_count);
}

Native *interp_make_native(Interpreter *interp, const char *name, NativeFn fn)
{
    Native *native = (Native *)arena_alloc(interp->arena, sizeof *native);

    if (native == NULL)
        return NULL;

    native->name = name;
    native->fn = fn;
    return native;
}

void interp_register_native(Interpreter *interp, const char *name, NativeFn fn)
{
    Native *native = interp_make_native(interp, name, fn);

    if (native != NULL)
        env_define(interp->globals, name, value_native(native));
}

void interp_raise(Interpreter *interp, Value value)
{
    StrBuf buffer;

    interp->error.set = 1;
    interp->error.value = value;
    interp->error.file = interp->file;
    interp->error.line = 0;
    interp->error.column = 0;
    interp->error.frame_count = 0;

    strbuf_init(&buffer);

    if (value_format(value, &buffer) && buffer.data != NULL)
        snprintf(interp->error.message, sizeof interp->error.message, "%s", buffer.data);
    else
        interp->error.message[0] = '\0';

    strbuf_free(&buffer);
}

void interp_fail(Interpreter *interp, int line, int column, const char *format, ...)
{
    va_list args;

    if (line <= 0 && interp->frame_count > 0) {
        const CallFrame *top = &interp->frames[interp->frame_count - 1];

        line = top->line;
        column = top->column;
    }

    interp->error.set = 1;
    interp->error.value = value_nil();
    interp->error.file = interp->file;
    interp->error.line = line;
    interp->error.column = column;
    interp->error.frame_count = 0;

    if (line > 0)
        update_top_frame(interp, line, column);

    va_start(args, format);
    vsnprintf(interp->error.message, sizeof interp->error.message, format, args);
    va_end(args);
}

static int value_as_number(Value value, double *out, int *is_int, long long *integer)
{
    if (value.type == VAL_INT) {
        *out = (double)value.as.integer;
        *is_int = 1;
        *integer = value.as.integer;
        return 1;
    }

    if (value.type == VAL_FLOAT) {
        *out = value.as.floating;
        *is_int = 0;
        *integer = 0;
        return 1;
    }

    return 0;
}

static int checked_add(long long left, long long right, long long *out)
{
    if ((right > 0 && left > LLONG_MAX - right) ||
        (right < 0 && left < LLONG_MIN - right))
        return 0;

    *out = left + right;
    return 1;
}

static int checked_sub(long long left, long long right, long long *out)
{
    if ((right > 0 && left < LLONG_MIN + right) ||
        (right < 0 && left > LLONG_MAX + right))
        return 0;

    *out = left - right;
    return 1;
}

static int checked_mul(long long left, long long right, long long *out)
{
    if (left == 0 || right == 0) {
        *out = 0;
        return 1;
    }

    if ((left == -1 && right == LLONG_MIN) || (right == -1 && left == LLONG_MIN))
        return 0;

    if (left > 0) {
        if (right > 0) {
            if (left > LLONG_MAX / right)
                return 0;
        } else {
            if (right < LLONG_MIN / left)
                return 0;
        }
    } else {
        if (right > 0) {
            if (left < LLONG_MIN / right)
                return 0;
        } else {
            if (left < LLONG_MAX / right)
                return 0;
        }
    }

    *out = left * right;
    return 1;
}

static int checked_div(long long left, long long right, long long *out)
{
    if (left == LLONG_MIN && right == -1)
        return 0;

    *out = left / right;
    return 1;
}

static int checked_mod(long long left, long long right, long long *out)
{
    if (left == LLONG_MIN && right == -1)
        return 0;

    *out = left % right;
    return 1;
}

static int checked_neg(long long value, long long *out)
{
    if (value == LLONG_MIN)
        return 0;

    *out = -value;
    return 1;
}

static ExecStatus eval_string(Interpreter *interp, AstNode *node, Value *out);

static ExecStatus apply_binary(Interpreter *interp, AstNode *node, Value left, Value right,
                               Value *out);

static ExecStatus eval_binary(Interpreter *interp, AstNode *node, Value *out)
{
    TokenType op = node->as.binary.op;
    Value left;
    Value right;
    ExecStatus status;

    if (op == TOK_AND) {
        status = eval_expr(interp, node->as.binary.left, &left);
        if (status != EXEC_OK)
            return status;

        if (!value_truthy(left)) {
            *out = left;
            return EXEC_OK;
        }

        return eval_expr(interp, node->as.binary.right, out);
    }

    if (op == TOK_OR) {
        status = eval_expr(interp, node->as.binary.left, &left);
        if (status != EXEC_OK)
            return status;

        if (value_truthy(left)) {
            *out = left;
            return EXEC_OK;
        }

        return eval_expr(interp, node->as.binary.right, out);
    }

    status = eval_expr(interp, node->as.binary.left, &left);
    if (status != EXEC_OK)
        return status;

    gc_push_root(&interp->gc, GC_ROOT_VALUE, &left);

    status = eval_expr(interp, node->as.binary.right, &right);
    if (status != EXEC_OK) {
        gc_pop_roots(&interp->gc, 1);
        return status;
    }

    gc_push_root(&interp->gc, GC_ROOT_VALUE, &right);

    status = apply_binary(interp, node, left, right, out);
    gc_pop_roots(&interp->gc, 2);
    return status;
}

static ExecStatus apply_binary(Interpreter *interp, AstNode *node, Value left, Value right,
                               Value *out)
{
    TokenType op = node->as.binary.op;

    if (op == TOK_EQ) {
        *out = value_bool(value_equals(left, right));
        return EXEC_OK;
    }

    if (op == TOK_NE) {
        *out = value_bool(!value_equals(left, right));
        return EXEC_OK;
    }

    if (op == TOK_PLUS && left.type == VAL_LIST && right.type == VAL_LIST) {
        Value list = value_list(&interp->gc);
        size_t i;

        for (i = 0; i < left.as.list->count; i++) {
            if (!value_list_push(list, left.as.list->items[i])) {
                interp_fail(interp, node->line, node->column, "out of memory");
                return EXEC_ERROR;
            }
        }

        for (i = 0; i < right.as.list->count; i++) {
            if (!value_list_push(list, right.as.list->items[i])) {
                interp_fail(interp, node->line, node->column, "out of memory");
                return EXEC_ERROR;
            }
        }

        *out = list;
        return EXEC_OK;
    }

    if (op == TOK_PLUS && (left.type == VAL_STRING || right.type == VAL_STRING)) {
        StrBuf buffer;

        strbuf_init(&buffer);

        if (!value_format(left, &buffer) || !value_format(right, &buffer)) {
            strbuf_free(&buffer);
            interp_fail(interp, node->line, node->column, "out of memory");
            return EXEC_ERROR;
        }

        *out = value_string(&interp->gc, buffer.data != NULL ? buffer.data : "", buffer.length);
        strbuf_free(&buffer);

        if (out->type != VAL_STRING) {
            interp_fail(interp, node->line, node->column, "out of memory");
            return EXEC_ERROR;
        }

        return EXEC_OK;
    }

    if (op == TOK_LT || op == TOK_LE || op == TOK_GT || op == TOK_GE) {
        int comparison;
        int unordered = 0;

        if (left.type == VAL_STRING && right.type == VAL_STRING) {
            size_t length = left.as.string->length < right.as.string->length
                                ? left.as.string->length
                                : right.as.string->length;

            comparison = memcmp(left.as.string->chars, right.as.string->chars, length);

            if (comparison == 0) {
                if (left.as.string->length < right.as.string->length)
                    comparison = -1;
                else if (left.as.string->length > right.as.string->length)
                    comparison = 1;
            }
        } else if ((left.type == VAL_INT || left.type == VAL_FLOAT) &&
                   (right.type == VAL_INT || right.type == VAL_FLOAT)) {
            comparison = value_number_compare(left, right, &unordered);
        } else {
            interp_fail(interp, node->line, node->column, "cannot compare %s and %s",
                        value_type_name(left), value_type_name(right));
            return EXEC_ERROR;
        }

        if (unordered) {
            *out = value_bool(0);
            return EXEC_OK;
        }

        switch (op) {
        case TOK_LT:
            *out = value_bool(comparison < 0);
            break;
        case TOK_LE:
            *out = value_bool(comparison <= 0);
            break;
        case TOK_GT:
            *out = value_bool(comparison > 0);
            break;
        default:
            *out = value_bool(comparison >= 0);
            break;
        }

        return EXEC_OK;
    }

    if (op == TOK_PLUS || op == TOK_MINUS || op == TOK_STAR || op == TOK_SLASH ||
        op == TOK_PERCENT) {
        double left_number;
        double right_number;
        int left_int;
        int right_int;
        long long left_integer;
        long long right_integer;

        if (!value_as_number(left, &left_number, &left_int, &left_integer) ||
            !value_as_number(right, &right_number, &right_int, &right_integer)) {
            interp_fail(interp, node->line, node->column, "operator %s expects numbers, got %s and %s",
                        token_type_name(op), value_type_name(left), value_type_name(right));
            return EXEC_ERROR;
        }

        if (left_int && right_int) {
            long long result;
            int ok;

            switch (op) {
            case TOK_PLUS:
                ok = checked_add(left_integer, right_integer, &result);
                break;
            case TOK_MINUS:
                ok = checked_sub(left_integer, right_integer, &result);
                break;
            case TOK_STAR:
                ok = checked_mul(left_integer, right_integer, &result);
                break;
            case TOK_SLASH:
                if (right_integer == 0) {
                    interp_fail(interp, node->line, node->column, "division by zero");
                    return EXEC_ERROR;
                }
                ok = checked_div(left_integer, right_integer, &result);
                break;
            default:
                if (right_integer == 0) {
                    interp_fail(interp, node->line, node->column, "division by zero");
                    return EXEC_ERROR;
                }
                ok = checked_mod(left_integer, right_integer, &result);
                break;
            }

            if (!ok) {
                interp_fail(interp, node->line, node->column, "integer overflow");
                return EXEC_ERROR;
            }

            *out = value_int(result);
            return EXEC_OK;
        }

        if (right_number == 0.0) {
            interp_fail(interp, node->line, node->column, "division by zero");
            return EXEC_ERROR;
        }

        switch (op) {
        case TOK_PLUS:
            *out = value_float(left_number + right_number);
            break;
        case TOK_MINUS:
            *out = value_float(left_number - right_number);
            break;
        case TOK_STAR:
            *out = value_float(left_number * right_number);
            break;
        case TOK_SLASH:
            *out = value_float(left_number / right_number);
            break;
        default:
            *out = value_float(fmod(left_number, right_number));
            break;
        }

        return EXEC_OK;
    }

    interp_fail(interp, node->line, node->column, "unsupported operator");
    return EXEC_ERROR;
}

static ExecStatus eval_call(Interpreter *interp, AstNode *node, Value *out);

static ExecStatus eval_index(Interpreter *interp, AstNode *node, Value *out)
{
    Value object;
    Value index;
    ExecStatus status = eval_expr(interp, node->as.index.object, &object);

    if (status != EXEC_OK)
        return status;

    gc_push_root(&interp->gc, GC_ROOT_VALUE, &object);

    status = eval_expr(interp, node->as.index.index, &index);

    gc_pop_roots(&interp->gc, 1);

    if (status != EXEC_OK)
        return status;

    if (object.type == VAL_LIST) {
        if (index.type != VAL_INT) {
            interp_fail(interp, node->line, node->column, "list index must be int, got %s",
                        value_type_name(index));
            return EXEC_ERROR;
        }

        if (index.as.integer < 0 || (size_t)index.as.integer >= object.as.list->count) {
            interp_fail(interp, node->line, node->column,
                        "index %lld out of bounds for list of length %d", index.as.integer,
                        (int)object.as.list->count);
            return EXEC_ERROR;
        }

        *out = object.as.list->items[index.as.integer];
        return EXEC_OK;
    }

    if (object.type == VAL_STRING) {
        if (index.type != VAL_INT) {
            interp_fail(interp, node->line, node->column, "string index must be int, got %s",
                        value_type_name(index));
            return EXEC_ERROR;
        }

        if (index.as.integer < 0 || (size_t)index.as.integer >= object.as.string->length) {
            interp_fail(interp, node->line, node->column,
                        "index %lld out of bounds for string of length %d", index.as.integer,
                        (int)object.as.string->length);
            return EXEC_ERROR;
        }

        *out = value_string(&interp->gc, object.as.string->chars + index.as.integer, 1);
        return EXEC_OK;
    }

    if (object.type == VAL_MAP) {
        Value value;

        if (index.type != VAL_STRING) {
            interp_fail(interp, node->line, node->column, "map index must be string, got %s",
                        value_type_name(index));
            return EXEC_ERROR;
        }

        if (!value_map_get(object, index.as.string->chars, index.as.string->length, &value)) {
            interp_fail(interp, node->line, node->column, "map has no key '%.*s'",
                        (int)index.as.string->length, index.as.string->chars);
            return EXEC_ERROR;
        }

        *out = value;
        return EXEC_OK;
    }

    interp_fail(interp, node->line, node->column, "cannot index %s", value_type_name(object));
    return EXEC_ERROR;
}

static ExecStatus assign_index(Interpreter *interp, AstNode *node, Value value)
{
    Value object;
    Value index;
    ExecStatus status = eval_expr(interp, node->as.index.object, &object);

    if (status != EXEC_OK)
        return status;

    gc_push_root(&interp->gc, GC_ROOT_VALUE, &object);

    status = eval_expr(interp, node->as.index.index, &index);

    gc_pop_roots(&interp->gc, 1);

    if (status != EXEC_OK)
        return status;

    if (object.type == VAL_LIST) {
        if (index.type != VAL_INT) {
            interp_fail(interp, node->line, node->column, "list index must be int, got %s",
                        value_type_name(index));
            return EXEC_ERROR;
        }

        if (index.as.integer < 0 || (size_t)index.as.integer >= object.as.list->count) {
            interp_fail(interp, node->line, node->column,
                        "index %lld out of bounds for list of length %d", index.as.integer,
                        (int)object.as.list->count);
            return EXEC_ERROR;
        }

        object.as.list->items[index.as.integer] = value;
        return EXEC_OK;
    }

    if (object.type == VAL_MAP) {
        if (index.type != VAL_STRING) {
            interp_fail(interp, node->line, node->column, "map index must be string, got %s",
                        value_type_name(index));
            return EXEC_ERROR;
        }

        if (!value_map_set(object, index.as.string, value)) {
            interp_fail(interp, node->line, node->column, "out of memory");
            return EXEC_ERROR;
        }

        return EXEC_OK;
    }

    if (object.type == VAL_STRING) {
        interp_fail(interp, node->line, node->column, "cannot assign to string index");
        return EXEC_ERROR;
    }

    interp_fail(interp, node->line, node->column, "cannot index %s", value_type_name(object));
    return EXEC_ERROR;
}

static ExecStatus eval_expr(Interpreter *interp, AstNode *node, Value *out)
{
    update_top_frame(interp, node->line, node->column);
    gc_safe_point(&interp->gc);

    switch (node->kind) {
    case AST_NUMBER:
        *out = node->as.number.is_float ? value_float(node->as.number.floating)
                                        : value_int(node->as.number.integer);
        return EXEC_OK;

    case AST_STRING:
        return eval_string(interp, node, out);

    case AST_BOOL:
        *out = value_bool(node->as.boolean.value);
        return EXEC_OK;

    case AST_NIL:
        *out = value_nil();
        return EXEC_OK;

    case AST_IDENT:
        if (!env_get(interp->env, node->as.ident.name, out)) {
            interp_fail(interp, node->line, node->column, "undefined variable '%s'",
                        node->as.ident.name);
            return EXEC_ERROR;
        }
        return EXEC_OK;

    case AST_LIST: {
        AstNodeList *item;
        Value list = value_list(&interp->gc);

        if (list.type != VAL_LIST) {
            interp_fail(interp, node->line, node->column, "out of memory");
            return EXEC_ERROR;
        }

        gc_push_root(&interp->gc, GC_ROOT_VALUE, &list);

        for (item = node->as.list.items; item != NULL; item = item->next) {
            Value value;
            ExecStatus status = eval_expr(interp, item->node, &value);

            if (status != EXEC_OK) {
                gc_pop_roots(&interp->gc, 1);
                return status;
            }

            if (!value_list_push(list, value)) {
                gc_pop_roots(&interp->gc, 1);
                interp_fail(interp, node->line, node->column, "out of memory");
                return EXEC_ERROR;
            }
        }

        gc_pop_roots(&interp->gc, 1);
        *out = list;
        return EXEC_OK;
    }

    case AST_MAP: {
        AstMapEntry *entry;
        Value map = value_map(&interp->gc);

        if (map.type != VAL_MAP) {
            interp_fail(interp, node->line, node->column, "out of memory");
            return EXEC_ERROR;
        }

        gc_push_root(&interp->gc, GC_ROOT_VALUE, &map);

        for (entry = node->as.map.entries; entry != NULL; entry = entry->next) {
            Value key;
            Value value;
            ExecStatus status;

            if (entry->key->kind == AST_IDENT) {
                const char *name = entry->key->as.ident.name;
                key = value_string(&interp->gc, name, strlen(name));
            } else {
                status = eval_expr(interp, entry->key, &key);
                if (status != EXEC_OK) {
                    gc_pop_roots(&interp->gc, 1);
                    return status;
                }

                if (key.type != VAL_STRING) {
                    StrBuf buffer;
                    Value converted;

                    strbuf_init(&buffer);
                    if (!value_format(key, &buffer)) {
                        strbuf_free(&buffer);
                        gc_pop_roots(&interp->gc, 1);
                        interp_fail(interp, node->line, node->column, "out of memory");
                        return EXEC_ERROR;
                    }

                    converted = value_string(&interp->gc, buffer.data != NULL ? buffer.data : "",
                                             buffer.length);
                    strbuf_free(&buffer);
                    key = converted;
                }
            }

            if (key.type != VAL_STRING) {
                gc_pop_roots(&interp->gc, 1);
                interp_fail(interp, node->line, node->column, "out of memory");
                return EXEC_ERROR;
            }

            gc_push_root(&interp->gc, GC_ROOT_VALUE, &key);

            status = eval_expr(interp, entry->value, &value);
            if (status != EXEC_OK) {
                gc_pop_roots(&interp->gc, 2);
                return status;
            }

            if (!value_map_set(map, key.as.string, value)) {
                gc_pop_roots(&interp->gc, 2);
                interp_fail(interp, node->line, node->column, "out of memory");
                return EXEC_ERROR;
            }

            gc_pop_roots(&interp->gc, 1);
        }

        gc_pop_roots(&interp->gc, 1);
        *out = map;
        return EXEC_OK;
    }

    case AST_UNARY: {
        Value operand;
        ExecStatus status = eval_expr(interp, node->as.unary.operand, &operand);

        if (status != EXEC_OK)
            return status;

        if (node->as.unary.op == TOK_NOT) {
            *out = value_bool(!value_truthy(operand));
            return EXEC_OK;
        }

        if (operand.type == VAL_INT) {
            if (node->as.unary.op == TOK_MINUS) {
                long long negated;

                if (!checked_neg(operand.as.integer, &negated)) {
                    interp_fail(interp, node->line, node->column, "integer overflow");
                    return EXEC_ERROR;
                }

                *out = value_int(negated);
            } else {
                *out = operand;
            }

            return EXEC_OK;
        }

        if (operand.type == VAL_FLOAT) {
            *out = value_float(node->as.unary.op == TOK_MINUS ? -operand.as.floating
                                                              : operand.as.floating);
            return EXEC_OK;
        }

        interp_fail(interp, node->line, node->column, "unary %s expects a number, got %s",
                    token_type_name(node->as.unary.op), value_type_name(operand));
        return EXEC_ERROR;
    }

    case AST_BINARY:
        return eval_binary(interp, node, out);

    case AST_FIELD: {
        Value object;
        Value value;
        ExecStatus status = eval_expr(interp, node->as.field.object, &object);

        if (status != EXEC_OK)
            return status;

        if (object.type == VAL_MAP) {
            if (value_map_get(object, node->as.field.name, strlen(node->as.field.name), &value)) {
                *out = value;
                return EXEC_OK;
            }

            interp_fail(interp, node->line, node->column, "map has no key '%s'",
                        node->as.field.name);
            return EXEC_ERROR;
        }

        interp_fail(interp, node->line, node->column, "cannot access field '%s' on %s",
                    node->as.field.name, value_type_name(object));
        return EXEC_ERROR;
    }

    case AST_INDEX:
        return eval_index(interp, node, out);

    case AST_CALL:
        return eval_call(interp, node, out);

    default:
        interp_fail(interp, node->line, node->column, "cannot evaluate this expression");
        return EXEC_ERROR;
    }
}

static int is_ident_start(int c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

static int is_ident_char(int c)
{
    return is_ident_start(c) || (c >= '0' && c <= '9');
}

static int hex_digit(int c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return -1;
}

static int decode_escape(const char *text, size_t length, size_t *index, StrBuf *buffer)
{
    char c = text[*index];

    *index += 1;

    switch (c) {
    case 'n':
        return strbuf_append_char(buffer, '\n');
    case 'r':
        return strbuf_append_char(buffer, '\r');
    case 't':
        return strbuf_append_char(buffer, '\t');
    case '0':
        return strbuf_append_char(buffer, '\0');
    case '\\':
    case '"':
    case '\'':
    case '$':
        return strbuf_append_char(buffer, c);
    case 'x': {
        int high;
        int low;
        char decoded;

        if (*index + 1 >= length)
            return strbuf_append_char(buffer, 'x');

        high = hex_digit((unsigned char)text[*index]);
        low = hex_digit((unsigned char)text[*index + 1]);

        if (high < 0 || low < 0)
            return strbuf_append_char(buffer, 'x');

        decoded = (char)((high << 4) | low);
        *index += 2;
        return strbuf_append_char(buffer, decoded);
    }
    default:
        return strbuf_append_char(buffer, c);
    }
}

static ExecStatus eval_interpolation(Interpreter *interp, AstNode *node, size_t start, size_t end,
                                     StrBuf *buffer)
{
    const char *text = node->as.string.value;
    size_t length = end - start;
    AstNode *expression;
    char error[256];
    Value value;
    ExecStatus status;

    expression = parser_parse_expression_text(interp->arena, text + start, length, error,
                                              sizeof error);

    if (expression == NULL) {
        interp_fail(interp, node->line, node->column, "bad interpolation: %s", error);
        return EXEC_ERROR;
    }

    status = eval_expr(interp, expression, &value);
    if (status != EXEC_OK)
        return status;

    if (!value_format(value, buffer)) {
        interp_fail(interp, node->line, node->column, "out of memory");
        return EXEC_ERROR;
    }

    return EXEC_OK;
}

static ExecStatus eval_string(Interpreter *interp, AstNode *node, Value *out)
{
    const char *text = node->as.string.value;
    size_t length = strlen(text);
    size_t i = 0;
    StrBuf buffer;

    strbuf_init(&buffer);

    while (i < length) {
        char c = text[i];

        if (c == '\\') {
            i += 1;

            if (i >= length)
                break;

            if (!decode_escape(text, length, &i, &buffer))
                goto oom_escape;

            continue;
        }

        if (c == '$' && i + 1 < length) {
            if (text[i + 1] == '{') {
                size_t start = i + 2;
                size_t j = start;
                int depth = 1;

                while (j < length && depth > 0) {
                    char current = text[j];

                    if (current == '\\') {
                        j += 2;
                        continue;
                    }

                    if (current == '"') {
                        j += 1;
                        while (j < length && text[j] != '"') {
                            if (text[j] == '\\' && j + 1 < length)
                                j += 1;
                            j += 1;
                        }
                        if (j < length)
                            j += 1;
                        continue;
                    }

                    if (current == '{') {
                        depth += 1;
                    } else if (current == '}') {
                        depth -= 1;
                        if (depth == 0)
                            break;
                    }

                    j += 1;
                }

                if (depth != 0) {
                    strbuf_free(&buffer);
                    interp_fail(interp, node->line, node->column, "unterminated interpolation");
                    return EXEC_ERROR;
                }

                {
                    ExecStatus status = eval_interpolation(interp, node, start, j, &buffer);

                    if (status != EXEC_OK) {
                        strbuf_free(&buffer);
                        return status;
                    }
                }

                i = j + 1;
                continue;
            }

            if (is_ident_start((unsigned char)text[i + 1])) {
                size_t start = i + 1;
                size_t j = start;
                Value value;

                while (j < length && is_ident_char((unsigned char)text[j]))
                    j += 1;

                {
                    char name[128];
                    size_t name_length = j - start;

                    if (name_length >= sizeof name)
                        name_length = sizeof name - 1;

                    memcpy(name, text + start, name_length);
                    name[name_length] = '\0';

                    if (!env_get(interp->env, name, &value)) {
                        strbuf_free(&buffer);
                        interp_fail(interp, node->line, node->column, "undefined variable '%s'",
                                    name);
                        return EXEC_ERROR;
                    }

                    if (!value_format(value, &buffer))
                        goto oom_escape;
                }

                i = j;
                continue;
            }
        }

        if (!strbuf_append_char(&buffer, c))
            goto oom_escape;

        i += 1;
    }

    *out = value_string(&interp->gc, buffer.data != NULL ? buffer.data : "", buffer.length);
    strbuf_free(&buffer);

    if (out->type != VAL_STRING) {
        interp_fail(interp, node->line, node->column, "out of memory");
        return EXEC_ERROR;
    }

    return EXEC_OK;

oom_escape:
    strbuf_free(&buffer);
    interp_fail(interp, node->line, node->column, "out of memory");
    return EXEC_ERROR;
}

static size_t count_params(const AstName *params)
{
    size_t count = 0;

    while (params != NULL) {
        count += 1;
        params = params->next;
    }

    return count;
}

static ExecStatus call_function(Interpreter *interp, Function *function, Value *args, size_t argc,
                                Value *out, int line, int column)
{
    size_t expected = count_params(function->params);
    Env *call_env;
    Env *caller_env;
    ExecStatus status;
    AstName *param;
    size_t index;
    const char *saved_file;
    CallFrame *frame;

    if (expected != argc) {
        interp_fail(interp, line, column, "function '%s' expects %d arguments, got %d",
                    function->name, (int)expected, (int)argc);
        return EXEC_ERROR;
    }

    if (interp->frame_count >= INTERP_MAX_FRAMES) {
        interp_fail(interp, line, column, "call depth limit reached");
        return EXEC_ERROR;
    }

    /*
     * The callee's environment chain starts at its closure and does not
     * include the caller's blocks, so the caller's current environment has to
     * stay rooted for the duration of the call.
     */
    caller_env = interp->env;
    gc_push_root(&interp->gc, GC_ROOT_ENV, &caller_env);

    call_env = env_new(&interp->gc, function->closure);
    if (call_env == NULL) {
        gc_pop_roots(&interp->gc, 1);
        interp_fail(interp, line, column, "out of memory");
        return EXEC_ERROR;
    }

    for (param = function->params, index = 0; param != NULL; param = param->next, index += 1)
        env_define(call_env, param->name, args[index]);

    saved_file = interp->file;
    interp->file = function->file != NULL ? function->file : saved_file;

    frame = &interp->frames[interp->frame_count];
    frame->function = function->name;
    frame->file = interp->file;
    frame->line = line;
    frame->column = column;
    interp->frame_count += 1;

    status = exec_block(interp, &function->body, call_env);

    gc_pop_roots(&interp->gc, 1);

    if (status == EXEC_BREAK || status == EXEC_CONTINUE) {
        CallFrame *top = &interp->frames[interp->frame_count - 1];

        interp_fail(interp, top->line, top->column,
                    status == EXEC_BREAK ? "break outside loop" : "skip outside loop");
        status = EXEC_ERROR;
    }

    if (status == EXEC_ERROR)
        capture_stack(interp);

    interp->frame_count -= 1;
    interp->file = saved_file;

    if (status == EXEC_RETURN) {
        *out = interp->return_value;
        interp->return_value = value_nil();
        status = EXEC_OK;
    } else if (status == EXEC_OK) {
        *out = value_nil();
    }

    return status;
}

static ExecStatus eval_call(Interpreter *interp, AstNode *node, Value *out)
{
    Value callee;
    Value *args = NULL;
    size_t argc = 0;
    size_t index = 0;
    size_t root_count;
    AstNodeList *arg;
    ExecStatus status;

    update_top_frame(interp, node->line, node->column);

    status = eval_expr(interp, node->as.call.callee, &callee);
    if (status != EXEC_OK)
        return status;

    gc_push_root(&interp->gc, GC_ROOT_VALUE, &callee);
    root_count = 1;

    for (arg = node->as.call.args; arg != NULL; arg = arg->next)
        argc += 1;

    if (argc > 0) {
        args = (Value *)malloc(argc * sizeof *args);
        if (args == NULL) {
            gc_pop_roots(&interp->gc, root_count);
            interp_fail(interp, node->line, node->column, "out of memory");
            return EXEC_ERROR;
        }

        memset(args, 0, argc * sizeof *args);

        for (index = 0; index < argc; index++) {
            gc_push_root(&interp->gc, GC_ROOT_VALUE, &args[index]);
            root_count += 1;
        }
    }

    index = 0;

    for (arg = node->as.call.args; arg != NULL; arg = arg->next, index += 1) {
        status = eval_expr(interp, arg->node, &args[index]);

        if (status != EXEC_OK) {
            free(args);
            gc_pop_roots(&interp->gc, root_count);
            return status;
        }
    }

    if (callee.type == VAL_NATIVE) {
        update_top_frame(interp, node->line, node->column);
        status = callee.as.native->fn(interp, args, argc, out);
        free(args);
        gc_pop_roots(&interp->gc, root_count);

        if (status == EXEC_ERROR && interp->error.line == 0) {
            interp->error.line = node->line;
            interp->error.column = node->column;
            update_top_frame(interp, node->line, node->column);
        }

        return status;
    }

    if (callee.type == VAL_FUNCTION) {
        update_top_frame(interp, node->line, node->column);
        status = call_function(interp, callee.as.function, args, argc, out, node->line,
                               node->column);
        free(args);
        gc_pop_roots(&interp->gc, root_count);
        return status;
    }

    free(args);
    gc_pop_roots(&interp->gc, root_count);
    interp_fail(interp, node->line, node->column, "%s is not callable", value_type_name(callee));
    return EXEC_ERROR;
}

static ExecStatus exec_loop(Interpreter *interp, AstNode *node)
{
    Value iterable;
    ExecStatus status = eval_expr(interp, node->as.loop.iterable, &iterable);
    size_t index = 0;

    if (status != EXEC_OK)
        return status;

    if (iterable.type != VAL_LIST && iterable.type != VAL_STRING && iterable.type != VAL_MAP) {
        interp_fail(interp, node->line, node->column, "cannot iterate over %s",
                    value_type_name(iterable));
        return EXEC_ERROR;
    }

    /*
     * The body may trigger collections, so the iterable itself has to stay
     * reachable for the whole loop.
     */
    gc_push_root(&interp->gc, GC_ROOT_VALUE, &iterable);

    for (;;) {
        size_t count;
        Value item;
        Env *iteration_env;
        Env *saved;

        if (iterable.type == VAL_LIST)
            count = iterable.as.list->count;
        else if (iterable.type == VAL_STRING)
            count = iterable.as.string->length;
        else
            count = iterable.as.map->count;

        if (index >= count)
            break;

        if (iterable.type == VAL_LIST) {
            item = iterable.as.list->items[index];
        } else if (iterable.type == VAL_STRING) {
            item = value_string(&interp->gc, iterable.as.string->chars + index, 1);
        } else {
            ObjString *key = iterable.as.map->entries[index].key;

            item = value_string(&interp->gc, key->chars, key->length);
        }

        if (item.type != VAL_STRING && iterable.type != VAL_LIST) {
            interp_fail(interp, node->line, node->column, "out of memory");
            status = EXEC_ERROR;
            break;
        }

        iteration_env = env_new(&interp->gc, interp->env);
        if (iteration_env == NULL) {
            interp_fail(interp, node->line, node->column, "out of memory");
            status = EXEC_ERROR;
            break;
        }

        env_define(iteration_env, node->as.loop.var, item);
        saved = interp->env;
        interp->env = iteration_env;
        status = exec_block(interp, &node->as.loop.body, iteration_env);
        interp->env = saved;

        if (status == EXEC_BREAK) {
            status = EXEC_OK;
            break;
        }

        if (status != EXEC_OK && status != EXEC_CONTINUE)
            break;

        status = EXEC_OK;
        index += 1;
    }

    gc_pop_roots(&interp->gc, 1);
    return status;
}

static ExecStatus exec_catch(Interpreter *interp, AstNode *node)
{
    Value expression_result;
    ExecStatus status = eval_expr(interp, node->as.catch_stmt.expr, &expression_result);

    if (status != EXEC_ERROR)
        return status;

    {
        Value error_value = interp->error.value;
        Env *catch_env;
        Env *saved;

        if (error_value.type == VAL_NIL && interp->error.message[0] != '\0') {
            error_value = value_string(&interp->gc, interp->error.message,
                                       strlen(interp->error.message));
        }

        gc_push_root(&interp->gc, GC_ROOT_VALUE, &error_value);

        catch_env = env_new(&interp->gc, interp->env);
        if (catch_env == NULL) {
            gc_pop_roots(&interp->gc, 1);
            interp_fail(interp, node->line, node->column, "out of memory");
            return EXEC_ERROR;
        }

        env_define(catch_env, node->as.catch_stmt.name, error_value);
        gc_pop_roots(&interp->gc, 1);

        runtime_error_clear(&interp->error);

        saved = interp->env;
        interp->env = catch_env;
        status = exec_block(interp, &node->as.catch_stmt.body, catch_env);
        interp->env = saved;
    }

    return status;
}

static ExecStatus exec_stmt(Interpreter *interp, AstNode *node)
{
    update_top_frame(interp, node->line, node->column);
    gc_safe_point(&interp->gc);

    switch (node->kind) {
    case AST_LOAD: {
        AstName *name;

        for (name = node->as.load.names; name != NULL; name = name->next) {
            ExecStatus status = libs_load(interp, name->name, node->line, node->column);

            if (status != EXEC_OK)
                return status;
        }

        return EXEC_OK;
    }

    case AST_DEFINE: {
        Function *function = (Function *)gc_alloc(&interp->gc, sizeof *function, GC_FUNCTION);

        if (function == NULL) {
            interp_fail(interp, node->line, node->column, "out of memory");
            return EXEC_ERROR;
        }

        function->name = node->as.define.name;
        function->file = interp->file;
        function->line = node->line;
        function->column = node->column;
        function->params = node->as.define.params;
        function->body = node->as.define.body;
        function->closure = interp->env;
        function->may_fail = node->as.define.may_fail;

        env_define(interp->env, node->as.define.name, value_function(function));
        return EXEC_OK;
    }

    case AST_IF: {
        Value condition;
        ExecStatus status = eval_expr(interp, node->as.if_stmt.condition, &condition);

        if (status != EXEC_OK)
            return status;

        if (value_truthy(condition))
            return exec_block(interp, &node->as.if_stmt.then_block, interp->env);

        if (node->as.if_stmt.has_else)
            return exec_block(interp, &node->as.if_stmt.else_block, interp->env);

        return EXEC_OK;
    }

    case AST_LOOP:
        return exec_loop(interp, node);

    case AST_ASSIGN: {
        Value value;
        ExecStatus status = eval_expr(interp, node->as.assign.value, &value);

        if (status != EXEC_OK)
            return status;

        gc_push_root(&interp->gc, GC_ROOT_VALUE, &value);

        if (node->as.assign.target->kind == AST_IDENT) {
            const char *name = node->as.assign.target->as.ident.name;

            if (!env_assign(interp->env, name, value))
                env_define(interp->env, name, value);

            gc_pop_roots(&interp->gc, 1);
            return EXEC_OK;
        }

        if (node->as.assign.target->kind == AST_FIELD) {
            AstNode *field = node->as.assign.target;
            Value object;
            Value key;

            status = eval_expr(interp, field->as.field.object, &object);
            if (status != EXEC_OK) {
                gc_pop_roots(&interp->gc, 1);
                return status;
            }

            if (object.type != VAL_MAP) {
                gc_pop_roots(&interp->gc, 1);
                interp_fail(interp, field->line, field->column, "cannot set field '%s' on %s",
                            field->as.field.name, value_type_name(object));
                return EXEC_ERROR;
            }

            key = value_string(&interp->gc, field->as.field.name,
                               strlen(field->as.field.name));

            if (key.type != VAL_STRING || !value_map_set(object, key.as.string, value)) {
                gc_pop_roots(&interp->gc, 1);
                interp_fail(interp, field->line, field->column, "out of memory");
                return EXEC_ERROR;
            }

            gc_pop_roots(&interp->gc, 1);
            return EXEC_OK;
        }

        if (node->as.assign.target->kind == AST_INDEX) {
            status = assign_index(interp, node->as.assign.target, value);
            gc_pop_roots(&interp->gc, 1);
            return status;
        }

        gc_pop_roots(&interp->gc, 1);
        interp_fail(interp, node->line, node->column, "invalid assignment target");
        return EXEC_ERROR;
    }

    case AST_EXPR: {
        Value ignored;

        return eval_expr(interp, node->as.expr_stmt.expr, &ignored);
    }

    case AST_CATCH:
        return exec_catch(interp, node);

    case AST_RETURN: {
        Value value = value_nil();

        if (node->as.ret.has_value) {
            ExecStatus status = eval_expr(interp, node->as.ret.value, &value);

            if (status != EXEC_OK)
                return status;
        }

        interp->return_value = value;
        return EXEC_RETURN;
    }

    case AST_SKIP:
    case AST_CONTINUE:
        return EXEC_CONTINUE;

    case AST_BREAK:
        return EXEC_BREAK;

    default:
        interp_fail(interp, node->line, node->column, "cannot execute this statement");
        return EXEC_ERROR;
    }
}

static ExecStatus exec_block(Interpreter *interp, const AstBlock *block, Env *parent)
{
    Env *env = env_new(&interp->gc, parent);
    Env *saved = interp->env;
    AstNode *node;
    ExecStatus status = EXEC_OK;

    if (env == NULL) {
        interp_fail(interp, block->head != NULL ? block->head->line : 0,
                    block->head != NULL ? block->head->column : 0, "out of memory");
        return EXEC_ERROR;
    }

    interp->env = env;

    for (node = block->head; node != NULL; node = node->next) {
        status = exec_stmt(interp, node);

        if (status != EXEC_OK)
            break;
    }

    interp->env = saved;
    return status;
}

static int program_is_declarations(const AstNode *program)
{
    const AstNode *node;

    for (node = program->as.program.body.head; node != NULL; node = node->next) {
        if (node->kind != AST_LOAD && node->kind != AST_DEFINE)
            return 0;
    }

    return 1;
}

static ExecStatus call_main(Interpreter *interp)
{
    Value main_value;

    if (!env_get(interp->globals, "main", &main_value) || main_value.type != VAL_FUNCTION)
        return EXEC_OK;

    {
        Function *function = main_value.as.function;
        size_t params = count_params(function->params);
        Value result;
        Value argument;

        if (params > 1) {
            interp_fail(interp, function->line, function->column,
                        "main takes at most one parameter");
            return EXEC_ERROR;
        }

        if (params == 1)
            argument = value_list(&interp->gc);

        return call_function(interp, function, params == 1 ? &argument : NULL, params, &result, 0,
                             0);
    }
}

ExecStatus interp_run(Interpreter *interp, AstNode *program)
{
    AstNode *node;
    AstNode *last = NULL;
    ExecStatus status = EXEC_OK;

    interp->env = interp->globals;

    for (node = program->as.program.body.head; node != NULL; node = node->next) {
        last = node;
        status = exec_stmt(interp, node);

        if (status != EXEC_OK)
            break;
    }

    if (status == EXEC_RETURN) {
        interp_fail(interp, last != NULL ? last->line : 0, last != NULL ? last->column : 0,
                    "return outside function");
        return EXEC_ERROR;
    }

    if (status == EXEC_BREAK || status == EXEC_CONTINUE) {
        interp_fail(interp, last != NULL ? last->line : 0, last != NULL ? last->column : 0,
                    status == EXEC_BREAK ? "break outside loop" : "skip outside loop");
        return EXEC_ERROR;
    }

    if (status != EXEC_OK)
        return status;

    if (program_is_declarations(program))
        return call_main(interp);

    return EXEC_OK;
}
