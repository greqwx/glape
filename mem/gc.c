// (c) Glape Contributors

#include "gc.h"

#include <stdlib.h>
#include <string.h>

#include "mem/value.h"
#include "runtime/env.h"

/*
 * The heap collects when live memory crosses next_collect. After a collection
 * the threshold grows with the surviving set, so programs that keep little
 * alive collect rarely, while leaking programs collect constantly.
 */
#define GC_INITIAL_THRESHOLD ((size_t)256 * 1024)
#define GC_MIN_THRESHOLD ((size_t)256 * 1024)

static int env_flag(const char *name)
{
    const char *value = getenv(name);

    return value != NULL && value[0] != '\0' && strcmp(value, "0") != 0;
}

void gc_init(GcHeap *heap)
{
    memset(heap, 0, sizeof *heap);
    heap->next_collect = GC_INITIAL_THRESHOLD;
    heap->running = 1;
    heap->stress = env_flag("GLAPE_GC_STRESS");
    heap->stats = env_flag("GLAPE_GC_STATS");
}

static void gray_push(GcHeap *heap, GcObject *object)
{
    if (heap->gray_count == heap->gray_capacity) {
        size_t capacity = heap->gray_capacity == 0 ? 64 : heap->gray_capacity * 2;
        GcObject **gray = (GcObject **)realloc(heap->gray, capacity * sizeof *gray);

        if (gray == NULL) {
            heap->broken = 1;
            return;
        }

        heap->gray = gray;
        heap->gray_capacity = capacity;
    }

    heap->gray[heap->gray_count++] = object;
}

static void mark_object(GcHeap *heap, GcObject *object)
{
    if (object == NULL || object->marked)
        return;

    object->marked = 1;
    gray_push(heap, object);
}

static void mark_value(GcHeap *heap, Value value)
{
    switch (value.type) {
    case VAL_STRING:
        mark_object(heap, (GcObject *)value.as.string);
        break;
    case VAL_LIST:
        mark_object(heap, (GcObject *)value.as.list);
        break;
    case VAL_MAP:
        mark_object(heap, (GcObject *)value.as.map);
        break;
    case VAL_FUNCTION:
        mark_object(heap, (GcObject *)value.as.function);
        break;
    default:
        break;
    }
}

static void mark_env(GcHeap *heap, Env *env)
{
    if (env != NULL)
        mark_object(heap, &env->header);
}

/*
 * Pushes everything an object owns onto the gray stack. The stack is explicit
 * so that deeply nested lists and maps cannot overflow the C stack.
 */
static void trace_object(GcHeap *heap, GcObject *object)
{
    switch ((GcKind)object->kind) {
    case GC_STRING:
        break;

    case GC_LIST: {
        ObjList *list = (ObjList *)object;
        size_t i;

        for (i = 0; i < list->count; i++)
            mark_value(heap, list->items[i]);
        break;
    }

    case GC_MAP: {
        ObjMap *map = (ObjMap *)object;
        size_t i;

        for (i = 0; i < map->count; i++) {
            mark_object(heap, (GcObject *)map->entries[i].key);
            mark_value(heap, map->entries[i].value);
        }
        break;
    }

    case GC_FUNCTION: {
        Function *function = (Function *)object;

        mark_env(heap, function->closure);
        break;
    }

    case GC_ENV: {
        Env *env = (Env *)object;

        mark_env(heap, env->parent);
        mark_object(heap, (GcObject *)env->bindings);
        break;
    }

    case GC_BINDING: {
        Binding *binding = (Binding *)object;

        mark_value(heap, binding->value);
        mark_object(heap, (GcObject *)binding->next);
        break;
    }
    }
}

static void free_object(GcObject *object)
{
    switch ((GcKind)object->kind) {
    case GC_LIST:
        free(((ObjList *)object)->items);
        break;
    case GC_MAP:
        free(((ObjMap *)object)->entries);
        break;
    default:
        break;
    }

    free(object);
}

static void clear_marks(GcHeap *heap)
{
    GcObject *object;

    for (object = heap->objects; object != NULL; object = object->next)
        object->marked = 0;
}

static void sweep(GcHeap *heap)
{
    GcObject **link = &heap->objects;

    while (*link != NULL) {
        GcObject *object = *link;

        if (object->marked) {
            object->marked = 0;
            link = &object->next;
            continue;
        }

        *link = object->next;
        heap->bytes_allocated -= object->bytes;
        heap->object_count -= 1;
        free_object(object);
    }
}

static void update_threshold(GcHeap *heap)
{
    size_t grown = heap->bytes_allocated + heap->bytes_allocated / 2 + GC_MIN_THRESHOLD;

    if (grown < heap->bytes_allocated) {
        heap->next_collect = (size_t)-1;
        return;
    }

    heap->next_collect = grown;
}

void gc_collect(GcHeap *heap)
{
    size_t i;

    if (!heap->running || heap->broken)
        return;

    for (i = 0; i < heap->root_count; i++) {
        GcRoot *root = &heap->roots[i];

        if (root->kind == GC_ROOT_VALUE)
            mark_value(heap, *(Value *)root->pointer);
        else
            mark_env(heap, *(Env **)root->pointer);
    }

    while (heap->gray_count > 0 && !heap->broken) {
        GcObject *object = heap->gray[--heap->gray_count];

        trace_object(heap, object);
    }

    heap->gray_count = 0;
    heap->pending = 0;
    heap->collections += 1;

    if (heap->broken) {
        clear_marks(heap);
        return;
    }

    sweep(heap);
    update_threshold(heap);
}

void gc_safe_point(GcHeap *heap)
{
    if (heap->stress || heap->pending)
        gc_collect(heap);
}

void *gc_alloc(GcHeap *heap, size_t size, GcKind kind)
{
    GcObject *object = (GcObject *)malloc(size);

    if (object == NULL)
        return NULL;

    memset(object, 0, size);
    object->heap = heap;
    object->bytes = size;
    object->kind = (unsigned char)kind;
    object->next = heap->objects;
    heap->objects = object;
    heap->object_count += 1;
    heap->bytes_allocated += size;

    if (heap->running && heap->bytes_allocated > heap->next_collect)
        heap->pending = 1;

    return object;
}

void gc_resize(GcObject *object, size_t bytes)
{
    GcHeap *heap = object->heap;

    heap->bytes_allocated -= object->bytes;
    heap->bytes_allocated += bytes;
    object->bytes = bytes;

    if (heap->running && heap->bytes_allocated > heap->next_collect)
        heap->pending = 1;
}

void gc_push_root(GcHeap *heap, GcRootKind kind, void *pointer)
{
    if (heap->root_count == heap->root_capacity) {
        size_t capacity = heap->root_capacity == 0 ? 32 : heap->root_capacity * 2;
        GcRoot *roots = (GcRoot *)realloc(heap->roots, capacity * sizeof *roots);

        if (roots == NULL) {
            heap->broken = 1;
            return;
        }

        heap->roots = roots;
        heap->root_capacity = capacity;
    }

    heap->roots[heap->root_count].kind = kind;
    heap->roots[heap->root_count].pointer = pointer;
    heap->root_count += 1;
}

void gc_pop_roots(GcHeap *heap, size_t count)
{
    if (count > heap->root_count)
        count = heap->root_count;

    heap->root_count -= count;
}

void gc_report(GcHeap *heap, FILE *out)
{
    fprintf(out, "gc: collections=%lu live_objects=%lu live_bytes=%lu\n",
            (unsigned long)heap->collections, (unsigned long)heap->object_count,
            (unsigned long)heap->bytes_allocated);
}

void gc_free_all(GcHeap *heap)
{
    GcObject *object = heap->objects;

    while (object != NULL) {
        GcObject *next = object->next;

        free_object(object);
        object = next;
    }

    free(heap->roots);
    free(heap->gray);

    memset(heap, 0, sizeof *heap);
}
