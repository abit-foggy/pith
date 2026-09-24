/*
 * lexer.c — UTF-8 scanner for The Pith Programming Language, v0.1
 *
 * Tracks 1-based line/col (col counts UTF-8 codepoints), treats
 * newlines as statement delimiters, skips whitespace and # comments,
 * lints identifiers toward lowerCamelCase, and reports problems with
 * rustc-style terminal diagnostics.
 */
#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../include/compiler.h"

/* ANSI escape sequences (enabled only when stderr is a terminal). */
#define C_RESET  "\x1b[0m"
#define C_RED    "\x1b[1;31m"
#define C_YELLOW "\x1b[1;33m"
#define C_BLUE   "\x1b[1;34m"

static int stderr_is_tty(void)
{
    return isatty(fileno(stderr));
}

/* ------------------------------------------------------------------ */
/* UTF-8 helpers                                                      */
/* ------------------------------------------------------------------ */

static size_t utf8_seq_len(unsigned char b)
{
    if (b < 0x80) return 1;
    if ((b & 0xE0) == 0xC0) return 2;
    if ((b & 0xF0) == 0xE0) return 3;
    if ((b & 0xF8) == 0xF0) return 4;
    return 0;
}

size_t pith_utf8_len(const char *s, size_t bytes)
{
    size_t n = 0, i = 0;
    while (i < bytes) {
        size_t l = utf8_seq_len((unsigned char)s[i]);
        if (l == 0 || i + l > bytes) l = 1;
        i += l;
        n++;
    }
    return n;
}

/* ------------------------------------------------------------------ */
/* Diagnostics                                                        */
/* ------------------------------------------------------------------ */

void pith_emit_diagnostic(const char *severity, const char *message,
                          const char *filepath, const char *source,
                          size_t line, size_t col, size_t span)
{
    int color = stderr_is_tty();
    const char *sev_color = "";
    const char *reset = "";
    const char *blue = "";

    if (color) {
        reset = C_RESET;
        blue = C_BLUE;
        if (strcmp(severity, "error") == 0)
            sev_color = C_RED;
        else if (strcmp(severity, "warning") == 0)
            sev_color = C_YELLOW;
        else
            sev_color = C_BLUE;
    }

    fprintf(stderr, "%s%s%s: %s\n", sev_color, severity, reset, message);

    if (!filepath)
        return;

    fprintf(stderr, "%s  -->%s %s:%zu:%zu\n",
            blue, reset, filepath, line, col);

    if (!source)
        return;

    /* locate the start of the requested line */
    size_t ln = 1, start = 0, idx = 0;
    while (ln < line) {
        if (source[idx] == '\0') { start = idx; break; }
        if (source[idx] == '\n') { ln++; start = idx + 1; }
        idx++;
    }
    size_t end = start;
    while (source[end] && source[end] != '\n') end++;

    /* gutter width = digits of the line number */
    size_t w = 1;
    for (size_t n = line; n >= 10; n /= 10) w++;

    fprintf(stderr, "%s%*s |%s\n", blue, (int)w, "", reset);

    /* source preview with tabs expanded to 4 spaces; simultaneously
       compute the visual offset of the caret column */
    char vis[4096];
    size_t vlen = 0, pad = 0, cp = 1;
    for (size_t i = start; i < end && vlen + 8 < sizeof(vis); ) {
        size_t l = utf8_seq_len((unsigned char)source[i]);
        if (l == 0 || i + l > end) l = 1;
        if (source[i] == '\t') {
            if (cp < col) pad += 4;
            for (size_t k = 0; k < 4; k++) vis[vlen++] = ' ';
        } else {
            if (cp < col) pad += 1;
            for (size_t k = 0; k < l; k++) vis[vlen++] = source[i + k];
        }
        cp++;
        i += l;
    }
    vis[vlen] = '\0';

    fprintf(stderr, "%s%*zu |%s %s\n", blue, (int)w, line, reset, vis);

    fprintf(stderr, "%*s | %s", (int)w, "", sev_color);
    for (size_t k = 0; k < pad; k++) fputc(' ', stderr);
    fputc('^', stderr);
    if (span == 0) span = 1;
    for (size_t k = 1; k < span; k++) fputc('~', stderr);
    fprintf(stderr, "%s\n", reset);
}

/* ------------------------------------------------------------------ */
/* Lexer                                                              */
/* ------------------------------------------------------------------ */

typedef struct {
    const char *filepath;
    const char *source;
    size_t      src_len;
    size_t      pos;       /* byte offset            */
    size_t      line;      /* 1-based                */
    size_t      col;       /* 1-based, codepoints    */
    size_t      errors;
    size_t      warnings;
    TokenList  *out;
} Lexer;

static void lex_error(Lexer *lx, size_t line, size_t col, size_t span,
                      const char *fmt, const char *arg)
{
    char msg[512];
    snprintf(msg, sizeof(msg), fmt, arg ? arg : "");
    pith_emit_diagnostic("error", msg, lx->filepath, lx->source,
                         line, col, span);
    lx->errors++;
}

static void lex_warning(Lexer *lx, size_t line, size_t col, size_t span,
                        const char *fmt, const char *arg)
{
    char msg[512];
    snprintf(msg, sizeof(msg), fmt, arg ? arg : "");
    pith_emit_diagnostic("warning", msg, lx->filepath, lx->source,
                         line, col, span);
    lx->warnings++;
}

static void push_token(Lexer *lx, TokenType type, size_t line, size_t col,
                       size_t offset, char *lexeme /* owned, may be NULL */,
                       size_t lexeme_len, long long iv, double fv)
{
    TokenList *list = lx->out;
    if (list->count == list->capacity) {
        size_t ncap = list->capacity ? list->capacity * 2 : 64;
        Token *nt = realloc(list->tokens, ncap * sizeof(Token));
        if (!nt) {
            fputs("pith: out of memory while lexing\n", stderr);
            exit(1);
        }
        list->tokens = nt;
        list->capacity = ncap;
    }
    Token *t = &list->tokens[list->count++];
    t->type = type;
    t->loc.filepath = lx->filepath;
    t->loc.line = line;
    t->loc.col = col;
    t->loc.offset = offset;
    t->lexeme = lexeme;
    t->lexeme_len = lexeme_len;
    t->int_value = iv;
    t->float_value = fv;
}

static int is_ident_start(unsigned char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

static int is_ident_cont(unsigned char c)
{
    return is_ident_start(c) || (c >= '0' && c <= '9');
}

static char *dup_slice(const char *s, size_t len)
{
    char *out = malloc(len + 1);
    if (!out) {
        fputs("pith: out of memory while lexing\n", stderr);
        exit(1);
    }
    memcpy(out, s, len);
    out[len] = '\0';
    return out;
}

static void lex_number(Lexer *lx)
{
    size_t line = lx->line, col = lx->col, start = lx->pos;
    int is_float = 0;

    while (lx->pos < lx->src_len && lx->source[lx->pos] >= '0' &&
           lx->source[lx->pos] <= '9') {
        lx->pos++;
        lx->col++;
    }
    /* a '.' starts a fractional part only when followed by a digit;
       otherwise it will be scanned as the dot operator (member access) */
    if (lx->pos < lx->src_len && lx->source[lx->pos] == '.' &&
        lx->pos + 1 < lx->src_len && lx->source[lx->pos + 1] >= '0' &&
        lx->source[lx->pos + 1] <= '9') {
        is_float = 1;
        lx->pos++;          /* '.' */
        lx->col++;
        while (lx->pos < lx->src_len && lx->source[lx->pos] >= '0' &&
               lx->source[lx->pos] <= '9') {
            lx->pos++;
            lx->col++;
        }
    }

    size_t len = lx->pos - start;
    char *text = dup_slice(lx->source + start, len);

    if (is_float) {
        char *endp = NULL;
        errno = 0;
        double v = strtod(text, &endp);
        push_token(lx, TOK_FLOAT_LITERAL, line, col, start, text, pith_utf8_len(text, strlen(text)), 0, v);
        (void)endp;
    } else {
        errno = 0;
        long long v = strtoll(text, NULL, 10);
        if (errno == ERANGE) {
            lex_error(lx, line, col, pith_utf8_len(text, len),
                      "integer literal `%s` is out of range", text);
            free(text);
        } else {
            push_token(lx, TOK_INT_LITERAL, line, col, start, text, pith_utf8_len(text, strlen(text)), v, 0.0);
        }
    }
}

static void lex_string(Lexer *lx)
{
    size_t line = lx->line, col = lx->col, start = lx->pos;

    lx->pos++;   /* opening quote */
    lx->col++;

    size_t cap = 32, len = 0;
    char *buf = malloc(cap);
    if (!buf) {
        fputs("pith: out of memory while lexing\n", stderr);
        exit(1);
    }

    for (;;) {
        if (lx->pos >= lx->src_len || lx->source[lx->pos] == '\n') {
            lex_error(lx, line, col, 1, "unterminated string literal", NULL);
            free(buf);
            return;
        }
        unsigned char c = (unsigned char)lx->source[lx->pos];

        if (c == '"') {
            lx->pos++;
            lx->col++;
            break;
        }

        if (c == '\\') {
            lx->pos++;
            lx->col++;
            if (lx->pos >= lx->src_len || lx->source[lx->pos] == '\n') {
                lex_error(lx, line, col, 1, "unterminated string literal",
                          NULL);
                free(buf);
                return;
            }
            char e = lx->source[lx->pos];
            char decoded;
            switch (e) {
            case 'n':  decoded = '\n'; break;
            case 't':  decoded = '\t'; break;
            case 'r':  decoded = '\r'; break;
            case '0':  decoded = '\0'; break;
            case '\\': decoded = '\\'; break;
            case '"':  decoded = '"';  break;
            default:
                lex_error(lx, lx->line, lx->col, 1,
                          "unknown escape sequence `\\%c` in string", &e);
                decoded = e;
                break;
            }
            if (len + 1 >= cap) {
                cap *= 2;
                char *nb = realloc(buf, cap);
                if (!nb) {
                    fputs("pith: out of memory while lexing\n", stderr);
                    exit(1);
                }
                buf = nb;
            }
            buf[len++] = decoded;
            lx->pos++;
            lx->col++;
            continue;
        }

        /* copy one UTF-8 codepoint (or repair an invalid sequence) */
        size_t l = utf8_seq_len(c);
        if (l == 0 || lx->pos + l > lx->src_len) {
            if (c >= 0x80)
                lex_error(lx, lx->line, lx->col, 1,
                          "invalid UTF-8 sequence in string", NULL);
            l = 1;
        }
        if (len + l + 1 > cap) {
            while (len + l + 1 > cap) cap *= 2;
            char *nb = realloc(buf, cap);
            if (!nb) {
                fputs("pith: out of memory while lexing\n", stderr);
                exit(1);
            }
            buf = nb;
        }
        for (size_t k = 0; k < l; k++) buf[len++] = lx->source[lx->pos + k];
        lx->pos += l;
        lx->col++;
    }

    buf[len] = '\0';
    push_token(lx, TOK_STRING_LITERAL, line, col, start, buf, len, 0, 0.0);
}

static void lex_ident(Lexer *lx)
{
    size_t line = lx->line, col = lx->col, start = lx->pos;
    int bad_non_ascii = 0;

    while (lx->pos < lx->src_len) {
        unsigned char c = (unsigned char)lx->source[lx->pos];
        if (is_ident_cont(c)) {
            lx->pos++;
            lx->col++;
        } else if (c >= 0x80) {
            size_t l = utf8_seq_len(c);
            if (l == 0 || lx->pos + l > lx->src_len) l = 1;
            if (!bad_non_ascii) {
                bad_non_ascii = 1;
                lex_error(lx, lx->line, lx->col, 1,
                          "non-ASCII codepoint in identifier "
                          "(identifiers are ASCII lowerCamelCase)", NULL);
            }
            lx->pos += l;
            lx->col++;
        } else {
            break;
        }
    }

    size_t len = lx->pos - start;
    char *text = dup_slice(lx->source + start, len);

    static const char *const keywords[] = {
        "if", "elseif", "else", "end", "print", "fn", "return", "import"
    };
    for (size_t i = 0; i < sizeof(keywords) / sizeof(keywords[0]); i++) {
        if (strcmp(text, keywords[i]) == 0) {
            static const TokenType kw_tok[] = {
                TOK_KW_IF, TOK_KW_ELSEIF, TOK_KW_ELSE, TOK_KW_END,
                TOK_KW_PRINT, TOK_KW_FN, TOK_KW_RETURN, TOK_KW_IMPORT
            };
            push_token(lx, kw_tok[i], line, col, start, text, pith_utf8_len(text, strlen(text)), 0, 0.0);
            return;
        }
    }

    /* lowerCamelCase lint: warn on a capital first letter */
    if (text[0] >= 'A' && text[0] <= 'Z')
        lex_warning(lx, line, col, pith_utf8_len(text, len),
                    "identifier `%s` should be lowerCamelCase "
                    "(starts with a capital letter)", text);

    push_token(lx, TOK_IDENTIFIER, line, col, start, text, pith_utf8_len(text, strlen(text)), 0, 0.0);
}

void pith_lex(const char *filepath, const char *source,
              TokenList *out, size_t *errors, size_t *warnings)
{
    Lexer lx;
    memset(&lx, 0, sizeof(lx));
    lx.filepath = filepath;
    lx.source = source;
    lx.src_len = source ? strlen(source) : 0;
    lx.pos = 0;
    lx.line = 1;
    lx.col = 1;
    lx.out = out;

    /* skip a UTF-8 byte order mark if present */
    if (lx.src_len >= 3 &&
        (unsigned char)source[0] == 0xEF &&
        (unsigned char)source[1] == 0xBB &&
        (unsigned char)source[2] == 0xBF)
        lx.pos = 3;

    for (;;) {
        size_t line = lx.line, col = lx.col, start = lx.pos;

        if (lx.pos >= lx.src_len) {
            push_token(&lx, TOK_EOF, line, col, start, NULL, 0, 0, 0.0);
            break;
        }

        unsigned char c = (unsigned char)lx.source[lx.pos];

        if (c == ' ' || c == '\t' || c == '\f' || c == '\v') {
            lx.pos++;
            lx.col++;
            continue;
        }

        if (c == '\r') {
            if (lx.pos + 1 < lx.src_len && lx.source[lx.pos + 1] == '\n') {
                lx.pos++;       /* CR of CRLF: the LF below delimits */
                continue;
            }
            lex_error(&lx, line, col, 1,
                      "unexpected carriage return (use LF newlines)", NULL);
            lx.pos++;
            lx.col++;
            continue;
        }

        if (c == '\n') {
            lx.pos++;
            lx.line++;
            lx.col = 1;
            push_token(&lx, TOK_NEWLINE, line, col, start, NULL, 0, 0, 0.0);
            continue;
        }

        if (c == '#') {
            while (lx.pos < lx.src_len && lx.source[lx.pos] != '\n')
                lx.pos++;
            continue;
        }

        if (c >= '0' && c <= '9') {
            lex_number(&lx);
            continue;
        }

        if (is_ident_start(c)) {
            lex_ident(&lx);
            continue;
        }

        if (c == '"') {
            lex_string(&lx);
            continue;
        }

        switch (c) {
        case '=':
            if (lx.pos + 1 < lx.src_len && lx.source[lx.pos + 1] == '=') {
                lx.pos += 2; lx.col += 2;
                push_token(&lx, TOK_OP_EQ, line, col, start,
                           dup_slice("==", 2), 2, 0, 0.0);
            } else {
                lx.pos++; lx.col++;
                push_token(&lx, TOK_OP_ASSIGN, line, col, start,
                           dup_slice("=", 1), 1, 0, 0.0);
            }
            continue;
        case '!':
            if (lx.pos + 1 < lx.src_len && lx.source[lx.pos + 1] == '=') {
                lx.pos += 2; lx.col += 2;
                push_token(&lx, TOK_OP_NE, line, col, start,
                           dup_slice("!=", 2), 2, 0, 0.0);
            } else {
                lex_error(&lx, line, col, 1,
                          "unexpected character `!` "
                          "(did you mean `!=`?)", NULL);
                lx.pos++; lx.col++;
            }
            continue;
        case '<':
            if (lx.pos + 1 < lx.src_len && lx.source[lx.pos + 1] == '=') {
                lx.pos += 2; lx.col += 2;
                push_token(&lx, TOK_OP_LE, line, col, start,
                           dup_slice("<=", 2), 2, 0, 0.0);
            } else {
                lx.pos++; lx.col++;
                push_token(&lx, TOK_OP_LT, line, col, start,
                           dup_slice("<", 1), 1, 0, 0.0);
            }
            continue;
        case '>':
            if (lx.pos + 1 < lx.src_len && lx.source[lx.pos + 1] == '=') {
                lx.pos += 2; lx.col += 2;
                push_token(&lx, TOK_OP_GE, line, col, start,
                           dup_slice(">=", 2), 2, 0, 0.0);
            } else {
                lx.pos++; lx.col++;
                push_token(&lx, TOK_OP_GT, line, col, start,
                           dup_slice(">", 1), 1, 0, 0.0);
            }
            continue;
        case '+':
            lx.pos++; lx.col++;
            push_token(&lx, TOK_OP_PLUS, line, col, start,
                       dup_slice("+", 1), 1, 0, 0.0);
            continue;
        case '-':
            lx.pos++; lx.col++;
            push_token(&lx, TOK_OP_MINUS, line, col, start,
                       dup_slice("-", 1), 1, 0, 0.0);
            continue;
        case '*':
            lx.pos++; lx.col++;
            push_token(&lx, TOK_OP_STAR, line, col, start,
                       dup_slice("*", 1), 1, 0, 0.0);
            continue;
        case '/':
            lx.pos++; lx.col++;
            push_token(&lx, TOK_OP_SLASH, line, col, start,
                       dup_slice("/", 1), 1, 0, 0.0);
            continue;
        case '.':
            lx.pos++; lx.col++;
            push_token(&lx, TOK_OP_DOT, line, col, start,
                       dup_slice(".", 1), 1, 0, 0.0);
            continue;
        case '(':
            lx.pos++; lx.col++;
            push_token(&lx, TOK_OP_LPAREN, line, col, start,
                       dup_slice("(", 1), 1, 0, 0.0);
            continue;
        case ')':
            lx.pos++; lx.col++;
            push_token(&lx, TOK_OP_RPAREN, line, col, start,
                       dup_slice(")", 1), 1, 0, 0.0);
            continue;
        case ',':
            lx.pos++; lx.col++;
            push_token(&lx, TOK_OP_COMMA, line, col, start,
                       dup_slice(",", 1), 1, 0, 0.0);
            continue;
        default:
            if (c >= 0x80) {
                size_t l = utf8_seq_len(c);
                if (l == 0 || lx.pos + l > lx.src_len) l = 1;
                lex_error(&lx, line, col, 1,
                          "unexpected codepoint outside a string or "
                          "comment", NULL);
                lx.pos += l;
                lx.col++;
            } else {
                char shown[2] = { (char)c, '\0' };
                lex_error(&lx, line, col, 1, "unexpected character `%c`",
                          shown);
                lx.pos++;
                lx.col++;
            }
            continue;
        }
    }

    if (errors)
        *errors = lx.errors;
    if (warnings)
        *warnings = lx.warnings;
}

void pith_token_list_free(TokenList *list)
{
    if (!list)
        return;
    for (size_t i = 0; i < list->count; i++)
        free(list->tokens[i].lexeme);
    free(list->tokens);
    list->tokens = NULL;
    list->count = 0;
    list->capacity = 0;
}
