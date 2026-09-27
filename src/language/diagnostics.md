# Diagnostics & Error Catalog

Pith provides clear, rustc-inspired compiler diagnostics designed to help developers immediately pinpoint and fix issues.

---

## Diagnostic Format

Every diagnostic displays the severity level, explanatory message, file path, line and column numbers, a source snippet preview, and an indicator caret:

```text
error: cannot assign twice to immutable variable `x` (declare with `mut` to reassign)
  --> script.pi:3:1
   |
 3 | x = 20
   | ^
```

---

## Common Compiler Errors

### 1. Reassigning Immutable Variable

```text
error: cannot assign twice to immutable variable `name` (declare with `mut` to reassign)
```
- **Cause**: Attempting to assign a new value to a variable that was declared without [`mut`](variables.md#the-mut-modifier).
- **Fix**: Declare the variable with `mut name = ...` if its value needs to change.

### 2. Loop Control Outside of a Loop

```text
error: `break` outside of a loop
error: `continue` outside of a loop
```
- **Cause**: Using `break` or `continue` outside of a [`while`](control-flow.md#loop-controls-break-and-continue) block.
- **Fix**: Ensure loop control statements are located inside a valid `while ... end` loop.

### 3. Out-of-Range Literal

```text
error: literal is out of range for type u8 (maximum value is 255)
error: unsigned type u8 cannot hold a negative value
```
- **Cause**: Assigning a literal value that exceeds the bit width or sign bounds of an [Explicit Type](types.md#static-bounds-checking).
- **Fix**: Adjust the literal value or widen the type (e.g. to `u16` or `i32`).

### 4. Nested Block Overflow

```text
error: block nesting is too deeply nested
```
- **Cause**: Nesting conditionals or loops beyond the compiler's safety guard (256 levels).
- **Fix**: Refactor deep nesting into separate [Functions](functions.md).

### 5. Undeclared Identifier

```text
error: use of undeclared identifier `variableName`
```
- **Cause**: Referencing an identifier that has not been defined in the current or parent scope.
- **Fix**: Declare the variable with `name = value` before referencing it.

### 6. Misplaced Import

```text
error: imports must be declared at the top level
```
- **Cause**: Placing an `import "file.c"` inside a function, `if`, or `while` block.
- **Fix**: Move the import statement to the top level of your script file.

---

## Informational Notes

The compiler also provides helpful informational notes during compilation:

- **Dead Function Elimination**:
  ```text
  note: private function is never referenced; eliminated (zero-bloat)
  ```
  Informs you that an unreferenced function was removed to keep the binary small.

- **Unreachable Code**:
  ```text
  note: unreachable exit path: function body always returns
  ```
  Informs you that all branches of a function return values, making any trailing statements unreachable.

- **Namespace Overrides**:
  ```text
  note: alice.os.identifyKernel overrides os.identifyKernel
  ```
  Informs you that an imported C module has overridden a symbol in the active namespace view.

---

## See Also

- [Syntax Overview](index.md) — Fundamental language rules
- [Variables & Mutability](variables.md) — Scope and mutation rules
- [C Imports (FFI)](../ffi.md) — Importing and calling native C libraries
