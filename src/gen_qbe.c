/*
 * gen_qbe.c - AST walker lowering Pith programs to QBE SSA (.ssa) plus
 * deterministic ARC instructions for The Pith Programming Language, v0.1.
 *
 * Type lowering:
 *   Integer -> l  (64-bit int)      Float  -> d  (64-bit float)
 *   String  -> l  (pointer to PithString)   Boolean -> w (32-bit int)
 *
 * Memory model: every variable lives in a stack slot (alloc8); string
 * literals are static data with a PithString ARC header; declarations
 * allocate, reassignments release-then-store, and scope exits (end,
 * return) release every local ARC allocation. Zero tracing GC.
 *
 * All emitted IR shapes are validated against QBE:
 *   - stack slots: %v =l alloc8 8, storel/stored/storew, loadl/loadd/loadw
 *   - calls: %r =l call $f(l %a), bare call $f(l $global)
 *   - float constants: d_<scientific notation> (d_1.5, d_0.0)
 *   - comparisons: ceql/cnel/csltl/cslel/csgtl/csgel, ceqw/cnew/csltw,
 *     ceqd/cned/cltd/cled/cgtd/cged
 *   - promotions: %d =d cast %l; jnz accepts l temporaries via subtyping
 */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../include/api.h"
#include "../include/compiler.h"

/* ------------------------------------------------------------------ */
/* Growable string buffer                                             */
/* ------------------------------------------------------------------ */

typedef struct {
    char  *buf;
    size_t len;
    size_t cap;
} StrBuf;

static void sb_init(StrBuf *sb)
{
    sb->buf = NULL;
    sb->len = 0;
    sb->cap = 0;
}

static void sb_putn(StrBuf *sb, const char *s, size_t n)
{
    if (sb->len + n + 1 > sb->cap) {
        size_t ncap = sb->cap ? sb->cap : 256;
        while (sb->len + n + 1 > ncap) ncap *= 2;
        char *nb = realloc(sb->buf, ncap);
        if (!nb) {
            fputs("pith: out of memory while generating QBE IR\n", stderr);
            exit(1);
        }
        sb->buf = nb;
        sb->cap = ncap;
    }
    memcpy(sb->buf + sb->len, s, n);
    sb->len += n;
    sb->buf[sb->len] = '\0';
}

static void sb_put(StrBuf *sb, const char *s)
{
    sb_putn(sb, s, strlen(s));
}

static void sb_fmt(StrBuf *sb, const char *fmt, ...)
{
    char tmp[1024];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(tmp, sizeof(tmp), fmt, ap);
    va_end(ap);
    sb_put(sb, tmp);
}

/* ------------------------------------------------------------------ */
/* Code generator state                                               */
/* ------------------------------------------------------------------ */

/*
 * An expression evaluation result. `ref` is a QBE value: a temporary
 * ("%t12"), a global ("$str.3"), or a constant ("42", "d_1.5").
 * `owned` marks a temporary holding a freshly created reference
 * (concat results, runtime strings) that must be either transferred
 * into a variable or released right after its single use.
 * `borrowed_arc` marks a pointer borrowed from a refcounted variable.
 */
typedef struct {
    char          ref[64];
    PithValueType type;
    bool          owned;
    bool          borrowed_arc;
} ExprResult;

/*
 * A deferred private function: v0.1 has no call syntax, so private
 * project functions are emitted only when actually referenced  - 
 * unreferenced ones are eliminated at the end of the pass (WPSSAC
 * zero-bloat). When call syntax lands, the reference tracking hooks
 * into the same list.
 */
typedef struct {
    char     clean[128];
    ASTNode *node;
    bool     referenced;
    bool     emitted;
} PendingFn;

typedef struct LoopCtx {
    struct LoopCtx *prev;
    int             head_lbl;
    int             exit_lbl;
    Scope          *outer_scope;
} LoopCtx;

typedef struct {
    StrBuf      data;      /* data definitions (string literals)       */
    StrBuf      funcs;     /* named fn bodies                          */
    StrBuf      main;      /* the $main body                           */
    StrBuf     *cur;       /* current emission target                  */
    Scope      *scope;     /* codegen scope stack                      */
    size_t      errors;
    unsigned    tmp;       /* %tN temp counter                         */
    unsigned    slot;      /* %vN slot counter                         */
    unsigned    lbl;       /* @LN label counter                        */
    unsigned    str;       /* $str.N counter                           */
    unsigned    fconst;    /* $fconst.N float-constant data counter    */
    bool        block_dead;/* last emitted statement was a return      */
    LoopCtx    *loop_ctx;  /* enclosing while loop context (for break) */
    /* per-unit diagnostics lookup (WPSSAC) */
    const char **unit_paths;
    const char **unit_sources;
    size_t      unit_count;
    /* emitted fn symbols (duplicate detection across units) */
    char       *fn_syms[128];
    size_t      fn_sym_count;
    PendingFn   pending_fns[64];
    size_t      pending_fn_count;
    /* native C import units (FFI shims + call resolution) */
    const PithImportUnit *imports;
    size_t      nimports;
    /* plugin mode: export fn declarations under the author scope */
    bool        plugin_mode;
    const char *plugin_author;
    const char *plugin_module;
} Codegen;

static const char *unit_source_for(Codegen *g, const char *filepath)
{
    for (size_t i = 0; i < g->unit_count; i++)
        if (strcmp(g->unit_paths[i], filepath) == 0)
            return g->unit_sources[i];
    return NULL;
}

static void cg_error(Codegen *g, SourceLoc loc, size_t span,
                     const char *fmt, ...)
{
    char msg[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(msg, sizeof(msg), fmt, ap);
    va_end(ap);
    pith_emit_diagnostic("error", msg, loc.filepath,
                         unit_source_for(g, loc.filepath),
                         loc.line, loc.col, span);
    g->errors++;
}

static void note(Codegen *g, SourceLoc loc, size_t span, const char *msg)
{
    pith_emit_diagnostic("note", msg, loc.filepath,
                         unit_source_for(g, loc.filepath),
                         loc.line, loc.col, span);
}

static const char *value_kind_name(PithValueType t)
{
    switch (t) {
    case PITH_VALUE_INT:    return "an integer";
    case PITH_VALUE_FLOAT:  return "a float";
    case PITH_VALUE_STRING: return "a string";
    case PITH_VALUE_BOOL:   return "a boolean";
    default:                return "an unknown value";
    }
}

/* ------------------------------------------------------------------ */
/* Scopes                                                             */
/* ------------------------------------------------------------------ */

static void cg_scope_push(Codegen *g)
{
    Scope *s = calloc(1, sizeof(Scope));
    if (!s) {
        fputs("pith: out of memory while generating QBE IR\n", stderr);
        exit(1);
    }
    s->parent = g->scope;
    g->scope = s;
}

static void cg_scope_pop(Codegen *g)
{
    Scope *s = g->scope;
    if (!s)
        return;
    ScopeVar *v = s->vars;
    while (v) {
        ScopeVar *next = v->next;
        free(v->name);
        free(v);
        v = next;
    }
    g->scope = s->parent;
    free(s);
}

static ScopeVar *cg_lookup(Codegen *g, const char *name)
{
    for (Scope *s = g->scope; s; s = s->parent)
        for (ScopeVar *v = s->vars; v; v = v->next)
            if (strcmp(v->name, name) == 0)
                return v;
    return NULL;
}

/* Sanitize into a QBE-safe identifier chunk (alnum / underscore). */
static void qbe_sanitize(const char *in, char *out, size_t n)
{
    size_t o = 0;
    for (size_t i = 0; in[i] && o + 1 < n; i++) {
        char c = in[i];
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '_')
            out[o++] = c;
        else
            out[o++] = '_';
    }
    out[o] = '\0';
}

static void var_slot_name(const ScopeVar *v, char *out, size_t n)
{
    char clean[128];
    qbe_sanitize(v->name, clean, sizeof(clean));
    snprintf(out, n, "%%.v%u_%s", v->slot, clean);
}

/* ------------------------------------------------------------------ */
/* Emission helpers                                                   */
/* ------------------------------------------------------------------ */

#define EMIT(...) sb_fmt(g->cur, __VA_ARGS__)

static unsigned new_tmp(Codegen *g, char *out, size_t n)
{
    unsigned id = ++g->tmp;
    snprintf(out, n, "%%.t%u", id);
    return id;
}

static unsigned new_label(Codegen *g)
{
    return ++g->lbl;
}

/*
 * Static string literal: a data definition carrying a PithValue ARC
 * header. The layout must match pith.h exactly:
 *   strongRefs(4) typeTag(2) flags(2) capacity(4) length(4) payload...
 * (the flexible `data[]` member sits at offset 16). strongRefs=1 with
 * PITH_FLAG_STATIC marks it immortal (retain/release no-op on it).
 * Printable bytes are emitted as string chunks; every other byte as a
 * numeric field; a NUL sentinel always terminates.
 */
static void emit_string_data(Codegen *g, const char *s, size_t len,
                             char out[64])
{
    unsigned id = ++g->str;
    sb_fmt(&g->data, "data $str.%u = { w 1, h %d, h %d, w %llu, w %llu, ",
           id, PITH_TAG_STRING, PITH_FLAG_STATIC,
           (unsigned long long)len + 1, (unsigned long long)len);

    size_t i = 0;
    while (i < len) {
        unsigned char c = (unsigned char)s[i];
        if (c >= 0x20 && c <= 0x7E && c != '"' && c != '\\') {
            sb_put(&g->data, "b \"");
            while (i < len) {
                unsigned char r = (unsigned char)s[i];
                if (!(r >= 0x20 && r <= 0x7E && r != '"' && r != '\\'))
                    break;
                char one[2] = { (char)r, '\0' };
                sb_put(&g->data, one);
                i++;
            }
            sb_put(&g->data, "\", ");
        } else {
            sb_fmt(&g->data, "b %u, ", (unsigned)c);
            i++;
        }
    }
    sb_put(&g->data, "b 0 }\n");
    snprintf(out, 64, "$str.%u", id);
}

/* Release one owned temporary (consumed at its single use point). */
static void release_owned(Codegen *g, const ExprResult *v)
{
    if (!v->owned)
        return;
    EMIT("\tcall $pith_release(l %s)\n", v->ref);
}

/* Store a value into a variable's stack slot. */
/* Storage size in bytes for a sized type. */
static size_t sized_type_bytes(PithSizedType t)
{
    switch (t) {
    case PITH_SIZED_I8:
    case PITH_SIZED_U8:   return 1;
    case PITH_SIZED_I16:
    case PITH_SIZED_U16:  return 2;
    case PITH_SIZED_I32:
    case PITH_SIZED_U32:
    case PITH_SIZED_F32:  return 4;
    default:              return 8;   /* i64, u64, f64, AUTO */
    }
}

/* Store a value into a variable's stack slot, truncating to the
   storage width of the sized type. */
static void emit_store(Codegen *g, const ExprResult *v, const char *slot,
                       PithSizedType st)
{
    switch (v->type) {
    case PITH_VALUE_FLOAT:
        if (st == PITH_SIZED_F32) {
            /* truncate double to single, then store */
            char s[64];
            new_tmp(g, s, sizeof(s));
            EMIT("\t%s =s truncd %s\n", s, v->ref);
            EMIT("\tstores %s, %s\n", s, slot);
        } else {
            EMIT("\tstored %s, %s\n", v->ref, slot);
        }
        break;
    case PITH_VALUE_BOOL:
        EMIT("\tstorew %s, %s\n", v->ref, slot);
        break;
    case PITH_VALUE_STRING:
        EMIT("\tstorel %s, %s\n", v->ref, slot);
        break;
    default:   /* PITH_VALUE_INT */
        switch (st) {
        case PITH_SIZED_I8:
        case PITH_SIZED_U8:
            EMIT("\tstoreb %s, %s\n", v->ref, slot);
            break;
        case PITH_SIZED_I16:
        case PITH_SIZED_U16:
            EMIT("\tstoreh %s, %s\n", v->ref, slot);
            break;
        case PITH_SIZED_I32:
        case PITH_SIZED_U32:
            EMIT("\tstorew %s, %s\n", v->ref, slot);
            break;
        default:   /* i64, u64, AUTO: full 64-bit store */
            EMIT("\tstorel %s, %s\n", v->ref, slot);
            break;
        }
        break;
    }
}

/* Load a variable's stack slot into a fresh temporary, extending
   to the full 64-bit register width for arithmetic. */
static void emit_load(Codegen *g, const ScopeVar *var, ExprResult *out)
{
    char slot[160];
    var_slot_name(var, slot, sizeof(slot));
    char t[64];
    new_tmp(g, t, sizeof(t));

    switch (var->var_type) {
    case PITH_VALUE_FLOAT:
        if (var->sized_type == PITH_SIZED_F32) {
            /* load single-precision, extend to double */
            char s[64];
            new_tmp(g, s, sizeof(s));
            EMIT("\t%s =s loads %s\n", s, slot);
            EMIT("\t%s =d exts %s\n", t, s);
        } else {
            EMIT("\t%s =d loadd %s\n", t, slot);
        }
        out->type = PITH_VALUE_FLOAT;
        break;
    case PITH_VALUE_BOOL:
        EMIT("\t%s =w loadw %s\n", t, slot);
        out->type = PITH_VALUE_BOOL;
        break;
    case PITH_VALUE_STRING:
        EMIT("\t%s =l loadl %s\n", t, slot);
        out->type = PITH_VALUE_STRING;
        break;
    default:   /* PITH_VALUE_INT */
        switch (var->sized_type) {
        case PITH_SIZED_I8:
            EMIT("\t%s =l loadsb %s\n", t, slot);
            break;
        case PITH_SIZED_U8:
            EMIT("\t%s =l loadub %s\n", t, slot);
            break;
        case PITH_SIZED_I16:
            EMIT("\t%s =l loadsh %s\n", t, slot);
            break;
        case PITH_SIZED_U16:
            EMIT("\t%s =l loaduh %s\n", t, slot);
            break;
        case PITH_SIZED_I32:
            EMIT("\t%s =l loadsw %s\n", t, slot);
            break;
        case PITH_SIZED_U32:
            EMIT("\t%s =l loaduw %s\n", t, slot);
            break;
        default:   /* i64, u64, AUTO: full 64-bit load */
            EMIT("\t%s =l loadl %s\n", t, slot);
            break;
        }
        out->type = PITH_VALUE_INT;
        break;
    }

    snprintf(out->ref, sizeof(out->ref), "%s", t);
    out->owned = false;
    out->borrowed_arc = var->is_arc && out->type == PITH_VALUE_STRING;
}

/* Convert an integer value to a double in place (%d =d sltof %l).
   Note: QBE's `cast` is a bit-reinterpretation, NOT a numeric
   conversion - sltof is the signed-long-to-double conversion. */
static void promote_to_float(Codegen *g, ExprResult *v)
{
    if (v->type != PITH_VALUE_INT)
        return;
    char t[64];
    new_tmp(g, t, sizeof(t));
    EMIT("\t%s =d sltof %s\n", t, v->ref);
    snprintf(v->ref, sizeof(v->ref), "%s", t);
    v->type = PITH_VALUE_FLOAT;
    v->owned = false;
    v->borrowed_arc = false;
}

/* Release every ARC allocation registered in one scope. */
static void emit_scope_releases(Codegen *g, Scope *s)
{
    for (ScopeVar *v = s->vars; v; v = v->next) {
        if (!v->is_arc)
            continue;
        char slot[160];
        var_slot_name(v, slot, sizeof(slot));
        char t[64];
        new_tmp(g, t, sizeof(t));
        EMIT("\t%s =l loadl %s\n", t, slot);
        EMIT("\tcall $pith_release(l %s)\n", t);
    }
}
/* ------------------------------------------------------------------ */
/* Native C imports & builtins: typed FFI letters                     */
/* ------------------------------------------------------------------ */

static char ffi_letter(PithFfiType t)
{
    switch (t) {
    case PITH_FFI_WORD:   return 'w';
    case PITH_FFI_LONG:   return 'l';
    case PITH_FFI_SINGLE: return 's';
    case PITH_FFI_DOUBLE: return 'd';
    default:              return 'w';
    }
}

/* ------------------------------------------------------------------ */
/* Builtin namespaces: os.* and net.*                                 */
/* ------------------------------------------------------------------ */

typedef struct {
    const char   *member;
    const char   *qbe_fn;
    PithValueType type;
    size_t        nparams;
    PithFfiType   params[4];
} OsMember;

static const OsMember os_members[] = {
    { "identifyKernel",         "$pith_rt_os_kernel",         PITH_VALUE_STRING, 0, {0} },
    { "identifyKernelVersion",  "$pith_rt_os_kernel_version", PITH_VALUE_STRING, 0, {0} },
    { "isNT",                   "$pith_rt_is_nt",             PITH_VALUE_BOOL,   0, {0} },
    { "isLinux",                "$pith_rt_is_linux",          PITH_VALUE_BOOL,   0, {0} },
    { "isFreeBSD",              "$pith_rt_is_freebsd",        PITH_VALUE_BOOL,   0, {0} },
    { "isDarwin",               "$pith_rt_is_darwin",         PITH_VALUE_BOOL,   0, {0} },
    { "isMacOS",                "$pith_rt_is_macos",          PITH_VALUE_BOOL,   0, {0} },
    { "getEnv",                 "$pith_rt_get_env",           PITH_VALUE_STRING, 1, { PITH_FFI_LONG } },
    { "exit",                   "$pith_rt_exit",              PITH_VALUE_ERROR,  1, { PITH_FFI_WORD } },
    { "argCount",               "$pith_rt_arg_count",         PITH_VALUE_INT,    0, {0} },
    { "getArg",                 "$pith_rt_get_arg",           PITH_VALUE_STRING, 1, { PITH_FFI_WORD } },
};

static const OsMember fs_members[] = {
    { "readFile",               "$pith_rt_file_read",         PITH_VALUE_STRING, 1, { PITH_FFI_LONG } },
    { "writeFile",              "$pith_rt_file_write",        PITH_VALUE_INT,    2, { PITH_FFI_LONG, PITH_FFI_LONG } },
};

static const OsMember net_members[] = {
    { "socket",                 "$pith_net_socket",           PITH_VALUE_INT,    3, { PITH_FFI_WORD, PITH_FFI_WORD, PITH_FFI_WORD } },
    { "connect",                "$pith_rt_net_connect",       PITH_VALUE_INT,    3, { PITH_FFI_WORD, PITH_FFI_LONG, PITH_FFI_WORD } },
    { "send",                   "$pith_rt_net_send",          PITH_VALUE_INT,    2, { PITH_FFI_WORD, PITH_FFI_LONG } },
    { "recv",                   "$pith_rt_net_recv",          PITH_VALUE_STRING, 2, { PITH_FFI_WORD, PITH_FFI_WORD } },
    { "close",                  "$pith_net_close",            PITH_VALUE_INT,    1, { PITH_FFI_WORD } },
};

static const OsMember *os_member_find(const char *name)
{
    for (size_t i = 0; i < sizeof(os_members) / sizeof(os_members[0]); i++)
        if (strcmp(os_members[i].member, name) == 0)
            return &os_members[i];
    return NULL;
}

static const OsMember *fs_member_find(const char *name)
{
    for (size_t i = 0; i < sizeof(fs_members) / sizeof(fs_members[0]); i++)
        if (strcmp(fs_members[i].member, name) == 0)
            return &fs_members[i];
    return NULL;
}

static const OsMember *net_member_find(const char *name)
{
    for (size_t i = 0; i < sizeof(net_members) / sizeof(net_members[0]); i++)
        if (strcmp(net_members[i].member, name) == 0)
            return &net_members[i];
    return NULL;
}

/* Does the builtin os namespace expose `name`? (public: used by the
   import discovery for override warnings) */
int pith_os_member_exists(const char *name)
{
    return os_member_find(name) != NULL;
}

int pith_fs_member_exists(const char *name)
{
    return fs_member_find(name) != NULL;
}

int pith_net_member_exists(const char *name)
{
    return net_member_find(name) != NULL;
}

static ExprResult gen_expr(Codegen *g, ASTNode *n);

static const PithImportUnit *find_import(Codegen *g, const char *ns)
{
    for (size_t i = 0; i < g->nimports; i++)
        if (strcmp(g->imports[i].ns, ns) == 0)
            return &g->imports[i];
    return NULL;
}

static const PithForeignFn *find_foreign_fn(const PithImportUnit *u,
                                            const char *name)
{
    for (size_t i = 0; i < u->nfn; i++)
        if (strcmp(u->fns[i].name, name) == 0)
            return &u->fns[i];
    return NULL;
}

/* Convert a Pith value to an FFI argument class in place; 0 on ok. */
static int ffi_convert_arg(Codegen *g, ExprResult *v, PithFfiType t,
                           SourceLoc loc, size_t span)
{
    char tmp[64];

    switch (t) {
    case PITH_FFI_WORD:
        switch (v->type) {
        case PITH_VALUE_INT: {
            /* truncate l -> w through a scratch slot */
            char slot[64];
            new_tmp(g, slot, sizeof(slot));
            new_tmp(g, tmp, sizeof(tmp));
            EMIT("\t%s =l alloc8 8\n", slot);
            EMIT("\tstorel %s, %s\n", v->ref, slot);
            EMIT("\t%s =w loadw %s\n", tmp, slot);
            snprintf(v->ref, sizeof(v->ref), "%s", tmp);
            v->type = PITH_VALUE_BOOL;
            return 0;
        }
        case PITH_VALUE_FLOAT:
            new_tmp(g, tmp, sizeof(tmp));
            EMIT("\t%s =w dtosi %s\n", tmp, v->ref);
            snprintf(v->ref, sizeof(v->ref), "%s", tmp);
            v->type = PITH_VALUE_BOOL;
            return 0;
        case PITH_VALUE_BOOL:
            return 0;   /* already a word */
        default:
            cg_error(g, loc, span, "cannot pass a string as a 32-bit "
                                    "integer argument", "");
            return -1;
        }
    case PITH_FFI_LONG:
        switch (v->type) {
        case PITH_VALUE_INT:
        case PITH_VALUE_STRING:
            return 0;   /* already a long (strings cross as borrows) */
        case PITH_VALUE_BOOL:
            new_tmp(g, tmp, sizeof(tmp));
            EMIT("\t%s =l extsw %s\n", tmp, v->ref);
            snprintf(v->ref, sizeof(v->ref), "%s", tmp);
            v->type = PITH_VALUE_INT;
            return 0;
        case PITH_VALUE_FLOAT:
            new_tmp(g, tmp, sizeof(tmp));
            EMIT("\t%s =l dtosi %s\n", tmp, v->ref);
            snprintf(v->ref, sizeof(v->ref), "%s", tmp);
            v->type = PITH_VALUE_INT;
            return 0;
        default:
            return -1;
        }
    case PITH_FFI_SINGLE:
        switch (v->type) {
        case PITH_VALUE_FLOAT:
            new_tmp(g, tmp, sizeof(tmp));
            EMIT("\t%s =s truncd %s\n", tmp, v->ref);
            snprintf(v->ref, sizeof(v->ref), "%s", tmp);
            return 0;
        case PITH_VALUE_INT:
        case PITH_VALUE_BOOL: {
            char d[64];
            new_tmp(g, d, sizeof(d));
            if (v->type == PITH_VALUE_INT)
                EMIT("\t%s =d sltof %s\n", d, v->ref);
            else
                EMIT("\t%s =d swtof %s\n", d, v->ref);
            new_tmp(g, tmp, sizeof(tmp));
            EMIT("\t%s =s truncd %s\n", tmp, d);
            snprintf(v->ref, sizeof(v->ref), "%s", tmp);
            return 0;
        }
        default:
            cg_error(g, loc, span, "cannot pass a string as a float "
                                    "argument", "");
            return -1;
        }
    case PITH_FFI_DOUBLE:
        switch (v->type) {
        case PITH_VALUE_FLOAT:
            return 0;
        case PITH_VALUE_INT:
            new_tmp(g, tmp, sizeof(tmp));
            EMIT("\t%s =d sltof %s\n", tmp, v->ref);
            snprintf(v->ref, sizeof(v->ref), "%s", tmp);
            return 0;
        case PITH_VALUE_BOOL:
            new_tmp(g, tmp, sizeof(tmp));
            EMIT("\t%s =d swtof %s\n", tmp, v->ref);
            snprintf(v->ref, sizeof(v->ref), "%s", tmp);
            return 0;
        default:
            cg_error(g, loc, span, "cannot pass a string as a double "
                                    "argument", "");
            return -1;
        }
    default:
        return -1;
    }
}

static ExprResult expr_dummy(void);

/* ns.fn(args) - a typed call through the namespaced shim. */
/* ------------------------------------------------------------------ */
/* Namespace resolution (unified for calls and bare accesses)         */
/* ------------------------------------------------------------------ */

typedef enum {
    NS_FOUND_IMPORT,     /* unit + fn set                              */
    NS_FOUND_BUILTIN,    /* om set                                     */
    NS_NOT_FOUND,
} NsKind;

typedef struct {
    NsKind kind;
    const PithImportUnit *unit;
    const PithForeignFn *fn;
    const OsMember *om;
} NsResolved;

/* Find an import unit by author + module name. */
static const PithImportUnit *find_import_am(Codegen *g,
                                            const char *author,
                                            const char *module)
{
    for (size_t i = 0; i < g->nimports; i++) {
        const PithImportUnit *u = &g->imports[i];
        if (strcmp(u->ns, module) != 0)
            continue;
        if (strcmp(u->author, author) == 0)
            return u;
    }
    return NULL;
}

/* Mangled QBE symbol for an imported function (author-aware). */
static void shim_symbol(const PithImportUnit *u, const PithForeignFn *fn,
                        char *out, size_t n)
{
    char mangled[192];
    pith_cffi_mangled_name(u->author, u->ns, fn->name, mangled,
                           sizeof(mangled));
    snprintf(out, n, "$%s", mangled);
}

/*
 * Unified resolution:
 *   1. fully-qualified: "root.<module>" resolves to the builtin ONLY
 *      (bypasses all overrides); "<author>.<module>" resolves to that
 *      author's unit directly
 *   2. unqualified "<module>": the overlay (active imports, newest
 *      first), then the root base (builtin)
 */
static NsResolved ns_resolve(Codegen *g, const char *ns_path,
                             const char *member)
{
    NsResolved r;
    memset(&r, 0, sizeof(r));

    const char *dot = strrchr(ns_path, '.');
    if (dot) {
        char scope[64], module[64];
        size_t slen = (size_t)(dot - ns_path);
        if (slen >= sizeof(scope)) slen = sizeof(scope) - 1;
        memcpy(scope, ns_path, slen);
        scope[slen] = '\0';
        snprintf(module, sizeof(module), "%s", dot + 1);

        if (strcmp(scope, "root") == 0) {
            /* the explicit root scope: base runtime ONLY */
            if (strcmp(module, "os") == 0) {
                const OsMember *om = os_member_find(member);
                if (om) {
                    r.kind = NS_FOUND_BUILTIN;
                    r.om = om;
                    return r;
                }
            } else if (strcmp(module, "fs") == 0) {
                const OsMember *fm = fs_member_find(member);
                if (fm) {
                    r.kind = NS_FOUND_BUILTIN;
                    r.om = fm;
                    return r;
                }
            } else if (strcmp(module, "net") == 0) {
                const OsMember *nm = net_member_find(member);
                if (nm) {
                    r.kind = NS_FOUND_BUILTIN;
                    r.om = nm;
                    return r;
                }
            }
            r.kind = NS_NOT_FOUND;
            return r;
        }

        /* author.module: direct lookup, bypassing overrides */
        const PithImportUnit *u = find_import_am(g, scope, module);
        if (u) {
            const PithForeignFn *fn = find_foreign_fn(u, member);
            if (fn) {
                r.kind = NS_FOUND_IMPORT;
                r.unit = u;
                r.fn = fn;
                return r;
            }
        }
        r.kind = NS_NOT_FOUND;
        return r;
    }

    /* unqualified module: the overlay first (newest import wins),
       then the builtin base */
    for (size_t i = g->nimports; i > 0; i--) {
        const PithImportUnit *u = &g->imports[i - 1];
        if (strcmp(u->ns, ns_path) != 0)
            continue;
        const PithForeignFn *fn = find_foreign_fn(u, member);
        if (fn) {
            r.kind = NS_FOUND_IMPORT;
            r.unit = u;
            r.fn = fn;
            return r;
        }
    }
    if (strcmp(ns_path, "os") == 0) {
        const OsMember *om = os_member_find(member);
        if (om) {
            r.kind = NS_FOUND_BUILTIN;
            r.om = om;
            return r;
        }
    } else if (strcmp(ns_path, "fs") == 0) {
        const OsMember *fm = fs_member_find(member);
        if (fm) {
            r.kind = NS_FOUND_BUILTIN;
            r.om = fm;
            return r;
        }
    } else if (strcmp(ns_path, "net") == 0) {
        const OsMember *nm = net_member_find(member);
        if (nm) {
            r.kind = NS_FOUND_BUILTIN;
            r.om = nm;
            return r;
        }
    }
    r.kind = NS_NOT_FOUND;
    return r;
}

/* Flatten a member-access chain into its dotted path: the LAST
   component is the member (the caller splits); "alice.os.identifyKernel"
   flattens to "alice.os.identifyKernel". Chains rooted at anything but
   an identifier (e.g. call results) flatten to an empty path. */
static void flatten_chain(ASTNode *node, char *out, size_t outlen)
{
    if (node->type == AST_MEMBER_ACCESS) {
        flatten_chain(node->as.member_access.base, out, outlen);
        size_t len = strlen(out);
        const char *m = node->as.member_access.member;
        size_t mlen = strlen(m);
        if (len && len + 1 + mlen + 1 <= outlen) {
            out[len] = '.';
            memcpy(out + len + 1, m, mlen);
            out[len + 1 + mlen] = '\0';
        }
    } else if (node->type == AST_IDENTIFIER_EXPR) {
        snprintf(out, outlen, "%s", node->as.identifier);
    } else {
        out[0] = '\0';
    }
}

/*
 * The unified namespace member access: identical resolution logic for
 * bare accesses (`ns.member`) and calls (`ns.member(...)`).
 */
static ExprResult ns_access(Codegen *g, const char *ns_path,
                            const char *member, SourceLoc loc,
                            size_t span, bool is_call, size_t arg_count,
                            ExprResult *args)
{
    NsResolved r = ns_resolve(g, ns_path, member);

    if (r.kind == NS_NOT_FOUND) {
        cg_error(g, loc, span, "unknown member or namespace `%s.%s`",
                 ns_path, member);
        return expr_dummy();
    }

    if (r.kind == NS_FOUND_BUILTIN) {
        const OsMember *bm = r.om;
        if (arg_count != bm->nparams) {
            if (bm->nparams == 0) {
                cg_error(g, loc, 1,
                         "`%s.%s` is a property and takes no arguments",
                         ns_path, member);
            } else {
                cg_error(g, loc, 1,
                         "`%s.%s` expects %zu argument%s, got %zu",
                         ns_path, member, bm->nparams,
                         bm->nparams == 1 ? "" : "s", arg_count);
            }
            return expr_dummy();
        }

        for (size_t i = 0; i < arg_count; i++) {
            if (ffi_convert_arg(g, &args[i], bm->params[i], loc, 1) != 0)
                return expr_dummy();
        }

        ExprResult res;
        memset(&res, 0, sizeof(res));

        char argtext[512];
        argtext[0] = '\0';
        if (arg_count > 0) {
            size_t at = 0;
            for (size_t i = 0; i < arg_count; i++) {
                if (i) {
                    argtext[at++] = ',';
                    argtext[at++] = ' ';
                }
                argtext[at++] = ffi_letter(bm->params[i]);
                argtext[at++] = ' ';
                size_t rl = strlen(args[i].ref);
                if (at + rl + 1 >= sizeof(argtext)) {
                    cg_error(g, loc, 1, "argument list too long", "");
                    return expr_dummy();
                }
                memcpy(argtext + at, args[i].ref, rl);
                at += rl;
            }
            argtext[at] = '\0';
        }

        if (bm->type == PITH_VALUE_ERROR) {
            /* void function, e.g. os.exit */
            if (arg_count > 0)
                EMIT("\tcall %s(%s)\n", bm->qbe_fn, argtext);
            else
                EMIT("\tcall %s()\n", bm->qbe_fn);
            snprintf(res.ref, sizeof(res.ref), "0");
            res.type = PITH_VALUE_ERROR;
            res.owned = false;
            res.borrowed_arc = false;
            return res;
        }

        char t[64];
        new_tmp(g, t, sizeof(t));
        if (bm->type == PITH_VALUE_BOOL) {
            if (arg_count > 0)
                EMIT("\t%s =w call %s(%s)\n", t, bm->qbe_fn, argtext);
            else
                EMIT("\t%s =w call %s()\n", t, bm->qbe_fn);
            res.type = PITH_VALUE_BOOL;
            res.owned = false;
            snprintf(res.ref, sizeof(res.ref), "%s", t);
        } else if (bm->type == PITH_VALUE_INT) {
            if (arg_count > 0)
                EMIT("\t%s =w call %s(%s)\n", t, bm->qbe_fn, argtext);
            else
                EMIT("\t%s =w call %s()\n", t, bm->qbe_fn);
            char t_ext[64];
            new_tmp(g, t_ext, sizeof(t_ext));
            EMIT("\t%s =l extsw %s\n", t_ext, t);
            res.type = PITH_VALUE_INT;
            res.owned = false;
            snprintf(res.ref, sizeof(res.ref), "%s", t_ext);
        } else { /* PITH_VALUE_STRING */
            if (arg_count > 0)
                EMIT("\t%s =l call %s(%s)\n", t, bm->qbe_fn, argtext);
            else
                EMIT("\t%s =l call %s()\n", t, bm->qbe_fn);
            res.type = PITH_VALUE_STRING;
            res.owned = true;
            snprintf(res.ref, sizeof(res.ref), "%s", t);
        }
        res.borrowed_arc = false;
        return res;
    }

    /* import: strict arity for calls; bare access only for zero-param
       members (called like a builtin property) */
    const PithForeignFn *fn = r.fn;

    if (!is_call && fn->nparams != 0) {
        cg_error(g, loc, span,
                 "member `%s.%s` expects %zu argument%s; call it with "
                 "(...)", ns_path, member, fn->nparams,
                 fn->nparams == 1 ? "" : "s");
        return expr_dummy();
    }
    if (is_call && arg_count != fn->nparams) {
        cg_error(g, loc, 1, "`%s.%s` expects %zu argument%s, got %zu",
                 ns_path, member, fn->nparams,
                 fn->nparams == 1 ? "" : "s", arg_count);
        return expr_dummy();
    }

    /* convert every argument to the resolved parameter class */
    for (size_t i = 0; i < arg_count; i++) {
        if (ffi_convert_arg(g, &args[i], fn->params[i], loc, 1) != 0)
            return expr_dummy();
    }

    char sym[192];
    shim_symbol(r.unit, fn, sym, sizeof(sym));

    char ns_clean[64];
    qbe_sanitize(ns_path, ns_clean, sizeof(ns_clean));

    ExprResult res;
    memset(&res, 0, sizeof(res));

    if (fn->ret == PITH_FFI_VOID) {
        if (is_call) {
            char argtext[512];
            size_t at = 0;
            for (size_t i = 0; i < arg_count; i++) {
                if (i) {
                    argtext[at++] = ',';
                    argtext[at++] = ' ';
                }
                argtext[at++] = ffi_letter(fn->params[i]);
                argtext[at++] = ' ';
                size_t rl = strlen(args[i].ref);
                if (at + rl + 1 >= sizeof(argtext)) {
                    cg_error(g, loc, 1, "internal error: argument list "
                                        "too long", "");
                    return expr_dummy();
                }
                memcpy(argtext + at, args[i].ref, rl);
                at += rl;
            }
            argtext[at] = '\0';
            EMIT("\tcall %s(%s)\n", sym, argtext);
        } else {
            EMIT("\tcall %s()\n", sym);
        }
        res.type = PITH_VALUE_ERROR;   /* usable only as a statement */
        res.owned = false;
        res.borrowed_arc = false;
        snprintf(res.ref, sizeof(res.ref), "0");
        return res;
    }

    char t[64];
    new_tmp(g, t, sizeof(t));

    if (is_call) {
        char argtext[512];
        size_t at = 0;
        for (size_t i = 0; i < arg_count; i++) {
            if (i) {
                argtext[at++] = ',';
                argtext[at++] = ' ';
            }
            argtext[at++] = ffi_letter(fn->params[i]);
            argtext[at++] = ' ';
            size_t rl = strlen(args[i].ref);
            if (at + rl + 1 >= sizeof(argtext)) {
                cg_error(g, loc, 1, "internal error: argument list too "
                                    "long", "");
                return expr_dummy();
            }
            memcpy(argtext + at, args[i].ref, rl);
            at += rl;
        }
        argtext[at] = '\0';
        EMIT("\t%s =%c call %s(%s)\n", t, ffi_letter(fn->ret), sym,
             argtext);
    } else {
        EMIT("\t%s =%c call %s()\n", t, ffi_letter(fn->ret), sym);
    }
    snprintf(res.ref, sizeof(res.ref), "%s", t);

    switch (fn->ret) {
    case PITH_FFI_WORD:
        new_tmp(g, t, sizeof(t));
        EMIT("\t%s =l extsw %s\n", t, res.ref);
        snprintf(res.ref, sizeof(res.ref), "%s", t);
        res.type = PITH_VALUE_INT;
        res.owned = false;
        res.borrowed_arc = false;
        break;
    case PITH_FFI_LONG:
        res.type = fn->ret_pith_value ? PITH_VALUE_STRING
                                      : PITH_VALUE_INT;
        res.owned = fn->ret_pith_value;
        res.borrowed_arc = false;
        break;
    case PITH_FFI_SINGLE:
        new_tmp(g, t, sizeof(t));
        EMIT("\t%s =d exts %s\n", t, res.ref);
        snprintf(res.ref, sizeof(res.ref), "%s", t);
        res.type = PITH_VALUE_FLOAT;
        res.owned = false;
        res.borrowed_arc = false;
        break;
    case PITH_FFI_DOUBLE:
        res.type = PITH_VALUE_FLOAT;
        res.owned = false;
        res.borrowed_arc = false;
        break;
    default:
        return expr_dummy();
    }
    return res;
}

/* ns.fn(args) - a typed call through the namespaced shim. */
static ExprResult gen_call(Codegen *g, ASTNode *n)
{
    ASTCallExpr *call = &n->as.call;
    ExprResult dummy = expr_dummy();

    if (call->callee->type == AST_IDENTIFIER_EXPR) {
        const char *name = call->callee->as.identifier;
        char clean[128];
        qbe_sanitize(name, clean, sizeof(clean));

        PendingFn *target = NULL;
        for (size_t i = 0; i < g->pending_fn_count; i++) {
            if (strcmp(g->pending_fns[i].clean, clean) == 0) {
                target = &g->pending_fns[i];
                break;
            }
        }
        if (!target) {
            cg_error(g, call->callee->loc, pith_utf8_len(name, strlen(name)),
                     "unknown function `%s`", name);
            return dummy;
        }

        target->referenced = true;

        ASTFnDecl *decl = &target->node->as.fn_decl;
        if (call->arg_count != decl->param_count) {
            cg_error(g, n->loc, pith_utf8_len(name, strlen(name)),
                     "function `%s` expects %zu argument%s, but %zu were provided",
                     name, decl->param_count,
                     decl->param_count == 1 ? "" : "s", call->arg_count);
            return dummy;
        }

        ExprResult args[PITH_FFI_MAX_PARAMS];
        for (size_t i = 0; i < call->arg_count; i++) {
            args[i] = gen_expr(g, call->args[i]);
            if (args[i].type == PITH_VALUE_ERROR)
                return dummy;
        }

        char ret_tmp[64];
        new_tmp(g, ret_tmp, sizeof(ret_tmp));

        EMIT("\t%s =l call $fn_%s(", ret_tmp, clean);
        for (size_t i = 0; i < call->arg_count; i++) {
            if (i > 0)
                EMIT(", ");
            EMIT("l %s", args[i].ref);
        }
        EMIT(")\n");

        for (size_t i = 0; i < call->arg_count; i++)
            release_owned(g, &args[i]);

        ExprResult res;
        snprintf(res.ref, sizeof(res.ref), "%s", ret_tmp);
        res.type = PITH_VALUE_INT;
        res.owned = false;
        res.borrowed_arc = false;
        return res;
    }

    if (call->callee->type != AST_MEMBER_ACCESS) {
        cg_error(g, call->callee->loc, 1,
                 "a call target must be a function name or namespace member", "");
        return dummy;
    }

    char path[512];
    flatten_chain(call->callee->as.member_access.base, path,
                  sizeof(path));
    if (!path[0]) {
        cg_error(g, call->callee->loc, 1,
                 "a call target must name a namespace member "
                 "(ns.fn)", "");
        return dummy;
    }
    const char *member =
        call->callee->as.member_access.member;

    /* evaluate and convert every argument */
    ExprResult args[PITH_FFI_MAX_PARAMS];
    for (size_t i = 0; i < call->arg_count; i++) {
        args[i] = gen_expr(g, call->args[i]);
        if (args[i].type == PITH_VALUE_ERROR)
            return dummy;
        /* conversion happens after resolution (the resolved function
           determines the parameter class); a placeholder pass would
           double-emit - so conversion is deferred to ns_access */
    }

    ExprResult res = ns_access(g, path, member, n->loc,
                               pith_utf8_len(member,
                                             strlen(member)),
                               true, call->arg_count, args);

    /* owned string temporaries passed as borrowed arguments are
       consumed by the call and released right after it */
    for (size_t i = 0; i < call->arg_count; i++)
        release_owned(g, &args[i]);

    return res;
}

static ExprResult gen_member_access(Codegen *g, ASTNode *n)
{
    ASTMemberAccess *m = &n->as.member_access;

    char path[512];
    flatten_chain(m->base, path, sizeof(path));
    if (!path[0]) {
        cg_error(g, m->base->loc, 2,
                 "unknown namespace in member access "
                 "(namespaces are `os.*`, `root.*`, or imported "
                 "modules)", "");
        return expr_dummy();
    }
    const char *member = m->member;

    return ns_access(g, path, member, n->loc,
                     pith_utf8_len(member, strlen(member)),
                     false, 0, NULL);
}

static ExprResult expr_dummy(void)
{
    ExprResult v;
    snprintf(v.ref, sizeof(v.ref), "0");
    v.type = PITH_VALUE_ERROR;
    v.owned = false;
    v.borrowed_arc = false;
    return v;
}

static ExprResult gen_logical_and(Codegen *g, ASTNode *n)
{
    char slot[64];
    new_tmp(g, slot, sizeof(slot));
    EMIT("\t%s =l alloc4 4\n", slot);

    int eval_right_lbl = new_label(g);
    int true_lbl = new_label(g);
    int false_lbl = new_label(g);
    int end_lbl = new_label(g);

    ExprResult l = gen_expr(g, n->as.binary_op.left);
    if (l.type == PITH_VALUE_STRING) {
        cg_error(g, n->loc, 3, "cannot apply `and` to a string value", "");
        release_owned(g, &l);
        return expr_dummy();
    }
    if (l.type == PITH_VALUE_FLOAT) {
        char t[64];
        new_tmp(g, t, sizeof(t));
        EMIT("\t%s =w cned %s, d_0.0\n", t, l.ref);
        EMIT("\tjnz %s, @L%u, @L%u\n", t, eval_right_lbl, false_lbl);
    } else {
        EMIT("\tjnz %s, @L%u, @L%u\n", l.ref, eval_right_lbl, false_lbl);
    }
    release_owned(g, &l);

    EMIT("@L%u\n", eval_right_lbl);
    ExprResult r = gen_expr(g, n->as.binary_op.right);
    if (r.type == PITH_VALUE_STRING) {
        cg_error(g, n->loc, 3, "cannot apply `and` to a string value", "");
        release_owned(g, &r);
        return expr_dummy();
    }
    if (r.type == PITH_VALUE_FLOAT) {
        char t[64];
        new_tmp(g, t, sizeof(t));
        EMIT("\t%s =w cned %s, d_0.0\n", t, r.ref);
        EMIT("\tjnz %s, @L%u, @L%u\n", t, true_lbl, false_lbl);
    } else {
        EMIT("\tjnz %s, @L%u, @L%u\n", r.ref, true_lbl, false_lbl);
    }
    release_owned(g, &r);

    EMIT("@L%u\n", true_lbl);
    EMIT("\tstorew 1, %s\n", slot);
    EMIT("\tjmp @L%u\n", end_lbl);

    EMIT("@L%u\n", false_lbl);
    EMIT("\tstorew 0, %s\n", slot);
    EMIT("\tjmp @L%u\n", end_lbl);

    EMIT("@L%u\n", end_lbl);
    char res[64];
    new_tmp(g, res, sizeof(res));
    EMIT("\t%s =w loadw %s\n", res, slot);

    ExprResult v;
    snprintf(v.ref, sizeof(v.ref), "%s", res);
    v.type = PITH_VALUE_BOOL;
    v.owned = false;
    v.borrowed_arc = false;
    return v;
}

static ExprResult gen_logical_or(Codegen *g, ASTNode *n)
{
    char slot[64];
    new_tmp(g, slot, sizeof(slot));
    EMIT("\t%s =l alloc4 4\n", slot);

    int eval_right_lbl = new_label(g);
    int true_lbl = new_label(g);
    int false_lbl = new_label(g);
    int end_lbl = new_label(g);

    ExprResult l = gen_expr(g, n->as.binary_op.left);
    if (l.type == PITH_VALUE_STRING) {
        cg_error(g, n->loc, 2, "cannot apply `or` to a string value", "");
        release_owned(g, &l);
        return expr_dummy();
    }
    if (l.type == PITH_VALUE_FLOAT) {
        char t[64];
        new_tmp(g, t, sizeof(t));
        EMIT("\t%s =w cned %s, d_0.0\n", t, l.ref);
        EMIT("\tjnz %s, @L%u, @L%u\n", t, true_lbl, eval_right_lbl);
    } else {
        EMIT("\tjnz %s, @L%u, @L%u\n", l.ref, true_lbl, eval_right_lbl);
    }
    release_owned(g, &l);

    EMIT("@L%u\n", eval_right_lbl);
    ExprResult r = gen_expr(g, n->as.binary_op.right);
    if (r.type == PITH_VALUE_STRING) {
        cg_error(g, n->loc, 2, "cannot apply `or` to a string value", "");
        release_owned(g, &r);
        return expr_dummy();
    }
    if (r.type == PITH_VALUE_FLOAT) {
        char t[64];
        new_tmp(g, t, sizeof(t));
        EMIT("\t%s =w cned %s, d_0.0\n", t, r.ref);
        EMIT("\tjnz %s, @L%u, @L%u\n", t, true_lbl, false_lbl);
    } else {
        EMIT("\tjnz %s, @L%u, @L%u\n", r.ref, true_lbl, false_lbl);
    }
    release_owned(g, &r);

    EMIT("@L%u\n", true_lbl);
    EMIT("\tstorew 1, %s\n", slot);
    EMIT("\tjmp @L%u\n", end_lbl);

    EMIT("@L%u\n", false_lbl);
    EMIT("\tstorew 0, %s\n", slot);
    EMIT("\tjmp @L%u\n", end_lbl);

    EMIT("@L%u\n", end_lbl);
    char res[64];
    new_tmp(g, res, sizeof(res));
    EMIT("\t%s =w loadw %s\n", res, slot);

    ExprResult v;
    snprintf(v.ref, sizeof(v.ref), "%s", res);
    v.type = PITH_VALUE_BOOL;
    v.owned = false;
    v.borrowed_arc = false;
    return v;
}

static ExprResult gen_binary(Codegen *g, ASTNode *n)
{
    ExprResult l = gen_expr(g, n->as.binary_op.left);
    ExprResult r = gen_expr(g, n->as.binary_op.right);
    TokenType op = n->as.binary_op.op;

    if (l.type == PITH_VALUE_ERROR || r.type == PITH_VALUE_ERROR)
        return expr_dummy();

    /* release owned string operands right after they are consumed */
    #define RELEASE_OWNED_OPS()          \
        do {                             \
            release_owned(g, &l);        \
            release_owned(g, &r);        \
        } while (0)

    if (op == TOK_OP_PLUS) {
        if (l.type == PITH_VALUE_STRING && r.type == PITH_VALUE_STRING) {
            char t[64];
            new_tmp(g, t, sizeof(t));
            EMIT("\t%s =l call $pith_str_concat(l %s, l %s)\n",
                 t, l.ref, r.ref);
            RELEASE_OWNED_OPS();
            ExprResult v;
            snprintf(v.ref, sizeof(v.ref), "%s", t);
            v.type = PITH_VALUE_STRING;
            v.owned = true;
            v.borrowed_arc = false;
            return v;
        }
        if (l.type == PITH_VALUE_STRING || r.type == PITH_VALUE_STRING) {
            cg_error(g, n->loc, 1,
                     "cannot add %s and %s "
                     "(`+` concatenates two strings)",
                     value_kind_name(l.type), value_kind_name(r.type));
            RELEASE_OWNED_OPS();
            return expr_dummy();
        }
        if (l.type == PITH_VALUE_INT && r.type == PITH_VALUE_INT) {
            char t[64];
            new_tmp(g, t, sizeof(t));
            EMIT("\t%s =l add %s, %s\n", t, l.ref, r.ref);
            ExprResult v;
            snprintf(v.ref, sizeof(v.ref), "%s", t);
            v.type = PITH_VALUE_INT;
            v.owned = false;
            v.borrowed_arc = false;
            return v;
        }
        promote_to_float(g, &l);
        promote_to_float(g, &r);
        char t[64];
        new_tmp(g, t, sizeof(t));
        EMIT("\t%s =d add %s, %s\n", t, l.ref, r.ref);
        ExprResult v;
        snprintf(v.ref, sizeof(v.ref), "%s", t);
        v.type = PITH_VALUE_FLOAT;
        v.owned = false;
        v.borrowed_arc = false;
        return v;
    }

    if (op == TOK_OP_MINUS || op == TOK_OP_STAR || op == TOK_OP_SLASH) {
        if (l.type == PITH_VALUE_STRING || r.type == PITH_VALUE_STRING) {
            cg_error(g, n->loc, 1,
                     "strings do not support arithmetic "
                     "(use `+` to concatenate)", "");
            RELEASE_OWNED_OPS();
            return expr_dummy();
        }
        const char *iname = op == TOK_OP_MINUS ? "sub"
                          : op == TOK_OP_STAR  ? "mul"
                          :                      "div";
        bool as_float = (l.type == PITH_VALUE_FLOAT ||
                         r.type == PITH_VALUE_FLOAT);
        if (as_float) {
            promote_to_float(g, &l);
            promote_to_float(g, &r);
        }
        char t[64];
        new_tmp(g, t, sizeof(t));
        if (as_float)
            EMIT("\t%s =d %s %s, %s\n", t, iname, l.ref, r.ref);
        else
            EMIT("\t%s =l %s %s, %s\n", t, iname, l.ref, r.ref);
        ExprResult v;
        snprintf(v.ref, sizeof(v.ref), "%s", t);
        v.type = as_float ? PITH_VALUE_FLOAT : PITH_VALUE_INT;
        v.owned = false;
        v.borrowed_arc = false;
        return v;
    }

    /* comparisons */
    if (l.type == PITH_VALUE_STRING || r.type == PITH_VALUE_STRING) {
        if (l.type != PITH_VALUE_STRING || r.type != PITH_VALUE_STRING) {
            cg_error(g, n->loc, 1, "cannot compare %s with %s",
                     value_kind_name(l.type), value_kind_name(r.type));
            RELEASE_OWNED_OPS();
            return expr_dummy();
        }
        char e[64];
        new_tmp(g, e, sizeof(e));
        EMIT("\t%s =w call $pith_str_equals(l %s, l %s)\n",
             e, l.ref, r.ref);
        RELEASE_OWNED_OPS();
        ExprResult v;
        if (op == TOK_OP_NE) {
            char t[64];
            new_tmp(g, t, sizeof(t));
            EMIT("\t%s =w ceqw %s, 0\n", t, e);
            snprintf(v.ref, sizeof(v.ref), "%s", t);
        } else {
            snprintf(v.ref, sizeof(v.ref), "%s", e);
        }
        v.type = PITH_VALUE_BOOL;
        v.owned = false;
        v.borrowed_arc = false;
        return v;
    }

    bool as_float = (l.type == PITH_VALUE_FLOAT || r.type == PITH_VALUE_FLOAT);
    if (as_float) {
        promote_to_float(g, &l);
        promote_to_float(g, &r);
    }

    const char *iname;
    if (as_float) {
        iname = op == TOK_OP_EQ ? "ceqd"   : op == TOK_OP_NE ? "cned"
              : op == TOK_OP_LT ? "cltd"   : op == TOK_OP_LE ? "cled"
              : op == TOK_OP_GT ? "cgtd"   :                   "cged";
    } else if (l.type == PITH_VALUE_BOOL || r.type == PITH_VALUE_BOOL) {
        /* word comparison: booleans are w; integer constants are
           accepted in a w context (low 32 bits) */
        iname = op == TOK_OP_EQ ? "ceqw"   : op == TOK_OP_NE ? "cnew"
              : op == TOK_OP_LT ? "csltw"  : op == TOK_OP_LE ? "cslew"
              : op == TOK_OP_GT ? "csgtw"  :                   "csgew";
    } else {
        iname = op == TOK_OP_EQ ? "ceql"   : op == TOK_OP_NE ? "cnel"
              : op == TOK_OP_LT ? "csltl"  : op == TOK_OP_LE ? "cslel"
              : op == TOK_OP_GT ? "csgtl"  :                   "csgel";
    }

    char t[64];
    new_tmp(g, t, sizeof(t));
    EMIT("\t%s =w %s %s, %s\n", t, iname, l.ref, r.ref);
    ExprResult v;
    snprintf(v.ref, sizeof(v.ref), "%s", t);
    v.type = PITH_VALUE_BOOL;
    v.owned = false;
    v.borrowed_arc = false;
    return v;
}

static ExprResult gen_expr(Codegen *g, ASTNode *n)
{
    switch (n->type) {
    case AST_INT_EXPR: {
        ExprResult v;
        snprintf(v.ref, sizeof(v.ref), "%lld", n->as.int_literal);
        v.type = PITH_VALUE_INT;
        v.owned = false;
        v.borrowed_arc = false;
        return v;
    }
    case AST_FLOAT_EXPR: {
        ExprResult v;
        /* emit the double's bit pattern as static data and load it.
           QBE lowers float constants to ".Lfp.N" labels, which it
           QUOTES in the emitted assembly (the name starts with a
           dot); tcc's built-in assembler cannot parse quoted labels,
           so float constants never appear in the IL. */
        uint64_t bits;
        memcpy(&bits, &n->as.float_literal, 8);
        char dref[64];
        unsigned id = ++g->fconst;
        sb_fmt(&g->data, "data $fconst.%u = { l %llu }\n", id,
               (unsigned long long)bits);
        snprintf(dref, sizeof(dref), "$fconst.%u", id);
        char t[64];
        new_tmp(g, t, sizeof(t));
        EMIT("\t%s =d loadd %s\n", t, dref);
        snprintf(v.ref, sizeof(v.ref), "%s", t);
        v.type = PITH_VALUE_FLOAT;
        v.owned = false;
        v.borrowed_arc = false;
        return v;
    }
    case AST_STRING_EXPR: {
        ExprResult v;
        emit_string_data(g, n->as.string_literal, n->string_len, v.ref);
        v.type = PITH_VALUE_STRING;
        v.owned = false;   /* static data, immortal */
        v.borrowed_arc = false;
        return v;
    }
    case AST_IDENTIFIER_EXPR: {
        const ScopeVar *var = cg_lookup(g, n->as.identifier);
        if (!var) {
            cg_error(g, n->loc,
                     pith_utf8_len(n->as.identifier,
                                   strlen(n->as.identifier)),
                     "use of undeclared identifier `%s`", n->as.identifier);
            return expr_dummy();
        }
        ExprResult v;
        emit_load(g, var, &v);
        return v;
    }
    case AST_MEMBER_ACCESS:
        return gen_member_access(g, n);
    case AST_UNARY_OP: {
        ExprResult operand = gen_expr(g, n->as.unary_op.operand);
        if (n->as.unary_op.op == TOK_KW_NOT) {
            char t[64];
            new_tmp(g, t, sizeof(t));
            if (operand.type == PITH_VALUE_FLOAT) {
                EMIT("\t%s =w ceqd %s, d_0.0\n", t, operand.ref);
            } else if (operand.type == PITH_VALUE_INT) {
                EMIT("\t%s =w ceql %s, 0\n", t, operand.ref);
            } else if (operand.type == PITH_VALUE_BOOL) {
                EMIT("\t%s =w ceqw %s, 0\n", t, operand.ref);
            } else {
                cg_error(g, n->loc, 3, "cannot apply `not` to %s",
                         value_kind_name(operand.type));
                release_owned(g, &operand);
                return expr_dummy();
            }
            release_owned(g, &operand);
            ExprResult v;
            snprintf(v.ref, sizeof(v.ref), "%s", t);
            v.type = PITH_VALUE_BOOL;
            v.owned = false;
            v.borrowed_arc = false;
            return v;
        }
        if (operand.type == PITH_VALUE_INT) {
            char t[64];
            new_tmp(g, t, sizeof(t));
            EMIT("\t%s =l neg %s\n", t, operand.ref);
            ExprResult v;
            snprintf(v.ref, sizeof(v.ref), "%s", t);
            v.type = PITH_VALUE_INT;
            v.owned = false;
            v.borrowed_arc = false;
            return v;
        }
        if (operand.type == PITH_VALUE_FLOAT) {
            char t[64];
            new_tmp(g, t, sizeof(t));
            EMIT("\t%s =d neg %s\n", t, operand.ref);
            ExprResult v;
            snprintf(v.ref, sizeof(v.ref), "%s", t);
            v.type = PITH_VALUE_FLOAT;
            v.owned = false;
            v.borrowed_arc = false;
            return v;
        }
        cg_error(g, n->loc, 1, "cannot negate %s",
                 value_kind_name(operand.type));
        release_owned(g, &operand);
        return expr_dummy();
    }
    case AST_BINARY_OP:
        if (n->as.binary_op.op == TOK_KW_AND)
            return gen_logical_and(g, n);
        if (n->as.binary_op.op == TOK_KW_OR)
            return gen_logical_or(g, n);
        return gen_binary(g, n);
    case AST_CALL_EXPR:
        return gen_call(g, n);
    default:
        cg_error(g, n->loc, 1, "internal error: unexpected expression node",
                 "");
        return expr_dummy();
    }
}

/* ------------------------------------------------------------------ */
/* Statements                                                         */
/* ------------------------------------------------------------------ */

static bool gen_block(Codegen *g, ASTBlock *block);

static void gen_assignment(Codegen *g, ASTNode *n)
{
    ASTAssignment *a = &n->as.assignment;

    /* 1. evaluate the value first - it may reference this variable */
    ExprResult v = gen_expr(g, a->value);

    ScopeVar *var = NULL;
    bool old_is_arc = false;

    if (a->is_declaration) {
        var = cg_lookup(g, a->var_name);
        if (var) {
            /* parser treats this as a declaration only when the name
               was unknown; matching lookup means a same-scope shadow
               snuck past (defensive) */
            cg_error(g, n->loc,
                     pith_utf8_len(a->var_name, strlen(a->var_name)),
                     "variable `%s` is already declared in this scope",
                     a->var_name);
            return;
        }
        var = calloc(1, sizeof(ScopeVar));
        if (!var) {
            fputs("pith: out of memory while generating QBE IR\n", stderr);
            exit(1);
        }
        var->name = strdup(a->var_name);
        if (!var->name) {
            fputs("pith: out of memory while generating QBE IR\n", stderr);
            exit(1);
        }
        var->var_type = PITH_VALUE_ERROR;
        var->is_arc = false;
        var->is_mut = a->is_mut;
        var->sized_type = a->has_explicit_type
                              ? a->sized_type
                              : PITH_SIZED_AUTO;
        var->slot = ++g->slot;
        var->next = g->scope->vars;
        g->scope->vars = var;

        /* sized allocation: small types use alloc4 (aligned), 8-byte
           types use alloc8 */
        char slot[160];
        var_slot_name(var, slot, sizeof(slot));
        if (sized_type_bytes(var->sized_type) <= 4)
            EMIT("\t%s =l alloc4 4\n", slot);
        else
            EMIT("\t%s =l alloc8 8\n", slot);
    } else {
        var = cg_lookup(g, a->var_name);
        if (!var) {
            cg_error(g, n->loc,
                     pith_utf8_len(a->var_name, strlen(a->var_name)),
                     "use of undeclared identifier `%s`", a->var_name);
            return;
        }
        old_is_arc = var->is_arc;
    }

    /* 2. when the new value borrows a refcounted variable, take our own
          reference BEFORE releasing the old one (keeps `x = x` safe) */
    bool new_is_arc = false;
    if (v.type == PITH_VALUE_STRING) {
        if (v.borrowed_arc) {
            EMIT("\tcall $pith_retain(l %s)\n", v.ref);
            new_is_arc = true;
        } else {
            new_is_arc = v.owned;   /* literals are static, not refcounted */
        }
    }

    /* 3. reassignment: release the previous value at this boundary */
    if (!a->is_declaration && old_is_arc) {
        char slot[160];
        var_slot_name(var, slot, sizeof(slot));
        char t[64];
        new_tmp(g, t, sizeof(t));
        EMIT("\t%s =l loadl %s\n", t, slot);
        EMIT("\tcall $pith_release(l %s)\n", t);
    }

    /* 4. store (truncated to the variable's storage width) */
    char slot[160];
    var_slot_name(var, slot, sizeof(slot));
    emit_store(g, &v, slot, var->sized_type);

    /* 5. record the variable's new type and ARC-ness */
    var->var_type = v.type == PITH_VALUE_ERROR ? var->var_type : v.type;
    var->is_arc = new_is_arc;
}

static void gen_print(Codegen *g, ASTNode *n)
{
    ExprResult v = gen_expr(g, n->as.print_stmt.expression);
    switch (v.type) {
    case PITH_VALUE_STRING:
        EMIT("\tcall $pith_rt_print(l %s)\n", v.ref);
        break;
    case PITH_VALUE_INT:
        EMIT("\tcall $pith_rt_print_int(l %s)\n", v.ref);
        break;
    case PITH_VALUE_BOOL:
        EMIT("\tcall $pith_rt_print_bool(w %s)\n", v.ref);
        break;
    default:
        if (v.type != PITH_VALUE_ERROR)
            cg_error(g, n->loc, 5, "print expects a printable value, not %s",
                     value_kind_name(v.type));
        break;
    }
    release_owned(g, &v);
}

/*
 * Lowers an if chain:
 *
 *     jnz %cond, @then_0, @check_1
 * @then_0   <body>  jmp @end
 * @check_1  (elseif cond1) jnz %cond1, @then_1, @check_2
 * @then_1   <body>  jmp @end
 * @check_2  (else)  <body>          -- falls through to @end
 * @end
 */
static void gen_branch(Codegen *g, ASTIfStmt *s, size_t idx, int end_lbl)
{
    ASTIfBranch *br;
    if (idx == 0)
        br = &s->then_branch;
    else if (idx <= s->elseif_count)
        br = &s->elseif_branches[idx - 1];
    else
        br = s->has_else ? &s->else_branch : NULL;

    if (!br)
        return;   /* no else: execution simply reaches @end */

    if (!br->condition) {
        /* the else body is the final check block; it falls through */
        gen_block(g, br->block);
        return;
    }

    ExprResult c = gen_expr(g, br->condition);
    int then_lbl = new_label(g);
    int next_lbl = new_label(g);

    switch (c.type) {
    case PITH_VALUE_BOOL:
    case PITH_VALUE_INT:
        /* jnz accepts word and (via subtyping) long temporaries */
        EMIT("\tjnz %s, @L%u, @L%u\n", c.ref, then_lbl, next_lbl);
        break;
    case PITH_VALUE_FLOAT: {
        char t[64];
        new_tmp(g, t, sizeof(t));
        EMIT("\t%s =w cned %s, d_0.0\n", t, c.ref);
        EMIT("\tjnz %s, @L%u, @L%u\n", t, then_lbl, next_lbl);
        break;
    }
    default:   /* STRING or ERROR: diagnosed; fall into the else chain */
        release_owned(g, &c);
        EMIT("\tjmp @L%u\n", next_lbl);
        break;
    }

    EMIT("@L%u\n", then_lbl);
    bool dead = gen_block(g, br->block);
    if (!dead)
        EMIT("\tjmp @L%u\n", end_lbl);
    EMIT("@L%u\n", next_lbl);

    gen_branch(g, s, idx + 1, end_lbl);
}

static void gen_if(Codegen *g, ASTNode *n)
{
    int end_lbl = new_label(g);
    gen_branch(g, &n->as.if_stmt, 0, end_lbl);
    EMIT("@L%u\n", end_lbl);
}

static void gen_while(Codegen *g, ASTNode *n)
{
    int head_lbl = new_label(g);
    int body_lbl = new_label(g);
    int exit_lbl = new_label(g);

    EMIT("\tjmp @L%u\n", head_lbl);
    EMIT("@L%u\n", head_lbl);

    ExprResult c = gen_expr(g, n->as.while_stmt.condition);
    switch (c.type) {
    case PITH_VALUE_BOOL:
    case PITH_VALUE_INT:
        EMIT("\tjnz %s, @L%u, @L%u\n", c.ref, body_lbl, exit_lbl);
        break;
    case PITH_VALUE_FLOAT: {
        char t[64];
        new_tmp(g, t, sizeof(t));
        EMIT("\t%s =w cned %s, d_0.0\n", t, c.ref);
        EMIT("\tjnz %s, @L%u, @L%u\n", t, body_lbl, exit_lbl);
        break;
    }
    default:
        release_owned(g, &c);
        EMIT("\tjmp @L%u\n", exit_lbl);
        break;
    }

    EMIT("@L%u\n", body_lbl);

    LoopCtx lctx;
    lctx.prev = g->loop_ctx;
    lctx.head_lbl = head_lbl;
    lctx.exit_lbl = exit_lbl;
    lctx.outer_scope = g->scope;
    g->loop_ctx = &lctx;

    bool dead = gen_block(g, n->as.while_stmt.body);

    g->loop_ctx = lctx.prev;

    if (!dead)
        EMIT("\tjmp @L%u\n", head_lbl);

    EMIT("@L%u\n", exit_lbl);
    g->block_dead = false;
}

static void gen_break(Codegen *g, ASTNode *n)
{
    if (!g->loop_ctx) {
        cg_error(g, n->loc, 5, "`break` statement outside of a loop", "");
        return;
    }

    /* scope exit boundary: release every local ARC allocation
       allocated inside the loop body, up to the loop's outer scope */
    for (Scope *s = g->scope; s && s != g->loop_ctx->outer_scope; s = s->parent)
        emit_scope_releases(g, s);

    EMIT("\tjmp @L%u\n", g->loop_ctx->exit_lbl);
    g->block_dead = true;
}

static void gen_continue(Codegen *g, ASTNode *n)
{
    if (!g->loop_ctx) {
        cg_error(g, n->loc, 8, "`continue` statement outside of a loop", "");
        return;
    }

    /* scope exit boundary: release every local ARC allocation
       allocated inside the loop body, up to the loop's outer scope */
    for (Scope *s = g->scope; s && s != g->loop_ctx->outer_scope; s = s->parent)
        emit_scope_releases(g, s);

    EMIT("\tjmp @L%u\n", g->loop_ctx->head_lbl);
    g->block_dead = true;
}

static void gen_return(Codegen *g, ASTNode *n)
{
    ExprResult v;
    v.type = PITH_VALUE_INT;
    v.owned = false;
    v.borrowed_arc = false;
    snprintf(v.ref, sizeof(v.ref), "0");

    if (n->as.return_stmt.has_value)
        v = gen_expr(g, n->as.return_stmt.value);

    /* scope exit boundary: release every local ARC allocation,
       innermost scope first */
    for (Scope *s = g->scope; s; s = s->parent)
        emit_scope_releases(g, s);

    switch (v.type) {
    case PITH_VALUE_INT: {
        if (g->cur != &g->main || g->plugin_mode) {
            /* functions return l (64-bit): no truncation */
            EMIT("\tret %s\n", v.ref);
            break;
        }
        /* main/fns return w; truncate the 64-bit int through a
           scratch slot (storel then loadw) */
        char t1[64], t2[64];
        unsigned a = new_tmp(g, t1, sizeof(t1));
        new_tmp(g, t2, sizeof(t2));
        EMIT("\t%s =l alloc8 8\n", t1);
        EMIT("\tstorel %s, %s\n", v.ref, t1);
        EMIT("\t%s =w loadw %s\n", t2, t1);
        EMIT("\tret %s\n", t2);
        (void)a;
        break;
    }
    case PITH_VALUE_BOOL:
        EMIT("\tret %s\n", v.ref);
        break;
    case PITH_VALUE_STRING:
        cg_error(g, n->loc, 6,
                 "cannot return a string value from a function "
                 "(expected int or boolean)", "");
        release_owned(g, &v);
        EMIT("\tret 0\n");
        break;
    default:   /* FLOAT or ERROR */
        if (v.type == PITH_VALUE_FLOAT)
            cg_error(g, n->loc, 5,
                     "cannot return a float value from a function "
                     "(expected int or boolean)", "");
        EMIT("\tret 0\n");
        break;
    }

    g->block_dead = true;
}

static void gen_fn_decl(Codegen *g, ASTNode *n)
{
    /* check if this exact AST node was already recorded */
    for (size_t i = 0; i < g->pending_fn_count; i++) {
        if (g->pending_fns[i].node == n)
            return;
    }

    char clean[128];
    qbe_sanitize(n->as.fn_decl.name, clean, sizeof(clean));

    /* whole-program dedup: the same fn in two units would collide */
    for (size_t i = 0; i < g->fn_sym_count; i++) {
        if (strcmp(g->fn_syms[i], clean) == 0) {
            cg_error(g, n->loc,
                     pith_utf8_len(n->as.fn_decl.name,
                                   strlen(n->as.fn_decl.name)),
                     "function `%s` is declared more than once",
                     n->as.fn_decl.name);
            return;
        }
    }
    if (g->fn_sym_count < sizeof(g->fn_syms) / sizeof(g->fn_syms[0]))
        g->fn_syms[g->fn_sym_count++] = strdup(clean);
    else {
        cg_error(g, n->loc, 1, "too many function declarations", "");
        return;
    }

    if (g->plugin_mode) {
        /* plugin mode: the fn is EXPORTED under the author scope's
           mangled name, installable and callable from consumers */
        char mangled[192];
        pith_cffi_mangled_name(g->plugin_author, g->plugin_module,
                               clean, mangled, sizeof(mangled));

        StrBuf *prev = g->cur;
        g->cur = &g->funcs;
        EMIT("export function l $%s() {\n", mangled);
        EMIT("@%s.start\n", mangled);
        bool dead = gen_block(g, n->as.fn_decl.body);
        EMIT("@%s.exit\n", mangled);
        EMIT("\tret 0\n}\n\n");
        if (dead)
            note(g, n->loc, 2,
                 "unreachable exit path: function body always returns");
        g->cur = prev;
        return;
    }

    if (g->pending_fn_count <
        sizeof(g->pending_fns) / sizeof(g->pending_fns[0])) {
        PendingFn *pf = &g->pending_fns[g->pending_fn_count++];
        snprintf(pf->clean, sizeof(pf->clean), "%s", clean);
        pf->node = n;
        pf->referenced = false;
        pf->emitted = false;
    }
}

static void emit_fn_body(Codegen *g, PendingFn *pf)
{
    pf->emitted = true;
    StrBuf *prev = g->cur;
    g->cur = &g->funcs;

    ASTFnDecl *decl = &pf->node->as.fn_decl;
    EMIT("function l $fn_%s(", pf->clean);
    for (size_t i = 0; i < decl->param_count; i++) {
        if (i > 0)
            EMIT(", ");
        EMIT("l %%.arg_%zu", i);
    }
    EMIT(") {\n");
    EMIT("@fn_%s.start\n", pf->clean);

    cg_scope_push(g);

    for (size_t i = 0; i < decl->param_count; i++) {
        ScopeVar *var = calloc(1, sizeof(ScopeVar));
        if (!var) {
            fputs("pith: out of memory while generating QBE IR\n", stderr);
            exit(1);
        }
        var->name = strdup(decl->params[i].name);
        if (!var->name) {
            fputs("pith: out of memory while generating QBE IR\n", stderr);
            exit(1);
        }
        var->var_type = (decl->params[i].sized_type == PITH_SIZED_F64 || decl->params[i].sized_type == PITH_SIZED_F32)
                        ? PITH_VALUE_FLOAT : PITH_VALUE_INT;
        var->is_arc = false;
        var->is_mut = true;
        var->sized_type = decl->params[i].sized_type;
        var->slot = ++g->slot;
        var->next = g->scope->vars;
        g->scope->vars = var;

        char slot[160];
        var_slot_name(var, slot, sizeof(slot));
        EMIT("\t%s =l alloc8 8\n", slot);
        EMIT("\tstorel %%.arg_%zu, %s\n", i, slot);
    }

    bool dead = gen_block(g, decl->body);

    if (!dead) {
        EMIT("@fn_%s.exit\n", pf->clean);
        emit_scope_releases(g, g->scope);
        EMIT("\tret 0\n");
    }
    EMIT("}\n\n");

    cg_scope_pop(g);

    if (dead)
        note(g, pf->node->loc, 2,
             "unreachable exit path: function body always returns");

    g->cur = prev;
}

static void gen_stmt(Codegen *g, ASTNode *n)
{
    switch (n->type) {
    case AST_ASSIGNMENT:
        gen_assignment(g, n);
        break;
    case AST_IF_STMT:
        gen_if(g, n);
        break;
    case AST_WHILE_STMT:
        gen_while(g, n);
        break;
    case AST_BREAK_STMT:
        gen_break(g, n);
        break;
    case AST_CONTINUE_STMT:
        gen_continue(g, n);
        break;
    case AST_PRINT_STMT:
        gen_print(g, n);
        break;
    case AST_FN_DECL:
        gen_fn_decl(g, n);
        break;
    case AST_RETURN_STMT:
        gen_return(g, n);
        break;
    case AST_IMPORT_STMT:
        /* handled at import-discovery time; the node is a marker */
        break;
    case AST_EXPR_STMT: {
        ExprResult v = gen_expr(g, n->as.expr_stmt.expr);
        release_owned(g, &v);
        break;
    }
    default:
        cg_error(g, n->loc, 1,
                 "internal error: unexpected statement node", "");
        break;
    }
}

/* ------------------------------------------------------------------ */
/* Blocks                                                             */
/* ------------------------------------------------------------------ */

/*
 * Emits one block's statements plus its scope-exit releases. Returns
 * whether the block always ends in a return (statements after a
 * return are unreachable and are skipped with a note).
 */
static bool gen_block(Codegen *g, ASTBlock *block)
{
    cg_scope_push(g);

    bool outer_dead = g->block_dead;
    g->block_dead = false;
    bool noted = false;

    for (size_t i = 0; i < block->count; i++) {
        if (g->block_dead) {
            if (!noted) {
                note(g, block->stmts[i]->loc, 1,
                     "unreachable code after `return`");
                noted = true;
            }
            break;
        }
        gen_stmt(g, block->stmts[i]);
    }

    bool dead = g->block_dead;
    if (!dead)
        emit_scope_releases(g, g->scope);

    g->block_dead = outer_dead;
    cg_scope_pop(g);
    return dead;
}

/* ------------------------------------------------------------------ */
/* Entry point                                                        */
/* ------------------------------------------------------------------ */

char *pith_gen_qbe(ASTBlock **programs, size_t unit_count,
                   const char **unit_paths, const char **unit_sources,
                   const PithImportUnit *imports, size_t nimports,
                   const PithPluginInfo *plugin, size_t *errors)
{
    Codegen gctx;
    Codegen *g = &gctx;
    memset(g, 0, sizeof(*g));
    sb_init(&g->data);
    sb_init(&g->funcs);
    sb_init(&g->main);
    g->cur = &g->main;
    g->unit_paths = unit_paths;
    g->unit_sources = unit_sources;
    g->unit_count = unit_count;
    g->imports = imports;
    g->nimports = nimports;
    g->plugin_mode = plugin && plugin->enabled;
    g->plugin_author = plugin ? plugin->author : NULL;
    g->plugin_module = plugin ? plugin->module : NULL;

    cg_scope_push(g);   /* the top-level scope */

    if (g->plugin_mode) {
        /* plugin mode: only fn declarations are emitted (exported
           under the author scope); there is no $main */
        for (size_t u = 0; u < unit_count; u++) {
            ASTBlock *unit = programs[u];
            for (size_t i = 0; i < unit->count; i++) {
                ASTNode *st = unit->stmts[i];
                if (st->type == AST_FN_DECL) {
                    gen_stmt(g, st);
                    continue;
                }
                note(g, st->loc, 1,
                     "plugin mode: top-level statements are ignored "
                     "(plugins export fn declarations only)");
            }
        }
        cg_scope_pop(g);

        if (errors)
            *errors = g->errors;
        if (g->errors > 0) {
            free(g->data.buf);
            free(g->funcs.buf);
            free(g->main.buf);
            for (size_t i = 0; i < g->fn_sym_count; i++)
                free(g->fn_syms[i]);
            return NULL;
        }

        StrBuf out;
        sb_init(&out);
        sb_put(&out, "# pith v" PITH_VERSION
                     " - QBE SSA plugin generated by pith (ARC on "
                     "strings)\n");
        if (g->data.len > 0) {
            sb_put(&out, "\n");
            sb_putn(&out, g->data.buf, g->data.len);
        }
        if (g->funcs.len > 0) {
            sb_put(&out, "\n");
            sb_putn(&out, g->funcs.buf, g->funcs.len);
        }
        sb_put(&out, "\n");

        free(g->data.buf);
        free(g->funcs.buf);
        free(g->main.buf);
        for (size_t i = 0; i < g->fn_sym_count; i++)
            free(g->fn_syms[i]);
        return out.buf;
    }

    /* Pre-pass: collect all function declarations */
    for (size_t u = 0; u < unit_count; u++) {
        ASTBlock *unit = programs[u];
        for (size_t i = 0; i < unit->count; i++) {
            if (unit->stmts[i]->type == AST_FN_DECL) {
                gen_fn_decl(g, unit->stmts[i]);
            }
        }
    }

    EMIT("export function w $main(w %%argc, l %%argv) {\n");
    EMIT("@main.start\n");
    EMIT("\tcall $pith_rt_init_args(w %%argc, l %%argv)\n");

    /* WPSSAC: every unit's top-level statements share the one $main */
    bool noted = false;
    for (size_t u = 0; u < unit_count; u++) {
        ASTBlock *unit = programs[u];
        for (size_t i = 0; i < unit->count; i++) {
            if (g->block_dead) {
                if (!noted) {
                    note(g, unit->stmts[i]->loc, 1,
                         "unreachable code after `return`");
                    noted = true;
                }
                break;
            }
            gen_stmt(g, unit->stmts[i]);
        }
        if (g->block_dead)
            break;
    }
    bool dead = g->block_dead;

    EMIT("@main.exit\n");
    if (!dead)
        emit_scope_releases(g, g->scope);   /* end of script: release all */
    EMIT("\tret 0\n}\n");

    /* WPSSAC zero-bloat: iteratively emit private functions that were referenced;
       eliminate unreferenced ones. */
    bool emitted_any;
    do {
        emitted_any = false;
        for (size_t i = 0; i < g->pending_fn_count; i++) {
            if (g->pending_fns[i].referenced && !g->pending_fns[i].emitted) {
                emit_fn_body(g, &g->pending_fns[i]);
                emitted_any = true;
            }
        }
    } while (emitted_any);

    for (size_t i = 0; i < g->pending_fn_count; i++) {
        if (!g->pending_fns[i].referenced) {
            note(g, g->pending_fns[i].node->loc,
                 pith_utf8_len(g->pending_fns[i].node->as.fn_decl.name,
                               strlen(g->pending_fns[i].node->as.fn_decl.name)),
                 "private function is never referenced; eliminated "
                 "(zero-bloat)");
        }
    }

    cg_scope_pop(g);

    if (errors)
        *errors = g->errors;

    if (g->errors > 0) {
        free(g->data.buf);
        free(g->funcs.buf);
        free(g->main.buf);
        for (size_t i = 0; i < g->fn_sym_count; i++)
            free(g->fn_syms[i]);
        return NULL;
    }

    /* assemble the final IR: header, data, named functions, main */
    StrBuf out;
    sb_init(&out);
    sb_put(&out, "# pith v" PITH_VERSION
                 " - QBE SSA generated by pith (ARC on strings)\n");
    if (g->data.len > 0) {
        sb_put(&out, "\n");
        sb_putn(&out, g->data.buf, g->data.len);
    }
    if (g->funcs.len > 0) {
        sb_put(&out, "\n");
        sb_putn(&out, g->funcs.buf, g->funcs.len);
    }
    sb_put(&out, "\n");
    sb_putn(&out, g->main.buf, g->main.len);

    free(g->data.buf);
    free(g->funcs.buf);
    free(g->main.buf);
    for (size_t i = 0; i < g->fn_sym_count; i++)
        free(g->fn_syms[i]);
    return out.buf;
}
