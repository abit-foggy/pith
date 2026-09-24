# Getting Started

Pith is a dead-simple, bracketless systems-scripting language that
compiles directly to native machine code via QBE. No garbage
collectors, no virtual machines, no heavyweight compiler drivers on
the hot path.

## The language shape

```pith
# System audit test
if os.identifyKernel == "linux"
    kernelVersion = os.identifyKernelVersion
    print "Running smoothly on Linux kernel version: " + kernelVersion
elseif os.isNT
    print "Running safely on Windows NT architecture."
else
    print "Running on an alternative platform."
end
```

- **Bracketless blocks** closed by a single `end` (Lua/Julia style).
- **No `let`/`var`/annotations**: `name = expr` declares when the name
  exists nowhere yet, reassigns when it does. Names are lowerCamelCase.
- **Deterministic ARC** on strings: retain/release calls are injected at
  scope boundaries (`end`, reassignments). Zero tracing GC.
- **Builtin namespace**: `os.identifyKernel`, `os.identifyKernelVersion`,
  `os.isNT`.

## Installation

From GitHub releases:

```sh
curl -fsSL https://raw.githubusercontent.com/abit-foggy/pith/main/install.sh | sh
```

Or build from source:

```sh
git clone --recurse-submodules https://github.com/abit-foggy/pith
cd pith
make
```

External tools needed at runtime: `qbe` (QBE compiler) and `as` (GNU
binutils). The vendored tcc is a git submodule; the build compiles it
automatically.

## Running and building

```sh
pith run script.pi       # instant pipeline: QBE IR -> assembly -> libtcc in-memory
pith build script.pi     # AOT: linked into a standalone native binary
pith decompile script.pi # print the generated QBE SSA IR
```

## Next steps

- Read the full [README](https://github.com/abit-foggy/pith/blob/main/README.md)
  for the package manager, toolchain proxying, custom tasks, and the
  embeddable C ABI.
- Browse the test suite under
  [tests/](https://github.com/abit-foggy/pith/tree/main/tests) for
  language feature examples.
