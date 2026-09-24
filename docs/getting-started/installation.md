# Installation

## From GitHub releases (recommended)

```sh
curl -fsSL https://raw.githubusercontent.com/abit-foggy/pith/main/install.sh | sh
```

This downloads the latest release and installs into `~/.local`:

```
~/.local/bin/pith                        — the toolchain binary
~/.local/lib/pith/tcc/libtcc1.a          — vendored tcc runtime
~/.local/lib/pith/runtime/libruntime.a   — ARC runtime library
~/.local/include/pith.h                 — FFI header
~/.local/include/api.h                   — runtime ABI bindings
```

Add `~/.local/bin` to your `PATH` if it isn't already:

```sh
echo 'export PATH="$HOME/.local/bin:$PATH"' >> ~/.profile
```

### Installer overrides

```sh
PREFIX=/usr/local sh install.sh          # install system-wide
PITH_REPO=owner/pith sh install.sh       # use a fork
PITH_TAG=v0.1.0 sh install.sh            # install a specific version
```

## From source

```sh
git clone --recurse-submodules https://github.com/abit-foggy/pith
cd pith
make
```

The vendored tcc is a git submodule — the build compiles it
automatically. If you cloned without `--recurse-submodules`:

```sh
git submodule update --init
```

Then run the test suite to verify:

```sh
make check
```

## External tools

Pith needs two external tools at runtime:

| Tool | Purpose | Install |
|---|---|---|
| `qbe` | Lowers QBE IL to machine assembly | [QBE](https://c9x.me/compile/) |
| `as` | Assembles the output | GNU binutils (pre-installed on Linux) |

`mold` is optional (used for Darwin AOT builds only).

## Verify

```sh
pith version
# pith 0.1.0

pith engine
# pith engine report
#   host os           : linux
#   execution backend : libtcc (in-memory)
#   aot linker        : tcc (embedded, in-process)
#   qbe               : /usr/bin/qbe
#   ...
```

## What's inside a release

| File | Description |
|---|---|
| `pith` | The toolchain binary — libtcc is embedded statically |
| `libtcc1.a` | tcc runtime, needed by the JIT at relocate time |
| `libruntime.a` | The ARC runtime, linked into `pith build` output |
| `include/pith.h` | The FFI header for imported C modules |
| `include/api.h` | Runtime ABI bindings |
| `include/compiler.h` | Internal API (tokens, AST, engine) |
| `include/pith_embed.h` | Embeddable C host interface |
