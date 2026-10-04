# Embeddable C ABI

Host C/C++ applications embed pith via `include/pith_embed.h`, a
pure C ABI with direct function pointer registration (no virtual stack
marshaling). Host functions registered with a signature are callable
from pith code as a namespace, the way a game engine exposes its API
to an embedded scripting language.

## API

```c
#include "pith_embed.h"

/* create a fresh embedding context */
PithContext *ctx = pith_context_new();

/* register a callable host function (see below) */
pith_register_ns_fn(ctx, "host", "addTarget",
                   addTarget, 'w', "pw");

/* register a plain runtime symbol (no signature, not callable
   from pith source; useful for resolving names the engine emits) */
pith_register_fn(ctx, "logInfo", my_log_fn);

/* register an object for the temp-executable fallback */
pith_register_link_object(ctx, "host_api.o");

/* evaluate a pith source string */
int rc = pith_eval_string(ctx, "print \"hello from pith\"");

/* free the context */
pith_context_free(ctx);
```

`pith_eval_string` runs the full pipeline, lex, parse (bump arena),
QBE lowering, `qbe`, execution, with the context's registered
functions available alongside the runtime ABI. Returns the program
exit code, or `1` on a compile/engine failure.

## Functions

| Function | Description |
|---|---|
| `pith_context_new()` | Create a fresh embedding context |
| `pith_context_free(ctx)` | Free a context |
| `pith_register_fn(ctx, name, fn_ptr)` | Register a raw runtime symbol under `name` |
| `pith_register_ns_fn(ctx, ns, name, fn_ptr, ret_class, param_classes)` | Register a host function callable from pith as `ns.name(...)` |
| `pith_register_link_object(ctx, obj_path)` | Supply the object the fallback links so host functions resolve there |
| `pith_eval_string(ctx, source)` | Compile and execute a pith source string |

## Typed host functions

`pith_register_ns_fn` teaches the frontend about a host function:
pith code calls it as `ns.name(...)` with the same typed, direct C ABI
calls used for native C imports. Every registered function joins a
namespace (the `ns` argument); the namespace is the call prefix, and
zero-parameter functions read as properties.

The signature is written with one class letter per slot:

| Class | C type | Pith type |
|---|---|---|
| `'v'` | `void` | statement calls only (return only) |
| `'w'` | `int`, `int32_t`, `enum`, `bool` | integer (widened via `extsw`) |
| `'l'` | `long`, `int64_t`, `size_t`, `T*` | integer or string |
| `'s'` | `float` | float (narrowed via `truncd`) |
| `'d'` | `double` | float |
| `'p'` | `PithValue*` | string (parameters are borrowed; a `'p'` return transfers a new `+1` reference the compiler releases at scope exit) |

`param_classes` holds one letter per parameter, in order (`""` or
`NULL` for zero parameters, at most 8). Registration fails with a
diagnostic on unknown letters, `void` parameters, duplicate names, or
a full namespace (64 functions per namespace).

### Calling conventions

Calls lower to direct typed calls through the System V AMD64 / AAPCS64
ABI, with widening and narrowing inserted at the boundary, exactly as
for native C imports:

```pith
# host.addTarget is registered as ('w', "pw")
host.addTarget("demo", host.exe)      # statement call, int return

if host.add(2, 3) == 5
    print "int parameters cross as 32-bit words"
end

print host.tag("owned")               # 'p' return: an owned string
host.note("borrowed parameter")       # 'v': statement only
```

## Execution backends

Evaluation runs in-memory through the embedded tcc when the host kernel
allows it. On hardened kernels (SELinux `mprotect` denials) and on
Darwin, pith falls back to a temporary executable: the script object
is linked with `runtime/libruntime.a` and executed immediately. The
fallback child process cannot bind in-memory addresses, so hosts
register their compiled API object with `pith_register_link_object`;
the engine links it into the child, and evaluation behaves identically
on both backends.

The registered object must export the `c_<ns>_<fn>` symbols the
generated calls reference. Hosts compile their API module with the
same author-aware renames pith applies to imported C units:

```sh
cc -DaddTarget=c_host_addTarget -Dtag=c_host_tag -c host_api.c -o host_api.o
```

Registering a link object is optional when in-memory execution is
available, and recommended for portability.

## Complete example

```c
#include <stdio.h>
#include "pith.h"
#include "pith_embed.h"

static int count;
static int cycles(void) { return count; }
static int addTarget(PithValue *name, int type)
{
    printf("[host] target %s (type %d)\n", pithStringData(name), type);
    count++;
    return 1;
}
static PithValue *tag(PithValue *s)
{
    char buf[512];
    snprintf(buf, sizeof(buf), "[%s]", pithStringData(s));
    return pithNewString(buf);   /* +1 reference */
}

int main(void)
{
    PithContext *ctx = pith_context_new();
    if (!ctx)
        return 1;

    pith_register_ns_fn(ctx, "host", "cycles", cycles, 'w', "");
    pith_register_ns_fn(ctx, "host", "addTarget", addTarget, 'w', "pw");
    pith_register_ns_fn(ctx, "host", "tag", tag, 'p', "p");
    pith_register_link_object(ctx, "host_api.o");

    int rc = pith_eval_string(ctx,
        "host.addTarget(\"demo\", 1)\n"
        "print host.tag(\"owned\")\n"
        "print host.cycles\n");

    printf("[host] eval exit code: %d\n", rc);
    pith_context_free(ctx);
    return rc;
}
```

## Building the embedder

Link the embedder against the pith frontend objects, the runtime, the
embedded qbe backend, and libtcc:

```
cc host_app.c -Iinclude -Ivendor/tcc \
   src/lexer.o src/parser.o src/gen_qbe.o \
   src/engine_proxy.o src/pith_embed.o src/tar.o src/config.o src/cffi.o \
   runtime/memory.o runtime/os_fs.o runtime/network.o \
   vendor/qbe/libqbe.a vendor/tcc/libtcc.a -ldl -o host_app
```

The `PithContext` is opaque; internally it holds the registered
runtime symbols, the typed namespace units the code generator
consumes, and the fallback link objects, and the engine registers
everything into every execution alongside the runtime ABI.

Host namespaces are root-level: they resolve like a bare
`import "thorn_engine.c"` and use the same `c_<ns>_<fn>` mangling.
Because the frontend resolves registered namespaces before the
builtin ones, a namespace named `fs`, `os`, `proc`, or `net`
deliberately shadows the builtin; prefer a distinct name.

Registered functions see exactly the ABI described in
[C Imports (FFI)](ffi.md): parameters are borrowed, `PithValue*`
returns are `+1`, and the test suite verifies the boundary leak-free.
