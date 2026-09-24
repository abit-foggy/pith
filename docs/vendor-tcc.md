# Vendored tcc

`tcc` lives at `vendor/tcc` as a **git submodule**, pinned to the
upstream `mob` branch. Pith uses it for two things:

1. `libtcc` — embedded in the pith binary (in-memory JIT execution
   bridge and built-in ELF linker for AOT builds).
2. `libtcc1.a` — the tcc runtime, needed by the JIT at relocate time.

## Provenance

- Upstream: https://repo.or.cz/tinycc.git (TinyCC)
- Branch: `mob`
- Pinned commit: `0fb54300b56512754221d80adda85ddb9815bceb`
  ("Make bound checking faster.")

## Working with the submodule

Clone with submodules:

    git clone --recurse-submodules <pith-url>

Existing checkout:

    git submodule update --init

Follow the upstream `mob` tip:

    git submodule update --remote vendor/tcc

## Upstreaming pith's changes

Any pith-specific patches under `vendor/tcc` should be kept small and
self-contained. To send work upstream:

    cd vendor/tcc
    git checkout -b pith-patches
    # apply/commit patches, test, then push to the mob branch
    # (upstream accepts work via the mob branch; see
    #  https://repo.or.cz/w/tinycc.git)

