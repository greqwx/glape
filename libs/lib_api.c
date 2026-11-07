// (c) Glape Contributors

#include "lib_api.h"

#include <string.h>

Value lib_module(Interpreter *interp)
{
    return value_map(&interp->gc);
}

Value lib_string(Interpreter *interp, const char *chars, size_t length)
{
    return value_string(&interp->gc, chars, length);
}

Value lib_string_cstr(Interpreter *interp, const char *chars)
{
    return value_string(&interp->gc, chars, strlen(chars));
}

int lib_define(Interpreter *interp, Value module, const char *name, NativeFn fn)
{
    Native *native;
    Value key;
    Value value;

    if (module.type != VAL_MAP)
        return 0;

    native = interp_make_native(interp, name, fn);
    if (native == NULL)
        return 0;

    key = value_string(&interp->gc, name, strlen(name));
    if (key.type != VAL_STRING)
        return 0;

    value = value_native(native);
    return value_map_set(module, key.as.string, value);
}

int lib_define_functions(Interpreter *interp, Value module, const LibFunction *functions,
                         size_t count)
{
    size_t i;

    for (i = 0; i < count; i++) {
        if (!lib_define(interp, module, functions[i].name, functions[i].fn))
            return 0;
    }

    return 1;
}

int lib_define_value(Interpreter *interp, Value module, const char *name, Value value)
{
    Value key;

    if (module.type != VAL_MAP)
        return 0;

    key = value_string(&interp->gc, name, strlen(name));
    if (key.type != VAL_STRING)
        return 0;

    return value_map_set(module, key.as.string, value);
}

int lib_define_values(Interpreter *interp, Value module, const LibConstant *constants,
                      size_t count)
{
    size_t i;

    for (i = 0; i < count; i++) {
        if (!lib_define_value(interp, module, constants[i].name, constants[i].value))
            return 0;
    }

    return 1;
}

int lib_expect_arity(Interpreter *interp, const char *function, size_t argc, size_t expected)
{
    if (argc != expected) {
        interp_fail(interp, 0, 0, "%s expects %d argument(s), got %d", function, (int)expected,
                    (int)argc);
        return 0;
    }

    return 1;
}

int lib_expect_arity_range(Interpreter *interp, const char *function, size_t argc, size_t minimum,
                           size_t maximum)
{
    if (minimum == maximum)
        return lib_expect_arity(interp, function, argc, minimum);

    if (argc < minimum || argc > maximum) {
        interp_fail(interp, 0, 0, "%s expects between %d and %d arguments, got %d", function,
                    (int)minimum, (int)maximum, (int)argc);
        return 0;
    }

    return 1;
}

int lib_expect_string(Interpreter *interp, const char *function, const char *argument, Value value,
                      const char **chars, size_t *length)
{
    if (value.type != VAL_STRING) {
        interp_fail(interp, 0, 0, "%s expects string for '%s', got %s", function, argument,
                    value_type_name(value));
        return 0;
    }

    *chars = value.as.string->chars;
    *length = value.as.string->length;
    return 1;
}

int lib_expect_int(Interpreter *interp, const char *function, const char *argument, Value value,
                   long long *out)
{
    if (value.type != VAL_INT) {
        interp_fail(interp, 0, 0, "%s expects int for '%s', got %s", function, argument,
                    value_type_name(value));
        return 0;
    }

    *out = value.as.integer;
    return 1;
}

int lib_expect_number(Interpreter *interp, const char *function, const char *argument, Value value,
                      double *out)
{
    if (value.type == VAL_INT) {
        *out = (double)value.as.integer;
        return 1;
    }

    if (value.type == VAL_FLOAT) {
        *out = value.as.floating;
        return 1;
    }

    interp_fail(interp, 0, 0, "%s expects number for '%s', got %s", function, argument,
                value_type_name(value));
    return 0;
}

int lib_expect_list(Interpreter *interp, const char *function, const char *argument, Value value,
                    ObjList **out)
{
    if (value.type != VAL_LIST) {
        interp_fail(interp, 0, 0, "%s expects list for '%s', got %s", function, argument,
                    value_type_name(value));
        return 0;
    }

    *out = value.as.list;
    return 1;
}

int lib_expect_map(Interpreter *interp, const char *function, const char *argument, Value value,
                   ObjMap **out)
{
    if (value.type != VAL_MAP) {
        interp_fail(interp, 0, 0, "%s expects map for '%s', got %s", function, argument,
                    value_type_name(value));
        return 0;
    }

    *out = value.as.map;
    return 1;
}