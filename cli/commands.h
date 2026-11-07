// (c) Glape Contributors

#ifndef GLAPE_CLI_COMMANDS_H
#define GLAPE_CLI_COMMANDS_H

#include "dispatcher.h"

int cli_cmd_info(void);
int cli_cmd_ast(const CliArgs *args);
int cli_cmd_run(const CliArgs *args);
int cli_cmd_tokens(const CliArgs *args);
int cli_cmd_version(const CliArgs *args);

#endif
