#!/usr/bin/env python3
"""Pack MiniHub /data/web factory files into one firmware blob.

Layout (little-endian):
  magic "MHWS"
  u32 file_count
  repeat:
    u16 name_len
    name bytes (relative, e.g. index.html, js/app.js)
    u32 data_len
    data bytes
"""

from __future__ import annotations

import argparse
from pathlib import Path
import struct


MAGIC = b"MHWS"


def collect(src: Path) -> list[tuple[str, bytes]]:
    files: list[tuple[str, bytes]] = []
    for path in sorted(src.rglob("*")):
        if not path.is_file():
            continue
        rel = path.relative_to(src).as_posix()
        if rel.startswith(".") or "/." in rel:
            continue
        files.append((rel, path.read_bytes()))
    return files


def pack(src: Path, dst: Path) -> None:
    files = collect(src)
    blob = bytearray(MAGIC)
    blob += struct.pack("<I", len(files))
    for name, data in files:
        encoded = name.encode("utf-8")
        if len(encoded) > 0xFFFF:
            raise SystemExit(f"web seed name too long: {name}")
        blob += struct.pack("<H", len(encoded))
        blob += encoded
        blob += struct.pack("<I", len(data))
        blob += data
    dst.parent.mkdir(parents=True, exist_ok=True)
    dst.write_bytes(blob)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--src", required=True, type=Path)
    parser.add_argument("--dst", required=True, type=Path)
    args = parser.parse_args()
    if not args.src.is_dir():
        raise SystemExit(f"web seed source missing: {args.src}")
    pack(args.src, args.dst)


if __name__ == "__main__":
    main()
