#!/usr/bin/env python3
"""Normalize project sources to UTF-8 without BOM and LF line endings."""
from __future__ import annotations
import pathlib
import sys
ROOT = pathlib.Path(__file__).resolve().parent.parent
SKIP_DIRS = {"build", "_deps", ".git", "photonCounter"}
SOURCE_NAMES = {".c", ".h", ".cpp", ".hpp", ".py", ".ini", ".md", ".cmake"}
EXTRA_NAMES = {"CMakeLists.txt", ".gitattributes"}

def should_process(path: pathlib.Path) -> bool:
    if any(part in SKIP_DIRS for part in path.parts):
        return False
    return path.name in EXTRA_NAMES or path.suffix in SOURCE_NAMES

def decode_raw(raw: bytes) -> tuple[str, str]:
    if raw.startswith(b"\xff\xfe"):
        return raw.decode("utf-16-le"), "utf-16-le"
    if raw.startswith(b"\xfe\xff"):
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