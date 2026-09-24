/*
 * main.c — CLI entrypoint for The Pith Programming Language.
 *
 *   pith run <file.pi> [more.pi]     compile & execute via the instant
 *                                    pipeline (QBE IR -> assembly ->
 *                                    libtcc in-memory)
 *   pith build <file.pi> [more.pi]   build a standalone native binary
 *                                    [--embed-source] attaches the
 *                                    workspace as an EOF overlay
 *   pith decompile <file.pi>         print the generated QBE SSA IR
 *   pith decompile <binary>          unpack an embedded debug workspace
 *   pith pkg [install|sync|add]      local-first package management
 *   pith engine [list|use|install]   toolchain version proxy
 *   pith <task> [args]               custom task from pith.toml [tasks]
 *   pith help                        show usage
 *   pith version                     print the version
 */
#define _POSIX_C_SOURCE 200809L

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>

#include "../include/compiler.h"

/* ------------------------------------------------------------------ */
/* File & path helpers                                                */
/* ------------------------------------------------------------------ */

static char *read_file(const char *path, size_t *out_len)
{
    FILE *fp = fopen(path, "rb");
    if (!fp)
        return NULL;

    size_t cap = 8192, len = 0;
    char *buf = malloc(cap);
    if (!buf) {
        fclose(fp);
        return NULL;
    }
    for (;;) {
        if (len + 4096 + 1 > cap) {
            cap *= 2;
            char *nb = realloc(buf, cap);
            if (!nb) {
                free(buf);
                fclose(fp);
                return NULL;
            }
            buf = nb;
        }
        size_t n = fread(buf + len, 1, 4096, fp);
        len += n;
        if (n == 0)
            break;
    }
    fclose(fp);
    buf[len] = '\0';
    if (out_len)
        *out_len = len;
    return buf;
}

static void cleanup_temp(char *path)
{
    if (path && path[0])
        unlink(path);
}

static int ends_with(const char *s, const char *suffix)
{
    size_t sl = strlen(s), fl = strlen(suffix);
    return sl >= fl && strcmp(s + sl - fl, suffix) == 0;
}

static int is_regular_file(const char *path)
{
    struct stat st;
    return stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

static void suffix_obj(const char *path, char *out, size_t n)
{
    size_t len = strlen(path);
    if (len > 2 && path[len - 2] == '.')
        len -= 2;
    snprintf(out, n, "%.*s.o", (int)len, path);
}

/* little-endian 64-bit serialization (portable) */
static void le64_put(unsigned char *p, uint64_t v)
{
    for (size_t i = 0; i < 8; i++)
        p[i] = (unsigned char)((v >> (8 * i)) & 0xFFu);
}

static uint64_t le64_get(const unsigned char *p)
{
    uint64_t v = 0;
    for (size_t i = 0; i < 8; i++)
        v |= (uint64_t)p[i] << (8 * i);
    return v;
}

/* ------------------------------------------------------------------ */
/* Frontend pipeline (multi-file WPSSAC)                              */
/* ------------------------------------------------------------------ */

/*
 * Lexes and parses EVERY translation unit into the bump arena, then
 * discovers native C imports (resolving paths relative to each unit's
 * directory, scanning prototypes) and runs one consolidated QBE
 * lowering pass over all of them. Returns the owned .ssa text, or
 * NULL on failure (diagnostics already emitted). `imports_out` (may
 * be NULL) receives a malloc'd array the caller frees.
 */
static char *compile_frontend(const char **paths, size_t count,
                              PithImportUnit **imports_out,
                              size_t *nimports_out)
{
    Arena arena;
    memset(&arena, 0, sizeof(arena));

    ASTBlock **programs = malloc(count * sizeof(ASTBlock *));
    const char **unit_paths = malloc(count * sizeof(char *));
    const char **unit_sources = malloc(count * sizeof(char *));
    if (!programs || !unit_paths || !unit_sources) {
        fprintf(stderr, "error: out of memory\n");
        free(programs);
        free(unit_paths);
        free(unit_sources);
        return NULL;
    }

    size_t total_errors = 0, total_warnings = 0;

    for (size_t i = 0; i < count; i++) {
        char *source = read_file(paths[i], NULL);
        if (!source) {
            fprintf(stderr, "error: cannot read %s\n", paths[i]);
            total_errors++;
            programs[i] = NULL;
            unit_paths[i] = paths[i];
            unit_sources[i] = "";
            continue;
        }

        TokenList tokens;
        memset(&tokens, 0, sizeof(tokens));

        size_t lex_errors = 0, lex_warnings = 0;
        pith_lex(paths[i], source, &tokens, &lex_errors, &lex_warnings);
        total_errors += lex_errors;
        total_warnings += lex_warnings;

        if (lex_errors > 0) {
            programs[i] = NULL;
        } else {
            size_t parse_errors = 0;
            programs[i] = pith_parse(paths[i], source, &tokens,
                                     &parse_errors, &arena);
            total_errors += parse_errors;
        }

        unit_paths[i] = paths[i];
        unit_sources[i] = source;
        pith_token_list_free(&tokens);
    }

    if (total_errors > 0) {
        if (total_warnings > 0)
            fprintf(stderr, "%zu warning%s emitted\n",
                    total_warnings, total_warnings == 1 ? "" : "s");
        fprintf(stderr, "error: aborting due to %zu previous error%s\n",
                total_errors, total_errors == 1 ? "" : "s");
        for (size_t i = 0; i < count; i++)
            free((char *)unit_sources[i]);
        free(programs);
        free(unit_paths);
        free(unit_sources);
        pith_arena_free(&arena);
        return NULL;
    }

    /* discover native C imports across every unit */
    PithImportUnit *imports = NULL;
    size_t nimports = 0;

    {
        size_t nstmt_imports = 0;
        for (size_t u = 0; u < count; u++)
            for (size_t i = 0; i < programs[u]->count; i++)
                if (programs[u]->stmts[i]->type == AST_IMPORT_STMT)
                    nstmt_imports++;

        if (nstmt_imports > 0) {
            imports = malloc(nstmt_imports * sizeof(PithImportUnit));
            if (!imports) {
                fprintf(stderr, "error: out of memory\n");
                for (size_t i = 0; i < count; i++)
                    free((char *)unit_sources[i]);
                free(programs);
                free(unit_paths);
                free(unit_sources);
                pith_arena_free(&arena);
                return NULL;
            }

            for (size_t u = 0; u < count; u++) {
                for (size_t i = 0; i < programs[u]->count; i++) {
                    ASTNode *st = programs[u]->stmts[i];
                    if (st->type != AST_IMPORT_STMT)
                        continue;

                    /* resolve relative to the importing unit's dir */
                    const char *ipath = st->as.import_stmt.path;
                    char resolved[4096];
                    if (ipath[0] == '/') {
                        snprintf(resolved, sizeof(resolved), "%s", ipath);
                    } else {
                        char dir[4096];
                        snprintf(dir, sizeof(dir), "%s", paths[u]);
                        char *slash = strrchr(dir, '/');
                        if (slash)
                            *slash = '\0';
                        else
                            snprintf(dir, sizeof(dir), ".");
                        snprintf(resolved, sizeof(resolved), "%s/%s",
                                 dir, ipath);
                    }

                    /* skip duplicate imports of the same unit */
                    int dup = 0;
                    for (size_t k = 0; k < nimports; k++) {
                        if (strcmp(imports[k].path, resolved) == 0) {
                            dup = 1;
                            break;
                        }
                    }
                    if (dup)
                        continue;

                    if (pith_cffi_scan_file(resolved,
                                            &imports[nimports]) != 0) {
                        fprintf(stderr, "error: cannot read or scan the "
                                        "imported C unit %s\n", resolved);
                        total_errors++;
                        continue;
                    }
                    nimports++;
                }
            }

            if (total_errors > 0) {
                fprintf(stderr, "error: aborting due to %zu previous "
                                "error%s\n",
                        total_errors, total_errors == 1 ? "" : "s");
                free(imports);
                for (size_t i = 0; i < count; i++)
                    free((char *)unit_sources[i]);
                free(programs);
                free(unit_paths);
                free(unit_sources);
                pith_arena_free(&arena);
                return NULL;
            }
        }
    }

    size_t gen_errors = 0;
    char *ssa = pith_gen_qbe(programs, count, unit_paths, unit_sources,
                             imports, nimports, &gen_errors);

    for (size_t i = 0; i < count; i++)
        free((char *)unit_sources[i]);
    free(programs);
    free(unit_paths);
    free(unit_sources);
    pith_arena_free(&arena);

    if (imports_out) {
        *imports_out = imports;
        *nimports_out = nimports;
    } else {
        free(imports);
    }

    if (gen_errors > 0 || !ssa) {
        fprintf(stderr, "error: aborting due to %zu previous error%s\n",
                gen_errors, gen_errors == 1 ? "" : "s");
        return NULL;
    }

    return ssa;
}

/* ------------------------------------------------------------------ */
/* Workspace overlay (embed-source)                                   */
/* ------------------------------------------------------------------ */

/* Append every .pi file in `dir` as tar entries; returns the count. */
static int append_pi_files(FILE *bin, const char *dir)
{
    DIR *d = opendir(dir);
    if (!d)
        return 0;
    struct dirent *ent;
    int appended = 0;
    while ((ent = readdir(d)) != NULL) {
        if (ent->d_name[0] == '.')
            continue;
        if (!ends_with(ent->d_name, ".pi"))
            continue;
        char full[4096];
        snprintf(full, sizeof(full), "%s/%s", dir, ent->d_name);
        if (!is_regular_file(full))
            continue;
        if (pith_tar_append_file(bin, full) == 0)
            appended++;
    }
    closedir(d);
    return appended;
}

/*
 * Appends the project workspace (pith.toml + the input scripts + all
 * .pi sources from the project root and src/) as an uncompressed tar
 * overlay at the EOF, followed by the 16-byte PithDebugFooter.
 */
static void attach_workspace(const char *binary_path,
                             const char **inputs, size_t input_count)
{
    FILE *bin = fopen(binary_path, "ab");
    if (!bin) {
        fprintf(stderr, "pith build: cannot attach the workspace "
                        "overlay to %s\n", binary_path);
        return;
    }

    long start = ftell(bin);

    int attached = 0;
    if (is_regular_file("pith.toml")) {
        if (pith_tar_append_file(bin, "pith.toml") == 0)
            attached++;
    }

    /* the input scripts themselves */
    for (size_t i = 0; i < input_count; i++) {
        if (is_regular_file(inputs[i])) {
            if (pith_tar_append_file(bin, inputs[i]) == 0)
                attached++;
        }
    }

    attached += append_pi_files(bin, ".");

    char src_dir[4096];
    snprintf(src_dir, sizeof(src_dir), "./src");
    attached += append_pi_files(bin, src_dir);

    pith_tar_finish(bin);

    long end = ftell(bin);
    uint64_t payload = (uint64_t)(end - start);

    PithDebugFooter footer;
    memset(&footer, 0, sizeof(footer));
    le64_put((unsigned char *)&footer.payload_size, payload);
    memcpy(footer.magic, PITH_DEBG_MAGIC, 8);
    fwrite(&footer, 1, sizeof(footer), bin);
    fclose(bin);

    printf("pith build: embedded workspace overlay (%d file%s, %llu "
           "bytes)\n", attached, attached == 1 ? "" : "s",
           (unsigned long long)payload);
}

/* ------------------------------------------------------------------ */
/* Commands                                                           */
/* ------------------------------------------------------------------ */

static int cmd_run(const char **paths, size_t count)
{
    PithImportUnit *imports = NULL;
    size_t nimports = 0;
    char *ssa = compile_frontend(paths, count, &imports, &nimports);
    if (!ssa)
        return 1;

    char ssa_path[4096], asm_path[4096];
    if (pith_stage_temp(ssa, ".ssa", ssa_path, sizeof(ssa_path)) != 0) {
        fprintf(stderr, "error: cannot create a temporary file\n");
        free(ssa);
        free(imports);
        return 1;
    }

    char *asm_src = pith_qbe_lower(ssa_path);
    free(ssa);
    if (!asm_src) {
        cleanup_temp(ssa_path);
        free(imports);
        return 1;
    }

    if (pith_stage_temp(asm_src, ".s", asm_path, sizeof(asm_path)) != 0) {
        fprintf(stderr, "error: cannot create a temporary file\n");
        free(asm_src);
        cleanup_temp(ssa_path);
        free(imports);
        return 1;
    }

    int rc = engine_dispatch_run(asm_src, asm_path, NULL, 0, imports,
                                 nimports);

    free(asm_src);
    free(imports);
    cleanup_temp(ssa_path);
    cleanup_temp(asm_path);
    return rc == -1 ? 1 : rc;
}

static int cmd_build(const char **paths, size_t count,
                     const char *out_override, int embed_flag)
{
    /* security default: stripped by default; embedding is triggered
       only by --embed-source or build.embedSource in pith.toml */
    int embed = embed_flag;
    if (!embed) {
        PithConfig cfg;
        if (pith_config_load("pith.toml", &cfg) == 0) {
            const char *v = pith_config_get(&cfg, "build.embedSource");
            if (v && strcmp(v, "true") == 0)
                embed = 1;
        }
    }

    PithImportUnit *imports = NULL;
    size_t nimports = 0;
    char *ssa = compile_frontend(paths, count, &imports, &nimports);
    if (!ssa)
        return 1;

    char ssa_path[4096], asm_path[4096], obj_path[4096];
    if (pith_stage_temp(ssa, ".ssa", ssa_path, sizeof(ssa_path)) != 0) {
        fprintf(stderr, "error: cannot create a temporary file\n");
        free(ssa);
        free(imports);
        return 1;
    }

    char *asm_src = pith_qbe_lower(ssa_path);
    free(ssa);
    if (!asm_src) {
        cleanup_temp(ssa_path);
        free(imports);
        return 1;
    }

    if (pith_stage_temp(asm_src, ".s", asm_path, sizeof(asm_path)) != 0) {
        fprintf(stderr, "error: cannot create a temporary file\n");
        free(asm_src);
        cleanup_temp(ssa_path);
        free(imports);
        return 1;
    }
    suffix_obj(asm_path, obj_path, sizeof(obj_path));

    /* default output: the first input's basename without .pi */
    char out_path[4096];
    if (out_override) {
        snprintf(out_path, sizeof(out_path), "%s", out_override);
    } else {
        const char *base = strrchr(paths[0], '/');
        base = base ? base + 1 : paths[0];
        size_t blen = strlen(base);
        if (blen > 3 && strcmp(base + blen - 3, ".pi") == 0)
            blen -= 3;
        snprintf(out_path, sizeof(out_path), "%.*s", (int)blen, base);
    }

    char rtlib[4096];
    if (!engine_find_runtime_lib(rtlib, sizeof(rtlib))) {
        fprintf(stderr, "error: runtime/libruntime.a not found "
                        "(build it with `make`, or set PITH_RUNTIME)\n");
        free(asm_src);
        free(imports);
        cleanup_temp(ssa_path);
        cleanup_temp(asm_path);
        return 1;
    }

    int rc = engine_build_aot(asm_path, obj_path, out_path, rtlib,
                              imports, nimports);

    free(asm_src);
    free(imports);
    cleanup_temp(ssa_path);
    cleanup_temp(asm_path);
    cleanup_temp(obj_path);

    if (rc != 0)
        return 1;

    if (embed) {
        attach_workspace(out_path, paths, count);
    }

    printf("built %s\n", out_path);
    return 0;
}

static int cmd_decompile_ir(const char **paths, size_t count)
{
    char *ssa = compile_frontend(paths, count, NULL, NULL);
    if (!ssa)
        return 1;

    fputs(ssa, stdout);
    free(ssa);
    return 0;
}

/*
 * `pith decompile <binary>`: self-healing workspace unpacking — read
 * the EOF PithDebugFooter, seek back to the payload, and extract the
 * archived files into ./restored_workspace/.
 */
static int cmd_decompile_binary(const char *path)
{
    FILE *fp = fopen(path, "rb");
    if (!fp) {
        fprintf(stderr, "error: cannot read %s\n", path);
        return 1;
    }

    if (fseek(fp, -16, SEEK_END) != 0) {
        fclose(fp);
        fprintf(stderr, "error: binary contains no embedded debug "
                        "workspace payload.\n");
        return 1;
    }

    unsigned char trailer[16];
    if (fread(trailer, 1, 16, fp) != 16) {
        fclose(fp);
        fprintf(stderr, "error: binary contains no embedded debug "
                        "workspace payload.\n");
        return 1;
    }

    if (memcmp(trailer + 8, PITH_DEBG_MAGIC, 8) != 0) {
        fclose(fp);
        fprintf(stderr, "error: binary contains no embedded debug "
                        "workspace payload.\n");
        return 1;
    }

    uint64_t payload = le64_get(trailer);
    if (payload == 0 || payload > (uint64_t)1 << 30) {
        fclose(fp);
        fprintf(stderr, "error: embedded payload has an implausible "
                        "size\n");
        return 1;
    }

    if (fseek(fp, -(long)(16 + (off_t)payload), SEEK_END) != 0) {
        fclose(fp);
        fprintf(stderr, "error: cannot seek to the embedded payload\n");
        return 1;
    }

    char *mem = malloc((size_t)payload);
    if (!mem) {
        fclose(fp);
        fprintf(stderr, "error: out of memory\n");
        return 1;
    }
    int ok = (fread(mem, 1, (size_t)payload, fp) == (size_t)payload);
    fclose(fp);
    if (!ok) {
        free(mem);
        fprintf(stderr, "error: cannot read the embedded payload\n");
        return 1;
    }

    int rc = pith_tar_extract_mem(mem, (size_t)payload,
                                  "restored_workspace");
    free(mem);
    if (rc != 0) {
        fprintf(stderr, "error: workspace extraction failed\n");
        return 1;
    }

    printf("restored workspace successfully extracted to "
           "./restored_workspace/\n");
    return 0;
}

static int cmd_decompile(const char **paths, size_t count)
{
    /* a non-.pi path is a binary for workspace unpacking */
    for (size_t i = 0; i < count; i++) {
        if (!ends_with(paths[i], ".pi")) {
            if (count > 1) {
                fprintf(stderr, "error: `pith decompile` with multiple "
                                "inputs expects only .pi files\n");
                return 2;
            }
            return cmd_decompile_binary(paths[i]);
        }
    }
    /* all .pi: decompile together (WPSSAC) */
    return cmd_decompile_ir(paths, count);
}

/* ------------------------------------------------------------------ */
/* Custom task runner                                                 */
/* ------------------------------------------------------------------ */

#ifdef _WIN32
static int spawn_and_wait(char **tokens)
{
    /* CreateProcess: build a command line from the tokens */
    char cmdline[16384];
    cmdline[0] = '\0';
    for (size_t i = 0; tokens[i]; i++) {
        if (i)
            strncat(cmdline, " ", sizeof(cmdline) - strlen(cmdline) - 1);
        strncat(cmdline, tokens[i], sizeof(cmdline) - strlen(cmdline) - 1);
    }
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);
    memset(&pi, 0, sizeof(pi));
    if (!CreateProcessA(NULL, cmdline, NULL, NULL, FALSE, 0, NULL, NULL,
                        &si, &pi))
        return -1;
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD code = 1;
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return (int)code;
}
#endif

static int run_custom_task(const char *verb, int argc, char **argv)
{
    char cfg_path[4096];
    if (!pith_find_config_upwards(cfg_path, sizeof(cfg_path))) {
        fprintf(stderr, "error: unknown command or task '%s'. Check "
                        "pith.toml [tasks].\n", verb);
        return 1;
    }

    PithConfig cfg;
    if (pith_config_load(cfg_path, &cfg) != 0) {
        fprintf(stderr, "error: cannot read %s\n", cfg_path);
        return 1;
    }

    /* `pith <verb> [subcommand]`: a subcommand inspects the deeper
       key first, falling back to the verb itself */
    const char *sub = (argc >= 3) ? argv[2] : NULL;
    const char *task = NULL;
    char key[512];
    if (sub) {
        snprintf(key, sizeof(key), "tasks.%s.%s", verb, sub);
        task = pith_config_get(&cfg, key);
    }
    if (!task) {
        snprintf(key, sizeof(key), "tasks.%s", verb);
        task = pith_config_get(&cfg, key);
    }
    if (!task) {
        fprintf(stderr, "error: unknown command or task '%s'. Check "
                        "pith.toml [tasks].\n", verb);
        return 1;
    }

    /* append any remaining arguments to the task command */
    char full[8192];
    snprintf(full, sizeof(full), "%s", task);
    size_t start = sub ? 3 : 2;
    for (int i = (int)start; i < argc; i++) {
        size_t len = strlen(full);
        if (len + strlen(argv[i]) + 2 >= sizeof(full))
            break;
        snprintf(full + len, sizeof(full) - len, " %s", argv[i]);
    }

    /* tokenize and execute via execvp (POSIX) / CreateProcess (NT) */
    char *tokens[256];
    size_t ntok = 0;
    char *p = full;
    while (*p && ntok + 1 < sizeof(tokens) / sizeof(tokens[0])) {
        while (*p == ' ' || *p == '\t')
            p++;
        if (!*p)
            break;
        tokens[ntok++] = p;
        while (*p && *p != ' ' && *p != '\t')
            p++;
        if (*p)
            *p++ = '\0';
    }
    tokens[ntok] = NULL;

#ifdef _WIN32
    int rc = spawn_and_wait(tokens);
    if (rc < 0) {
        fprintf(stderr, "error: failed to execute task '%s'\n", verb);
        return 1;
    }
    exit(rc);
#else
    execvp(tokens[0], tokens);
    /* execvp failed: a bare name may refer to a binary in the current
       directory (e.g. the freshly built `pith`) */
    if (strchr(tokens[0], '/') == NULL) {
        char local[4096];
        snprintf(local, sizeof(local), "./%s", tokens[0]);
        if (access(local, X_OK) == 0) {
            execv(local, tokens);
        }
    }
    fprintf(stderr, "error: failed to execute task '%s' (%s)\n", verb,
            tokens[0]);
    return 1;
#endif
}

/* ------------------------------------------------------------------ */
/* Usage & engine report                                              */
/* ------------------------------------------------------------------ */

static void print_help(void)
{
    printf("pith v" PITH_VERSION
           " — a dead-simple, bracketless systems-scripting language\n"
           "that compiles directly to native machine code via QBE.\n\n"
           "USAGE:\n"
           "    pith run <file.pi> [more.pi]   compile & execute via the "
           "instant pipeline\n"
           "    pith build <file.pi> [more.pi] [--embed-source] [-o out]\n"
           "                                 build a standalone native "
           "binary\n"
           "    pith decompile <file.pi>     print the generated QBE SSA "
           "IR\n"
           "    pith decompile <binary>      unpack an embedded debug "
           "workspace\n"
           "    pith pkg [install|sync|add]  local-first package "
           "management\n"
           "    pith engine [list|use|install]  toolchain version "
           "proxy\n"
           "    pith <task> [args]           run a custom task from "
           "pith.toml [tasks]\n"
           "    pith help                    display this usage\n"
           "    pith version                 print the version\n\n"
           "PKG FLAGS:\n"
           "    --global        install into the user home directory\n"
           "    --global-root   install machine-wide (requires sudo)\n\n"
           "ENVIRONMENT:\n"
           "    PITH_QBE       qbe binary (default: qbe)\n"
           "    PITH_AS       assembler (default: as)\n"
           "    PITH_CC       compiler driver / system linker "
           "(default: cc)\n"
           "    PITH_MOLD     mold linker override\n"
           "    PITH_TCC      tcc binary override\n"
           "    PITH_TCCDIR   directory containing libtcc1.a\n"
           "    PITH_RUNTIME  path to runtime/libruntime.a\n"
           "    PITH_CACHE    dependency cache directory\n"
           "    PITH_REGISTRY local package registry directory\n");
}

static int cmd_engine_report(void)
{
    char rtlib[4096];
    char qbe_path[4096], as_path[4096], mold_path[4096];
    const char *qbe = pith_find_in_path("qbe", qbe_path, sizeof(qbe_path));
    const char *as_bin = pith_find_in_path("as", as_path, sizeof(as_path));
    const char *mold = pith_find_in_path("mold", mold_path,
                                         sizeof(mold_path));

    printf("pith engine report\n");
    printf("  host os           : %s\n", engine_host_os());
    printf("  execution backend : %s\n", engine_backend_name());
    printf("  aot linker        : %s\n", engine_aot_linker_name());
    printf("  qbe               : %s\n", qbe ? qbe : "not found in PATH");
    printf("  as                : %s\n", as_bin ? as_bin : "not found in PATH");
    printf("  mold              : %s\n", mold ? mold : "not found in PATH "
           "(builds will use the tcc linker)");
    printf("  runtime library   : %s\n",
           engine_find_runtime_lib(rtlib, sizeof(rtlib))
               ? rtlib : "not found (run `make`)");
    return 0;
}

/* ------------------------------------------------------------------ */
/* Entrypoint                                                         */
/* ------------------------------------------------------------------ */

static int pith_main(int argc, char **argv);

int main(int argc, char **argv)
{
    int rc = pith_main(argc, argv);
    fflush(stdout);   /* abnormal exits must not discard buffered output */
    return rc;
}

static int pith_main(int argc, char **argv)
{
    if (argc < 2) {
        print_help();
        return 0;
    }

    /* toolchain version proxying: a project's [toolchain].pithVersion
       forwards to ~/.pith/toolchains/<ver>/bin/pith. Skipped for
       engine management so toolchains can always be administered. */
    if (strcmp(argv[1], "engine") != 0)
        engine_toolchain_forward(argc, argv);

    const char *cmd = argv[1];

    if (strcmp(cmd, "help") == 0 || strcmp(cmd, "--help") == 0 ||
        strcmp(cmd, "-h") == 0) {
        print_help();
        return 0;
    }
    if (strcmp(cmd, "version") == 0 || strcmp(cmd, "--version") == 0) {
        printf("pith " PITH_VERSION "\n");
        return 0;
    }

    if (strcmp(cmd, "engine") == 0) {
        if (argc < 3)
            return cmd_engine_report();
        if (strcmp(argv[2], "list") == 0)
            return engine_toolchain_list();
        if (strcmp(argv[2], "use") == 0) {
            if (argc < 4) {
                fprintf(stderr, "error: `pith engine use` expects a "
                                "version\n");
                return 2;
            }
            return engine_toolchain_use(argv[3]);
        }
        if (strcmp(argv[2], "install") == 0) {
            if (argc < 4) {
                fprintf(stderr, "error: `pith engine install` expects a "
                                "version\n");
                return 2;
            }
            return engine_toolchain_install(argv[3]);
        }
        fprintf(stderr, "error: unknown engine subcommand `%s`\n",
                argv[2]);
        return 2;
    }

    if (strcmp(cmd, "pkg") == 0) {
        if (argc < 3) {
            printf("usage: pith pkg install [--global|--global-root]\n"
                   "       pith pkg sync [--global|--global-root]\n"
                   "       pith pkg add <name> <version>\n");
            return 0;
        }
        if (strcmp(argv[2], "install") == 0) {
            PkgScope scope = PKG_SCOPE_LOCAL;
            for (int i = 3; i < argc; i++) {
                if (strcmp(argv[i], "--global") == 0)
                    scope = PKG_SCOPE_USER;
                else if (strcmp(argv[i], "--global-root") == 0)
                    scope = PKG_SCOPE_ROOT;
            }
            return pkg_install(scope, argc, argv);
        }
        if (strcmp(argv[2], "sync") == 0) {
            PkgScope scope = PKG_SCOPE_LOCAL;
            for (int i = 3; i < argc; i++) {
                if (strcmp(argv[i], "--global") == 0)
                    scope = PKG_SCOPE_USER;
                else if (strcmp(argv[i], "--global-root") == 0)
                    scope = PKG_SCOPE_ROOT;
            }
            return pkg_sync(scope);
        }
        if (strcmp(argv[2], "add") == 0) {
            if (argc < 5) {
                fprintf(stderr, "error: `pith pkg add` expects a name "
                                "and a version\n");
                return 2;
            }
            return pkg_add(argv[3], argv[4]);
        }
        fprintf(stderr, "error: unknown pkg subcommand `%s`\n", argv[2]);
        return 2;
    }

    if (strcmp(cmd, "run") == 0) {
        if (argc < 3) {
            fprintf(stderr, "error: `pith run` expects a script file\n");
            return 2;
        }
        const char *paths[64];
        size_t count = 0;
        for (int i = 2; i < argc && count < 64; i++) {
            if (strcmp(argv[i], "-o") == 0) {
                i++;   /* skip the flag's value */
                continue;
            }
            paths[count++] = argv[i];
        }
        if (count == 0) {
            fprintf(stderr, "error: `pith run` expects a script file\n");
            return 2;
        }
        return cmd_run(paths, count);
    }
    if (strcmp(cmd, "build") == 0) {
        if (argc < 3) {
            fprintf(stderr, "error: `pith build` expects a script file\n");
            return 2;
        }
        const char *out_override = NULL;
        int embed_flag = 0;
        const char *paths[64];
        size_t count = 0;
        for (int i = 2; i < argc && count < 64; i++) {
            if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
                out_override = argv[++i];
                continue;
            }
            if (strcmp(argv[i], "--embed-source") == 0) {
                embed_flag = 1;
                continue;
            }
            paths[count++] = argv[i];
        }
        if (count == 0) {
            fprintf(stderr, "error: `pith build` expects a script file\n");
            return 2;
        }
        return cmd_build(paths, count, out_override, embed_flag);
    }
    if (strcmp(cmd, "decompile") == 0) {
        if (argc < 3) {
            fprintf(stderr, "error: `pith decompile` expects a script "
                            "or binary file\n");
            return 2;
        }
        const char *paths[64];
        size_t count = 0;
        for (int i = 2; i < argc && count < 64; i++)
            paths[count++] = argv[i];
        return cmd_decompile(paths, count);
    }

    /* not a built-in: dispatch through pith.toml [tasks] */
    return run_custom_task(cmd, argc, argv);
}
