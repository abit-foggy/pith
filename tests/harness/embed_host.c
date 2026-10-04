/*
 * [EMBED] tests/harness/embed_host.c
 *
 * The embeddable C ABI end to end: a host registers typed namespace
 * functions, evaluates pith source that calls them, and asserts the
 * exit code the script computed from their results. The script's
 * checks are self-contained, so the harness is transparently
 * exercised on BOTH execution backends:
 *
 *   - in-memory tcc: the registered function addresses are bound
 *     directly into the execution state
 *   - temp-executable fallback (hardened kernels, Darwin): the
 *     registered link object supplies the c_host_* symbols to the
 *     child process
 *
 * Expected behavior: exit 0 - the script observes correct values
 * from every ABI class and exits 42; anything else fails.
 */
#define _POSIX_C_SOURCE 200809L

#include <stdio.h>

#include "../../include/pith.h"
#include "../../include/pith_embed.h"

/* defined in embed_host_impl.c (compiled with the c_host_* renames) */
extern int seven(void);
extern int twice(int x);
extern long wide(long x);
extern void note(PithValue *s);
extern PithValue *tag(PithValue *s);

static const char *SCRIPT =
    "mut ok = 0\n"
    "if host.seven == 7\n"
    "    mut ok = ok + 1\n"
    "end\n"
    "if host.twice(21) == 42\n"
    "    mut ok = ok + 1\n"
    "end\n"
    "if host.wide(1000000000) == 2000000000\n"
    "    mut ok = ok + 1\n"
    "end\n"
    "if host.tag(\"owned\") == \"[owned]\"\n"
    "    mut ok = ok + 1\n"
    "end\n"
    "print host.seven\n"
    "print host.twice(21)\n"
    "print host.wide(1000000000)\n"
    "print host.tag(\"owned\")\n"
    "host.note(\"registered functions work\")\n"
    "if ok == 4\n"
    "    proc.exit(42)\n"
    "end\n"
    "proc.exit(1)\n";

int main(int argc, char **argv)
{
    PithContext *ctx = pith_context_new();
    if (!ctx) {
        fputs("embed_host: context allocation failed\n", stderr);
        return 1;
    }

    /* the registration surface: a pseudo-constant, 32/64-bit int
       calls, a void statement call, and an owned string return */
    if (pith_register_ns_fn(ctx, "host", "seven", seven, 'w', "") != 0 ||
        pith_register_ns_fn(ctx, "host", "twice", twice, 'w', "w") != 0 ||
        pith_register_ns_fn(ctx, "host", "wide", wide, 'l', "l") != 0 ||
        pith_register_ns_fn(ctx, "host", "note", note, 'v', "p") != 0 ||
        pith_register_ns_fn(ctx, "host", "tag", tag, 'p', "p") != 0) {
        fputs("embed_host: registration failed\n", stderr);
        pith_context_free(ctx);
        return 1;
    }

    /* the fallback path links this object into the temporary
       executable; the in-memory path uses the addresses above */
    const char *obj = (argc > 1) ? argv[1]
                                 : "tests/harness/embed_host_impl.o";
    if (pith_register_link_object(ctx, obj) != 0) {
        fputs("embed_host: link-object registration failed\n", stderr);
        pith_context_free(ctx);
        return 1;
    }

    int rc = pith_eval_string(ctx, SCRIPT);
    pith_context_free(ctx);

    if (rc == 42) {
        printf("embed_host: PASS (script exit 42)\n");
        return 0;
    }
    fprintf(stderr, "embed_host: FAIL (script exit %d, expected 42)\n",
            rc);
    return 1;
}
