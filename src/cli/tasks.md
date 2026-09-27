# Custom Tasks

Projects define command recipes in `pith.toml` under `[tasks]`. Any
unknown verb dispatches through the task table.

## Defining tasks

```toml
[tasks.build]
run = "pith run main.pi"

[tasks.test]
all = "pith run tests/all.pi"
unit = "pith run tests/unit.pi"

[tasks.deploy]
prod = "pith build main.pi -o dist/app --embed-source"
dev = "pith build main.pi -o dist/app-dev"
```

## Running tasks

```sh
pith build              # -> tasks.build ("pith run main.pi")
pith test all           # -> tasks.test.all ("pith run tests/all.pi")
pith deploy prod        # -> tasks.deploy.prod
pith test               # -> tasks.test (falls back, no bare key)
```

## Resolution order

When `pith <verb> [sub] [args...]` is received:

1. Check if `<verb>` is a built-in (`run`, `build`, `decompile`, `pkg`,
   `engine`, `help`, `version`)
2. If not built-in, find the nearest `pith.toml` (cwd upwards)
3. If `<sub>` is provided, look up `tasks.<verb>.<sub>` first
4. Fall back to `tasks.<verb>`
5. Append remaining arguments to the command string
6. Execute via `execvp` (POSIX) or `CreateProcess` (Windows NT)

## Built-in commands take precedence

`pith run`, `pith build`, etc. are always handled as built-ins, a
task named `run` in `pith.toml` is unreachable. Choose task names
that don't collide:

```toml
[tasks.quick-run]     # works, "run" is built-in, "quick-run" isn't
script = "pith run main.pi"
```

## Arguments

Extra CLI arguments are appended to the task command:

```sh
pith deploy prod --verbose
# executes: pith build main.pi -o dist/app --embed-source --verbose
```
