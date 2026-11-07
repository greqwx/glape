// (c) Glape Contributors

#include "lib_str.h"

#include <ctype.h>
#include <string.h>

#include "lib_api.h"

static int finish_string(Interpreter *interp, StrBuf *buffer, Value *result)
{
    *result = lib_string(interp, buffer->data != NULL ? buffer->data : "", buffer->length);
    strbuf_free(buffer);

    if (result->type != VAL_STRING) {
        interp_fail(interp, 0, 0, "out of memory");
        return 0;
    }

    return 1;
}

static ExecStatus build_case(Interpreter *interp, Value *args, size_t argc, Value *result,
                             int upper)
{
    const char *text;
    size_t length;
    size_t i;
    StrBuf buffer;

    if (!lib_expect_arity(interp, upper ? "str.upper" : "str.lower", argc, 1))
        return EXEC_ERROR;

    if (!lib_expect_string(interp, upper ? "str.upper" : "str.lower", "text", args[0], &text,
                           &length))
        return EXEC_ERROR;

    strbuf_init(&buffer);

    for (i = 0; i < length; i++) {
        unsigned char c = (unsigned char)text[i];
        char converted = (char)(upper ? toupper(c) : tolower(c));

        if (!strbuf_append_char(&buffer, converted)) {
            strbuf_free(&buffer);
            interp_fail(interp, 0, 0, "out of memory");
            return EXEC_ERROR;
        }
    }

    return finish_string(interp, &buffer, result) ? EXEC_OK : EXEC_ERROR;
}

static ExecStatus native_upper(Interpreter *interp, Value *args, size_t argc, Value *result)
{
    return build_case(interp, args, argc, result, 1);
}

static ExecStatus native_lower(Interpreter *interp, Value *args, size_t argc, Value *result)
{
    return build_case(interp, args, argc, result, 0);
}

static ExecStatus native_trim(Interpreter *interp, Value *args, size_t argc, Value *result)
{
    const char *text;
    size_t length;
    size_t start = 0;
    size_t end;

    if (!lib_expect_arity(interp, "str.trim", argc, 1))
        return EXEC_ERROR;

    if (!lib_expect_string(interp, "str.trim", "text", args[0], &text, &length))
        return EXEC_ERROR;

    end = length;

    while (start < end && isspace((unsigned char)text[start]))
        start++;

    while (end > start && isspace((unsigned char)text[end - 1]))
        end--;

    *result = lib_string(interp, text + start, end - start);

    if (result->type != VAL_STRING) {
        interp_fail(interp, 0, 0, "out of memory");
        return EXEC_ERROR;
    }

    return EXEC_OK;
}

static ExecStatus native_trim_start(Interpreter *interp, Value *args, size_t argc, Value *result)
{
    const char *text;
    size_t length;
    size_t start = 0;

    if (!lib_expect_arity(interp, "str.trim_start", argc, 1))
        return EXEC_ERROR;

    if (!lib_expect_string(interp, "str.trim_start", "text", args[0], &text, &length))
        return EXEC_ERROR;

    while (start < length && isspace((unsigned char)text[start]))
        start++;

    *result = lib_string(interp, text + start, length - start);

    if (result->type != VAL_STRING) {
        interp_fail(interp, 0, 0, "out of memory");
        return EXEC_ERROR;
    }

    return EXEC_OK;
}

static ExecStatus native_trim_end(Interpreter *interp, Value *args, size_t argc, Value *result)
{
    const char *text;
    size_t length;
    size_t end;

    if (!lib_expect_arity(interp, "str.trim_end", argc, 1))
        return EXEC_ERROR;

    if (!lib_expect_string(interp, "str.trim_end", "text", args[0], &text, &length))
        return EXEC_ERROR;

    end = length;

    while (end > 0 && isspace((unsigned char)text[end - 1]))
        end--;

    *result = lib_string(interp, text, end);

    if (result->type != VAL_STRING) {
        interp_fail(interp, 0, 0, "out of memory");
        return EXEC_ERROR;
    }

    return EXEC_OK;
}

static ptrdiff_t find_bytes(const char *text, size_t length, const char *needle, size_t needle_length)
{
    size_t i;

    if (needle_length == 0)
        return 0;

    if (needle_length > length)
        return -1;

    for (i = 0; i + needle_length <= length; i++) {
        if (memcmp(text + i, needle, needle_length) == 0)
            return (ptrdiff_t)i;
    }

    return -1;
}

static ExecStatus native_find(Interpreter *interp, Value *args, size_t argc, Value *result)
{
    const char *text;
    const char *needle;
    size_t length;
    size_t needle_length;
    ptrdiff_t index;

    if (!lib_expect_arity(interp, "str.find", argc, 2))
        return EXEC_ERROR;

    if (!lib_expect_string(interp, "str.find", "text", args[0], &text, &length))
        return EXEC_ERROR;

    if (!lib_expect_string(interp, "str.find", "needle", args[1], &needle, &needle_length))
        return EXEC_ERROR;

    index = find_bytes(text, length, needle, needle_length);
    *result = value_int((long long)index);
    return EXEC_OK;
}

static ExecStatus native_contains(Interpreter *interp, Value *args, size_t argc, Value *result)
{
    const char *text;
    const char *needle;
    size_t length;
    size_t needle_length;

    if (!lib_expect_arity(interp, "str.contains", argc, 2))
        return EXEC_ERROR;

    if (!lib_expect_string(interp, "str.contains", "text", args[0], &text, &length))
        return EXEC_ERROR;

    if (!lib_expect_string(interp, "str.contains", "needle", args[1], &needle, &needle_length))
        return EXEC_ERROR;

    *result = value_bool(find_bytes(text, length, needle, needle_length) >= 0);
    return EXEC_OK;
}

static ExecStatus native_starts_with(Interpreter *interp, Value *args, size_t argc, Value *result)
{
    const char *text;
    const char *prefix;
    size_t length;
    size_t prefix_length;

    if (!lib_expect_arity(interp, "str.starts_with", argc, 2))
        return EXEC_ERROR;

    if (!lib_expect_string(interp, "str.starts_with", "text", args[0], &text, &length))
        return EXEC_ERROR;

    if (!lib_expect_string(interp, "str.starts_with", "prefix", args[1], &prefix, &prefix_length))
        return EXEC_ERROR;

    *result = value_bool(prefix_length <= length &&
                         memcmp(text, prefix, prefix_length) == 0);
    return EXEC_OK;
}

static ExecStatus native_ends_with(Interpreter *interp, Value *args, size_t argc, Value *result)
{
    const char *text;
    const char *suffix;
    size_t length;
    size_t suffix_length;

    if (!lib_expect_arity(interp, "str.ends_with", argc, 2))
        return EXEC_ERROR;

    if (!lib_expect_string(interp, "str.ends_with", "text", args[0], &text, &length))
        return EXEC_ERROR;

    if (!lib_expect_string(interp, "str.ends_with", "suffix", args[1], &suffix, &suffix_length))
        return EXEC_ERROR;

    *result = value_bool(suffix_length <= length &&
                         memcmp(text + length - suffix_length, suffix, suffix_length) == 0);
    return EXEC_OK;
}

static ExecStatus native_split(Interpreter *interp, Value *args, size_t argc, Value *result)
{
    const char *text;
    const char *separator;
    size_t length;
    size_t separator_length;
    size_t start = 0;
    size_t i = 0;
    Value list;

    if (!lib_expect_arity(interp, "str.split", argc, 2))
        return EXEC_ERROR;

    if (!lib_expect_string(interp, "str.split", "text", args[0], &text, &length))
        return EXEC_ERROR;

    if (!lib_expect_string(interp, "str.split", "separator", args[1], &separator,
                           &separator_length))
        return EXEC_ERROR;

    if (separator_length == 0) {
        interp_fail(interp, 0, 0, "str.split separator must not be empty");
        return EXEC_ERROR;
    }

    list = value_list(&interp->gc);
    if (list.type != VAL_LIST) {
        interp_fail(interp, 0, 0, "out of memory");
        return EXEC_ERROR;
    }

    while (i + separator_length <= length) {
        if (memcmp(text + i, separator, separator_length) == 0) {
            Value piece = lib_string(interp, text + start, i - start);

            if (piece.type != VAL_STRING) {
                interp_fail(interp, 0, 0, "out of memory");
                return EXEC_ERROR;
            }

            if (!value_list_push(list, piece)) {
                interp_fail(interp, 0, 0, "out of memory");
                return EXEC_ERROR;
            }

            i += separator_length;
            start = i;
            continue;
        }

        i++;
    }

    {
        Value piece = lib_string(interp, text + start, length - start);

        if (piece.type != VAL_STRING) {
            interp_fail(interp, 0, 0, "out of memory");
            return EXEC_ERROR;
        }

        if (!value_list_push(list, piece)) {
            interp_fail(interp, 0, 0, "out of memory");
            return EXEC_ERROR;
        }
    }

    *result = list;
    return EXEC_OK;
}

static ExecStatus native_join(Interpreter *interp, Value *args, size_t argc, Value *result)
{
    const char *separator;
    size_t separator_length;
    ObjList *list;
    size_t i;
    StrBuf buffer;

    if (!lib_expect_arity(interp, "str.join", argc, 2))
        return EXEC_ERROR;

    if (!lib_expect_list(interp, "str.join", "items", args[0], &list))
        return EXEC_ERROR;

    if (!lib_expect_string(interp, "str.join", "separator", args[1], &separator,
                           &separator_length))
        return EXEC_ERROR;

    strbuf_init(&buffer);

    for (i = 0; i < list->count; i++) {
        const char *text;
        size_t length;

        if (i > 0 && !strbuf_append(&buffer, separator, separator_length)) {
            strbuf_free(&buffer);
            interp_fail(interp, 0, 0, "out of memory");
            return EXEC_ERROR;
        }

        if (!lib_expect_string(interp, "str.join", "item", list->items[i], &text, &length)) {
            strbuf_free(&buffer);
            return EXEC_ERROR;
        }

        if (!strbuf_append(&buffer, text, length)) {
            strbuf_free(&buffer);
            interp_fail(interp, 0, 0, "out of memory");
            return EXEC_ERROR;
        }
    }

    return finish_string(interp, &buffer, result) ? EXEC_OK : EXEC_ERROR;
}

static ExecStatus native_replace(Interpreter *interp, Value *args, size_t argc, Value *result)
{
    const char *text;
    const char *from;
    const char *to;
    size_t length;
    size_t from_length;
    size_t to_length;
    size_t start = 0;
    size_t i = 0;
    StrBuf buffer;

    if (!lib_expect_arity(interp, "str.replace", argc, 3))
        return EXEC_ERROR;

    if (!lib_expect_string(interp, "str.replace", "text", args[0], &text, &length))
        return EXEC_ERROR;

    if (!lib_expect_string(interp, "str.replace", "from", args[1], &from, &from_length))
        return EXEC_ERROR;

    if (!lib_expect_string(interp, "str.replace", "to", args[2], &to, &to_length))
        return EXEC_ERROR;

    if (from_length == 0) {
        interp_fail(interp, 0, 0, "str.replace search string must not be empty");
        return EXEC_ERROR;
    }

    strbuf_init(&buffer);

    while (i + from_length <= length) {
        if (memcmp(text + i, from, from_length) == 0) {
            if (!strbuf_append(&buffer, text + start, i - start) ||
                !strbuf_append(&buffer, to, to_length)) {
                strbuf_free(&buffer);
                interp_fail(interp, 0, 0, "out of memory");
                return EXEC_ERROR;
            }

            i += from_length;
            start = i;
            continue;
        }

        i++;
    }

    if (!strbuf_append(&buffer, text + start, length - start)) {
        strbuf_free(&buffer);
        interp_fail(interp, 0, 0, "out of memory");
        return EXEC_ERROR;
    }

    return finish_string(interp, &buffer, result) ? EXEC_OK : EXEC_ERROR;
}

static ExecStatus native_repeat(Interpreter *interp, Value *args, size_t argc, Value *result)
{
    const char *text;
    size_t length;
    long long count;
    long long i;
    StrBuf buffer;

    if (!lib_expect_arity(interp, "str.repeat", argc, 2))
        return EXEC_ERROR;

    if (!lib_expect_string(interp, "str.repeat", "text", args[0], &text, &length))
        return EXEC_ERROR;

    if (!lib_expect_int(interp, "str.repeat", "count", args[1], &count))
        return EXEC_ERROR;

    if (count < 0) {
        interp_fail(interp, 0, 0, "str.repeat count must not be negative");
        return EXEC_ERROR;
    }

    strbuf_init(&buffer);

    for (i = 0; i < count; i++) {
        if (!strbuf_append(&buffer, text, length)) {
            strbuf_free(&buffer);
            interp_fail(interp, 0, 0, "out of memory");
            return EXEC_ERROR;
        }
    }

    return finish_string(interp, &buffer, result) ? EXEC_OK : EXEC_ERROR;
}

static ExecStatus native_substr(Interpreter *interp, Value *args, size_t argc, Value *result)
{
    const char *text;
    size_t length;
    long long start;
    long long count;

    if (!lib_expect_arity(interp, "str.substr", argc, 3))
        return EXEC_ERROR;

    if (!lib_expect_string(interp, "str.substr", "text", args[0], &text, &length))
        return EXEC_ERROR;

    if (!lib_expect_int(interp, "str.substr", "start", args[1], &start))
        return EXEC_ERROR;

    if (!lib_expect_int(interp, "str.substr", "length", args[2], &count))
        return EXEC_ERROR;

    if (start < 0 || count < 0 || (unsigned long long)start > length ||
        (unsigned long long)count > length - (size_t)start) {
        interp_fail(interp, 0, 0, "str.substr range is out of bounds");
        return EXEC_ERROR;
    }

    *result = lib_string(interp, text + start, (size_t)count);

    if (result->type != VAL_STRING) {
        interp_fail(interp, 0, 0, "out of memory");
        return EXEC_ERROR;
    }

    return EXEC_OK;
}

static ExecStatus native_slice(Interpreter *interp, Value *args, size_t argc, Value *result)
{
    const char *text;
    size_t length;
    long long start;
    long long end;

    if (!lib_expect_arity(interp, "str.slice", argc, 3))
        return EXEC_ERROR;

    if (!lib_expect_string(interp, "str.slice", "text", args[0], &text, &length))
        return EXEC_ERROR;

    if (!lib_expect_int(interp, "str.slice", "start", args[1], &start))
        return EXEC_ERROR;

    if (!lib_expect_int(interp, "str.slice", "end", args[2], &end))
        return EXEC_ERROR;

    if (start < 0 || end < start || (unsigned long long)end > length) {
        interp_fail(interp, 0, 0, "str.slice range is out of bounds");
        return EXEC_ERROR;
    }

    *result = lib_string(interp, text + start, (size_t)(end - start));

    if (result->type != VAL_STRING) {
        interp_fail(interp, 0, 0, "out of memory");
        return EXEC_ERROR;
    }

    return EXEC_OK;
}

static const LibFunction functions[] = {
    {"upper", native_upper},
    {"lower", native_lower},
    {"trim", native_trim},
    {"trim_start", native_trim_start},
    {"trim_end", native_trim_end},
    {"find", native_find},
    {"contains", native_contains},
    {"starts_with", native_starts_with},
    {"ends_with", native_ends_with},
    {"split", native_split},
    {"join", native_join},
    {"replace", native_replace},
    {"repeat", native_repeat},
    {"substr", native_substr},
    {"slice", native_slice},
};

ExecStatus lib_str_install(Interpreter *interp, Value *module)
{
    *module = lib_module(interp);

    if (module->type != VAL_MAP) {
        interp_fail(interp, 0, 0, "out of memory");
        return EXEC_ERROR;
    }

    if (!lib_define_functions(interp, *module, functions, sizeof functions / sizeof functions[0])) {
        interp_fail(interp, 0, 0, "out of memory");
        return EXEC_ERROR;
    }

    return EXEC_OK;
}