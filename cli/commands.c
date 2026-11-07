// (c) Glape Contributors

#if defined(_MSC_VER)
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "commands.h"

#include <stdio.h>
#include <stdlib.h>

#include "cli/console.h"
#include "libs/lib_core.h"
#include "mem/util.h"
#include "parse/ast.h"
#include "parse/lexer.h"
#include "parse/parser.h"
#include "runtime/interp.h"

#ifndef GLAPE_VERSION
#define GLAPE_VERSION "0.1"
#endif

static const char *platform_os(void)
{
#if defined(_WIN32)
    return "windows";
#elif defined(__APPLE__)
    return "macos";
#elif defined(__linux__)
    return "linux";
#elif defined(__FreeBSD__)
    return "freebsd";
#else
    return "unknown";
#endif
}

static const char *platform_arch(void)
{
#if defined(_M_X64) || defined(__x86_64__)
    return "amd64";
#elif defined(_M_IX86) || defined(__i386__)
    return "x86";
#elif defined(_M_ARM64) || defined(__aarch64__)
    return "arm64";
#elif defined(_M_ARM) || defined(__arm__)
    return "arm";
#else
    return "unknown";
#endif
}

int cli_cmd_info(void)
{
    printf("(c) Glape Contributors\n");
    printf("https://github.com/greqwx/glape\n");
    return 0;
}

static char *read_file(const char *path, size_t *out_length)
{
    FILE *file = fopen(path, "rb");
    long size;
    char *data;

    if (!file)
        return NULL;

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return NULL;
    }

    size = ftell(file);
    if (size < 0) {
        fclose(file);
        return NULL;
    }

    rewind(file);
    data = (char *)malloc((size_t)size + 1);
    if (!data) {
        fclose(file);
        return NULL;
    }

    if (size > 0 && fread(data, 1, (size_t)size, file) != (size_t)size) {
        free(data);
        fclose(file);
        return NULL;
    }

    data[size] = '\0';
    fclose(file);
    *out_length = (size_t)size;
    return data;
}

static void print_token(const Token *token)
{
    printf("%d:%d %s", token->line, token->column, token_type_name(token->type));

    if (token->type == TOK_IDENT || token->type == TOK_NUMBER || token->type == TOK_STRING) {
        printf(" ");
        fwrite(token->start, 1, (size_t)token->length, stdout);
    }

    printf("\n");
}

int cli_cmd_ast(const CliArgs *args)
{
    const char *path;
    char *source;
    size_t length;
    Arena arena;
    Parser parser;
    AstNode *program;

    if (args->positional_count < 1) {
        fprintf(stderr, "usage: glape ast <file>\n");
        return 2;
    }

    path = args->positional[0];
    source = read_file(path, &length);
    if (!source) {
        fprintf(stderr, "glape: cannot read %s\n", path);
        return 1;
    }

    arena_init(&arena);
    parser_init(&parser, &arena, source, length);
    program = parser_parse_program(&parser);

    if (program == NULL) {
        console_error_location(stderr, path, parser_error_line(&parser),
                               parser_error_column(&parser));
        fprintf(stderr, "%s\n", parser_error(&parser));
        arena_free(&arena);
        free(source);
        return 1;
    }

    ast_dump(program, stdout);
    arena_free(&arena);
    free(source);
    return 0;
}

static void print_runtime_error(Interpreter *interp, const char *path)
{
    const char *file = interp->error.file != NULL ? interp->error.file : path;
    int frame;

    if (interp->error.message[0] != '\0') {
        console_error_location(stderr, file, interp->error.line, interp->error.column);
        fprintf(stderr, "%s\n", interp->error.message);
    } else {
        StrBuf buffer;

        strbuf_init(&buffer);

        if (value_format(interp->error.value, &buffer) && buffer.data != NULL) {
            console_error_location(stderr, file, interp->error.line, interp->error.column);
            fprintf(stderr, "%s\n", buffer.data);
        }

        strbuf_free(&buffer);
    }

    for (frame = interp->error.frame_count - 1; frame >= 0; frame--) {
        const CallFrame *entry = &interp->error.frames[frame];
        const char *frame_file = entry->file != NULL ? entry->file : file;

        console_stack_frame(stderr, entry->function, frame_file, entry->line, entry->column);
    }
}

int cli_cmd_run(const CliArgs *args)
{
    const char *path;
    char *source;
    size_t length;
    Arena arena;
    Parser parser;
    AstNode *program;
    Interpreter interp;
    ExecStatus status;

    if (args->positional_count < 1) {
        fprintf(stderr, "usage: glape run <file>\n");
        return 2;
    }

    path = args->positional[0];
    source = read_file(path, &length);
    if (!source) {
        fprintf(stderr, "glape: cannot read %s\n", path);
        return 1;
    }

    arena_init(&arena);
    parser_init(&parser, &arena, source, length);
    program = parser_parse_program(&parser);

    if (program == NULL) {
        console_error_location(stderr, path, parser_error_line(&parser),
                               parser_error_column(&parser));
        fprintf(stderr, "%s\n", parser_error(&parser));
        arena_free(&arena);
        free(source);
        return 1;
    }

    interp_init(&interp, &arena, stdout, path);
    lib_core_install(&interp);
    status = interp_run(&interp, program);

    if (status != EXEC_OK)
        print_runtime_error(&interp, path);

    if (interp.gc.stats) {
        /*
         * Collect once more so the report describes the live set instead of
         * whatever was garbage when the program happened to stop.
         */
        gc_collect(&interp.gc);
        gc_report(&interp.gc, stderr);
    }

    interp_free(&interp);
    arena_free(&arena);
    free(source);
    return status == EXEC_OK ? 0 : 1;
}

int cli_cmd_tokens(const CliArgs *args)
{
    const char *path;
    char *source;
    size_t length;
    Lexer lexer;

    if (args->positional_count < 1) {
        fprintf(stderr, "usage: glape tokens <file>\n");
        return 2;
    }

    path = args->positional[0];
    source = read_file(path, &length);
    if (!source) {
        fprintf(stderr, "glape: cannot read %s\n", path);
        return 1;
    }

    lexer_init(&lexer, source, length);

    for (;;) {
        Token token = lexer_next(&lexer);

        if (token.type == TOK_ERROR) {
            console_error_location(stderr, path, token.line, token.column);
            fprintf(stderr, "%s\n", lexer_error(&lexer));
            free(source);
            return 1;
        }

        print_token(&token);

        if (token.type == TOK_EOF)
            break;
    }

    free(source);
    return 0;
}

int cli_cmd_version(const CliArgs *args)
{
    (void)args;
    printf("glape %s (%s,%s)\n", GLAPE_VERSION, platform_os(), platform_arch());
    return 0;
}
