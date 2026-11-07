// (c) Glape Contributors

#ifndef GLAPE_SYS_H
#define GLAPE_SYS_H

#include <stddef.h>

#define SYS_TEXT_MAX 4096

#ifdef _WIN32
#define SYS_PATH_SEP "\\"
#define SYS_EOL "\r\n"
#define SYS_HAS_DRIVES 1
#define SYS_IS_SEP(c) ((c) == '/' || (c) == '\\')
#else
#define SYS_PATH_SEP "/"
#define SYS_EOL "\n"
#define SYS_HAS_DRIVES 0
#define SYS_IS_SEP(c) ((c) == '/')
#endif

/* "linux", "macos", "windows" or "unknown" */
const char *sys_os_name(void);

/* "amd64", "arm64", "x86", "arm" or "unknown" */
const char *sys_arch_name(void);

int sys_cpu_count(void);

/*
 * The functions below write a NUL terminated string into out and return 1,
 * or return 0 when the value is unavailable or does not fit into size bytes.
 */
int sys_hostname(char *out, size_t size);
int sys_username(char *out, size_t size);
int sys_home_dir(char *out, size_t size);
int sys_temp_dir(char *out, size_t size);

#endif
