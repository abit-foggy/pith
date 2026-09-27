# Tutorial: Working with Data Types

Every computer program works with data—whether it's keeping track of a player's score, greeting a user by name, or checking if a setting is switched on or off.

In Pith, you don't have to wrestle with complicated type declarations. You just write your value, and Pith figures out the type automatically!

---

## 1. Numbers: Counting and Measuring

Pith handles both whole numbers and decimals naturally.

### Whole Numbers (Integers)
Use whole numbers for counting items, tracking scores, or measuring age:

```pith
score = 100
level = 1
items_collected = 5
temperature = -4    # Negative numbers work just as easily!
```

### Decimals (Floating-Point Numbers)
When you need to measure something with precision—like money or percentages—add a decimal point:

```pith
price = 19.99
multiplier = 1.5
pi = 3.14159
```

You can do math with numbers freely using `+`, `-`, `*`, and `/`:

```pith
total = price * 2
print "Total cost: " + total
```

---

## 2. Text (Strings)

Text in Pith is called a **string**. You write text by enclosing words between double quotes `"..."`:

```pith
player_name = "Alex"
message = "Welcome to the game!"
```

### Joining Text Together
You can glue pieces of text together using the `+` symbol:

```pith
greeting = "Hello, " + player_name + "!"
print greeting    # prints "Hello, Alex!"
```

You can even add numbers directly into text messages:

```pith
score = 250
print "Your score is: " + score   # prints "Your score is: 250"
```

---

## 3. True & False (Booleans)

Sometimes your program needs to know if a statement is true or false. In Pith, decisions evaluate to:
- `1` for **True** (yes, active, enabled)
- `0` for **False** (no, inactive, disabled)

```pith
is_logged_in = 1
has_key = 0

if is_logged_in
    print "Welcome back!"
end

if not has_key
    print "The door is locked."
end
```

Conditions like `score > 50` or `name == "Alex"` automatically produce `1` or `0`.

---

## 4. Power Feature: Exact Sizes (When You Need Extra Control)

One of Pith's greatest strengths is that it's as easy as Python, but gives you the low-level power of C or Rust whenever you want it.

If you are building games, networking tools, or high-performance apps and want to limit a number to an exact size, you can add an optional type with a colon `:`:

```pith
# u8 means "unsigned 8-bit integer": holds whole numbers from 0 to 255
byte_value: u8 = 200

# i32 means standard 32-bit integer: holds numbers up to 2 billion
large_count: i32 = 1000000
```

### Popular Sizes at a Glance

| Size | Values Allowed | Ideal For |
|---|---|---|
| `u8` | `0` to `255` | Bytes, colors (Red/Green/Blue), small counters |
| `i8` | `-128` to `127` | Small signed values |
| `u16` | `0` to `65,535` | Network ports, game inventory slots |
| `i32` | Over 2 billion | High scores, large item counts |
| `f64` | Decimals | Scientific precision and 3D game coordinates |

### Automatic Safety Checks
If you use an exact size, Pith protects you from putting the wrong value into it before your program even runs:

```pith
score: u8 = 500     # Error: 500 is too large for u8 (max is 255)
```

---

## Next Steps

Now that you know what kinds of values you can work with:

- **[Math & Logic](expressions.md)** — Learn how to add, compare, and check conditions
- **[If Statements & Loops](control-flow.md)** — Guide how your program makes choices
