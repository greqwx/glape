// (c) Glape Contributors

#include "lib_os.h"

#include "lib_api.h"
#include "sys.h"

typedef int (*SysTextFn)(char *out, size_t size);

static ExecStatus text_query(Interpreter *interp, const char *name, SysTextFn query, size_t argc,
                             Value *result)
{
    char buffer[SYS_TEXT_MAX];

    if (!lib_expect_arity(interp, name, argc, 0))
        return EXEC_ERROR;

    if (!query(buffer, sizeof buffer)) {
        interp_fail(interp, 0, 0, "%s failed", name);
        return EXEC_ERROR;
    }

    *result = lib_string_cstr(interp, buffer);

    if (result->type != VAL_STRING) {
        interp_fail(interp, 0, 0, "out of memory");
        return EXEC_ERROR;
    }

    return EXEC_OK;
}

static ExecStatus native_hostname(Interpreter *interp, Value *args, size_t argc, Value *result)
{
    (void)args;
    return text_query(interp, "os.hostname", sys_hostname, argc, result);
}

static ExecStatus native_username(Interpreter *interp, Value *args, size_t argc, Value *result)
{
    (void)args;
    return text_query(interp, "os.username", sys_username, argc, result);
}

static ExecStatus native_home_dir(Interpreter *interp, Value *args, size_t argc, Value *result)
{
    (void)args;
    return text_query(interp, "os.home_dir", sys_home_dir, argc, result);
}

static ExecStatus native_temp_dir(Interpreter *interp, Value *args, size_t argc, Value *result)
{
    (void)args;
    return text_query(interp, "os.temp_dir", sys_temp_dir, argc, result);
}

static ExecStatus native_cpu_count(Interpreter *interp, Value *args, size_t argc, Value *result)
{
    (void)args;

    if (!lib_expect_arity(interp, "os.cpu_count", argc, 0))
        return EXEC_ERROR;

    *result = value_int(sys_cpu_count());
    return EXEC_OK;
}

static const LibFunction functions[] = {
    {"hostname", native_hostname},
    {"username", native_username},
    {"home_dir", native_home_dir},
    {"temp_dir", native_temp_dir},
    {"cpu_count", native_cpu_count},
};

ExecStatus lib_os_install(Interpreter *interp, Value *module)
{
    LibConstant constants[4];
    size_t i;

    constants[0].name = "name";
    constants[0].value = lib_string_cstr(interp, sys_os_name());
    constants[1].name = "arch";
    constants[1].value = lib_string_cstr(interp, sys_arch_name());
    constants[2].name = "sep";
    constants[2].value = lib_string_cstr(interp, SYS_PATH_SEP);
    constants[3].name = "eol";
    constants[3].value = lib_string_cstr(interp, SYS_EOL);

    for (i = 0; i < sizeof constants / sizeof constants[0]; i++) {
        if (constants[i].value.type != VAL_STRING) {
            interp_fail(interp, 0, 0, "out of memory");
            return EXEC_ERROR;
        }
    }

    *module = lib_module(interp);

    if (module->type != VAL_MAP) {
        interp_fail(interp, 0, 0, "out of memory");
        return EXEC_ERROR;
    }

    if (!lib_define_functions(interp, *module, functions, sizeof functions / sizeof functions[0])) {
        interp_fail(interp, 0, 0, "out of memory");
        return EXEC_ERROR;
    }

    if (!lib_define_values(interp, *module, constants, sizeof constants / sizeof constants[0])) {
        interp_fail(interp, 0, 0, "out of memory");
        return EXEC_ERROR;
    }

    return EXEC_OK;
}
