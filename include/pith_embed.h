/*
 * pith_embed.h — embeddable C ABI host interface for The Pith
 * Programming Language.
 *
 * Host C/C++ applications embed a PithContext, register C function
 * pointers (direct registration — no virtual stack marshaling), and
 * evaluate pith source strings, which are lowered to QBE IR and
 * executed in-memory via libtcc.
 *
 * Example:
 *
 *     PithContext *ctx = pith_context_new();
 *     pith_register_fn(ctx, "logInfo", my_log_fn);
 *     int rc = pith_eval_string(ctx, "print \"hello from pith\"");
 *     pith_context_free(ctx);
 */
#ifndef PITH_EMBED_H
#define PITH_EMBED_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct PithContext PithContext;

/* Create a fresh embedding context. */
PithContext *pith_context_new(void);

/* Free an embedding context. */
void pith_context_free(PithContext *ctx);

/*
 * Register a host function under `name`. Compiled/evaluated pith code
 * calls it directly through its C function pointer.
 */
void pith_register_fn(PithContext *ctx, const char *name, void *fn_ptr);

/*
 * Lex, parse and lower `source` to QBE IR, then execute it in-memory
 * via libtcc with the context's registered functions available.
 * Returns the program exit code, or 1 on a compile/engine failure.
 */
int pith_eval_string(PithContext *ctx, const char *source);

#ifdef __cplusplus
}
#endif

#endif /* PITH_EMBED_H */
