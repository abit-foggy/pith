# If Statements & Loops

Control flow lets your program make choices and repeat actions.

---

## Making Decisions with `if`

An `if` statement runs code only when a condition is met. The block finishes with `end`:

```pith
score = 100

if score >= 100
    print "You win!"
end
```

### Adding `else` and `elseif`

You can handle other cases with `elseif` and a fallback `else`:

```pith
score = 75

if score >= 90
    print "Grade: A"
elseif score >= 70
    print "Grade: B"
elseif score >= 50
    print "Grade: C"
else
    print "Need to study more!"
end
```

Notice:
- No parentheses `()` needed around the conditions.
- No colons `:` at the end of the lines.
- One single `end` closes the entire chain.

---

## Repeating Code with `while`

A `while` loop repeats a block of code as long as its condition stays true:

```pith
mut count = 1

while count <= 5
    print "Count: " + count
    count = count + 1
end

print "Finished!"
```

Output:
```text
Count: 1
Count: 2
Count: 3
Count: 4
Count: 5
Finished!
```

---

## Controlling Loops: `break` and `continue`

You can control a loop from the inside:

### 1. `break` (Stop the loop right now)
Use `break` to exit a loop immediately:

```pith
mut n = 1
while n <= 10
    if n == 4
        print "Found 4! Stopping loop."
        break
    end
    print n
    n = n + 1
end
```

### 2. `continue` (Skip to the next step)
Use `continue` to skip the rest of the current turn and jump straight to the next check:

```pith
mut n = 0
while n < 5
    n = n + 1
    if n == 3
        # Skip printing 3
        continue
    end
    print n
end
```

Output:
```text
1
2
4
5
```

---

## See Also

- [Math & Logic](expressions.md) — Checking conditions with `and`, `or`, and `not`
- [Variables & Mutability](variables.md) — Using `mut` to update loop counters
