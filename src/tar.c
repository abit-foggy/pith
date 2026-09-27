/*
 * tar.c - uncompressed ustar-compatible tar writer/reader.
 *
 * Used for the EOF debug-workspace overlay (pith build --embed-source,
 * pith decompile <binary>) and for unpacking package tarballs
 * (pith pkg sync).
 */
#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "../include/compiler.h"

#define TAR_BLOCK 512

/* ------------------------------------------------------------------ */
/* Writer                                                             */
/* ------------------------------------------------------------------ */

/* Format a 64-bit value as a zero-padded octal field. */
static void octal_field(char *dst, size_t width, unsigned long long v)
{
    snprintf(dst, width, "%0*llo", (int)(width - 1), v);
    dst[width - 1] = '\0';
}

static void put_block(FILE *out, const char *block)
{
    fwrite(block, 1, TAR_BLOCK, out);
}

int pith_tar_append_file(FILE *out, const char *path)
{
    return pith_tar_append_file_as(out, path, path);
}

/* Append one file entry stored under an explicit archive name. */
int pith_tar_append_file_as(FILE *out, const char *path,
                            const char *entry_name)
{
    FILE *in = fopen(path, "rb");
    if (!in)
        return -1;

    /* size */
    fseek(in, 0, SEEK_END);
    long sz = ftell(in);
    fseek(in, 0, SEEK_SET);
    if (sz < 0) {
        fclose(in);
        return -1;
    }

    /* ustar header */
    char hdr[TAR_BLOCK];
    memset(hdr, 0, sizeof(hdr));
    snprintf(hdr, 100, "%s", entry_name);           /* name          */

    /* record the file's actual permission bits */
    unsigned mode = 0644;
    {
        struct stat st;
        if (stat(path, &st) == 0)
            mode = (unsigned)(st.st_mode & 07777u);
    }
    octal_field(hdr + 100, 8, mode);                /* mode          */
    memcpy(hdr + 108, "0000000", 7);                /* uid           */
    memcpy(hdr + 116, "0000000", 7);                /* gid           */
    octal_field(hdr + 124, 12, (unsigned long long)sz);   /* size    */
    octal_field(hdr + 136, 12, 0);                  /* mtime         */
    memcpy(hdr + 148, "        ", 8);               /* chksum (blanks) */
    hdr[156] = '0';                                 /* typeflag: file */
    memcpy(hdr + 257, "ustar", 6);                  /* magic          */
    memcpy(hdr + 263, "00", 2);                     /* version        */
    memcpy(hdr + 265, "pith", 4);                   /* uname          */
    memcpy(hdr + 297, "pith", 4);                   /* gname          */

    /* checksum: header bytes with the chksum field counted as blanks */
    unsigned sum = 0;
    for (size_t i = 0; i < TAR_BLOCK; i++)
        sum += (unsigned char)hdr[i];
    octal_field(hdr + 148, 7, sum);
    hdr[155] = ' ';

    put_block(out, hdr);

    /* payload + padding */
    char buf[TAR_BLOCK];
    size_t total = 0;
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), in)) > 0) {
        fwrite(buf, 1, n, out);
        total += n;
    }
    fclose(in);

    size_t pad = (TAR_BLOCK - (total % TAR_BLOCK)) % TAR_BLOCK;
    if (pad) {
        memset(buf, 0, sizeof(buf));
        fwrite(buf, 1, pad, out);
    }
    return 0;
}

void pith_tar_finish(FILE *out)
{
    static const char zero[TAR_BLOCK] = { 0 };
    put_block(out, zero);
    put_block(out, zero);
}

/* ------------------------------------------------------------------ */
/* Reader                                                             */
/* ------------------------------------------------------------------ */

/* mkdir -p: create every component of `path`. */
static int mkdirs(const char *path)
{
    char tmp[4096];
    snprintf(tmp, sizeof(tmp), "%s", path);
    size_t len = strlen(tmp);

    for (size_t i = 1; i < len; i++) {
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

static unsigned long long parse_octal(const char *field, size_t width)
{
    unsigned long long v = 0;
    for (size_t i = 0; i < width; i++) {
        char c = field[i];
        if (c == '\0' || c == ' ')
            break;
        if (c < '0' || c > '7')
            break;
        v = v * 8 + (unsigned long long)(c - '0');
    }
    return v;
}

int pith_tar_extract_mem(const char *mem, size_t size, const char *dest_root)
{
    if (mkdirs(dest_root) != 0)
        return -1;

    size_t pos = 0;
    while (pos + TAR_BLOCK <= size) {
        const char *hdr = mem + pos;

        /* end of archive: zero block */
        int all_zero = 1;
        for (size_t i = 0; i < TAR_BLOCK; i++) {
            if (hdr[i]) {
                all_zero = 0;
                break;
            }
        }
        if (all_zero)
            break;

        /* sanity: reject corrupt headers (missing ustar magic) */
        if (memcmp(hdr + 257, "ustar", 5) != 0)
            return -1;

        char name[101], prefix[156];
        memcpy(name, hdr, 100);
        name[100] = '\0';
        memcpy(prefix, hdr + 345, 155);
        prefix[155] = '\0';

        /*
         * Path traversal hardening (tar slip): extraction is rigidly
         * sandboxed to dest_root. Reject absolute entry paths, empty
         * names, and any ".." component before it ever touches the
         * filesystem.
         */
        if (!name[0] || name[0] == '/')
            return -1;
        {
            char joined[4096];
            if (prefix[0]) {
                if (prefix[0] == '/')
                    return -1;
                snprintf(joined, sizeof(joined), "%s/%s", prefix, name);
            } else {
                snprintf(joined, sizeof(joined), "%s", name);
            }
            /* every path component must be non-empty and not ".." */
            char *comp = joined;
            while (*comp) {
                char *slash = strchr(comp, '/');
                size_t clen = slash ? (size_t)(slash - comp)
                                    : strlen(comp);
                if (clen == 0 ||
                    (clen == 2 && strncmp(comp, "..", 2) == 0))
                    return -1;
                comp = slash ? slash + 1 : comp + clen;
            }
        }

        char full[4096];
        if (prefix[0])
            snprintf(full, sizeof(full), "%s/%s/%s", dest_root, prefix,
                     name);
        else
            snprintf(full, sizeof(full), "%s/%s", dest_root, name);

        unsigned long long fsize = parse_octal(hdr + 124, 12);

        /* sanity: the declared size must fit within the buffer */
        if (fsize > size - pos - TAR_BLOCK)
            return -1;

        char typeflag = hdr[156];

        if (typeflag == '5') {
            if (mkdirs(full) != 0)
                return -1;
        } else {
            /* regular file ('0' or '\0') */
            char *slash = strrchr(full, '/');
            if (slash) {
                char parent[4096];
                snprintf(parent, sizeof(parent), "%.*s",
                         (int)(slash - full), full);
                if (mkdirs(parent) != 0)
                    return -1;
            }
            FILE *fp = fopen(full, "wb");
            if (!fp)
                return -1;
            fwrite(mem + pos + TAR_BLOCK, 1, (size_t)fsize, fp);
            fclose(fp);

            /* restore the recorded permission bits */
            {
                unsigned long long mode = parse_octal(hdr + 100, 8);
                if (mode)
                    chmod(full, (mode_t)mode);
            }
        }

        pos += TAR_BLOCK + (size_t)((fsize + TAR_BLOCK - 1) /
                                    TAR_BLOCK) * TAR_BLOCK;
    }
    return 0;
}
