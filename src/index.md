# Welcome to Pith

> **pith** /pɪθ/ *noun*
>
> 1. The essential substance or central core of a matter; the heart.
> 2. Vigorous, concise, and pointed expression.

A minimal, zero-dependency systems-scripting language built for deterministic execution and structural clarity.

---

**Pith** is designed to be dead simple to read and write, giving you the ease of a beginner-friendly scripting language with the raw speed and power of native machine code.

If you are new to programming, Pith is built for you: no curly braces `{}`, no semicolons `;`, and no confusing boilerplate. If you already know other languages, Pith gives you speed and simplicity without getting in your way.

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
- **Standalone Binaries**: Turn any script into a standalone native executable with `pith build`.

---

## Getting Started

1. **[Installation](getting-started/installation.md)**: Download and install Pith in one command.
2. **[Your First Script](getting-started/first-script.md)**: Follow our step-by-step beginner tutorial.
3. **[Language Reference](language/index.md)**: Learn how variables, loops, and functions work.
4. **[Built-in Tools](namespaces.md)**: Work with files (`fs`), system info (`os`), and networking (`net`).
