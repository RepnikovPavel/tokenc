# Installing tokenc — for humans

### Requirements

- A C++17 compiler: `g++ >= 9` or `clang++ >= 9`
- `cmake >= 3.10`
- `make` (or `ninja`)
- `git` (only for the one-line install below)
- `pkg-config`, `libpcre2-dev`, `zlib1g-dev`, `xxd`

Ubuntu/Debian: `sudo apt install build-essential cmake git pkg-config libpcre2-dev zlib1g-dev xxd`
Fedora/RHEL:   `sudo dnf install gcc-c++ cmake make git pkg-config pcre2-devel zlib-devel vim-common`

### One-line install

```sh
curl -fsSL https://raw.githubusercontent.com/RepnikovPavel/tokenc/main/read_me_if_it_is_not_installed/install.sh | sh
```

This clones the repo, builds it, and installs `tokenc` to `/usr/local/bin/`
(prompting for `sudo` only if needed). It takes no arguments.

### Manual install

```sh
git clone https://github.com/RepnikovPavel/tokenc.git
cd tokenc
cmake -S . -B build
cmake --build build -j"$(nproc)"
sudo cmake --install build          # /usr/local/bin/tokenc
tokenc --version
```

### Verify

```sh
tokenc .
```

### Uninstall

```sh
sudo rm -f /usr/local/bin/tokenc
rm -rf "${XDG_CACHE_HOME:-$HOME/.cache}/tokenc"
```
