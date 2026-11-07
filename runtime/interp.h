// (c) Glape Contributors

#ifndef GLAPE_INTERP_H
#define GLAPE_INTERP_H

#include <stddef.h>
#include <stdio.h>

#include "mem/gc.h"
#include "mem/value.h"
#include "runtime/env.h"
#include "runtime/error.h"

#define INTERP_MAX_FRAMES 512

struct Interpreter {
    Arena *arena;
    GcHeap gc;
    Env *globals;
    Env *env;
    RuntimeError error;
    FILE *out;
    const char *file;
    Value return_value;
    int frame_count;
    CallFrame frames[INTERP_MAX_FRAMES];
};

void interp_init(Interpreter *interp, Arena *arena, FILE *out, const char *file);
void interp_free(Interpreter *interp);
Native *interp_make_native(Interpreter *interp, const char *name, NativeFn fn);
void interp_register_native(Interpreter *interp, const char *name, NativeFn fn);
ExecStatus interp_run(Interpreter *interp, AstNode *program);

void interp_raise(Interpreter *interp, Value value);
void interp_fail(Interpreter *interp, int line, int column, const char *format, ...);

#endif
