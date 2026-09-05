#!/usr/bin/env python3
"""Wrap a deployable GSPB in the ESP-Mosaico ui_apps image format."""

from __future__ import annotations

import argparse
import hashlib
import os
import struct
import tempfile
from pathlib import Path


MAGIC = b"MOSGSP\x00\x01"
HEADER_VERSION = 1
HEADER_SIZE = 64
GSPB_HEADER_SIZE = 16
HEADER = struct.Struct("<8sHHI32s16s")


def pack_partition_image(bundle_path: Path, output_path: Path, max_size: int) -> None:
    bundle = bundle_path.read_bytes()
    if len(bundle) < GSPB_HEADER_SIZE or bundle[:4] != b"GSPB":
        raise ValueError(f"not a GSPB bundle: {bundle_path}")
    declared_size = struct.unpack_from("<I", bundle, 8)[0]
    if declared_size != len(bundle):
        raise ValueError(
            f"GSPB size field is {declared_size}, actual size is {len(bundle)}"
        )

    header = HEADER.pack(
        MAGIC,
        HEADER_VERSION,
        HEADER_SIZE,
        len(bundle),
        hashlib.sha256(bundle).digest(),
        bytes(16),
    )
    image = header + bundle
    if len(image) > max_size:
        raise ValueError(
            f"ui_apps image is {len(image)} bytes, partition capacity is {max_size}"
        )

    output_path.parent.mkdir(parents=True, exist_ok=True)
    descriptor, temporary_name = tempfile.mkstemp(
        prefix=f".{output_path.name}.", dir=output_path.parent
    )
    try:
        os.fchmod(descriptor, 0o644)
        with os.fdopen(descriptor, "wb") as handle:
            handle.write(image)
            handle.flush()
            os.fsync(handle.fileno())
        os.replace(temporary_name, output_path)
    except BaseException:
        try:
            os.unlink(temporary_name)
        except FileNotFoundError:
            pass
        raise


def _integer(value: str) -> int:
    return int(value, 0)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--bundle", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--max-size", type=_integer, required=True)
    args = parser.parse_args()
    pack_partition_image(args.bundle, args.output, args.max_size)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
