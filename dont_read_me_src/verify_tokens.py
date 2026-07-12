#!/usr/bin/env python3
# SPDX-License-Identifier: 0BSD
"""Optional dev check: native tokenc cl100k vs reference tiktoken."""
from __future__ import annotations

import subprocess
import sys
from pathlib import Path


def parse_total(text: str) -> int | None:
    for line in text.splitlines():
        if line.startswith("Total\t"):
            return int(line.split("\t")[3])
    return None


def main() -> int:
    if len(sys.argv) < 3:
        print(f"usage: {sys.argv[0]} <tokenc-binary> <path>", file=sys.stderr)
        return 1

    binary = Path(sys.argv[1])
    root = Path(sys.argv[2]).resolve()
    out = subprocess.check_output([str(binary), "--no-cache", str(root)], text=True)
    reported = parse_total(out)
    if reported is None:
        print("verify_tokens: no Total row", file=sys.stderr)
        return 1

    try:
        import tiktoken
    except ImportError:
        print(f"OK native cl100k_base={reported} (tiktoken not installed; skip reference)")
        return 0

    enc = tiktoken.get_encoding("cl100k_base")
    cache_paths = Path.home() / ".cache/tokenc/tokenize_paths.txt"
    if not cache_paths.exists():
        print("verify_tokens: run tokenc first to populate paths cache", file=sys.stderr)
        return 1

    ref = 0
    for line in cache_paths.read_text().splitlines():
        p = line.strip()
        if p:
            ref += len(enc.encode_ordinary(Path(p).read_text(encoding="utf-8", errors="replace")))

    if reported != ref:
        print(f"MISMATCH cl100k_base: tokenc={reported} tiktoken={ref}")
        return 1
    print(f"OK cl100k_base={reported}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())