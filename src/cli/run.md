# pith run

```sh
pith run <file.pi> [more.pi ...]
```

Compile and execute via the instant pipeline, no temp executable,
no heavyweight compiler driver.

## Pipeline

```
source.pi → lex → parse (bump arena) → QBE IR (WPSSAC)
          → qbe → assembly → as → .o
          → libtcc (TCC_OUTPUT_MEMORY, symbols registered, relocated)
          → main() called natively in host memory
```

On **Darwin**, an ad-hoc signed temp executable is written by the
system clang at `-O0` and executed immediately.

## Multi-file (WPSSAC)

Pass multiple translation units to compile them into a single
`.ssa` module:

```sh
pith run main.pi utils.pi helpers.pi
```

All units' top-level statements share the one exported `$main`. The
single-pass QBE lowering performs whole-program constant folding and
register allocation. Unreferenced private functions are eliminated.

## Native C imports

When a script contains `import "path/to/file.c"`, the engine compiles
each imported unit into its own libtcc state (in memory), registers
the runtime symbols into it, relocates it, and links its exported
functions into the main execution state. The import states stay
alive until execution finishes.

See [C Imports (FFI)](../ffi.md).

## Diagnostics

Compile errors print rustc-style diagnostics and exit with status 1:

```
error: use of undeclared identifier `undefinedVar`
  --> script.pi:1:7
   |
 1 | print undefinedVar
   |       ^~~~~~~~~~~~
error: aborting due to 1 previous error
```

Runtime exit codes propagate: the script's `return` value (or 0)
becomes the `pith run` exit code.
