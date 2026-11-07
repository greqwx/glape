// (c) Glape Contributors

#ifndef GLAPE_ERROR_H
#define GLAPE_ERROR_H

#include "mem/value.h"

#define RUNTIME_MAX_FRAMES 64

typedef struct {
    const char *function;
    const char *file;
    int line;
    int column;
} CallFrame;

typedef struct {
    int set;
    Value value;
    const char *file;
    int line;
    int column;
    char message[256];
    int frame_count;
    CallFrame frames[RUNTIME_MAX_FRAMES];
} RuntimeError;

void runtime_error_init(RuntimeError *error);
void runtime_error_clear(RuntimeError *error);
void runtime_error_set(RuntimeError *error, int line, int column, const char *format, ...);
void runtime_error_set_value(RuntimeError *error, Value value, int line, int column);
void runtime_error_capture(RuntimeError *error, const CallFrame *frames, int count);

#endif
