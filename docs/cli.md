# CLI Reference

## pith run

```sh
pith run <file.pi> [more.pi]
```

Compile and execute via the instant pipeline: lex → parse → QBE IR →
`qbe` → assembly → **libtcc in-memory** (Linux, Windows NT, FreeBSD).
On Darwin, an ad-hoc signed temp executable via the system linker.

Multiple translation units are concatenated into a single `.ssa`
module (WPSSAC) — all units' top-level statements share the one
`$main` entry point.

## pith build

```sh
pith build <file.pi> [more.pi] [--embed-source] [-o output]
```

Build a standalone native binary: QBE IR → assembled via `as` →
linked **in-process by the embedded tcc** (its built-in ELF linker) on
Linux/Windows NT/FreeBSD, or via **mold** on Darwin.

### --embed-source

Attaches the project workspace (input scripts + all `.pi` files from
the project root and `src/`) as an uncompressed tar overlay at the
end of the executable, followed by a 16-byte `PITHDEBG` footer.

Recover with `pith decompile <binary>`.

## pith decompile

```sh
pith decompile <file.pi> [more.pi]   # print the generated QBE SSA IR
pith decompile <binary>              # unpack an embedded workspace
```

Binary mode reads the EOF `PithDEBG` footer, seeks backward, and
extracts the archived files into `./restored_workspace/`. If the
binary has no embedded payload, prints an error and exits 1.

## pith pkg

```sh
pith pkg install [--global|--global-root]
pith pkg sync    [--global|--global-root]
pith pkg add <name> <version>
pith pkg                          # status report
```

Three isolated scopes:

| Scope | Flag | Root | Notes |
|---|---|---|---|
| Local (default) | — | `<cwd>/.pith/pkgs/` | hermetic, writes `pith.lock` |
| User global | `--global` | `~/.pith/pkgs/` | no sudo, tools into `~/.pith/bin/` |
| Machine root | `--global-root` | `/usr/local/pith/pkgs/` | privilege-checked |

Sources are local-first: `PITH_REGISTRY` tarballs, already-installed
scopes, or a path directly in the project's `pith.toml`. `pith.lock`
records resolved versions with FNV-1a integrity hashes.

## pith engine

```sh
pith engine                        # report the engine configuration
pith engine list                    # installed toolchains + active default
pith engine use <version>           # set the global default
pith engine install <version>       # install the running toolchain locally
```

When a project's `pith.toml` pins a `[toolchain]` version different
from the running binary, invocations are forwarded to
`~/.pith/toolchains/<ver>/bin/pith` via `execv`.

## Custom tasks

Projects define task recipes in `pith.toml` under `[tasks]`:

```toml
[tasks.pulp]
build = "pith run scripts/build.pi"
```

Unknown verbs dispatch through the task table:

```sh
pith pulp build    # executes tasks.pulp.build via execvp
```

## Environment overrides

| Variable | Default | Description |
|---|---|---|
| `PITH_QBE` | `qbe` | QBE compiler binary |
| `PITH_AS` | `as` | Assembler |
| `PITH_CC` | `cc` | Compiler driver / system linker |
| `PITH_MOLD` | `mold` | mold linker override |
| `PITH_TCC` | `tcc` | tcc binary override |
| `PITH_TCCDIR` | (auto) | Directory containing libtcc1.a |
| `PITH_RUNTIME` | (auto) | Path to runtime/libruntime.a |
| `PITH_CACHE` | (auto) | Dependency cache directory |
| `PITH_REGISTRY` | (none) | Local package registry directory |
