// (c) Glape Contributors

#include "env.h"

#include <string.h>

Env *env_new(GcHeap *heap, Env *parent)
{
    Env *env = (Env *)gc_alloc(heap, sizeof *env, GC_ENV);

    if (env == NULL)
        return NULL;

    env->parent = parent;
    env->bindings = NULL;
    return env;
}

static Binding *find_binding(Env *env, const char *name)
{
    Binding *binding;

    for (binding = env->bindings; binding != NULL; binding = binding->next) {
        if (strcmp(binding->name, name) == 0)
            return binding;
    }

    return NULL;
}

void env_define(Env *env, const char *name, Value value)
{
    Binding *binding = find_binding(env, name);

    if (binding != NULL) {
        binding->value = value;
        return;
    }

    binding = (Binding *)gc_alloc(env->header.heap, sizeof *binding, GC_BINDING);
    if (binding == NULL)
        return;

    binding->name = name;
    binding->value = value;
    binding->next = env->bindings;
    env->bindings = binding;
}

int env_get(Env *env, const char *name, Value *out)
{
    for (; env != NULL; env = env->parent) {
        Binding *binding = find_binding(env, name);

        if (binding != NULL) {
            *out = binding->value;
            return 1;
        }
    }

    return 0;
}

int env_assign(Env *env, const char *name, Value value)
{
    for (; env != NULL; env = env->parent) {
        Binding *binding = find_binding(env, name);

        if (binding != NULL) {
            binding->value = value;
            return 1;
        }
    }

    return 0;
}
