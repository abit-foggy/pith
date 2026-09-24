# Embeddable C ABI

Host C/C++ applications embed pith via `include/pith_embed.h`, a
pure C ABI with direct function pointer registration (no virtual stack
marshaling).

## API

```c
#include "pith_embed.h"

/* create a fresh embedding context */
PithContext *ctx = pith_context_new();

/* register a host function callable from pith code */
pith_register_fn(ctx, "logInfo", my_log_fn);

/* evaluate a pith source string (lowers to QBE IR, runs via libtcc) */
int rc = pith_eval_string(ctx, "print \"hello from pith\"");

/* free the context */
pith_context_free(ctx);
```

## Functions

| Function | Description |
|---|---|
| `pith_context_new()` | Create a fresh embedding context |
| `pith_context_free(ctx)` | Free a context |
| `pith_register_fn(ctx, name, fn_ptr)` | Register a C function under `name` |
| `pith_eval_string(ctx, source)` | Compile and execute a pith source string |

`pith_eval_string` runs the full pipeline, lex, parse (bump arena),
QBE lowering, `qbe`, in-memory libtcc execution, with the context's
registered functions available alongside the runtime ABI. Returns the
program exit code, or `1` on a compile/engine failure.

## Complete example

```c
#include <stdio.h>
#include "pith_embed.h"

static void log_info(void)
{
    printf("[host] logInfo called from pith\n");
}

int main(void)
{
    PithContext *ctx = pith_context_new();
    if (!ctx)
        return 1;

    pith_register_fn(ctx, "logInfo", log_info);

    int rc = pith_eval_string(ctx,
        "print \"hello from embedded pith\"\n"
        "x = 20\n"
        "if x == 20\n"
        "    print \"embed: arithmetic ok\"\n"
        "end\n");

    printf("[host] eval exit code: %d\n", rc);
    pith_context_free(ctx);
    return rc;
}
```

## Building the embedder

Link the embedder against the pith frontend objects plus the runtime
and libtcc:

```
cc host_app.c -Iinclude -Ivendor/tcc \
   src/lexer.o src/parser.o src/gen_qbe.o \
   src/engine_proxy.o src/pith_embed.o src/tar.o src/config.o \
   runtime/memory.o runtime/os_fs.o runtime/network.o \
   vendor/tcc/libtcc.a -ldl -o host_app
```

The `PithContext` is opaque, it internally holds a dynamic array of
`{ name, function_pointer }` pairs that the engine registers into
every libtcc execution alongside the runtime symbols.
