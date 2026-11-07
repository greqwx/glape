// (c) Glape Contributors

#include "lib_map.h"

#include "lib_api.h"

static ExecStatus native_keys(Interpreter *interp, Value *args, size_t argc, Value *result)
{
    ObjMap *map;
    Value list;
    size_t i;

    if (!lib_expect_arity(interp, "map.keys", argc, 1))
        return EXEC_ERROR;

    if (!lib_expect_map(interp, "map.keys", "map", args[0], &map))
        return EXEC_ERROR;

    list = value_list(&interp->gc);
    if (list.type != VAL_LIST) {
        interp_fail(interp, 0, 0, "out of memory");
        return EXEC_ERROR;
    }

    for (i = 0; i < map->count; i++) {
        Value key = value_string(&interp->gc, map->entries[i].key->chars,
                                 map->entries[i].key->length);

        if (key.type != VAL_STRING) {
            interp_fail(interp, 0, 0, "out of memory");
            return EXEC_ERROR;
        }

        if (!value_list_push(list, key)) {
            interp_fail(interp, 0, 0, "out of memory");
            return EXEC_ERROR;
        }
    }

    *result = list;
    return EXEC_OK;
}

static ExecStatus native_values(Interpreter *interp, Value *args, size_t argc, Value *result)
{
    ObjMap *map;
    Value list;
    size_t i;

    if (!lib_expect_arity(interp, "map.values", argc, 1))
        return EXEC_ERROR;

    if (!lib_expect_map(interp, "map.values", "map", args[0], &map))
        return EXEC_ERROR;

    list = value_list(&interp->gc);
    if (list.type != VAL_LIST) {
        interp_fail(interp, 0, 0, "out of memory");
        return EXEC_ERROR;
    }

    for (i = 0; i < map->count; i++) {
        if (!value_list_push(list, map->entries[i].value)) {
            interp_fail(interp, 0, 0, "out of memory");
            return EXEC_ERROR;
        }
    }

    *result = list;
    return EXEC_OK;
}

static ExecStatus native_has(Interpreter *interp, Value *args, size_t argc, Value *result)
{
    ObjMap *map;
    const char *key;
    size_t key_length;
    Value value;

    if (!lib_expect_arity(interp, "map.has", argc, 2))
        return EXEC_ERROR;

    if (!lib_expect_map(interp, "map.has", "map", args[0], &map))
        return EXEC_ERROR;

    if (!lib_expect_string(interp, "map.has", "key", args[1], &key, &key_length))
        return EXEC_ERROR;

    *result = value_bool(value_map_get(args[0], key, key_length, &value));
    return EXEC_OK;
}

static ExecStatus native_get(Interpreter *interp, Value *args, size_t argc, Value *result)
{
    ObjMap *map;
    const char *key;
    size_t key_length;

    if (!lib_expect_arity_range(interp, "map.get", argc, 2, 3))
        return EXEC_ERROR;

    if (!lib_expect_map(interp, "map.get", "map", args[0], &map))
        return EXEC_ERROR;

    (void)map;

    if (!lib_expect_string(interp, "map.get", "key", args[1], &key, &key_length))
        return EXEC_ERROR;

    if (!value_map_get(args[0], key, key_length, result)) {
        if (argc == 3) {
            *result = args[2];
            return EXEC_OK;
        }

        interp_fail(interp, 0, 0, "map has no key '%.*s'", (int)key_length, key);
        return EXEC_ERROR;
    }

    return EXEC_OK;
}

static ExecStatus native_set(Interpreter *interp, Value *args, size_t argc, Value *result)
{
    ObjMap *map;
    const char *key;
    size_t key_length;
    Value key_value;

    if (!lib_expect_arity(interp, "map.set", argc, 3))
        return EXEC_ERROR;

    if (!lib_expect_map(interp, "map.set", "map", args[0], &map))
        return EXEC_ERROR;

    (void)map;

    if (!lib_expect_string(interp, "map.set", "key", args[1], &key, &key_length))
        return EXEC_ERROR;

    key_value = value_string(&interp->gc, key, key_length);
    if (key_value.type != VAL_STRING) {
        interp_fail(interp, 0, 0, "out of memory");
        return EXEC_ERROR;
    }

    if (!value_map_set(args[0], key_value.as.string, args[2])) {
        interp_fail(interp, 0, 0, "out of memory");
        return EXEC_ERROR;
    }

    *result = args[0];
    return EXEC_OK;
}

static ExecStatus native_remove(Interpreter *interp, Value *args, size_t argc, Value *result)
{
    ObjMap *map;
    const char *key;
    size_t key_length;

    if (!lib_expect_arity(interp, "map.remove", argc, 2))
        return EXEC_ERROR;

    if (!lib_expect_map(interp, "map.remove", "map", args[0], &map))
        return EXEC_ERROR;

    (void)map;

    if (!lib_expect_string(interp, "map.remove", "key", args[1], &key, &key_length))
        return EXEC_ERROR;

    if (!value_map_remove(args[0], key, key_length, result)) {
        interp_fail(interp, 0, 0, "map has no key '%.*s'", (int)key_length, key);
        return EXEC_ERROR;
    }

    return EXEC_OK;
}

static ExecStatus native_clear(Interpreter *interp, Value *args, size_t argc, Value *result)
{
    ObjMap *map;

    if (!lib_expect_arity(interp, "map.clear", argc, 1))
        return EXEC_ERROR;

    if (!lib_expect_map(interp, "map.clear", "map", args[0], &map))
        return EXEC_ERROR;

    map->count = 0;
    *result = args[0];
    return EXEC_OK;
}

static ExecStatus native_update(Interpreter *interp, Value *args, size_t argc, Value *result)
{
    ObjMap *target;
    ObjMap *source;
    size_t i;

    if (!lib_expect_arity(interp, "map.update", argc, 2))
        return EXEC_ERROR;

    if (!lib_expect_map(interp, "map.update", "map", args[0], &target))
        return EXEC_ERROR;

    if (!lib_expect_map(interp, "map.update", "other", args[1], &source))
        return EXEC_ERROR;

    (void)target;

    for (i = 0; i < source->count; i++) {
        if (!value_map_set(args[0], source->entries[i].key, source->entries[i].value)) {
            interp_fail(interp, 0, 0, "out of memory");
            return EXEC_ERROR;
        }
    }

    *result = args[0];
    return EXEC_OK;
}

static const LibFunction functions[] = {
    {"keys", native_keys},
    {"values", native_values},
    {"has", native_has},
    {"get", native_get},
    {"set", native_set},
    {"remove", native_remove},
    {"clear", native_clear},
    {"update", native_update},
};

ExecStatus lib_map_install(Interpreter *interp, Value *module)
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