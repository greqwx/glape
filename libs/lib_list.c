// (c) Glape Contributors

#include "lib_list.h"

#include "lib_api.h"

static ExecStatus native_push(Interpreter *interp, Value *args, size_t argc, Value *result)
{
    ObjList *list;

    if (!lib_expect_arity(interp, "list.push", argc, 2))
        return EXEC_ERROR;

    if (!lib_expect_list(interp, "list.push", "items", args[0], &list))
        return EXEC_ERROR;

    if (!value_list_push(args[0], args[1])) {
        interp_fail(interp, 0, 0, "out of memory");
        return EXEC_ERROR;
    }

    *result = args[0];
    return EXEC_OK;
}

static ExecStatus native_pop(Interpreter *interp, Value *args, size_t argc, Value *result)
{
    ObjList *list;

    if (!lib_expect_arity(interp, "list.pop", argc, 1))
        return EXEC_ERROR;

    if (!lib_expect_list(interp, "list.pop", "items", args[0], &list))
        return EXEC_ERROR;

    if (list->count == 0) {
        interp_fail(interp, 0, 0, "list.pop from empty list");
        return EXEC_ERROR;
    }

    if (!value_list_remove(args[0], list->count - 1, result)) {
        interp_fail(interp, 0, 0, "list.pop failed");
        return EXEC_ERROR;
    }

    return EXEC_OK;
}

static ExecStatus native_insert(Interpreter *interp, Value *args, size_t argc, Value *result)
{
    ObjList *list;
    long long index;

    if (!lib_expect_arity(interp, "list.insert", argc, 3))
        return EXEC_ERROR;

    if (!lib_expect_list(interp, "list.insert", "items", args[0], &list))
        return EXEC_ERROR;

    if (!lib_expect_int(interp, "list.insert", "index", args[1], &index))
        return EXEC_ERROR;

    if (index < 0 || (unsigned long long)index > list->count) {
        interp_fail(interp, 0, 0, "list.insert index %lld out of bounds for list of length %d",
                    index, (int)list->count);
        return EXEC_ERROR;
    }

    if (!value_list_insert(args[0], (size_t)index, args[2])) {
        interp_fail(interp, 0, 0, "out of memory");
        return EXEC_ERROR;
    }

    *result = args[0];
    return EXEC_OK;
}

static ExecStatus native_remove(Interpreter *interp, Value *args, size_t argc, Value *result)
{
    ObjList *list;
    long long index;

    if (!lib_expect_arity(interp, "list.remove", argc, 2))
        return EXEC_ERROR;

    if (!lib_expect_list(interp, "list.remove", "items", args[0], &list))
        return EXEC_ERROR;

    if (!lib_expect_int(interp, "list.remove", "index", args[1], &index))
        return EXEC_ERROR;

    if (index < 0 || (unsigned long long)index >= list->count) {
        interp_fail(interp, 0, 0, "list.remove index %lld out of bounds for list of length %d",
                    index, (int)list->count);
        return EXEC_ERROR;
    }

    if (!value_list_remove(args[0], (size_t)index, result)) {
        interp_fail(interp, 0, 0, "list.remove failed");
        return EXEC_ERROR;
    }

    return EXEC_OK;
}

static ExecStatus native_contains(Interpreter *interp, Value *args, size_t argc, Value *result)
{
    ObjList *list;
    size_t i;

    if (!lib_expect_arity(interp, "list.contains", argc, 2))
        return EXEC_ERROR;

    if (!lib_expect_list(interp, "list.contains", "items", args[0], &list))
        return EXEC_ERROR;

    for (i = 0; i < list->count; i++) {
        if (value_equals(list->items[i], args[1])) {
            *result = value_bool(1);
            return EXEC_OK;
        }
    }

    *result = value_bool(0);
    return EXEC_OK;
}

static ExecStatus native_index_of(Interpreter *interp, Value *args, size_t argc, Value *result)
{
    ObjList *list;
    size_t i;

    if (!lib_expect_arity(interp, "list.index_of", argc, 2))
        return EXEC_ERROR;

    if (!lib_expect_list(interp, "list.index_of", "items", args[0], &list))
        return EXEC_ERROR;

    for (i = 0; i < list->count; i++) {
        if (value_equals(list->items[i], args[1])) {
            *result = value_int((long long)i);
            return EXEC_OK;
        }
    }

    *result = value_int(-1);
    return EXEC_OK;
}

static ExecStatus native_reverse(Interpreter *interp, Value *args, size_t argc, Value *result)
{
    ObjList *list;
    size_t i;

    if (!lib_expect_arity(interp, "list.reverse", argc, 1))
        return EXEC_ERROR;

    if (!lib_expect_list(interp, "list.reverse", "items", args[0], &list))
        return EXEC_ERROR;

    for (i = 0; i < list->count / 2; i++) {
        Value tmp = list->items[i];

        list->items[i] = list->items[list->count - 1 - i];
        list->items[list->count - 1 - i] = tmp;
    }

    *result = args[0];
    return EXEC_OK;
}

static ExecStatus native_slice(Interpreter *interp, Value *args, size_t argc, Value *result)
{
    ObjList *list;
    long long start;
    long long end;
    Value slice;
    long long i;

    if (!lib_expect_arity(interp, "list.slice", argc, 3))
        return EXEC_ERROR;

    if (!lib_expect_list(interp, "list.slice", "items", args[0], &list))
        return EXEC_ERROR;

    if (!lib_expect_int(interp, "list.slice", "start", args[1], &start))
        return EXEC_ERROR;

    if (!lib_expect_int(interp, "list.slice", "end", args[2], &end))
        return EXEC_ERROR;

    if (start < 0 || end < start || (unsigned long long)end > list->count) {
        interp_fail(interp, 0, 0, "list.slice range is out of bounds");
        return EXEC_ERROR;
    }

    slice = value_list(&interp->gc);
    if (slice.type != VAL_LIST) {
        interp_fail(interp, 0, 0, "out of memory");
        return EXEC_ERROR;
    }

    for (i = start; i < end; i++) {
        if (!value_list_push(slice, list->items[i])) {
            interp_fail(interp, 0, 0, "out of memory");
            return EXEC_ERROR;
        }
    }

    *result = slice;
    return EXEC_OK;
}

static ExecStatus native_extend(Interpreter *interp, Value *args, size_t argc, Value *result)
{
    ObjList *target;
    ObjList *source;
    size_t count;
    size_t i;

    if (!lib_expect_arity(interp, "list.extend", argc, 2))
        return EXEC_ERROR;

    if (!lib_expect_list(interp, "list.extend", "items", args[0], &target))
        return EXEC_ERROR;

    if (!lib_expect_list(interp, "list.extend", "other", args[1], &source))
        return EXEC_ERROR;

    count = source->count;

    for (i = 0; i < count; i++) {
        if (!value_list_push(args[0], source->items[i])) {
            interp_fail(interp, 0, 0, "out of memory");
            return EXEC_ERROR;
        }
    }

    *result = args[0];
    return EXEC_OK;
}

static ExecStatus native_clear(Interpreter *interp, Value *args, size_t argc, Value *result)
{
    ObjList *list;

    if (!lib_expect_arity(interp, "list.clear", argc, 1))
        return EXEC_ERROR;

    if (!lib_expect_list(interp, "list.clear", "items", args[0], &list))
        return EXEC_ERROR;

    list->count = 0;
    *result = args[0];
    return EXEC_OK;
}

static const LibFunction functions[] = {
    {"push", native_push},
    {"pop", native_pop},
    {"insert", native_insert},
    {"remove", native_remove},
    {"contains", native_contains},
    {"index_of", native_index_of},
    {"reverse", native_reverse},
    {"slice", native_slice},
    {"extend", native_extend},
    {"clear", native_clear},
};

ExecStatus lib_list_install(Interpreter *interp, Value *module)
{
    *module = lib_module(interp);

    if (module->type != VAL_MAP) {
        interp_fail(interp, 0, 0, "out of memory");
        return EXEC_ERROR;
    }

    if (!lib_define_functions(interp, *module, functions,
                              sizeof functions / sizeof functions[0])) {
        interp_fail(interp, 0, 0, "out of memory");
        return EXEC_ERROR;
    }

    return EXEC_OK;
}