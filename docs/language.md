# Language Reference

Pith is a bracketless, semicolon-free systems-scripting language. This page covers the complete syntax and semantics.

## Statements

Every statement lives on its own line. No semicolons, no braces.

```pith
x = 10
print "hello"
if x == 10
    print "ten"
end
```

### Assignment

`name = expr` declares a new variable when the name exists nowhere in
any enclosing scope; it reassigns when the name already exists.

```pith
x = 10          # declaration: x doesn't exist yet
x = x + 5       # reassignment: x exists in this scope
```

Names must be `lowerCamelCase` — the compiler warns on a capital
first letter. There is no `let`, `var`, or type annotation.

### print

Prints a string to stdout followed by a newline:

```pith
print "Running on " + os.identifyKernel
```

### return

Exits the enclosing function (or `$main` at the top level). Takes an
optional expression:

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

See the [C Imports](/ffi) page for the full pipeline.

### fn (top-level only)

Declares a private function (emitted but not callable from other
units in v0.1):

```pith
fn myHelper
    return 1
end
```

## Control flow

### if / elseif / else / end

No parentheses, no colons — the condition is just an expression
followed by a newline:

```pith
if os.identifyKernel == "linux"
    print "linux"
elseif os.isNT
    print "windows"
else
    print "other"
end
```

Branches can nest arbitrarily deep (up to 256 levels — the compiler
rejects deeper nesting with a clean diagnostic, not a stack overflow).
Blocks may be empty:

```pith
if 1 == 0
else
end
```

## Expressions

### Operators

| Precedence (low→high) | Operators |
|---|---|
| Equality | `==` `!=` |
| Comparison | `<` `<=` `>` `>=` |
| Additive | `+` `-` |
| Multiplicative | `*` `/` |
| Unary | `-` (negation) |
| Member access / call | `.` `(args)` |

Parentheses group subexpressions (up to 128 levels of nesting — deeper
expressions are rejected with a clean diagnostic).

### Types

| Pith type | QBE type | Description |
|---|---|---|
| `integer` | `l` (64-bit) | Signed 64-bit two's-complement |
| `float` | `d` (64-bit) | IEEE 754 double |
| `string` | `l` (pointer) | Refcounted `PithValue` |
| `boolean` | `w` (32-bit) | 0 or 1 |

Type inference is purely lexical: the type of an expression is
determined by its operands. Mixed int/float arithmetic promotes the
int to float. String `+` concatenates; numeric `+` adds.

### String literals

Double-quoted with C-style escapes:

```pith
s = "hello\tworld"
q = "quote:\"back\\slash"
n = "a\0b"        # embedded NUL — length-aware, lossless
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

Calls use postfix syntax on a namespace member:

```pith
result = math.addInts(3, 4)
math.logNote(42)     # void calls may be bare statements
```

See [C Imports](/ffi) for how namespaces are created.

## Memory model

Pith uses **deterministic Automated Reference Counting** (ARC). The
compiler injects retain/release calls at scope boundaries — you never
manage memory manually.

- **Declarations** allocate a stack slot (`alloc8`) and store the value.
- **Reassignments** release the old value before storing the new one.
- **Scope exits** (`end`, `return`) release every local ARC allocation.
- **String concatenation** produces a fresh owned value, released when
  its consuming scope exits.

Static string literals are immortal (flagged `PITH_FLAG_STATIC`) —
retain and release are no-ops on them.

## Comments

`#` starts a line comment (like Lua):

```pith
# this is a comment
x = 1  # trailing comment
```

## Diagnostics

Errors and warnings are rustc-style with source preview, line numbers,
and a colored caret pointer:

```
error: use of undeclared identifier `undefinedVar`
  --> tests/test_err1.pi:1:7
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
