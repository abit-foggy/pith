/*
 * compiler.h - The Pith Programming Language, v0.1
 *
 * Shared vocabulary for the Pith frontend: source locations, tokens,
 * AST structures, lexical scopes, and the interfaces exposed by the
 * compiler stages (lexer, parser, QBE code generator, engine proxy,
 * package manager).
 *
 * Pith is a bracketless, semicolon-free language. Blocks open with a
 * keyword (if/fn) and close with a single `end`. Newlines separate
 * statements. Bindings have no `let`/`var`/annotations: an identifier
 * that is assigned before it exists in any enclosing scope is a
 * declaration, otherwise the assignment is a reassignment.
 */
#ifndef PITH_COMPILER_H
#define PITH_COMPILER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#ifdef _WIN32
#include <direct.h>
#include <io.h>
#ifndef mkdir
#define mkdir(p, m) _mkdir(p)
#endif
#endif

#define PITH_VERSION "0.1.0"

/* ------------------------------------------------------------------ */
/* EOF debug-workspace overlay footer (pith build --embed-source)     */
/* ------------------------------------------------------------------ */

#define PITH_DEBG_MAGIC "PITHDEBG"

typedef struct {
    uint64_t payload_size; /* Little-endian 64-bit unsigned integer */
    char     magic[8];     /* "PITHDEBG"                            */
} PithDebugFooter;

/* ------------------------------------------------------------------ */
/* QBE type tags                                                      */
/* ------------------------------------------------------------------ */

typedef enum {
    QBE_TYPE_WORD = 0,   /* w - 32-bit int (booleans)                 */
    QBE_TYPE_LONG = 1,    /* l - 64-bit int (integers, string pointers)*/
    QBE_TYPE_DOUBLE = 2   /* d - 64-bit float                          */
} QbeTypeTag;

/* ------------------------------------------------------------------ */
/* Source locations                                                   */
/* ------------------------------------------------------------------ */

typedef struct {
    const char *filepath;   /* borrowed, NUL-terminated                */
    size_t      line;       /* 1-based                                 */
    size_t      col;        /* 1-based, counts UTF-8 codepoints        */
    size_t      offset;     /* byte offset into the source buffer      */
} SourceLoc;

/* ------------------------------------------------------------------ */
/* Tokens                                                             */
/* ------------------------------------------------------------------ */

typedef enum {
    TOK_EOF = 0,
    TOK_NEWLINE,           /* statement delimiter                     */

    /* keywords */
    TOK_KW_IF,
    TOK_KW_ELSEIF,
    TOK_KW_ELSE,
    TOK_KW_END,
    TOK_KW_PRINT,
    TOK_KW_FN,
    TOK_KW_RETURN,
    TOK_KW_IMPORT,
    TOK_KW_MUT,
    TOK_KW_WHILE,
    TOK_KW_BREAK,
    TOK_KW_CONTINUE,
    TOK_KW_AND,
    TOK_KW_OR,
    TOK_KW_NOT,

    /* sized type keywords */
    TOK_TYPE_I8,
    TOK_TYPE_U8,
    TOK_TYPE_I16,
    TOK_TYPE_U16,
    TOK_TYPE_I32,
    TOK_TYPE_U32,
    TOK_TYPE_I64,
    TOK_TYPE_U64,
    TOK_TYPE_F32,
    TOK_TYPE_F64,

    /* identifiers & literals */
    TOK_IDENTIFIER,
    TOK_INT_LITERAL,
    TOK_FLOAT_LITERAL,
    TOK_STRING_LITERAL,

    /* operators */
    TOK_OP_ASSIGN,         /* =   */
    TOK_OP_EQ,              /* ==  */
    TOK_OP_NE,              /* !=  */
    TOK_OP_LT,              /* <   */
    TOK_OP_LE,              /* <=  */
    TOK_OP_GT,              /* >   */
    TOK_OP_GE,              /* >=  */
    TOK_OP_PLUS,            /* +   */
    TOK_OP_MINUS,           /* -   */
    TOK_OP_STAR,            /* *   */
    TOK_OP_SLASH,           /* /   */
    TOK_OP_DOT,             /* .   */
    TOK_OP_COLON,           /* :   */
    TOK_OP_LPAREN,          /* (   */
    TOK_OP_RPAREN,          /* )   */
    TOK_OP_COMMA,           /* ,   */

    TOK_ERROR               /* never produced on a successful lex     */
} TokenType;

typedef struct {
    TokenType  type;
    SourceLoc  loc;
    char      *lexeme;        /* owned; identifier text or decoded
                                 string literal bytes                  */
    size_t     lexeme_len;    /* lexeme byte length (strings may hold
                                 embedded NUL bytes)                   */
    long long  int_value;     /* valid for TOK_INT_LITERAL             */
    double     float_value;   /* valid for TOK_FLOAT_LITERAL           */
} Token;

typedef struct {
    Token  *tokens;
    size_t  count;
    size_t  capacity;
} TokenList;

/* ------------------------------------------------------------------ */
/* Pith value kinds (inferred during code generation)                 */
/* ------------------------------------------------------------------ */

typedef enum {
    PITH_VALUE_INT,
    PITH_VALUE_FLOAT,
    PITH_VALUE_STRING,     /* l - pointer to a refcounted PithString   */
    PITH_VALUE_BOOL,
    PITH_VALUE_ERROR       /* already-diagnosed type mismatch          */
} PithValueType;

/* ------------------------------------------------------------------ */
/* Sized storage types                                                 */
/* ------------------------------------------------------------------ */

typedef enum {
    PITH_SIZED_AUTO = 0,    /* infer from value: i64 or f64            */
    PITH_SIZED_I8,
    PITH_SIZED_U8,
    PITH_SIZED_I16,
    PITH_SIZED_U16,
    PITH_SIZED_I32,
    PITH_SIZED_U32,
    PITH_SIZED_I64,
    PITH_SIZED_U64,
    PITH_SIZED_F32,
    PITH_SIZED_F64,
} PithSizedType;

/* ------------------------------------------------------------------ */
/* AST                                                                */
/* ------------------------------------------------------------------ */

typedef struct ASTNode  ASTNode;
typedef struct ASTBlock ASTBlock;

struct ASTBlock {
    ASTNode **stmts;
    size_t    count;
    size_t    capacity;
};

/* var_name = value - records declaration vs reassignment */
typedef struct {
    char    *var_name;       /* owned                                    */
    ASTNode *value;          /* expression                               */
    bool     is_declaration; /* set by the parser's scope tracking      */
    bool     is_mut;         /* declared with `mut`                      */
    bool     has_explicit_type; /* declared with `: type`                */
    PithSizedType sized_type;   /* the explicit type, if any             */
} ASTAssignment;

/* one branch of an if chain; condition == NULL only for the else */
typedef struct {
    ASTNode  *condition;
    ASTBlock *block;
} ASTIfBranch;

/* if [elseif]* [else] end */
typedef struct {
    ASTIfBranch  then_branch;
    ASTIfBranch *elseif_branches;   /* owned array                      */
    size_t       elseif_count;
    ASTIfBranch  else_branch;
    bool         has_else;
} ASTIfStmt;

/* print <expr> */
typedef struct {
    ASTNode *expression;
} ASTPrintStmt;

/* base.member - e.g. os.identifyKernel */
typedef struct {
    ASTNode *base;          /* identifier expression or nested access   */
    char    *member;         /* owned member name                        */
} ASTMemberAccess;

/* left <op> right */
typedef struct {
    ASTNode  *left;
    ASTNode  *right;
    TokenType op;
} ASTBinaryOp;

/* -operand or not operand */
typedef struct {
    ASTNode  *operand;
    TokenType op;
} ASTUnaryOp;

typedef struct {
    char         *name;
    PithSizedType sized_type;
} ASTFnParam;

/* fn name(a, b) ... end */
typedef struct {
    char        *name;          /* owned                                    */
    ASTFnParam  *params;        /* arena-owned array                        */
    size_t       param_count;
    ASTBlock    *body;
} ASTFnDecl;

/* return [expr] */
typedef struct {
    ASTNode *value;
    bool     has_value;
} ASTReturnStmt;

/* import "path.c" - a native C import */
typedef struct {
    char *path;              /* owned; the .c file path                */
} ASTImportStmt;

/* callee(arg, ...) - a (possibly foreign) call */
typedef struct {
    ASTNode  *callee;        /* member access naming ns.fn             */
    ASTNode **args;          /* arena-owned array                      */
    size_t    arg_count;
} ASTCallExpr;

/* a bare call used as a statement */
typedef struct {
    ASTNode *expr;
} ASTExprStmt;

/* while cond ... end */
typedef struct {
    ASTNode  *condition;
    ASTBlock *body;
} ASTWhileStmt;

typedef enum {
    AST_ASSIGNMENT,
    AST_IF_STMT,
    AST_PRINT_STMT,
    AST_MEMBER_ACCESS,
    AST_BINARY_OP,
    AST_UNARY_OP,
    AST_IDENTIFIER_EXPR,
    AST_INT_EXPR,
    AST_FLOAT_EXPR,
    AST_STRING_EXPR,
    AST_FN_DECL,
    AST_RETURN_STMT,
    AST_IMPORT_STMT,
    AST_CALL_EXPR,
    AST_EXPR_STMT,
    AST_WHILE_STMT,
    AST_BREAK_STMT,
    AST_CONTINUE_STMT
} ASTNodeType;

struct ASTNode {
    ASTNodeType type;
    SourceLoc   loc;
    size_t      string_len;      /* AST_STRING_EXPR: payload byte length
                                     (embedded NULs are payload)        */
    union {
        ASTAssignment   assignment;
        ASTIfStmt       if_stmt;
        ASTPrintStmt    print_stmt;
        ASTMemberAccess member_access;
        ASTBinaryOp     binary_op;
        ASTUnaryOp      unary_op;
        ASTFnDecl       fn_decl;
        ASTReturnStmt   return_stmt;
        ASTImportStmt   import_stmt;
        ASTCallExpr     call;
        ASTExprStmt     expr_stmt;
        ASTWhileStmt    while_stmt;
        char           *identifier;      /* AST_IDENTIFIER_EXPR, owned  */
        long long       int_literal;     /* AST_INT_EXPR                */
        double          float_literal;   /* AST_FLOAT_EXPR              */
        char           *string_literal;  /* AST_STRING_EXPR, owned      */
    } as;
};

/* ------------------------------------------------------------------ */
/* Lexical scopes (parser: declaration detection,                     */
/*                 codegen: variable table + ARC bookkeeping)          */
/* ------------------------------------------------------------------ */

typedef struct ScopeVar {
    struct ScopeVar *next;
    char            *name;       /* owned                              */
    PithValueType    var_type;
    PithSizedType    sized_type; /* storage width (alloc/loads/stores) */
    bool             is_arc;     /* holds a refcounted heap value     */
    bool             is_mut;     /* reassignable?                     */
    unsigned         slot;       /* codegen: stack-slot number         */
} ScopeVar;

typedef struct Scope {
    ScopeVar        *vars;       /* innermost-first linked list       */
    struct Scope    *parent;
} Scope;

/* ------------------------------------------------------------------ */
/* Diagnostics (src/lexer.c)                                          */
/* ------------------------------------------------------------------ */

/*
 * Rust-style terminal diagnostic:
 *
 *   error: unexpected character '`'
 *     --> tests/bad.pi:3:7
 *      |
 *    3 | x = `hi`
 *      |       ^
 *
 * `severity` is "error", "warning", or "note". `span` is the width of
 * the caret underline, in codepoints. Colors are emitted when stderr
 * is a terminal. `source` may be NULL (no preview is printed then).
 */
void pith_emit_diagnostic(const char *severity, const char *message,
                          const char *filepath, const char *source,
                          size_t line, size_t col, size_t span);

/* number of UTF-8 codepoints in the first `bytes` bytes of `s` */
size_t pith_utf8_len(const char *s, size_t bytes);

/* ------------------------------------------------------------------ */
/* Lexer (src/lexer.c)                                                */
/* ------------------------------------------------------------------ */

/* Tokenizes the whole source; appends into `out` (reset by caller).  */
void pith_lex(const char *filepath, const char *source,
              TokenList *out, size_t *errors, size_t *warnings);

void pith_token_list_free(TokenList *list);

/* ------------------------------------------------------------------ */
/* Parser (src/parser.c)                                              */
/* ------------------------------------------------------------------ */

/*
 * Bump arena: all parser allocations (AST nodes, strings, scope
 * entries) come from here. Freeing is wholesale via pith_arena_free  - 
 * individual nodes are never freed one by one.
 */
typedef struct ArenaBlock {
    struct ArenaBlock *next;
    size_t used;
    size_t cap;
    size_t reserved;   /* keeps block data 16-byte aligned */
    /* block data follows */
} ArenaBlock;

typedef struct {
    ArenaBlock *head;
    ArenaBlock *tail;
} Arena;

void *pith_arena_alloc(Arena *a, size_t n);
void  pith_arena_free(Arena *a);

/*
 * Parses the token stream into a program block. All allocations come
 * from `arena`. Returns NULL when parsing failed (diagnostics already
 * emitted).
 */
ASTBlock *pith_parse(const char *filepath, const char *source,
                     TokenList *tokens, size_t *errors, Arena *arena);

/* ------------------------------------------------------------------ */
/* Native C imports (src/cffi.c)                                       */
/* ------------------------------------------------------------------ */

/*
 * Foreign function ABI types, mapping C declarations onto QBE IL
 * argument/return classes (System V AMD64 / AAPCS64):
 *   32-bit int/enum  -> w     64-bit int/pointer -> l
 *   float (single)   -> s     double             -> d
 */
typedef enum {
    PITH_FFI_VOID = 0,
    PITH_FFI_WORD,      /* w - 32-bit int, enum, bool                */
    PITH_FFI_LONG,      /* l - 64-bit int, pointer                   */
    PITH_FFI_SINGLE,    /* s - float                                 */
    PITH_FFI_DOUBLE     /* d - double                                */
} PithFfiType;

#define PITH_FFI_MAX_PARAMS 8
#define PITH_FFI_MAX_FNS    64

typedef struct {
    char       name[128];          /* the C symbol name               */
    PithFfiType ret;               /* PITH_FFI_VOID for void           */
    PithFfiType params[PITH_FFI_MAX_PARAMS];
    size_t      nparams;
    bool        ret_pith_value;    /* returns PithValue* (+1 ref)      */
} PithForeignFn;

/*
 * One imported .c translation unit and its discovered prototypes.
 *
 * Namespace scoping: the AUTHOR is derived from the import path's
 * directory part as written ("alice/os.c" -> author "alice", module
 * "os"); a bare "os.c" (no directory) is a root-level import whose
 * symbols join the merged module namespace directly.
 */
typedef struct {
    char ns[64];                   /* module name (basename sans .c)   */
    char author[64];               /* author scope ("" for root level) */
    char path[4096];               /* resolved .c file path; for plugin
                                       units, the plugin's object file  */
    bool is_plugin;                /* a compiled pith plugin (the path
                                       is a pre-built .o, not C source)*/
    PithForeignFn fns[PITH_FFI_MAX_FNS];
    size_t nfn;
} PithImportUnit;

/* The pre-baked <pith.h> text injected into import compilations. */
const char *pith_cffi_header_text(void);

/*
 * The author-aware mangled symbol name for an imported function:
 * `c_<author>_<module>_<name>` (or `c_<module>_<name>` for root-level
 * imports). This ONE name is used for the QBE call emission, the
 * compile-time C symbol rename (tcc_define_symbol), the JIT
 * registration, and the AOT object symbol, so no forwarding shims are
 * needed.
 */
void pith_cffi_mangled_name(const char *author, const char *module,
                            const char *fn, char *out, size_t n);

/* Scan a .c file's non-static function prototypes. 0 on success. */
int pith_cffi_scan_file(const char *path, PithImportUnit *out);

/* Map a C type spelling (e.g. "const char *") to an FFI class; sets
   *is_pith_value when the type names a PithValue or PithString. */
PithFfiType pith_cffi_map_type(const char *type_text,
                               bool *is_pith_value);

/* Does the builtin os namespace expose `name`? (public: used by the
   import discovery for override warnings) */
int pith_os_member_exists(const char *name);
int pith_proc_member_exists(const char *name);
int pith_fs_member_exists(const char *name);
int pith_net_member_exists(const char *name);

/* ------------------------------------------------------------------ */
/* QBE code generator (src/gen_qbe.c)                                 */
/* ------------------------------------------------------------------ */

/*
 * Whole-Program SSA Concatenation (WPSSAC): lowers EVERY translation
 * unit into a single consolidated .ssa module - all units' top-level
 * statements share the one exported $main entry point, project
 * functions stay private, and QBE folds constants and performs
 * register allocation across the whole application in one fast pass.
 *
 * `imports` carries discovered C import units: for each foreign
 * function a private namespaced shim is emitted
 * (`$c_<ns>_<name>`) that forwards to the real C symbol with the
 * typed System V AMD64 / AAPCS64 signature.
 *
 * `unit_paths`/`unit_sources` back each program block for per-unit
 * diagnostics. `plugin` (may be NULL) enables plugin mode: the
 * project's fn declarations are exported under author-namespaced
 * mangled symbols (`c_<author>_<module>_<name>`) and no $main is
 * emitted - the artifact is a linkable plugin, not an executable.
 *
 * Returns a NUL-terminated QBE .ssa buffer (owned by the caller).
 * Returns NULL on failure (diagnostics already emitted).
 */
typedef struct {
    bool        enabled;
    const char *author;    /* [project].author */
    const char *module;    /* [project].name   */
} PithPluginInfo;

char *pith_gen_qbe(ASTBlock **programs, size_t unit_count,
                   const char **unit_paths, const char **unit_sources,
                   const PithImportUnit *imports, size_t nimports,
                   const PithPluginInfo *plugin, size_t *errors);

/* ------------------------------------------------------------------ */
/* Config reader (src/config.c) - flat dotted-key TOML subset         */
/* ------------------------------------------------------------------ */

#define PITH_CONFIG_MAX_ENTRIES 128
#define PITH_CONFIG_KEY_MAX     128
#define PITH_CONFIG_VALUE_MAX   256

typedef struct {
    char key[PITH_CONFIG_KEY_MAX];       /* dotted: "tasks.build.run"  */
    char value[PITH_CONFIG_VALUE_MAX];
} PithConfigEntry;

typedef struct {
    PithConfigEntry entries[PITH_CONFIG_MAX_ENTRIES];
    size_t count;
} PithConfig;

/* Loads a pith.toml: 0 ok, -1 unreadable, -2 malformed. */
int pith_config_load(const char *path, PithConfig *out);

/* Look up a dotted key; returns the value or NULL. */
const char *pith_config_get(const PithConfig *cfg, const char *dotted_key);

/* ------------------------------------------------------------------ */
/* Tar archive (src/tar.c) - uncompressed ustar                      */
/* ------------------------------------------------------------------ */

/* Append one file entry for `path` to an output stream (append mode). */
int pith_tar_append_file(FILE *out, const char *path);

/* Append one file entry stored under an explicit archive name. */
int pith_tar_append_file_as(FILE *out, const char *path,
                            const char *entry_name);

/* Terminate an archive (two zero blocks). */
void pith_tar_finish(FILE *out);

/* Extract entries from a memory buffer into `dest_root` (created if
   missing, including intermediate directories). */
int pith_tar_extract_mem(const char *mem, size_t size, const char *dest_root);

/* ------------------------------------------------------------------ */
/* Engine proxy (src/engine_proxy.c)                                 */
/* ------------------------------------------------------------------ */

/* A runtime symbol registered into the execution engine. */
typedef struct {
    const char *name;
    const void *addr;
} RuntimeSymbol;

/*
 * Hot path: assemble `asm_src` (also available on disk at `asm_path`)
 * and execute its `main()` natively, in-memory, without touching a
 * heavyweight compiler driver.
 *
 *   - Linux / Windows NT / FreeBSD: libtcc (TCC_OUTPUT_MEMORY),
 *     runtime symbols registered via tcc_add_symbol, entrypoint
 *     fetched with tcc_get_symbol and called directly.
 *   - Darwin: ad-hoc signed temporary executable produced by the
 *     system clang at -O0, executed immediately.
 *
 * `extra_syms` (may be NULL) registers additional host functions  - 
 * used by the embeddable C ABI (pith_embed.h) so host-registered
 * functions are callable from evaluated pith code.
 *
 * `imports` (may be NULL) are native C import units: each .c file is
 * compiled into its own libtcc state in memory, relocated, and its
 * exported symbols are registered so the emitted shims resolve.
 *
 * Returns the program exit code, or -1 on engine failure.
 */
int engine_dispatch_run(const char *asm_src, const char *asm_path,
                        const RuntimeSymbol *extra_syms, size_t nextra,
                        const PithImportUnit *imports, size_t nimports);

/*
 * AOT: `as` the assembly into `obj_path`, then link against
 * `runtime_lib` and every imported unit's compiled object - in-process
 * via the embedded tcc (its built-in ELF linker) on Linux / Windows
 * NT / FreeBSD, via mold through the compiler driver on Darwin,
 * falling back to a tcc binary or the system linker. `prebuilt_objs`
 * (may be NULL) are already-compiled objects (e.g. installed pith
 * plugins) joined into the link.
 */
int engine_build_aot(const char *asm_path, const char *obj_path,
                     const char *output_path, const char *runtime_lib,
                     const PithImportUnit *imports, size_t nimports,
                     const char *const *prebuilt_objs, size_t nprebuilt);

/* Human-readable name of the active execution backend. */
const char *engine_backend_name(void);

/* Human-readable name of the linker used for AOT builds. */
const char *engine_aot_linker_name(void);

/* Host platform pith itself was built for: "linux"|"darwin"|"nt"|"freebsd". */
const char *engine_host_os(void);

/* Locate `tool` in $PATH; writes the path into `out` and returns it,
   or returns NULL when not found. */
const char *pith_find_in_path(const char *tool, char *out, size_t n);

/* Fill `out` with the path of runtime/libruntime.a, or return NULL. */
const char *engine_find_runtime_lib(char *out, size_t n);

/* Create a unique temp file under $TMPDIR (or /tmp); 0 on success. */
int pith_make_temp(const char *suffix, char *out, size_t n);

/* Directory containing libtcc1.a (auto-discovered); NULL if absent. */
const char *pith_tcc_dir(void);

/* Find the nearest pith.toml (cwd upwards); returns the path or NULL. */
int pith_find_config_upwards(char *out, size_t n);

/* Write text into a fresh temp file; 0 on success. */
int pith_stage_temp(const char *text, const char *suffix,
                    char *out, size_t n);

/* Run `qbe` over a staged .ssa file; returns the owned assembly text
   or NULL (diagnostic already printed). */
char *pith_qbe_lower(const char *ssa_path);

/* Lower QBE SSA directly from in-memory string; returns owned assembly text or NULL. */
char *pith_qbe_lower_string(const char *ssa_text);

/*
 * Toolchain version proxying: find the nearest pith.toml (cwd
 * upwards); when [toolchain].pithVersion names a different version,
 * forward this invocation to ~/.pith/toolchains/<ver>/bin/pith via
 * execv (POSIX) or CreateProcess (Win32). Returns 0 to continue with
 * the running binary; never returns when forwarding succeeds.
 */
int engine_toolchain_forward(int argc, char **argv);

/* `pith engine list`: available local toolchains + active default. */
int engine_toolchain_list(void);

/* `pith engine use <version>`: update the global default pointer. */
int engine_toolchain_use(const char *version);

/* `pith engine install <version>`: install the running toolchain into
   the local cache so the proxy can forward to it. */
int engine_toolchain_install(const char *version);

/* ------------------------------------------------------------------ */
/* Package manager (src/pkg_manager.c)                                */
/* ------------------------------------------------------------------ */

/*
 * Dependency resolution across three isolated scopes:
 *   local (default): <root>/.pith/pkgs/<name>@<version>/  + pith.lock
 *   user global:     ~/.pith/pkgs/...  (+ tools into ~/.pith/bin/)
 *   root global:     /usr/local/pith/pkgs/  (privilege-checked)
 */
typedef enum {
    PKG_SCOPE_LOCAL = 0,
    PKG_SCOPE_USER,
    PKG_SCOPE_ROOT
} PkgScope;

/* `pith pkg install [--global|--global-root]`; argc/argv are kept for
   the sudo re-exec path. */
int pkg_install(PkgScope scope, int argc, char **argv);

/* `pith pkg sync`: resolve [dependencies], install missing, write lock. */
int pkg_sync(PkgScope scope);

/* `pith pkg add <name> <version>`: append to pith.toml, then sync. */
int pkg_add(const char *name, const char *version);

/* `pith pkg` (no args): report dependency status for the project. */
int pkg_resolve(const char *project_root);

#endif /* PITH_COMPILER_H */
