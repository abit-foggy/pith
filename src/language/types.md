# Types of Data

In Pith, you usually don't need to worry about types—Pith figures them out for you automatically.

```pith
count = 42          # A whole number
price = 9.99        # A decimal number
name = "Alex"       # Text (string)
is_ready = 1        # True / False
```

---

## The Main Types

### 1. Whole Numbers (Integers)
Numbers without a decimal point:

```pith
players = 4
temperature = -12
score = 1000000
```

### 2. Decimals (Floating-point)
Numbers with a decimal point:

```pith
pi = 3.14159
percentage = 0.75
```

### 3. Text (Strings)
Words and text are placed inside double quotes. You can combine text with `+`:

```pith
first = "Hello, "
second = "World!"
print first + second    # prints "Hello, World!"
```

### 4. True & False (Booleans)
Conditions evaluate to `1` (true) or `0` (false):

```pith
has_key = 1
is_open = 0

if has_key
    print "You unlocked the door!"
end
```

---

## Optional: Sized Types (Advanced)

If you are writing games or systems code and want to save memory or restrict a number to an exact range, you can add an optional type with `:`:

```pith
age: u8 = 25            # u8 holds numbers from 0 to 255
high_score: i32 = 50000 # i32 holds standard large numbers
```

### Common Sized Types

| Type | Range | Good For |
|---|---|---|
| `u8` | `0` to `255` | Byte data, RGB colors, small counters |
| `i8` | `-128` to `127` | Small signed values |
| `u16` | `0` to `65,535` | Port numbers, medium positive numbers |
| `i16` | `-32,768` to `32,767` | Medium numbers |
| `u32` / `i32` | Over 2 billion | Standard large integers |
| `f32` / `f64` | Decimals | Scientific or game math |

### Compile-Time Safety
If you set a sized type and accidentally give it a number that is too large or negative, Pith catches it before your program even runs:

```pith
level: u8 = 300     # Error: 300 is too large for u8 (max is 255)
```

---

## See Also

- [Variables & Mutability](variables.md) — Storing values
- [Math & Logic](expressions.md) — Doing math with numbers
