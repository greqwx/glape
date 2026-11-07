// (c) Glape Contributors

#include "error.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

void runtime_error_init(RuntimeError *error)
{
    error->set = 0;
    error->value = value_nil();
    error->file = NULL;
    error->line = 0;
    error->column = 0;
    error->message[0] = '\0';
    error->frame_count = 0;
}

void runtime_error_clear(RuntimeError *error)
{
    error->set = 0;
    error->value = value_nil();
    error->file = NULL;
    error->line = 0;
    error->column = 0;
    error->message[0] = '\0';
    error->frame_count = 0;
}

void runtime_error_set(RuntimeError *error, int line, int column, const char *format, ...)
{
    va_list args;

    error->set = 1;
    error->value = value_nil();
    error->file = NULL;
    error->line = line;
    error->column = column;
    error->frame_count = 0;

    va_start(args, format);
    vsnprintf(error->message, sizeof error->message, format, args);
    va_end(args);
}

void runtime_error_set_value(RuntimeError *error, Value value, int line, int column)
{
    error->set = 1;
    error->value = value;
    error->file = NULL;
    error->line = line;
    error->column = column;
    error->message[0] = '\0';
    error->frame_count = 0;
}

void runtime_error_capture(RuntimeError *error, const CallFrame *frames, int count)
{
    const CallFrame *start = frames;
    int copied = count;

    if (copied > RUNTIME_MAX_FRAMES) {
        start = frames + (copied - RUNTIME_MAX_FRAMES);
        copied = RUNTIME_MAX_FRAMES;
    }

    if (copied > 0)
        memcpy(error->frames, start, (size_t)copied * sizeof *error->frames);

    error->frame_count = copied;
}
