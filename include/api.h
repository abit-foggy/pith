/*
 * api.h - The Pith Programming Language
 *
 * Internal runtime ABI: everything the compiler frontend, engine, and
 * generated QBE code call into. The value object itself is defined by
 * the FFI header (pith.h), which this header includes - imported C
 * modules see the same canonical definition.
 *
 * ABI notes:
 *   - Integer/boolean values are passed as 64-bit/32-bit C integers.
 *   - Strings are PithValue pointers. Functions returning PithValue*
 *     transfer one reference to the caller; generated pith code
 *     releases it at scope boundaries. Parameters are borrowed.
 */
#ifndef PITH_API_H
#define PITH_API_H

#include <stddef.h>
#include <stdint.h>

#include "pith.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ */
/* ARC engine (runtime/memory.c)                                      */
/* ------------------------------------------------------------------ */

/* Allocate a string of `len` bytes, copying `initial` (may be NULL). */
PithValue *pith_str_new(const char *initial, size_t len);

/* New string holding a's bytes followed by b's. Caller owns it. */
PithValue *pith_str_concat(PithValue *a, PithValue *b);

/* Content equality; returns 1 if equal, 0 otherwise. */
int32_t pith_str_equals(PithValue *a, PithValue *b);

/* Atomically bump the reference count of a refcounted value. */
void pith_retain(void *ptr);

/* Atomically drop a reference; frees the value when it reaches zero. */
void pith_release(void *ptr);

/*
 * Explicit circular-reference mitigation: the caller zeroes the
 * back-reference slot in the parent's payload first, then hands the
 * parent/child pair here; the runtime drops the parent's strong
 * reference to the child cleanly, prior to scope exit.
 */
void pith_break_cycle(void *parent, void *child);

/* camelCase aliases exposed to imported C modules (see pith.h). */
void pithRetain(PithValue *val);
void pithRelease(PithValue *val);
PithValue *pithNewString(const char *cstr);
PithValue *pithNewStringN(const char *bytes, uint32_t len);
const char *pithStringData(PithValue *val);
uint32_t pithStringLength(PithValue *val);
int pithStringEquals(PithValue *a, PithValue *b);

/* ------------------------------------------------------------------ */
/* Platform & I/O runtime (runtime/os_fs.c)                           */
/* ------------------------------------------------------------------ */

/* "linux", "darwin", "nt", or "freebsd". Caller owns the result. */
PithValue *pith_rt_os_kernel(void);

/* Kernel/OS version string (uname.release on POSIX). Caller owns it. */
PithValue *pith_rt_os_kernel_version(void);

/* 1 on Windows NT, 0 elsewhere. */
int32_t pith_rt_is_nt(void);

/* 1 on Linux, 0 elsewhere. */
int32_t pith_rt_is_linux(void);

/* 1 on FreeBSD, 0 elsewhere. */
int32_t pith_rt_is_freebsd(void);

/* 1 when the kernel is Darwin (macOS and other Darwin systems). */
int32_t pith_rt_is_darwin(void);

/* 1 only on Apple macOS (Apple-specific, not generic Darwin). */
int32_t pith_rt_is_macos(void);

/* Print functions */
void pith_rt_print(PithValue *str);
void pith_rt_print_int(int64_t val);
void pith_rt_print_bool(int32_t val);

/* Process & Environment (proc.*) */
void pith_rt_init_args(int argc, char **argv);
int32_t pith_rt_arg_count(void);
PithValue *pith_rt_get_arg(int32_t index);
PithValue *pith_rt_get_env(PithValue *key);
int32_t pith_rt_proc_pid(void);
void pith_rt_exit(int32_t code);

/* File I/O */
PithValue *pith_rt_file_read(PithValue *path);
int32_t pith_rt_file_write(PithValue *path, PithValue *content);

/* ------------------------------------------------------------------ */
/* Network primitives (runtime/network.c) - POSIX/Win32 stubs          */
/* ------------------------------------------------------------------ */

int32_t pith_net_socket(int32_t domain, int32_t type, int32_t protocol);
int32_t pith_net_connect(int32_t fd, const char *host, int32_t port);
int32_t pith_net_send(int32_t fd, const char *buf, int32_t len);
int32_t pith_net_recv(int32_t fd, char *buf, int32_t len);
int32_t pith_net_close(int32_t fd);

/* Higher-level net wrappers */
int32_t pith_rt_net_connect(int32_t fd, PithValue *host, int32_t port);
int32_t pith_rt_net_send(int32_t fd, PithValue *data);
PithValue *pith_rt_net_recv(int32_t fd, int32_t max_len);

#ifdef __cplusplus
}
#endif

#endif /* PITH_API_H */
