/*
 * parser.c — recursive descent parser for The Pith Programming Language
 *
 * Bracketless blocks close with a single `end`; newlines separate
 * statements. A lexical scope stack decides whether `name = expr` is a
 * declaration (the name exists nowhere yet) or a reassignment (the name
 * exists in the current or a parent scope).
 */
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../include/compiler.h"

#define PITH_MAX_EXPR_DEPTH 128
#define PITH_MAX_BLOCK_DEPTH 256

/* ------------------------------------------------------------------ */
/* Parser state                                                       */
/* ------------------------------------------------------------------ */

typedef struct {
    TokenList   tokens;
    size_t      pos;        /* index into tokens.tokens              */
    const char *filepath;
    const char *source;
    Scope      *scope;      /* lexical scope stack                   */
    Arena      *arena;      /* bump arena owning all allocations     */
    size_t      errors;
    int         depth;      /* expression nesting guard              */
    int         block_depth;/* block nesting guard (C stack safety)  */
} Parser;

/* ------------------------------------------------------------------ */
/* Bump arena                                                         */
/* ------------------------------------------------------------------ */

void *pith_arena_alloc(Arena *a, size_t n)
{
    if (!a)
        return NULL;
    n = (n + 15) & ~(size_t)15;   /* keep 16-byte alignment */
    if (!a->head || a->tail->used + n > a->tail->cap) {
        size_t cap = 16384;
        if (n > cap)
            cap = n;
        ArenaBlock *b = malloc(sizeof(ArenaBlock) + cap);
        if (!b) {
            fputs("pith: out of memory while parsing\n", stderr);
            exit(1);
        }
        b->next = NULL;
        b->used = 0;
        b->cap = cap;
        b->reserved = 0;
        if (a->tail)
            a->tail->next = b;
        else
            a->head = b;
        a->tail = b;
    }
    void *ptr = (char *)(a->tail + 1) + a->tail->used;
    a->tail->used += n;
    return ptr;
}

void pith_arena_free(Arena *a)
{
    if (!a)
        return;
    ArenaBlock *b = a->head;
    while (b) {
        ArenaBlock *next = b->next;
        free(b);
        b = next;
    }
    a->head = NULL;
    a->tail = NULL;
}

static char *arena_strdup(Arena *a, const char *s)
{
    size_t n = strlen(s) + 1;
    char *out = pith_arena_alloc(a, n);
    if (out)
        memcpy(out, s, n);
    return out;
}

/* Copy exactly `n` bytes plus a NUL sentinel (embedded NULs kept). */
static char *arena_strdup_bytes(Arena *a, const char *s, size_t n)
{
    char *out = pith_arena_alloc(a, n + 1);
    if (out) {
        if (n)
            memcpy(out, s, n);
        out[n] = '\0';
    }
    return out;
}

/* ------------------------------------------------------------------ */
/* Scopes                                                             */
/* ------------------------------------------------------------------ */

static Scope *scope_push(Parser *p)
{
    Scope *s = pith_arena_alloc(p->arena, sizeof(Scope));
    if (!s) {
        fputs("pith: out of memory while parsing\n", stderr);
        exit(1);
    }
    memset(s, 0, sizeof(*s));
    s->parent = p->scope;
    p->scope = s;
    return s;
}

static void scope_pop(Parser *p)
{
    /* entries are arena-owned; nothing to free individually */
    if (p->scope)
        p->scope = p->scope->parent;
}

static ScopeVar *scope_lookup(Parser *p, const char *name)
{
    for (Scope *s = p->scope; s; s = s->parent) {
        for (ScopeVar *v = s->vars; v; v = v->next)
            if (strcmp(v->name, name) == 0)
                return v;
    }
    return NULL;
}

/* Register `name` in the innermost scope; returns the new entry. */
static ScopeVar *scope_declare(Parser *p, const char *name)
{
    ScopeVar *v = pith_arena_alloc(p->arena, sizeof(ScopeVar));
    if (!v) {
        fputs("pith: out of memory while parsing\n", stderr);
        exit(1);
    }
    memset(v, 0, sizeof(*v));
    v->name = arena_strdup(p->arena, name);
    if (!v->name) {
        fputs("pith: out of memory while parsing\n", stderr);
        exit(1);
    }
    v->var_type = PITH_VALUE_ERROR;
    v->is_arc = false;
    v->slot = 0;
    v->next = p->scope->vars;
    p->scope->vars = v;
    return v;
}

/* ------------------------------------------------------------------ */
/* Diagnostics                                                        */
/* ------------------------------------------------------------------ */

static void parse_error_tok(Parser *p, const Token *t, size_t span,
                            const char *fmt, const char *arg)
{
    char msg[512];
    snprintf(msg, sizeof(msg), fmt, arg ? arg : "");
    pith_emit_diagnostic("error", msg, p->filepath, p->source,
                         t ? t->loc.line : 1, t ? t->loc.col : 1, span);
    p->errors++;
}

static const char *tok_kind_name(TokenType type)
{
    switch (type) {
    case TOK_KW_IF:      return "`if`";
    case TOK_KW_ELSEIF:  return "`elseif`";
    case TOK_KW_ELSE:    return "`else`";
    case TOK_KW_END:     return "`end`";
    case TOK_KW_PRINT:   return "`print`";
    case TOK_KW_FN:      return "`fn`";
    case TOK_KW_RETURN:  return "`return`";
    case TOK_IDENTIFIER: return "an identifier";
    case TOK_INT_LITERAL:  return "an integer literal";
    case TOK_FLOAT_LITERAL: return "a float literal";
    case TOK_STRING_LITERAL: return "a string literal";
    case TOK_NEWLINE:    return "end of line";
    case TOK_EOF:        return "end of file";
    case TOK_OP_ASSIGN:  return "`=`";
    case TOK_OP_EQ:      return "`==`";
    case TOK_OP_NE:      return "`!=`";
    case TOK_OP_DOT:     return "`.`";
    default:             return "an operator";
    }
}

/* ------------------------------------------------------------------ */
/* Token stream helpers                                               */
/* ------------------------------------------------------------------ */

static const Token *peek(Parser *p)
{
    return &p->tokens.tokens[p->pos];
}

static const Token *peek_at(Parser *p, size_t ahead)
{
    size_t i = p->pos + ahead;
    if (i >= p->tokens.count)
        i = p->tokens.count - 1;   /* EOF sentinel */
    return &p->tokens.tokens[i];
}

static const Token *advance(Parser *p)
{
    const Token *t = peek(p);
    if (t->type != TOK_EOF)
        p->pos++;
    return t;
}

static int at(Parser *p, TokenType type)
{
    return peek(p)->type == type;
}

static const Token *expect(Parser *p, TokenType type, const char *what)
{
    if (at(p, type))
        return advance(p);
    parse_error_tok(p, peek(p), 1, "expected %s, found %s",
                    what ? what : tok_kind_name(type));
    return NULL;
}

/* Skip tokens until a newline (consumed) or a block terminator / EOF. */
static void sync_to_newline(Parser *p)
{
    while (!at(p, TOK_EOF) && !at(p, TOK_NEWLINE) &&
           !at(p, TOK_KW_ELSEIF) && !at(p, TOK_KW_ELSE) &&
           !at(p, TOK_KW_END))
        advance(p);
    if (at(p, TOK_NEWLINE))
        advance(p);
}

/* ------------------------------------------------------------------ */
/* AST construction helpers                                           */
/* ------------------------------------------------------------------ */

static ASTNode *node_new(Parser *p, ASTNodeType type, SourceLoc loc)
{
    ASTNode *n = pith_arena_alloc(p->arena, sizeof(ASTNode));
    if (!n) {
        fputs("pith: out of memory while parsing\n", stderr);
        exit(1);
    }
    memset(n, 0, sizeof(*n));
    n->type = type;
    n->loc = loc;
    return n;
}

static void block_append(Parser *p, ASTBlock *b, ASTNode *n)
{
    if (b->count == b->capacity) {
        /* arena growth: allocate a fresh array and copy (the old
           allocation is absorbed by the bump arena) */
        size_t ncap = b->capacity ? b->capacity * 2 : 8;
        ASTNode **ns = pith_arena_alloc(p->arena, ncap * sizeof(ASTNode *));
        if (!ns) {
            fputs("pith: out of memory while parsing\n", stderr);
            exit(1);
        }
        for (size_t i = 0; i < b->count; i++)
            ns[i] = b->stmts[i];
        b->stmts = ns;
        b->capacity = ncap;
    }
    b->stmts[b->count++] = n;
}

static ASTBlock *block_new(Parser *p)
{
    ASTBlock *b = pith_arena_alloc(p->arena, sizeof(ASTBlock));
    if (!b) {
        fputs("pith: out of memory while parsing\n", stderr);
        exit(1);
    }
    memset(b, 0, sizeof(*b));
    return b;
}

/* ------------------------------------------------------------------ */
/* Expressions                                                        */
/* ------------------------------------------------------------------ */

static ASTNode *parse_expression(Parser *p);

static ASTNode *parse_primary(Parser *p)
{
    const Token *t = peek(p);
    ASTNode *node;

    switch (t->type) {
    case TOK_INT_LITERAL:
        advance(p);
        node = node_new(p, AST_INT_EXPR, t->loc);
        node->as.int_literal = t->int_value;
        return node;
    case TOK_FLOAT_LITERAL:
        advance(p);
        node = node_new(p, AST_FLOAT_EXPR, t->loc);
        node->as.float_literal = t->float_value;
        return node;
    case TOK_STRING_LITERAL:
        advance(p);
        node = node_new(p, AST_STRING_EXPR, t->loc);
        node->as.string_literal = arena_strdup_bytes(p->arena, t->lexeme,
                                                     t->lexeme_len);
        if (!node->as.string_literal) {
            fputs("pith: out of memory while parsing\n", stderr);
            exit(1);
        }
        node->string_len = t->lexeme_len;
        return node;
    case TOK_IDENTIFIER:
        advance(p);
        node = node_new(p, AST_IDENTIFIER_EXPR, t->loc);
        node->as.identifier = arena_strdup(p->arena, t->lexeme);
        if (!node->as.identifier) {
            fputs("pith: out of memory while parsing\n", stderr);
            exit(1);
        }
        return node;
    case TOK_OP_LPAREN: {
        advance(p);
        if (++p->depth > PITH_MAX_EXPR_DEPTH) {
            parse_error_tok(p, t, 1,
                            "expression is nested too deeply", NULL);
            p->depth--;
            return node_new(p, AST_INT_EXPR, t->loc);
        }
        ASTNode *inner = parse_expression(p);
        p->depth--;
        if (!expect(p, TOK_OP_RPAREN, "`)`")) {
            sync_to_newline(p);
            return inner ? inner : node_new(p, AST_INT_EXPR, t->loc);
        }
        return inner;
    }
    default:
        parse_error_tok(p, t, 1, "expected an expression, found %s",
                        tok_kind_name(t->type));
        advance(p);   /* make progress */
        return NULL;
    }
}

static ASTNode *parse_postfix(Parser *p)
{
    ASTNode *node = parse_primary(p);
    if (!node)
        return NULL;

    while (at(p, TOK_OP_DOT)) {
        const Token *dot = advance(p);
        const Token *member = expect(p, TOK_IDENTIFIER, "a member name");
        if (!member) {
            sync_to_newline(p);
            return node;
        }
        (void)dot;
        ASTNode *access = node_new(p, AST_MEMBER_ACCESS, member->loc);
        access->as.member_access.base = node;
        access->as.member_access.member = arena_strdup(p->arena,
                                                       member->lexeme);
        if (!access->as.member_access.member) {
            fputs("pith: out of memory while parsing\n", stderr);
            exit(1);
        }
        node = access;
    }

    /* call suffix: callee(arg, ...) — possibly chained */
    while (at(p, TOK_OP_LPAREN)) {
        const Token *lp = advance(p);

        ASTNode **args = NULL;
        size_t count = 0, cap = 0;

        if (!at(p, TOK_OP_RPAREN)) {
            for (;;) {
                ASTNode *arg = parse_expression(p);
                if (!arg) {
                    sync_to_newline(p);
                    return NULL;
                }
                if (count == cap) {
                    /* arena growth: fresh array + copy */
                    size_t ncap = cap ? cap * 2 : 4;
                    ASTNode **na = pith_arena_alloc(p->arena,
                                                     ncap *
                                                     sizeof(ASTNode *));
                    if (!na) {
                        fputs("pith: out of memory while parsing\n",
                              stderr);
                        exit(1);
                    }
                    for (size_t i = 0; i < count; i++)
                        na[i] = args[i];
                    args = na;
                    cap = ncap;
                }
                args[count++] = arg;

                if (at(p, TOK_OP_COMMA)) {
                    advance(p);
                    continue;
                }
                break;
            }
        }
        if (!expect(p, TOK_OP_RPAREN, "`)`")) {
            sync_to_newline(p);
            return NULL;
        }

        ASTNode *call = node_new(p, AST_CALL_EXPR, lp->loc);
        call->as.call.callee = node;
        call->as.call.args = args;
        call->as.call.arg_count = count;
        node = call;
    }
    return node;
}

static ASTNode *parse_unary(Parser *p)
{
    if (at(p, TOK_OP_MINUS)) {
        const Token *op = advance(p);
        ASTNode *operand = parse_unary(p);
        if (!operand)
            return NULL;

        /* fold negation into numeric literals */
        if (operand->type == AST_INT_EXPR) {
            if (operand->as.int_literal == LLONG_MIN) {
                parse_error_tok(p, op, 1,
                                "integer literal is too large to negate",
                                NULL);
                return operand;
            }
            operand->as.int_literal = -operand->as.int_literal;
            return operand;
        }
        if (operand->type == AST_FLOAT_EXPR) {
            operand->as.float_literal = -operand->as.float_literal;
            return operand;
        }

        ASTNode *n = node_new(p, AST_UNARY_OP, op->loc);
        n->as.unary_op.operand = operand;
        return n;
    }
    return parse_postfix(p);
}

static ASTNode *binary_join(Parser *p, const Token *op,
                            ASTNode *left, ASTNode *right)
{
    if (!left || !right) {
        /* one side already failed; the partial nodes are absorbed by
           the bump arena and freed wholesale */
        return NULL;
    }
    ASTNode *n = node_new(p, AST_BINARY_OP, op->loc);
    n->as.binary_op.left = left;
    n->as.binary_op.right = right;
    n->as.binary_op.op = op->type;
    return n;
}

static ASTNode *parse_multiplicative(Parser *p)
{
    ASTNode *left = parse_unary(p);
    while (left && (at(p, TOK_OP_STAR) || at(p, TOK_OP_SLASH))) {
        const Token *op = advance(p);
        ASTNode *right = parse_unary(p);
        left = binary_join(p, op, left, right);
    }
    return left;
}

static ASTNode *parse_additive(Parser *p)
{
    ASTNode *left = parse_multiplicative(p);
    while (left && (at(p, TOK_OP_PLUS) || at(p, TOK_OP_MINUS))) {
        const Token *op = advance(p);
        ASTNode *right = parse_multiplicative(p);
        left = binary_join(p, op, left, right);
    }
    return left;
}

static ASTNode *parse_comparison(Parser *p)
{
    ASTNode *left = parse_additive(p);
    while (left && (at(p, TOK_OP_LT) || at(p, TOK_OP_LE) ||
                    at(p, TOK_OP_GT) || at(p, TOK_OP_GE))) {
        const Token *op = advance(p);
        ASTNode *right = parse_additive(p);
        left = binary_join(p, op, left, right);
    }
    return left;
}

static ASTNode *parse_equality(Parser *p)
{
    ASTNode *left = parse_comparison(p);
    while (left && (at(p, TOK_OP_EQ) || at(p, TOK_OP_NE))) {
        const Token *op = advance(p);
        ASTNode *right = parse_comparison(p);
        left = binary_join(p, op, left, right);
    }
    return left;
}

static ASTNode *parse_expression(Parser *p)
{
    if (++p->depth > PITH_MAX_EXPR_DEPTH) {
        parse_error_tok(p, peek(p), 1, "expression is nested too deeply",
                        NULL);
        p->depth--;
        return NULL;
    }
    ASTNode *n = parse_equality(p);
    p->depth--;
    return n;
}

/* ------------------------------------------------------------------ */
/* Statements                                                         */
/* ------------------------------------------------------------------ */

static ASTNode *parse_statement(Parser *p, int top_level);

static ASTNode *parse_assignment(Parser *p)
{
    const Token *name = advance(p);   /* TOK_IDENTIFIER */
    if (!expect(p, TOK_OP_ASSIGN, "`=`"))
        return NULL;

    ASTNode *value = parse_expression(p);
    if (!value) {
        sync_to_newline(p);
        return NULL;
    }

    /* declaration vs reassignment: if the name exists in the current or
       a parent scope this is a reassignment; otherwise it declares the
       variable in the current scope. The RHS is parsed first, so
       `x = x` on an undeclared x reports the use of `x`. */
    bool is_decl = (scope_lookup(p, name->lexeme) == NULL);
    if (is_decl) {
        ScopeVar *v = scope_declare(p, name->lexeme);
        if (strcmp(name->lexeme, "os") == 0)
            pith_emit_diagnostic("warning",
                                 "variable `os` shadows the builtin os "
                                 "namespace",
                                 p->filepath, p->source,
                                 name->loc.line, name->loc.col,
                                 strlen(name->lexeme));
        (void)v;
    }

    ASTNode *n = node_new(p, AST_ASSIGNMENT, name->loc);
    n->as.assignment.var_name = arena_strdup(p->arena, name->lexeme);
    if (!n->as.assignment.var_name) {
        fputs("pith: out of memory while parsing\n", stderr);
        exit(1);
    }
    n->as.assignment.value = value;
    n->as.assignment.is_declaration = is_decl;
    return n;
}

static ASTBlock *parse_block(Parser *p);

static ASTNode *parse_if(Parser *p)
{
    const Token *kw = advance(p);   /* TOK_KW_IF */

    ASTNode *cond = parse_expression(p);
    if (!cond) {
        sync_to_newline(p);
        return NULL;
    }
    if (!expect(p, TOK_NEWLINE, "end of line after the if condition")) {
        sync_to_newline(p);
    }

    ASTNode *n = node_new(p, AST_IF_STMT, kw->loc);
    n->as.if_stmt.then_branch.condition = cond;
    n->as.if_stmt.then_branch.block = parse_block(p);
    n->as.if_stmt.elseif_branches = NULL;
    n->as.if_stmt.elseif_count = 0;
    n->as.if_stmt.has_else = false;

    while (at(p, TOK_KW_ELSEIF)) {
        const Token *ekw = advance(p);
        ASTNode *econd = parse_expression(p);
        if (!econd) {
            sync_to_newline(p);
        }
        if (!expect(p, TOK_NEWLINE,
                    "end of line after the elseif condition")) {
            sync_to_newline(p);
        }
        /* arena growth: fresh array + copy (the old allocation is
           absorbed by the bump arena) */
        size_t idx = n->as.if_stmt.elseif_count;
        size_t ncount = idx + 1;
        ASTIfBranch *nbrs = pith_arena_alloc(p->arena,
                                             ncount * sizeof(ASTIfBranch));
        if (!nbrs) {
            fputs("pith: out of memory while parsing\n", stderr);
            exit(1);
        }
        for (size_t k = 0; k < idx; k++)
            nbrs[k] = n->as.if_stmt.elseif_branches[k];
        n->as.if_stmt.elseif_branches = nbrs;
        ASTIfBranch *br = &nbrs[idx];
        br->condition = econd;
        br->block = parse_block(p);
        n->as.if_stmt.elseif_count = ncount;
        (void)ekw;
    }

    if (at(p, TOK_KW_ELSE)) {
        advance(p);
        if (!expect(p, TOK_NEWLINE, "end of line after `else`"))
            sync_to_newline(p);
        n->as.if_stmt.has_else = true;
        n->as.if_stmt.else_branch.condition = NULL;
        n->as.if_stmt.else_branch.block = parse_block(p);
    }

    if (!expect(p, TOK_KW_END, "`end` to close the if block"))
        sync_to_newline(p);

    return n;
}

static ASTNode *parse_print(Parser *p)
{
    const Token *kw = advance(p);   /* TOK_KW_PRINT */
    ASTNode *value = parse_expression(p);
    if (!value) {
        sync_to_newline(p);
        return NULL;
    }
    ASTNode *n = node_new(p, AST_PRINT_STMT, kw->loc);
    n->as.print_stmt.expression = value;
    return n;
}

static ASTNode *parse_return(Parser *p)
{
    const Token *kw = advance(p);   /* TOK_KW_RETURN */

    ASTNode *n = node_new(p, AST_RETURN_STMT, kw->loc);
    n->as.return_stmt.value = NULL;
    n->as.return_stmt.has_value = false;

    /* `return` may stand alone; a value expression is optional */
    TokenType next = peek(p)->type;
    if (next != TOK_NEWLINE && next != TOK_EOF &&
        next != TOK_KW_ELSEIF && next != TOK_KW_ELSE && next != TOK_KW_END) {
        ASTNode *value = parse_expression(p);
        if (value) {
            n->as.return_stmt.value = value;
            n->as.return_stmt.has_value = true;
        } else {
            sync_to_newline(p);
        }
    }
    return n;
}

static ASTNode *parse_fn_decl(Parser *p)
{
    const Token *kw = advance(p);   /* TOK_KW_FN */
    const Token *name = expect(p, TOK_IDENTIFIER, "a function name");
    if (!name)
        return NULL;
    if (!expect(p, TOK_NEWLINE, "end of line after the function name"))
        sync_to_newline(p);

    ASTNode *n = node_new(p, AST_FN_DECL, kw->loc);
    n->as.fn_decl.name = arena_strdup(p->arena, name->lexeme);
    if (!n->as.fn_decl.name) {
        fputs("pith: out of memory while parsing\n", stderr);
        exit(1);
    }
    n->as.fn_decl.body = parse_block(p);
    if (!expect(p, TOK_KW_END, "`end` to close the fn block"))
        sync_to_newline(p);
    return n;
}

/* import "path.c" — native C import (top level only) */
static ASTNode *parse_import(Parser *p)
{
    const Token *kw = advance(p);   /* TOK_KW_IMPORT */
    const Token *path = expect(p, TOK_STRING_LITERAL,
                               "a C source path string literal");
    if (!path)
        return NULL;

    ASTNode *n = node_new(p, AST_IMPORT_STMT, kw->loc);
    n->as.import_stmt.path = arena_strdup_bytes(p->arena, path->lexeme,
                                                path->lexeme_len);
    if (!n->as.import_stmt.path) {
        fputs("pith: out of memory while parsing\n", stderr);
        exit(1);
    }
    return n;
}

/*
 * Parses statements until a block terminator (`elseif`, `else`, `end`)
 * or EOF. Newlines between statements are consumed; two statements on
 * one line produce a diagnostic.
 */
static ASTBlock *parse_block(Parser *p)
{
    /* block nesting guard: parse_block/parse_if recurse in C, so
       pathological nesting must be rejected, not stack-overflowed */
    if (++p->block_depth > PITH_MAX_BLOCK_DEPTH) {
        parse_error_tok(p, peek(p), 1, "block nesting is too deeply "
                                        "nested", NULL);
        p->block_depth--;
        sync_to_newline(p);
        return block_new(p);
    }

    ASTBlock *block = block_new(p);
    scope_push(p);

    for (;;) {
        while (at(p, TOK_NEWLINE))
            advance(p);

        if (at(p, TOK_EOF) || at(p, TOK_KW_ELSEIF) ||
            at(p, TOK_KW_ELSE) || at(p, TOK_KW_END))
            break;

        ASTNode *stmt = parse_statement(p, 0);
        if (stmt)
            block_append(p, block, stmt);

        if (at(p, TOK_NEWLINE)) {
            advance(p);
        } else if (!at(p, TOK_EOF) && !at(p, TOK_KW_ELSEIF) &&
                   !at(p, TOK_KW_ELSE) && !at(p, TOK_KW_END)) {
            parse_error_tok(p, peek(p), 1,
                            "expected end of line between statements, "
                            "found %s",
                            tok_kind_name(peek(p)->type));
            sync_to_newline(p);
        }
    }

    scope_pop(p);
    p->block_depth--;
    return block;
}

static ASTNode *parse_statement(Parser *p, int top_level)
{
    const Token *t = peek(p);

    switch (t->type) {
    case TOK_KW_IF:
        return parse_if(p);
    case TOK_KW_PRINT:
        return parse_print(p);
    case TOK_KW_RETURN:
        return parse_return(p);
    case TOK_KW_FN:
        if (!top_level) {
            parse_error_tok(p, t, 1,
                            "functions must be declared at the top level",
                            NULL);
            sync_to_newline(p);
            return NULL;
        }
        return parse_fn_decl(p);
    case TOK_KW_IMPORT:
        if (!top_level) {
            parse_error_tok(p, t, 1,
                            "imports must be declared at the top level",
                            NULL);
            sync_to_newline(p);
            return NULL;
        }
        return parse_import(p);
    case TOK_IDENTIFIER:
        if (peek_at(p, 1)->type == TOK_OP_ASSIGN)
            return parse_assignment(p);
        {
            /* a bare call may be used as a statement: ns.fn(args) */
            ASTNode *expr = parse_expression(p);
            if (!expr)
                return NULL;
            if (expr->type != AST_CALL_EXPR) {
                parse_error_tok(p, t, strlen(t->lexeme),
                                "bare expressions are not statements "
                                "(only calls are)", NULL);
                return NULL;
            }
            ASTNode *n = node_new(p, AST_EXPR_STMT, t->loc);
            n->as.expr_stmt.expr = expr;
            return n;
        }
    default:
        parse_error_tok(p, t, 1, "expected a statement, found %s",
                        tok_kind_name(t->type));
        sync_to_newline(p);
        return NULL;
    }
}

/* ------------------------------------------------------------------ */
/* Entry point                                                        */
/* ------------------------------------------------------------------ */

ASTBlock *pith_parse(const char *filepath, const char *source,
                     TokenList *tokens, size_t *errors, Arena *arena)
{
    Parser p;
    memset(&p, 0, sizeof(p));
    p.tokens = *tokens;
    p.pos = 0;
    p.filepath = filepath;
    p.source = source;
    p.scope = NULL;
    p.arena = arena;
    p.depth = 0;

    scope_push(&p);   /* the script's top-level scope */

    ASTBlock *program = block_new(&p);
    for (;;) {
        while (at(&p, TOK_NEWLINE))
            advance(&p);
        if (at(&p, TOK_EOF))
            break;

        if (at(&p, TOK_KW_FN)) {
            ASTNode *stmt = parse_fn_decl(&p);
            if (stmt)
                block_append(&p, program, stmt);
        } else if (at(&p, TOK_KW_IMPORT)) {
            ASTNode *stmt = parse_import(&p);
            if (stmt)
                block_append(&p, program, stmt);
        } else {
            ASTNode *stmt = parse_statement(&p, 1);
            if (stmt)
                block_append(&p, program, stmt);
        }

        if (at(&p, TOK_NEWLINE)) {
            advance(&p);
        } else if (!at(&p, TOK_EOF)) {
            parse_error_tok(&p, peek(&p), 1,
                            "expected end of line between statements, "
                            "found %s",
                            tok_kind_name(peek(&p)->type));
            sync_to_newline(&p);
        }
    }

    scope_pop(&p);

    if (errors)
        *errors = p.errors;
    return program;
}
