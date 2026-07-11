# Installing tokenc when building a Docker image

A ready-to-use [`Dockerfile`](../Dockerfile) lives at the repo root. Two ways
to add tokenc to your own image are shown below.

> Note: avoid the legacy `gcc:9` / `gcc:10` Docker images — their apt mirrors
> are archived and `apt-get install` no longer works on them. Use a current
> Debian/Ubuntu base and install the compiler from apt instead (any version is
> fine; tokenc only needs C++17, i.e. `g++ >= 9`).

## Option A — Use the repo's Dockerfile directly

```sh
git clone https://github.com/RepnikovPavel/tokenc.git
cd tokenc
docker build -t tokenc .
docker run --rm -v "$PWD":/work -w /work tokenc /work
```

It is a multi-stage build: the compiler toolchain stays in the builder stage;
the final image contains only the `tokenc` binary on `debian:stable-slim`.

## Option B — Multi-stage snippet for your own Dockerfile

```dockerfile
# ---- builder ----
FROM debian:stable-slim AS builder
RUN apt-get update \
 && apt-get install -y --no-install-recommends \
        build-essential cmake ninja-build git ca-certificates \
 && rm -rf /var/lib/apt/lists/*
WORKDIR /src
RUN git clone https://github.com/RepnikovPavel/tokenc.git .
RUN cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
 && cmake --build build -j"$(nproc)"

# ---- final image ----
FROM debian:stable-slim
COPY --from=builder /src/build/tokenc /usr/local/bin/tokenc
ENTRYPOINT ["tokenc"]
CMD ["--help"]
```

## Option C — One-line install inside an existing Dockerfile

If your image already has `curl`, a compiler and `cmake`, add tokenc with one
`RUN`:

```dockerfile
RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential cmake git curl ca-certificates \
 && curl -fsSL https://raw.githubusercontent.com/RepnikovPavel/tokenc/main/read_me_if_it_is_not_installed/install.sh | sh \
 && apt-get purge -y --auto-remove git \
 && rm -rf /var/lib/apt/lists/*
```

## Notes

- No `PATH` or shell-rc changes are needed — `install.sh` and the Dockerfile
  put the binary in `/usr/local/bin`, which is on `PATH` by default.
- For stateless container runs pass `--no-cache`. If you want caching within a
  container's lifetime, set `XDG_CACHE_HOME=/tmp` (or any writable dir).
