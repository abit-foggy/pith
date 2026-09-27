# pith repl

```sh
pith repl
# or simply:
pith
```

Interactive Read-Eval-Print Loop (REPL) for experimenting with Pith syntax, exploring APIs, and testing code.

## Interactive Sessions

When launched with no arguments or with `pith repl`, the Pith shell displays the interactive prompt:

```text
pith 0.1.0 interactive repl
type exit or press ctrl+d to quit

pith> x = 10
pith> x + 32
42
```

Bare expressions are automatically evaluated and formatted to stdout.

## Multi-line Blocks

Multi-line blocks (`if`, `while`, `fn`) continue across lines using the `... ` continuation prompt until closed with matching `end` statements:

```pith
pith> fn greet(name)
...       return "hello, " + name
...   end
pith> greet("world")
hello, world
```

## Session Persistence

Variable declarations (`name = val`, `mut name = val`) and function declarations persist across evaluations in the REPL session:

```pith
pith> mut count = 0
pith> while count < 3
...       print count
...       count = count + 1
...   end
0
1
2
```

## Exiting

To exit the REPL, type `exit` and press Enter, or send EOF via `Ctrl+D`.
