# Variables & Mutability

In Pith, variables are declared using the assignment operator `=`. Variables are **immutable by default** to eliminate accidental state mutations and ensure predictable program flow.

---

## Declaration vs Reassignment

A variable binding is introduced the first time a name appears on the left-hand side of `=`:

```pith
x = 10          # Immutable declaration
mut y = 20      # Mutable declaration
```

The compiler tracks identifiers through a lexical scope stack:
- If `name` does not exist in the current scope or any parent scope, `name = expr` is a **declaration**.
- If `name` already exists in the current or an enclosing scope, `name = expr` is a **reassignment**.

### Immutability by Default

Attempting to reassign an immutable variable produces a compile-time error:

```pith
x = 10
x = 20          # Compile error!
```

Diagnostics output:
```text
error: cannot assign twice to immutable variable `x` (declare with `mut` to reassign)
  --> script.pi:2:1
   |
 2 | x = 20
   | ^
```

See the [Diagnostics Catalog](diagnostics.md) for details on error messages.

---

## The `mut` Modifier

To allow a variable's value to change over time, declare it with the `mut` keyword:

```pith
mut counter = 0
counter = counter + 1
counter = counter * 2
print counter       # prints 2
```

`mut` applies directly to the binding. You can also combine `mut` with [Explicit Types](types.md):

```pith
mut index: u32 = 0
mut balance: f64 = 100.50
```

---

## Scoping & Block Lifetimes

Every block construct ([`if`](control-flow.md#conditionals-if--elseif--else--end), [`while`](control-flow.md#while-loops-while--end), and [`fn`](functions.md)) introduces a new lexical scope:

```pith
x = 100

if os.isLinux
    y = 200         # Declared inside the if-block
    print x + y     # 300 (x is accessible from parent scope)
end

# y is no longer accessible here
```

If an inner block introduces a variable with the same name as a parent variable:

```pith
x = 10
if 1
    x = 20          # Reassigns outer x if x is mut, or errors if x is immutable!
end
```

To create a new inner variable, blocks maintain isolation. When exiting a scope, all variables declared within that block are cleanly destroyed.

---

## Memory & ARC Lifecycle

For numeric values (integers and floats), variables live on the stack or in machine registers.

For string values, Pith uses **Deterministic ARC (Automatic Reference Counting)**:
1. When a string variable is initialized (`s = "hello"`), the variable takes ownership with a reference count of 1.
2. If assigned to another variable (`copy = s`), the runtime increments the reference count (`pith_rt_retain`).
3. When the variable goes out of scope (at `end`, `break`, `continue`, or `return`), the compiler automatically emits release code (`pith_rt_release`).
4. Reassigning a mutable string variable (`s = "new string"`) releases the previous value before storing the new one.

Read more about retain/release mechanics in the [Runtime & Memory Model](../runtime.md) documentation.

---

## See Also

- [Types & Static Bounds](types.md) — Sized integer and floating-point types
- [Control Flow](control-flow.md) — Scopes inside loops and conditionals
- [Runtime & Memory Model](../runtime.md) — Atomic reference counting and memory safety
