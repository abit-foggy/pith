# Variables & Mutability

Variables let you store information so you can use it later in your program.

---

## Creating a Variable

To create a variable, write its name, followed by `=`, and then the value:

```pith
name = "Alex"
age = 20
print name
print age
```

---

## Immutable by Default (Preventing Bugs)

In Pith, variables **do not change by default**. This is called *immutability*, and it prevents accidental bugs:

```pith
score = 10
score = 20      # Error! Pith protects you from accidentally changing score.
```

If you try to change an immutable variable, Pith tells you clearly:

```text
error: cannot assign twice to immutable variable `score` (declare with `mut` to reassign)
```

---

## Making a Variable Changeable with `mut`

If you *want* a variable's value to change (like a counter or a game score), add the `mut` word (short for *mutable*):

```pith
mut score = 0
score = 10
score = score + 5
print score     # prints 15
```

---

## Where Variables Live (Scope)

Variables created inside a block (such as an `if` statement, loop, or function) only exist inside that block:

```pith
if 1
    inside = "I only exist in here"
    print inside
end

# `inside` is no longer available out here
```

Variables from the outside can still be read on the inside:

```pith
greeting = "Hello"

if 1
    print greeting   # Works! Prints "Hello"
end
```

---

## Automatic Cleanup

You never have to worry about managing memory, pointers, or garbage collection in Pith. When a variable is no longer needed, Pith cleans it up automatically and instantly with zero performance lag.

---

## See Also

- [Types of Data](types.md) — Numbers, text, and true/false values
- [If Statements & Loops](control-flow.md) — Controlling when code runs
- [Error Guide](diagnostics.md) — What to do if you see an error
