#!/bin/sh
set -e

# scripts/test_darwin.sh - Test Pith Darwin Mach-O cross-compilation

TMP_DIR=$(mktemp -d /tmp/pith_darwin_XXXXXX)
trap 'rm -rf "$TMP_DIR"' EXIT

echo "--> Decompiling tests/test_os_net.pi to QBE SSA IR..."
./pith decompile tests/test_os_net.pi > "$TMP_DIR/test_darwin.ssa"

echo "--> Generating Darwin x86_64 assembly (QBE amd64_apple)..."
./vendor/qbe/qbe -t amd64_apple "$TMP_DIR/test_darwin.ssa" -o "$TMP_DIR/test_darwin_x86_64.s"

echo "--> Generating Darwin ARM64 assembly (QBE arm64_apple)..."
./vendor/qbe/qbe -t arm64_apple "$TMP_DIR/test_darwin.ssa" -o "$TMP_DIR/test_darwin_arm64.s"

echo "--> Assembling Mach-O 64-bit x86_64 object..."
clang -target x86_64-apple-darwin -c "$TMP_DIR/test_darwin_x86_64.s" -o "$TMP_DIR/test_darwin_x86_64.o"

echo "--> Assembling Mach-O 64-bit ARM64 object..."
clang -target arm64-apple-darwin -c "$TMP_DIR/test_darwin_arm64.s" -o "$TMP_DIR/test_darwin_arm64.o"

echo "--> Inspecting Mach-O binaries:"
file "$TMP_DIR/test_darwin_x86_64.o"
file "$TMP_DIR/test_darwin_arm64.o"

if [ -x /usr/lib/llvm-18/bin/llvm-otool ]; then
    echo "--> Verifying Darwin Mach-O headers via llvm-otool:"
    /usr/lib/llvm-18/bin/llvm-otool -hv "$TMP_DIR/test_darwin_x86_64.o"
    /usr/lib/llvm-18/bin/llvm-otool -hv "$TMP_DIR/test_darwin_arm64.o"
fi

if [ -x /usr/lib/llvm-18/bin/llvm-lipo ]; then
    echo "--> Packaging Universal 2 Fat Mach-O binary via llvm-lipo..."
    /usr/lib/llvm-18/bin/llvm-lipo -create "$TMP_DIR/test_darwin_x86_64.o" "$TMP_DIR/test_darwin_arm64.o" \
        -output "$TMP_DIR/test_darwin_universal.o"
    file "$TMP_DIR/test_darwin_universal.o"
fi

echo "--> Darwin Mach-O compilation complete: all checks passed!"
