/*
 * strings.c - the str.* builtin namespace for The Pith Programming
 * Language.
 *
 * Minimal, deterministic string helpers in the spirit of the other
 * builtin namespaces (fs, os, proc, net):
 *
 *   str.length(s)          payload byte length
 *   str.contains(s, sub)   1 when sub appears anywhere in s
 *   str.startsWith(s, p)   1 when s begins with p
 *   str.endsWith(s, p)     1 when s ends with p
 *   str.upper(s)           new string, ASCII A-Z folded up (+1 ref)
 *   str.lower(s)           new string, ASCII a-z folded down (+1 ref)
 *
 * Semantics are byte-oriented (lengths are payload bytes, not
 * codepoints) and case mapping is ASCII-only; every other byte
 * passes through unchanged. All parameters are borrowed; string
 * returns transfer a new reference the compiler releases.
 */
#define _POSIX_C_SOURCE 200809L

#include <stdint.h>
#include <string.h>

#include "../include/api.h"

/* Does the byte range [n, n + nl) appear inside [h, h + hl)? */
static int32_t bytes_contains(const char *h, uint32_t hl,
                               const char *n, uint32_t nl)
{
    if (nl == 0)
        return 1;
    if (nl > hl)
        return 0;
    for (uint32_t i = 0; i + nl <= hl; i++) {
        if (memcmp(h + i, n, nl) == 0)
            return 1;
    }
    return 0;
}

int32_t pith_str_length(PithValue *s)
{
    if (!s)
        return 0;
    return (int32_t)s->length;
}

int32_t pith_str_contains(PithValue *s, PithValue *sub)
{
    if (!s || !sub)
        return 0;
    return bytes_contains(s->data, s->length, sub->data, sub->length);
}

int32_t pith_str_starts_with(PithValue *s, PithValue *prefix)
{
    if (!s || !prefix)
        return 0;
    if (prefix->length > s->length)
        return 0;
    return memcmp(s->data, prefix->data, prefix->length) == 0;
}

int32_t pith_str_ends_with(PithValue *s, PithValue *suffix)
{
    if (!s || !suffix)
        return 0;
    if (suffix->length > s->length)
        return 0;
    return memcmp(s->data + (s->length - suffix->length),
                  suffix->data, suffix->length) == 0;
}

/* Shared ASCII case mapper: 0 folds down, 1 folds up. */
static PithValue *ascii_fold(PithValue *s, int up)
{
    if (!s)
        return pith_str_new("", 0);
    PithValue *out = pith_str_new(NULL, s->length);
    if (!out)
        return pith_str_new("", 0);
    for (uint32_t i = 0; i < s->length; i++) {
        char c = s->data[i];
        if (up && c >= 'a' && c <= 'z')
            c = (char)(c - 'a' + 'A');
        else if (!up && c >= 'A' && c <= 'Z')
            c = (char)(c - 'A' + 'a');
        out->data[i] = c;
    }
    return out;
}

PithValue *pith_str_upper(PithValue *s)
{
    return ascii_fold(s, 1);
}

PithValue *pith_str_lower(PithValue *s)
{
    return ascii_fold(s, 0);
}
