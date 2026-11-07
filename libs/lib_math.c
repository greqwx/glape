// (c) Glape Contributors

#include "lib_math.h"

#include <limits.h>
#include <math.h>

#include "lib_api.h"

typedef double (*UnaryMathFn)(double);

static ExecStatus simple_unary(Interpreter *interp, const char *name, Value *args, size_t argc,
                               Value *result, UnaryMathFn fn)
{
    double value;

    if (!lib_expect_arity(interp, name, argc, 1))
        return EXEC_ERROR;

    if (!lib_expect_number(interp, name, "value", args[0], &value))
        return EXEC_ERROR;

    *result = value_float(fn(value));
    return EXEC_OK;
}

static ExecStatus native_abs(Interpreter *interp, Value *args, size_t argc, Value *result)
{
    if (!lib_expect_arity(interp, "math.abs", argc, 1))
        return EXEC_ERROR;

    if (args[0].type == VAL_INT) {
        if (args[0].as.integer == LLONG_MIN) {
            interp_fail(interp, 0, 0, "math.abs overflows int");
            return EXEC_ERROR;
        }

        *result = value_int(args[0].as.integer < 0 ? -args[0].as.integer : args[0].as.integer);
        return EXEC_OK;
    }

    if (args[0].type == VAL_FLOAT) {
        *result = value_float(fabs(args[0].as.floating));
        return EXEC_OK;
    }

    interp_fail(interp, 0, 0, "math.abs expects number for 'value', got %s",
                value_type_name(args[0]));
    return EXEC_ERROR;
}

static ExecStatus native_sqrt(Interpreter *interp, Value *args, size_t argc, Value *result)
{
    double value;

    if (!lib_expect_arity(interp, "math.sqrt", argc, 1))
        return EXEC_ERROR;

    if (!lib_expect_number(interp, "math.sqrt", "value", args[0], &value))
        return EXEC_ERROR;

    if (value < 0.0) {
        interp_fail(interp, 0, 0, "math.sqrt domain error");
        return EXEC_ERROR;
    }

    *result = value_float(sqrt(value));
    return EXEC_OK;
}

static ExecStatus native_log(Interpreter *interp, Value *args, size_t argc, Value *result)
{
    double value;

    if (!lib_expect_arity(interp, "math.log", argc, 1))
        return EXEC_ERROR;

    if (!lib_expect_number(interp, "math.log", "value", args[0], &value))
        return EXEC_ERROR;

    if (value <= 0.0) {
        interp_fail(interp, 0, 0, "math.log domain error");
        return EXEC_ERROR;
    }

    *result = value_float(log(value));
    return EXEC_OK;
}

static ExecStatus native_log10(Interpreter *interp, Value *args, size_t argc, Value *result)
{
    double value;

    if (!lib_expect_arity(interp, "math.log10", argc, 1))
        return EXEC_ERROR;

    if (!lib_expect_number(interp, "math.log10", "value", args[0], &value))
        return EXEC_ERROR;

    if (value <= 0.0) {
        interp_fail(interp, 0, 0, "math.log10 domain error");
        return EXEC_ERROR;
    }

    *result = value_float(log10(value));
    return EXEC_OK;
}

static ExecStatus native_asin(Interpreter *interp, Value *args, size_t argc, Value *result)
{
    double value;

    if (!lib_expect_arity(interp, "math.asin", argc, 1))
        return EXEC_ERROR;

    if (!lib_expect_number(interp, "math.asin", "value", args[0], &value))
        return EXEC_ERROR;

    if (value < -1.0 || value > 1.0) {
        interp_fail(interp, 0, 0, "math.asin domain error");
        return EXEC_ERROR;
    }

    *result = value_float(asin(value));
    return EXEC_OK;
}

static ExecStatus native_acos(Interpreter *interp, Value *args, size_t argc, Value *result)
{
    double value;

    if (!lib_expect_arity(interp, "math.acos", argc, 1))
        return EXEC_ERROR;

    if (!lib_expect_number(interp, "math.acos", "value", args[0], &value))
        return EXEC_ERROR;

    if (value < -1.0 || value > 1.0) {
        interp_fail(interp, 0, 0, "math.acos domain error");
        return EXEC_ERROR;
    }

    *result = value_float(acos(value));
    return EXEC_OK;
}

static ExecStatus native_floor(Interpreter *interp, Value *args, size_t argc, Value *result)
{
    return simple_unary(interp, "math.floor", args, argc, result, floor);
}

static ExecStatus native_ceil(Interpreter *interp, Value *args, size_t argc, Value *result)
{
    return simple_unary(interp, "math.ceil", args, argc, result, ceil);
}

static ExecStatus native_round(Interpreter *interp, Value *args, size_t argc, Value *result)
{
    return simple_unary(interp, "math.round", args, argc, result, round);
}

static ExecStatus native_exp(Interpreter *interp, Value *args, size_t argc, Value *result)
{
    return simple_unary(interp, "math.exp", args, argc, result, exp);
}

static ExecStatus native_sin(Interpreter *interp, Value *args, size_t argc, Value *result)
{
    return simple_unary(interp, "math.sin", args, argc, result, sin);
}

static ExecStatus native_cos(Interpreter *interp, Value *args, size_t argc, Value *result)
{
    return simple_unary(interp, "math.cos", args, argc, result, cos);
}

static ExecStatus native_tan(Interpreter *interp, Value *args, size_t argc, Value *result)
{
    return simple_unary(interp, "math.tan", args, argc, result, tan);
}

static ExecStatus native_atan(Interpreter *interp, Value *args, size_t argc, Value *result)
{
    return simple_unary(interp, "math.atan", args, argc, result, atan);
}

static ExecStatus native_pow(Interpreter *interp, Value *args, size_t argc, Value *result)
{
    double base;
    double exponent;

    if (!lib_expect_arity(interp, "math.pow", argc, 2))
        return EXEC_ERROR;

    if (!lib_expect_number(interp, "math.pow", "base", args[0], &base))
        return EXEC_ERROR;

    if (!lib_expect_number(interp, "math.pow", "exponent", args[1], &exponent))
        return EXEC_ERROR;

    *result = value_float(pow(base, exponent));
    return EXEC_OK;
}

static ExecStatus native_atan2(Interpreter *interp, Value *args, size_t argc, Value *result)
{
    double y;
    double x;

    if (!lib_expect_arity(interp, "math.atan2", argc, 2))
        return EXEC_ERROR;

    if (!lib_expect_number(interp, "math.atan2", "y", args[0], &y))
        return EXEC_ERROR;

    if (!lib_expect_number(interp, "math.atan2", "x", args[1], &x))
        return EXEC_ERROR;

    *result = value_float(atan2(y, x));
    return EXEC_OK;
}

static ExecStatus min_max(Interpreter *interp, Value *args, size_t argc, Value *result, int maximum)
{
    if (!lib_expect_arity(interp, maximum ? "math.max" : "math.min", argc, 2))
        return EXEC_ERROR;

    if (args[0].type == VAL_INT && args[1].type == VAL_INT) {
        long long left = args[0].as.integer;
        long long right = args[1].as.integer;

        if (maximum)
            *result = value_int(left > right ? left : right);
        else
            *result = value_int(left < right ? left : right);
        return EXEC_OK;
    }

    {
        double left;
        double right;

        if (!lib_expect_number(interp, maximum ? "math.max" : "math.min", "value", args[0], &left))
            return EXEC_ERROR;

        if (!lib_expect_number(interp, maximum ? "math.max" : "math.min", "value", args[1], &right))
            return EXEC_ERROR;

        if (maximum)
            *result = value_float(left > right ? left : right);
        else
            *result = value_float(left < right ? left : right);
    }

    return EXEC_OK;
}

static ExecStatus native_min(Interpreter *interp, Value *args, size_t argc, Value *result)
{
    return min_max(interp, args, argc, result, 0);
}

static ExecStatus native_max(Interpreter *interp, Value *args, size_t argc, Value *result)
{
    return min_max(interp, args, argc, result, 1);
}

static ExecStatus native_clamp(Interpreter *interp, Value *args, size_t argc, Value *result)
{
    if (!lib_expect_arity(interp, "math.clamp", argc, 3))
        return EXEC_ERROR;

    if (args[0].type == VAL_INT && args[1].type == VAL_INT && args[2].type == VAL_INT) {
        long long value = args[0].as.integer;
        long long low = args[1].as.integer;
        long long high = args[2].as.integer;

        if (low > high) {
            interp_fail(interp, 0, 0, "math.clamp lower bound is greater than upper bound");
            return EXEC_ERROR;
        }

        if (value < low)
            *result = value_int(low);
        else if (value > high)
            *result = value_int(high);
        else
            *result = value_int(value);
        return EXEC_OK;
    }

    {
        double value;
        double low;
        double high;

        if (!lib_expect_number(interp, "math.clamp", "value", args[0], &value))
            return EXEC_ERROR;

        if (!lib_expect_number(interp, "math.clamp", "low", args[1], &low))
            return EXEC_ERROR;

        if (!lib_expect_number(interp, "math.clamp", "high", args[2], &high))
            return EXEC_ERROR;

        if (low > high) {
            interp_fail(interp, 0, 0, "math.clamp lower bound is greater than upper bound");
            return EXEC_ERROR;
        }

        *result = value_float(value < low ? low : (value > high ? high : value));
    }

    return EXEC_OK;
}

static const LibFunction functions[] = {
    {"abs", native_abs},
    {"sqrt", native_sqrt},
    {"log", native_log},
    {"log10", native_log10},
    {"asin", native_asin},
    {"acos", native_acos},
    {"floor", native_floor},
    {"ceil", native_ceil},
    {"round", native_round},
    {"exp", native_exp},
    {"sin", native_sin},
    {"cos", native_cos},
    {"tan", native_tan},
    {"atan", native_atan},
    {"pow", native_pow},
    {"atan2", native_atan2},
    {"min", native_min},
    {"max", native_max},
    {"clamp", native_clamp},
};

ExecStatus lib_math_install(Interpreter *interp, Value *module)
{
    LibConstant constants[2];

    constants[0].name = "pi";
    constants[0].value = value_float(3.14159265358979323846);
    constants[1].name = "e";
    constants[1].value = value_float(2.71828182845904523536);

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