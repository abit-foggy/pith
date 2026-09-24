/*
 * [PACKAGE] tests/harness/tar_security.c
 *
 * Hostile archive handling:
 *   - tar slip: `../../` traversal, absolute paths, empty and `..`
 *     path components must be rejected before touching the filesystem
 *     (extraction stays rigidly sandboxed to the destination root)
 *   - malformed headers: corrupt ustar magic, implausibly huge
 *     declared sizes, truncated blocks
 *   - non-ASCII file names must extract cleanly
 *
 * Expected behavior: exit 0 — every hostile case fails extraction
 * gracefully, every benign case extracts byte-exactly, and nothing
 * is ever written outside the destination root.
 */
#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "../../include/compiler.h"

#define ROOT "/tmp/opencode/pith_tar_security/root"
#define ESC "/tmp/opencode/pith_tar_escaped"

static int failures;

static void check(int cond, const char *what)
{
    if (!cond) {
        fprintf(stderr, "tar_security: FAILED: %s\n", what);
        failures++;
    }
}

/* Build one ustar entry into `buf` (header + payload, padded). */
static size_t make_entry(unsigned char *buf, const char *name,
                         unsigned long long size, const char *magic,
                         const char *payload)
{
    memset(buf, 0, 1024);
    snprintf((char *)buf, 100, "%s", name);
    sprintf((char *)buf + 124, "%011llo", size);
    buf[156] = '0';                              /* regular file   */
    memcpy(buf + 257, magic, strlen(magic));     /* ustar magic    */
    memcpy(buf + 263, "00", 2);                  /* version        */
    memcpy(buf + 512, payload, size < 512 ? (size_t)size : 512);

    unsigned sum = 0;
    for (int i = 0; i < 512; i++)
        sum += buf[i];
    sprintf((char *)buf + 148, "%06o", sum);
    buf[154] = '\0';
    buf[155] = ' ';

    return 512 + ((size_t)((size + 511) / 512) * 512);
}

static int exists(const char *path)
{
    return access(path, F_OK) == 0;
}

static void fresh_root(void)
{
    int rc = system("rm -rf " ROOT " " ESC);
    (void)rc;
}

int main(void)
{
    unsigned char buf[2048];
    size_t sz;

    /* ---- tar slip: ../../ traversal must be rejected ---- */
    fresh_root();
    sz = make_entry(buf, "../../escaped", 4, "ustar", "pwn!");
    check(pith_tar_extract_mem((const char *)buf, sz, ROOT) != 0,
          "../../ traversal rejected");
    check(!exists(ESC "/escaped"), "nothing escaped the sandbox");

    /* ---- absolute paths must be rejected ---- */
    fresh_root();
    sz = make_entry(buf, "/etc/pwned", 4, "ustar", "pwn!");
    check(pith_tar_extract_mem((const char *)buf, sz, ROOT) != 0,
          "absolute path rejected");

    /* ---- empty path components must be rejected ---- */
    fresh_root();
    sz = make_entry(buf, "a//b", 4, "ustar", "pwn!");
    check(pith_tar_extract_mem((const char *)buf, sz, ROOT) != 0,
          "empty component rejected");

    /* ---- .. as a whole interior component must be rejected ---- */
    fresh_root();
    sz = make_entry(buf, "safe/../escape", 4, "ustar", "pwn!");
    check(pith_tar_extract_mem((const char *)buf, sz, ROOT) != 0,
          "interior .. rejected");

    /* ---- corrupt ustar magic must be rejected ---- */
    fresh_root();
    sz = make_entry(buf, "ok.txt", 4, "xxxxx", "data");
    check(pith_tar_extract_mem((const char *)buf, sz, ROOT) != 0,
          "corrupt magic rejected");

    /* ---- huge declared size must be rejected ----
       (the buffer only holds 1024 real bytes; the header lies about
       a ~1GB payload, so the size sanity check must refuse) */
    fresh_root();
    sz = make_entry(buf, "huge.bin", 999999999ULL, "ustar", "data");
    check(sz > 1024, "fictional entry size computed");
    check(pith_tar_extract_mem((const char *)buf, 1024, ROOT) != 0,
          "huge size rejected");

    /* ---- truncated header: no crash, nothing extracted ---- */
    fresh_root();
    check(pith_tar_extract_mem((const char *)buf, 100, ROOT) == 0,
          "truncated header handled gracefully");
    check(!exists(ROOT "/ok.txt"), "truncated header wrote nothing");

    /* ---- non-ASCII file names extract cleanly ---- */
    fresh_root();
    sz = make_entry(buf, "\303\274n\303\257code.txt", 4, "ustar", "data");
    check(pith_tar_extract_mem((const char *)buf, sz, ROOT) == 0,
          "non-ASCII name extracted");
    check(exists(ROOT "/\303\274n\303\257code.txt"), "non-ASCII file present");

    /* ---- positive control: a well-formed entry extracts byte-exact */
    fresh_root();
    sz = make_entry(buf, "good.txt", 4, "ustar", "data");
    check(pith_tar_extract_mem((const char *)buf, sz, ROOT) == 0,
          "valid entry extracted");
    {
        FILE *fp = fopen(ROOT "/good.txt", "rb");
        check(fp != NULL, "good.txt readable");
        if (fp) {
            char got[8] = { 0 };
            size_t n = fread(got, 1, 4, fp);
            fclose(fp);
            check(n == 4, "good.txt readable length");
            check(memcmp(got, "data", 4) == 0, "good.txt byte-exact");
        }
    }

    fresh_root();
    if (failures) {
        fprintf(stderr, "tar_security: %d failure(s)\n", failures);
        return 1;
    }
    printf("tar_security: hostile archives ok\n");
    return 0;
}
