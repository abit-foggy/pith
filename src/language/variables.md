# Variables & Mutability

Think of a variable as a labeled storage box in your computer's memory. You give it a name, put some data inside, and use it whenever you need it.

---

## Step 1: Creating Your First Variable

To create a variable, write its name, followed by an equals sign `=`, and the value you want to store:

```pith
name = "Alex"
score = 10

print "Player: " + name
print "Score: " + score
```

When you run this, Pith prints:
```text
Player: Alex
Score: 10
```

Notice how easy that was? You didn't have to declare complex types, write semicolons, or wrap anything in brackets. Pith takes care of the details so you can focus on building your idea.

---

## Step 2: Understanding Why Variables Stay Safe by Default

Imagine you're writing a game. You set a player's score at the start:

```pith
score = 10
```

Later on in your code, you accidentally try to overwrite it without realizing it:

```pith
score = 10
score = 20      # Oops! Did you mean to change it, or was it an accident?
```

In many other languages, this bug slips through quietly and breaks your program later. But Pith protects you right away! If you run this code, Pith gives you a friendly, clear message:

```text
error: cannot assign twice to immutable variable `score` (declare with `mut` to reassign)
```

By default, every variable in Pith is **immutable** (meaning *unchangeable*). This simple rule prevents common bugs before they ever happen.

---

## Step 3: Making Variables Changeable with `mut`

What if you *do* want a variable to change (like when a player scores points, or when you are counting items in a loop)?

Just put the word `mut` (short for *mutable*, or changeable) in front of the variable name when you first create it.

Look at how the exact same variable `score` works now:

```pith
# Notice the only change: we added `mut` at the beginning!
mut score = 10
score = 20          # Works perfectly! The score is now 20.
score = score + 5   # We can keep updating it as much as we want!

print "Final score: " + score
```

Output:
```text
Final score: 25
```

### Side-by-Side Comparison

Look at both examples together:

| Without `mut` | With `mut` |
|---|---|
| ```pith<br>score = 10<br>score = 20 # Error! Protected from changes<br>``` | ```pith<br>mut score = 10<br>score = 20 # Allowed! Can change freely<br>``` |

Both examples use the exact same variable `score`. Adding `mut` is simply your way of telling Pith: *"I plan on updating this value later."*

---

## Step 4: Where Variables Live (Scope)

Variables belong to the block of code where you created them. For example, if you create a variable inside an `if` block, it only lives inside that block:

```pith
if 1
    secret = "Top secret message"
    print secret     # Works!
end

# Once the block ends, `secret` disappears to keep memory clean.
```

However, variables created outside a block can easily be read inside it:

```pith
welcome = "Welcome back, Alex!"

if 1
    print welcome    # Works! Reads the variable from outside.
end
```

---

## Fast & Automatic Cleanup

You never have to manage memory, free pointers, or wait on a sluggish garbage collector. When your variables finish their job, Pith cleans them up instantly behind the scenes. You get the simplicity of a beginner-friendly scripting language with the blazing speed of native code.

---

## Next Steps

Now that you know how to save and change information, let's explore what kinds of data you can store:

- **[Types of Data](types.md)**: Numbers, text, and true/false conditions
- **[Math & Logic](expressions.md)**: Adding numbers, comparing values, and logic
- **[If Statements & Loops](control-flow.md)**: Making choices and repeating code
