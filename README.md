# tokenc

Counts lines of source code in a directory tree, recursively. Counts **only
real code** — data, config and markup files (json, yaml, xml, md, lockfiles,
…) are never counted, and `.gitignore` rules plus a built-in non-code ignore
list (build dirs, `node_modules`, `__pycache__`, `*.o`, …) are respected.

Output is **machine-readable TSV**, designed to be parsed by scripts, agents
and LLMs:

```
$ tokenc .
language	files	lines
C++	8	1325
C++ Header	7	335
Total	15	1660
```

## Install

Requires a C++17 compiler (`g++ >= 9` or `clang++ >= 9`) and `cmake >= 3.10`.

```sh
git clone https://github.com/RepnikovPavel/tokenc.git
cd tokenc
cmake -S . -B build && cmake --build build -j"$(nproc)"
sudo cmake --install build      # installs to /usr/local/bin/tokenc
```

One-line remote install, Docker-image install, and an agent-facing guide are in
[`read_me_if_it_is_not_installed/`](read_me_if_it_is_not_installed/).

## Usage

```sh
tokenc                 # count the current directory
tokenc /some/folder    # count a specific folder
tokenc --help
```

Flags: `--no-gitignore` · `--no-default-ignore` · `--no-cache` · `-j N`.

## For AI agents

Feed agents [`prompt.txt`](prompt.txt) — a minimal, low-token brief of what
tokenc does and how to call it. Project structure is intentionally
self-describing:

- [`dont_read_me_src/`](dont_read_me_src) — the source code (no need to read it).
- [`if_necessary_you_can_read_me/`](if_necessary_you_can_read_me) — architecture notes.
- [`read_me_if_it_is_not_installed/`](read_me_if_it_is_not_installed) — install guides.

## License

0BSD — do whatever you want. See [LICENSE](LICENSE).
