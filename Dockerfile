# Multi-stage build for tokenc.
# Builder uses a current Debian with a C++17 compiler from apt; the final image
# copies only the single static-ish binary. We avoid the old `gcc:9` image on
# purpose: its apt mirrors are archived and no longer install packages.
#
# Build:  docker build -t tokenc .
# Run:    docker run --rm -v "$PWD":/work -w /work tokenc /work
#
FROM debian:stable-slim AS builder
RUN apt-get update \
 && apt-get install -y --no-install-recommends \
        build-essential cmake ninja-build git ca-certificates \
 && rm -rf /var/lib/apt/lists/*
WORKDIR /src
COPY . .
RUN cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
 && cmake --build build -j"$(nproc)"

FROM debian:stable-slim
RUN apt-get update \
 && apt-get install -y --no-install-recommends libstdc++6 python3 python3-pip \
 && pip3 install --no-cache-dir tiktoken \
 && rm -rf /var/lib/apt/lists/*
COPY --from=builder /src/build/tokenc /usr/local/bin/tokenc
COPY --from=builder /src/scripts/tokenc_tokenize.py /usr/local/share/tokenc/tokenc_tokenize.py
ENTRYPOINT ["tokenc"]
CMD ["--help"]
