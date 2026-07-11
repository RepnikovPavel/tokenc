#!/usr/bin/env bash
#
# install.sh — build and install tokenc as a system-wide command.
#
# This script is intended to be run from the root of a clone of the tokenc
# repository. It configures the project with CMake, compiles it with the
# available compiler, and installs the resulting binary into the system PATH
# (default /usr/local/bin).
#
# Requirements (checked below):
#   - a C++20 compiler: g++ >= 11 or clang++ >= 13
#   - cmake >= 3.16
#   - make or ninja (ninja is used if present, otherwise make)
#
# Environment variables:
#   PREFIX       install prefix (default: /usr/local)
#   BUILD_DIR    build directory (default: ./build)
#   BUILD_TYPE   CMAKE_BUILD_TYPE   (default: Release)
#   TOKENC_STATIC_STDLIB  ON/OFF static link of libstdc++/libgcc (default: ON)
#
set -euo pipefail

# --- Pretty logging ----------------------------------------------------------
if [ -t 1 ] && [ -z "${NO_COLOR:-}" ]; then
    BOLD=$'\033[1m'; GREEN=$'\033[32m'; YELLOW=$'\033[33m'; RED=$'\033[31m'; RESET=$'\033[0m'
else
    BOLD=''; GREEN=''; YELLOW=''; RED=''; RESET=''
fi
info()  { printf '%s==>%s %s\n' "${GREEN}" "${RESET}" "$*"; }
warn()  { printf '%s==> [%sWARNING%s]%s %s\n' "${BOLD}" "${YELLOW}" "${RESET}" "${RESET}" "$*" >&2; }
error() { printf '%s==> [%sERROR%s]%s %s\n' "${BOLD}" "${RED}" "${RESET}" "${RESET}" "$*" >&2; }
die()   { error "$*"; exit 1; }

# --- Locate the source directory ---------------------------------------------
# Resolve the directory of this script so it works from anywhere.
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

[ -f CMakeLists.txt ] || die "CMakeLists.txt not found in $SCRIPT_DIR — please run this script from the tokenc source directory."
[ -d src ]           || die "src/ directory not found — is this the tokenc repository?"

# --- Detect build tools ------------------------------------------------------
command -v cmake >/dev/null 2>&1 || die "cmake is required but was not found. Install it (e.g. 'sudo apt install cmake')."

CXX="${CXX:-}"
if [ -z "$CXX" ]; then
    if command -v g++ >/dev/null 2>&1; then CXX=g++
    elif command -v clang++ >/dev/null 2>&1; then CXX=clang++
    else die "No C++ compiler found. Install g++ (>= 11) or clang++ (>= 13)."
    fi
fi
command -v "$CXX" >/dev/null 2>&1 || die "C++ compiler '$CXX' not found."

# Validate C++20 support (g++ >= 11, clang++ >= 13).
cxx_version_line="$("$CXX" --version 2>/dev/null | head -1 || true)"
info "Compiler: $CXX ($cxx_version_line)"
info "cmake:    $(cmake --version | head -1)"

# --- Settings ----------------------------------------------------------------
PREFIX="${PREFIX:-/usr/local}"
BUILD_DIR="${BUILD_DIR:-./build}"
BUILD_TYPE="${BUILD_TYPE:-Release}"
STATIC="${TOKENC_STATIC_STDLIB:-ON}"
INSTALL_BINDIR="$PREFIX/bin"

# Prefer Ninja if available for faster parallel builds; fall back to Unix Makefiles.
GENERATOR=()
if command -v ninja >/dev/null 2>&1; then
    GENERATOR=(-G Ninja)
    info "Using Ninja."
else
    command -v make >/dev/null 2>&1 || die "Neither ninja nor make was found. Install one of them."
    GENERATOR=(-G "Unix Makefiles")
    info "Using GNU Make."
fi

# --- Configure ---------------------------------------------------------------
info "Configuring (BUILD_TYPE=$BUILD_TYPE, PREFIX=$PREFIX, STATIC_STDLIB=$STATIC)..."
cmake -S . -B "$BUILD_DIR" \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
    -DCMAKE_INSTALL_PREFIX="$PREFIX" \
    -DTOKENC_STATIC_STDLIB="$STATIC" \
    "${GENERATOR[@]}"

# --- Build -------------------------------------------------------------------
JOBS="$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 2)"
info "Building with $JOBS parallel jobs..."
cmake --build "$BUILD_DIR" --config "$BUILD_TYPE" --parallel "$JOBS"

BINARY="$BUILD_DIR/tokenc"
[ -x "$BINARY" ] || die "Build did not produce an executable at $BINARY."

info "Built: $BINARY"
"$BINARY" --version

# --- Install -----------------------------------------------------------------
# Decide whether we need elevated privileges to write into the install prefix.
# We need to be able to both create $INSTALL_BINDIR (and parents) and write
# inside it. We check the existing prefix path: if it already exists and is
# writable, no sudo; if it doesn't exist yet, check whether we can create it
# (i.e. its parent is writable).
needs_sudo=0
if [ "$(id -u)" -eq 0 ]; then
    needs_sudo=0
elif [ -d "$INSTALL_BINDIR" ]; then
    [ -w "$INSTALL_BINDIR" ] || needs_sudo=1
elif [ -d "$PREFIX" ]; then
    [ -w "$PREFIX" ] || needs_sudo=1
else
    # PREFIX doesn't exist either: walk up to the first existing ancestor and
    # test whether it is writable.
    parent="$PREFIX"
    while [ "$parent" != "/" ] && [ ! -d "$parent" ]; do
        parent="$(dirname "$parent")"
    done
    [ -w "$parent" ] || needs_sudo=1
fi

install_binary() {
    install -d "$INSTALL_BINDIR"
    install -m 0755 "$BINARY" "$INSTALL_BINDIR/tokenc"
}

if [ "$needs_sudo" -eq 1 ]; then
    warn "Installing into $INSTALL_BINDIR requires root privileges."
    sudo -v 2>/dev/null || die "Failed to obtain sudo privileges for installation."
    sudo bash -c "$(declare -f install_binary); INSTALL_BINDIR='$INSTALL_BINDIR'; BINARY='$BINARY'; install_binary"
else
    install_binary
fi

INSTALLED="$INSTALL_BINDIR/tokenc"
info "Installed to: $INSTALLED"
"$INSTALLED" --version

# --- Prepare the cache directory --------------------------------------------
CACHE_DIR="${XDG_CACHE_HOME:-$HOME/.cache}/tokenc"
mkdir -p "$CACHE_DIR"
info "Cache directory: $CACHE_DIR"

# --- Verify availability on PATH --------------------------------------------
if command -v tokenc >/dev/null 2>&1; then
    info "tokenc is available on PATH as: $(command -v tokenc)"
else
    warn "$INSTALL_BINDIR is not on your PATH."
    warn "Add it to your shell profile, e.g.:"
    warn "    export PATH=\"$INSTALL_BINDIR:\$PATH\""
fi

printf '\n%sDone.%s Try it:\n' "${GREEN}${BOLD}" "${RESET}"
printf '    tokenc .\n'
