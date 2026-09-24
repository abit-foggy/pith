/*
 * FFI test module for pith's native C import pipeline.
 *
 * Covers: 32/64-bit integer args+returns, double and single-precision
 * floats, string borrows, owned string returns (+1 reference, per the
 * pith.h ABI contract), void calls, and internal state.
 */
#include <pith.h>
#include <stdio.h>

static long counter = 0;

int addInts(int a, int b)
{
    return a + b;
}

long addLongs(long a, long b)
{
    return a + b;
}

double scale(double x, double k)
{
    return x * k;
}

float halfFloat(float x)
{
    return x * 0.5f;
}

/* borrowed string parameter */
long stringLength(PithValue *s)
{
    return (long)pithStringLength(s);
}

/* borrowed parameters; returns an owned string (+1 reference) */
PithValue *greet(PithValue *name)
{
    char buf[256];
    snprintf(buf, sizeof(buf), "hello, %s!", pithStringData(name));
    return pithNewString(buf);
}

/* returns an owned string with no parameters */
PithValue *makeSuffix(void)
{
    return pithNewString("!!!");
}

/* void return; internal state proves the call executed */
void bumpCounter(long by)
{
    counter += by;
}

long readCounter(void)
{
    return counter;
}

/* copy a borrowed value into a fresh owned string (+1 reference) */
PithValue *echo(PithValue *s)
{
    return pithNewStringN(pithStringData(s), pithStringLength(s));
}
