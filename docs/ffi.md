# Native C Imports (FFI)

Pith scripts import C source directly. The exported (non-static)
functions become callable through the unit's namespace, with the
compiler handling type conversions and ARC at every boundary.

## Syntax

```pith
import "ffi/math.c"

sum = math.addInts(3, 4)
msg = math.greet("world")     # owned string return, ARC-released
math.logNote(42)              # void call as a statement
```

The namespace is the file's basename sans `.c`, `ffi/math.c` becomes
`math`.

## How it works

### JIT (in-memory)

Each imported unit compiles into its **own libtcc state** (per-state
isolation prevents symbol collisions between imports). The pre-baked
`<pith.h>` virtual header is staged into a temp include dir, and the
runtime symbols (`pithRetain`, `pithNewString`, etc.) are registered
into the import's state before compilation. After relocation, the
exported function addresses are linked into the main execution state
under their real symbol names.

### AOT (standalone binary)

Each unit compiles to an object file via libtcc `TCC_OUTPUT_OBJ` (or
the platform C compiler when libtcc is absent). The object joins the
QBE-generated `.o` and `runtime/libruntime.a` at link time.

### Codegen

The compiler emits a private namespaced shim per foreign function
with the typed System V AMD64 / AAPCS64 signature:

```qbe
function s $c_math_halfFloat(s %a0) {
@c_math_halfFloat.start
    %r =s call $halfFloat(s %a0)
    ret %r
}
```

Call sites emit typed calls through the shim, with widening/narrowing
conversions at the boundary.

## Type mapping

| C type | QBE type | Pith type |
|---|---|---|
| `int`, `enum`, `bool`, `int32_t` | `w` | integer (widened via `extsw`) |
| `long`, `int64_t`, `size_t`, `T*` | `l` | integer or string |
| `float` | `s` | float (narrowed via `truncd`) |
| `double` | `d` | float |
| `void` |, | (statement only) |
| `PithValue*` | `l` | string (owned, +1 reference) |

The prototype scanner (`src/cffi.c`) discovers non-static function
definitions at compile time, parsing return types and parameter lists
from the C source. Multi-line signatures, pointer params, and `(void)`
parameter lists are handled.

## ABI contract

Imported C code includes `<pith.h>` and follows these rules:

- **Parameters are borrowed**, the callee must call `pithRetain()`
  before storing a `PithValue*` beyond the call, and owns that extra
  reference afterwards.
- **Returns must be +1**, a function returning `PithValue*` must
  return a new reference; the Pith compiler injects the matching
  release at scope boundaries.
- **Handoffs are leak-free**, the compiler's ASan-verified test suite
  confirms zero leaks across the FFI boundary.

## Writing an import module

```c
#include <pith.h>
#include <stdio.h>

/* 32-bit ints: w */
int addInts(int a, int b)
{
    return a + b;
}

/* borrowed string param */
long stringLength(PithValue *s)
{
    return (long)pithStringLength(s);
}

/* owned string return: +1 reference */
PithValue *greet(PithValue *name)
{
    char buf[256];
    snprintf(buf, sizeof(buf), "hello, %s!", pithStringData(name));
    return pithNewString(buf);
}

/* void: call as a statement */
void logNote(int code)
{
    fprintf(stderr, "note: %d\n", code);
}
```

## Runtime helpers available to imports

From `pith.h`:

| Function | Description |
|---|---|
| `pithRetain(PithValue*)` | Atomically bump the refcount |
| `pithRelease(PithValue*)` | Drop a ref; frees at zero |
| `pithNewString(const char*)` | Allocate a string (+1 reference) |
| `pithNewStringN(const char*, uint32_t)` | Allocate with explicit length |
| `pithStringData(PithValue*)` | Borrowed payload pointer |
| `pithStringLength(PithValue*)` | Payload byte length |
| `pithStringEquals(PithValue*, PithValue*)` | Content equality |
| `pithStringConcat(PithValue*, PithValue*)` | New concatenated string (+1) |
