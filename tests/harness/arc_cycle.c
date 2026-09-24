/*
 * [ARC] tests/harness/arc_cycle.c
 *
 * Circular reference mitigation: pith_break_cycle drops the parent's
 * strong reference to the child cleanly (without freeing it while
 * other owners exist) and flags the child IS_SHARED.
 *
 * Expected behavior: exit 0; zero leaks under ASan.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../include/api.h"

static int failures;

static void check(int cond, const char *what)
{
    if (!cond) {
        fprintf(stderr, "arc_cycle: FAILED: %s\n", what);
        failures++;
    }
}

int main(void)
{
    /* parent holds a strong ref to child; a second owner keeps the
       child alive across the cycle break */
    PithValue *child = pith_str_new("node", 4);
    pith_retain(child);
    PithValue *parent = pith_str_new("parent", 6);

    check(child && parent, "allocation");
    check(child->strongRefs == 2, "two owners");

    /* explicit cycle break: drops the parent's strong reference */
    pith_break_cycle(parent, child);

    check(child->strongRefs == 1, "one owner after break");
    check((child->flags & PITH_FLAG_SHARED) != 0,
          "child flagged IS_SHARED");
    check(pith_str_equals(child, child) == 1, "child still usable");

    /* the child lives on; the second owner frees it now */
    pith_release(child);
    pith_release(parent);

    /* static data must be immortal: retain/release are no-ops */
    PithValue *empty = pith_str_new(NULL, 0);
    check(empty != NULL, "empty allocation");
    pith_retain(empty);
    pith_release(empty);
    pith_release(empty);

    if (failures) {
        fprintf(stderr, "arc_cycle: %d failure(s)\n", failures);
        return 1;
    }
    printf("arc_cycle: break_cycle semantics ok\n");
    return 0;
}
