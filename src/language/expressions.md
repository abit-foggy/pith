# Tutorial: Math, Comparisons & Logic

Computers are world-class calculators. In this tutorial, you'll learn how to do calculations, compare values, and combine conditions using simple, everyday English words like `and`, `or`, and `not`.

---

## Step 1: Doing Math

Pith handles regular arithmetic just like you'd write it on paper:

```pith
# 1. Addition and Subtraction
items = 10 + 5       # 15
remaining = 20 - 8   # 12

# 2. Multiplication and Division
area = 5 * 10        # 50
half = 100 / 2       # 50

# 3. Negative numbers
change = -15
```

### Controlling Order with Parentheses
Just like in school math, multiplication and division happen before addition and subtraction. If you want addition to happen first, wrap it in parentheses `( )`:

```pith
# Without parentheses: 3 * 4 = 12, then 2 + 12 = 14
result1 = 2 + 3 * 4

# With parentheses: (2 + 3) = 5, then 5 * 4 = 20
result2 = (2 + 3) * 4

print "Result 1: " + result1
print "Result 2: " + result2
```

---

## Step 2: Combining Text with `+`

The `+` symbol is extra handy in Pith: it also glues text together!

```pith
first_name = "Alex"
message = "Welcome, " + first_name + "! Ready to code?"
print message
```

Output:
```text
Welcome, Alex! Ready to code?
```

---

## Step 3: Comparing Values

Whenever your program needs to check if two things match or if one number is bigger than another, use comparison symbols. Comparisons always evaluate to `1` (true) or `0` (false).

| Symbol | What It Checks | Example | Meaning |
|---|---|---|---|
| `==` | Exactly equal to | `score == 100` | Is the score exactly 100? |
| `!=` | Not equal to | `items != 0` | Are items different from 0? |
| `<` | Less than | `age < 18` | Is age under 18? |
| `<=` | Less than or equal | `level <= 5` | Is level 5 or lower? |
| `>` | Greater than | `score > 50` | Is score higher than 50? |
| `>=` | Greater than or equal | `coins >= 10` | Do you have 10 or more coins? |

### Comparing Text
You can also compare text directly with `==` and `!=`:

```pith
user_role = "admin"

if user_role == "admin"
    print "Welcome to the dashboard!"
end
```

---

## Step 4: Logic with `and`, `or`, and `not`

In some other languages, you have to remember cryptic symbols like `&&`, `||`, and `!`. In Pith, you write the exact English words you're thinking!

### 1. `and`: Both Must Be True
Use `and` when both requirements have to be met:

```pith
has_ticket = 1
is_open = 1

if has_ticket and is_open
    print "You may enter the concert!"
end
```

### 2. `or`: At Least One Must Be True
Use `or` when having either option is good enough:

```pith
is_weekend = 1
on_holiday = 0

if is_weekend or on_holiday
    print "Time to relax!"
end
```

### 3. `not`: Flip True to False (or False to True)
Use `not` when you want to check if something is *not* the case:

```pith
is_raining = 0

if not is_raining
    print "Let's go for a walk outside!"
end
```

### Combining Everything Naturally
You can combine them easily using parentheses:

```pith
has_id = 1
is_vip = 0
is_banned = 0

if (has_id or is_vip) and not is_banned
    print "Entry approved!"
end
```

---

## Next Steps

Now that you can calculate and compare values:

- **[If Statements & Loops](control-flow.md)**: Put your comparisons to work in real programs
- **[Functions](functions.md)**: Package calculations into reusable actions
