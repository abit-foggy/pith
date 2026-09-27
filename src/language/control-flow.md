# Tutorial: Decisions & Loops

Programs become truly powerful when they can make choices on their own and repeat actions automatically. 

In this tutorial, you'll learn how to guide your code using `if` statements and repeat work using `while` loops.

---

## Step 1: Making Decisions with `if`

An `if` statement tells your computer: *"Only run this code if a specific condition is true."*

Let's test if a player has reached a winning score:

```pith
score = 100

if score >= 100
    print "Congratulations, you won!"
end
```

Notice how clean the syntax is:
- No parentheses `()` required around `score >= 100`
- No colon `:` at the end of the line
- Just write your code, and close it with `end`

---

## Step 2: Handling Alternatives with `else` and `elseif`

What if the condition isn't met? You can provide a fallback response with `else`:

```pith
score = 45

if score >= 50
    print "You passed the test!"
else
    print "Keep practicing, you can do it!"
end
```

If you have multiple options to check (like assigning grades), chain them together using `elseif`:

```pith
score = 85

if score >= 90
    print "Grade: A"
elseif score >= 80
    print "Grade: B"
elseif score >= 70
    print "Grade: C"
else
    print "Grade: Needs improvement"
end
```

Pith checks each condition in order from top to bottom. As soon as one matches, it runs that code and moves on!

---

## Step 3: Repeating Actions with `while`

What if you want to print a countdown or repeat an action 10 times? Instead of copying and pasting your code, use a `while` loop!

A `while` loop keeps running as long as its condition stays true:

```pith
# Remember: we use `mut` because `count` is going to change!
mut count = 1

while count <= 5
    print "Turn: " + count
    count = count + 1
end

print "All turns completed!"
```

Output:
```text
Turn: 1
Turn: 2
Turn: 3
Turn: 4
Turn: 5
All turns completed!
```

---

## Step 4: Special Loop Controls (`break` and `continue`)

Sometimes you want to break out of a loop early, or skip one specific round. Pith gives you two simple commands:

### 1. `break`: Stop the Loop Immediately
Use `break` when you've found what you were looking for and don't need to keep searching:

```pith
mut number = 1

while number <= 10
    if number == 4
        print "Found number 4! Stopping early."
        break
    end
    print "Checking: " + number
    number = number + 1
end
```

Output:
```text
Checking: 1
Checking: 2
Checking: 3
Found number 4! Stopping early.
```

### 2. `continue`: Skip to the Next Round
Use `continue` when you want to skip the rest of the current turn and jump straight to the next one:

```pith
mut number = 0

while number < 5
    number = number + 1
    if number == 3
        # Skip number 3!
        continue
    end
    print "Processing item: " + number
end
```

Output:
```text
Processing item: 1
Processing item: 2
Processing item: 4
Processing item: 5
```

Notice that `Processing item: 3` was skipped completely!

---

## Next Steps

Now that you can guide your code's decisions and repeat actions:

- **[Functions](functions.md)** — Learn how to bundle your code into reusable actions
- **[Math & Logic](expressions.md)** — Combine conditions with `and`, `or`, and `not`
