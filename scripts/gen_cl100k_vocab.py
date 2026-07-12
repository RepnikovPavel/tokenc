#!/usr/bin/env python3
# SPDX-License-Identifier: 0BSD
"""Generate data/cl100k_vocab.z from tiktoken (build-time only)."""
from __future__ import annotations

import argparse
import struct
import sys
import zlib
from pathlib import Path


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--out", default="data/cl100k_vocab.z")
    args = parser.parse_args()
    try:
        import tiktoken
    except ImportError:
        print("gen_cl100k_vocab: pip install tiktoken (build-time only)", file=sys.stderr)
        return 2

    enc = tiktoken.get_encoding("cl100k_base")
    ranks = enc._mergeable_ranks
    inv = [b""] * (max(ranks.values()) + 1)
    for token_bytes, rank in ranks.items():
        inv[rank] = token_bytes

    blob = bytearray()
    blob += struct.pack("<I", len(inv))
    for token_bytes in inv:
        if len(token_bytes) > 65535:
            raise SystemExit("token too long")
        blob += struct.pack("<H", len(token_bytes))
        blob += token_bytes

    out = Path(args.out)
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_bytes(zlib.compress(bytes(blob), 9))
    print(f"wrote {out} ({out.stat().st_size} bytes), vocab={len(inv)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())