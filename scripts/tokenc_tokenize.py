#!/usr/bin/env python3
# SPDX-License-Identifier: 0BSD
# tokenc-embed-version: 2
"""Batch-tokenize files with cl100k_base and o200k_base (OpenAI tiktoken)."""
from __future__ import annotations

import argparse
import sys


def main() -> int:
    parser = argparse.ArgumentParser(add_help=False)
    parser.add_argument("--paths-file", required=True)
    parser.add_argument("--output-file", required=True)
    args = parser.parse_args()

    try:
        import tiktoken
    except ImportError:
        print("tokenc_tokenize: pip install tiktoken", file=sys.stderr)
        return 2

    enc_cl = tiktoken.get_encoding("cl100k_base")
    enc_o2 = tiktoken.get_encoding("o200k_base")

    try:
        with open(args.paths_file, "r", encoding="utf-8") as pf:
            paths = [line.strip() for line in pf if line.strip()]
    except OSError as e:
        print(f"tokenc_tokenize: paths-file: {e}", file=sys.stderr)
        return 2

    try:
        out = open(args.output_file, "w", encoding="utf-8")
    except OSError as e:
        print(f"tokenc_tokenize: output-file: {e}", file=sys.stderr)
        return 2

    with out:
        for path in paths:
            try:
                with open(path, "rb") as f:
                    text = f.read().decode("utf-8", "replace")
                n1 = len(enc_cl.encode(text))
                n2 = len(enc_o2.encode(text))
                out.write(f"{path}\t{n1}\t{n2}\n")
            except OSError as e:
                print(f"tokenc_tokenize: {path}: {e}", file=sys.stderr)
                out.write(f"{path}\t0\t0\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())