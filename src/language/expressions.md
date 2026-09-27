# Math & Logic

In Pith, you can do math, compare values, and combine conditions using simple, clear words.

---

## Basic Math

Pith supports standard math operations:

| Operator | What it does | Example | Result |
|---|---|---|---|
| `+` | Add | `10 + 5` | `15` |
| `-` | Subtract | `20 - 8` | `12` |
| `*` | Multiply | `4 * 7` | `28` |
| `/` | Divide | `100 / 4` | `25` |
| `-` | Negative | `-score` | Negates a number |

### Using Parentheses
You can use parentheses `(` and `)` to choose which math happens first:

```pith
result = (2 + 3) * 4   # result is 20, because (2 + 3) happens first
```

### Combining Text with `+`
The `+` sign also joins pieces of text together:

```pith
first_name = "Alex"
full_greeting = "Hello, " + first_name + "!"
print full_greeting     # prints "Hello, Alex!"
```

---

## Comparing Values

Comparisons check relationships between values and return `1` (true) or `0` (false):

| Operator | Meaning | Example |
|---|---|---|
| `==` | Equal to | `score == 100` |
| `!=` | Not equal to | `score != 0` |
| `<` | Less than | `age < 18` |
| `<=` | Less than or equal to | `age <= 21` |
| `>` | Greater than | `health > 0` |
| `>=` | Greater than or equal to | `coins >= 50` |

You can also compare text directly:

```pith
if os.identifyKernel == "linux"
    print "Running on Linux"
end
```

---

## Logic: `and`, `or`, `not`

Pith uses plain English words instead of symbols like `&&` or `||`:

### 1. `and` (Both must be true)
```pith
if has_key and door_unlocked
    print "You may enter!"
end
```

### 2. `or` (At least one must be true)
```pith
if is_saturday or is_sunday
    print "It's the weekend!"
end
```

### 3. `not` (Inverts true / false)
```pith
if not game_over
    print "Keep playing!"
end
```

You can combine them easily:

```pith
if (is_admin or has_pass) and not banned
    print "Access granted"
end
```

---

## See Also

- [If Statements & Loops](control-flow.md) — Using conditions to control code
- [Functions](functions.md) — Reusable blocks of code
