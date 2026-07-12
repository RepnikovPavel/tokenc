#!/usr/bin/env python3
# SPDX-License-Identifier: 0BSD
"""Compare tokenc token totals against reference tiktoken per file."""
from __future__ import annotations

import subprocess
import sys
from pathlib import Path

try:
    import tiktoken
except ImportError:
    print("verify_tokens: pip install tiktoken", file=sys.stderr)
    raise SystemExit(2)


def parse_tokenc_tsv(text: str) -> dict[str, tuple[int, int]]:
    rows: dict[str, tuple[int, int]] = {}
    for line in text.splitlines():
        if not line or line.startswith("language"):
            continue
        parts = line.split("\t")
        if len(parts) < 5:
            continue
        lang, cl, o2 = parts[0], int(parts[3]), int(parts[4])
        rows[lang] = (cl, o2)
    return rows


def reference_for_paths(paths_file: Path) -> tuple[int, int]:
    enc_cl = tiktoken.get_encoding("cl100k_base")
    enc_o2 = tiktoken.get_encoding("o200k_base")
    cl = o2 = 0
    for line in paths_file.read_text().splitlines():
        p = line.strip()
        if not p:
            continue
        data = Path(p).read_bytes().decode("utf-8", "replace")
        cl += len(enc_cl.encode(data))
        o2 += len(enc_o2.encode(data))
    return cl, o2


def main() -> int:
    if len(sys.argv) < 3:
        print(f"usage: {sys.argv[0]} <tokenc-binary> <path>", file=sys.stderr)
        return 1

    binary = Path(sys.argv[1])
    root = Path(sys.argv[2]).resolve()
    out = subprocess.check_output([str(binary), "--no-cache", str(root)], text=True)
    reported = parse_tokenc_tsv(out)
    total = reported.get("Total")
    if not total:
        print("verify_tokens: no Total row in tokenc output", file=sys.stderr)
        return 1

    cache_paths = Path.home() / ".cache/tokenc/tokenize_paths.txt"
    if not cache_paths.exists():
        print("verify_tokens: missing cache paths file; run tokenc first", file=sys.stderr)
        return 1

    ref_cl, ref_o2 = reference_for_paths(cache_paths)
    ok = True
    if total[0] != ref_cl:
        print(f"MISMATCH cl100k_base: tokenc={total[0]} tiktoken={ref_cl}")
        ok = False
    if total[1] != ref_o2:
        print(f"MISMATCH o200k_base: tokenc={total[1]} tiktoken={ref_o2}")
        ok = False
    if ok:
        print(f"OK cl100k_base={total[0]} o200k_base={total[1]} ({cache_paths.stat().st_size} bytes paths list)")
    return 0 if ok else 1


if __name__ == "__main__":
    raise SystemExit(main())