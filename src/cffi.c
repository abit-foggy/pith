/*
 * cffi.c — native C import pipeline for The Pith Programming Language.
 *
 * `import "*.c"` compiles a C translation unit into the running
 * program (JIT) or into the standalone artifact (AOT). This module
 * provides:
 *
 *   - the pre-baked <pith.h> virtual header injected into every
 *     imported C compilation context (kept in sync with
 *     include/pith.h)
 *   - a lightweight prototype scanner that discovers a unit's
 *     exported (non-static) function definitions and maps their C
 *     types onto the FFI ABI classes (w / l / s / d), per System V
 *     AMD64 / AAPCS64 conventions
 */
#define _POSIX_C_SOURCE 200809L

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../include/compiler.h"

/* ------------------------------------------------------------------ */
/* Pre-baked <pith.h> (virtual header)                                */
/* ------------------------------------------------------------------ */

const char *pith_cffi_header_text(void)
{
    return
"/* pith.h — minimal C runtime header for imported Pith C modules.\n"
"   NOTE: pre-baked copy; keep in sync with include/pith.h. */\n"
"#ifndef PITH_H\n"
"#define PITH_H\n"
"#include <stddef.h>\n"
"#include <stdint.h>\n"
"#ifdef __cplusplus\n"
"extern \"C\" {\n"
"#endif\n"
"#define PITH_FLAG_STATIC 0x0001u\n"
"#define PITH_FLAG_SHARED 0x0002u\n"
"enum { PITH_TAG_INT = 1, PITH_TAG_FLOAT, PITH_TAG_STRING,\n"
"       PITH_TAG_BOOL, PITH_TAG_OBJECT };\n"
"typedef struct PithValue PithValue;\n"
"struct PithValue {\n"
"    volatile uint32_t strongRefs; /* inline 32-bit reference count */\n"
"    uint16_t typeTag;             /* type discriminator            */\n"
"    uint16_t flags;               /* PITH_FLAG_* bits              */\n"
"    uint32_t capacity;            /* payload bytes allocated       */\n"
"    uint32_t length;              /* payload bytes in use          */\n"
"    char data[];                 /* payload (strings)             */\n"
"};\n"
"void pithRetain(PithValue *val);\n"
"void pithRelease(PithValue *val);\n"
"PithValue *pithNewString(const char *cstr);\n"
"PithValue *pithNewStringN(const char *bytes, uint32_t len);\n"
"const char *pithStringData(PithValue *val);\n"
"uint32_t pithStringLength(PithValue *val);\n"
"int pithStringEquals(PithValue *a, PithValue *b);\n"
"PithValue *pithStringConcat(PithValue *a, PithValue *b);\n"
"#ifdef __cplusplus\n"
"}\n"
"#endif\n"
"#endif /* PITH_H */\n";
}

/* ------------------------------------------------------------------ */
/* Type mapping                                                       */
/* ------------------------------------------------------------------ */

PithFfiType pith_cffi_map_type(const char *type_text,
                               bool *is_pith_value)
{
    if (is_pith_value)
        *is_pith_value = false;

    char buf[256];
    snprintf(buf, sizeof(buf), "%s", type_text);

    int has_star = 0;
    int saw[32];      /* keyword hits */
    int nsaw = 0;
    size_t i = 0;

#define SAW(k) do { if (nsaw < 32) saw[nsaw++] = (k); } while (0)
    enum { KW_VOID, KW_FLOAT, KW_DOUBLE, KW_LONG, KW_PITH,
           KW_PTRWORD, KW_OTHER };

    while (buf[i]) {
        while (buf[i] == ' ' || buf[i] == '\t' || buf[i] == '\n' ||
               buf[i] == '\r')
            i++;
        if (buf[i] == '*') {
            has_star = 1;
            i++;
            continue;
        }
        if (!isalpha((unsigned char)buf[i]) && buf[i] != '_')
            break;

        /* read one word */
        char word[32];
        size_t w = 0;
        while ((isalnum((unsigned char)buf[i]) || buf[i] == '_') &&
               w + 1 < sizeof(word))
            word[w++] = buf[i++];
        word[w] = '\0';

        if (strcmp(word, "void") == 0)
            SAW(KW_VOID);
        else if (strcmp(word, "float") == 0)
            SAW(KW_FLOAT);
        else if (strcmp(word, "double") == 0)
            SAW(KW_DOUBLE);
        else if (strcmp(word, "long") == 0 || strcmp(word, "int64_t") == 0 ||
                 strcmp(word, "uint64_t") == 0 ||
                 strcmp(word, "size_t") == 0 || strcmp(word, "ssize_t") == 0 ||
                 strcmp(word, "ptrdiff_t") == 0 ||
                 strcmp(word, "intptr_t") == 0 ||
                 strcmp(word, "uintptr_t") == 0)
            SAW(KW_LONG);
        else if (strcmp(word, "PithValue") == 0 ||
                 strcmp(word, "PithString") == 0)
            SAW(KW_PITH);
        else if (strcmp(word, "char") == 0 || strcmp(word, "short") == 0 ||
                 strcmp(word, "int") == 0 || strcmp(word, "bool") == 0 ||
                 strcmp(word, "_Bool") == 0 ||
                 strncmp(word, "int8_t", 6) == 0 ||
                 strncmp(word, "uint8_t", 7) == 0 ||
                 strncmp(word, "int16_t", 7) == 0 ||
                 strncmp(word, "uint16_t", 8) == 0 ||
                 strncmp(word, "int32_t", 7) == 0 ||
                 strncmp(word, "uint32_t", 8) == 0)
            SAW(KW_PTRWORD);
        else
            SAW(KW_OTHER);
    }
#undef SAW

    int saw_void = 0, saw_float = 0, saw_double = 0, saw_long = 0,
        saw_pith = 0, saw_ptrword = 0;
    for (int k = 0; k < nsaw; k++) {
        switch (saw[k]) {
        case KW_VOID:     saw_void = 1; break;
        case KW_FLOAT:    saw_float = 1; break;
        case KW_DOUBLE:   saw_double = 1; break;
        case KW_LONG:     saw_long = 1; break;
        case KW_PITH:     saw_pith = 1; break;
        case KW_PTRWORD:  saw_ptrword = 1; break;
        default: break;
        }
    }

    if (saw_pith) {
        if (is_pith_value)
            *is_pith_value = true;
        return PITH_FFI_LONG;   /* Pith values cross as pointers */
    }
    if (has_star)
        return PITH_FFI_LONG;   /* any pointer */
    if (saw_float)
        return PITH_FFI_SINGLE;
    if (saw_double)
        return PITH_FFI_DOUBLE;
    if (saw_long)
        return PITH_FFI_LONG;
    if (saw_void)
        return PITH_FFI_VOID;
    if (saw_ptrword)
        return PITH_FFI_WORD;
    return PITH_FFI_WORD;      /* unknown/enum/typedef'd int-ish */
}

/* ------------------------------------------------------------------ */
/* Prototype scanner                                                  */
/* ------------------------------------------------------------------ */

/* Blank comments, string/char literals, and preprocessor lines so the
   structural walk only sees code. */
static char *blank_noncode(const char *src, size_t len)
{
    char *out = malloc(len + 1);
    if (!out)
        return NULL;

    int in_block = 0, in_line = 0, in_str = 0, in_chr = 0;
    int line_start = 1;

    for (size_t i = 0; i < len; i++) {
        char c = src[i];
        char n = (i + 1 < len) ? src[i + 1] : '\0';

        if (in_block) {
            if (c == '*' && n == '/') {
                out[i] = ' ';
                out[++i] = ' ';
                in_block = 0;
            } else {
                out[i] = (c == '\n') ? '\n' : ' ';
            }
            continue;
        }
        if (in_line) {
            out[i] = (c == '\n') ? '\n' : ' ';
            if (c == '\n')
                in_line = 0;
            continue;
        }
        if (in_str || in_chr) {
            if (c == '\\') {
                out[i] = ' ';
                if (i + 1 < len)
                    out[++i] = ' ';
                continue;
            }
            out[i] = (c == '\n') ? '\n' : ' ';
            if ((in_str && c == '"') || (in_chr && c == '\''))
                in_str = in_chr = 0;
            continue;
        }
        if (c == '/' && n == '/') {
            out[i] = ' ';
            out[++i] = ' ';
            in_line = 1;
            continue;
        }
        if (c == '/' && n == '*') {
            out[i] = ' ';
            out[++i] = ' ';
            in_block = 1;
            continue;
        }
        if (c == '"') {
            in_str = 1;
            out[i] = ' ';
            continue;
        }
        if (c == '\'') {
            in_chr = 1;
            out[i] = ' ';
            continue;
        }
        if (c == '#' && line_start) {
            /* blank the whole preprocessor line */
            while (i < len) {
                out[i] = (src[i] == '\n') ? '\n' : ' ';
                if (src[i] == '\n')
                    break;
                i++;
            }
            line_start = 1;
            continue;
        }
        out[i] = c;
        line_start = (c == '\n');
    }
    out[len] = '\0';
    return out;
}

/* Trim helper: returns the first non-space pointer, NUL-terminates the
   trailing whitespace. */
static char *trim(char *s)
{
    while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r')
        s++;
    size_t len = strlen(s);
    while (len && (s[len - 1] == ' ' || s[len - 1] == '\t' ||
                   s[len - 1] == '\n' || s[len - 1] == '\r'))
        s[--len] = '\0';
    return s;
}

/* Try to parse one declaration head as an exported function.
   `head` spans [begin, end) of the blanked source. Returns 1 and
   fills `fn` on success. */
static int parse_head(const char *head, size_t len, PithForeignFn *fn)
{
    /* trim bounds */
    while (len && (head[0] == ' ' || head[0] == '\n'))
        head++, len--;
    while (len && (head[len - 1] == ' ' || head[len - 1] == '\n'))
        len--;
    if (len < 4 || head[len - 1] != ')')
        return 0;

    /* find the '(' matching the trailing ')' */
    int depth = 0;
    long open = -1;
    for (long i = (long)len - 1; i >= 0; i--) {
        if (head[i] == ')')
            depth++;
        else if (head[i] == '(') {
            depth--;
            if (depth == 0) {
                open = i;
                break;
            }
        }
    }
    if (open <= 0)
        return 0;

    /* the function name: the last identifier before the '(' */
    long name_end = open;
    while (name_end > 0 && (head[name_end - 1] == ' ' ||
                            head[name_end - 1] == '\t' ||
                            head[name_end - 1] == '\n'))
        name_end--;
    long name_start = name_end;
    while (name_start > 0 &&
           (isalnum((unsigned char)head[name_start - 1]) ||
            head[name_start - 1] == '_'))
        name_start--;
    if (name_start == name_end || name_start < 0)
        return 0;

    /* the return type: everything before the name */
    char retbuf[256];
    size_t rlen = (size_t)name_start;
    if (rlen >= sizeof(retbuf))
        rlen = sizeof(retbuf) - 1;
    memcpy(retbuf, head, rlen);
    retbuf[rlen] = '\0';
    char *ret_text = trim(retbuf);

    /* reject static (file-local) definitions and macro-ish heads */
    if (strncmp(ret_text, "static", 6) == 0 &&
        (ret_text[6] == '\0' || ret_text[6] == ' '))
        return 0;
    if (strchr(ret_text, '=') || strchr(ret_text, ';'))
        return 0;
    if (!ret_text[0])
        return 0;

    memset(fn, 0, sizeof(*fn));
    size_t nlen = (size_t)(name_end - name_start);
    if (nlen >= sizeof(fn->name))
        nlen = sizeof(fn->name) - 1;
    memcpy(fn->name, head + name_start, nlen);
    fn->name[nlen] = '\0';

    bool is_pv = false;
    fn->ret = pith_cffi_map_type(ret_text, &is_pv);
    fn->ret_pith_value = is_pv && fn->ret == PITH_FFI_LONG;

    /* parameters: text strictly inside the parens */
    char pbuf[512];
    size_t plen = (size_t)(len - open - 2);
    if (plen >= sizeof(pbuf))
        plen = sizeof(pbuf) - 1;
    memcpy(pbuf, head + open + 1, plen);
    pbuf[plen] = '\0';
    char *params = trim(pbuf);

    if (!params[0] || strcmp(params, "void") == 0)
        return 1;   /* no parameters */

    /* split on top-level commas */
    int pdepth = 0;
    char *start = params;
    for (char *c = params; ; c++) {
        if (*c == '(')
            pdepth++;
        else if (*c == ')')
            pdepth--;
        else if ((*c == ',' && pdepth == 0) || *c == '\0') {
            int done = (*c == '\0');
            *c = '\0';
            char *one = trim(start);
            if (one[0] && fn->nparams < PITH_FFI_MAX_PARAMS) {
                /* strip a trailing parameter NAME (an identifier after
                   the type) so mapping sees the pure type spelling; a
                   param ending in '*' is a pointer type and is kept */
                char mapbuf[256];
                snprintf(mapbuf, sizeof(mapbuf), "%s", one);
                size_t mlen = strlen(mapbuf);

                if (mapbuf[mlen - 1] != '*') {
                    /* find the start of the last identifier token */
                    size_t j = mlen;
                    while (j && (isalnum((unsigned char)mapbuf[j - 1]) ||
                                 mapbuf[j - 1] == '_'))
                        j--;
                    /* identifier must be preceded by type text */
                    if (j > 0 && j < mlen) {
                        size_t k = j;
                        while (k && mapbuf[k - 1] == ' ')
                            k--;
                        if (k > 0) {
                            mapbuf[j] = '\0';
                            mlen = strlen(trim(mapbuf));
                        }
                    }
                }
                fn->params[fn->nparams++] = pith_cffi_map_type(mapbuf,
                                                               NULL);
            }
            if (done)
                break;
            start = c + 1;
        }
    }

    return 1;
}

int pith_cffi_scan_file(const char *path, PithImportUnit *out)
{
    FILE *fp = fopen(path, "rb");
    if (!fp)
        return -1;
    fseek(fp, 0, SEEK_END);
    long sz = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    if (sz < 0) {
        fclose(fp);
        return -1;
    }
    char *src = malloc((size_t)sz + 1);
    if (!src) {
        fclose(fp);
        return -1;
    }
    int ok = (fread(src, 1, (size_t)sz, fp) == (size_t)sz);
    fclose(fp);
    if (!ok) {
        free(src);
        return -1;
    }
    src[sz] = '\0';

    char *code = blank_noncode(src, (size_t)sz);
    free(src);
    if (!code)
        return -1;

    memset(out, 0, sizeof(*out));
    snprintf(out->path, sizeof(out->path), "%s", path);

    /* namespace: basename sans extension */
    const char *base = strrchr(path, '/');
    base = base ? base + 1 : path;
    size_t nlen = strlen(base);
    if (nlen > 3 && strcmp(base + nlen - 2, ".c") == 0)
        nlen -= 2;
    if (nlen >= sizeof(out->ns))
        nlen = sizeof(out->ns) - 1;
    memcpy(out->ns, base, nlen);
    out->ns[nlen] = '\0';

    /* structural walk at brace depth 0: a "head" is the text since
       the last top-level ';' or '}' up to a top-level '{' */
    size_t len = (size_t)sz;
    size_t head_start = 0;
    int depth = 0;

    for (size_t i = 0; i < len; i++) {
        char c = code[i];
        if (c == '{') {
            if (depth == 0) {
                if (i > head_start) {
                    PithForeignFn fn;
                    if (parse_head(code + head_start,
                                   i - head_start, &fn) &&
                        out->nfn < PITH_FFI_MAX_FNS)
                        out->fns[out->nfn++] = fn;
                }
            }
            depth++;
        } else if (c == '}') {
            if (depth > 0)
                depth--;
            if (depth == 0)
                head_start = i + 1;
        } else if (c == ';' && depth == 0) {
            head_start = i + 1;
        }
    }

    free(code);
    return 0;
}
