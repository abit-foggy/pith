/*
 * memory.c - deterministic ARC engine for The Pith Programming Language
 *
 * Every value carries an inline 32-bit reference count and a type
 * discriminator (see pith.h). The compiler injects deterministic
 * retain/release calls at scope boundaries (end, reassignments); there
 * is no tracing garbage collector. Counts are updated atomically
 * (C11 <stdatomic.h> when available, GCC/Clang __sync builtins
 * otherwise), making shared values thread-safe.
 */
#include <stdlib.h>
#include <string.h>

#include "../include/api.h"

#if defined(_WIN32) || defined(_WIN64) || defined(__NT__)
#include <windows.h>
#define PITH_HAVE_WIN32_ATOMICS 1
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L && \
    !defined(__STDC_NO_ATOMICS__)
#include <stdatomic.h>
#define PITH_HAVE_ATOMICS 1
#endif

/* Current count. */
static uint32_t refs_get(PithValue *v)
{
#if defined(PITH_HAVE_WIN32_ATOMICS)
    return (uint32_t)InterlockedCompareExchange((LONG volatile *)&v->strongRefs, 0, 0);
#elif defined(PITH_HAVE_ATOMICS)
    return atomic_load_explicit(&v->strongRefs, memory_order_acquire);
#else
    return __sync_fetch_and_add(&v->strongRefs, 0);
#endif
}

/* Increment; returns the new count. */
static uint32_t refs_inc(PithValue *v)
{
#if defined(PITH_HAVE_WIN32_ATOMICS)
    return (uint32_t)InterlockedIncrement((LONG volatile *)&v->strongRefs);
#elif defined(PITH_HAVE_ATOMICS)
    return atomic_fetch_add_explicit(&v->strongRefs, 1u,
                                     memory_order_relaxed) + 1u;
#else
    return __sync_add_and_fetch(&v->strongRefs, 1u);
#endif
}

/* Decrement; returns the PREVIOUS count. */
static uint32_t refs_dec(PithValue *v)
{
#if defined(PITH_HAVE_WIN32_ATOMICS)
    return (uint32_t)InterlockedDecrement((LONG volatile *)&v->strongRefs) + 1u;
#elif defined(PITH_HAVE_ATOMICS)
    return atomic_fetch_sub_explicit(&v->strongRefs, 1u,
                                      memory_order_acq_rel);
#else
    return __sync_fetch_and_sub(&v->strongRefs, 1u);
#endif
}

PithValue *pith_str_new(const char *initial, size_t len)
{
    if (len > 0xFFFFFFFFu - 1)
        return NULL;
    PithValue *v = malloc(sizeof(PithValue) + len + 1);
    if (!v)
        return NULL;
    v->strongRefs = 1;
    v->typeTag = PITH_TAG_STRING;
    v->flags = 0;
    v->capacity = (uint32_t)len + 1;
    v->length = (uint32_t)len;
    if (initial && len)
        memcpy(v->data, initial, len);
    else
        memset(v->data, 0, len + 1);
    v->data[len] = '\0';
    return v;
}

void pith_retain(void *ptr)
{
    PithValue *v = (PithValue *)ptr;
    if (!v)
        return;
    if (v->flags & PITH_FLAG_STATIC)
        return;   /* immortal static data */
    refs_inc(v);
}

void pith_release(void *ptr)
{
    PithValue *v = (PithValue *)ptr;
    if (!v)
        return;
    if (v->flags & PITH_FLAG_STATIC)
        return;   /* immortal static data */
    if (refs_get(v) == 0)
        return;   /* defensive: already freed */
    if (refs_dec(v) == 1)
        free(v);   /* deallocator invoked immediately at zero */
}

PithValue *pith_str_concat(PithValue *a, PithValue *b)
{
    uint32_t la = a ? a->length : 0;
    uint32_t lb = b ? b->length : 0;

    PithValue *out = pith_str_new(NULL, (size_t)la + (size_t)lb);
    if (!out)
        return NULL;

    if (a && la)
        memcpy(out->data, a->data, la);
    if (b && lb)
        memcpy(out->data + la, b->data, lb);
    out->data[la + lb] = '\0';
    return out;
}

int32_t pith_str_equals(PithValue *a, PithValue *b)
{
    if (!a || !b)
        return a == b ? 1 : 0;
    if (a->length != b->length)
        return 0;
    return memcmp(a->data, b->data, a->length) == 0 ? 1 : 0;
}

void pith_break_cycle(void *parent, void *child)
{
    PithValue *c = (PithValue *)child;
    if (!c)
        return;
    if (!(c->flags & PITH_FLAG_STATIC))
        c->flags |= PITH_FLAG_SHARED;
    /* drop the parent's strong reference to the child; the caller has
       already zeroed the back-reference slot in the parent's payload */
    pith_release(c);
}

/* ------------------------------------------------------------------ */
/* camelCase FFI aliases (imported C modules use these names)         */
/* ------------------------------------------------------------------ */

void pithRetain(PithValue *val)
{
    pith_retain(val);
}

void pithRelease(PithValue *val)
{
    pith_release(val);
}

PithValue *pithNewString(const char *cstr)
{
    return pith_str_new(cstr, cstr ? strlen(cstr) : 0);
}

PithValue *pithNewStringN(const char *bytes, uint32_t len)
{
    return pith_str_new(bytes, len);
}

const char *pithStringData(PithValue *val)
{
    return val ? val->data : NULL;
}

uint32_t pithStringLength(PithValue *val)
{
    return val ? val->length : 0;
}

int pithStringEquals(PithValue *a, PithValue *b)
{
    return pith_str_equals(a, b);
}

PithValue *pithStringConcat(PithValue *a, PithValue *b)
{
    return pith_str_concat(a, b);
}
