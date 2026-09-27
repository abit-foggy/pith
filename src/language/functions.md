# Tutorial: Reusable Functions

As your programs get bigger, you'll often want to perform the same action in several different places. Instead of copying and pasting code, you can group it into a **function**!

Think of a function like a recipe or an action button: you define what it does once, and then you can trigger it whenever you need it.

---

## Step 1: Creating Your First Function

In Pith, you create a function with the `fn` keyword, followed by the name you choose, parentheses `( )` for any inputs, and finish with `end`:

```pith
fn greet(name)
    print "Hello, " + name + "! Welcome to Pith."
end

# Now call your function whenever you want:
greet("Alex")
greet("Jordan")
```

Output:
```text
Hello, Alex! Welcome to Pith.
Hello, Jordan! Welcome to Pith.
```

---

## Step 2: Sending Back an Answer with `return`

Sometimes you want a function to calculate a value and hand it back to you. You do this using `return`:

```pith
fn add(first, second)
    return first + second
end

# Store the result in a variable:
total = add(15, 25)
print "The sum is: " + total
```

Output:
```text
The sum is: 40
```

---

## Step 3: Making Decisions Inside Functions

You can use `if` statements inside your function to return different results depending on the input:

```pith
fn check_pass(score)
    if score >= 70
        return "Passed"
    end
    return "Needs Retest"
end

print check_pass(85)   # prints "Passed"
print check_pass(55)   # prints "Needs Retest"
```

Notice that as soon as Pith hits a `return`, it immediately exits the function with the answer and skips any lines below it.

---

## Step 4: Functions That Call Themselves (Recursion)

A function can even call itself to solve a countdown or repetitive task!

```pith
fn blastoff(seconds)
    if seconds <= 0
        print "Blast off! 🚀"
        return 0
    end

    print seconds
    return blastoff(seconds - 1)
end

blastoff(3)
```

Output:
```text
3
2
1
Blast off! 🚀
```

---

## High Performance & Zero Waste

In Pith, functions run at native machine speed. Even better: if you write helper functions in your project that you don't end up using, Pith's compiler automatically removes them when building your final application. Your executable stays as small, lean, and fast as possible with zero bloat!

---

## Next Steps

Now you have mastered the core foundations of Pith!

- **[Working with Built-in Tools](../namespaces.md)**: Learn how to read and write files (`fs`), inspect the system (`os`), and connect to networks (`net`)
- **[Setting Up a Project](../getting-started/project-setup.md)**: Organize code into multiple files with `pith.toml`
