// (c) Glape Contributors

#ifndef GLAPE_ENV_H
#define GLAPE_ENV_H

#include "mem/gc.h"
#include "mem/value.h"

typedef struct Binding Binding;

typedef struct Env {
    GcObject header;
    struct Env *parent;
    Binding *bindings;
} Env;

struct Binding {
    GcObject header;
    const char *name;
    Value value;
    Binding *next;
};

Env *env_new(GcHeap *heap, Env *parent);
void env_define(Env *env, const char *name, Value value);
int env_get(Env *env, const char *name, Value *out);
int env_assign(Env *env, const char *name, Value value);

#endif
