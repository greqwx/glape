// (c) Glape Contributors

#ifndef GLAPE_CONSOLE_H
#define GLAPE_CONSOLE_H

#include <stdio.h>

int console_supports_color(FILE *stream);
void console_error_location(FILE *stream, const char *file, int line, int column);
void console_stack_frame(FILE *stream, const char *function, const char *file, int line,
                         int column);

#endif
