# pith build

```sh
pith build <file.pi> [more.pi ...] [--embed-source] [-o output]
```

Build a standalone native binary.

## Pipeline

```
source.pi → lex → parse → QBE IR (WPSSAC) → qbe → assembly
          → as → .o
          → link: embedded tcc linker (Linux/NT/FreeBSD)
                  or mold via compiler driver (Darwin)
                  or system linker (fallback)
          → standalone executable
```

On Linux, Windows NT, and FreeBSD, the link happens **in-process** via
the embedded tcc's built-in ELF linker, no external linker or
subprocess. On Darwin, mold is driven through the compiler driver
(`clang -fuse-ld=mold`). Fallbacks: a tcc binary, then the system
linker.

## Options

| Flag | Description |
|---|---|
| `-o <path>` | Output path (default: input basename sans `.pi`) |
| `--embed-source` | Attach the workspace as a tar overlay with a `PITHDEBG` footer |
| `--plugin` | Build an installable plugin (.ppkg) instead of an executable |

`--embed-source` can also be set permanently via `build.embedSource =
true` in `pith.toml`; `--plugin` via `toolchain.pithPlugin = "yes"`.

## Plugin builds (.ppkg)

With `--plugin` (or `[toolchain].pithPlugin = "yes"`), the build
produces `<name>.ppkg`, a tar bundle containing:

| Entry | What it is |
|---|---|
| `plugin.o` | The compiled object with exported `c_<author>_<module>_<fn>` symbols |
| `manifest` | The plugin's symbol table: author, module, and each fn (name, return class, parameter count) |

The author comes from `[project].author`; the module from
`[project].name`. Only `fn` declarations are exported (zero-parameter,
returning 64-bit integers in v0.1); top-level statements are ignored.

```
pith build myos.pi --plugin -o myplugin
# built plugin myplugin.ppkg (2 exported fns)
```

The plugin is installable with `pith pkg` (from the .ppkg or from
source) and callable from consuming projects as
`<author>.<module>.<fn>`:

```pith
if alice.myos.identifyKernel == 42
    print "plugin works"
end
```

See [pith pkg](/cli/pkg) for installing and
[Namespaces](/namespaces) for the resolution rules.

## Multi-file (WPSSAC)

Multiple translation units concatenate into one `.ssa` module, the
same as `pith run`. All imported C units are compiled to object files
and linked in alongside the QBE-generated object and the runtime
archive.

## Output

The resulting binary is a stripped, dynamically linked executable
that needs only libc at runtime. It contains no VM, no interpreter,
and no pith-specific runtime beyond `libruntime.a` (the ARC engine
and platform primitives).

## --embed-source

When enabled, the build appends:
1. An uncompressed ustar tar archive containing the project's
   `pith.toml`, input scripts, and all `.pi` files from the project
   root and `src/`
2. A 16-byte footer: `{ uint64_t payload_size; char magic[8] }` where
   `magic` is `"PITHDEBG"`

Recover with `pith decompile <binary>`.

## Default builds are stripped

Without `--embed-source`, the binary contains **no** source metadata,
the last 16 bytes are normal ELF data, not the `PITHDEBG` magic.
