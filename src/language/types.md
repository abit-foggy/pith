# Types & Static Bounds

Pith combines the simplicity of dynamic scripting with the precision and performance of a native systems language. You can let types be inferred automatically, or annotate variables with precise bit widths.

---

## Type Inference & Defaults

If you declare a variable without an explicit type, Pith assigns the standard platform defaults:
- **Integers**: Default to `i64` (64-bit signed two's complement integer).
- **Floating-point**: Default to `f64` (IEEE 754 double precision).
- **Strings**: Default to reference-counted ARC strings.
- **Booleans**: Evaluated as `1` (`true`) or `0` (`false`).

```pith
count = 42          # Inferred as i64
pi = 3.14159        # Inferred as f64
name = "Pith"       # Inferred as refcounted string
```

---

## Available Types

Pith provides a full suite of sized numeric types, floating-point representations, and primitive types:

| Type | Classification | Size | Range / Description | QBE IR Type |
|---|---|---|---|---|
| `i8` | Signed Integer | 1 byte | `-128` to `127` | `w` (word) |
| `u8` | Unsigned Integer | 1 byte | `0` to `255` | `w` (word) |
| `i16` | Signed Integer | 2 bytes | `-32,768` to `32,767` | `w` (word) |
| `u16` | Unsigned Integer | 2 bytes | `0` to `65,535` | `w` (word) |
| `i32` | Signed Integer | 4 bytes | `-2,147,483,648` to `2,147,483,647` | `w` (word) |
| `u32` | Unsigned Integer | 4 bytes | `0` to `4,294,967,295` | `w` (word) |
| `i64` | Signed Integer | 8 bytes | `-9,223,372,036,854,775,808` to `9,223,372,036,854,775,807` | `l` (long) |
| `u64` | Unsigned Integer | 8 bytes | `0` to `18,446,744,073,709,551,615` (up to INT64_MAX) | `l` (long) |
| `f32` | Floating-point | 4 bytes | IEEE 754 single precision float | `s` (single) |
| `f64` | Floating-point | 8 bytes | IEEE 754 double precision float | `d` (double) |
| `bool` | Boolean | 1 byte | Truth values (`1` or `0`) | `w` (word) |
| `string` | Dynamic String | Pointer | Length-prefixed, UTF-8 ARC buffer | `l` (pointer) |

---

## Explicit Type Annotations

Declare explicit types using the `:` syntax on [Variables](variables.md) or [Function Parameters](functions.md):

```pith
# Immutable typed variables
byte: u8 = 200
half: i16 = -1000
quad: i32 = 100000
wide: i64 = 9223372036854775800
flt: f32 = 1.5
precise: f64 = 3.141592653589793

# Mutable typed variables
mut counter: u32 = 0
```

---

## Static Bounds Checking

When integer literals are assigned to explicitly typed variables, Pith performs compile-time bounds verification. If a literal cannot fit within the type's range, compilation halts immediately with an informative diagnostic:

```pith
val: u8 = 256     # error: literal is out of range for type u8
neg: u8 = -1      # error: unsigned type u8 cannot hold a negative value
idx: i8 = 128     # error: literal is out of range for type i8
```

Diagnostic example:
```text
error: literal is out of range for type u8 (maximum value is 255)
  --> script.pi:1:11
   |
 1 | val: u8 = 256
   |           ^^^
```

---

## Storage & Deterministic Wrapping

How sized types work under the hood in Pith:

1. **Storage Width**: Variables are stored in memory at their exact declared bit width. For example, a `u8` takes exactly 1 byte on the stack slot.
2. **Arithmetic at 64-bit**: When performing arithmetic, values are loaded and promoted to 64-bit registers:
   - Unsigned types (`u8`, `u16`, `u32`) are **zero-extended**.
   - Signed types (`i8`, `i16`, `i32`) are **sign-extended**.
3. **Writeback Truncation**: When storing the result back, the value is truncated via bit slicing (`storeb`, `storeh`, `storew` in QBE IR):

```pith
mut b: u8 = 255
b = b + 1         # b wraps to 0 via storeb truncation

mut s: i8 = 127
s = s + 1         # s wraps to -128 via storeb + loadsb sign extension
```

This guarantees predictable, standard systems behavior without hidden runtimes or unpredictable overflow panics.

---

## Strings

Strings in Pith are length-prefixed, null-terminated, UTF-8 buffers managed by [Deterministic ARC](../runtime.md):

```pith
greeting = "Hello, World!"
utf8_str = "ünïcødé → ✓"
raw_data = "null\0embedded"   # Embedded NUL bytes are completely lossless
```

- **Escape Sequences**: `\n` (newline), `\t` (tab), `\r` (carriage return), `\0` (NUL), `\\` (backslash), `\"` (quote).
- **Concatenation**: Strings can be concatenated using the `+` operator.

---

## See Also

- [Variables & Mutability](variables.md) — How to declare and reassign variables
- [Expressions & Operators](expressions.md) — Arithmetic and type promotion rules
- [Runtime & Memory Model](../runtime.md) — How ARC manages strings with zero GC
