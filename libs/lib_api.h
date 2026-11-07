// (c) Glape Contributors

#ifndef GLAPE_LIB_API_H
#define GLAPE_LIB_API_H

#include <stddef.h>

#include "runtime/interp.h"

typedef struct {
    const char *name;
    NativeFn fn;
} LibFunction;

typedef struct {
    const char *name;
    Value value;
} LibConstant;

Value lib_module(Interpreter *interp);
int lib_define(Interpreter *interp, Value module, const char *name, NativeFn fn);
int lib_define_functions(Interpreter *interp, Value module, const LibFunction *functions,
                         size_t count);
int lib_define_value(Interpreter *interp, Value module, const char *name, Value value);
int lib_define_values(Interpreter *interp, Value module, const LibConstant *constants,
                      size_t count);

int lib_expect_arity(Interpreter *interp, const char *function, size_t argc, size_t expected);
int lib_expect_arity_range(Interpreter *interp, const char *function, size_t argc, size_t minimum,
                           size_t maximum);
int lib_expect_string(Interpreter *interp, const char *function, const char *argument, Value value,
                      const char **chars, size_t *length);
int lib_expect_int(Interpreter *interp, const char *function, const char *argument, Value value,
                   long long *out);
int lib_expect_number(Interpreter *interp, const char *function, const char *argument, Value value,
                      double *out);
int lib_expect_list(Interpreter *interp, const char *function, const char *argument, Value value,
                    ObjList **out);
int lib_expect_map(Interpreter *interp, const char *function, const char *argument, Value value,
                   ObjMap **out);

Value lib_string(Interpreter *interp, const char *chars, size_t length);
Value lib_string_cstr(Interpreter *interp, const char *chars);

#endif