# Namespaces

Namespaces are fixed roots that expose related functions through
dotted member access: `os.identifyKernel`, `math.addInts`. A
namespace member used alone is a value; with `(args)` it is a call.

Pith has two kinds of namespaces:

1. **The builtin `os` namespace**, compiled into the runtime
2. **FFI namespaces**, created by `import "module.c"` statements

## The builtin `os` namespace

The `os` namespace exposes platform identification. It is always
available, no import needed:

```pith
if os.isLinux
    print "running on linux"
end

kernel = os.identifyKernel
version = os.identifyKernelVersion
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

### String members are ARC values

`os.identifyKernel` and `os.identifyKernelVersion` return freshly
allocated strings: assigning one to a variable makes the variable an
ARC owner, and the value is released at scope exit. Using one
directly (e.g. in a comparison or `print`) releases the temporary
right after its single use.

### Boolean members

The `is*` members return 32-bit booleans. Comparing them with integer
literals works (`os.isNT == 0`), as do direct truthiness checks
(`if os.isNT`).

## FFI namespaces

An `import "path.c"` statement creates a namespace from the file's
basename (sans `.c`). Every non-static function the C file exports
becomes a member:

```pith
import "ffi/math.c"

sum = math.addInts(3, 4)      # call with args
math.logNote(42)              # void call as a bare statement
```

- Calls are **typed**: the compiler scans the C file's prototypes and
  emits type conversions at every boundary
- String parameters are **borrows**; `PithValue*` returns are **owned
  values** (+1 reference, released at scope exit)
- Two imports may not export the same symbol (the linker would
  collide); the JIT's per-import state isolation prevents most cases

See [C Imports (FFI)](/ffi) for the full pipeline and ABI contract.

## Conflicts and shadowing

### Import vs the builtin os namespace

`import "os.c"` creates a namespace named `os`, which collides with
the builtin. The rule: **imports shadow the builtin, everywhere**.

```pith
import "os.c"

os.identifyKernel()     # -> the import's identifyKernel (called)
os.identifyKernel       # -> the import's identifyKernel (bare access)
```

- The compiler emits a warning: `import 'os' shadows the builtin os
  namespace`
- Every `os.*` access (bare or called) resolves to the import
- Builtin members are hidden; to reach them, remove the import

The import's members must follow the builtin's property semantics for
bare access: a bare `os.member` resolves only when the member takes
zero parameters. Members with parameters must be called:

```pith
os.add(1, 2)     # ok (call)
os.add           # error: member `os.add` expects 2 arguments; call it with (...)
```

### Resolution order

| Access form | Import exists? | Resolves to |
|---|---|---|
| `ns.member` (bare) | yes | the import's member (zero-param only) |
| `ns.member` (bare) | no, ns == `os` | the builtin member |
| `ns.member(...)` (call) | yes | the import's function |
| `ns.member(...)` (call) | no, ns == `os` | the builtin member (zero-arg) |
| either form | no, other ns | error: unknown namespace |

### Variable vs namespace shadowing

Declaring a variable named `os` also shadows the builtin in call
position (the compiler warns). FFI namespaces take precedence over
variables in member-access position.
