# The Pith Reference

Welcome to **The Pith Reference**, the official guide and documentation for the Pith programming language.

Pith is a dead-simple, bracketless systems-scripting language compiled directly to native machine code via QBE. It is designed to be immediately readable for beginners while giving systems developers uncompromising native control with zero garbage collection pauses.

---

## Core Tenets

- **Bracketless & Clean**: Blocks close with a single `end`. No semicolons, no braces, no boilerplate.
- **Deterministic ARC**: Atomic 32-bit reference counts injected at lexical scope boundaries. Zero tracing GC, zero VM, zero pauses.
- **Native via QBE**: Compiles to QBE intermediate language (WPSSAC), lowers to native machine code, and links with an embedded compiler or system linker.
- **C Imports (FFI)**: Import C source files directly into your scripts with typed calls and leak-free ARC handoffs.
- **Instant Feedback**: In-memory JIT execution via libtcc. Compile and run scripts in milliseconds.
- **Interactive REPL**: A built-in interactive shell (`pith repl`) for testing expressions, exploring APIs, and prototyping functions.
- **Embeddable**: Clean, pure C ABI host interface for integrating Pith into existing engines and native applications.

---

## Navigating This Book

- **[Getting Started](getting-started/installation.md)**: Install Pith, run your first script, and configure your project workspace.
- **[Language Reference](language/index.md)**: Comprehensive guide to Pith grammar, variables, types, operators, control flow, and functions.
- **[Standard Runtime & Namespaces](namespaces.md)**: Explore builtin `os.*` tools and `net.*` TCP socket networking.
- **[Runtime & Memory Model](runtime.md)**: Deep dive into deterministic ARC, reference cycles, and memory safety invariants.
- **[CLI Reference](cli/run.md)**: Complete guide to CLI commands (`run`, `repl`, `build`, `decompile`, `pkg`, `engine`).
- **[C Interop (FFI)](ffi.md)**: Directly import and call C functions from Pith scripts.
- **[Embedding Pith](embed.md)**: Integrate the Pith runtime and compiler into your C/C++ applications.
