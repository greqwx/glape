// (c) Glape Contributors

#include "lexer.h"

#include <stdio.h>
#include <string.h>

typedef struct {
    const char *name;
    TokenType type;
} Keyword;

static const Keyword keywords[] = {
    {"load", TOK_LOAD},
    {"define", TOK_DEFINE},
    {"if", TOK_IF},
    {"else", TOK_ELSE},
    {"loop", TOK_LOOP},
    {"in", TOK_IN},
    {"catch", TOK_CATCH},
    {"skip", TOK_SKIP},
    {"return", TOK_RETURN},
    {"break", TOK_BREAK},
    {"continue", TOK_CONTINUE},
    {"and", TOK_AND},
    {"or", TOK_OR},
    {"not", TOK_NOT},
    {"true", TOK_TRUE},
    {"false", TOK_FALSE},
    {"nil", TOK_NIL}
};

static const char *const token_names[] = {
    "EOF",
    "ERROR",
    "NEWLINE",
    "INDENT",
    "DEDENT",
    "IDENT",
    "NUMBER",
    "STRING",
    "LOAD",
    "DEFINE",
    "IF",
    "ELSE",
    "LOOP",
    "IN",
    "CATCH",
    "SKIP",
    "RETURN",
    "BREAK",
    "CONTINUE",
    "AND",
    "OR",
    "NOT",
    "TRUE",
    "FALSE",
    "NIL",
    "LPAREN",
    "RPAREN",
    "LBRACKET",
    "RBRACKET",
    "LBRACE",
    "RBRACE",
    "COMMA",
    "COLON",
    "DOT",
    "BANG",
    "ASSIGN",
    "EQ",
    "NE",
    "LT",
    "LE",
    "GT",
    "GE",
    "PLUS",
    "MINUS",
    "STAR",
    "SLASH",
    "PERCENT"
};

typedef char token_names_match[(sizeof(token_names) / sizeof(token_names[0]) == TOK_COUNT) ? 1 : -1];

static int peek(const Lexer *lx, size_t offset)
{
    size_t at = lx->pos + offset;

    if (at >= lx->length)
        return -1;
    return (unsigned char)lx->source[at];
}

static void step(Lexer *lx)
{
    lx->pos++;
    lx->column++;
}

static void fail(Lexer *lx, const char *message)
{
    if (lx->failed)
        return;
    lx->failed = 1;
    snprintf(lx->error, sizeof lx->error, "%s", message);
}

static Token make_token(TokenType type, const char *start, int length, int line, int column)
{
    Token token;

    token.type = type;
    token.start = start;
    token.length = length;
    token.line = line;
    token.column = column;
    return token;
}

static Token error_token(Lexer *lx, int line, int column)
{
    return make_token(TOK_ERROR, lx->source + lx->pos, 0, line, column);
}

static void consume_newline(Lexer *lx)
{
    if (lx->source[lx->pos] == '\r') {
        lx->pos++;
        if (lx->pos < lx->length && lx->source[lx->pos] == '\n')
            lx->pos++;
    } else {
        lx->pos++;
    }
    lx->line++;
    lx->column = 1;
}

static int is_digit(int c)
{
    return c >= '0' && c <= '9';
}

static int is_hex_digit(int c)
{
    return is_digit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

static int is_ident_start(int c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

static int is_ident_char(int c)
{
    return is_ident_start(c) || is_digit(c);
}

static int scan_indent(Lexer *lx, int *width)
{
    int spaces = 0;
    int tabs = 0;
    int total = 0;

    while (lx->pos < lx->length) {
        char c = lx->source[lx->pos];

        if (c == ' ') {
            spaces++;
            total++;
        } else if (c == '\t') {
            tabs++;
            total += 8;
        } else {
            break;
        }
        lx->pos++;
        lx->column++;
    }

    if (spaces > 0 && tabs > 0) {
        fail(lx, "mixed tabs and spaces in indentation");
        return 0;
    }

    *width = total;
    return 1;
}

static TokenType keyword_type(const char *text, int length)
{
    size_t i;

    for (i = 0; i < sizeof(keywords) / sizeof(keywords[0]); i++) {
        if ((int)strlen(keywords[i].name) == length &&
            memcmp(keywords[i].name, text, (size_t)length) == 0)
            return keywords[i].type;
    }
    return TOK_IDENT;
}

static Token scan_number(Lexer *lx, int line, int column)
{
    size_t start = lx->pos;

    if (peek(lx, 0) == '0' && (peek(lx, 1) == 'x' || peek(lx, 1) == 'X')) {
        step(lx);
        step(lx);
        if (!is_hex_digit(peek(lx, 0))) {
            fail(lx, "malformed hexadecimal number");
            return error_token(lx, line, column);
        }
        while (is_hex_digit(peek(lx, 0)) || peek(lx, 0) == '_')
            step(lx);
    } else {
        while (is_digit(peek(lx, 0)) || peek(lx, 0) == '_')
            step(lx);

        if (peek(lx, 0) == '.' && is_digit(peek(lx, 1))) {
            step(lx);
            while (is_digit(peek(lx, 0)) || peek(lx, 0) == '_')
                step(lx);
        }

        if (peek(lx, 0) == 'e' || peek(lx, 0) == 'E') {
            size_t saved_pos = lx->pos;
            int saved_column = lx->column;

            step(lx);
            if (peek(lx, 0) == '+' || peek(lx, 0) == '-')
                step(lx);

            if (!is_digit(peek(lx, 0))) {
                lx->pos = saved_pos;
                lx->column = saved_column;
            } else {
                while (is_digit(peek(lx, 0)) || peek(lx, 0) == '_')
                    step(lx);
            }
        }
    }

    lx->line_has_token = 1;
    return make_token(TOK_NUMBER, lx->source + start, (int)(lx->pos - start), line, column);
}

static int skip_quoted(Lexer *lx)
{
    step(lx);

    while (lx->pos < lx->length) {
        int c = peek(lx, 0);

        if (c == '\n' || c == '\r')
            return 0;

        if (c == '\\') {
            step(lx);
            if (lx->pos >= lx->length)
                return 0;
            c = peek(lx, 0);
            if (c == '\n' || c == '\r')
                return 0;
            step(lx);
            continue;
        }

        if (c == '"') {
            step(lx);
            return 1;
        }

        step(lx);
    }

    return 0;
}

static Token scan_string(Lexer *lx, int line, int column)
{
    size_t content = lx->pos + 1;

    step(lx);

    while (lx->pos < lx->length) {
        int c = peek(lx, 0);

        if (c == '\n' || c == '\r') {
            fail(lx, "unterminated string");
            return error_token(lx, lx->line, lx->column);
        }

        if (c == '\\') {
            step(lx);
            if (lx->pos >= lx->length)
                break;
            c = peek(lx, 0);
            if (c == '\n' || c == '\r')
                break;
            step(lx);
            continue;
        }

        if (c == '$' && peek(lx, 1) == '{') {
            int depth = 1;

            step(lx);
            step(lx);

            while (depth > 0) {
                if (lx->pos >= lx->length) {
                    fail(lx, "unterminated interpolation");
                    return error_token(lx, lx->line, lx->column);
                }

                c = peek(lx, 0);

                if (c == '\n' || c == '\r') {
                    fail(lx, "unterminated interpolation");
                    return error_token(lx, lx->line, lx->column);
                }

                if (c == '"') {
                    if (!skip_quoted(lx)) {
                        fail(lx, "unterminated string");
                        return error_token(lx, lx->line, lx->column);
                    }
                    continue;
                }

                if (c == '{') {
                    depth++;
                } else if (c == '}') {
                    depth--;
                }
                step(lx);
            }
            continue;
        }

        if (c == '"') {
            size_t end = lx->pos;

            step(lx);
            lx->line_has_token = 1;
            return make_token(TOK_STRING, lx->source + content, (int)(end - content), line, column);
        }

        step(lx);
    }

    fail(lx, "unterminated string");
    return error_token(lx, lx->line, lx->column);
}

static Token scan_operator(Lexer *lx, int line, int column)
{
    int c = peek(lx, 0);
    size_t start = lx->pos;
    TokenType type = TOK_ERROR;
    int length = 1;
    int i;

    switch (c) {
    case '(':
        type = TOK_LPAREN;
        lx->bracket_depth++;
        break;
    case ')':
        if (lx->bracket_depth == 0) {
            fail(lx, "unexpected ')'");
            return error_token(lx, line, column);
        }
        lx->bracket_depth--;
        type = TOK_RPAREN;
        break;
    case '[':
        type = TOK_LBRACKET;
        lx->bracket_depth++;
        break;
    case ']':
        if (lx->bracket_depth == 0) {
            fail(lx, "unexpected ']'");
            return error_token(lx, line, column);
        }
        lx->bracket_depth--;
        type = TOK_RBRACKET;
        break;
    case '{':
        type = TOK_LBRACE;
        lx->bracket_depth++;
        break;
    case '}':
        if (lx->bracket_depth == 0) {
            fail(lx, "unexpected '}'");
            return error_token(lx, line, column);
        }
        lx->bracket_depth--;
        type = TOK_RBRACE;
        break;
    case ',':
        type = TOK_COMMA;
        break;
    case ':':
        type = TOK_COLON;
        break;
    case '.':
        type = TOK_DOT;
        break;
    case '+':
        type = TOK_PLUS;
        break;
    case '-':
        type = TOK_MINUS;
        break;
    case '*':
        type = TOK_STAR;
        break;
    case '/':
        type = TOK_SLASH;
        break;
    case '%':
        type = TOK_PERCENT;
        break;
    case '=':
        if (peek(lx, 1) == '=') {
            type = TOK_EQ;
            length = 2;
        } else {
            type = TOK_ASSIGN;
        }
        break;
    case '!':
        if (peek(lx, 1) == '=') {
            type = TOK_NE;
            length = 2;
        } else {
            type = TOK_BANG;
        }
        break;
    case '<':
        if (peek(lx, 1) == '=') {
            type = TOK_LE;
            length = 2;
        } else {
            type = TOK_LT;
        }
        break;
    case '>':
        if (peek(lx, 1) == '=') {
            type = TOK_GE;
            length = 2;
        } else {
            type = TOK_GT;
        }
        break;
    default:
        fail(lx, "unexpected character");
        return error_token(lx, line, column);
    }

    for (i = 0; i < length; i++)
        step(lx);

    lx->line_has_token = 1;
    return make_token(type, lx->source + start, length, line, column);
}

void lexer_init(Lexer *lx, const char *source, size_t length)
{
    memset(lx, 0, sizeof *lx);
    lx->source = source;
    lx->length = length;
    lx->line = 1;
    lx->column = 1;
    lx->at_line_start = 1;
    lx->indent_stack[0] = 0;
    lx->indent_count = 1;
}

Token lexer_next(Lexer *lx)
{
    for (;;) {
        if (lx->failed)
            return make_token(TOK_EOF, lx->source + lx->pos, 0, lx->line, lx->column);

        if (lx->pending_dedents > 0) {
            lx->pending_dedents--;
            return make_token(TOK_DEDENT, lx->source + lx->pos, 0, lx->line, lx->column);
        }

        if (lx->at_line_start) {
            int width;

            if (lx->pos >= lx->length) {
                if (lx->line_has_token) {
                    lx->line_has_token = 0;
                    return make_token(TOK_NEWLINE, lx->source + lx->pos, 0, lx->line, lx->column);
                }
                if (lx->indent_count > 1) {
                    lx->pending_dedents = lx->indent_count - 1;
                    lx->indent_count = 1;
                    continue;
                }
                return make_token(TOK_EOF, lx->source + lx->pos, 0, lx->line, lx->column);
            }

            if (!scan_indent(lx, &width))
                return error_token(lx, lx->line, lx->column);

            {
                int c = peek(lx, 0);

                if (c == '\n' || c == '\r' || (c == '/' && peek(lx, 1) == '/')) {
                    lx->at_line_start = 0;
                    continue;
                }
            }

            if (lx->bracket_depth == 0) {
                int top = lx->indent_stack[lx->indent_count - 1];

                if (width > top) {
                    if (lx->indent_count >= LEXER_MAX_INDENT) {
                        fail(lx, "indentation too deep");
                        return error_token(lx, lx->line, lx->column);
                    }
                    lx->indent_stack[lx->indent_count++] = width;
                    lx->at_line_start = 0;
                    return make_token(TOK_INDENT, lx->source + lx->pos, 0, lx->line, lx->column);
                }

                if (width < top) {
                    int popped = 0;

                    while (lx->indent_count > 1 &&
                           lx->indent_stack[lx->indent_count - 1] > width) {
                        lx->indent_count--;
                        popped++;
                    }

                    if (lx->indent_stack[lx->indent_count - 1] != width) {
                        fail(lx, "inconsistent indentation");
                        return error_token(lx, lx->line, lx->column);
                    }

                    lx->pending_dedents = popped;
                    lx->at_line_start = 0;
                    continue;
                }
            }

            lx->at_line_start = 0;
            continue;
        }

        while (lx->pos < lx->length) {
            int c = peek(lx, 0);

            if (c == ' ' || c == '\t' || c == '\f')
                step(lx);
            else
                break;
        }

        if (lx->pos >= lx->length) {
            if (lx->line_has_token) {
                lx->line_has_token = 0;
                return make_token(TOK_NEWLINE, lx->source + lx->pos, 0, lx->line, lx->column);
            }
            if (lx->indent_count > 1) {
                lx->pending_dedents = lx->indent_count - 1;
                lx->indent_count = 1;
                continue;
            }
            return make_token(TOK_EOF, lx->source + lx->pos, 0, lx->line, lx->column);
        }

        {
            int c = peek(lx, 0);
            int line = lx->line;
            int column = lx->column;

            if (c == '\n' || c == '\r') {
                int has_token = lx->line_has_token;

                consume_newline(lx);
                lx->at_line_start = 1;
                lx->line_has_token = 0;

                if (has_token && lx->bracket_depth == 0)
                    return make_token(TOK_NEWLINE, lx->source + lx->pos, 0, line, column);
                continue;
            }

            if (c == '/' && peek(lx, 1) == '/') {
                while (lx->pos < lx->length) {
                    c = peek(lx, 0);
                    if (c == '\n' || c == '\r')
                        break;
                    step(lx);
                }
                continue;
            }

            if (c == '"')
                return scan_string(lx, line, column);

            if (is_digit(c))
                return scan_number(lx, line, column);

            if (is_ident_start(c)) {
                size_t start = lx->pos;

                while (is_ident_char(peek(lx, 0)))
                    step(lx);

                lx->line_has_token = 1;
                return make_token(keyword_type(lx->source + start, (int)(lx->pos - start)),
                                  lx->source + start, (int)(lx->pos - start), line, column);
            }

            return scan_operator(lx, line, column);
        }
    }
}

const char *lexer_error(const Lexer *lx)
{
    return lx->error;
}

const char *token_type_name(TokenType type)
{
    if (type < 0 || type >= TOK_COUNT)
        return "?";
    return token_names[type];
}
