# Language Reference

Pith is a bracketless, semicolon-free systems-scripting language.
Statements are newline-delimited; blocks close with a single `end`.

## Statements

### Assignment

`name = expr` declares a new variable when the name exists nowhere in
any enclosing scope; it reassigns when the name already exists:

```pith
x = 10          # declaration: x doesn't exist yet
x = x + 5       # reassignment: x exists in this scope
```

No `let`, `var`, or type annotations. Names must be `lowerCamelCase`
— the compiler warns on a capital first letter.

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

No parentheses, no colons — the condition is just an expression
followed by a newline. Blocks may nest up to 256 levels (the
compiler rejects deeper nesting with a clean diagnostic, not a stack
overflow) and may be empty.

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

Parentheses group subexpressions (up to 128 levels).

### Types

| Pith type | QBE type | Size |
|---|---|---|
| integer | `l` | 64-bit signed two's-complement |
| float | `d` | IEEE 754 double |
| string | `l` (pointer) | Refcounted `PithValue` |
| boolean | `w` | 32-bit (0 or 1) |

Mixed int/float arithmetic promotes the int to float. String `+`
concatenates; numeric `+` adds.

### String literals

Double-quoted with C-style escapes:

```pith
s = "hello\tworld"
q = "quote:\"back\\slash"
n = "a\0b"         # embedded NUL — lossless
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

Deterministic ARC — the compiler injects retain/release at scope
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
error: use of undeclared identifier `undefinedVar`
  --> script.pi:1:7
   |
 1 | print undefinedVar
   |       ^~~~~~~~~~~~
```

## Builtins

| Builtin | Returns | Description |
|---|---|---|
| `os.identifyKernel` | string | `"linux"`, `"darwin"`, `"nt"`, `"freebsd"` |
| `os.identifyKernelVersion` | string | Kernel/OS version (uname.release) |
| `os.isNT` | bool | `1` on Windows NT, `0` elsewhere |
