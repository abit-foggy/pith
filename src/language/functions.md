# Functions

Functions let you group code into a reusable action so you don't have to repeat yourself.

---

## Creating a Function

Use `fn`, followed by the function name, any inputs inside `( )`, and end the block with `end`:

```pith
fn greet(name)
    print "Hello, " + name + "!"
end

# Calling the function:
greet("Alex")
greet("Sam")
```

Output:
```text
Hello, Alex!
Hello, Sam!
```

---

## Returning Values with `return`

A function can calculate something and send the answer back using `return`:

```pith
fn add(a, b)
    return a + b
end

total = add(10, 20)
print total     # prints 30
```

You can also return early from inside an `if` statement:

```pith
fn check_age(age)
    if age < 18
        return "Minor"
    end
    return "Adult"
end

print check_age(21)     # prints "Adult"
```

---

## Functions Calling Themselves (Recursion)

A function can call itself to break down a bigger problem:

```pith
fn countdown(n)
    if n <= 0
        print "Blast off!"
        return 0
    end
    print n
    return countdown(n - 1)
end

countdown(3)
```

Output:
```text
3
2
1
Blast off!
```

---

## Clean & Fast

If you write a function but never end up using it, Pith automatically skips it when building your program so it never slows your app down or wastes space.

---

## See Also

- [Variables & Mutability](variables.md) — How variables work inside functions
- [If Statements & Loops](control-flow.md) — Using conditions inside functions
