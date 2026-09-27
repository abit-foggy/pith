# pith

> **pith** /pɪθ/ *noun*
>
> The essential substance or central core of a matter; the heart.
>
> Vigorous, concise, and pointed expression.

A minimal, zero-dependency systems-scripting language built for deterministic execution and structural clarity.

[![CI](https://github.com/abit-foggy/pith/actions/workflows/ci.yml/badge.svg)](https://github.com/abit-foggy/pith/actions)
[![Documentation](https://img.shields.io/badge/docs-mdBook-emerald.svg)](https://abit-foggy.github.io/pith/)
[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)

---

## Highlights

- **Zero Runtime Dependencies**: Both QBE (SSA intermediate compiler) and TinyCC (ELF linker and memory execution engine) are embedded in-process (`libqbe` and `libtcc`). No external compiler or backend binary is required in `PATH`.
- **Deterministic ARC Memory**: Deterministic Automatic Reference Counting for strings and heap objects with compiler-injected release points at scope exits. No garbage collection pauses.
- **Dual Pipeline**: Run scripts instantly via in-memory JIT execution (`pith run`), or emit standalone native binaries (`pith build`).
- **Minimal, Expressive Syntax**: Clean indentation-free block structure (`if ... end`, `while ... end`, `fn ... end`) with strong static types (`i8` through `u64`, `f32`, `f64`, `bool`, `string`) and immutability by default (`mut` for reassignable bindings).
- **Batteries Included**: Built-in, zero-dependency namespaces for filesystem operations (`fs`), process management (`proc`), platform introspection (`os`), and TCP networking (`net`).

---

## Installation

### One-line installer (Linux / macOS / BSD)

```bash
curl -fsSL https://raw.githubusercontent.com/abit-foggy/pith/main/install.sh | sh
```

### Windows (PowerShell)

```powershell
irm https://raw.githubusercontent.com/abit-foggy/pith/main/install.ps1 | iex
```

### Building from Source

```bash
git clone --recurse-submodules https://github.com/abit-foggy/pith.git
cd pith
make -j$(nproc)
make check
sudo cp pith /usr/local/bin/
```

---

## Quick Start

Create a file named `main.pi`:

```pith
# Check operating system
print "Kernel: " + os.identifyKernel

# Process and arguments
print "Running PID: " + proc.pid
if proc.argCount > 0
    print "First argument: " + proc.getArg(0)
end

# File I/O
fs.writeFile("hello.txt", "Pith deterministic systems scripting.")
if fs.exists("hello.txt")
    content = fs.readFile("hello.txt")
    print "Read: " + content
    fs.remove("hello.txt")
end

# Functions and recursion
fn fib(n)
    if n <= 1
        return n
    end
    return fib(n - 1) + fib(n - 2)
end

print "fib(10) = " + fib(10)
```

### Instant Execution (JIT)

```bash
pith run main.pi
```

### Standalone Native Compilation (AOT)

```bash
pith build -o myapp main.pi
./myapp
```

---

## Standard Namespaces

| Namespace | Focus | Key Members |
|---|---|---|
| `fs` | File system I/O | `readFile`, `writeFile`, `exists`, `remove` |
| `proc` | Process lifecycle & environment | `pid`, `argCount`, `getArg`, `getEnv`, `sleep`, `exit` |
| `os` | Platform & kernel detection | `identifyKernel`, `identifyKernelVersion`, `isLinux`, `isDarwin`, `isMacOS`, `isNT`, `isFreeBSD` |
| `net` | Minimalist socket networking | `socket`, `connect`, `send`, `recv`, `close` |

---

## Links & Documentation

- **Documentation**: [https://abit-foggy.github.io/pith/](https://abit-foggy.github.io/pith/)
- **Standard Library Specification**: [https://abit-foggy.github.io/pith/stdlib-reference.html](https://abit-foggy.github.io/pith/stdlib-reference.html)
- **Repository**: [https://github.com/abit-foggy/pith](https://github.com/abit-foggy/pith)
- **Issue Tracker**: [https://github.com/abit-foggy/pith/issues](https://github.com/abit-foggy/pith/issues)

---

## License

MIT License. See [LICENSE](LICENSE) for details.
