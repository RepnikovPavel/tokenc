#!/usr/bin/env sh
# install.sh — build and install tokenc system-wide.
#
# Works two ways:
#   1. Piped from curl (standalone):  curl -fsSL <raw-url>/install.sh | sh
#      Clones the repo to a temp dir, builds, installs, removes the temp dir.
#   2. Run from inside a clone:       sh install.sh
#      Builds the current checkout in place.
#
# No arguments, no environment variables. Installs to /usr/local/bin/tokenc
# (uses sudo only if root is required).
#
# Requirements on the target machine: a C++17 compiler (g++ >= 9 or clang++
# >= 9), cmake >= 3.10, and make or ninja.
set -eu

REPO="https://github.com/RepnikovPavel/tokenc.git"
PREFIX="/usr/local"
BINDIR="$PREFIX/bin"

msg() { printf '==> %s\n' "$*"; }
err() { printf '==> ERROR: %s\n' "$*" >&2; }
die() { err "$*"; exit 1; }

# --- locate / fetch the source ------------------------------------------------
# If CMakeLists.txt sits next to this script, we are inside a checkout.
SCRIPT_DIR=$(cd "$(dirname "$0")" 2>/dev/null && pwd) || SCRIPT_DIR=""
if [ -n "$SCRIPT_DIR" ] && [ -f "$SCRIPT_DIR/CMakeLists.txt" ]; then
    SRC_DIR="$SCRIPT_DIR"
    msg "Building from: $SRC_DIR"
else
    # Standalone (e.g. curl | sh): clone to a temp directory.
    command -v git >/dev/null 2>&1 || die "git is required for remote install."
    SRC_DIR=$(mktemp -d)
    msg "Cloning $REPO into $SRC_DIR"
    trap 'rm -rf "$SRC_DIR"' EXIT
    git clone --quiet "$REPO" "$SRC_DIR"
fi

[ -f "$SRC_DIR/CMakeLists.txt" ] || die "CMakeLists.txt not found in $SRC_DIR"

# --- check prerequisites ------------------------------------------------------
command -v cmake >/dev/null 2>&1 || die "cmake is required but not found."
CXX="${CXX:-}"
[ -z "$CXX" ] && { command -v g++     >/dev/null 2>&1 && CXX=g++; }
[ -z "$CXX" ] && { command -v clang++ >/dev/null 2>&1 && CXX=clang++; }
[ -n "$CXX" ] || die "No C++17 compiler found (need g++ >= 9 or clang++ >= 9)."

# --- build --------------------------------------------------------------------
BUILD_DIR="$SRC_DIR/build"
msg "Configuring ($CXX)..."
cmake -S "$SRC_DIR" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release >/dev/null

JOBS=$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 2)
msg "Building with $JOBS jobs..."
cmake --build "$BUILD_DIR" --parallel "$JOBS" >/dev/null

BIN="$BUILD_DIR/tokenc"
[ -x "$BIN" ] || die "Build did not produce $BIN"
"$BIN" --version

# --- install ------------------------------------------------------------------
# Decide whether we can create/write the install dir without root.
writable_without_root() {
    [ "$(id -u)" -eq 0 ] && return 0
    if [ -d "$BINDIR" ]; then
        [ -w "$BINDIR" ] && return 0
        return 1
    fi
    # BINDIR doesn't exist yet: check the first existing ancestor.
    parent="$BINDIR"
    while [ "$parent" != "/" ] && [ ! -d "$parent" ]; do
        parent=$(dirname "$parent")
    done
    [ -w "$parent" ]
}

install_tokenc() {
    mkdir -p "$BINDIR"
    install -m 0755 "$BIN" "$BINDIR/tokenc"
}

if writable_without_root; then
    install_tokenc
else
    msg "Installing to $BINDIR requires root (using sudo)."
    sudo sh -c "BIN='$BIN' BINDIR='$BINDIR'; mkdir -p \"\$BINDIR\"; install -m 0755 \"\$BIN\" \"\$BINDIR/tokenc\""
fi

msg "Installed: $BINDIR/tokenc"
"$BINDIR/tokenc" --version

if command -v tokenc >/dev/null 2>&1; then
    msg "Done. Available on PATH as: $(command -v tokenc)"
else
    msg "Done. Note: $BINDIR is not on your PATH; add it or call it directly."
fi
