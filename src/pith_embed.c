/*
 * pith_embed.c - implementation of the embeddable C ABI host interface
 * (include/pith_embed.h).
 *
 * A PithContext holds:
 *
 *   - host-registered runtime symbols: { name, address } pairs the
 *     engine registers into every execution alongside the runtime ABI
 *   - typed namespace units: the callable surface. Registration with
 *     pith_register_ns_fn builds PithImportUnit records (author "",
 *     one per namespace) that pith_eval_string hands to the code
 *     generator, so the frontend resolves `ns.fn(...)` through the
 *     standard FFI machinery and emits typed calls to the mangled
 *     c_<ns>_<fn> symbols; the matching addresses are registered as
 *     runtime symbols for the in-memory backend
 *   - fallback link objects: object files engine_dispatch_run links
 *     into the temporary executable when in-memory execution is
 *     unavailable, so host functions resolve on every platform
 */
#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../include/pith_embed.h"
#include "../include/compiler.h"

typedef struct {
    char  name[192];                /* wide enough for mangled names */
    void *addr;
} EmbedSym;

struct PithContext {
    EmbedSym *syms;
    size_t    count;
    size_t    cap;

    PithImportUnit *units;         /* namespaces seen by the frontend */
    size_t    nunits;
    size_t    cap_units;

    char    **link_objs;            /* fallback-link object paths */
    size_t    nlink;
    size_t    cap_link;
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
    free(ctx->units);
    for (size_t i = 0; i < ctx->nlink; i++)
        free(ctx->link_objs[i]);
    free(ctx->link_objs);
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

/* ------------------------------------------------------------------ */
/* Typed namespace registration                                       */
/* ------------------------------------------------------------------ */

static int sym_push(PithContext *ctx, const char *name, void *addr)
{
    if (ctx->count == ctx->cap) {
        size_t ncap = ctx->cap ? ctx->cap * 2 : 16;
        EmbedSym *ns = realloc(ctx->syms, ncap * sizeof(EmbedSym));
        if (!ns)
            return -1;
        ctx->syms = ns;
        ctx->cap = ncap;
    }
    EmbedSym *s = &ctx->syms[ctx->count++];
    snprintf(s->name, sizeof(s->name), "%s", name);
    s->addr = addr;
    return 0;
}

/* Map a signature class letter onto its FFI ABI class. */
static int class_map(PithFfiType *out, int *is_pith_value, char c)
{
    if (is_pith_value)
        *is_pith_value = 0;
    switch (c) {
    case 'v': *out = PITH_FFI_VOID;   return 0;
    case 'w': *out = PITH_FFI_WORD;   return 0;
    case 'l': *out = PITH_FFI_LONG;   return 0;
    case 's': *out = PITH_FFI_SINGLE; return 0;
    case 'd': *out = PITH_FFI_DOUBLE; return 0;
    case 'p':
        *out = PITH_FFI_LONG;
        if (is_pith_value)
            *is_pith_value = 1;
        return 0;
    default:
        return -1;
    }
}

static PithImportUnit *unit_find(PithContext *ctx, const char *ns)
{
    for (size_t i = 0; i < ctx->nunits; i++)
        if (strcmp(ctx->units[i].ns, ns) == 0)
            return &ctx->units[i];
    return NULL;
}

static PithImportUnit *unit_new(PithContext *ctx, const char *ns)
{
    if (ctx->nunits == ctx->cap_units) {
        size_t ncap = ctx->cap_units ? ctx->cap_units * 2 : 8;
        PithImportUnit *nu = realloc(ctx->units,
                                      ncap * sizeof(PithImportUnit));
        if (!nu)
            return NULL;
        ctx->units = nu;
        ctx->cap_units = ncap;
    }
    PithImportUnit *u = &ctx->units[ctx->nunits++];
    memset(u, 0, sizeof(*u));
    snprintf(u->ns, sizeof(u->ns), "%s", ns);
    /* author "": host namespaces are root-level, so the frontend and
       the runtime agree on the c_<ns>_<fn> mangling */
    u->author[0] = '\0';
    u->path[0] = '\0';
    return u;
}

int pith_register_ns_fn(PithContext *ctx, const char *ns,
                        const char *name, void *fn_ptr,
                        char ret_class, const char *param_classes)
{
    if (!ctx || !ns || !*ns || !name || !*name || !fn_ptr) {
        fprintf(stderr, "pith embed: registration needs a namespace, "
                        "a name, and a function pointer\n");
        return -1;
    }

    PithFfiType ret;
    int ret_pv = 0;
    if (class_map(&ret, &ret_pv, ret_class) != 0) {
        fprintf(stderr, "pith embed: unknown return class `%c` for "
                        "%s.%s\n", ret_class, ns, name);
        return -1;
    }

    size_t nparams = param_classes ? strlen(param_classes) : 0;
    if (nparams > PITH_EMBED_MAX_PARAMS) {
        fprintf(stderr, "pith embed: %s.%s has %zu parameters (at most "
                        "%d)\n", ns, name, nparams,
                PITH_EMBED_MAX_PARAMS);
        return -1;
    }

    PithForeignFn proto;
    memset(&proto, 0, sizeof(proto));
    snprintf(proto.name, sizeof(proto.name), "%s", name);
    proto.ret = ret;
    proto.ret_pith_value = ret_pv && ret == PITH_FFI_LONG;
    proto.nparams = nparams;
    for (size_t i = 0; i < nparams; i++) {
        PithFfiType pt;
        if (class_map(&pt, NULL, param_classes[i]) != 0 ||
            pt == PITH_FFI_VOID) {
            fprintf(stderr, "pith embed: unknown parameter class `%c` "
                            "for %s.%s\n", param_classes[i], ns, name);
            return -1;
        }
        proto.params[i] = pt;
    }

    PithImportUnit *u = unit_find(ctx, ns);
    if (!u) {
        u = unit_new(ctx, ns);
        if (!u) {
            fprintf(stderr, "pith embed: out of memory\n");
            return -1;
        }
    }
    for (size_t i = 0; i < u->nfn; i++) {
        if (strcmp(u->fns[i].name, name) == 0) {
            fprintf(stderr, "pith embed: %s.%s is already registered\n",
                    ns, name);
            return -1;
        }
    }
    if (u->nfn >= PITH_FFI_MAX_FNS) {
        fprintf(stderr, "pith embed: namespace `%s` is full (%d "
                        "functions)\n", ns, PITH_FFI_MAX_FNS);
        return -1;
    }
    u->fns[u->nfn++] = proto;

    /* the runtime symbol under the mangled name the generated calls
       reference; the in-memory backend binds it to this address */
    char mangled[192];
    pith_cffi_mangled_name("", ns, name, mangled, sizeof(mangled));
    if (sym_push(ctx, mangled, fn_ptr) != 0) {
        u->nfn--;
        fprintf(stderr, "pith embed: out of memory\n");
        return -1;
    }
    return 0;
}

int pith_register_link_object(PithContext *ctx, const char *obj_path)
{
    if (!ctx || !obj_path || !*obj_path) {
        fprintf(stderr, "pith embed: link-object registration needs "
                        "a path\n");
        return -1;
    }
    if (ctx->nlink == ctx->cap_link) {
        size_t ncap = ctx->cap_link ? ctx->cap_link * 2 : 8;
        char **nl = realloc(ctx->link_objs, ncap * sizeof(char *));
        if (!nl) {
            fprintf(stderr, "pith embed: out of memory\n");
            return -1;
        }
        ctx->link_objs = nl;
        ctx->cap_link = ncap;
    }
    char *dup = strdup(obj_path);
    if (!dup) {
        fprintf(stderr, "pith embed: out of memory\n");
        return -1;
    }
    ctx->link_objs[ctx->nlink++] = dup;
    return 0;
}

/* ------------------------------------------------------------------ */
/* Evaluation                                                         */
/* ------------------------------------------------------------------ */

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

    /* the typed host namespaces join the code generator as import
       units: `ns.fn(...)` resolves and lowers exactly like a native
       C import (they are never dispatched to the engine as imports:
       their symbols resolve through the registered runtime symbols
       or the fallback link objects) */
    size_t gen_errors = 0;
    char *ssa = pith_gen_qbe(&program, 1, (const char **)&name, &source,
                             ctx ? ctx->units : NULL,
                             ctx ? ctx->nunits : 0,
                             NULL, &gen_errors);
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

    /* the fallback path links the registered objects into the
       temporary executable so the host functions resolve there */
    const char **prebuilt = NULL;
    size_t nprebuilt = 0;
    if (ctx && ctx->nlink > 0) {
        prebuilt = malloc(ctx->nlink * sizeof(char *));
        if (prebuilt) {
            for (size_t i = 0; i < ctx->nlink; i++)
                prebuilt[i] = ctx->link_objs[i];
            nprebuilt = ctx->nlink;
        }
    }

    int rc = engine_dispatch_run(asm_src, asm_path, extra, nextra,
                                 NULL, 0, prebuilt, nprebuilt);
    free(extra);
    free(prebuilt);

    free(asm_src);
    unlink(ssa_path);
    unlink(asm_path);

    return rc == -1 ? 1 : rc;
}
