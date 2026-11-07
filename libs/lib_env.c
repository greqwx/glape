// (c) Glape Contributors

#include "lib_env.h"

#include <stdlib.h>
#include <string.h>

#include "lib_api.h"
#include "sys.h"

static int expect_name(Interpreter *interp, const char *function, Value value, const char **name)
{
    size_t length;

    if (!lib_expect_string(interp, function, "name", value, name, &length))
        return 0;

    if (length == 0 || strlen(*name) != length || strchr(*name, '=') != NULL) {
        interp_fail(interp, 0, 0, "%s got an invalid variable name", function);
        return 0;
    }

    return 1;
}

static ExecStatus native_get(Interpreter *interp, Value *args, size_t argc, Value *result)
{
    const char *name;
    char *text = NULL;
    int found;

    if (!lib_expect_arity_range(interp, "env.get", argc, 1, 2))
        return EXEC_ERROR;

    if (!expect_name(interp, "env.get", args[0], &name))
        return EXEC_ERROR;

    found = sys_env_get(name, &text);

    if (found < 0) {
        interp_fail(interp, 0, 0, "out of memory");
        return EXEC_ERROR;
    }

    if (found == 0) {
        *result = argc == 2 ? args[1] : value_nil();
        return EXEC_OK;
    }

    *result = lib_string_cstr(interp, text);
    free(text);

    if (result->type != VAL_STRING) {
        interp_fail(interp, 0, 0, "out of memory");
        return EXEC_ERROR;
    }

    return EXEC_OK;
}

static ExecStatus native_has(Interpreter *interp, Value *args, size_t argc, Value *result)
{
    const char *name;
    char *text = NULL;
    int found;

    if (!lib_expect_arity(interp, "env.has", argc, 1))
        return EXEC_ERROR;

    if (!expect_name(interp, "env.has", args[0], &name))
        return EXEC_ERROR;

    found = sys_env_get(name, &text);

    if (found < 0) {
        interp_fail(interp, 0, 0, "out of memory");
        return EXEC_ERROR;
    }

    free(text);
    *result = value_bool(found);
    return EXEC_OK;
}

static ExecStatus native_set(Interpreter *interp, Value *args, size_t argc, Value *result)
{
    const char *name;
    const char *text;
    size_t length;

    if (!lib_expect_arity(interp, "env.set", argc, 2))
        return EXEC_ERROR;

    if (!expect_name(interp, "env.set", args[0], &name))
        return EXEC_ERROR;

    if (!lib_expect_string(interp, "env.set", "value", args[1], &text, &length))
        return EXEC_ERROR;

    if (strlen(text) != length) {
        interp_fail(interp, 0, 0, "env.set value must not contain a NUL byte");
        return EXEC_ERROR;
    }

    if (!sys_env_set(name, text)) {
        interp_fail(interp, 0, 0, "env.set failed for '%s'", name);
        return EXEC_ERROR;
    }

    *result = value_nil();
    return EXEC_OK;
}

static ExecStatus native_remove(Interpreter *interp, Value *args, size_t argc, Value *result)
{
    const char *name;
    char *text = NULL;
    int found;

    if (!lib_expect_arity(interp, "env.remove", argc, 1))
        return EXEC_ERROR;

    if (!expect_name(interp, "env.remove", args[0], &name))
        return EXEC_ERROR;

    found = sys_env_get(name, &text);

    if (found < 0) {
        interp_fail(interp, 0, 0, "out of memory");
        return EXEC_ERROR;
    }

    free(text);

    if (found && !sys_env_remove(name)) {
        interp_fail(interp, 0, 0, "env.remove failed for '%s'", name);
        return EXEC_ERROR;
    }

    *result = value_bool(found);
    return EXEC_OK;
}

typedef struct {
    Interpreter *interp;
    Value map;
} AllContext;

static int add_variable(const char *name, size_t name_length, const char *value, void *context)
{
    AllContext *all = (AllContext *)context;
    Value key = lib_string(all->interp, name, name_length);
    Value text = lib_string_cstr(all->interp, value);

    if (key.type != VAL_STRING || text.type != VAL_STRING)
        return 0;

    return value_map_set(all->map, key.as.string, text);
}

static ExecStatus native_all(Interpreter *interp, Value *args, size_t argc, Value *result)
{
    AllContext all;

    (void)args;

    if (!lib_expect_arity(interp, "env.all", argc, 0))
        return EXEC_ERROR;

    all.interp = interp;
    all.map = value_map(&interp->gc);

    if (all.map.type != VAL_MAP || !sys_env_each(add_variable, &all)) {
        interp_fail(interp, 0, 0, "out of memory");
        return EXEC_ERROR;
    }

    *result = all.map;
    return EXEC_OK;
}

static const LibFunction functions[] = {
    {"get", native_get},
    {"has", native_has},
    {"set", native_set},
    {"remove", native_remove},
    {"all", native_all},
};

ExecStatus lib_env_install(Interpreter *interp, Value *module)
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
