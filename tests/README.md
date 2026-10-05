# Pith verification suite

The adversarial test suite run by `make check`. Every deliverable
lists its category tag and expected behavior. Clean-exit tests must
exit 0; error-expectation tests must exit non-zero with a diagnostic
(and never a segfault or malformed QBE IL); harness tests assert exact
runtime semantics.

## Core pipeline

| Path | Category | Expected behavior |
|---|---|---|
| `test_audit.pi` | [CODEGEN] | exit 0; correct platform branch, ARC releases at `end` |
| `test_multi_extra.pi` | [CODEGEN] | multi-file build: both units in one `.ssa`, unreferenced private fn eliminated |
| `test_sweep.pi` | [CODEGEN] | exit 0; arithmetic, parens, negation, float promotion, camelCase lint warning |
| `test_arc.pi` | [ARC] | exit 0; reassignment/copy retain-release balance, scoped strings |
| `test_fn.pi` | [CODEGEN] | exit 0; fn decls lower to private functions, return path verified |
| `test_err1.pi` | [PARSER] | compile error (undeclared identifier) |
| `test_err2.pi` | [PARSER] | compile error (string + integer concatenation) |
| `test_str.pi` | [STDLIB] | exit 0; `str.*` builtin namespace (`length`, `contains`, `startsWith`, `endsWith`, `upper`, `lower`), ASCII semantics, ARC-clean returns |
| `build --object` | [BUILD] | `pith build <file.pi> --object -o <file.o>` writes raw plugin-mode object without `.ppkg` tar wrapper, symbols exported as `c_<author>_<mod>_<fn>` |
| embed/decompile roundtrip | [PACKAGE] | default build stripped of `PITHDEBG`; `--embed-source` attaches it; `pith decompile <bin>` restores `./restored_workspace/` |

## DOMAIN 1 - ARC runtime & memory lifetimes

| Path | Category | Expected behavior |
|---|---|---|
| `arc_deep.pi` | [ARC] | 12 nested scopes, heap string at every boundary: exit 0, outer binding alive after unwinding, zero leaks under ASan |
| `arc_selfassign.pi` | [ARC] | `x = x`, aliasing, self-then-extend, replace-after-self: exit 0, no double-free (retain-before-release ordering), zero leaks |
| `harness/arc_stress.c` | [ARC] | 10^6 create/retain/release/concat iterations + 10^5 alias transfers: balanced refcounts at every step, no heap exhaustion, zero leaks under ASan |
| `harness/arc_cycle.c` | [ARC] | `pith_break_cycle` drops the parent's strong ref without freeing a still-owned child, flags `IS_SHARED`: exit 0, zero leaks |

v0.1 has no loop or call syntax, so high-frequency churn and cycle
semantics are driven from C harnesses against the same runtime the
compiler emits calls into; scope-exit unwinding is covered in Pith
(`arc_deep.pi`). Graph structures are not yet expressible in the
language - `pith_break_cycle` is exercised at the ABI level.

## DOMAIN 2 - QBE codegen & calling conventions

| Path | Category | Expected behavior |
|---|---|---|
| `codegen_int64.pi` | [CODEGEN] | INT64_MAX+1 wraps to INT64_MIN, signed compares hold across the boundary, boundary round trips: exit 0, no BROKEN |
| `codegen_branches.pi` | [CODEGEN] | empty then/elseif/else blocks, nested chains, dead code after `return` (diagnosed, not emitted): exit 0 |
| `codegen_div0.pi` | [CODEGEN] | division by an opaque zero must die by deterministic SIGFPE (no hang, no wrong result) - verified by `harness/div0.c` |
| `harness/div0.c` | [CODEGEN] | forks + execs `pith run codegen_div0.pi`, asserts the child died by exactly SIGFPE: exit 0 |
| `ffi/math.c` + `test_ffi.pi` | [FFI] | every ABI class (w/l/s/d), string borrows, owned string returns (+1 ref), void calls with internal state, round trips: exit 0 on BOTH the JIT and AOT paths, zero leaks under ASan |

Adversarial findings documented here:
- The naive `z = 0; x = 1 / z` does **not** trap: QBE's load/store
  forwarding promotes the zero to a constant and folds the division
  away. The divisor in `codegen_div0.pi` is made opaque via a
  runtime-call-dependent conditional assignment.
- v0.1 has no structs, `&&`/`||`, or shifts; aggregate ABI and
  short-circuit tests land with the language features they target.

## DOMAIN 3 - parser & lexer torture

| Path | Category | Expected behavior |
|---|---|---|
| `lexer_strings.pi` | [PARSER] | escapes, UTF-8 payloads, embedded-NUL length semantics: exit 0, byte-exact comparisons |
| `lexer_nul.pi` | [PARSER] | `"a\0b"` stays three payload bytes through concat and equality (no truncation) |
| `lexer_unterminated.pi` | [PARSER] | compile error pointing at the opening quote |
| `lexer_ident_nonascii.pi` | [PARSER] | compile error: identifiers are ASCII lowerCamelCase only |
| `lexer_int_overflow.pi` | [PARSER] | compile error: literal beyond INT64_MAX is out of range |
| `gen_deep_blocks.pi` (generated) | [PARSER] | 200 nested blocks: exit 0 (within the 256 C-stack guard) |
| `gen_deep_over.pi` (generated) | [PARSER] | 400 nested blocks: clean compile error, not a stack overflow |
| `gen_deep_parens.pi` (generated) | [PARSER] | 200-deep parentheses: clean compile error from the 128 expression-depth guard |

## DOMAIN 4 - tar, VFS & package isolation

| Path | Category | Expected behavior |
|---|---|---|
| `harness/tar_security.c` | [PACKAGE] | tar slip (`../../`, absolute paths, empty/`..` components), corrupt ustar magic, huge declared sizes, truncated headers: all rejected before touching the filesystem; non-ASCII names and valid entries extract byte-exactly; nothing ever escapes the destination root |

Hardening added alongside these tests (in `src/tar.c`): ustar magic
verification, path-component validation before any filesystem access,
and declared-size sanity against the actual buffer.

## DOMAIN 5 - embeddable C ABI

| Path | Category | Expected behavior |
|---|---|---|
| `harness/embed_host.c` | [EMBED] | typed host registration (`pith_register_ns_fn`): a zero-arg pseudo-constant, `w`/`l` calls, a `v` statement call, and an owned `p` string return all answer correctly from evaluated pith source; the script exits 42. Transparently exercises BOTH execution backends: in-memory tcc (registered addresses) and the temp-executable fallback (`pith_register_link_object` supplies the renamed `c_host_*` symbols to the child process) |
