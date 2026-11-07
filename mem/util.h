// (c) Glape Contributors

#ifndef GLAPE_MEM_UTIL_H
#define GLAPE_MEM_UTIL_H

#include <stddef.h>

typedef struct ArenaBlock ArenaBlock;

typedef struct {
    ArenaBlock *head;
} Arena;

void arena_init(Arena *arena);
void *arena_alloc(Arena *arena, size_t size);
void arena_free(Arena *arena);

typedef struct {
    char *data;
    size_t length;
    size_t capacity;
} StrBuf;

void strbuf_init(StrBuf *buffer);
int strbuf_append(StrBuf *buffer, const char *text, size_t length);
int strbuf_append_cstr(StrBuf *buffer, const char *text);
int strbuf_append_char(StrBuf *buffer, char c);
char *strbuf_take(StrBuf *buffer);
void strbuf_free(StrBuf *buffer);

#endif
