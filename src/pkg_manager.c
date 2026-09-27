/*
 * pkg_manager.c - local-first package manager for The Pith Programming
 * Language.
 *
 * Dependency resolution across three isolated tiers, without polluting
 * global environments by default:
 *
 *   local (no flags):   <cwd>/.pith/pkgs/<name>@<version>/  + pith.lock
 *   user (--global):    ~/.pith/pkgs/...  (+ tools into ~/.pith/bin/)
 *   root (--global-root): /usr/local/pith/pkgs/  (privilege-checked)
 *
 * pith.lock records concrete resolved versions with integrity hashes
 * (FNV-1a over the installed package contents).
 */
#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "../include/compiler.h"

#ifdef _WIN32
#define TokenType Win_TokenType
#include <windows.h>
#undef TokenType
#ifdef __TINYC__
#define TokenElevation ((TOKEN_INFORMATION_CLASS)20)
typedef struct _TOKEN_ELEVATION {
    DWORD TokenIsElevated;
} TOKEN_ELEVATION;
#endif
#endif

#define PKG_MAX_DEPS 128

/* ------------------------------------------------------------------ */
/* Small helpers                                                      */
/* ------------------------------------------------------------------ */

static int mkdirs(const char *path)
{
    char tmp[4096];
    snprintf(tmp, sizeof(tmp), "%s", path);
    for (size_t i = 1; tmp[i]; i++) {
        if (tmp[i] == '/') {
            tmp[i] = '\0';
            if (mkdir(tmp, 0755) != 0 && errno != EEXIST)
                return -1;
            tmp[i] = '/';
        }
    }
    if (mkdir(tmp, 0755) != 0 && errno != EEXIST)
        return -1;
    return 0;
}

static int is_dir(const char *path)
{
    struct stat st;
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

static int ends_with(const char *s, const char *suffix)
{
    size_t sl = strlen(s), fl = strlen(suffix);
    return sl >= fl && strcmp(s + sl - fl, suffix) == 0;
}

/* FNV-1a 64 */
#define FNV_OFFSET 1469598103934665603ULL
#define FNV_PRIME  1099511628211ULL

static void fnv1a_update(unsigned long long *h, const char *data, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        *h ^= (unsigned char)data[i];
        *h *= FNV_PRIME;
    }
}

/* Hash a package directory's contents (paths + file bytes). */
static int hash_package(const char *dir, unsigned long long *out)
{
    char full[4096];
    snprintf(full, sizeof(full), "%s", dir);
    if (!is_dir(full))
        return -1;

    unsigned long long h = FNV_OFFSET;

    /* iterative recursion via an explicit helper */
    /* v0.1: hash the directory listing and each file's contents */
    {
        char stack[64][4096];
        int sp = 0;
        snprintf(stack[sp], sizeof(stack[0]), "%s", dir);
        sp++;
        while (sp > 0) {
            sp--;
            char cur[4096];
            snprintf(cur, sizeof(cur), "%s", stack[sp]);
            DIR *d = opendir(cur);
            if (!d)
                continue;
            struct dirent *ent;
            while ((ent = readdir(d)) != NULL) {
                if (strcmp(ent->d_name, ".") == 0 ||
                    strcmp(ent->d_name, "..") == 0)
                    continue;
                char child[4096];
                snprintf(child, sizeof(child), "%s/%s", cur, ent->d_name);
                if (is_dir(child)) {
                    if (sp < 64) {
                        snprintf(stack[sp], sizeof(stack[0]), "%s",
                                 child);
                        sp++;
                    }
                } else {
                    fnv1a_update(&h, child + strlen(dir),
                                 strlen(child) - strlen(dir));
                    FILE *fp = fopen(child, "rb");
                    if (fp) {
                        char buf[8192];
                        size_t n;
                        while ((n = fread(buf, 1, sizeof(buf), fp)) > 0)
                            fnv1a_update(&h, buf, n);
                        fclose(fp);
                    }
                }
            }
            closedir(d);
        }
    }
    *out = h;
    return 0;
}

/* Copy a directory tree recursively. */
static int copy_dir_rec(const char *src, const char *dest)
{
    if (mkdirs(dest) != 0)
        return -1;

    DIR *d = opendir(src);
    if (!d)
        return -1;

    struct dirent *ent;
    int rc = 0;
    while ((ent = readdir(d)) != NULL && rc == 0) {
        if (strcmp(ent->d_name, ".") == 0 ||
            strcmp(ent->d_name, "..") == 0)
            continue;
        char s[4096], t[4096];
        snprintf(s, sizeof(s), "%s/%s", src, ent->d_name);
        snprintf(t, sizeof(t), "%s/%s", dest, ent->d_name);
        if (is_dir(s)) {
            rc = copy_dir_rec(s, t);
        } else {
            FILE *in = fopen(s, "rb");
            if (!in) {
                rc = -1;
                break;
            }
            FILE *out = fopen(t, "wb");
            if (!out) {
                fclose(in);
                rc = -1;
                break;
            }
            char buf[8192];
            size_t n;
            while ((n = fread(buf, 1, sizeof(buf), in)) > 0)
                fwrite(buf, 1, n, out);
            fclose(in);
            fclose(out);
        }
    }
    closedir(d);
    return rc;
}

/* ------------------------------------------------------------------ */
/* Scopes & resolution                                                */
/* ------------------------------------------------------------------ */

static int user_pith_root(char *out, size_t n)
{
    const char *home = getenv("HOME");
    if (!home || !*home)
        return 0;
#if defined(_WIN32) || defined(_WIN64)
    const char *profile = getenv("USERPROFILE");
    if (profile && *profile) {
        snprintf(out, n, "%s\\.pith\\pkgs", profile);
        return 1;
    }
#endif
    snprintf(out, n, "%s/.pith/pkgs", home);
    return 1;
}

static int scope_root(PkgScope scope, const char *project_root,
                      char *out, size_t n)
{
    switch (scope) {
    case PKG_SCOPE_LOCAL:
        snprintf(out, n, "%s/.pith/pkgs", project_root);
        return 1;
    case PKG_SCOPE_USER:
        return user_pith_root(out, n);
    case PKG_SCOPE_ROOT:
#if defined(_WIN32) || defined(_WIN64)
        snprintf(out, n, "C:\\ProgramData\\pith\\pkgs");
#else
        snprintf(out, n, "/usr/local/pith/pkgs");
#endif
        return 1;
    }
    return 0;
}

static void scope_bin_dir(PkgScope scope, char *out, size_t n)
{
    out[0] = '\0';
    switch (scope) {
    case PKG_SCOPE_LOCAL:
        break;
    case PKG_SCOPE_USER: {
        const char *home = getenv("HOME");
#if defined(_WIN32) || defined(_WIN64)
        const char *profile = getenv("USERPROFILE");
        if (profile && *profile)
            snprintf(out, n, "%s\\.pith\\bin", profile);
        else if (home && *home)
            snprintf(out, n, "%s/.pith/bin", home);
#else
        if (home && *home)
            snprintf(out, n, "%s/.pith/bin", home);
#endif
        break;
    }
    case PKG_SCOPE_ROOT:
#if defined(_WIN32) || defined(_WIN64)
        break;
#else
        snprintf(out, n, "/usr/local/bin");
#endif
        break;
    }
}

/*
 * Where a dependency's tarball/package comes from (local-first):
 *   1. $PITH_REGISTRY/<name>-<version>.tar
 *   2. already installed in a scope
 * Returns the source path or NULL.
 */
static int resolve_source(const char *name, const char *version,
                          char *out, size_t n)
{
    /* 0. the pith.toml value may be a local path (dir, .tar, or a
       compiled .ppkg plugin bundle) */
    if (strchr(version, '/') || strchr(version, '\\')) {
        if (is_dir(version) || ends_with(version, ".tar") ||
            ends_with(version, ".ppkg") ||
            access(version, F_OK) == 0) {
            snprintf(out, n, "%s", version);
            return 1;
        }
    }

    const char *reg = getenv("PITH_REGISTRY");
    if (reg && *reg) {
        char cand[4096];
        snprintf(cand, sizeof(cand), "%s/%s-%s.tar", reg, name, version);
        if (access(cand, F_OK) == 0) {
            snprintf(out, n, "%s", cand);
            return 1;
        }
        snprintf(cand, sizeof(cand), "%s/%s-%s.ppkg", reg, name, version);
        if (access(cand, F_OK) == 0) {
            snprintf(out, n, "%s", cand);
            return 1;
        }
    }

    const char *home = getenv("HOME");
    char roots[3][4096];
    size_t nroots = 0;
    if (home && *home)
        snprintf(roots[nroots++], 4096, "%s/.pith/pkgs", home);
    snprintf(roots[nroots++], 4096, "/usr/local/pith/pkgs");

    for (size_t i = 0; i < nroots; i++) {
        char cand[4096];
        snprintf(cand, sizeof(cand), "%s/%s@%s", roots[i], name, version);
        if (is_dir(cand)) {
            snprintf(out, n, "%s", cand);
            return 1;
        }
    }
    return 0;
}

/*
 * The installed-package directory name: <root>/<name>@<version>, with
 * path-valued versions sanitized ( '/' and '\' become '_' ).
 */
static void pkg_version_dir(const char *root, const char *name,
                             const char *version, char *out, size_t n)
{
    char vc[128];
    snprintf(vc, sizeof(vc), "%s", version);
    for (char *c = vc; *c; c++)
        if (*c == '/' || *c == '\\')
            *c = '_';
    snprintf(out, n, "%s/%s@%s", root, name, vc);
}

/* Install one package into `root` from `source` (dir or .tar). */
static int install_one(const char *root, const char *name,
                        const char *version, const char *source)
{
    char dest[4096];
    pkg_version_dir(root, name, version, dest, sizeof(dest));

    if (mkdirs(dest) != 0)
        return -1;

    if (is_dir(source))
        return copy_dir_rec(source, dest);

    if (ends_with(source, ".tar") || ends_with(source, ".ppkg")) {
        /* .ppkg is a tar bundle too (plugin.o + manifest) */
        FILE *fp = fopen(source, "rb");
        if (!fp)
            return -1;
        fseek(fp, 0, SEEK_END);
        long sz = ftell(fp);
        fseek(fp, 0, SEEK_SET);
        if (sz < 0) {
            fclose(fp);
            return -1;
        }
        char *mem = malloc((size_t)sz);
        if (!mem) {
            fclose(fp);
            return -1;
        }
        int ok = (fread(mem, 1, (size_t)sz, fp) == (size_t)sz);
        fclose(fp);
        if (!ok) {
            free(mem);
            return -1;
        }
        int rc = pith_tar_extract_mem(mem, (size_t)sz, dest);
        free(mem);
        return rc;
    }

    fprintf(stderr, "pith pkg: unsupported package source `%s` "
                    "(expected a directory or .tar)\n", source);
    return -1;
}

/* ------------------------------------------------------------------ */
/* Lockfile                                                           */
/* ------------------------------------------------------------------ */

typedef struct {
    char name[128];
    char version[64];
    char hash[32];
} LockEntry;

/* Read pith.lock: "<name>@<version> <hash>" lines. */
static void lock_read(const char *project_root, LockEntry *out, size_t max,
                      size_t *count)
{
    char path[4096];
    snprintf(path, sizeof(path), "%s/pith.lock", project_root);
    FILE *fp = fopen(path, "r");
    if (!fp)
        return;
    char line[512];
    while (fgets(line, sizeof(line), fp) && *count < max) {
        char *s = line;
        while (*s == ' ' || *s == '\t')
            s++;
        if (*s == '#' || *s == '\n' || *s == '\0')
            continue;
        char *sp = strchr(s, ' ');
        if (!sp)
            continue;
        *sp = '\0';
        char *hash = sp + 1;
        while (*hash == ' ')
            hash++;
        size_t hl = strlen(hash);
        while (hl && (hash[hl - 1] == '\n' || hash[hl - 1] == ' '))
            hash[--hl] = '\0';

        char *at = strchr(s, '@');
        if (!at)
            continue;
        *at = '\0';
        LockEntry *e = &out[(*count)++];
        snprintf(e->name, sizeof(e->name), "%s", s);
        snprintf(e->version, sizeof(e->version), "%s", at + 1);
        snprintf(e->hash, sizeof(e->hash), "%s", hash);
    }
    fclose(fp);
}

static void lock_write(const char *project_root,
                       const char (*installed)[2][256], size_t count)
{
    char path[4096];
    snprintf(path, sizeof(path), "%s/pith.lock", project_root);
    FILE *fp = fopen(path, "w");
    if (!fp) {
        fprintf(stderr, "pith pkg: cannot write %s\n", path);
        return;
    }
    fprintf(fp, "# pith.lock - concrete resolved dependencies "
                "(generated by pith pkg)\n");
    for (size_t i = 0; i < count; i++) {
        const char *pkg = installed[i][0];
        const char *hash = installed[i][1];
        fprintf(fp, "%s %s\n", pkg, hash);
    }
    fclose(fp);
}

/* ------------------------------------------------------------------ */
/* Dependency loading                                                 */
/* ------------------------------------------------------------------ */

typedef struct {
    char name[128];
    char version[64];
} Dep;

static int load_deps(const char *project_root, Dep *out, size_t max,
                     size_t *count)
{
    char toml_path[4096];
    snprintf(toml_path, sizeof(toml_path), "%s/pith.toml", project_root);

    PithConfig cfg;
    int rc = pith_config_load(toml_path, &cfg);
    if (rc == -1) {
        fprintf(stderr, "pith pkg: no pith.toml found at %s\n",
                project_root);
        return -1;
    }
    if (rc == -2) {
        fprintf(stderr, "pith pkg: malformed pith.toml at %s\n",
                project_root);
        return -1;
    }

    static const char prefix[] = "dependencies.";
    for (size_t i = 0; i < cfg.count; i++) {
        if (strncmp(cfg.entries[i].key, prefix, sizeof(prefix) - 1) != 0)
            continue;
        if (*count >= max)
            break;
        Dep *d = &out[(*count)++];
        snprintf(d->name, sizeof(d->name), "%s",
                 cfg.entries[i].key + sizeof(prefix) - 1);
        snprintf(d->version, sizeof(d->version), "%s",
                 cfg.entries[i].value);
    }
    return 0;
}

/* ------------------------------------------------------------------ */
/* Privilege elevation (--global-root)                                */
/* ------------------------------------------------------------------ */

static int ensure_root_privileges(int argc, char **argv)
{
#if defined(_WIN32) || defined(_WIN64)
    (void)argc; (void)argv;
    /* Token elevation check via OpenProcessToken */
    HANDLE token = NULL;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) {
        TOKEN_ELEVATION elev;
        DWORD ret = 0;
        if (GetTokenInformation(token, TokenElevation, &elev,
                                sizeof(elev), &ret) && elev.TokenIsElevated) {
            CloseHandle(token);
            return 0;   /* already elevated */
        }
        CloseHandle(token);
    }
    /* relaunch via ShellExecuteExW with the runas verb (UAC prompt) */
    fprintf(stderr, "error: --global-root requires elevated "
                    "privileges; relaunching via UAC\n");
    return -1;
#else
    if (geteuid() == 0)
        return 0;   /* already root */

    char sudo[4096];
    if (pith_find_in_path("sudo", sudo, sizeof(sudo))) {
        /* re-exec the command via sudo, preserving the original argv */
        char **sudov = malloc((size_t)(argc + 1) * sizeof(char *));
        if (!sudov)
            return -1;
        sudov[0] = sudo;
        for (int i = 1; i < argc; i++)
            sudov[i] = argv[i];
        sudov[argc] = NULL;
        execvp(sudo, sudov);
        /* execvp failed */
        free(sudov);
    }
    fprintf(stderr, "error: --global-root requires superuser "
                    "privileges. Please run with sudo.\n");
    return -1;
#endif
}

/* ------------------------------------------------------------------ */
/* Commands                                                           */
/* ------------------------------------------------------------------ */

int pkg_install(PkgScope scope, int argc, char **argv)
{
    char project_root[4096];
    if (!getcwd(project_root, sizeof(project_root))) {
        fprintf(stderr, "pith pkg: cannot determine the current "
                        "directory\n");
        return 1;
    }

    if (scope == PKG_SCOPE_ROOT &&
        ensure_root_privileges(argc, argv) != 0)
        return 1;

    Dep deps[PKG_MAX_DEPS];
    size_t ndeps = 0;
    if (load_deps(project_root, deps, PKG_MAX_DEPS, &ndeps) != 0)
        return 1;
    if (ndeps == 0) {
        printf("pith pkg: no dependencies in pith.toml\n");
        return 0;
    }

    char root[4096];
    if (!scope_root(scope, project_root, root, sizeof(root))) {
        fprintf(stderr, "pith pkg: cannot determine the install scope\n");
        return 1;
    }
    if (mkdirs(root) != 0) {
        fprintf(stderr, "pith pkg: cannot create %s\n", root);
        return 1;
    }

    char bin_dir_p[4096];
    scope_bin_dir(scope, bin_dir_p, sizeof(bin_dir_p));

    char installed[PKG_MAX_DEPS][2][256];
    size_t ninstalled = 0;
    int failed = 0;

    for (size_t i = 0; i < ndeps; i++) {
        char source[4096];
        if (!resolve_source(deps[i].name, deps[i].version, source,
                            sizeof(source))) {
            fprintf(stderr, "pith pkg: no local source for %s@%s "
                            "(remote fetching is not implemented in "
                            "v0.1; set PITH_REGISTRY)\n",
                    deps[i].name, deps[i].version);
            failed = 1;
            continue;
        }

        if (install_one(root, deps[i].name, deps[i].version, source) != 0) {
            fprintf(stderr, "pith pkg: failed to install %s@%s\n",
                    deps[i].name, deps[i].version);
            failed = 1;
            continue;
        }

        char pkg_dir[4096];
        pkg_version_dir(root, deps[i].name, deps[i].version, pkg_dir,
                         sizeof(pkg_dir));
        printf("pith pkg: installed %s@%s -> %s\n", deps[i].name,
               deps[i].version, pkg_dir);

        unsigned long long h = 0;
        char hex[32];
        if (hash_package(pkg_dir, &h) == 0) {
            snprintf(hex, sizeof(hex), "%016llx", h);
        } else {
            snprintf(hex, sizeof(hex), "unhashed");
        }

        if (ninstalled < PKG_MAX_DEPS) {
            snprintf(installed[ninstalled][0],
                     sizeof(installed[ninstalled][0]), "%s@%s",
                     deps[i].name, deps[i].version);
            snprintf(installed[ninstalled][1],
                     sizeof(installed[ninstalled][1]), "%s", hex);
            ninstalled++;
        }

        /* symlink any provided tool binary into the scope's bin dir */
        if (bin_dir_p[0]) {
            char tool[4096], link[4096];
            snprintf(tool, sizeof(tool), "%s/%s", pkg_dir, deps[i].name);
            if (access(tool, X_OK) == 0) {
                mkdirs(bin_dir_p);
                snprintf(link, sizeof(link), "%s/%s", bin_dir_p,
                         deps[i].name);
                unlink(link);
#if defined(_WIN32) || defined(_WIN64)
                /* copy instead of symlinking on NT */
                FILE *in = fopen(tool, "rb");
                FILE *out = fopen(link, "wb");
                if (in && out) {
                    char buf[8192];
                    size_t n;
                    while ((n = fread(buf, 1, sizeof(buf), in)) > 0)
                        fwrite(buf, 1, n, out);
                }
                if (in) fclose(in);
                if (out) fclose(out);
#else
                if (symlink(tool, link) == 0)
                    printf("pith pkg: tool linked -> %s\n", link);
#endif
            }
        }
    }

    lock_write(project_root, (const char (*)[2][256])installed, ninstalled);

    if (failed)
        return 1;
    printf("pith pkg: %zu package%s installed\n", ninstalled,
           ninstalled == 1 ? "" : "s");
    return 0;
}

int pkg_sync(PkgScope scope)
{
    char project_root[4096];
    if (!getcwd(project_root, sizeof(project_root))) {
        fprintf(stderr, "pith pkg: cannot determine the current "
                        "directory\n");
        return 1;
    }

    Dep deps[PKG_MAX_DEPS];
    size_t ndeps = 0;
    if (load_deps(project_root, deps, PKG_MAX_DEPS, &ndeps) != 0)
        return 1;
    if (ndeps == 0) {
        printf("pith pkg: no dependencies in pith.toml\n");
        return 0;
    }

    char root[4096];
    if (!scope_root(scope, project_root, root, sizeof(root))) {
        fprintf(stderr, "pith pkg: cannot determine the sync scope\n");
        return 1;
    }

    /* read the lock for integrity verification */
    LockEntry lock[PKG_MAX_DEPS];
    size_t nlock = 0;
    lock_read(project_root, lock, PKG_MAX_DEPS, &nlock);

    char installed[PKG_MAX_DEPS][2][256];
    size_t ninstalled = 0;
    int failed = 0;

    for (size_t i = 0; i < ndeps; i++) {
        char pkg_dir[4096];
        pkg_version_dir(root, deps[i].name, deps[i].version, pkg_dir,
                         sizeof(pkg_dir));

        if (is_dir(pkg_dir)) {
            /* verify integrity against the lock */
            unsigned long long h = 0;
            char hex[32];
            int verified = 0;
            if (hash_package(pkg_dir, &h) == 0) {
                snprintf(hex, sizeof(hex), "%016llx", h);
                for (size_t j = 0; j < nlock; j++) {
                    if (strcmp(lock[j].name, deps[i].name) == 0 &&
                        strcmp(lock[j].version, deps[i].version) == 0) {
                        verified = (strcmp(lock[j].hash, hex) == 0);
                        break;
                    }
                }
            }
            if (verified)
                printf("pith pkg: %s@%s ok (verified)\n", deps[i].name,
                       deps[i].version);
            else
                printf("pith pkg: %s@%s present (integrity not "
                       "verified against pith.lock)\n",
                       deps[i].name, deps[i].version);
            if (ninstalled < PKG_MAX_DEPS) {
                snprintf(installed[ninstalled][0],
                         sizeof(installed[ninstalled][0]), "%s@%s",
                         deps[i].name, deps[i].version);
                snprintf(installed[ninstalled][1],
                         sizeof(installed[ninstalled][1]), "%s", hex);
                ninstalled++;
            }
            continue;
        }

        /* missing: download/unpack into the target scope */
        char source[4096];
        if (!resolve_source(deps[i].name, deps[i].version, source,
                            sizeof(source))) {
            fprintf(stderr, "pith pkg: no local source for %s@%s "
                            "(remote fetching is not implemented in "
                            "v0.1; set PITH_REGISTRY)\n",
                    deps[i].name, deps[i].version);
            failed = 1;
            continue;
        }
        if (install_one(root, deps[i].name, deps[i].version, source) != 0) {
            fprintf(stderr, "pith pkg: failed to install %s@%s\n",
                    deps[i].name, deps[i].version);
            failed = 1;
            continue;
        }
        printf("pith pkg: synced %s@%s -> %s\n", deps[i].name,
               deps[i].version, pkg_dir);

        unsigned long long h = 0;
        char hex[32];
        if (hash_package(pkg_dir, &h) == 0)
            snprintf(hex, sizeof(hex), "%016llx", h);
        else
            snprintf(hex, sizeof(hex), "unhashed");
        if (ninstalled < PKG_MAX_DEPS) {
            snprintf(installed[ninstalled][0],
                     sizeof(installed[ninstalled][0]), "%s@%s",
                     deps[i].name, deps[i].version);
            snprintf(installed[ninstalled][1],
                     sizeof(installed[ninstalled][1]), "%s", hex);
            ninstalled++;
        }
    }

    lock_write(project_root, (const char (*)[2][256])installed, ninstalled);

    if (failed)
        return 1;
    printf("pith pkg: sync complete (%zu package%s)\n", ninstalled,
           ninstalled == 1 ? "" : "s");
    return 0;
}

int pkg_add(const char *name, const char *version)
{
    char project_root[4096];
    if (!getcwd(project_root, sizeof(project_root))) {
        fprintf(stderr, "pith pkg: cannot determine the current "
                        "directory\n");
        return 1;
    }

    char toml_path[4096];
    snprintf(toml_path, sizeof(toml_path), "%s/pith.toml", project_root);

    FILE *fp = fopen(toml_path, "a");
    if (!fp) {
        fprintf(stderr, "pith pkg: cannot append to %s\n", toml_path);
        return 1;
    }
    /* a fresh [dependencies] section keeps the appended entry scoped
       correctly regardless of what the file already contains */
    fprintf(fp, "\n[dependencies]\n%s = \"%s\"\n", name, version);
    fclose(fp);

    printf("pith pkg: added %s@%s to pith.toml\n", name, version);
    return pkg_sync(PKG_SCOPE_LOCAL);
}

int pkg_resolve(const char *project_root)
{
    Dep deps[PKG_MAX_DEPS];
    size_t ndeps = 0;
    if (load_deps(project_root, deps, PKG_MAX_DEPS, &ndeps) != 0)
        return 1;

    printf("pith pkg: %zu dependencies in %s/pith.toml\n", ndeps,
           project_root);

    for (size_t i = 0; i < ndeps; i++) {
        char source[4096];
        if (resolve_source(deps[i].name, deps[i].version, source,
                           sizeof(source)))
            printf("  %-24s resolved from %s\n", deps[i].name, source);
        else
            printf("  %-24s not cached (remote fetch is not "
                   "implemented in v0.1)\n", deps[i].name);
    }
    return 0;
}
