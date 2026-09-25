#!/bin/sh
#
# install.sh — installer for The Pith Programming Language.
#
# Downloads the latest release from GitHub and installs it under a
# prefix (default: ~/.local). Safe to re-run; overrides are honored.
#
#   curl -fsSL https://raw.githubusercontent.com/abit-foggy/pith/main/install.sh | sh
#
# Overrides:
#   PREFIX=/usr/local sh install.sh      install location (default ~/.local)
#   PITH_REPO=owner/pith sh install.sh   GitHub repository (default abit-foggy/pith)
#   PITH_TAG=v0.1.0 sh install.sh        install a specific release
#                                        (default: the latest tag)
#
set -eu

REPO="${PITH_REPO:-abit-foggy/pith}"
PREFIX="${PREFIX:-${HOME}/.local}"
TAG="${PITH_TAG:-}"

say() { printf 'install.sh: %s\n' "$*"; }
die() { printf 'install.sh: error: %s\n' "$*" >&2; exit 1; }

# ------------------------------------------------------------------ #
# prerequisites                                                      #
# ------------------------------------------------------------------ #

command -v curl >/dev/null 2>&1 || command -v wget >/dev/null 2>&1 || \
    die "neither curl nor wget is installed"

fetch() {
    # fetch <url> [output]
    if command -v curl >/dev/null 2>&1; then
        if [ "$#" -eq 2 ]; then
            curl -fsSL "$1" -o "$2"
        else
            curl -fsSL "$1"
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
        printf 'install.sh: install them now? [y/N] '
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
# latest release resolution                                          #
# ------------------------------------------------------------------ #

if [ -z "$TAG" ]; then
    say "resolving the latest release of $REPO"
    TAG="$(fetch "https://api.github.com/repos/$REPO/releases/latest" |
        grep -o '"tag_name": *"[^"]*"' |
        sed 's/.*"tag_name": *"\([^"]*\)".*/\1/')" || TAG=""
    [ -n "$TAG" ] || die "no releases found for $REPO (has a release \
been published? build from source: https://github.com/$REPO)"
fi

say "installing $TAG from https://github.com/$REPO"

# ------------------------------------------------------------------ #
# download + install                                                 #
# ------------------------------------------------------------------ #

TMPDIR_PITH="$(mktemp -d 2>/dev/null || mktemp -d -t pith-install)"
trap 'rm -rf "$TMPDIR_PITH"' EXIT INT TERM

TARBALL="pith-$TRIPLET.tar.gz"
URL="https://github.com/$REPO/releases/download/$TAG/$TARBALL"

say "downloading $TARBALL"
fetch "$URL" "$TMPDIR_PITH/$TARBALL" ||
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
        say "NOTE: $PREFIX/bin is not in your PATH — add it:"
        say "  echo 'export PATH=\"$PREFIX/bin:\$PATH\"' >> ~/.profile"
        ;;
esac

"$PREFIX/bin/pith" version ||
    die "installed binary failed to run"

say "installed pith $($PREFIX/bin/pith version 2>/dev/null | awk '{print $2}') to $PREFIX/bin/pith"
say "next: pith run yourscript.pi   (needs qbe and GNU as in PATH)"
