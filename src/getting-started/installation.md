# Installation

## From GitHub releases (recommended)

**POSIX shell (Linux, macOS, FreeBSD)**:
```sh
curl -fsSL https://raw.githubusercontent.com/abit-foggy/pith/main/install.sh | sh
```

**Windows (PowerShell)**:
```powershell
iwr https://raw.githubusercontent.com/abit-foggy/pith/main/install.ps1 -UseBasicParsing | iex
```

**Windows (Command Prompt)**:
```cmd
curl -fsSL https://raw.githubusercontent.com/abit-foggy/pith/main/install.cmd -o install.cmd && install.cmd
```

This downloads the latest release and installs under a prefix
(`~/.local` on POSIX, `%LOCALAPPDATA%\pith` on Windows NT):

| Path | What it is |
|---|---|
| `<prefix>/bin/pith` | The toolchain binary (libtcc embedded statically) |
| `<prefix>/lib/pith/tcc/libtcc1.a` | The vendored tcc runtime |
| `<prefix>/lib/pith/runtime/libruntime.a` | The ARC runtime library |
| `<prefix>/include/pith.h` (+ other headers) | The FFI and ABI headers |

Add `<prefix>/bin` to your `PATH` if it isn't already (the installer
prints the exact command).

### macOS note

The installer checks for the Xcode command line utilities (they
provide clang and the SDK). If missing, it offers to install them
(`xcode-select --install`, password prompt).

### Installer overrides

```sh
PREFIX=/usr/local sh install.sh          # install system-wide (POSIX)
PITH_REPO=owner/pith sh install.sh       # use a fork
PITH_TAG=v0.1.0 sh install.sh            # install a specific version
```

## From source

```sh
git clone --recurse-submodules https://github.com/abit-foggy/pith
cd pith
make
```

The vendored tcc is a git submodule, the build compiles it
automatically. If you cloned without `--recurse-submodules`:

```sh
git submodule update --init
```

Then run the test suite to verify:

```sh
make check
```

## External tools

Pith needs one external tool at runtime:

| Tool | Purpose | Install |
|---|---|---|
| `qbe` | Lowers QBE IL to machine assembly | [QBE](https://c9x.me/compile/) |

The assembly is assembled **in-process** by the embedded tcc's
built-in assembler (Linux, Windows NT, FreeBSD); on Darwin, clang's
assembler is used (from the Xcode command line utilities). `mold` is
optional (Darwin AOT builds only).

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
| `pith` | The toolchain binary, libtcc is embedded statically |
| `libtcc1.a` | tcc runtime, needed by the JIT at relocate time |
| `libruntime.a` | The ARC runtime, linked into `pith build` output |
| `include/pith.h` | The FFI header for imported C modules |
| `include/api.h` | Runtime ABI bindings |
| `include/compiler.h` | Internal API (tokens, AST, engine) |
| `include/pith_embed.h` | Embeddable C host interface |
