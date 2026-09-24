# Getting Started

## Install

From GitHub releases:

```sh
curl -fsSL https://raw.githubusercontent.com/abit-foggy/pith/main/install.sh | sh
```

Or build from source:

```sh
git clone --recurse-submodules https://github.com/abit-foggy/pith
cd pith
make
```

External tools needed at runtime: `qbe` (QBE compiler) and `as` (GNU
binutils). The vendored tcc is a git submodule; the build compiles it
automatically.

## Your first script

Create a file called `hello.pi`:

```pith
x = "hello"
if x == "hello"
    print x + ", pith!"
end
```

Run it:

```sh
pith run hello.pi
```

Output:

```
hello, pith!
```

## Build a native binary

```sh
pith build hello.pi
./hello
```

The output is a fully standalone native executable — no VM, no
interpreter, no runtime dependency beyond libc.

## Decompile

Inspect the generated QBE IL:

```sh
pith decompile hello.pi
```

```qbe
data $str.1 = { w 1, h 3, h 1, w 7, w 6, b "hello", b 0 }

export function w $main() {
@main.start
    %.v1_x =l alloc8 8
    storel $str.1, %.v1_x
    %.t1 =l loadl %.v1_x
    %.t2 =w call $pith_str_equals(l %.t1, l $str.1)
    jnz %.t2, @L2, @L3
@L2
    %.t3 =l loadl %.v1_x
    %.t4 =l call $pith_str_concat(l %.t3, l $str.2)
    call $pith_rt_print(l %.t4)
    call $pith_release(l %.t4)
    jmp @L1
@L3
@L1
@main.exit
    ret 0
}
```

Notice:
- `alloc8 8` — the variable's stack slot
- `call $pith_str_concat` — string `+` lowered to a runtime call
- `call $pith_release` — deterministic ARC at the scope boundary

## Import C code

Write a small C module:

```c
/* ffi/math.c */
#include <pith.h>

int addInts(int a, int b)
{
    return a + b;
}
```

Then use it from pith:

```pith
import "ffi/math.c"
sum = math.addInts(3, 4)
if sum == 7
    print "ffi works!"
end
```

See the [C Imports](/ffi) page for the full ABI contract.

## What's next

- [Language Reference](/language) — complete syntax and semantics
- [C Imports (FFI)](/ffi) — typed native C calls with ARC handoffs
- [Runtime & Memory](/runtime) — the PithValue ABI and ARC internals
- [Embeddable C ABI](/embed) — host pith in your C/C++ application
- [CLI Reference](/cli) — run, build, decompile, pkg, engine, tasks
