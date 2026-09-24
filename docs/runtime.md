# Runtime & Memory Model

Pith uses **deterministic Automated Reference Counting** (ARC) — zero
tracing garbage collectors, zero safepoint stops, zero hidden
runtime bloat.

## PithValue

Every pith value carries an inline 32-bit atomic reference count and
a type discriminator. The canonical definition lives in
`include/pith.h`:

```c
typedef struct PithValue PithValue;
struct PithValue {
    volatile uint32_t strongRefs;  /* inline 32-bit reference count */
    uint16_t typeTag;              /* type discriminator            */
    uint16_t flags;                /* PITH_FLAG_* bits              */
    uint32_t capacity;             /* payload bytes allocated       */
    uint32_t length;               /* payload bytes in use          */
    char data[];                   /* payload (strings)             */
};
```

- **32-bit atomic refcounts** — updated with C11 `<stdatomic.h>` when
  available, GCC/Clang `__sync` builtins otherwise. Thread-safe.
- **Type discriminators** — `PITH_TAG_INT`, `PITH_TAG_FLOAT`,
  `PITH_TAG_STRING`, `PITH_TAG_BOOL`, `PITH_TAG_OBJECT`.
- **Flags** — `PITH_FLAG_STATIC` marks immortal static data (string
  literals); `PITH_FLAG_SHARED` marks values involved in a broken
  cycle.

## Reference counting semantics

| Event | Emitted instruction |
|---|---|
| Declaration (`name = "heap" + "-string"`) | `alloc8` + `storel` |
| Scope exit (`end`) | `loadl` + `call $pith_release` |
| Reassignment (ARC value overwritten) | `loadl` + `call $pith_release` on the old value |
| Borrow-to-own (`y = x` where `x` is ARC) | `call $pith_retain` on the new owner |
| Foreign return (`PithValue*` from C) | ownership transfers to the variable (+1 from C) |

The compiler emits these deterministically at every scope boundary —
there is no runtime collector, no safepoint, no pause.

## Static data

String literals compile to QBE `data` definitions carrying the
`PithValue` header with `PITH_FLAG_STATIC` set:

```qbe
data $str.1 = { w 1, h 3, h 1, w 6, w 5, b "linux", b 0 }
```

- `w 1` — refcount (irrelevant for statics)
- `h 3` — typeTag = PITH_TAG_STRING
- `h 1` — flags = PITH_FLAG_STATIC
- `w 6` — capacity (length + NUL)
- `w 5` — length

`pithRetain` and `pithRelease` are **no-ops** on static-flagged values
— they are immortal and never freed.

## Cycle mitigation

`pith_break_cycle(parent, child)` drops the parent's strong reference
to the child cleanly, prior to scope exit:

```c
void pith_break_cycle(void *parent, void *child);
```

The caller zeroes the back-reference slot in the parent's payload
first, then hands the pair here. The runtime drops the reference and
flags the child `PITH_FLAG_SHARED` for diagnostics.

## Runtime primitives

From `include/api.h`:

| Function | Description |
|---|---|
| `pith_str_new(initial, len)` | Allocate a string value (+1 ref) |
| `pith_str_concat(a, b)` | New concatenated string (+1 ref) |
| `pith_str_equals(a, b)` | Content equality (returns 0/1) |
| `pith_retain(ptr)` | Atomically bump the refcount |
| `pith_release(ptr)` | Atomically drop a ref; frees at zero |
| `pith_break_cycle(parent, child)` | Explicit cycle break |
| `pith_rt_os_kernel()` | Platform string ("linux", "nt", etc.) |
| `pith_rt_os_kernel_version()` | Kernel/OS version string |
| `pith_rt_is_nt()` | 1 on Windows NT, 0 elsewhere |
| `pith_rt_print(str)` | Print a string value to stdout |

## Thread safety

Reference counts are updated atomically (32-bit). Shared values are
safe to retain/release from multiple threads. The compiler-injected
releases remain deterministic — they always happen at scope
boundaries in the owning thread.
