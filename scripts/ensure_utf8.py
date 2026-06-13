#!/usr/bin/env python3
"""Normalize project sources to UTF-8 without BOM and LF line endings."""
from __future__ import annotations

import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
SKIP_DIRS = {"build", "_deps", ".git", "photonCounter"}
SOURCE_NAMES = {".c", ".h", ".cpp", ".hpp", ".py", ".ini", ".md", ".cmake", ".sh"}
EXTRA_NAMES = {"CMakeLists.txt", ".gitattributes"}


def should_process(path: pathlib.Path) -> bool:
    if any(part in SKIP_DIRS for part in path.parts):
        return False
    return path.name in EXTRA_NAMES or path.suffix in SOURCE_NAMES


def looks_utf16_le(raw: bytes) -> bool:
    if len(raw) < 4 or len(raw) % 2 != 0:
        return False
    sample = raw[: min(len(raw), 512)]
    pairs = len(sample) // 2
    if pairs < 2:
        return False
    null_high = sum(1 for i in range(1, len(sample), 2) if sample[i] == 0)
    ascii_low = sum(1 for i in range(0, len(sample), 2) if sample[i] < 128)
    return null_high >= pairs * 0.8 and ascii_low >= pairs * 0.5


def looks_utf16_be(raw: bytes) -> bool:
    if len(raw) < 4 or len(raw) % 2 != 0:
        return False
    sample = raw[: min(len(raw), 512)]
    pairs = len(sample) // 2
    if pairs < 2:
        return False
    null_low = sum(1 for i in range(0, len(sample), 2) if sample[i] == 0)
    ascii_high = sum(1 for i in range(1, len(sample), 2) if sample[i] < 128)
    return null_low >= pairs * 0.8 and ascii_high >= pairs * 0.5


def decode_raw(raw: bytes) -> tuple[str, str]:
    if raw.startswith(b"\xff\xfe"):
        return raw.decode("utf-16-le"), "utf-16-le-bom"
    if raw.startswith(b"\xfe\xff"):
        return raw.decode("utf-16-be"), "utf-16-be-bom"
    if looks_utf16_le(raw):
        return raw.decode("utf-16-le"), "utf-16-le"
    if looks_utf16_be(raw):
        return raw.decode("utf-16-be"), "utf-16-be"
    if raw.startswith(b"\xef\xbb\xbf"):
        return raw.decode("utf-8-sig"), "utf-8-bom"
    return raw.decode("utf-8"), "utf-8"


def normalize(path: pathlib.Path) -> str | None:
    raw = path.read_bytes()
    try:
        text, source = decode_raw(raw)
    except UnicodeDecodeError:
        return None
    text = text.lstrip("\ufeff").replace("\r\n", "\n").replace("\r", "\n")
    out = text.encode("utf-8")
    if out == raw:
        return None
    path.write_bytes(out)
    return source


def main() -> int:
    fixed = []
    for path in sorted(ROOT.rglob("*")):
        if not path.is_file() or not should_process(path):
            continue
        source = normalize(path)
        if source:
            fixed.append((path, source))
    if not fixed:
        print("All checked files are already UTF-8 without BOM (LF).")
        return 0
    for path, source in fixed:
        print(f"fixed {path.relative_to(ROOT)} ({source})")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
