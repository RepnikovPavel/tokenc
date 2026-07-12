# Architecture

You only need this if you want to understand how tokenc works internally. For
normal use, see `../prompt.txt` instead.

## Goal

Count physical lines and OpenAI token totals (cl100k_base, o200k_base) for
source trees. C++ core has no runtime deps beyond glibc; token columns use an
optional `python3` + `tiktoken` helper (`scripts/tokenc_tokenize.py`).

## Module map (all under `../dont_read_me_src/`)

| File             | Responsibility                                              |
|------------------|-------------------------------------------------------------|
| `main.cpp`       | CLI parsing, orchestration, exit codes.                     |
| `languages.*`    | Extension → language table; built-in default ignore list.   |
| `fs.*`           | POSIX `stat`/`lstat`/path helpers; XDG cache dir.           |
| `gitignore.*`    | git-compatible pattern parser + matcher (`*`,`?`,`[...]`,`**`,`/`,`!`). |
| `walker.*`       | Recursive directory walk; composes nested ignore layers.    |
| `counter.*`      | `mmap`-based line counting across a thread pool.             |
| `cache.*`        | On-disk cache keyed on `(dev, ino, size, mtime_ns)`.        |
| `tokenize.*`     | Batch tokenization via tiktoken helper script.              |
| `output.*`       | TSV rendering (header + per-language rows + Total).         |

## Data flow

```
walk() ──> [FoundFile list] ──> count_all() ──> [LanguageStats] ──> render_tsv()
  ^                                ^
  |  applies ignore layers         |  consults LineCache, stores fresh counts
  |  (default + gitignore)         |  flushes atomically at the end
```

## Ignore layering (applied innermost-wins, like git)

1. **Code filter** — only recognized source extensions are considered at all.
2. **Built-in defaults** (`--no-default-ignore` to disable) — VCS/dep/build dirs,
   compiled artifacts, minified bundles, lock files, IDE folders.
3. **Virtualenv markers** — any directory containing `pyvenv.cfg` or
   `conda-meta/` is treated as a Python environment and skipped wholesale,
   regardless of its name. This catches `venv`, `.venv`, `env`, and any
   custom-named env that the name list above would miss.
4. **Ignore files** (`--no-gitignore` to disable) — `.gitignore`, `.ignore`,
   `.dockerignore`, `.npmignore`, `.rgignore`, read at every directory level
   and composed exactly like git does, including `!` negation.

Symlinks are never followed, so files outside the scanned tree are never pulled
in (no loops, no generated outputs counted).

## Counting

A "line" is a run of bytes terminated by `\n`; a final line without a trailing
`\n` still counts. Files are `mmap`-ed and scanned with `memchr` for `'\n'` in a
thread pool. This counts ~800k lines across ~2300 files in well under 100 ms.

## Cache

`$XDG_CACHE_HOME/tokenc/linecache.tsv` (falls back to `~/.cache/tokenc/`).
Plain text, one record per line, atomic write (temp + rename). Safe to delete
at any time. Keyed on file identity, so an unchanged file is never re-read.

## Why no C++20 / no dependencies

C++17 + g++ 9 builds on the oldest mainstream Linux distros. The C++ runtime is
linked statically, so the binary depends only on glibc. No fmt, no CLI11, no
std::filesystem — only the standard library and POSIX.
