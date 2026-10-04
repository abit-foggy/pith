/*
 * [EMBED] tests/harness/embed_host_impl.c
 *
 * The host functions for the embed harness. Compiled to its own
 * object under the c_host_* symbol renames (exactly the renames pith
 * applies to imported C units): the in-memory backend binds the
 * registered addresses, the temp-executable fallback links this
 * object into the child process, and both must observe the same
 * behavior.
 *
 * Expected behavior: exit 0 via the harness below - each function
 * answers through the FFI ABI classes it is registered with.
 */
#define _POSIX_C_SOURCE 200809L

#include <stdio.h>

#include "../../include/pith.h"

/* 'w' return, zero parameters: readable as a pseudo-constant
   (`host.seven` bare member access) */
int seven(void)
{
    return 7;
}

/* 'w' return, one 'w' parameter (the compiler truncates pith's
   64-bit integers at the boundary) */
int twice(int x)
{
    return 2 * x;
}

/* 'l' return, one 'l' parameter: full-width integers */
long wide(long x)
{
    return x + x;
}

/* 'v' return, one borrowed 'p' parameter: statement calls only */
void note(PithValue *s)
{
    printf("note: %s\n", pithStringData(s));
}

/* 'p' return, one borrowed 'p' parameter: a NEW reference (+1) that
   the generated code releases at the scope boundary */
PithValue *tag(PithValue *s)
{
    char buf[512];
    snprintf(buf, sizeof(buf), "[%s]", pithStringData(s));
    return pithNewString(buf);
}
