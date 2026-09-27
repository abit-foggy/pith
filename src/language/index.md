# Language Reference

Pith is designed to be simple and natural to read. 

There are no curly braces `{}` and no semicolons `;`. Each line is its own statement, and code blocks simply finish with the word `end`.

---

## Quick Cheat Sheet

| What you want to do | Example | Description |
|---|---|---|
| Create a variable | `name = "Alex"` | Stores a value that won't change |
| Create a changeable variable | `mut score = 0` | Stores a value you can change later |
| Print something | `print "Hello!"` | Shows text or numbers in the terminal |
| If / else decision | `if score > 10 ... end` | Runs code only when something is true |
| Repeat code | `while score < 10 ... end` | Repeats code in a loop |
| Break out of a loop | `break` | Stops a loop immediately |
| Skip to next loop step | `continue` | Jumps to the next turn of the loop |
| Check two things | `if logged_in and is_admin` | True only if both conditions are true |
| Check either thing | `if is_weekend or on_vacation` | True if at least one condition is true |
| Invert a condition | `if not finished` | True if finished is false |
| Create a function | `fn add(a, b) ... end` | Reusable block of code |
| Return from function | `return a + b` | Sends a value back to whoever called it |

---

## The Basic Rules

### 1. One Statement Per Line
You don't need semicolons. Just write one command per line:

```pith
x = 10
y = 20
print x + y
```

### 2. Comments Start with `#`
Use `#` to write notes in your code that the computer ignores:

```pith
# This is a helpful comment
score = 100  # You can also add comments at the end of a line
```

### 3. Strings Use Double Quotes
Text is written inside double quotes:

```pith
message = "Hello, World!"
```

To include special characters:
- `\n` creates a new line
- `\t` creates a tab space
- `\"` includes a quote inside the text

---

## Detailed Chapters

- **[Variables & Mutability](variables.md)** — Storing values with `=` and changing them with `mut`.
- **[Types of Data](types.md)** — Working with numbers, text, and true/false values.
- **[Math & Logic](expressions.md)** — Adding numbers, comparing values, and using `and`, `or`, `not`.
- **[If Statements & Loops](control-flow.md)** — Making decisions with `if` and repeating code with `while`.
- **[Functions](functions.md)** — Grouping code into reusable actions with `fn`.
- **[Error Guide](diagnostics.md)** — How to read and fix common mistakes.
