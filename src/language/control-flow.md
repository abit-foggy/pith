# Control Flow

Pith uses clean, bracketless control flow constructs. No parentheses or colons are required. Every block is closed with a single `end` keyword.

---

## Conditionals (`if` / `elseif` / `else` / `end`)

Conditionals branch execution based on whether an expression evaluates to truthy (non-zero) or falsey (`0`):

```pith
if os.isLinux
    print "Running on Linux"
elseif os.isNT
    print "Running on Windows NT"
elseif os.isDarwin
    print "Running on macOS / Darwin"
else
    print "Other operating system"
end
```

### Syntax Rules

1. **No Parentheses**: You do not need to wrap conditions in parentheses `( )`, though they are permitted if desired.
2. **Newline Delimited**: A condition is terminated by a newline.
3. **Closing `end`**: The entire `if` chain closes with a single `end`.
4. **Nesting**: Blocks may nest up to 256 levels and can be left empty without error.

---

## While Loops (`while` / `end`)

Loops repeat a block of statements as long as the condition evaluates to truthy:

```pith
mut i = 0
while i < 10
    print "count: " + i
    i = i + 1
end
```

---

## Loop Controls (`break` and `continue`)

Within a `while` loop, you can control the iteration flow using `break` and `continue`:

- **`break`**: Immediately terminates the innermost loop and jumps to the statement following `end`.
- **`continue`**: Immediately stops the current iteration and jumps to the condition check for the next iteration.

```pith
mut n = 0
while n < 10
    n = n + 1
    
    if n == 5
        continue    # Skip printing 5
    end
    
    if n == 8
        break       # Stop the loop at 8
    end
    
    print n
end
```

### Compile-Time Loop Guards

Using `break` or `continue` outside of an enclosing `while` loop is rejected at compile time:

```text
error: `break` outside of a loop
  --> script.pi:5:5
   |
 5 |     break
   |     ^^^^^
```

---

## Deterministic ARC Inside Loops

In many languages with automatic memory management, creating strings or allocations inside tight loops requires waiting for a garbage collector or creating manual autorelease pools.

In Pith, **Deterministic ARC** guarantees that all local string allocations inside the loop body are cleanly released:
- At the end of each iteration before re-evaluating the condition.
- Immediately before executing a `continue` jump.
- Immediately before executing a `break` jump.

```pith
while active
    msg = net.recv(fd, 1024)   # msg allocated
    process(msg)
    # msg automatically released at iteration boundary — zero memory creep!
end
```

Read more about retain and release invariants in the [Runtime & Memory Model](../runtime.md) guide.

---

## See Also

- [Expressions & Operators](expressions.md) — Boolean logic and relational operators
- [Variables & Mutability](variables.md) — Mutable loops and scoped bindings
- [Runtime & Memory Model](../runtime.md) — ARC lifecycle at scope exits
