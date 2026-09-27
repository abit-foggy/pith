/*
 * pith.h - the minimal C runtime header exposed to imported C modules
 * (`import "*.c"`). This is the canonical definition of the Pith value
 * object; the compiler runtime (include/api.h) builds on it, and the
 * engine injects a pre-baked copy of this header into every imported
 * C compilation context (src/cffi.c carries the embedded text - keep
 * the two in sync).
 *
 * ABI contract for imported C code:
 *   - PithValues passed as parameters are BORROWED: the callee must
 *     call pithRetain() before storing them beyond the call, and owns
 *     that extra reference afterwards.
 *   - A C function returning PithValue* must return a NEW reference
 *     (+1); the Pith compiler injects the matching release.
 *   - Raw C types map onto the C ABI directly (System V AMD64 on
 *     x86_64, AAPCS64 on arm64): 32-bit ints/enums are `int`, 64-bit
 *     ints/pointers are `long`, single/double floats are
 *     `float`/`double`.
 */
#ifndef PITH_H
#define PITH_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ */
/* Value object                                                       */
/* ------------------------------------------------------------------ */

/* Flag bits. */
#define PITH_FLAG_STATIC 0x0001u   /* immortal static data (literals)  */
#define PITH_FLAG_SHARED 0x0002u   /* involved in a (broken) cycle     */

/* Type discriminators. */
enum {
    PITH_TAG_INT = 1,
    PITH_TAG_FLOAT,
    PITH_TAG_STRING,
    PITH_TAG_BOOL,
    PITH_TAG_OBJECT
};

/*
 * Every Pith value carries an inline 32-bit reference count and a
 * type discriminator. Reference counts are updated atomically.
 * The payload (for strings) follows the header at offset 16.
 */
typedef struct PithValue PithValue;
struct PithValue {
    volatile uint32_t strongRefs;   /* inline 32-bit reference count */
    uint16_t typeTag;               /* type discriminator            */
    uint16_t flags;                 /* PITH_FLAG_* bits              */
    uint32_t capacity;              /* payload bytes allocated       */
    uint32_t length;                /* payload bytes in use          */
    char data[];                    /* payload (strings)             */
};

/* ------------------------------------------------------------------ */
/* C-callable ARC primitives                                          */
/* ------------------------------------------------------------------ */

/* Bump the reference count (no-op on immortal static data). */
void pithRetain(PithValue *val);

/* Drop a reference; frees the value when the count reaches zero. */
void pithRelease(PithValue *val);

/* ------------------------------------------------------------------ */
/* String helpers                                                     */
/* ------------------------------------------------------------------ */

/* Allocate a string copying a NUL-terminated C string (+1 reference). */
PithValue *pithNewString(const char *cstr);

/* Allocate a string of `len` bytes copied from `bytes` (+1 reference). */
PithValue *pithNewStringN(const char *bytes, uint32_t len);

/* Borrowed payload pointer of a string (do not free; NUL-terminated). */
const char *pithStringData(PithValue *val);

/* Payload byte length. */
uint32_t pithStringLength(PithValue *val);

/* Content equality; 1 if equal. */
int pithStringEquals(PithValue *a, PithValue *b);

/* New string holding a's bytes followed by b's (+1 reference). */
PithValue *pithStringConcat(PithValue *a, PithValue *b);

#ifdef __cplusplus
}
#endif

#endif /* PITH_H */
