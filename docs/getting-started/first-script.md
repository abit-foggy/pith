# Your First Script

## Write it

Create a file called `hello.pi`:

```pith
# hello.pi
name = "pith"
if name == "pith"
    print "hello, " + name + "!"
end
```

Pith is bracketless: blocks open with `if`/`fn` and close with a
single `end`. No semicolons, no colons after conditions. Statements
are newline-delimited.

## Variables and types

Variables are **immutable by default**. Use `mut` for reassignment:

```pith
x = 10          # immutable
mut y = 20      # mutable
y = y + 5       # ok
```

Assign explicit sized types with `name : type = expr`:

```pith
mut byte: u8 = 255
byte = byte + 1
if byte == 0
    print "wrapped!"
end
```

See the [Language Reference](/language) for all types and wrapping
semantics.

## Run it

```sh
pith run hello.pi
```

```
hello, pith!
```

This takes the instant pipeline: lex → parse → QBE IR → `qbe` →
assembly → **libtcc in-memory** → executed natively. No temp
executable, no heavyweight compiler driver.

## Build a standalone binary

```sh
pith build hello.pi
./hello
```

The output is a fully standalone native executable linked with the
embedded tcc linker. No VM, no interpreter, no runtime dependency
beyond libc.

## See what the compiler produced

```sh
pith decompile hello.pi
```

```qbe
data $str.1 = { w 1, h 3, h 1, w 6, w 5, b "pith", b 0 }

export function w $main() {
@main.start
    %.v1_name =l alloc8 8
    storel $str.1, %.v1_name
    %.t1 =l loadl %.v1_name
    %.t2 =w call $pith_str_equals(l %.t1, l $str.1)
    jnz %.t2, @L2, @L3
@L2
    %.t3 =l call $pith_str_concat(l $str.2, l %.v1_name)
    %.t4 =l call $pith_str_concat(l %.t3, l $str.3)
    call $pith_rt_print(l %.t4)
    call $pith_release(l %.t4)
    call $pith_release(l %.t3)
    jmp @L1
@L3
@L1
@main.exit
    ret 0
}
```

Notice:
- `alloc8 8` — every variable lives in its own stack slot (sized
  types use `alloc4` for 1-4 byte storage)
- `call $pith_str_concat` — string `+` lowered to a runtime call
- `call $pith_release` — deterministic ARC at the scope boundary

## Import C code

Create a small C module:

```c
/* ffi/math.c */
#include <pith.h>

int addInts(int a, int b)
{
    return a + b;
}

double scale(double x, double k)
{
    return x * k;
}
```

Use it from pith:

```pith
import "ffi/math.c"

sum = math.addInts(3, 4)
if sum == 7
    print "ints: ok"
end

scaled = math.scale(1.5, 2.0)
if scaled == 3.0
    print "doubles: ok"
end
```

Run it, both the JIT and AOT paths support imports:

```sh
pith run script.pi
pith build script.pi && ./script
```

See [C Imports (FFI)](/ffi) for the full ABI contract and type mapping.

## Next steps

- [Setting Up a Project](/getting-started/project-setup) — pith.toml, multi-file builds, tasks
- [Language Reference](/language) — complete syntax, types, and mutability
- [CLI Reference](/cli/run) — every command in detail
