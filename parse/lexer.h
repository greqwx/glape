// (c) Glape Contributors

#ifndef GLAPE_LEXER_H
#define GLAPE_LEXER_H

#include <stddef.h>

#define LEXER_MAX_INDENT 64

typedef enum {
    TOK_EOF,
    TOK_ERROR,
    TOK_NEWLINE,
    TOK_INDENT,
    TOK_DEDENT,

    TOK_IDENT,
    TOK_NUMBER,
    TOK_STRING,

    TOK_LOAD,
    TOK_DEFINE,
    TOK_IF,
    TOK_ELSE,
    TOK_LOOP,
    TOK_IN,
    TOK_CATCH,
    TOK_SKIP,
    TOK_RETURN,
    TOK_BREAK,
    TOK_CONTINUE,
    TOK_AND,
    TOK_OR,
    TOK_NOT,
    TOK_TRUE,
    TOK_FALSE,
    TOK_NIL,

    TOK_LPAREN,
    TOK_RPAREN,
    TOK_LBRACKET,
    TOK_RBRACKET,
    TOK_LBRACE,
    TOK_RBRACE,
    TOK_COMMA,
    TOK_COLON,
    TOK_DOT,
    TOK_BANG,

    TOK_ASSIGN,
    TOK_EQ,
    TOK_NE,
    TOK_LT,
    TOK_LE,
    TOK_GT,
    TOK_GE,
    TOK_PLUS,
    TOK_MINUS,
    TOK_STAR,
    TOK_SLASH,
    TOK_PERCENT,

    TOK_COUNT
} TokenType;

typedef struct {
    TokenType type;
    const char *start;
    int length;
    int line;
    int column;
} Token;

typedef struct {
    const char *source;
    size_t length;
    size_t pos;
    int line;
    int column;
    int bracket_depth;
    int at_line_start;
    int line_has_token;
    int indent_stack[LEXER_MAX_INDENT];
    int indent_count;
    int pending_dedents;
    int failed;
    char error[160];
} Lexer;

void lexer_init(Lexer *lx, const char *source, size_t length);
Token lexer_next(Lexer *lx);
const char *lexer_error(const Lexer *lx);
const char *token_type_name(TokenType type);

#endif
