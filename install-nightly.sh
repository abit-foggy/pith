#!/bin/sh
#
# install-nightly.sh - nightly installer for The Pith Programming Language.
#
# Downloads the nightly release from GitHub and installs it under a
# prefix (default: ~/.local). Safe to re-run; overrides are honored.
#
#   curl -fsSL https://raw.githubusercontent.com/abit-foggy/pith/main/install-nightly.sh | sh
#
# Overrides:
#   PREFIX=/usr/local sh install-nightly.sh      install location (default ~/.local)
#   PITH_REPO=owner/pith sh install-nightly.sh   GitHub repository (default abit-foggy/pith)
#   PITH_TAG=nightly sh install-nightly.sh       install a specific release (default: nightly)
#
set -eu

REPO="${PITH_REPO:-abit-foggy/pith}"
PREFIX="${PREFIX:-${HOME}/.local}"
TAG="${PITH_TAG:-nightly}"

say() { printf 'install-nightly.sh: %s\n' "$*"; }
die() { printf 'install-nightly.sh: error: %s\n' "$*" >&2; exit 1; }

# ------------------------------------------------------------------ #
# prerequisites                                                      #
# ------------------------------------------------------------------ #

command -v curl >/dev/null 2>&1 || command -v wget >/dev/null 2>&1 || \
    die "neither curl nor wget is installed"

fetch() {
    # fetch <url> [output]
    _token="${GITHUB_TOKEN:-${GH_TOKEN:-}}"
    if command -v curl >/dev/null 2>&1; then
        if [ -n "$_token" ]; then
            if [ "$#" -eq 2 ]; then
                curl -fsSL -H "Authorization: Bearer $_token" -H "User-Agent: pith-installer" "$1" -o "$2"
            else
                curl -fsSL -H "Authorization: Bearer $_token" -H "User-Agent: pith-installer" "$1"
            fi
        else
            if [ "$#" -eq 2 ]; then
                curl -fsSL -H "User-Agent: pith-installer" "$1" -o "$2"
            else
                curl -fsSL -H "User-Agent: pith-installer" "$1"
            fi
        fi
    else
        if [ "$#" -eq 2 ]; then
            wget -qO "$2" "$1"
        else
            wget -qO- "$1"
        fi
    fi
}

# ------------------------------------------------------------------ #
# platform detection                                                 #
# ------------------------------------------------------------------ #

OS="$(uname -s)"
ARCH="$(uname -m)"

case "$OS:$ARCH" in
    Linux:x86_64)        TRIPLET="x86_64-linux" ;;
    Linux:aarch64)       TRIPLET="aarch64-linux" ;;
    Linux:arm64)         TRIPLET="aarch64-linux" ;;
    Darwin:x86_64)       TRIPLET="x86_64-darwin" ;;
    Darwin:arm64)        TRIPLET="aarch64-darwin" ;;
    FreeBSD:amd64)       TRIPLET="x86_64-freebsd" ;;
    FreeBSD:aarch64)     TRIPLET="aarch64-freebsd" ;;
    *) die "unsupported platform: $OS $ARCH (build from source instead)" ;;
esac

say "detected $OS ($ARCH) -> pith-$TRIPLET"

# ------------------------------------------------------------------ #
# toolchain prerequisites                                            #
# ------------------------------------------------------------------ #

# macOS (not generic Darwin): the Apple toolchain assembles and links;
# the Xcode command line utilities provide clang and the SDK. Offer to
# install them when missing (the installer prompts for the password).
if [ "$OS" = "Darwin" ]; then
    if ! xcode-select -p >/dev/null 2>&1; then
        say "the Xcode command line utilities are not installed"
        printf 'install-nightly.sh: install them now? [y/N] '
        read -r answer
        case "$answer" in
            [yY]*)
                say "running xcode-select --install (enter your password if prompted)"
                xcode-select --install || \
                    die "xcode-select --install failed; install the command line utilities manually"
                say "waiting for the installation to finish..."
                until xcode-select -p >/dev/null 2>&1; do
                    sleep 5
                done
                ;;
            *)
                die "the Xcode command line utilities are required; install them with: xcode-select --install"
                ;;
        esac
    fi
    command -v clang >/dev/null 2>&1 || \
        die "clang is not installed; please install clang (xcode-select --install)"
fi

# ------------------------------------------------------------------ #
# download + install                                                 #
# ------------------------------------------------------------------ #

say "installing $TAG from https://github.com/$REPO"

TMPDIR_PITH="$(mktemp -d 2>/dev/null || mktemp -d -t pith-install)"
trap 'rm -rf "$TMPDIR_PITH"' EXIT INT TERM

TARBALL="pith-$TRIPLET-nightly.tar.gz"
URL="https://github.com/$REPO/releases/download/$TAG/$TARBALL"
FALLBACK_TARBALL="pith-$TRIPLET.tar.gz"
FALLBACK_URL="https://github.com/$REPO/releases/download/$TAG/$FALLBACK_TARBALL"

say "downloading $TARBALL"
downloaded=0
if fetch "$URL" "$TMPDIR_PITH/$TARBALL" 2>/dev/null; then
    downloaded=1
elif fetch "$FALLBACK_URL" "$TMPDIR_PITH/$TARBALL" 2>/dev/null; then
    downloaded=1
elif command -v gh >/dev/null 2>&1; then
    if gh release download "$TAG" -R "$REPO" -p "$TARBALL" -O "$TMPDIR_PITH/$TARBALL" 2>/dev/null; then
        downloaded=1
    elif gh release download "$TAG" -R "$REPO" -p "$FALLBACK_TARBALL" -O "$TMPDIR_PITH/$TARBALL" 2>/dev/null; then
        downloaded=1
    fi
fi

[ "$downloaded" -eq 1 ] || \
    die "download failed: $URL (no asset for $TRIPLET in $TAG?)"

say "verifying + extracting"
tar -xzf "$TMPDIR_PITH/$TARBALL" -C "$TMPDIR_PITH"
[ -f "$TMPDIR_PITH/pith" ] ||
    die "archive is missing the pith binary"

mkdir -p "$PREFIX/bin" "$PREFIX/lib/pith/tcc" \
         "$PREFIX/lib/pith/runtime" "$PREFIX/include"

install -m 0755 "$TMPDIR_PITH/pith" "$PREFIX/bin/pith"

if [ -f "$TMPDIR_PITH/libtcc1.a" ]; then
    install -m 0644 "$TMPDIR_PITH/libtcc1.a" "$PREFIX/lib/pith/tcc/"
fi
if [ -f "$TMPDIR_PITH/libruntime.a" ]; then
    install -m 0644 "$TMPDIR_PITH/libruntime.a" \
        "$PREFIX/lib/pith/runtime/"
fi
for h in "$TMPDIR_PITH"/include/*.h; do
    [ -e "$h" ] && install -m 0644 "$h" "$PREFIX/include/"
done

# ------------------------------------------------------------------ #
# PATH hint                                                          #
# ------------------------------------------------------------------ #

case ":$PATH:" in
    *":$PREFIX/bin:"*) ;;
    *)
        say "NOTE: $PREFIX/bin is not in your PATH - add it:"
        say "  echo 'export PATH=\"$PREFIX/bin:\$PATH\"' >> ~/.profile"
        ;;
esac

"$PREFIX/bin/pith" version ||
    die "installed binary failed to run"

say "installed pith $($PREFIX/bin/pith version 2>/dev/null | awk '{print $2}') to $PREFIX/bin/pith"
say "next: pith run yourscript.pi   (needs qbe and GNU as in PATH)"
