# Common Errors & How to Fix Them

When something in your code needs attention, Pith points directly to the line and explains the problem in plain English.

```text
error: cannot assign twice to immutable variable `score` (declare with `mut` to reassign)
  --> game.pi:3:1
   |
 3 | score = 20
   | ^
```

Here are the most common errors and how to solve them:

---

## 1. Changing a Variable Without `mut`

```text
error: cannot assign twice to immutable variable `x` (declare with `mut` to reassign)
```

- **What it means**: You created `x = 10` and later tried to change it to `x = 20`.
- **How to fix it**: Add `mut` when you first create the variable:
  ```pith
  mut x = 10
  x = 20    # Works!
  ```

---

## 2. Using `break` or `continue` Outside a Loop

```text
error: `break` outside of a loop
error: `continue` outside of a loop
```

- **What it means**: `break` and `continue` only make sense inside a loop.
- **How to fix it**: Place `break` or `continue` inside a `while ... end` loop.

---

## 3. Using a Variable Before Creating It

```text
error: use of undeclared identifier `name`
```

- **What it means**: Pith doesn't recognize `name`.
- **How to fix it**: Make sure you created the variable with `name = "..."` earlier in the code, or check for typos.

---

## 4. Multiple Commands on One Line

```text
error: expected end of line between statements
```

- **What it means**: You wrote two commands on the same line without pressing Enter.
- **How to fix it**: Put each command on its own line:
  ```pith
  # Instead of: x = 10 y = 20
  x = 10
  y = 20
  ```

---

## 5. Number Too Big for Sized Type

```text
error: literal is out of range for type u8 (maximum value is 255)
```

- **What it means**: You gave a number larger than the maximum allowed by that type (like `val: u8 = 300`).
- **How to fix it**: Use a smaller number, or remove the `: u8` to let Pith handle the size automatically.

---

## Helpful Notes

You may also see green or blue notes in the terminal:

- **`note: private function is never referenced; eliminated`**: Pith noticed a function you wrote was never called, so it left it out to keep your program lean and fast.
- **`note: unreachable exit path`**: A function returns early before reaching subsequent lines.
