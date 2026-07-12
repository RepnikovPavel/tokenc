#!/usr/bin/env sh
# install.sh — build and install tokenc system-wide.
#
#   curl -fsSL https://raw.githubusercontent.com/RepnikovPavel/tokenc/main/read_me_if_it_is_not_installed/install.sh | sh
#   sh read_me_if_it_is_not_installed/install.sh   # from a clone
#
set -eu

REPO="https://github.com/RepnikovPavel/tokenc.git"
PREFIX="/usr/local"
BINDIR="$PREFIX/bin"
BRANCH="${TOKENC_BRANCH:-main}"

msg() { printf '==> %s\n' "$*"; }
step() { printf '\n[%s] %s\n' "$1" "$2"; }
err() { printf '==> ERROR: %s\n' "$*" >&2; }
die() { err "$*"; exit 1; }

STEP=0
next_step() { STEP=$((STEP + 1)); step "$STEP" "$1"; }

# --- locate / fetch the source ------------------------------------------------
SCRIPT_DIR=$(cd "$(dirname "$0")" 2>/dev/null && pwd) || SCRIPT_DIR=""
if [ -n "$SCRIPT_DIR" ] && [ -f "$SCRIPT_DIR/../CMakeLists.txt" ]; then
    SRC_DIR=$(cd "$SCRIPT_DIR/.." && pwd)
    msg "Building from checkout: $SRC_DIR"
else
    command -v git >/dev/null 2>&1 || die "git is required for remote install."
    SRC_DIR=$(mktemp -d)
    msg "Temp build dir: $SRC_DIR"
    trap 'rm -rf "$SRC_DIR"' EXIT
    next_step "Cloning $REPO (branch $BRANCH)"
    git clone --progress --depth 1 --branch "$BRANCH" "$REPO" "$SRC_DIR" || die "git clone failed"
fi

[ -f "$SRC_DIR/CMakeLists.txt" ] || die "CMakeLists.txt not found in $SRC_DIR"
[ -f "$SRC_DIR/data/cl100k_vocab.z" ] || die "data/cl100k_vocab.z missing (corrupt checkout?)"

# --- prerequisites ------------------------------------------------------------
next_step "Checking build tools"
command -v cmake >/dev/null 2>&1 || die "cmake is required (apt: build-essential cmake pkg-config)"
command -v pkg-config >/dev/null 2>&1 || die "pkg-config is required"
command -v xxd >/dev/null 2>&1 || die "xxd is required (apt: xxd or vim-common)"

pkg-config --exists libpcre2-8 2>/dev/null || die "libpcre2-8 not found (apt: libpcre2-dev)"
pkg-config --exists zlib 2>/dev/null || die "zlib not found (apt: zlib1g-dev)"

CXX="${CXX:-}"
[ -z "$CXX" ] && { command -v g++ >/dev/null 2>&1 && CXX=g++; }
[ -z "$CXX" ] && { command -v clang++ >/dev/null 2>&1 && CXX=clang++; }
[ -n "$CXX" ] || die "No C++17 compiler (need g++ >= 9 or clang++ >= 9)"

msg "cmake: $(cmake --version | head -n1)"
msg "compiler: $($CXX --version | head -n1)"
msg "pcre2: $(pkg-config --modversion libpcre2-8)"
msg "zlib: $(pkg-config --modversion zlib)"

# --- build --------------------------------------------------------------------
BUILD_DIR="$SRC_DIR/build"
JOBS=$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 2)

next_step "CMake configure (Release)"
cmake -S "$SRC_DIR" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release

next_step "Compiling tokenc ($JOBS parallel jobs)"
cmake --build "$BUILD_DIR" --parallel "$JOBS"

BIN="$BUILD_DIR/tokenc"
[ -x "$BIN" ] || die "Build did not produce $BIN"

next_step "Smoke test"
"$BIN" --version

# --- install ------------------------------------------------------------------
writable_without_root() {
    [ "$(id -u)" -eq 0 ] && return 0
    if [ -d "$BINDIR" ]; then
        [ -w "$BINDIR" ] && return 0
        return 1
    fi
    parent="$BINDIR"
    while [ "$parent" != "/" ] && [ ! -d "$parent" ]; do
        parent=$(dirname "$parent")
    done
    [ -w "$parent" ]
}

next_step "Installing to $BINDIR/tokenc"
if writable_without_root; then
    mkdir -p "$BINDIR"
    install -m 0755 "$BIN" "$BINDIR/tokenc"
else
    msg "Need root for $BINDIR (running sudo)..."
    sudo sh -c "mkdir -p '$BINDIR' && install -m 0755 '$BIN' '$BINDIR/tokenc'"
fi

"$BINDIR/tokenc" --version
if command -v tokenc >/dev/null 2>&1; then
    msg "Done. tokenc is on PATH: $(command -v tokenc)"
else
    msg "Done. Binary: $BINDIR/tokenc (add $BINDIR to PATH if needed)"
fi