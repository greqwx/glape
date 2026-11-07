// (c) Glape Contributors

#include "cli/commands.h"
#include "cli/dispatcher.h"

static const CliCommand commands[] = {
    {"ast", "print the syntax tree of a source file", NULL, 0, cli_cmd_ast, NULL, 0, "<file>"},
    {"run", "run a source file", NULL, 0, cli_cmd_run, NULL, 0, "<file>"},
    {"tokens", "print the token stream of a source file", NULL, 0, cli_cmd_tokens, NULL, 0,
     "<file>"},
    {"version", "show the version", NULL, 0, cli_cmd_version, NULL, 0, NULL}
};

int main(int argc, char **argv)
{
    CliDispatcher dispatcher;

    cli_dispatcher_init(&dispatcher, "glape", commands, sizeof(commands) / sizeof(commands[0]),
                        cli_cmd_info);

    return cli_dispatcher_run(&dispatcher, argc, argv);
}
