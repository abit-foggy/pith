# Setting Up a Project

## Directory layout

A typical pith project:

```
myproject/
├── pith.toml            # project configuration
├── pith.lock            # resolved dependency hashes (generated)
├── main.pi              # entry point
├── utils.pi             # additional translation units
├── ffi/
│   └── math.c           # native C imports
└── .pith/
    └── pkgs/            # locally installed packages (generated)
```

## pith.toml

The project manifest. Create one with:

```toml
[project]
name = "myproject"
version = "0.1.0"

[build]
target = "native"
linker = "auto"
engine = "auto"

[toolchain]
pithVersion = "0.1.0"

[dependencies]
# os-utils = "1.0.0"
# mylib = "./path/to/lib"

[tasks.build]
run = "pith run main.pi"
```

See the [Configuration](/config) page for every table and key.

## Multi-file builds

Pass multiple translation units, they're concatenated into a single
`.ssa` module (WPSSAC):

```sh
pith run main.pi utils.pi
pith build main.pi utils.pi
```

All units share the one exported `$main` entry point. Private
functions stay private; unreferenced ones are eliminated for
zero-bloat output.

## Custom tasks

Define recipes in `pith.toml` under `[tasks]`:

```toml
[tasks.build]
run = "pith run main.pi"

[tasks.test]
all = "pith run tests/all.pi"

[tasks.deploy]
prod = "pith build main.pi -o dist/app --embed-source"
```

Then run them:

```sh
pith build          # executes tasks.build.run
pith test all       # executes tasks.test.all
pith deploy prod    # executes tasks.deploy.prod
```

Unknown verbs dispatch through `[tasks]`, the rest are built-in
commands. See [Custom Tasks](/cli/tasks).

## Dependencies

Add a dependency:

```sh
pith pkg add os-utils 1.0.0
```

This appends to `pith.toml` and runs `sync`, the package is
installed into `<project>/.pith/pkgs/os-utils@1.0.0/` and recorded in
`pith.lock` with an FNV-1a integrity hash.

See [pith pkg](/cli/pkg) for all scopes (local, user, machine).

## Embed the workspace into a binary

Attach your source code to the built executable for later recovery:

```sh
pith build main.pi --embed-source -o app
```

The project's `pith.toml` and all `.pi` files are appended as a tar
overlay at the end of the binary, followed by a 16-byte `PITHDEBG`
footer. Anyone with the binary can recover the source:

```sh
pith decompile ./app
# restored workspace successfully extracted to ./restored_workspace/
```

See [pith decompile](/cli/decompile) and [pith build](/cli/build).

## Toolchain pinning

Pin a project to a specific compiler version:

```toml
[toolchain]
pithVersion = "0.1.0"
```

When the pinned version differs from the running binary, invocations
forward to `~/.pith/toolchains/0.1.0/bin/pith` via `execv`. Install
the toolchain locally first:

```sh
pith engine install 0.1.0
```

See [pith engine](/cli/engine).
