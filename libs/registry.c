// (c) Glape Contributors

#include "registry.h"

#include <string.h>

#include "lib_list.h"
#include "lib_map.h"
#include "lib_math.h"
#include "lib_str.h"

static const LibDef libraries[] = {
    {"str", lib_str_install},
    {"list", lib_list_install},
    {"map", lib_map_install},
    {"math", lib_math_install},
};

static const LibDef *find_library(const char *name)
{
    size_t i;

    for (i = 0; i < sizeof libraries / sizeof libraries[0]; i++) {
        if (strcmp(libraries[i].name, name) == 0)
            return &libraries[i];
    }

    return NULL;
}

ExecStatus libs_load(Interpreter *interp, const char *name, int line, int column)
{
    const LibDef *definition;
    Value existing;
    Value module;
    ExecStatus status;

    if (name == NULL) {
        interp_fail(interp, line, column, "invalid module name");
        return EXEC_ERROR;
    }

    if (env_get(interp->env, name, &existing))
        return EXEC_OK;

    definition = find_library(name);
    if (definition == NULL) {
        interp_fail(interp, line, column, "unknown module '%s'", name);
        return EXEC_ERROR;
    }

    module = value_nil();
    status = definition->install(interp, &module);

    if (status != EXEC_OK)
        return status;

    if (module.type != VAL_MAP) {
        interp_fail(interp, line, column, "module '%s' did not install a map", name);
        return EXEC_ERROR;
    }

    env_define(interp->env, name, module);
    return EXEC_OK;
}