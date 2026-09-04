#!/usr/bin/env python3
"""Stage an ESP-Mosaico application System Update manifest and components."""

from __future__ import annotations

import argparse
import csv
import hashlib
import json
import shutil
from pathlib import Path


PARTITION_TABLE_REGION_BYTES = 0x1000
BOOTLOADER_OFFSET = 0x2000
PARTITION_TABLE_OFFSET = 0x8000
EXPECTED_LAYOUT = {
    "otadata": (0x9000, 0x2000),
    "phy_init": (0xB000, 0x1000),
    "sysmeta": (0xC000, 0x14000),
    "factory": (0x20000, 0x200000),
    "coredump": (0x220000, 0xD0000),
    "nvs": (0x2F0000, 0x10000),
    "ota_0": (0x300000, 0xD00000),
}
# Layout hash observed on the existing development device before migration to
# the hello_world partition contract. Keep this compatibility value explicit;
# the target hash is always calculated from the current build output below.
COMPATIBLE_SOURCE_LAYOUTS = (
    "1c8c4109ee5232ee43508eef3f60bd127de9349fab991b7722a84a4f25889f4c",
)


def _integer(value: str) -> int:
    value = value.strip()
    if value.upper().endswith("K"):
        return int(value[:-1].strip(), 0) * 1024
    return int(value, 0)


def _read_layout(path: Path) -> dict[str, tuple[int, int]]:
    rows: dict[str, tuple[int, int]] = {}
    with path.open(encoding="utf-8", newline="") as handle:
        reader = csv.reader(
            line for line in handle if not line.lstrip().startswith("#")
        )
        for row in reader:
            if not row or not row[0].strip():
                continue
            if len(row) < 5:
                raise ValueError(f"invalid partition row: {row!r}")
            rows[row[0].strip()] = (_integer(row[3]), _integer(row[4]))
    return rows


def _require_image(path: Path, name: str, capacity: int) -> None:
    if not path.is_file() or path.stat().st_size == 0:
        raise ValueError(f"{name} image is missing or empty: {path}")
    if path.stat().st_size > capacity:
        raise ValueError(
            f"{name} image is {path.stat().st_size} bytes, capacity is {capacity}"
        )


def _layout_sha256(path: Path) -> str:
    data = path.read_bytes()
    if not data or len(data) > PARTITION_TABLE_REGION_BYTES:
        raise ValueError("partition table must fit its 4 KiB Flash sector")
    return hashlib.sha256(data.ljust(PARTITION_TABLE_REGION_BYTES, b"\xff")).hexdigest()


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--partition-csv", type=Path, required=True)
    parser.add_argument("--partition-table", type=Path, required=True)
    parser.add_argument("--application", type=Path, required=True)
    parser.add_argument("--bootloader", type=Path, required=True)
    parser.add_argument("--stage-dir", type=Path, required=True)
    parser.add_argument("--release", required=True)
    parser.add_argument(
        "--source-layout",
        action="append",
        default=[],
        help="additional accepted source layout SHA-256 (repeatable)",
    )
    args = parser.parse_args()

    layout = _read_layout(args.partition_csv)
    for name, expected in EXPECTED_LAYOUT.items():
        if layout.get(name) != expected:
            raise ValueError(
                f"unexpected {name} layout: {layout.get(name)!r}, expected {expected!r}"
            )

    target_layout = _layout_sha256(args.partition_table)
    _require_image(args.application, "application", EXPECTED_LAYOUT["ota_0"][1])
    _require_image(args.bootloader, "bootloader", PARTITION_TABLE_OFFSET - BOOTLOADER_OFFSET)

    stage_dir = args.stage_dir.resolve()
    stage_dir.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(args.application, stage_dir / "ota_0.bin")
    shutil.copyfile(args.bootloader, stage_dir / "bootloader.bin")
    shutil.copyfile(args.partition_table, stage_dir / "partition-table.bin")

    source_layouts = list(
        dict.fromkeys(
            args.source_layout or [*COMPATIBLE_SOURCE_LAYOUTS, target_layout]
        )
    )
    manifest = {
        "schema": "esp-iris-system-update/v1",
        "release": args.release,
        "minimum_recovery_version": "2.2.0-recovery",
        "target": {"chip_id": 0x20, "flash_size": 16 * 1024 * 1024},
        "source_layout_sha256": source_layouts,
        "target_layout_sha256": target_layout,
        "components": [
            {
                "id": 1,
                "kind": "application",
                "target_offset": EXPECTED_LAYOUT["ota_0"][0],
                "file": "ota_0.bin",
            },
            {
                "id": 2,
                "kind": "bootloader",
                "target_offset": BOOTLOADER_OFFSET,
                "file": "bootloader.bin",
            },
            {
                "id": 3,
                "kind": "partition_table",
                "target_offset": PARTITION_TABLE_OFFSET,
                "file": "partition-table.bin",
            },
        ],
    }
    (stage_dir / "manifest.json").write_text(
        json.dumps(manifest, ensure_ascii=False, indent=2) + "\n",
        encoding="utf-8",
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
