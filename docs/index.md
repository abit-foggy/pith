---
layout: home

hero:
  name: "Pith"
  text: "A dead-simple, bracketless systems-scripting language."
  tagline: Compiles directly to native machine code via QBE. Zero GC, zero VM, zero bloat.
  actions:
    - theme: brand
      text: Get Started
      link: /getting-started
    - theme: alt
      text: View on GitHub
      link: https://github.com/abit-foggy/pith

features:
  - title: Bracketless
    details: Blocks close with a single <code>end</code>. No semicolons, no braces, no noise.
  - title: Deterministic ARC
    details: Reference counts injected at scope boundaries. No tracing garbage collector, ever.
  - title: Native via QBE
    details: Lowers to QBE IL, assembles to machine code, links with the embedded tcc linker or mold.
  - title: C Imports (FFI)
    details: Import .c source directly into your script with typed calls and leak-free ARC handoffs.
  - title: Instant Dev Loop
    details: In-memory JIT execution via libtcc. Compile and run in milliseconds.
  - title: Self-Healing Binaries
    details: Attach your workspace to a built executable and recover it with <code>pith decompile</code>.
---
