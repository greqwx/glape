// (c) Glape Contributors

#if !defined(_WIN32)
#define _POSIX_C_SOURCE 200809L
#endif

#include "console.h"

#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <io.h>
#include <windows.h>
#else
#include <unistd.h>
#endif

int console_supports_color(FILE *stream)
{
    const char *setting = getenv("GLAPE_COLOR");
    const char *no_color = getenv("NO_COLOR");

    if (setting != NULL) {
        if (strcmp(setting, "always") == 0 || strcmp(setting, "1") == 0)
            return 1;

        if (strcmp(setting, "never") == 0 || strcmp(setting, "0") == 0)
            return 0;
    }

    if (no_color != NULL && no_color[0] != '\0')
        return 0;

#if defined(_WIN32)
    {
        int fd = _fileno(stream);
        HANDLE handle;
        DWORD mode;

        if (fd < 0 || !_isatty(fd))
            return 0;

        handle = (HANDLE)_get_osfhandle(fd);
        if (handle == INVALID_HANDLE_VALUE)
            return 0;

        if (!GetConsoleMode(handle, &mode))
            return 0;

        if (!(mode & ENABLE_VIRTUAL_TERMINAL_PROCESSING)) {
            mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;

            if (!SetConsoleMode(handle, mode))
                return 0;
        }

        return 1;
    }
#else
    return isatty(fileno(stream)) != 0;
#endif
}

void console_error_location(FILE *stream, const char *file, int line, int column)
{
    int color = console_supports_color(stream);

    if (file != NULL && file[0] != '\0') {
        if (color)
            fprintf(stream, "\x1b[97m%s:\x1b[0m", file);
        else
            fprintf(stream, "%s:", file);
    }

    if (color)
        fprintf(stream, "\x1b[91m%d:%d:\x1b[0m ", line, column);
    else
        fprintf(stream, "%d:%d: ", line, column);
}

void console_stack_frame(FILE *stream, const char *function, const char *file, int line,
                         int column)
{
    int color = console_supports_color(stream);

    fprintf(stream, "  at %s (", function != NULL ? function : "?");

    if (file != NULL && file[0] != '\0') {
        if (color)
            fprintf(stream, "\x1b[97m%s:\x1b[0m", file);
        else
            fprintf(stream, "%s:", file);
    }

    if (color)
        fprintf(stream, "\x1b[91m%d:%d\x1b[0m)\n", line, column);
    else
        fprintf(stream, "%d:%d)\n", line, column);
}
