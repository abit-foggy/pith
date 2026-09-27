#!/bin/sh
set -e

# scripts/test_wine.sh - Test Pith Windows compilation and execution under Wine

if ! command -v wine >/dev/null 2>&1; then
    echo "test-wine: wine not found in PATH" >&2
    exit 1
fi

WIN_TCC="vendor/tcc/x86_64-win32-tcc"
if [ ! -x "$WIN_TCC" ]; then
    echo "test-wine: $WIN_TCC not found, building..."
    (cd vendor/tcc && make cross)
fi

WIN_LIBTCC1="vendor/tcc/x86_64-win32-libtcc1.a"
if [ ! -f "$WIN_LIBTCC1" ]; then
    echo "test-wine: $WIN_LIBTCC1 not found, building..."
    (cd vendor/tcc && make cross)
fi

TMP_DIR=$(mktemp -d /tmp/pith_wine_XXXXXX)
trap 'rm -rf "$TMP_DIR"' EXIT

echo "--> Compiling Windows runtime objects..."
"$WIN_TCC" -c runtime/memory.c -o "$TMP_DIR/memory_win.o" \
    -Iinclude -Ivendor/tcc/include -Ivendor/tcc/win32/include -Ivendor/tcc/win32/include/winapi
"$WIN_TCC" -c runtime/os_fs.c -o "$TMP_DIR/os_fs_win.o" \
    -Iinclude -Ivendor/tcc/include -Ivendor/tcc/win32/include -Ivendor/tcc/win32/include/winapi

echo "--> Decompiling tests/test_wine.pi to QBE SSA IR..."
./pith decompile tests/test_wine.pi > "$TMP_DIR/test_wine.ssa"

echo "--> Lowering SSA to Windows x86_64 assembly (QBE amd64_win)..."
./vendor/qbe/qbe -t amd64_win "$TMP_DIR/test_wine.ssa" -o "$TMP_DIR/test_wine.s"

echo "--> Assembling and linking Windows PE32+ executable..."
"$WIN_TCC" -c "$TMP_DIR/test_wine.s" -o "$TMP_DIR/test_wine.o"
"$WIN_TCC" -Ivendor/tcc/include -Ivendor/tcc/win32/include -Ivendor/tcc/win32/include/winapi \
    -Lvendor/tcc/win32/lib -Lvendor/tcc \
    -o "$TMP_DIR/test_wine.exe" \
    "$TMP_DIR/test_wine.o" "$TMP_DIR/memory_win.o" "$TMP_DIR/os_fs_win.o" "$WIN_LIBTCC1"

echo "--> Executable details:"
file "$TMP_DIR/test_wine.exe"

echo "--> Executing under Wine..."
WINEDEBUG=-all wine "$TMP_DIR/test_wine.exe"

echo "--> Wine testing complete: all checks passed!"
