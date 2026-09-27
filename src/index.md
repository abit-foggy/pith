# Welcome to Pith

**Pith** is a programming language designed to be dead simple to read and write.

If you are new to programming, Pith is built for you: no curly braces `{}`, no semicolons `;`, and no confusing syntax. If you already know other languages, Pith gives you speed and simplicity without getting in your way.

```pith
# A simple Pith program
name = "World"
print "Hello, " + name

mut count = 1
while count <= 3
    print "Count: " + count
    count = count + 1
end
```

---

## Why Pith?

- **Easy to Read**: Blocks close with a simple `end`. No braces, no semicolons, no clutter.
- **Fast & Lightweight**: Pith compiles directly to fast machine code. There is no heavy virtual machine or garbage collector slowing things down.
- **Helpful Errors**: When something goes wrong, Pith shows you exactly where the error is and how to fix it in plain English.
- **Instant Testing**: Run scripts instantly with `pith run`, or experiment live in the terminal using `pith repl`.

---

## Getting Started

1. **[Installation](getting-started/installation.md)** — Download and install Pith in one command.
2. **[Your First Script](getting-started/first-script.md)** — Write and run your first Pith program.
3. **[Language Reference](language/index.md)** — Learn how variables, loops, and functions work.
4. **[Built-in Tools](namespaces.md)** — Work with files, system info, and networking.
