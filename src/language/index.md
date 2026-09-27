# Language Reference

Pith is a bracketless, semicolon-free systems-scripting language compiled directly to native machine code via QBE. Statements are newline-delimited, and blocks close with a single `end`.

---

## Language Reference Chapters

This reference is divided into in-depth, dedicated chapters:

1. **[Variables & Mutability](variables.md)** — Variable bindings, `mut` semantics, immutability by default, lexical scoping, and shadowing.
2. **[Types & Static Bounds](types.md)** — Sized integer types (`i8`–`u64`), floating-point numbers (`f32`/`f64`), booleans, strings, and compile-time bounds checking.
3. **[Expressions & Operators](expressions.md)** — Operator precedence, short-circuit `and`/`or`, `not`, arithmetic, comparison, and member access.
4. **[Control Flow](control-flow.md)** — `if` / `elseif` / `else` / `end`, `while` / `break` / `continue` / `end`, nesting limits, and loop ARC cleanup.
5. **[Functions & Calls](functions.md)** — `fn` declarations, parameter lists, typed parameters, recursion, return semantics, and zero-bloat dead function elimination.
6. **[Diagnostics & Error Catalog](diagnostics.md)** — Compiler error format, common beginner mistakes, and resolution steps.

---

## Master Syntax & Keyword Matrix

| Keyword / Construct | Example Syntax | Purpose | Chapter |
|---|---|---|---|
| `name = expr` | `x = 42` | Immutable variable declaration | [Variables](variables.md) |
| `mut` | `mut count = 0` | Mutable variable declaration | [Variables](variables.md) |
| `name : type` | `val: u8 = 200` | Explicit sized type annotation | [Types](types.md) |
| `print` | `print "hello"` | Prints string, integer, or bool to stdout | [Expressions](expressions.md) |
| `if` / `else` | `if x > 0 ... end` | Conditional branching | [Control Flow](control-flow.md) |
| `while` | `while i < 10 ... end` | Loop block | [Control Flow](control-flow.md) |
| `break` | `break` | Exits innermost loop | [Control Flow](control-flow.md) |
| `continue` | `continue` | Skips to next loop iteration | [Control Flow](control-flow.md) |
| `and` / `or` / `not` | `a and not b` | Short-circuit boolean logic | [Expressions](expressions.md) |
| `fn` | `fn add(a, b) ... end` | User-defined function | [Functions](functions.md) |
| `return` | `return 42` | Returns value from function | [Functions](functions.md) |
| `import` | `import "math.c"` | Direct C FFI source import | [FFI](../ffi.md) |
| `.` | `os.isLinux` | Dotted member / namespace access | [Namespaces](../namespaces.md) |

---

## Lexical Structure

### Newlines & Statements

Pith code is organized into lines. Semicolons are not used. Statements are separated by newlines:

```pith
x = 10
y = 20
print x + y
```

Placing multiple statements on a single line without a newline produces a compiler diagnostic.

### Comments

Comments begin with `#` and continue until the end of the line:

```pith
# This is a full-line comment
x = 10  # This is a trailing inline comment
```

### Identifiers

Identifiers must start with an ASCII letter or underscore (`[a-zA-Z_]`) and may be followed by letters, digits, or underscores (`[a-zA-Z0-9_]`). Keywords cannot be used as variable or function names.

### String Literals & Escapes

String literals are enclosed in double quotes (`"..."`):

```pith
greeting = "Hello, World!\n"
path = "C:\\Windows\\System32"
quote = "He said \"Pith is fast\""
embedded_nul = "a\0b"
```

Supported escape sequences:
- `\n` — Line feed
- `\r` — Carriage return
- `\t` — Tab
- `\0` — Null character (lossless embedded byte)
- `\\` — Backslash
- `\"` — Double quote

---

## Standard Runtime & Namespaces

Pith provides built-in tools for systems development:

- **`os` Namespace**: Query platform details (`os.isLinux`, `os.isNT`), read/write files (`os.readFile`, `os.writeFile`), query environment variables (`os.getEnv`), inspect CLI args (`os.argCount`, `os.getArg`), and control exit codes (`os.exit`). See [Namespaces](../namespaces.md).
- **`net` Namespace**: Dead-simple TCP socket operations (`net.connect`, `net.send`, `net.recv`, `net.close`). See [Namespaces](../namespaces.md).
