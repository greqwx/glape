// (c) Glape Contributors

#include "dispatcher.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CLI_MAX_PATH 256
#define CLI_MAX_ERROR 256
#define CLI_MAX_EDIT 128

typedef struct {
    const CliCommand *command;
    char path[CLI_MAX_PATH];
    int next;
} CliResolved;

static int is_help_flag(const char *arg)
{
    return strcmp(arg, "--help") == 0 || strcmp(arg, "-h") == 0;
}

static const CliCommand *find_command(const CliCommand *list, size_t count, const char *name)
{
    size_t i;

    for (i = 0; i < count; i++) {
        if (strcmp(list[i].name, name) == 0)
            return &list[i];
    }
    return NULL;
}

static const CliFlag *find_flag(const CliCommand *command, const char *name)
{
    size_t i;

    for (i = 0; i < command->flag_count; i++) {
        if (strcmp(command->flags[i].name, name) == 0)
            return &command->flags[i];
    }
    return NULL;
}

static size_t edit_distance(const char *a, const char *b)
{
    size_t a_len = strlen(a);
    size_t b_len = strlen(b);
    size_t previous[CLI_MAX_EDIT];
    size_t current[CLI_MAX_EDIT];
    size_t i;
    size_t j;

    if (a_len >= CLI_MAX_EDIT || b_len >= CLI_MAX_EDIT)
        return CLI_MAX_EDIT;

    for (j = 0; j <= b_len; j++)
        previous[j] = j;

    for (i = 1; i <= a_len; i++) {
        current[0] = i;

        for (j = 1; j <= b_len; j++) {
            size_t cost = a[i - 1] == b[j - 1] ? 0 : 1;
            size_t remove = previous[j] + 1;
            size_t insert = current[j - 1] + 1;
            size_t change = previous[j - 1] + cost;
            size_t best = remove < insert ? remove : insert;

            current[j] = best < change ? best : change;
        }

        for (j = 0; j <= b_len; j++)
            previous[j] = current[j];
    }

    return previous[b_len];
}

static void suggest_command(const CliCommand *list, size_t count, const char *name)
{
    size_t best = CLI_MAX_EDIT;
    const char *best_name = NULL;
    size_t i;

    for (i = 0; i < count; i++) {
        size_t distance = edit_distance(name, list[i].name);

        if (distance < best) {
            best = distance;
            best_name = list[i].name;
        }
    }

    if (best_name != NULL && best <= 2 && best < strlen(name))
        fprintf(stderr, "did you mean '%s'?\n", best_name);
}

static int append_path(char *path, size_t capacity, const char *name)
{
    size_t path_len = strlen(path);
    size_t name_len = strlen(name);
    size_t extra = path_len == 0 ? 0 : 1;

    if (path_len + extra + name_len + 1 > capacity)
        return 0;

    if (extra != 0)
        path[path_len++] = ' ';

    memcpy(path + path_len, name, name_len + 1);
    return 1;
}

static int resolve(const CliDispatcher *dispatcher, int argc, char **argv, int start,
                   CliResolved *out)
{
    const CliCommand *level = dispatcher->commands;
    size_t level_count = dispatcher->command_count;
    int i = start;

    out->command = NULL;
    out->path[0] = '\0';
    out->next = start;

    while (i < argc && argv[i][0] != '-') {
        const CliCommand *command = find_command(level, level_count, argv[i]);

        if (command == NULL) {
            if (out->path[0] != '\0')
                fprintf(stderr, "unknown command: %s %s\n", out->path, argv[i]);
            else
                fprintf(stderr, "unknown command: %s\n", argv[i]);
            suggest_command(level, level_count, argv[i]);
            return 0;
        }

        out->command = command;

        if (!append_path(out->path, sizeof out->path, command->name)) {
            fprintf(stderr, "command path is too long\n");
            return 0;
        }

        i++;

        if (command->subcommand_count == 0)
            break;

        level = command->subcommands;
        level_count = command->subcommand_count;
    }

    out->next = i;
    return 1;
}

static void print_flag_label(const CliFlag *flag, char *buffer, size_t size)
{
    if (flag->value_hint == NULL)
        snprintf(buffer, size, "--%s", flag->name);
    else
        snprintf(buffer, size, "--%s %s", flag->name, flag->value_hint);
}

static void print_root_help(FILE *out, const CliDispatcher *dispatcher)
{
    const CliCommand **sorted = NULL;
    size_t width = strlen("help");
    size_t i;

    fprintf(out, "usage: %s <command> [options]\n\ncommands:\n", dispatcher->program);

    sorted = (const CliCommand **)malloc((dispatcher->command_count + 1) * sizeof *sorted);
    if (sorted != NULL) {
        for (i = 0; i < dispatcher->command_count; i++)
            sorted[i] = &dispatcher->commands[i];
    }

    for (i = 0; i < dispatcher->command_count; i++) {
        size_t length = strlen(dispatcher->commands[i].name);

        if (length > width)
            width = length;
    }

    if (sorted != NULL) {
        size_t j;

        for (i = 1; i < dispatcher->command_count; i++) {
            const CliCommand *current = sorted[i];

            j = i;
            while (j > 0 && strcmp(sorted[j - 1]->name, current->name) > 0) {
                sorted[j] = sorted[j - 1];
                j--;
            }
            sorted[j] = current;
        }

        for (i = 0; i < dispatcher->command_count; i++)
            fprintf(out, "  %-*s   %s\n", (int)width, sorted[i]->name, sorted[i]->description);

        free(sorted);
    } else {
        for (i = 0; i < dispatcher->command_count; i++)
            fprintf(out, "  %-*s   %s\n", (int)width, dispatcher->commands[i].name,
                    dispatcher->commands[i].description);
    }

    fprintf(out, "  %-*s   %s\n", (int)width, "help", "show help for a command");
    fprintf(out, "\nrun '%s help <command>' for details on a command.\n", dispatcher->program);
}

static void print_command_help(FILE *out, const CliDispatcher *dispatcher, const char *path,
                               const CliCommand *command)
{
    size_t width;
    size_t i;

    fprintf(out, "usage: %s %s", dispatcher->program, path);

    if (command->subcommand_count > 0)
        fprintf(out, " <command>");

    if (command->positional != NULL)
        fprintf(out, " %s", command->positional);

    for (i = 0; i < command->flag_count; i++) {
        char label[128];

        print_flag_label(&command->flags[i], label, sizeof label);

        if (command->flags[i].required)
            fprintf(out, " %s", label);
        else
            fprintf(out, " [%s]", label);
    }

    fprintf(out, "\n\n%s\n", command->description);

    if (command->subcommand_count > 0) {
        width = 0;

        for (i = 0; i < command->subcommand_count; i++) {
            size_t length = strlen(command->subcommands[i].name);

            if (length > width)
                width = length;
        }

        fprintf(out, "\ncommands:\n");

        for (i = 0; i < command->subcommand_count; i++)
            fprintf(out, "  %-*s   %s\n", (int)width, command->subcommands[i].name,
                    command->subcommands[i].description);

        return;
    }

    width = strlen("-h, --help");

    for (i = 0; i < command->flag_count; i++) {
        char label[128];
        size_t length;

        print_flag_label(&command->flags[i], label, sizeof label);
        length = strlen(label);

        if (length > width)
            width = length;
    }

    fprintf(out, "\noptions:\n");

    for (i = 0; i < command->flag_count; i++) {
        char label[128];

        print_flag_label(&command->flags[i], label, sizeof label);
        fprintf(out, "  %-*s   %s\n", (int)width, label, command->flags[i].description);
    }

    fprintf(out, "  %-*s   %s\n", (int)width, "-h, --help", "show this help");
}

static int add_option(CliArgs *args, const char *name, const char *value)
{
    if (args->option_count >= CLI_MAX_OPTIONS)
        return 0;

    args->options[args->option_count].name = name;
    args->options[args->option_count].value = value;
    args->option_count++;
    return 1;
}

static int parse_args(const CliCommand *command, int argc, char **argv, int from, CliArgs *out,
                      char *error, size_t error_size)
{
    int i;

    for (i = from; i < argc; i++) {
        const char *token = argv[i];

        if (strncmp(token, "--", 2) != 0) {
            if (command->positional == NULL) {
                snprintf(error, error_size, "unknown argument: %s", token);
                return 0;
            }

            if (out->positional_count >= CLI_MAX_POSITIONAL) {
                snprintf(error, error_size, "too many arguments");
                return 0;
            }

            out->positional[out->positional_count++] = token;
            continue;
        }

        {
            const char *name = token + 2;
            const char *equals = strchr(name, '=');
            const char *value = NULL;
            char name_buffer[64];
            size_t name_length;
            const CliFlag *flag;

            if (equals != NULL) {
                name_length = (size_t)(equals - name);
                value = equals + 1;
            } else {
                name_length = strlen(name);
            }

            if (name_length == 0 || name_length >= sizeof name_buffer) {
                snprintf(error, error_size, "invalid option: %s", token);
                return 0;
            }

            memcpy(name_buffer, name, name_length);
            name_buffer[name_length] = '\0';

            flag = find_flag(command, name_buffer);
            if (flag == NULL) {
                snprintf(error, error_size, "unknown option: --%s", name_buffer);
                return 0;
            }

            if (flag->value_hint == NULL) {
                if (value != NULL) {
                    snprintf(error, error_size, "option --%s does not take a value", flag->name);
                    return 0;
                }

                if (!add_option(out, flag->name, "1")) {
                    snprintf(error, error_size, "too many options");
                    return 0;
                }
                continue;
            }

            if (value == NULL) {
                if (i + 1 >= argc) {
                    snprintf(error, error_size, "option --%s needs a value", flag->name);
                    return 0;
                }
                value = argv[++i];
            }

            if (value[0] == '\0') {
                snprintf(error, error_size, "option --%s needs a value", flag->name);
                return 0;
            }

            if (!add_option(out, flag->name, value)) {
                snprintf(error, error_size, "too many options");
                return 0;
            }
        }
    }

    for (i = 0; i < (int)command->flag_count; i++) {
        if (command->flags[i].required && !cli_args_has(out, command->flags[i].name)) {
            snprintf(error, error_size, "missing required option: --%s", command->flags[i].name);
            return 0;
        }
    }

    return 1;
}

void cli_dispatcher_init(CliDispatcher *dispatcher, const char *program,
                         const CliCommand *commands, size_t command_count,
                         CliOnEmpty on_empty)
{
    dispatcher->program = program;
    dispatcher->commands = commands;
    dispatcher->command_count = command_count;
    dispatcher->on_empty = on_empty;
}

int cli_args_has(const CliArgs *args, const char *name)
{
    size_t i;

    for (i = 0; i < args->option_count; i++) {
        if (strcmp(args->options[i].name, name) == 0)
            return 1;
    }
    return 0;
}

const char *cli_args_get(const CliArgs *args, const char *name)
{
    size_t i;

    for (i = 0; i < args->option_count; i++) {
        if (strcmp(args->options[i].name, name) == 0)
            return args->options[i].value;
    }
    return "";
}

int cli_dispatcher_run(const CliDispatcher *dispatcher, int argc, char **argv)
{
    CliResolved target;
    CliArgs args;
    char error[CLI_MAX_ERROR];
    int i;

    if (argc < 2) {
        if (dispatcher->on_empty != NULL)
            return dispatcher->on_empty();

        print_root_help(stdout, dispatcher);
        return 0;
    }

    if (is_help_flag(argv[1])) {
        print_root_help(stdout, dispatcher);
        return 0;
    }

    if (strcmp(argv[1], "help") == 0) {
        if (argc == 2) {
            print_root_help(stdout, dispatcher);
            return 0;
        }

        if (!resolve(dispatcher, argc, argv, 2, &target)) {
            fprintf(stderr, "run '%s help' to see the list of commands.\n", dispatcher->program);
            return 1;
        }

        if (target.command != NULL)
            print_command_help(stdout, dispatcher, target.path, target.command);
        else
            print_root_help(stdout, dispatcher);

        return 0;
    }

    if (!resolve(dispatcher, argc, argv, 1, &target)) {
        fprintf(stderr, "run '%s help' to see the list of commands.\n", dispatcher->program);
        return 1;
    }

    if (target.command == NULL) {
        fprintf(stderr, "unknown option: %s\n", argv[1]);
        fprintf(stderr, "run '%s help' to see the list of commands.\n", dispatcher->program);
        return 1;
    }

    for (i = target.next; i < argc; i++) {
        if (is_help_flag(argv[i])) {
            print_command_help(stdout, dispatcher, target.path, target.command);
            return 0;
        }
    }

    if (target.command->handler == NULL) {
        print_command_help(stderr, dispatcher, target.path, target.command);
        return 1;
    }

    memset(&args, 0, sizeof args);

    if (!parse_args(target.command, argc, argv, target.next, &args, error, sizeof error)) {
        fprintf(stderr, "%s\n", error);
        fprintf(stderr, "run '%s help %s' for usage.\n", dispatcher->program, target.path);
        return 1;
    }

    return target.command->handler(&args);
}
