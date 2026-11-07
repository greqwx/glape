// (c) Glape Contributors

/* on macOS this macro hides _SC_NPROCESSORS_ONLN, the default headers expose everything */
#if !defined(_WIN32) && !defined(__APPLE__)
#define _POSIX_C_SOURCE 200809L
#endif

#include "sys.h"

#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <pwd.h>
#include <unistd.h>
#endif

static int copy_text(char *out, size_t size, const char *text)
{
    size_t length;

    if (text == NULL || text[0] == '\0')
        return 0;

    length = strlen(text);
    if (length >= size)
        return 0;

    memcpy(out, text, length + 1);
    return 1;
}

const char *sys_os_name(void)
{
#if defined(_WIN32)
    return "windows";
#elif defined(__APPLE__)
    return "macos";
#elif defined(__linux__)
    return "linux";
#else
    return "unknown";
#endif
}

const char *sys_arch_name(void)
{
#if defined(_M_ARM64) || defined(__aarch64__)
    return "arm64";
#elif defined(_M_X64) || defined(__x86_64__)
    return "amd64";
#elif defined(_M_IX86) || defined(__i386__)
    return "x86";
#elif defined(_M_ARM) || defined(__arm__)
    return "arm";
#else
    return "unknown";
#endif
}

#ifdef _WIN32

int sys_cpu_count(void)
{
    SYSTEM_INFO info;

    GetSystemInfo(&info);
    return info.dwNumberOfProcessors > 0 ? (int)info.dwNumberOfProcessors : 1;
}

int sys_hostname(char *out, size_t size)
{
    char name[MAX_COMPUTERNAME_LENGTH + 1];
    DWORD length = (DWORD)sizeof name;

    if (!GetComputerNameA(name, &length))
        return 0;

    return copy_text(out, size, name);
}

int sys_username(char *out, size_t size)
{
    char name[257];
    DWORD length = (DWORD)sizeof name;

    if (!GetUserNameA(name, &length))
        return 0;

    return copy_text(out, size, name);
}

int sys_home_dir(char *out, size_t size)
{
    char path[SYS_TEXT_MAX];
    DWORD length = GetEnvironmentVariableA("USERPROFILE", path, (DWORD)sizeof path);

    if (length == 0 || length >= sizeof path)
        return 0;

    return copy_text(out, size, path);
}

int sys_temp_dir(char *out, size_t size)
{
    char path[SYS_TEXT_MAX];
    DWORD length = GetTempPathA((DWORD)sizeof path, path);

    if (length == 0 || length >= sizeof path)
        return 0;

    /* GetTempPathA keeps the trailing separator, drop it unless the path is a drive root */
    if (length > 3 && (path[length - 1] == '\\' || path[length - 1] == '/'))
        path[length - 1] = '\0';

    return copy_text(out, size, path);
}

#else

int sys_cpu_count(void)
{
    long count = sysconf(_SC_NPROCESSORS_ONLN);

    return count > 0 ? (int)count : 1;
}

int sys_hostname(char *out, size_t size)
{
    if (size == 0 || gethostname(out, size) != 0)
        return 0;

    /* gethostname does not terminate a truncated name */
    out[size - 1] = '\0';
    return out[0] != '\0';
}

int sys_username(char *out, size_t size)
{
    struct passwd *entry = getpwuid(geteuid());

    if (entry != NULL && copy_text(out, size, entry->pw_name))
        return 1;

    if (copy_text(out, size, getenv("LOGNAME")))
        return 1;

    return copy_text(out, size, getenv("USER"));
}

int sys_home_dir(char *out, size_t size)
{
    struct passwd *entry;

    if (copy_text(out, size, getenv("HOME")))
        return 1;

    entry = getpwuid(geteuid());
    return entry != NULL && copy_text(out, size, entry->pw_dir);
}

int sys_temp_dir(char *out, size_t size)
{
    if (copy_text(out, size, getenv("TMPDIR")))
        return 1;

    return copy_text(out, size, "/tmp");
}

#endif
