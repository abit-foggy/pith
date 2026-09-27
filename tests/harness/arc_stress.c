/*
 * [ARC] tests/harness/arc_stress.c
 *
 * Temporary churn: 10^6 iterations of create/retain/release/concat
 * against the ARC engine, verifying no heap exhaustion, no
 * double-free, and balanced refcounts at every step.
 *
 * Expected behavior: exit 0. Build with -fsanitize=address for the
 * leak-checked variant (make check runs both).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../include/api.h"

#define N 1000000

static int failures;

static void check(int cond, const char *what)
{
    if (!cond) {
        fprintf(stderr, "arc_stress: FAILED: %s\n", what);
        failures++;
    }
}

int main(void)
{
    for (int i = 0; i < N; i++) {
        PithValue *a = pith_str_new("stress", 6);
        PithValue *b = pith_str_new("-churn", 7);
        check(a && b, "allocation");
        if (!a || !b) {
            return 1;
        }

        PithValue *c = pith_str_concat(a, b);
        check(c != NULL, "concat");
        check(c->length == 13, "concat length");
        check(memcmp(c->data, "stress-churn", 13) == 0, "concat bytes");

        /* two extra owners */
        pith_retain(c);
        pith_retain(c);
        check(c->strongRefs == 3, "refcount after retains");
        pith_release(c);
        pith_release(c);
        check(c->strongRefs == 1, "refcount after releases");

        pith_release(c);
        pith_release(a);
        pith_release(b);
    }

    /* aliasing churn: transfer semantics must stay balanced - this is
       exactly what the compiler emits for `y = x` (retain the alias,
       drop the original owner, release at scope exit) */
    for (int i = 0; i < 100000; i++) {
        PithValue *s = pith_str_new("alias", 5);   /* refs: 1 */
        pith_retain(s);                             /* refs: 2 */
        PithValue *alias = s;                      /* transferred ref */
        pith_release(s);                            /* refs: 1 */
        check(alias->strongRefs == 1, "alias refcount");
        pith_release(alias);                        /* freed */
    }

    /* equality under churn */
    PithValue *x = pith_str_new("same", 4);
    PithValue *y = pith_str_new("same", 4);
    check(pith_str_equals(x, y) == 1, "equals true");
    PithValue *z = pith_str_new("diff", 4);
    check(pith_str_equals(x, z) == 0, "equals false");
    pith_release(x);
    pith_release(y);
    pith_release(z);

    if (failures) {
        fprintf(stderr, "arc_stress: %d failure(s)\n", failures);
        return 1;
    }
    printf("arc_stress: %d+100000 iterations ok\n", N);
    return 0;
}
