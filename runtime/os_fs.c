/*
 * os_fs.c — platform identification and file descriptor primitives for
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
 * Kernel-level Darwin check: TRUE whenever uname reports "Darwin" —
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
