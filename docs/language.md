# Language Reference

Pith is a bracketless, semicolon-free systems-scripting language.
Statements are newline-delimited; blocks close with a single `end`.

## Statements

### Variable declaration

`name = expr` declares an **immutable** variable. The `mut` modifier
makes it mutable:

```pith
x = 10          # immutable — reassigning is a compile error
mut y = 20      # mutable — can be reassigned
```

Reassigning an immutable variable:

```pith
x = 10
x = 20          # error: cannot assign twice to immutable variable `x`
```

### Explicit types

`name : type = expr` declares a variable with an explicit sized type:

```pith
byte: u8 = 200
half: i16 = -1000
quad: i32 = 100000
wide: u64 = 42
flt: f32 = 1.5
precise: f64 = 3.14159265358979

mut counter: u32 = 0     # mutable + typed
```

Available types:

| Type | Size | Range |
|---|---|---|
| `i8` | 1 byte | -128 to 127 |
| `u8` | 1 byte | 0 to 255 |
| `i16` | 2 bytes | -32768 to 32767 |
| `u16` | 2 bytes | 0 to 65535 |
| `i32` | 4 bytes | -2147483648 to 2147483647 |
| `u32` | 4 bytes | 0 to 4294967295 |
| `i64` | 8 bytes | -9223372036854775808 to 9223372036854775807 |
| `u64` | 8 bytes | 0 to 18446744073709551615 (up to INT64_MAX in v0.1) |
| `f32` | 4 bytes | IEEE 754 single-precision |
| `f64` | 8 bytes | IEEE 754 double-precision |

Without an explicit type, integers default to `i64` and floats to `f64`.

### Static bounds checking

Literals assigned to explicitly typed variables are checked at compile
time:

```pith
val: u8 = 256     # error: literal is out of range for type u8
val: u8 = -1      # error: unsigned type u8 cannot hold a negative value
val: i8 = 128     # error: literal is out of range for type i8
```

### Storage and wrapping

Sized values are stored at their declared width and truncated on
writeback. Arithmetic always happens at 64-bit, so wrapping occurs
when the result is stored back:

```pith
mut b: u8 = 255
b = b + 1         # b is now 0 (wraps via storeb truncation)

mut s: i8 = 127
s = s + 1         # s is now -128 (wraps via storeb + loadsb)
```

Loads sign-extend or zero-extend to the full 64-bit register for
arithmetic. Unsigned types (`u8`, `u16`, `u32`) zero-extend; signed
types (`i8`, `i16`, `i32`) sign-extend.

### print

Prints a string to stdout followed by a newline:

```pith
print "Running on " + os.identifyKernel
```

### return

Exits the enclosing function (or `$main`). Takes an optional
expression:

```pith
fn getAnswer
    return 42
end
```

### import (top-level only)

Imports a C source file for native FFI calls:

```pith
import "ffi/math.c"
sum = math.addInts(3, 4)
```

See [C Imports (FFI)](/ffi).

### fn (top-level only)

Declares a private function:

```pith
fn myHelper
    return 1
end
```

## Control flow

### if / elseif / else / end

```pith
if os.identifyKernel == "linux"
    print "linux"
elseif os.isNT
    print "windows"
else
    print "other"
end
```

No parentheses, no colons. The condition is just an expression followed
by a newline. Blocks may nest up to 256 levels and may be empty.

## Expressions

### Operator precedence

| Level (low→high) | Operators |
|---|---|
| Equality | `==` `!=` |
| Comparison | `<` `<=` `>` `>=` |
| Additive | `+` `-` |
| Multiplicative | `*` `/` |
| Unary | `-` (negation) |
| Postfix | `.` (member access), `(...)` (call) |

### Types

| Pith type | QBE type | Size |
|---|---|---|
| integer (default) | `l` | 64-bit signed two's-complement |
| float (default) | `d` | IEEE 754 double |
| string | `l` (pointer) | Refcounted `PithValue` |
| boolean | `w` | 32-bit (0 or 1) |

Mixed int/float arithmetic promotes the int to float. String `+`
concatenates; numeric `+` adds. Small integer types (i8, u8, etc.)
are promoted to 64-bit when loaded for arithmetic.

### String literals

Double-quoted with C-style escapes:

```pith
s = "hello\tworld"
q = "quote:\"back\\slash"
n = "a\0b"         # embedded NUL, lossless
u = "ünïcødé → ✓"  # UTF-8 payload bytes
```

Escape sequences: `\n` `\t` `\r` `\0` `\\` `\"`

### Member access

```pith
os.identifyKernel          # -> "linux", "darwin", "nt", "freebsd"
os.identifyKernelVersion   # -> kernel version string
os.isNT                    # -> 1 on Windows NT, 0 elsewhere
```

### Function calls (FFI)

```pith
result = math.addInts(3, 4)
math.logNote(42)     # void calls may be bare statements
```

## Memory model

Deterministic ARC, the compiler injects retain/release at scope
boundaries. See [Runtime & Memory](/runtime).

## Comments

`#` starts a line comment:

```pith
# this is a comment
x = 1  # trailing comment
```

## Diagnostics

Errors use rustc-style output with source preview, line numbers, and
a colored caret:

```
error: cannot assign twice to immutable variable `x` (declare with `mut` to reassign)
  --> script.pi:3:1
   |
 3 | x = 20
   | ^
```

## Builtins

| Builtin | Returns | Description |
|---|---|---|
| `os.identifyKernel` | string | `"linux"`, `"darwin"`, `"nt"`, `"freebsd"` |
| `os.identifyKernelVersion` | string | Kernel/OS version (uname.release) |
| `os.isNT` | bool | `1` on Windows NT, `0` elsewhere |
