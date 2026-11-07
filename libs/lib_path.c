// (c) Glape Contributors

#include "lib_path.h"

#include <ctype.h>
#include <stdlib.h>

#include "lib_api.h"
#include "sys.h"

typedef struct {
    size_t start;
    size_t length;
} Span;

static int is_drive(const char *path, size_t length)
{
    return SYS_HAS_DRIVES && length >= 2 && isalpha((unsigned char)path[0]) && path[1] == ':';
}

static int is_parent(const char *path, Span span)
{
    return span.length == 2 && path[span.start] == '.' && path[span.start + 1] == '.';
}

/* bytes taken by the drive letter and the first separator, if there are any */
static size_t root_length(const char *path, size_t length)
{
    size_t i = is_drive(path, length) ? 2 : 0;

    if (i < length && SYS_IS_SEP(path[i]))
        i++;

    return i;
}

/* finds the last component, ignoring trailing separators */
static void last_component(const char *path, size_t length, size_t root, size_t *start,
                           size_t *end)
{
    size_t stop = length;
    size_t i;

    while (stop > root && SYS_IS_SEP(path[stop - 1]))
        stop--;

    i = stop;
    while (i > root && !SYS_IS_SEP(path[i - 1]))
        i--;

    *start = i;
    *end = stop;
}

static int is_absolute_path(const char *path, size_t length)
{
    if (length > 0 && SYS_IS_SEP(path[0]))
        return 1;

    return is_drive(path, length) && length >= 3 && SYS_IS_SEP(path[2]);
}

static int normalize_into(const char *path, size_t length, StrBuf *out)
{
    Span *parts = (Span *)malloc((length / 2 + 2) * sizeof *parts);
    size_t count = 0;
    size_t i = 0;
    size_t k;
    int rooted = 0;
    int ok = 1;

    if (parts == NULL)
        return 0;

    if (is_drive(path, length)) {
        ok = strbuf_append(out, path, 2);
        i = 2;
    }

    if (SYS_HAS_DRIVES && i == 0 && length >= 2 && SYS_IS_SEP(path[0]) && SYS_IS_SEP(path[1])) {
        /* a network path keeps its two leading separators */
        ok = ok && strbuf_append_cstr(out, SYS_PATH_SEP SYS_PATH_SEP);
        rooted = 1;
    } else if (i < length && SYS_IS_SEP(path[i])) {
        ok = ok && strbuf_append_cstr(out, SYS_PATH_SEP);
        rooted = 1;
    }

    while (i < length) {
        Span span;

        while (i < length && SYS_IS_SEP(path[i]))
            i++;

        span.start = i;

        while (i < length && !SYS_IS_SEP(path[i]))
            i++;

        span.length = i - span.start;

        if (span.length == 0 || (span.length == 1 && path[span.start] == '.'))
            continue;

        if (is_parent(path, span)) {
            if (count > 0 && !is_parent(path, parts[count - 1]))
                count--;
            else if (!rooted)
                parts[count++] = span;

            continue;
        }

        parts[count++] = span;
    }

    for (k = 0; ok && k < count; k++) {
        if (k > 0)
            ok = strbuf_append_cstr(out, SYS_PATH_SEP);

        ok = ok && strbuf_append(out, path + parts[k].start, parts[k].length);
    }

    if (ok && out->length == 0)
        ok = strbuf_append_cstr(out, ".");

    free(parts);
    return ok;
}

static ExecStatus string_result(Interpreter *interp, const char *chars, size_t length,
                                Value *result)
{
    *result = lib_string(interp, chars, length);

    if (result->type != VAL_STRING) {
        interp_fail(interp, 0, 0, "out of memory");
        return EXEC_ERROR;
    }

    return EXEC_OK;
}

static ExecStatus native_join(Interpreter *interp, Value *args, size_t argc, Value *result)
{
    StrBuf joined;
    ExecStatus status;
    StrBuf cleaned;
    size_t i;
    int ok = 1;

    if (argc == 0) {
        interp_fail(interp, 0, 0, "path.join expects at least 1 argument, got 0");
        return EXEC_ERROR;
    }

    strbuf_init(&joined);

    for (i = 0; i < argc; i++) {
        const char *part;
        size_t length;

        if (!lib_expect_string(interp, "path.join", "part", args[i], &part, &length)) {
            strbuf_free(&joined);
            return EXEC_ERROR;
        }

        if (length == 0)
            continue;

        if (joined.length > 0 && !SYS_IS_SEP(joined.data[joined.length - 1]))
            ok = ok && strbuf_append_cstr(&joined, SYS_PATH_SEP);

        ok = ok && strbuf_append(&joined, part, length);
    }

    strbuf_init(&cleaned);
    ok = ok && normalize_into(joined.data != NULL ? joined.data : "", joined.length, &cleaned);
    strbuf_free(&joined);

    if (!ok) {
        strbuf_free(&cleaned);
        interp_fail(interp, 0, 0, "out of memory");
        return EXEC_ERROR;
    }

    status = string_result(interp, cleaned.data, cleaned.length, result);
    strbuf_free(&cleaned);
    return status;
}

static ExecStatus native_normalize(Interpreter *interp, Value *args, size_t argc, Value *result)
{
    const char *path;
    size_t length;
    StrBuf cleaned;
    ExecStatus status;

    if (!lib_expect_arity(interp, "path.normalize", argc, 1))
        return EXEC_ERROR;

    if (!lib_expect_string(interp, "path.normalize", "path", args[0], &path, &length))
        return EXEC_ERROR;

    strbuf_init(&cleaned);

    if (!normalize_into(path, length, &cleaned)) {
        strbuf_free(&cleaned);
        interp_fail(interp, 0, 0, "out of memory");
        return EXEC_ERROR;
    }

    status = string_result(interp, cleaned.data, cleaned.length, result);
    strbuf_free(&cleaned);
    return status;
}

static ExecStatus native_basename(Interpreter *interp, Value *args, size_t argc, Value *result)
{
    const char *path;
    size_t length;
    size_t start;
    size_t end;

    if (!lib_expect_arity(interp, "path.basename", argc, 1))
        return EXEC_ERROR;

    if (!lib_expect_string(interp, "path.basename", "path", args[0], &path, &length))
        return EXEC_ERROR;

    last_component(path, length, root_length(path, length), &start, &end);
    return string_result(interp, path + start, end - start, result);
}

static ExecStatus native_dirname(Interpreter *interp, Value *args, size_t argc, Value *result)
{
    const char *path;
    size_t length;
    size_t root;
    size_t start;
    size_t end;

    if (!lib_expect_arity(interp, "path.dirname", argc, 1))
        return EXEC_ERROR;

    if (!lib_expect_string(interp, "path.dirname", "path", args[0], &path, &length))
        return EXEC_ERROR;

    root = root_length(path, length);
    last_component(path, length, root, &start, &end);

    if (start == root)
        return root > 0 ? string_result(interp, path, root, result)
                        : string_result(interp, ".", 1, result);

    while (start > root && SYS_IS_SEP(path[start - 1]))
        start--;

    return string_result(interp, path, start, result);
}

static ExecStatus native_ext(Interpreter *interp, Value *args, size_t argc, Value *result)
{
    const char *path;
    const char *base;
    size_t length;
    size_t start;
    size_t end;
    size_t size;
    size_t i;

    if (!lib_expect_arity(interp, "path.ext", argc, 1))
        return EXEC_ERROR;

    if (!lib_expect_string(interp, "path.ext", "path", args[0], &path, &length))
        return EXEC_ERROR;

    last_component(path, length, root_length(path, length), &start, &end);
    base = path + start;
    size = end - start;

    /* a leading dot belongs to the name, and "." and ".." have no extension */
    if (!(size == 1 && base[0] == '.') && !(size == 2 && base[0] == '.' && base[1] == '.')) {
        for (i = size; i > 1; i--) {
            if (base[i - 1] == '.')
                return string_result(interp, base + i - 1, size - i + 1, result);
        }
    }

    return string_result(interp, "", 0, result);
}

static ExecStatus native_is_absolute(Interpreter *interp, Value *args, size_t argc, Value *result)
{
    const char *path;
    size_t length;

    if (!lib_expect_arity(interp, "path.is_absolute", argc, 1))
        return EXEC_ERROR;

    if (!lib_expect_string(interp, "path.is_absolute", "path", args[0], &path, &length))
        return EXEC_ERROR;

    *result = value_bool(is_absolute_path(path, length));
    return EXEC_OK;
}

static const LibFunction functions[] = {
    {"join", native_join},
    {"normalize", native_normalize},
    {"basename", native_basename},
    {"dirname", native_dirname},
    {"ext", native_ext},
    {"is_absolute", native_is_absolute},
};

ExecStatus lib_path_install(Interpreter *interp, Value *module)
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
