// (c) Glape Contributors

#ifndef GLAPE_CLI_DISPATCHER_H
#define GLAPE_CLI_DISPATCHER_H

#include <stddef.h>

#define CLI_MAX_OPTIONS 32
#define CLI_MAX_POSITIONAL 32

typedef struct {
    const char *name;
    const char *value_hint;
    const char *description;
    int required;
} CliFlag;

typedef struct {
    const char *name;
    const char *value;
} CliOption;

typedef struct {
    CliOption options[CLI_MAX_OPTIONS];
    size_t option_count;
    const char *positional[CLI_MAX_POSITIONAL];
    size_t positional_count;
} CliArgs;

typedef int (*CliHandler)(const CliArgs *args);
typedef int (*CliOnEmpty)(void);

typedef struct CliCommand {
    const char *name;
    const char *description;
    const CliFlag *flags;
    size_t flag_count;
    CliHandler handler;
    const struct CliCommand *subcommands;
    size_t subcommand_count;
    const char *positional;
} CliCommand;

typedef struct {
    const char *program;
    const CliCommand *commands;
    size_t command_count;
    CliOnEmpty on_empty;
} CliDispatcher;

void cli_dispatcher_init(CliDispatcher *dispatcher, const char *program,
                         const CliCommand *commands, size_t command_count,
                         CliOnEmpty on_empty);

int cli_dispatcher_run(const CliDispatcher *dispatcher, int argc, char **argv);

int cli_args_has(const CliArgs *args, const char *name);
const char *cli_args_get(const CliArgs *args, const char *name);

#endif
