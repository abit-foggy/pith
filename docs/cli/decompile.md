# pith decompile

Two modes, selected by the file extension.

## IR mode (`.pi` input)

```sh
pith decompile <file.pi> [more.pi ...]
```

Prints the generated QBE SSA IL to stdout. Useful for inspecting what
the compiler produces:

```sh
pith decompile hello.pi
```

```qbe
data $str.1 = { w 1, h 3, h 1, w 6, w 5, b "pith", b 0 }
data $str.2 = { w 1, h 3, h 1, w 8, w 7, b "hello, ", b 0 }
data $str.3 = { w 1, h 3, h 1, w 2, w 1, b "!", b 0 }

export function w $main() {
@main.start
    %.v1_name =l alloc8 8
    storel $str.1, %.v1_name
    %.t1 =l loadl %.v1_name
    %.t2 =w call $pith_str_equals(l %.t1, l $str.1)
    jnz %.t2, @L2, @L3
@L2
    %.t3 =l call $pith_str_concat(l $str.2, l %.t1)
    %.t4 =l call $pith_str_concat(l %.t3, l $str.3)
    call $pith_rt_print(l %.t4)
    call $pith_release(l %.t4)
    call $pith_release(l %.t3)
    jmp @L1
@L3
@L1
@main.exit
    ret 0
}
```

## Binary mode (workspace restore)

```sh
pith decompile <binary>
```

Unpacks an embedded debug workspace from a binary built with
`--embed-source`:

```
1. Open the executable in binary read mode.
2. Seek to SEEK_END - 16.
3. Read the 16-byte PithDebugFooter trailer.
4. Check the magic ("PITHDEBG").
5. Read payload_size, seek backward by 16 + payload_size.
6. Extract the archived files into ./restored_workspace/.
```

```
pith decompile ./myapp
# restored workspace successfully extracted to ./restored_workspace/
```

If the binary has no embedded payload (was built without
`--embed-source`), prints:

```
error: binary contains no embedded debug workspace payload.
```

and exits with status 1.

### Security

The tar extraction is hardened against:
- **Path traversal** (`../../` components) — rejected before any
  filesystem access
- **Absolute paths** (`/etc/passwd`) — rejected
- **Empty path components** (`a//b`) — rejected
- **Corrupt headers** (missing ustar magic) — rejected
- **Implausibly huge declared sizes** — rejected against the actual
  buffer
- **Truncated blocks** — handled gracefully, nothing written
