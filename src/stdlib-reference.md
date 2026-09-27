# Standard Library Specification

This document provides a low-level, authoritative reference for experienced systems developers. It details the runtime signatures, memory contracts, error semantics, and underlying system call implementations of Pith's standard library namespaces.

---

## Namespace: `fs`

The `fs` namespace exposes file descriptor operations and direct filesystem access.

### Functions

```pith
fn readFile(path: string) string
```

Reads the complete contents of the file specified by `path` into a newly allocated, null-terminated string.

- **Parameters**:
  - `path`: Relative or absolute filesystem path.
- **Return value**:
  - A reference-counted string containing the file data.
  - Returns an empty string `""` on failure (e.g. `ENOENT`, `EACCES`, or if allocation fails).
- **Underlying Syscalls**:
  - POSIX: `stat(2)` / `open(2)` / `read(2)` / `close(2)`.
  - Windows: `CreateFileA` / `GetFileSizeEx` / `ReadFile` / `CloseHandle`.
- **Memory Safety**:
  - Allocates a contiguous buffer sized to file length plus one byte. The returned `PithValue*` has an initial strong reference count of 1 and is reclaimed deterministically at scope exit.

```pith
fn writeFile(path: string, content: string) int
```

Creates or truncates the file at `path` and writes the provided `content` string.

- **Parameters**:
  - `path`: Target file path.
  - `content`: Data buffer to write.
- **Return value**:
  - `1` if all bytes were successfully written to disk.
  - `0` on system error or partial write.
- **Underlying Syscalls**:
  - POSIX: `open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644)`, followed by complete loop write and `close(2)`.
  - Windows: `CreateFileA(..., GENERIC_WRITE, ..., CREATE_ALWAYS, ...)`.

```pith
fn exists(path: string) int
```

Checks whether a filesystem entry exists at `path`.

- **Parameters**:
  - `path`: Target file or directory path.
- **Return value**:
  - `1` if the file or directory exists.
  - `0` if it does not exist or access is denied.
- **Underlying Syscalls**:
  - POSIX: `access(path, F_OK)`.
  - Windows: `GetFileAttributesA(path)`.

```pith
fn remove(path: string) int
```

Deletes the file entry at `path`.

- **Parameters**:
  - `path`: Target file path.
- **Return value**:
  - `1` if the file was deleted successfully.
  - `0` on system error (`ENOENT`, `EACCES`, etc.).
- **Underlying Syscalls**:
  - POSIX: `unlink(path)`.
  - Windows: `DeleteFileA(path)`.

---

## Namespace: `os`

The `os` namespace provides process control, command-line arguments, environment variable access, and host kernel identification.

### Host Detection Properties

All platform inspection properties are pure query values evaluated at runtime:

```pith
val identifyKernel: string
```
Returns a static string literal identifying the host kernel: `"linux"`, `"darwin"`, `"nt"`, `"freebsd"`, or `"unknown"`.

```pith
val identifyKernelVersion: string
```
Returns the operating system kernel release string. On POSIX systems, this reads from `utsname.release` via `uname(2)`.

```pith
val isLinux: int     # 1 on Linux kernels, 0 otherwise
val isDarwin: int    # 1 on any Darwin kernel (uname = Darwin), 0 otherwise
val isMacOS: int     # 1 specifically on Apple macOS (__APPLE__ && __MACH__), 0 otherwise
val isNT: int        # 1 on Windows NT, 0 otherwise
val isFreeBSD: int   # 1 on FreeBSD kernels, 0 otherwise
```

---

## Namespace: `proc`

The `proc` namespace provides process control, command-line arguments, environment variable inspection, and process lifecycle primitives.

### Properties

```pith
val pid: int
```
Returns the unique process identifier of the current calling process.
- **Underlying Syscalls**:
  - POSIX: `getpid(2)`.
  - Windows: `GetCurrentProcessId()`.

```pith
val argCount: int
```
Returns the number of command-line arguments passed to the script (equivalent to `argc - 1` from the interpreter, or full `argc` for standalone compiled binaries). Can also be invoked as a function `proc.argCount()`.

### Functions

```pith
fn getEnv(name: string) string
```

Queries the environment list for the key `name`.

- **Return value**:
  - Value associated with `name` as a string.
  - Empty string `""` if the key is not found in the environment block.
- **Underlying Syscalls**:
  - POSIX: `getenv(3)`.
  - Windows: `GetEnvironmentVariableA`.

```pith
fn getArg(index: int) string
```

Returns the argument at 0-based `index`.

- **Return value**:
  - String argument value.
  - Returns an empty string `""` if `index < 0` or `index >= argCount()`.

```pith
fn sleep(ms: int) void
```

Suspends execution of the calling process for at least `ms` milliseconds.

- **Parameters**:
  - `ms`: Duration in milliseconds (non-negative).
- **Underlying Syscalls**:
  - POSIX: `nanosleep(2)`.
  - Windows: `Sleep(DWORD)`.

```pith
fn exit(code: int) void
```

Immediately terminates the calling process with status `code`.

- **Underlying Syscalls**:
  - Invokes C `exit(code)` / `_exit(2)`. Buffered file streams are flushed.

---

## Namespace: `net`

The `net` namespace provides minimalist, non-blocking-capable TCP socket networking primitives.

### Functions

```pith
fn socket(domain: int, type: int, protocol: int) int
```

Allocates a raw communication socket descriptor.

- **Parameters**:
  - `domain`: Address family (`2` for `AF_INET`, `10` for `AF_INET6`).
  - `type`: Socket semantics (`1` for `SOCK_STREAM`, `2` for `SOCK_DGRAM`).
  - `protocol`: Protocol number (`0` for IP default).
- **Return value**:
  - Socket integer descriptor (`>= 0`) on success.
  - `-1` on socket creation error.
- **Underlying Syscalls**:
  - POSIX: `socket(2)`.
  - Windows: `WSASocket` / `socket`.

```pith
fn connect(host: string, port: int) int
```

Resolves the destination `host` and establishes a TCP stream connection on `port`.

- **Parameters**:
  - `host`: IPv4/IPv6 address or domain hostname (e.g. `"127.0.0.1"`, `"example.com"`).
  - `port`: Destination port number (`1` to `65535`).
- **Return value**:
  - Connected socket descriptor (`>= 0`).
  - `-1` if resolution or connection establishment fails.
- **Underlying Syscalls**:
  - POSIX: `gethostbyname(3)` / `getaddrinfo(3)`, socket creation, and `connect(2)`.

```pith
fn send(fd: int, message: string) int
```

Transmits bytes over an open socket descriptor.

- **Parameters**:
  - `fd`: Connected socket descriptor.
  - `message`: Payload string to transmit.
- **Return value**:
  - Number of bytes sent (`>= 0`).
  - `-1` on write error or closed connection (`EPIPE`, `ECONNRESET`).
- **Underlying Syscalls**:
  - POSIX: `send(fd, buf, len, 0)` / `write(2)`.

```pith
fn recv(fd: int, maxBytes: int) string
```

Receives incoming stream bytes from `fd`.

- **Parameters**:
  - `fd`: Open socket descriptor.
  - `maxBytes`: Maximum buffer capacity to read in this call.
- **Return value**:
  - A newly allocated string containing received bytes.
  - Returns `""` on EOF (graceful remote shutdown) or socket read error.
- **Underlying Syscalls**:
  - POSIX: `recv(fd, buf, maxBytes, 0)` / `read(2)`.

```pith
fn close(fd: int) int
```

Closes the active socket descriptor and releases system resources.

- **Return value**:
  - `0` on success.
  - `-1` on error (`EBADF`).
- **Underlying Syscalls**:
  - POSIX: `close(2)`.
  - Windows: `closesocket`.

---

## Memory & ABI Specification

### PithValue Representation

All dynamic data (strings, objects) share an identical binary layout defined in `include/pith.h`:

```c
typedef struct PithValue PithValue;
struct PithValue {
    volatile uint32_t strongRefs;  /* Atomic reference counter */
    uint16_t          typeTag;     /* Type identifier          */
    uint16_t          flags;       /* Bit flags                */
    uint32_t          capacity;    /* Allocated byte capacity  */
    uint32_t          length;      /* Active byte length       */
    char              data[];      /* Contiguous byte payload  */
};
```

### Type Tags

| Constant | Value | Description |
|---|---|---|
| `PITH_TAG_INT` | `1` | 64-bit signed integer |
| `PITH_TAG_FLOAT` | `2` | 64-bit IEEE-754 floating-point |
| `PITH_TAG_STRING` | `3` | Refcounted, length-prefixed, NUL-terminated byte array |
| `PITH_TAG_BOOL` | `4` | 32-bit boolean value (`0` or `1`) |
| `PITH_TAG_OBJECT` | `5` | Refcounted record / dictionary pointer |

### Bit Flags

- **`PITH_FLAG_STATIC` (`0x01`)**: Marks immortal static data (e.g. string literals emitted directly into the `.data` ELF section). Calls to `pithRetain` and `pithRelease` are no-ops on static-flagged values.
- **`PITH_FLAG_SHARED` (`0x02`)**: Marks values managed across shared boundaries.

### Calling Convention & Lowering

1. **Intermediate Representation**:
   - Pith statements lower into QBE SSA intermediate representation.
   - Expressions evaluate in 64-bit registers: integer values use `l` (long, 64-bit), float values use `d` (double, 64-bit).
2. **Machine ABI**:
   - System V AMD64 ABI (Linux, FreeBSD, macOS).
   - Parameters passed in registers: `%rdi`, `%rsi`, `%rdx`, `%rcx`, `%r8`, `%r9`.
   - Floating-point parameters passed in `%xmm0` through `%xmm7`.
   - Callee-preserved registers: `%rbx`, `%rsp`, `%rbp`, `%r12`, `%r13`, `%r14`, `%r15`.
