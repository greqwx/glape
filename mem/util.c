// (c) Glape Contributors

#include "util.h"

#include <stdlib.h>
#include <string.h>

#define ARENA_ALIGN 8
#define ARENA_BLOCK_SIZE 4096

struct ArenaBlock {
    ArenaBlock *next;
    size_t used;
    size_t size;
    unsigned char *data;
};

void arena_init(Arena *arena)
{
    arena->head = NULL;
}

void *arena_alloc(Arena *arena, size_t size)
{
    ArenaBlock *block;
    size_t aligned = (size + ARENA_ALIGN - 1) & ~(size_t)(ARENA_ALIGN - 1);
    void *result;

    if (aligned == 0)
        aligned = ARENA_ALIGN;

    block = arena->head;

    if (block == NULL || block->used + aligned > block->size) {
        size_t block_size = aligned > ARENA_BLOCK_SIZE ? aligned : ARENA_BLOCK_SIZE;

        block = (ArenaBlock *)malloc(sizeof *block);
        if (block == NULL)
            return NULL;

        block->data = (unsigned char *)malloc(block_size);
        if (block->data == NULL) {
            free(block);
            return NULL;
        }

        block->used = 0;
        block->size = block_size;
        block->next = arena->head;
        arena->head = block;
    }

    result = block->data + block->used;
    block->used += aligned;
    return result;
}

void arena_free(Arena *arena)
{
    ArenaBlock *block = arena->head;

    while (block != NULL) {
        ArenaBlock *next = block->next;

        free(block->data);
        free(block);
        block = next;
    }

    arena->head = NULL;
}

void strbuf_init(StrBuf *buffer)
{
    buffer->data = NULL;
    buffer->length = 0;
    buffer->capacity = 0;
}

static int strbuf_reserve(StrBuf *buffer, size_t extra)
{
    size_t needed = buffer->length + extra + 1;
    size_t capacity;
    char *data;

    if (needed <= buffer->capacity)
        return 1;

    capacity = buffer->capacity == 0 ? 64 : buffer->capacity;

    while (capacity < needed) {
        if (capacity > ((size_t)-1) / 2)
            return 0;
        capacity *= 2;
    }

    data = (char *)realloc(buffer->data, capacity);
    if (data == NULL)
        return 0;

    buffer->data = data;
    buffer->capacity = capacity;
    return 1;
}

int strbuf_append(StrBuf *buffer, const char *text, size_t length)
{
    if (!strbuf_reserve(buffer, length))
        return 0;

    if (length > 0)
        memcpy(buffer->data + buffer->length, text, length);

    buffer->length += length;
    buffer->data[buffer->length] = '\0';
    return 1;
}

int strbuf_append_cstr(StrBuf *buffer, const char *text)
{
    return strbuf_append(buffer, text, strlen(text));
}

int strbuf_append_char(StrBuf *buffer, char c)
{
    return strbuf_append(buffer, &c, 1);
}

char *strbuf_take(StrBuf *buffer)
{
    char *data = buffer->data;

    buffer->data = NULL;
    buffer->length = 0;
    buffer->capacity = 0;
    return data;
}

void strbuf_free(StrBuf *buffer)
{
    free(buffer->data);
    strbuf_init(buffer);
}
