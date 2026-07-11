# tokenc

`tokenc` is a small, fast command-line tool that recursively counts lines of
**source code** in a directory tree. It is built around a single principle:
count only the *useful payload* — actual code — and ignore everything else
(generated files, build output, dependencies, data, markup, config, and
anything listed in your `.gitignore`).

It is written in C++20 with **no third-party dependencies** (standard library +
POSIX only), ships as a single self-contained binary, and installs as a normal
system command.

## Why

- **Code only.** JSON, YAML, XML, TOML, Markdown, HTML, lock files and other
  data/markup are never counted, because they are not hand-written code.
- **Respects `.gitignore`.** A faithful re-implementation of git's pathspec
  matching (`*`, `?`, `[...]`, `**`, leading/trailing `/`, `!` negation,
  nested `.gitignore` files) is built in. Also honors `.ignore`,
  `.dockerignore`, `.npmignore` and `.rgignore`.
- **Sensible defaults.** A built-in ignore list skips obvious non-code:
  `.git/`, `node_modules/`, `__pycache__/`, `build/`, `dist/`, `target/`,
  `.venv/`, `*.o`, `*.class`, minified bundles, lock files, IDE folders, and
  many more.
- **Fast.** Files are read with `mmap` and counted in a thread pool. Counting
  ~800k lines across 2 300 files of the `git` source tree takes well under
  100 ms.
- **Minimal.** The binary links the C++ runtime statically and depends only on
  glibc — drop it on any modern Linux.
- **Cacheable.** A tiny on-disk cache (`$XDG_CACHE_HOME/tokenc/`) keys line
  counts on file identity `(dev, ino, size, mtime)`, so repeated runs on an
  unchanged tree are instant.

## Install

### From source (recommended)

You need a C++20 compiler (`g++ >= 11` or `clang++ >= 13`), `cmake >= 3.16`,
and `make` or `ninja`.

```sh
git clone https://github.com/RepnikovPavel/tokenc.git
cd tokenc
./install.sh
```

`install.sh` configures the build, compiles with all available CPU cores, and
copies the binary to `/usr/local/bin/tokenc` (asking for `sudo` only if
needed). A few environment variables customize the install:

| Variable                | Default     | Purpose                                     |
|-------------------------|-------------|---------------------------------------------|
| `PREFIX`                | `/usr/local`| Install prefix (`$PREFIX/bin/tokenc`)       |
| `BUILD_DIR`             | `./build`   | Where to build                              |
| `BUILD_TYPE`            | `Release`   | CMake build type                            |
| `TOKENC_STATIC_STDLIB`  | `ON`        | Statically link libstdc++/libgcc            |

Example — install into your home directory without `sudo`:

```sh
PREFIX="$HOME/.local" ./install.sh
```

### Manual build

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"
# optionally:
sudo install -m 0755 build/tokenc /usr/local/bin/tokenc
```

## Usage

```sh
tokenc [options] [path]
```

If `path` is omitted, the current directory is used.

```
$ tokenc .
  Language     Files   Lines   Lines %
--------------------------------------
  C++              8   1,630      82.4
  C++ Header       7     347      17.6
--------------------------------------
  Total           15   1,977     100.0

2 languages, 15 files, 1,977 lines  ·  1.7 ms
```

### Options

| Option                         | Description                                            |
|--------------------------------|--------------------------------------------------------|
| `path`                         | Directory or file to count (default: `.`)              |
| `--no-gitignore`               | Ignore `.gitignore`/`.ignore`/`.dockerignore` files    |
| `--no-default-ignore`          | Disable the built-in non-code ignore list              |
| `--no-cache`                   | Do not read or write the line cache                    |
| `--no-color`                   | Disable ANSI colors                                    |
| `--color`                      | Force ANSI colors on (e.g. when piping)                |
| `--sort=lines\|files\|name`    | Sort order (default: `lines`)                          |
| `-j, --jobs=N`                 | Worker threads (default: hardware concurrency)         |
| `-h, --help`                   | Show help and exit                                     |
| `-v, --version`                | Print version and exit                                 |

### Examples

```sh
tokenc                       # count the current directory
tokenc src                   # count a subdirectory
tokenc --no-color . | tee report.txt
tokenc --sort=name ~/projects/myapp
tokenc -j8 large-monorepo
```

## What counts as "code"

`tokenc` recognizes source files by extension (and a few well-known extension-
less filenames like `Makefile`, `Dockerfile`, `CMakeLists.txt`). A broad set of
languages is supported out of the box, including C, C++, C#, Java, Kotlin,
Scala, Go, Rust, Python, Ruby, JavaScript, TypeScript, PHP, Swift, Dart, Lua,
R, Julia, Haskell, Elixir, Erlang, Fortran, Shell, SQL, Assembly, and more.

Deliberately **not** counted: `json`, `yaml`, `yml`, `xml`, `toml`, `ini`,
`cfg`, `conf`, `txt`, `md`, `rst`, `csv`, `html`, `css`, `svg`, lock files and
similar — these are data, configuration or markup rather than code.

## How ignore rules work

Three layers are applied, in order of increasing specificity:

1. **Code filter.** Only recognized source extensions are even considered.
2. **Built-in defaults.** A curated list skips version-control dirs, dependency
   dirs, build/output dirs, compiled artifacts, minified bundles, lock files,
   IDE folders and OS metadata. Disable with `--no-default-ignore`.
3. **Ignore files** (`.gitignore`, `.ignore`, `.dockerignore`, `.npmignore`,
   `.rgignore`). These are read **at every directory level** and composed just
   like git does: nested files override ancestors, and `!negation` re-includes.
   Disable with `--no-gitignore`.

Symlinks are never followed, so generated files outside the scanned tree are
never pulled in.

## Cache

Line counts are cached under `$XDG_CACHE_HOME/tokenc/linecache.tsv` (falling
back to `~/.cache/tokenc/`). The cache keys on a file's `(dev, ino, size,
mtime_ns)`, so an unchanged file is never re-read. The cache is plain text and
safe to delete at any time:

```sh
rm -rf "${XDG_CACHE_HOME:-$HOME/.cache}/tokenc"
```

## Project layout

```
tokenc/
├── CMakeLists.txt        Build system
├── install.sh            One-command build + install
└── src/
    ├── main.cpp          CLI, argument parsing, orchestration
    ├── fs.{hpp,cpp}      POSIX filesystem helpers (stat, paths, XDG cache dir)
    ├── gitignore.{hpp,cpp}  git-compatible pattern parser and matcher
    ├── walker.{hpp,cpp}  Recursive directory walk with ignore rules
    ├── languages.{hpp,cpp} Extension → language table + default ignore list
    ├── counter.{hpp,cpp} mmap-based line counting + thread pool
    ├── cache.{hpp,cpp}   XDG line-count cache
    └── output.{hpp,cpp}  Table rendering, colors, summary
```

## Limitations

- Physical lines only. Comments and blank lines are included in the count (a
  line is a run of bytes terminated by `\n`, plus a final line if the file
  doesn't end with a newline). Splitting code/comment/blank per language is
  intentionally out of scope to keep the tool fast and dependency-free.
- Linux only (POSIX `dirent`, `mmap`, `stat`).
- C++20 compiler required to build from source.

## License

MIT — see [LICENSE](LICENSE).
