# pith engine

Transparent toolchain version proxying — multiple pith compiler
versions coexist, switching automatically per project.

## Commands

```sh
pith engine                        # report the engine configuration
pith engine list                    # installed toolchains + active default
pith engine use <version>           # set the global default
pith engine install <version>        # install the running toolchain locally
```

## Toolchain pinning

A project's `pith.toml` may pin a compiler version:

```toml
[toolchain]
pithVersion = "0.1.0"
```

When the pinned version differs from the running binary's version:

1. The nearest `pith.toml` is found (cwd, then traversing upwards)
2. The pinned version is looked up in `~/.pith/toolchains/<ver>/bin/pith`
3. If found: `argc` and `argv` are forwarded directly via `execv`,
   bypassing the rest of the host execution
4. If not found: an info message is printed and the running binary
   continues

A forwarding guard (`PITH_TOOLCHAIN_ACTIVE`) prevents the forwarded
binary from re-forwarding (infinite loops when both binaries read the
same project config).

## pith engine list

```
pith toolchains (/home/user/.pith/toolchains):
  0.1.0   <- active default
  0.2.0
```

The active default is read from `~/.pith/default_version`.

## pith engine use

```sh
pith engine use 0.2.0
# pith: default toolchain set to v0.2.0
```

Writes the version pointer to `~/.pith/default_version`.

## pith engine install

```sh
pith engine install 0.1.0
# pith: toolchain v0.1.0 installed to ~/.pith/toolchains/0.1.0/bin/pith
```

Copies the currently running binary into the local toolchain cache.
In v0.1, only the running version can be installed (remote fetching
is not implemented).

## Engine report

```sh
pith engine
```

```
pith engine report
  host os           : linux
  execution backend : libtcc (in-memory)
  aot linker        : tcc (embedded, in-process)
  qbe               : /usr/bin/qbe
  as                : /usr/bin/as
  mold              : not found in PATH
  runtime library   : /path/to/runtime/libruntime.a
```
