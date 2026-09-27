# Namespaces

Namespaces are fixed roots that expose related functions through
dotted member access: `os.identifyKernel`, `math.addInts`. A
namespace member used alone is a value; with `(args)` it is a call.

Pith namespaces are organized in a **scope hierarchy**:

1. **The root scope** (`root.os.*`), the base runtime implementation
2. **The merged module view** (`os.*`), the builtin plus active
   import overrides
3. **Developer scopes** (`alice.os.*`), an imported author's module

## The scope hierarchy

```
root.os.identifyKernel     <- base runtime (no overrides, ever)
os.identifyKernel          <- merged view (overrides apply)
alice.os.identifyKernel    <- alice's import (direct)
```

## The builtin `os` namespace

The `os` namespace exposes platform identification. It is always
available, no import needed, through BOTH the unadorned namespace and
the explicit root scope:

```pith
if os.isLinux
    print "running on linux"
end

kernel = os.identifyKernel           # merged view
base = root.os.identifyKernel        # base runtime, bypassing overrides
```

### Members

| Member | Type | Returns |
|---|---|---|
| `os.identifyKernel` | string | `"linux"`, `"darwin"`, `"nt"`, `"freebsd"`, `"unknown"` |
| `os.identifyKernelVersion` | string | Kernel/OS version string (uname.release on POSIX) |
| `os.isNT` | bool | `1` on Windows NT, `0` elsewhere |
| `os.isLinux` | bool | `1` on Linux, `0` elsewhere |
| `os.isFreeBSD` | bool | `1` on FreeBSD, `0` elsewhere |
| `os.isDarwin` | bool | `1` when the kernel is Darwin (macOS and other Darwin systems) |
| `os.isMacOS` | bool | `1` only on Apple macOS |
| `os.getEnv(name)` | string | Environment variable value (or `""` if unset) |
| `os.exit(code)` | void | Immediately terminates the process with exit code |
| `os.argCount` | int | Total number of CLI arguments passed to the script |
| `os.getArg(index)` | string | Argument at 0-based index |

### isDarwin vs isMacOS

These are slightly different:

- **`os.isDarwin`** is a *kernel-level* check: TRUE whenever the
  kernel reports "Darwin" (via uname). This includes Apple macOS AND
  non-Apple Darwin systems.
- **`os.isMacOS`** is an *Apple-specific* check: TRUE only on Apple's
  macOS (detected via the Apple toolchain's `__APPLE__` + `__MACH__`
  defines).

```pith
if os.isDarwin
    print "darwin kernel"
end
if os.isMacOS
    print "apple macos"
end
```

## The builtin `fs` namespace

The `fs` namespace provides simple, built-in filesystem operations. Like `os`, it is always available without an import, mapped directly to the base runtime implementation `root.fs.*`:

```pith
# Write to a file (returns 1 on success)
fs.writeFile("hello.txt", "Hello from Pith!")

# Read from a file (returns file content or "" on error)
content = fs.readFile("hello.txt")
print content
```

### Members

| Member | Type | Returns |
|---|---|---|
| `fs.readFile(path)` | string | File content as a string (or `""` on error) |
| `fs.writeFile(path, content)` | int | `1` on successful write, `0` on error |

Following Pith's namespace standard (`devName.namespace.function`), `root.fs.*` represents the builtin implementation, `fs.*` represents the active merged view, and custom modules (e.g. `alice.fs.*`) can provide extensions or overrides.

## The builtin `net` namespace

The `net` namespace exposes dead-simple TCP networking primitives. Like `os`, it is always available without an import, mapped directly to the base runtime implementation `root.net.*`:

```pith
fd = net.connect("127.0.0.1", 8080)
if fd >= 0
    net.send(fd, "GET / HTTP/1.0\r\n\r\n")
    response = net.recv(fd, 4096)
    print response
    net.close(fd)
end
```

### Members

| Member | Type | Returns |
|---|---|---|
| `net.socket()` | int | New TCP socket descriptor (or `-1` on error) |
| `net.connect(host, port)` | int | Connected socket descriptor (or `-1` on error) |
| `net.send(fd, message)` | int | Number of bytes sent (or `-1` on error) |
| `net.recv(fd, maxBytes)` | string | Read data as an ARC string (empty on EOF/error) |
| `net.close(fd)` | void | Closes the socket descriptor |

Following Pith's namespace standard (`devName.namespace.function`), `root.net.*` represents the builtin implementation, `net.*` represents the active merged view, and custom modules (e.g. `abit.net.*`) can provide extensions or overrides.

## Developer-scoped imports

An `import "path.c"` statement creates a namespace from the path:

- `import "os.c"` (no directory): a **root-level import**, its symbols
  join the merged `os.*` view directly
- `import "alice/os.c"` (with a directory): a **developer-scoped
  import**, the directory is the author scope, accessible as
  `alice.os.*`

```pith
import "alice/os.c"

alice.os.identifyKernel()     # alice's export, direct
```

Every non-static function the C file exports becomes a member of the
author's module namespace.

## Symbol overlay and override precedence

Imports do NOT shadow a namespace entirely. Instead, each symbol
merges into the module's namespace view:

### Fallback / pass-through

If the builtin exposes a member the import does not define, the
unqualified lookup resolves cleanly to the base implementation:

```pith
import "alice/os.c"    # alice only overrides identifyKernel

if os.isNT == 0        # os.isNT is NOT overridden: the builtin runs
    print "builtin isNT"
end
```

### Selective override

If both the builtin and an imported author module define the same
member, the imported author's symbol takes precedence in the
unqualified lookup:

```pith
import "alice/os.c"    # alice overrides identifyKernel

os.identifyKernel()    # -> alice.os.identifyKernel()
```

The compiler emits an informational note at import time:

```
note: alice.os.identifyKernel overrides os.identifyKernel
```

### Fully-qualified disambiguation

- `alice.os.identifyKernel` always resolves directly to Alice's
  export, bypassing all overrides
- `root.os.identifyKernel` always resolves to the base runtime
  implementation, bypassing all overrides

```pith
import "alice/os.c"

if os.identifyKernel == "alice"           # override wins
if root.os.identifyKernel == "linux"      # base runtime wins
if alice.os.identifyKernel == "alice"     # alice direct
```

## Resolution order

The compiler uses identical resolution logic for calls
(`ns.member(...)`) and bare accesses (`ns.member`):

1. **Fully-qualified path** (`author.module.symbol` or
   `root.module.symbol`): direct layer lookup
2. **Unqualified** (`module.symbol`): the overlay table (active
   imports, newest first), then the root base table
3. If unresolved in all layers: `unknown member or namespace` error

| Access | Resolves to |
|---|---|
| `root.os.identifyKernel` | the base runtime, always |
| `alice.os.identifyKernel` | alice's import, always |
| `os.identifyKernel` (alice imported) | alice's (override) |
| `os.identifyKernel` (no import) | the builtin |
| `os.isNT` (alice imported) | the builtin (not overridden) |

## Arity rules

Strict arity validation across both bare and call lookups:

- Bare access to a multi-parameter function: error
  (`member 'os.add' expects 2 arguments; call it with (...)`)
- Bare access to a zero-parameter member: called like a property
- Calls: the argument count must match exactly

## Memory Safety

Strings returned by namespaces are managed automatically. You don't need to manually free or allocate memory when reading files, receiving network data, or checking the operating system.
