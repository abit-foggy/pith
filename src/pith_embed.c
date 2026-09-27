/*
 * pith_embed.c - implementation of the embeddable C ABI host interface
 * (include/pith_embed.h).
 *
 * A PithContext holds host-registered function pointers. Evaluating a
 * source string runs the standard pipeline - lex, parse (bump arena),
 * QBE lowering, qbe, in-memory libtcc - with the context's functions
 * registered into the engine alongside the runtime ABI.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../include/pith_embed.h"
#include "../include/compiler.h"

typedef struct {
    char  name[128];
    void *addr;
} EmbedSym;

struct PithContext {
    EmbedSym *syms;
    size_t    count;
    size_t    cap;
};

PithContext *pith_context_new(void)
{
    PithContext *ctx = calloc(1, sizeof(PithContext));
    if (!ctx)
        return NULL;
    return ctx;
}

void pith_context_free(PithContext *ctx)
{
    if (!ctx)
        return;
    free(ctx->syms);
    free(ctx);
}

void pith_register_fn(PithContext *ctx, const char *name, void *fn_ptr)
{
    if (!ctx || !name)
        return;
    if (ctx->count == ctx->cap) {
        size_t ncap = ctx->cap ? ctx->cap * 2 : 16;
        EmbedSym *ns = realloc(ctx->syms, ncap * sizeof(EmbedSym));
        if (!ns)
            return;
        ctx->syms = ns;
        ctx->cap = ncap;
    }
    EmbedSym *s = &ctx->syms[ctx->count++];
    snprintf(s->name, sizeof(s->name), "%s", name);
    s->addr = fn_ptr;
}

int pith_eval_string(PithContext *ctx, const char *source)
{
    if (!source)
        return 1;

    const char *name = "<embedded>";

    TokenList tokens;
    memset(&tokens, 0, sizeof(tokens));

    size_t lex_errors = 0, lex_warnings = 0;
    pith_lex(name, source, &tokens, &lex_errors, &lex_warnings);
    if (lex_errors > 0) {
        if (lex_warnings > 0)
            fprintf(stderr, "%zu warning%s emitted\n",
                    lex_warnings, lex_warnings == 1 ? "" : "s");
        fprintf(stderr, "error: aborting due to %zu previous error%s\n",
                lex_errors, lex_errors == 1 ? "" : "s");
        pith_token_list_free(&tokens);
        return 1;
    }

    Arena arena;
    memset(&arena, 0, sizeof(arena));

    size_t parse_errors = 0;
    ASTBlock *program = pith_parse(name, source, &tokens, &parse_errors,
                                   &arena);
    pith_token_list_free(&tokens);
    if (parse_errors > 0 || !program) {
        fprintf(stderr, "error: aborting due to %zu previous error%s\n",
                parse_errors, parse_errors == 1 ? "" : "s");
        pith_arena_free(&arena);
        return 1;
    }

    size_t gen_errors = 0;
    char *ssa = pith_gen_qbe(&program, 1, (const char **)&name, &source,
                             NULL, 0, NULL, &gen_errors);
    pith_arena_free(&arena);
    if (gen_errors > 0 || !ssa) {
        fprintf(stderr, "error: aborting due to %zu previous error%s\n",
                gen_errors, gen_errors == 1 ? "" : "s");
        return 1;
    }

    char ssa_path[4096], asm_path[4096];
    if (pith_stage_temp(ssa, ".ssa", ssa_path, sizeof(ssa_path)) != 0) {
        free(ssa);
        return 1;
    }

    char *asm_src = pith_qbe_lower(ssa_path);
    free(ssa);
    if (!asm_src) {
        unlink(ssa_path);
        return 1;
    }

    if (pith_stage_temp(asm_src, ".s", asm_path, sizeof(asm_path)) != 0) {
        free(asm_src);
        unlink(ssa_path);
        return 1;
    }

    /* host-registered functions join the runtime ABI in the engine */
    RuntimeSymbol *extra = NULL;
    size_t nextra = 0;
    if (ctx && ctx->count > 0) {
        extra = malloc(ctx->count * sizeof(RuntimeSymbol));
        if (extra) {
            for (size_t i = 0; i < ctx->count; i++) {
                extra[i].name = ctx->syms[i].name;
                extra[i].addr = ctx->syms[i].addr;
            }
            nextra = ctx->count;
        }
    }

    int rc = engine_dispatch_run(asm_src, asm_path, extra, nextra,
                                 NULL, 0);
    free(extra);

    free(asm_src);
    unlink(ssa_path);
    unlink(asm_path);

    return rc == -1 ? 1 : rc;
}
