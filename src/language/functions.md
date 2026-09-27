# Functions & Calls

Functions in Pith encapsulate reusable logic. They are declared with the `fn` keyword and closed with `end`.

---

## Function Declarations

Functions are declared at the top level of a script:

```pith
fn greet(name)
    print "Hello, " + name
end
```

### Parameters & Type Annotations

Parameters are listed in parentheses following the function name:

```pith
fn add(a, b)
    return a + b
end

sum = add(10, 20)
print sum           # 30
```

Parameters may optionally specify [Sized Types](types.md):

```pith
fn multiply(x: i32, y: i32)
    return x * y
end
```

Inside the function body, parameters act as local variables.

---

## Returning Values

The `return` statement exits the function and passes a value back to the caller:

```pith
fn getAnswer
    return 42
end

val = getAnswer()
```

- A function can return early at any point.
- If execution reaches the end of a function without an explicit `return`, it implicitly returns `0`.
- The compiler statically detects when a function body always returns and notes unreachable exit paths.

---

## Recursion

Functions in Pith can call themselves recursively:

```pith
fn fib(n)
    if n <= 1
        return n
    end
    return fib(n - 1) + fib(n - 2)
end

print fib(10)       # 55
```

Each recursive call allocates a fresh native stack frame in machine memory with zero VM overhead.

---

## Direct Calls vs Native C Imports

- **User Functions**: Called directly by identifier name (`add(10, 20)` or `greet("Alice")`).
- **Imported C Functions**: Called via their module namespace (`math.addInts(3, 4)`). See [C Imports (FFI)](../ffi.md).

```pith
import "ffi/math.c"

fn doubleValue(x)
    return math.addInts(x, x)
end

print doubleValue(21)   # 42
```

---

## Zero-Bloat Dead Function Elimination

Pith enforces a strict **zero-bloat** principle. During whole-program compilation (WPSSAC), the compiler inspects which functions are referenced by the script's execution path.

If a private function is declared but never called, Pith eliminates it entirely from the generated QBE IR and emits an informational note:

```text
note: private function is never referenced; eliminated (zero-bloat)
  --> script.pi:2:1
   |
 2 | fn unusedHelper
   | ^~~~~~~~~~~~~~~
```

This ensures that binaries stay lean, fast, and completely free of unused dead code.

---

## See Also

- [Variables & Mutability](variables.md) — Scope rules for local variables and parameters
- [Types & Static Bounds](types.md) — Sized parameter types
- [C Imports (FFI)](../ffi.md) — Interfacing with native C functions
- [CLI Reference](../cli/repl.md) — Prototyping functions interactively in the REPL
