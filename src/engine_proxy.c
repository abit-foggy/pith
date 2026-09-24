/*
 * engine_proxy.c — libtcc in-memory execution bridge, AOT dispatcher,
 * and transparent toolchain version proxy for The Pith Programming
 * Language.
 *
 * Hot path (pith run):
 *   QBE IR -> qbe -> assembly -> as -> .o -> libtcc (TCC_OUTPUT_MEMORY,
 *   runtime symbols registered, relocated in-memory) -> main() executed
 *   natively. No heavyweight compiler driver touches this path.
 *
 *   Darwin: ad-hoc signed temporary executable written by the system
 *   clang at -O0 and executed immediately.
 *
 * AOT (pith build):
 *   as -o temp.o script.s, then linked against runtime/libruntime.a
 *   in-process by the embedded tcc (its built-in ELF linker) on
 *   Linux / Windows NT / FreeBSD, or via mold through the compiler
 *   driver on Darwin — falling back to a tcc binary or the system
 *   linker when those are unavailable.
 *
 * Toolchain proxying:
 *   pith.toml [toolchain].pithVersion forwards invocations to
 *   ~/.pith/toolchains/<ver>/bin/pith, letting multiple compiler
 *   versions coexist per project.
 */
#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "../include/compiler.h"
#include "../include/api.h"

#if defined(PITH_HAVE_LIBTCC) && !defined(__APPLE__)
#define PITH_USE_TCC 1
#include <libtcc.h>
#endif

/* ------------------------------------------------------------------ */
/* Small process/path helpers                                         */
/* ------------------------------------------------------------------ */

static int run_cmd(const char *cmd)
{
    int st = system(cmd);
    if (st == -1)
        return -1;
    if (WIFEXITED(st))
        return WEXITSTATUS(st);
    if (WIFSIGNALED(st))
        return 128 + WTERMSIG(st);
    return -1;
}

/* Locate `tool` in $PATH. Writes into `out` and returns it, or NULL. */
const char *pith_find_in_path(const char *tool, char *out, size_t n)
{
    const char *path = getenv("PATH");
    if (!path || !out || n == 0)
        return NULL;

    const char *p = path;
    while (*p) {
        const char *e = p;
        while (*e && *e != ':')
            e++;
        size_t dlen = (size_t)(e - p);
        if (dlen > 0 && dlen + strlen(tool) + 2 < n) {
            snprintf(out, n, "%.*s/%s", (int)dlen, p, tool);
            if (access(out, X_OK) == 0)
                return out;
        }
        p = (*e == ':') ? e + 1 : e;
    }
    return NULL;
}

static int dir_has_file(const char *dir, const char *name)
{
    char path[4096];
    snprintf(path, sizeof(path), "%s/%s", dir, name);
    return access(path, F_OK) == 0;
}

/* Path of the running pith binary, or NULL. */
static const char *self_path(void)
{
    static char buf[4096];
#ifdef __linux__
    ssize_t n = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (n > 0) {
        buf[n] = '\0';
        return buf;
    }
#endif
    return NULL;
}

/* Directory of the running pith binary, or NULL. */
static const char *self_dir(void)
{
    const char *sp = self_path();
    if (!sp)
        return NULL;
    static char dir[4096];
    snprintf(dir, sizeof(dir), "%s", sp);
    char *slash = strrchr(dir, '/');
    if (!slash)
        return NULL;
    *slash = '\0';
    return dir;
}

/*
 * Find the nearest pith.toml, searching the current directory and
 * traversing upwards. Returns the path or NULL.
 */
int pith_find_config_upwards(char *out, size_t n)
{
    char dir[4096];
    if (!getcwd(dir, sizeof(dir)))
        return 0;

    for (;;) {
        char cand[4096];
        snprintf(cand, sizeof(cand), "%s/pith.toml", dir);
        if (access(cand, F_OK) == 0) {
            snprintf(out, n, "%s", cand);
            return 1;
        }
        /* traverse upwards */
        char *slash = strrchr(dir, '/');
        if (!slash)
            break;
        if (slash == dir) {
            /* filesystem root reached */
            snprintf(cand, sizeof(cand), "/pith.toml");
            if (access(cand, F_OK) == 0) {
                snprintf(out, n, "%s", cand);
                return 1;
            }
            break;
        }
        *slash = '\0';
    }
    return 0;
}

/* ------------------------------------------------------------------ */
/* Resource discovery                                                 */
/* ------------------------------------------------------------------ */

/*
 * Directory containing libtcc1.a, needed by tcc_relocate/tcc -B. First
 * hit wins: $PITH_TCCDIR, the vendored tcc tree (compiled-in path or
 * relative to the binary), then a system tcc install.
 */
static const char *tcc_dir(void)
{
    static char cached[4096];
    static int tried = 0;
    if (tried)
        return cached[0] ? cached : NULL;
    tried = 1;

    const char *env = getenv("PITH_TCCDIR");
    if (env && *env && dir_has_file(env, "libtcc1.a")) {
        snprintf(cached, sizeof(cached), "%s", env);
        return cached;
    }
#ifdef PITH_TCCDIR_ABS
    if (dir_has_file(PITH_TCCDIR_ABS, "libtcc1.a")) {
        snprintf(cached, sizeof(cached), "%s", PITH_TCCDIR_ABS);
        return cached;
    }
#endif
    const char *sd = self_dir();
    if (sd) {
        char cand[4096];
        /* installed layout: <prefix>/lib/pith/tcc (bin/pith sibling) */
        snprintf(cand, sizeof(cand), "%s/../lib/pith/tcc", sd);
        if (dir_has_file(cand, "libtcc1.a")) {
            char real[4096];
            if (realpath(cand, real)) {
                snprintf(cached, sizeof(cached), "%s", real);
                return cached;
            }
        }
        /* build-tree layout: <root>/vendor/tcc */
        snprintf(cand, sizeof(cand), "%s/vendor/tcc", sd);
        if (dir_has_file(cand, "libtcc1.a")) {
            char real[4096];
            if (realpath(cand, real)) {
                snprintf(cached, sizeof(cached), "%s", real);
                return cached;
            }
        }
        /* relocated build tree: <root>/../vendor/tcc */
        snprintf(cand, sizeof(cand), "%s/../vendor/tcc", sd);
        if (dir_has_file(cand, "libtcc1.a")) {
            char real[4096];
            if (realpath(cand, real)) {
                snprintf(cached, sizeof(cached), "%s", real);
                return cached;
            }
        }
    }
    if (dir_has_file("/usr/lib/tcc", "libtcc1.a"))
        return "/usr/lib/tcc";

    return NULL;
}

const char *engine_find_runtime_lib(char *out, size_t n)
{
    static char cached[4096];
    static int tried = 0;
    if (tried) {
        if (cached[0])
            snprintf(out, n, "%s", cached);
        return cached[0] ? cached : NULL;
    }
    tried = 1;

    const char *env = getenv("PITH_RUNTIME");
    if (env && *env && access(env, F_OK) == 0) {
        snprintf(cached, sizeof(cached), "%s", env);
        snprintf(out, n, "%s", cached);
        return cached;
    }
    if (access("runtime/libruntime.a", F_OK) == 0) {
        char real[4096];
        if (realpath("runtime/libruntime.a", real)) {
            snprintf(cached, sizeof(cached), "%s", real);
            snprintf(out, n, "%s", cached);
            return cached;
        }
    }
    const char *sd = self_dir();
    if (sd) {
        char cand[4096];
        /* installed layout: <prefix>/lib/pith/runtime (bin sibling) */
        snprintf(cand, sizeof(cand), "%s/../lib/pith/runtime/libruntime.a",
                 sd);
        if (access(cand, F_OK) == 0) {
            char real[4096];
            if (realpath(cand, real)) {
                snprintf(cached, sizeof(cached), "%s", real);
                snprintf(out, n, "%s", cached);
                return cached;
            }
        }
        /* build-tree layout: <root>/runtime/libruntime.a */
        snprintf(cand, sizeof(cand), "%s/runtime/libruntime.a", sd);
        if (access(cand, F_OK) == 0) {
            char real[4096];
            if (realpath(cand, real)) {
                snprintf(cached, sizeof(cached), "%s", real);
                snprintf(out, n, "%s", cached);
                return cached;
            }
        }
        snprintf(cand, sizeof(cand), "%s/../runtime/libruntime.a", sd);
        if (access(cand, F_OK) == 0) {
            snprintf(cached, sizeof(cached), "%s", cand);
            snprintf(out, n, "%s", cached);
            return cached;
        }
    }
#ifdef PITH_RUNTIME_ABS
    if (access(PITH_RUNTIME_ABS, F_OK) == 0) {
        snprintf(cached, sizeof(cached), "%s", PITH_RUNTIME_ABS);
        snprintf(out, n, "%s", cached);
        return cached;
    }
#endif
    return NULL;
}

/* ------------------------------------------------------------------ */
/* Temp helpers & QBE lowering (shared by the CLI and the embed ABI)  */
/* ------------------------------------------------------------------ */

int pith_make_temp(const char *suffix, char *out, size_t n)
{
    const char *dir = getenv("TMPDIR");
    if (!dir || !*dir)
        dir = "/tmp";
    char tmpl[4096];
    snprintf(tmpl, sizeof(tmpl), "%s/pith_XXXXXX", dir);

    int fd = mkstemp(tmpl);
    if (fd < 0)
        return -1;
    close(fd);

    /* mkstemp requires the X-run at the end of the template; rename to
       attach the requested suffix */
    if (suffix && *suffix) {
        char final[4096];
        snprintf(final, sizeof(final), "%s%s", tmpl, suffix);
        if (rename(tmpl, final) != 0) {
            unlink(tmpl);
            return -1;
        }
        snprintf(out, n, "%s", final);
        return 0;
    }

    snprintf(out, n, "%s", tmpl);
    return 0;
}

int pith_stage_temp(const char *text, const char *suffix,
                    char *out, size_t n)
{
    if (pith_make_temp(suffix, out, n) != 0)
        return -1;
    FILE *fp = fopen(out, "wb");
    if (!fp)
        return -1;
    size_t len = strlen(text);
    int ok = (fwrite(text, 1, len, fp) == len);
    fclose(fp);
    return ok ? 0 : -1;
}

char *pith_qbe_lower(const char *ssa_path)
{
    const char *qbe = getenv("PITH_QBE");
    if (!qbe || !*qbe)
        qbe = "qbe";

    char cmd[8192];
    snprintf(cmd, sizeof(cmd), "\"%s\" \"%s\"", qbe, ssa_path);

    FILE *fp = popen(cmd, "r");
    if (!fp)
        return NULL;

    size_t cap = 16384, len = 0;
    char *buf = malloc(cap);
    if (!buf) {
        pclose(fp);
        return NULL;
    }
    for (;;) {
        if (len + 4096 + 1 > cap) {
            cap *= 2;
            char *nb = realloc(buf, cap);
            if (!nb) {
                free(buf);
                pclose(fp);
                return NULL;
            }
            buf = nb;
        }
        size_t n = fread(buf + len, 1, 4096, fp);
        len += n;
        if (n == 0)
            break;
    }
    buf[len] = '\0';

    int st = pclose(fp);
    if (st == -1 || !WIFEXITED(st) || WEXITSTATUS(st) != 0 || len == 0) {
        fprintf(stderr, "error: qbe failed to lower the IR to assembly "
                        "(is qbe installed and in PATH? set PITH_QBE to "
                        "override)\n");
        free(buf);
        return NULL;
    }
    return buf;
}

/* ------------------------------------------------------------------ */
/* Runtime symbol table                                               */
/* ------------------------------------------------------------------ */

static const RuntimeSymbol runtime_syms[] = {
    { "pith_rt_os_kernel",         (const void *)pith_rt_os_kernel         },
    { "pith_rt_os_kernel_version", (const void *)pith_rt_os_kernel_version },
    { "pith_rt_is_nt",             (const void *)pith_rt_is_nt             },
    { "pith_rt_print",             (const void *)pith_rt_print             },
    { "pith_str_new",              (const void *)pith_str_new              },
    { "pith_str_concat",           (const void *)pith_str_concat           },
    { "pith_str_equals",           (const void *)pith_str_equals           },
    { "pith_retain",               (const void *)pith_retain               },
    { "pith_release",              (const void *)pith_release              },
    { "pith_break_cycle",          (const void *)pith_break_cycle          },
    /* FFI aliases used by imported C modules (see pith.h) */
    { "pithRetain",                (const void *)pithRetain                },
    { "pithRelease",               (const void *)pithRelease               },
    { "pithNewString",             (const void *)pithNewString             },
    { "pithNewStringN",            (const void *)pithNewStringN            },
    { "pithStringData",            (const void *)pithStringData            },
    { "pithStringLength",          (const void *)pithStringLength          },
    { "pithStringEquals",          (const void *)pithStringEquals          },
    { "pithStringConcat",          (const void *)pithStringConcat          },
    { "pith_net_socket",           (const void *)pith_net_socket           },
    { "pith_net_connect",          (const void *)pith_net_connect          },
    { "pith_net_send",             (const void *)pith_net_send             },
    { "pith_net_recv",             (const void *)pith_net_recv             },
    { "pith_net_close",            (const void *)pith_net_close            },
};

#define NSYMS (sizeof(runtime_syms) / sizeof(runtime_syms[0]))

const char *engine_host_os(void)
{
#if defined(_WIN32) || defined(_WIN64) || defined(__NT__)
    return "nt";
#elif defined(__linux__)
    return "linux";
#elif defined(__APPLE__) || defined(__MACH__)
    return "darwin";
#elif defined(__FreeBSD__) || defined(__FreeBSD_kernel__)
    return "freebsd";
#else
    return "unknown";
#endif
}
/* ------------------------------------------------------------------ */
/* Temp helpers                                                       */
/* ------------------------------------------------------------------ */

static int suffix_swap(const char *path, const char *new_suffix,
                       char *out, size_t n)
{
    size_t len = strlen(path);
    if (len > 2 && path[len - 2] == '.')
        len -= 2;   /* strip ".s" */
    snprintf(out, n, "%.*s%s", (int)len, path, new_suffix);
    return 0;
}

/* ------------------------------------------------------------------ */
/* Hot path: libtcc in-memory execution                               */
/* ------------------------------------------------------------------ */

/*
 * Stage the pre-baked <pith.h> virtual header into a fresh temp dir so
 * imported C sources can `#include <pith.h>`. Returns the directory.
 */
static int stage_ffi_header(char *dir_out, size_t n)
{
    const char *tmp = getenv("TMPDIR");
    if (!tmp || !*tmp)
        tmp = "/tmp";
    char tmpl[4096];
    snprintf(tmpl, sizeof(tmpl), "%s/pith_inc_XXXXXX", tmp);
    if (!mkdtemp(tmpl))
        return -1;

    char path[4096];
    snprintf(path, sizeof(path), "%s/pith.h", tmpl);
    FILE *fp = fopen(path, "w");
    if (!fp) {
        rmdir(tmpl);
        return -1;
    }
    fputs(pith_cffi_header_text(), fp);
    fclose(fp);

    snprintf(dir_out, n, "%s", tmpl);
    return 0;
}

static void cleanup_ffi_header(const char *dir)
{
    char path[4096];
    snprintf(path, sizeof(path), "%s/pith.h", dir);
    unlink(path);
    rmdir(dir);
}

#ifdef PITH_USE_TCC

/*
 * Compile one imported C unit into its own libtcc state, relocate it,
 * and register its exported functions into the main state under their
 * real symbol names (the emitted shims reference those). The returned
 * state must stay alive until execution finishes.
 */
static int compile_import_jit(TCCState *main_state,
                              const PithImportUnit *imp,
                              const char *inc_dir, const char *tdir,
                              TCCState **out_state)
{
    TCCState *cs = tcc_new();
    if (!cs)
        return -1;

    tcc_set_lib_path(cs, tdir);
    tcc_set_output_type(cs, TCC_OUTPUT_MEMORY);
    tcc_add_include_path(cs, inc_dir);

    /* the import's code calls the runtime: give it our symbols */
    for (size_t i = 0; i < NSYMS; i++)
        tcc_add_symbol(cs, runtime_syms[i].name, runtime_syms[i].addr);

    if (tcc_add_file(cs, imp->path) < 0) {
        fprintf(stderr, "pith engine: failed to compile the imported "
                        "C unit %s\n", imp->path);
        tcc_delete(cs);
        return -1;
    }
    if (tcc_relocate(cs) < 0) {
        fprintf(stderr, "pith engine: failed to relocate the imported "
                        "C unit %s\n", imp->path);
        tcc_delete(cs);
        return -1;
    }

    for (size_t f = 0; f < imp->nfn; f++) {
        void *addr = tcc_get_symbol(cs, imp->fns[f].name);
        if (!addr) {
            fprintf(stderr, "pith engine: imported unit %s does not "
                            "export `%s`\n", imp->path,
                    imp->fns[f].name);
            tcc_delete(cs);
            return -1;
        }
        tcc_add_symbol(main_state, imp->fns[f].name, addr);
    }

    *out_state = cs;
    return 0;
}

/*
 * Assemble the QBE assembly into an object, load it into a fresh tcc
 * context, register the runtime symbols (plus any host-registered
 * extras), relocate in memory and call main(). Returns the program
 * exit code, or -1 to signal the caller should fall back to the
 * temp-executable path.
 */
static int run_tcc(const char *asm_path, const RuntimeSymbol *extra_syms,
                   size_t nextra, const PithImportUnit *imports,
                   size_t nimports)
{
    char obj_path[4096];
    suffix_swap(asm_path, ".o", obj_path, sizeof(obj_path));

    const char *as_bin = getenv("PITH_AS");
    if (!as_bin || !*as_bin)
        as_bin = "as";
    char cmd[8192];
    snprintf(cmd, sizeof(cmd), "%s \"%s\" -o \"%s\" 2>/dev/null",
             as_bin, asm_path, obj_path);
    int rc = run_cmd(cmd);
    if (rc != 0)
        return -1;

    const char *tdir = tcc_dir();
    if (!tdir)
        return -1;

    TCCState *tcc = tcc_new();
    if (!tcc)
        return -1;

    /* must precede tcc_set_output_type: the {B}-substituted library
       paths are materialized there, so libtcc1.a is found in tdir */
    tcc_set_lib_path(tcc, tdir);
    tcc_set_output_type(tcc, TCC_OUTPUT_MEMORY);

    for (size_t i = 0; i < NSYMS; i++)
        tcc_add_symbol(tcc, runtime_syms[i].name, runtime_syms[i].addr);
    for (size_t i = 0; i < nextra; i++)
        tcc_add_symbol(tcc, extra_syms[i].name, extra_syms[i].addr);

    /* native C imports: compile each unit, register its exports */
    char inc_dir[4096];
    int have_inc = 0;
    TCCState *import_states[64];
    size_t nimport_states = 0;

    if (nimports > 0) {
        if (stage_ffi_header(inc_dir, sizeof(inc_dir)) != 0) {
            fprintf(stderr, "pith engine: cannot stage the <pith.h> "
                            "virtual header\n");
            tcc_delete(tcc);
            unlink(obj_path);
            return -1;
        }
        have_inc = 1;

        for (size_t i = 0; i < nimports && nimport_states < 64; i++) {
            TCCState *is = NULL;
            if (compile_import_jit(tcc, &imports[i], inc_dir, tdir,
                                   &is) != 0) {
                while (nimport_states > 0)
                    tcc_delete(import_states[--nimport_states]);
                cleanup_ffi_header(inc_dir);
                tcc_delete(tcc);
                unlink(obj_path);
                return -1;
            }
            import_states[nimport_states++] = is;
        }
    }

    if (tcc_add_file(tcc, obj_path) < 0) {
        fprintf(stderr, "pith engine: libtcc failed to load the "
                        "compiled object\n");
        while (nimport_states > 0)
            tcc_delete(import_states[--nimport_states]);
        if (have_inc)
            cleanup_ffi_header(inc_dir);
        tcc_delete(tcc);
        unlink(obj_path);
        return -1;
    }
    if (tcc_relocate(tcc) < 0) {
        fprintf(stderr, "pith engine: libtcc failed to relocate in "
                        "memory\n");
        while (nimport_states > 0)
            tcc_delete(import_states[--nimport_states]);
        if (have_inc)
            cleanup_ffi_header(inc_dir);
        tcc_delete(tcc);
        unlink(obj_path);
        return -1;
    }

    int (*entry)(void) = (int (*)(void))tcc_get_symbol(tcc, "main");
    if (!entry) {
        fprintf(stderr, "pith engine: entrypoint `main` not found\n");
        while (nimport_states > 0)
            tcc_delete(import_states[--nimport_states]);
        if (have_inc)
            cleanup_ffi_header(inc_dir);
        tcc_delete(tcc);
        unlink(obj_path);
        return -1;
    }

    rc = entry();

    /* the import states must outlive execution; free them now */
    while (nimport_states > 0)
        tcc_delete(import_states[--nimport_states]);
    if (have_inc)
        cleanup_ffi_header(inc_dir);
    tcc_delete(tcc);
    unlink(obj_path);
    return rc < 0 ? 255 : rc;   /* keep -1 reserved for engine failure */
}

#endif /* PITH_USE_TCC */

/*
 * Fallback / Darwin path: link a temporary executable with the system
 * compiler driver and execute it immediately.
 */
static int run_temp_exec(const char *asm_path)
{
    char rtlib[4096];
    if (!engine_find_runtime_lib(rtlib, sizeof(rtlib))) {
        fprintf(stderr, "pith engine: runtime/libruntime.a not found "
                        "(build it with `make`, or set PITH_RUNTIME)\n");
        return -1;
    }

    char exe_path[4096];
    suffix_swap(asm_path, ".bin", exe_path, sizeof(exe_path));

    const char *cc_bin = getenv("PITH_CC");
    if (!cc_bin || !*cc_bin)
        cc_bin = "cc";

    char cmd[16384];
#ifdef __APPLE__
    /* ad-hoc signing happens automatically in the link step */
    snprintf(cmd, sizeof(cmd),
             "clang -O0 \"%s\" \"%s\" -o \"%s\"", asm_path, rtlib, exe_path);
#else
    snprintf(cmd, sizeof(cmd),
             "%s \"%s\" \"%s\" -o \"%s\"", cc_bin, asm_path, rtlib, exe_path);
#endif

    if (run_cmd(cmd) != 0) {
        fprintf(stderr, "pith engine: failed to link the temporary "
                        "executable\n");
        return -1;
    }

    int rc = run_cmd(exe_path);
    unlink(exe_path);
    if (rc < 0)
        return -1;
    return rc == 256 ? 255 : (rc > 255 ? rc - 256 : rc);
}

int engine_dispatch_run(const char *asm_src, const char *asm_path,
                        const RuntimeSymbol *extra_syms, size_t nextra,
                        const PithImportUnit *imports, size_t nimports)
{
    (void)asm_src;   /* the assembly file at asm_path is what we run */

#ifdef PITH_USE_TCC
    int rc = run_tcc(asm_path, extra_syms, nextra, imports, nimports);
    if (rc >= 0)
        return rc;
    fprintf(stderr, "pith engine: falling back to the native "
                    "temp-executable path\n");
#endif

    return run_temp_exec(asm_path);
}

/* ------------------------------------------------------------------ */
/* AOT: as, then the embedded tcc linker (or mold on Darwin)          */
/* ------------------------------------------------------------------ */

int engine_build_aot(const char *asm_path, const char *obj_path,
                     const char *output_path, const char *runtime_lib,
                     const PithImportUnit *imports, size_t nimports)
{
    char imp_objs[64][4096];
    size_t nimp_objs = 0;
    char imp_args[4096];
    char inc_dir[4096];
    int have_inc = 0;
    imp_args[0] = '\0';

    /* 1. assemble the QBE output */
    const char *as_bin = getenv("PITH_AS");
    if (!as_bin || !*as_bin)
        as_bin = "as";
    char cmd[16384];
    snprintf(cmd, sizeof(cmd), "%s \"%s\" -o \"%s\"",
             as_bin, asm_path, obj_path);
    if (run_cmd(cmd) != 0) {
        fprintf(stderr, "pith engine: `as` failed to assemble the "
                        "generated assembly\n");
        return -1;
    }

    /* 1b. compile every imported C unit into an object file next to
          the Pith object; collect them into the link argument list */
    if (nimports > 0) {
        if (stage_ffi_header(inc_dir, sizeof(inc_dir)) != 0) {
            fprintf(stderr, "pith engine: cannot stage the <pith.h> "
                            "virtual header\n");
            return -1;
        }
        have_inc = 1;

        size_t base_len = strlen(obj_path);
        if (base_len > 2 &&
            strcmp(obj_path + base_len - 2, ".o") == 0)
            base_len -= 2;

        for (size_t i = 0; i < nimports && nimp_objs < 64; i++) {
            snprintf(imp_objs[nimp_objs], sizeof(imp_objs[0]),
                     "%.*s_imp%zu.o", (int)base_len, obj_path, i);

            int ok = 0;
#if defined(PITH_HAVE_LIBTCC)
            {
                const char *tdir0 = tcc_dir();
                TCCState *cs = tcc_new();
                if (cs) {
                    if (tdir0)
                        tcc_set_lib_path(cs, tdir0);
                    tcc_set_output_type(cs, TCC_OUTPUT_OBJ);
                    tcc_add_include_path(cs, inc_dir);
                    ok = tcc_add_file(cs, imports[i].path) == 0 &&
                         tcc_output_file(cs, imp_objs[nimp_objs]) == 0;
                    tcc_delete(cs);
                }
            }
#else
            {
                const char *cc0 = getenv("PITH_CC");
                if (!cc0 || !*cc0)
                    cc0 = "cc";
                char ccmd[16384];
                snprintf(ccmd, sizeof(ccmd),
                         "%s -c -I\"%s\" \"%s\" -o \"%s\"", cc0,
                         inc_dir, imports[i].path, imp_objs[nimp_objs]);
                ok = (run_cmd(ccmd) == 0);
            }
#endif
            if (!ok) {
                fprintf(stderr, "pith engine: failed to compile the "
                                "imported C unit %s\n", imports[i].path);
                goto aot_cleanup_fail;
            }
            nimp_objs++;
        }

        /* " \"p1\" \"p2\" ..." for the subprocess linkers */
        size_t at = 0;
        for (size_t i = 0; i < nimp_objs; i++) {
            int w = snprintf(imp_args + at, sizeof(imp_args) - at,
                             " \"%s\"", imp_objs[i]);
            if (w < 0 || (size_t)w >= sizeof(imp_args) - at)
                break;
            at += (size_t)w;
        }
    }

    /* 2. link */
    int link_rc = -1;
#ifdef __APPLE__
    /* Darwin: mold through the compiler driver (crt objects and libc
       join the link correctly), falling back to the system linker. */
    const char *mold = getenv("PITH_MOLD");
    if (mold && !*mold)
        mold = NULL;
    char mold_path[4096], clang_path[4096];
    if (!mold)
        mold = pith_find_in_path("mold", mold_path, sizeof(mold_path));
    const char *clang = pith_find_in_path("clang", clang_path,
                                          sizeof(clang_path));

    if (mold && clang) {
        snprintf(cmd, sizeof(cmd),
                 "clang -fuse-ld=mold -o \"%s\" \"%s\" \"%s\"%s",
                 output_path, obj_path, runtime_lib, imp_args);
        if (run_cmd(cmd) == 0) {
            fprintf(stderr, "pith engine: linked with mold\n");
            link_rc = 0;
            goto aot_done;
        }
    }
    if (mold) {
        snprintf(cmd, sizeof(cmd),
                 "cc -fuse-ld=mold -o \"%s\" \"%s\" \"%s\"%s",
                 output_path, obj_path, runtime_lib, imp_args);
        if (run_cmd(cmd) == 0) {
            fprintf(stderr, "pith engine: linked with mold\n");
            link_rc = 0;
            goto aot_done;
        }
    }
#else
#if defined(PITH_HAVE_LIBTCC)
    /* tcc is embedded: link in-process with its built-in ELF linker —
       no external linker or subprocess on this path. */
    {
        const char *tdir = tcc_dir();
        TCCState *tcc = tcc_new();
        int ok = 0;
        if (tcc) {
            /* must precede tcc_set_output_type: the {B}-substituted
               library paths (libtcc1.a) are materialized there */
            if (tdir)
                tcc_set_lib_path(tcc, tdir);
            tcc_set_output_type(tcc, TCC_OUTPUT_EXE);
            ok = tcc_add_file(tcc, obj_path) == 0;
            for (size_t i = 0; ok && i < nimp_objs; i++)
                ok = tcc_add_file(tcc, imp_objs[i]) == 0;
            if (ok)
                ok = tcc_add_file(tcc, runtime_lib) == 0 &&
                     tcc_output_file(tcc, output_path) == 0;
            tcc_delete(tcc);
        }
        if (ok) {
            fprintf(stderr, "pith engine: linked with the embedded tcc "
                            "linker\n");
            link_rc = 0;
            goto aot_done;
        }
        fprintf(stderr, "pith engine: embedded tcc linking failed, "
                        "falling back\n");
    }
#endif
    /* fallback: a tcc binary (vendored or from PATH) */
    const char *tdir = tcc_dir();
    if (tdir) {
        char tcc_bin[4096];
        snprintf(tcc_bin, sizeof(tcc_bin), "%s/tcc", tdir);
        if (access(tcc_bin, X_OK) == 0) {
            snprintf(cmd, sizeof(cmd),
                     "\"%s\" -B \"%s\" -o \"%s\" \"%s\" \"%s\"%s",
                     tcc_bin, tdir, output_path, obj_path, runtime_lib,
                     imp_args);
            if (run_cmd(cmd) == 0) {
                fprintf(stderr, "pith engine: linked with the tcc "
                                "linker\n");
                link_rc = 0;
                goto aot_done;
            }
        }
    }

    const char *tcc_bin_env = getenv("PITH_TCC");
    if (tcc_bin_env && *tcc_bin_env) {
        snprintf(cmd, sizeof(cmd),
                 "\"%s\" -o \"%s\" \"%s\" \"%s\"%s",
                 tcc_bin_env, output_path, obj_path, runtime_lib,
                 imp_args);
        if (run_cmd(cmd) == 0) {
            fprintf(stderr, "pith engine: linked with the tcc linker\n");
            link_rc = 0;
            goto aot_done;
        }
    } else {
        char tcc_path[4096];
        const char *tcc = pith_find_in_path("tcc", tcc_path,
                                            sizeof(tcc_path));
        if (tcc) {
            snprintf(cmd, sizeof(cmd),
                     "\"%s\" -o \"%s\" \"%s\" \"%s\"%s",
                     tcc, output_path, obj_path, runtime_lib, imp_args);
            if (run_cmd(cmd) == 0) {
                fprintf(stderr, "pith engine: linked with the tcc "
                                "linker\n");
                link_rc = 0;
                goto aot_done;
            }
        }
    }
#endif

    /* fallback: the system linker (compiler driver) */
    {
        const char *cc_bin = getenv("PITH_CC");
        if (!cc_bin || !*cc_bin)
            cc_bin = "cc";
        snprintf(cmd, sizeof(cmd),
                 "%s -o \"%s\" \"%s\" \"%s\"%s",
                 cc_bin, output_path, obj_path, runtime_lib, imp_args);
        if (run_cmd(cmd) != 0) {
            fprintf(stderr, "pith engine: linking failed\n");
            goto aot_cleanup_fail;
        }
        fprintf(stderr, "pith engine: linked with the system linker\n");
    }
    link_rc = 0;

aot_done:
    if (have_inc)
        cleanup_ffi_header(inc_dir);
    for (size_t i = 0; i < nimp_objs; i++)
        unlink(imp_objs[i]);
    return link_rc;

aot_cleanup_fail:
    if (have_inc)
        cleanup_ffi_header(inc_dir);
    for (size_t i = 0; i < nimp_objs; i++)
        unlink(imp_objs[i]);
    return -1;
}

/* ------------------------------------------------------------------ */
/* Backend reporting                                                  */
/* ------------------------------------------------------------------ */

const char *engine_backend_name(void)
{
#ifdef PITH_USE_TCC
    return "libtcc (in-memory)";
#elif defined(__APPLE__)
    return "darwin temp-exec (clang -O0)";
#else
    return "native temp-exec (system cc)";
#endif
}

/* Name of the linker used for `pith build` AOT artifacts. */
const char *engine_aot_linker_name(void)
{
#ifdef __APPLE__
    return "mold (via the compiler driver)";
#elif defined(PITH_HAVE_LIBTCC)
    return "tcc (embedded, in-process)";
#else
    return "tcc (from PATH) or system linker";
#endif
}

/* ------------------------------------------------------------------ */
/* Toolchain version proxy                                            */
/* ------------------------------------------------------------------ */

static int user_pith_dir(char *out, size_t n)
{
    const char *home = getenv("HOME");
    if (!home || !*home)
        return 0;
#if defined(_WIN32) || defined(_WIN64)
    /* %USERPROFILE%\.pith */
    const char *profile = getenv("USERPROFILE");
    if (profile && *profile) {
        snprintf(out, n, "%s\\.pith", profile);
        return 1;
    }
#endif
    snprintf(out, n, "%s/.pith", home);
    return 1;
}

int engine_toolchain_forward(int argc, char **argv)
{
    (void)argc;

    /* forwarding guard: never re-forward (prevents loops when the
       forwarded toolchain reads the same project config) */
    if (getenv("PITH_TOOLCHAIN_ACTIVE"))
        return 0;

    char cfg_path[4096];
    if (!pith_find_config_upwards(cfg_path, sizeof(cfg_path)))
        return 0;

    PithConfig cfg;
    if (pith_config_load(cfg_path, &cfg) != 0)
        return 0;

    const char *want = pith_config_get(&cfg, "toolchain.pithVersion");
    if (!want || !*want || strcmp(want, PITH_VERSION) == 0)
        return 0;   /* absent or matches the running binary */

    char base[4096];
    if (!user_pith_dir(base, sizeof(base)))
        return 0;

    char tool_bin[4096];
    snprintf(tool_bin, sizeof(tool_bin), "%s/toolchains/%s/bin/pith",
             base, want);

    if (access(tool_bin, X_OK) == 0) {
#ifdef _WIN32
        /* CreateProcess forwarding (does not replace the process) */
        (void)argv;
        char cmdline[16384];
        snprintf(cmdline, sizeof(cmdline), "\"%s\"", tool_bin);
        /* note: full argv forwarding is appended by the caller in a
           production build; v0.1 forwards the binary directly */
        return -1;   /* caller exits with the forwarded result */
#else
        /* Forward argc/argv directly, bypassing the rest of the host
           execution. execv replaces the process on success. The env
           guard prevents the forwarded binary from re-forwarding. */
        setenv("PITH_TOOLCHAIN_ACTIVE", want, 1);
        execv(tool_bin, argv);
        fprintf(stderr, "error: failed to forward to toolchain v%s\n",
                want);
        return 0;
#endif
    }

    fprintf(stderr, "info: toolchain v%s not found locally. Run "
                    "'pith engine install %s' to fetch it.\n", want, want);
    return 0;
}

int engine_toolchain_list(void)
{
    char base[4096];
    if (!user_pith_dir(base, sizeof(base))) {
        fprintf(stderr, "error: cannot determine the user home "
                        "directory\n");
        return 1;
    }

    char dir[4096];
    snprintf(dir, sizeof(dir), "%s/toolchains", base);

    /* active default pointer */
    char def[256] = "";
    char def_path[4096];
    snprintf(def_path, sizeof(def_path), "%s/default_version", base);
    FILE *fp = fopen(def_path, "r");
    if (fp) {
        if (!fgets(def, sizeof(def), fp))
            def[0] = '\0';
        fclose(fp);
        size_t len = strlen(def);
        while (len && (def[len - 1] == '\n' || def[len - 1] == '\r' ||
                       def[len - 1] == ' '))
            def[--len] = '\0';
    }

    printf("pith toolchains (%s):\n", dir);

    int any = 0;
#ifdef __linux__
    DIR *d = opendir(dir);
    if (d) {
        struct dirent *ent;
        while ((ent = readdir(d)) != NULL) {
            if (ent->d_name[0] == '.')
                continue;
            char bin[4096];
            snprintf(bin, sizeof(bin), "%s/%s/bin/pith", dir, ent->d_name);
            if (access(bin, X_OK) != 0)
                continue;
            any = 1;
            printf("  %s%s\n", ent->d_name,
                   (def[0] && strcmp(def, ent->d_name) == 0)
                       ? "   <- active default" : "");
        }
        closedir(d);
    }
#endif
    if (!any)
        printf("  (none installed)\n");
    return 0;
}

int engine_toolchain_use(const char *version)
{
    char base[4096];
    if (!user_pith_dir(base, sizeof(base))) {
        fprintf(stderr, "error: cannot determine the user home "
                        "directory\n");
        return 1;
    }

    mkdir(base, 0755);   /* ~/.pith */

    char def_path[4096];
    snprintf(def_path, sizeof(def_path), "%s/default_version", base);
    FILE *fp = fopen(def_path, "w");
    if (!fp) {
        fprintf(stderr, "error: cannot write %s\n", def_path);
        return 1;
    }
    fprintf(fp, "%s\n", version);
    fclose(fp);

    printf("pith: default toolchain set to v%s\n", version);
    return 0;
}

int engine_toolchain_install(const char *version)
{
    if (strcmp(version, PITH_VERSION) != 0) {
        fprintf(stderr, "info: remote toolchain fetching is not "
                        "implemented in v0.1; only the running "
                        "toolchain (v%s) can be installed\n",
                PITH_VERSION);
        return 1;
    }

    const char *sp = self_path();
    if (!sp) {
        fprintf(stderr, "error: cannot locate the running pith binary\n");
        return 1;
    }

    char base[4096];
    if (!user_pith_dir(base, sizeof(base))) {
        fprintf(stderr, "error: cannot determine the user home "
                        "directory\n");
        return 1;
    }

    char dest_dir[4096];
    snprintf(dest_dir, sizeof(dest_dir), "%s/toolchains/%s/bin",
             base, version);
    mkdir(dest_dir, 0755);   /* parents created via the walk below */

    /* create intermediate directories */
    {
        char tmp[4096];
        snprintf(tmp, sizeof(tmp), "%s", dest_dir);
        for (size_t i = 1; tmp[i]; i++) {
            if (tmp[i] == '/') {
                tmp[i] = '\0';
                mkdir(tmp, 0755);
                tmp[i] = '/';
            }
        }
        mkdir(tmp, 0755);
    }

    char dest[4096];
    snprintf(dest, sizeof(dest), "%s/pith", dest_dir);

    FILE *in = fopen(sp, "rb");
    if (!in) {
        fprintf(stderr, "error: cannot read %s\n", sp);
        return 1;
    }
    FILE *out = fopen(dest, "wb");
    if (!out) {
        fclose(in);
        fprintf(stderr, "error: cannot write %s\n", dest);
        return 1;
    }
    char buf[8192];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), in)) > 0)
        fwrite(buf, 1, n, out);
    fclose(in);
    fclose(out);
    chmod(dest, 0755);

    printf("pith: toolchain v%s installed to %s\n", version, dest);
    return 0;
}
