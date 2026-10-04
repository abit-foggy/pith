/*
 * pith_embed.h - embeddable C ABI host interface for The Pith
 * Programming Language.
 *
 * Host C/C++ applications embed a PithContext, register C function
 * pointers (direct registration - no virtual stack marshaling), and
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
 * Register a host function under a namespace with an explicit FFI
 * signature; evaluated pith code calls it as `ns.name(...)`. The
 * signature classes are one character per slot, matching the FFI ABI
 * (see the FFI reference for the full type mapping):
 *
 *   'v'  void            (return only; the call is a statement)
 *   'w'  32-bit int
 *   'l'  64-bit int / pointer
 *   's'  float
 *   'd'  double
 *   'p'  PithValue*      (a pith string: parameters are borrowed, a
 *                        'p' return transfers a new +1 reference)
 *
 * `param_classes` holds one character per parameter, in order (NULL
 * or "" for zero parameters, at most PITH_EMBED_MAX_PARAMS). Returns
 * 0 on success, -1 on error with a diagnostic on stderr.
 */
int pith_register_ns_fn(PithContext *ctx, const char *ns,
                        const char *name, void *fn_ptr,
                        char ret_class, const char *param_classes);

/*
 * Register an object file the engine links into the temporary
 * executable when in-memory execution is unavailable (hardened
 * kernels, Darwin fallbacks), so the functions registered above
 * resolve there as well. The path is used verbatim at evaluation
 * time and must export the c_<ns>_<fn> symbols (see
 * pith_cffi_mangled_name in include/compiler.h); hosts compile their
 * registration module with the identical -D<fn>=c_<ns>_<fn> renames
 * pith applies to imported C units. Optional: the in-memory backend
 * uses the registered addresses and needs no object.
 */
int pith_register_link_object(PithContext *ctx, const char *obj_path);

/*
 * Lex, parse and lower `source` to QBE IR, then execute it in-memory
 * via libtcc with the context's registered functions available.
 * Returns the program exit code, or 1 on a compile/engine failure.
 */
int pith_eval_string(PithContext *ctx, const char *source);

/* Maximum FFI parameters per registered function (the import-unit
 * limit; see include/compiler.h). */
#define PITH_EMBED_MAX_PARAMS 8

#ifdef __cplusplus
}
#endif

#endif /* PITH_EMBED_H */
