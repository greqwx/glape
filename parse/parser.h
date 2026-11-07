// (c) Glape Contributors

#ifndef GLAPE_PARSER_H
#define GLAPE_PARSER_H

#include <stddef.h>

#include "mem/util.h"
#include "parse/ast.h"
#include "parse/lexer.h"

typedef struct {
    Lexer lexer;
    Token current;
    Arena *arena;
    int failed;
    int error_line;
    int error_column;
    char error[256];
} Parser;

void parser_init(Parser *parser, Arena *arena, const char *source, size_t length);
AstNode *parser_parse_program(Parser *parser);
AstNode *parser_parse_expression_text(Arena *arena, const char *source, size_t length,
                                      char *error, size_t error_size);
const char *parser_error(const Parser *parser);
int parser_error_line(const Parser *parser);
int parser_error_column(const Parser *parser);

#endif
