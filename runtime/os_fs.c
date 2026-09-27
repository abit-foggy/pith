/*
 * os_fs.c - platform identification and file descriptor primitives for
 * The Pith Programming Language runtime.
 *
 * Strings returned to compiled pith code are freshly allocated ARC
 * values: the compiler-injected releases at scope boundaries own and
 * free them.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../include/api.h"

#if !defined(_WIN32) && !defined(_WIN64)
#include <sys/utsname.h>
#endif

static PithValue *rt_str(const char *cstr)
{
    return pith_str_new(cstr, strlen(cstr));
}

PithValue *pith_rt_os_kernel(void)
{
#if defined(_WIN32) || defined(_WIN64) || defined(__NT__)
    return rt_str("nt");
#elif defined(__linux__)
    return rt_str("linux");
#elif defined(__APPLE__) || defined(__MACH__)
    return rt_str("darwin");
#elif defined(__FreeBSD__) || defined(__FreeBSD_kernel__)
    return rt_str("freebsd");
#else
    return rt_str("unknown");
#endif
}

PithValue *pith_rt_os_kernel_version(void)
{
#if defined(_WIN32) || defined(_WIN64) || defined(__NT__)
    /* Win32: a stable sentinel for v0.1. */
    return rt_str("nt-kernel");
#else
    struct utsname uts;
    if (uname(&uts) == 0 && uts.release[0])
        return rt_str(uts.release);
    return rt_str("unknown");
#endif
}

int32_t pith_rt_is_nt(void)
{
#if defined(_WIN32) || defined(_WIN64) || defined(__NT__)
    return 1;
#else
    return 0;
#endif
}

int32_t pith_rt_is_linux(void)
{
#if defined(__linux__)
    return 1;
#else
    return 0;
#endif
}

int32_t pith_rt_is_freebsd(void)
{
#if defined(__FreeBSD__) || defined(__FreeBSD_kernel__)
    return 1;
#else
    return 0;
#endif
}

/*
 * Kernel-level Darwin check: TRUE whenever uname reports "Darwin"  - 
 * this covers Apple macOS as well as non-Apple Darwin systems.
 */
int32_t pith_rt_is_darwin(void)
{
#if defined(_WIN32) || defined(_WIN64) || defined(__NT__)
    return 0;
#else
    struct utsname uts;
    if (uname(&uts) == 0)
        return strcmp(uts.sysname, "Darwin") == 0 ? 1 : 0;
    return 0;
#endif
}

/*
 * Apple macOS check: TRUE only on Apple's macOS (the Apple toolchain
 * defines __APPLE__ + __MACH__). Slightly different from is_darwin:
 * the kernel check above is true on any Darwin system, this one is
 * Apple-specific.
 */
int32_t pith_rt_is_macos(void)
{
#if defined(__APPLE__) && defined(__MACH__)
    return 1;
#else
    return 0;
#endif
}

void pith_rt_print(PithValue *str)
{
    if (!str)
        return;
    fwrite(str->data, 1, str->length, stdout);
    fputc('\n', stdout);
    fflush(stdout);
}

void pith_rt_print_int(int64_t val)
{
    printf("%lld\n", (long long)val);
    fflush(stdout);
}

void pith_rt_print_bool(int32_t val)
{
    printf("%s\n", val ? "true" : "false");
    fflush(stdout);
}

static int g_argc = 0;
static char **g_argv = NULL;

void pith_rt_init_args(int argc, char **argv)
{
    g_argc = argc;
    g_argv = argv;
}

int32_t pith_rt_arg_count(void)
{
    return (int32_t)g_argc;
}

PithValue *pith_rt_get_arg(int32_t index)
{
    if (index < 0 || index >= g_argc || !g_argv || !g_argv[index])
        return rt_str("");
    return rt_str(g_argv[index]);
}

PithValue *pith_rt_get_env(PithValue *key)
{
    if (!key || key->length == 0)
        return rt_str("");
    char k[256];
    size_t len = key->length < sizeof(k) - 1 ? key->length : sizeof(k) - 1;
    memcpy(k, key->data, len);
    k[len] = '\0';
    const char *val = getenv(k);
    if (!val)
        return rt_str("");
    return rt_str(val);
}

void pith_rt_exit(int32_t code)
{
    exit((int)code);
}

PithValue *pith_rt_file_read(PithValue *path)
{
    if (!path || path->length == 0)
        return rt_str("");
    char p[4096];
    size_t len = path->length < sizeof(p) - 1 ? path->length : sizeof(p) - 1;
    memcpy(p, path->data, len);
    p[len] = '\0';

    FILE *f = fopen(p, "rb");
    if (!f)
        return rt_str("");

    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return rt_str("");
    }
    long sz = ftell(f);
    if (sz < 0) {
        fclose(f);
        return rt_str("");
    }
    rewind(f);

    char *buf = malloc((size_t)sz + 1);
    if (!buf) {
        fclose(f);
        return rt_str("");
    }
    size_t read_bytes = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    buf[read_bytes] = '\0';

    PithValue *res = pith_str_new(buf, read_bytes);
    free(buf);
    return res;
}

int32_t pith_rt_file_write(PithValue *path, PithValue *content)
{
    if (!path || path->length == 0 || !content)
        return 0;
    char p[4096];
    size_t len = path->length < sizeof(p) - 1 ? path->length : sizeof(p) - 1;
    memcpy(p, path->data, len);
    p[len] = '\0';

    FILE *f = fopen(p, "wb");
    if (!f)
        return 0;

    size_t written = fwrite(content->data, 1, content->length, f);
    fclose(f);
    return written == content->length ? 1 : 0;
}
