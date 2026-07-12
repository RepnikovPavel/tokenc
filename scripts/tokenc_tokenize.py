#!/usr/bin/env python3
# SPDX-License-Identifier: 0BSD
"""Batch-tokenize file paths (stdin or argv) with cl100k_base and o200k_base.

Output (TSV, stdout):
  path<TAB>cl100k_base<TAB>o200k_base

Requires: pip install tiktoken
"""
from __future__ import annotations

import sys


def main() -> int:
    try:
        import tiktoken
    except ImportError:
        print("tokenc_tokenize: tiktoken not installed (pip install tiktoken)", file=sys.stderr)
        return 2

    enc_cl = tiktoken.get_encoding("cl100k_base")
    enc_o2 = tiktoken.get_encoding("o200k_base")

    paths = [p.strip() for p in sys.argv[1:] if p.strip()] if len(sys.argv) > 1 else None
    if not paths:
        paths = [line.strip() for line in sys.stdin if line.strip()]

    for path in paths:
        try:
            with open(path, "rb") as f:
                text = f.read().decode("utf-8", "replace")
            n1 = len(enc_cl.encode(text))
            n2 = len(enc_o2.encode(text))
            print(f"{path}\t{n1}\t{n2}")
        except OSError as e:
            print(f"tokenc_tokenize: {path}: {e}", file=sys.stderr)
            print(f"{path}\t0\t0")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())