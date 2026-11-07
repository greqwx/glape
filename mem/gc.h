// (c) Glape Contributors

#ifndef GLAPE_GC_H
#define GLAPE_GC_H

#include <stddef.h>
#include <stdio.h>

typedef struct GcHeap GcHeap;

typedef enum {
    GC_ENV,
    GC_BINDING,
    GC_STRING,
    GC_LIST,
    GC_MAP,
    GC_FUNCTION
} GcKind;

typedef struct GcObject {
    struct GcObject *next;
    GcHeap *heap;
    size_t bytes;
    unsigned char kind;
    unsigned char marked;
} GcObject;

typedef enum {
    GC_ROOT_VALUE,
    GC_ROOT_ENV
} GcRootKind;

typedef struct GcRoot {
    GcRootKind kind;
    void *pointer;
} GcRoot;

struct GcHeap {
    GcObject *objects;
    GcRoot *roots;
    GcObject **gray;
    size_t object_count;
    size_t bytes_allocated;
    size_t next_collect;
    size_t collections;
    size_t root_count;
    size_t root_capacity;
    size_t gray_count;
    size_t gray_capacity;
    int running;
    int pending;
    int broken;
    int stress;
    int stats;
};

void gc_init(GcHeap *heap);
void gc_free_all(GcHeap *heap);

void *gc_alloc(GcHeap *heap, size_t size, GcKind kind);
void gc_resize(GcObject *object, size_t bytes);

/*
 * Roots are pointers to the C storage that holds a live value or environment.
 * The interpreter keeps its own state rooted for the whole run, temporaries
 * are rooted around the expression that may trigger a collection.
 */
void gc_push_root(GcHeap *heap, GcRootKind kind, void *pointer);
void gc_pop_roots(GcHeap *heap, size_t count);

void gc_safe_point(GcHeap *heap);
void gc_collect(GcHeap *heap);
void gc_report(GcHeap *heap, FILE *out);

#endif
