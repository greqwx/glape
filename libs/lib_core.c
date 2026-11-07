// (c) Glape Contributors

#include "lib_core.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int expect_arity(Interpreter *interp, const char *name, size_t argc, size_t expected)
{
    if (argc != expected) {
        interp_fail(interp, 0, 0, "%s expects %d argument(s), got %d", name, (int)expected,
                    (int)argc);
        return 0;
    }

    return 1;
}

static ExecStatus native_print(Interpreter *interp, Value *args, size_t argc, Value *result)
{
    size_t i;

    for (i = 0; i < argc; i++) {
        StrBuf buffer;

        strbuf_init(&buffer);

        if (!value_format(args[i], &buffer)) {
            strbuf_free(&buffer);
            interp_fail(interp, 0, 0, "out of memory");
            return EXEC_ERROR;
        }

        if (i > 0)
            fputc(' ', interp->out);

        if (buffer.data != NULL)
            fwrite(buffer.data, 1, buffer.length, interp->out);

        strbuf_free(&buffer);
    }

    fputc('\n', interp->out);
    *result = value_nil();
    return EXEC_OK;
}

static ExecStatus native_fail(Interpreter *interp, Value *args, size_t argc, Value *result)
{
    StrBuf buffer;

    (void)result;

    if (!expect_arity(interp, "fail", argc, 1))
        return EXEC_ERROR;

    strbuf_init(&buffer);

    if (!value_format(args[0], &buffer)) {
        strbuf_free(&buffer);
        interp_fail(interp, 0, 0, "out of memory");
        return EXEC_ERROR;
    }

    {
        Value message = value_string(&interp->gc, buffer.data != NULL ? buffer.data : "",
                                     buffer.length);
        strbuf_free(&buffer);

        if (message.type != VAL_STRING) {
            interp_fail(interp, 0, 0, "out of memory");
            return EXEC_ERROR;
        }

        interp_raise(interp, message);
    }

    return EXEC_ERROR;
}

static ExecStatus native_len(Interpreter *interp, Value *args, size_t argc, Value *result)
{
    if (!expect_arity(interp, "len", argc, 1))
        return EXEC_ERROR;

    switch (args[0].type) {
    case VAL_STRING:
        *result = value_int((long long)args[0].as.string->length);
        return EXEC_OK;
    case VAL_LIST:
        *result = value_int((long long)args[0].as.list->count);
        return EXEC_OK;
    case VAL_MAP:
        *result = value_int((long long)args[0].as.map->count);
        return EXEC_OK;
    default:
        interp_fail(interp, 0, 0, "len does not support %s", value_type_name(args[0]));
        return EXEC_ERROR;
    }
}

static ExecStatus native_to_str(Interpreter *interp, Value *args, size_t argc, Value *result)
{
    StrBuf buffer;

    if (!expect_arity(interp, "to_str", argc, 1))
        return EXEC_ERROR;

    strbuf_init(&buffer);

    if (!value_format(args[0], &buffer)) {
        strbuf_free(&buffer);
        interp_fail(interp, 0, 0, "out of memory");
        return EXEC_ERROR;
    }

    *result = value_string(&interp->gc, buffer.data != NULL ? buffer.data : "", buffer.length);
    strbuf_free(&buffer);

    if (result->type != VAL_STRING) {
        interp_fail(interp, 0, 0, "out of memory");
        return EXEC_ERROR;
    }

    return EXEC_OK;
}

static char *trim_text(char *text)
{
    char *end;

    while (*text != '\0' && isspace((unsigned char)*text))
        text += 1;

    end = text + strlen(text);

    while (end > text && isspace((unsigned char)end[-1]))
        end -= 1;

    *end = '\0';
    return text;
}

static ExecStatus native_to_int(Interpreter *interp, Value *args, size_t argc, Value *result)
{
    if (!expect_arity(interp, "to_int", argc, 1))
        return EXEC_ERROR;

    switch (args[0].type) {
    case VAL_INT:
        *result = args[0];
        return EXEC_OK;
    case VAL_FLOAT: {
        double number = args[0].as.floating;

        if (number != number || number < -9223372036854775808.0 ||
            number >= 9223372036854775808.0) {
            interp_fail(interp, 0, 0, "cannot convert float to int");
            return EXEC_ERROR;
        }

        *result = value_int((long long)number);
        return EXEC_OK;
    }
    case VAL_BOOL:
        *result = value_int(args[0].as.boolean ? 1 : 0);
        return EXEC_OK;
    case VAL_STRING: {
        char *text = (char *)malloc(args[0].as.string->length + 1);
        char *end;
        long long value;

        if (text == NULL) {
            interp_fail(interp, 0, 0, "out of memory");
            return EXEC_ERROR;
        }

        memcpy(text, args[0].as.string->chars, args[0].as.string->length);
        text[args[0].as.string->length] = '\0';

        {
            char *trimmed = trim_text(text);

            errno = 0;
            value = strtoll(trimmed, &end, 10);

            if (end == trimmed || *end != '\0' || errno == ERANGE) {
                free(text);
                interp_fail(interp, 0, 0, "cannot convert string to int");
                return EXEC_ERROR;
            }
        }

        free(text);
        *result = value_int(value);
        return EXEC_OK;
    }
    default:
        interp_fail(interp, 0, 0, "to_int does not support %s", value_type_name(args[0]));
        return EXEC_ERROR;
    }
}

static ExecStatus native_to_float(Interpreter *interp, Value *args, size_t argc, Value *result)
{
    if (!expect_arity(interp, "to_float", argc, 1))
        return EXEC_ERROR;

    switch (args[0].type) {
    case VAL_FLOAT:
        *result = args[0];
        return EXEC_OK;
    case VAL_INT:
        *result = value_float((double)args[0].as.integer);
        return EXEC_OK;
    case VAL_BOOL:
        *result = value_float(args[0].as.boolean ? 1.0 : 0.0);
        return EXEC_OK;
    case VAL_STRING: {
        char *text = (char *)malloc(args[0].as.string->length + 1);
        char *end;
        double value;

        if (text == NULL) {
            interp_fail(interp, 0, 0, "out of memory");
            return EXEC_ERROR;
        }

        memcpy(text, args[0].as.string->chars, args[0].as.string->length);
        text[args[0].as.string->length] = '\0';

        {
            char *trimmed = trim_text(text);

            errno = 0;
            value = strtod(trimmed, &end);

            if (end == trimmed || *end != '\0' || errno == ERANGE) {
                free(text);
                interp_fail(interp, 0, 0, "cannot convert string to float");
                return EXEC_ERROR;
            }
        }

        free(text);
        *result = value_float(value);
        return EXEC_OK;
    }
    default:
        interp_fail(interp, 0, 0, "to_float does not support %s", value_type_name(args[0]));
        return EXEC_ERROR;
    }
}

static ExecStatus native_type(Interpreter *interp, Value *args, size_t argc, Value *result)
{
    const char *name;

    if (!expect_arity(interp, "type", argc, 1))
        return EXEC_ERROR;

    name = value_type_name(args[0]);
    *result = value_string(&interp->gc, name, strlen(name));

    if (result->type != VAL_STRING) {
        interp_fail(interp, 0, 0, "out of memory");
        return EXEC_ERROR;
    }

    return EXEC_OK;
}

void lib_core_install(Interpreter *interp)
{
    interp_register_native(interp, "print", native_print);
    interp_register_native(interp, "fail", native_fail);
    interp_register_native(interp, "len", native_len);
    interp_register_native(interp, "to_str", native_to_str);
    interp_register_native(interp, "to_int", native_to_int);
    interp_register_native(interp, "to_float", native_to_float);
    interp_register_native(interp, "type", native_type);
}
