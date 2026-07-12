# tokenc

Counts lines of source code in a directory tree, recursively. Counts **only
real code** — data, config and markup files (json, yaml, xml, md, lockfiles,
…) are never counted, and `.gitignore` rules plus a built-in non-code ignore
list (build dirs, `node_modules`, `__pycache__`, `*.o`, …) are respected.

Output is **machine-readable TSV**, designed to be parsed by scripts, agents
and LLMs:

```
$ tokenc .
language	files	lines	cl100k_base	o200k_base
C++	8	1325	11842	11796
C++ Header	7	335	2980	2955
Total	15	1660	14822	14751
```

**cl100k_base** (GPT-4) is counted by a **native C++** encoder ported from
[OpenAI tiktoken](https://github.com/openai/tiktoken) (`src/lib.rs`) — no
Python or pip at runtime. The `o200k_base` column is reserved (0 for now).
Use `--no-tokens` for lines-only output.

## Install

Requires a C++17 compiler (`g++ >= 9` or `clang++ >= 9`) and `cmake >= 3.10`.
Runtime deps for token columns: `libpcre2-8` and `zlib` (standard on Linux).

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
