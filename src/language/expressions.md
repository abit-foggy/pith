# Expressions & Operators

An expression in Pith computes a value. Expressions can be literals, variable references, operator evaluations, member accesses, or function calls.

---

## Operator Precedence

Operators in Pith follow standard mathematical precedence, evaluated from lowest to highest:

| Level | Operator | Associativity | Description |
|---|---|---|---|
| 1 (Lowest) | `or` | Left-to-right | Short-circuit logical OR |
| 2 | `and` | Left-to-right | Short-circuit logical AND |
| 3 | `==` `!=` | Left-to-right | Equality and inequality |
| 4 | `<` `<=` `>` `>=` | Left-to-right | Relational comparisons |
| 5 | `+` `-` | Left-to-right | Addition, subtraction, string concatenation |
| 6 | `*` `/` | Left-to-right | Multiplication and division |
| 7 | `-` `not` | Right-to-left | Unary negation, logical NOT |
| 8 (Highest) | `.` `(...)` | Left-to-right | Member access, function call |

Parentheses `(` and `)` can be used to override default precedence:

```pith
val = (2 + 3) * 4      # 20
```

---

## Logical Operators (`and`, `or`, `not`)

Pith provides English-word logical operators that are dead-simple to read:

- **`and`**: Returns true if both operands are truthy.
- **`or`**: Returns true if either operand is truthy.
- **`not`**: Inverts the truth value of an expression.

```pith
if os.isLinux and not os.isNT
    print "Running on Linux"
end

if port == 80 or port == 443
    print "Standard HTTP/HTTPS port"
end
```

### Short-Circuit Evaluation

Logical expressions in Pith are strictly **short-circuiting**:
- In `a and b`, if `a` evaluates to false (`0`), `b` is never evaluated.
- In `a or b`, if `a` evaluates to true (non-zero), `b` is never evaluated.

This guarantees safety when guarding operations:

```pith
if fd >= 0 and net.send(fd, "ping") > 0
    print "Sent successfully"
end
```

---

## Arithmetic Operators

Pith supports integer and floating-point arithmetic:

- `+` (Addition / String Concatenation)
- `-` (Subtraction)
- `*` (Multiplication)
- `/` (Division)
- `-` (Unary negation)

```pith
sum = 10 + 25       # 35
diff = 100 - 35     # 65
prod = 6 * 7        # 42
quot = 100 / 4      # 25
neg = -sum          # -35
```

### Type Promotion & Division

- Mixed integer and float operations automatically promote the integer to a float (`f64`).
- Integer division truncates toward zero, matching C99 standards.
- Division by zero generates a deterministic `SIGFPE` hardware exception.

### String Concatenation

The `+` operator also concatenates strings:

```pith
greeting = "Hello, " + "World!"
print greeting      # prints "Hello, World!"
```

When concatenating strings, a new ARC string buffer is allocated, and temporary buffers are automatically released.

---

## Relational & Equality Operators

Comparisons evaluate to `1` (true) or `0` (false):

- `==` (Equal to)
- `!=` (Not equal to)
- `<` (Less than)
- `<=` (Less than or equal to)
- `>` (Greater than)
- `>=` (Greater than or equal to)

```pith
if count >= 10
    print "Limit reached"
end
```

### String Comparison

Comparing two strings with `==` or `!=` compares their content byte-by-byte:

```pith
if os.identifyKernel == "linux"
    print "Linux kernel detected"
end
```

---

## Member Access & Calls

- **Member Access (`.`)**: Accesses namespaced functions and properties (e.g. `os.isLinux`, `net.connect`).
- **Function Call (`(...)`)**: Invokes a user-defined function or an imported C function.

```pith
# Direct function call
total = add(10, 20)

# Namespaced member call
fd = net.connect("127.0.0.1", 8080)
```

---

## See Also

- [Types & Static Bounds](types.md) — Numeric types and promotion rules
- [Control Flow](control-flow.md) — Using conditions in `if` and `while`
- [Functions & Calls](functions.md) — Declaring and invoking functions
