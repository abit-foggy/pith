# The Pith Programming Language

A dead-simple, bracketless systems-scripting language that compiles
directly to native machine code via QBE. No garbage collectors, no
virtual machines, no heavyweight compiler drivers on the hot path.

```
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

## Language shape

- Bracketless blocks closed by a single `end` (Lua/Julia style, not
  whitespace-sensitive). No semicolons — newlines delimit statements.
- No `let`/`var`/annotations: `name = expr` declares when the name
  exists nowhere yet, and reassigns when it exists in the current or a
  parent scope. Names are lowerCamelCase (lint-enforced).
- Memory model: deterministic Automatic Reference Counting with
  thread-safe atomic refcounts (C11 `<stdatomic.h>` when available,
  `__sync` builtins otherwise), injected at scope boundaries (`end`,
  reassignments). Zero tracing GC. `pith_break_cycle` breaks reference
  cycles explicitly.
- Builtin namespace: `os.identifyKernel`, `os.identifyKernelVersion`,
  `os.isNT`.

## Pipeline

1. `pith run <script.pi> [more.pi]` — hot path, sub-5ms: lex -> parse
   (bump arena) -> QBE IR (`gen_qbe.c`) -> `qbe` -> assembly ->
   executed in-memory via the embedded `libtcc` (Linux, Windows NT,
   FreeBSD). On Darwin, an ad-hoc signed temp executable via the
   system linker.
2. `pith build <script.pi> [more.pi]` — AOT native artifact: multiple
   translation units are concatenated into ONE `.ssa` module
   (Whole-Program SSA Concatenation — entry point exported, private
   functions stay private, unreferenced ones eliminated), assembled
   via `as`, then linked in-process by the embedded tcc (its built-in
   ELF linker) on Linux / Windows NT / FreeBSD, or via `mold` on
   Darwin. Stripped by default; `--embed-source` (or
   `build.embedSource` in pith.toml) attaches the workspace as an
   uncompressed tar overlay with a `PITHDEBG` EOF footer.
3. `pith decompile <script.pi> [more.pi]` — print the generated QBE
   SSA IR.
4. `pith decompile <binary>` — unpack an embedded debug workspace into
   `./restored_workspace/`.

## Package management, toolchain & tasks

`pith pkg install|sync|add`, `pith engine list|use|install`, and custom
tasks are driven by a project `pith.toml` — they apply to pith
*projects*, not to the toolchain's own source tree. Their configuration
format is documented separately (docs/, forthcoming).

## Embedding (C ABI)

Host applications embed pith via `include/pith_embed.h`:

```c
PithContext *ctx = pith_context_new();
pith_register_fn(ctx, "logInfo", my_log_fn);
int rc = pith_eval_string(ctx, "print \"hello\"");
pith_context_free(ctx);
```

`pith_eval_string` lowers to QBE IR and executes in-memory via libtcc
with host-registered functions callable directly.

## Native C imports (FFI)

Pith scripts import C source directly; the exported (non-static)
functions become callable through the unit's namespace:

```pith
import "ffi/math.c"

sum = math.addInts(3, 4)
msg = math.greet("world")     # owned string return, ARC-released
math.logNote(42)              # void call as a statement
```

- **JIT**: each imported unit compiles into its own libtcc state
  (in-memory), with the pre-baked `<pith.h>` virtual header staged
  into its include path and the runtime symbols registered; its
  exports are then linked into the main execution state.
- **AOT**: each unit compiles to an object (libtcc `TCC_OUTPUT_OBJ`)
  and links into the standalone binary alongside the QBE-generated
  object and `runtime/libruntime.a`.
- **Type mapping** (System V AMD64 / AAPCS64): 32-bit int/enum → `w`,
  64-bit int/pointer → `l`, `float` → `s`, `double` → `d`. The
  compiler emits typed namespaced shims and the widening/narrowing
  conversions at every call boundary.
- **ABI contract** (`include/pith.h`, the canonical header imported C
  modules include): `PithValue` carries an inline 32-bit atomic
  reference count and a type discriminator. Parameters are *borrowed*;
  functions returning `PithValue*` must return a *new* (+1) reference,
  which the compiler releases at scope boundaries. `pithRetain` /
  `pithRelease` keep handoffs leak-free and deterministic.

## Layout

```
Makefile               strict POSIX C99 build recipe for pith + runtime
install.sh             release installer (curl-able; see "Installing")
include/api.h          C/C++ plugin ABI bindings (PithValue, runtime)
include/compiler.h     tokens, AST, SourceLoc, QBE type tags, engine API
include/pith_embed.h   embeddable C ABI host interface
src/                   frontend: lexer, parser, QBE codegen, engine proxy,
                       package manager, config, tar, embed ABI, CLI
runtime/               ARC engine (memory.c), platform/fs (os_fs.c),
                       network stubs (network.c)
tests/                 verification tests + C harnesses (make check)
vendor/tcc/            vendored tcc (git submodule) — libtcc is built from it
```

## Building & testing

```
git clone --recurse-submodules https://github.com/abit-foggy/pith
cd pith
make           build the pith binary and runtime
make check     build and run the verification tests
make clean     remove build artifacts
```

The vendored tcc is a git submodule (see `docs/vendor-tcc.md`); the
build compiles it automatically. External tools used at runtime:
`qbe` (QBE compiler) and `as` (GNU binutils); `mold` is optional for
Darwin AOT builds.

## Installing (from GitHub releases)

```
curl -fsSL https://raw.githubusercontent.com/abit-foggy/pith/main/install.sh | sh
```

Installs the latest tagged release into `~/.local` (override with
`PREFIX=...`; see the script header for more options). A release
workflow (`.github/workflows/release.yml`) builds and attaches
per-platform tarballs whenever a `v*` tag is pushed.

## Environment overrides

```
PITH_QBE        qbe binary (default: qbe)
PITH_AS         assembler (default: as)
PITH_CC         compiler driver / system linker (default: cc)
PITH_MOLD       mold linker override
PITH_TCC        tcc binary override
PITH_TCCDIR     directory containing libtcc1.a
PITH_RUNTIME    path to runtime/libruntime.a
PITH_CACHE      dependency cache directory
PITH_REGISTRY   local package registry directory
```
