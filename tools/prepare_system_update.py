#!/usr/bin/env python3
"""Stage an ESP-Mosaico application System Update manifest and components."""

from __future__ import annotations

import argparse
import csv
import hashlib
import json
import shutil
from pathlib import Path
from typing import NamedTuple

PARTITION_TABLE_REGION_BYTES = 0x1000
BOOTLOADER_OFFSET = 0x2000
PARTITION_TABLE_OFFSET = 0x8000


class Partition(NamedTuple):
    type: str
    subtype: str
    offset: int
    size: int
    flags: str


IMMUTABLE_LAYOUT = {
    "otadata": Partition("data", "ota", 0x9000, 0x2000, ""),
    "phy_init": Partition("data", "phy", 0xB000, 0x1000, ""),
    "sysmeta": Partition("data", "nvs", 0xC000, 0x14000, ""),
    "factory": Partition("app", "factory", 0x20000, 0x200000, ""),
    "coredump": Partition("data", "coredump", 0x220000, 0xD0000, ""),
}


def _integer(value: str) -> int:
    value = value.strip()
    if value.upper().endswith("K"):
        return int(value[:-1].strip(), 0) * 1024
    return int(value, 0)


def _read_layout(path: Path) -> dict[str, Partition]:
    rows: dict[str, Partition] = {}
    with path.open(encoding="utf-8", newline="") as handle:
        reader = csv.reader(
            line for line in handle if not line.lstrip().startswith("#")
        )
        for row in reader:
            if not row or not row[0].strip():
                continue
            if len(row) < 5:
                raise ValueError(f"invalid partition row: {row!r}")
            name = row[0].strip()
            if name in rows:
                raise ValueError(f"duplicate partition name: {name}")
            rows[name] = Partition(
                row[1].strip().lower(),
                row[2].strip().lower(),
                _integer(row[3]),
                _integer(row[4]),
                row[5].strip().lower() if len(row) > 5 else "",
            )
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
    parser.add_argument("--ui-apps", type=Path)
    parser.add_argument("--stage-dir", type=Path, required=True)
    parser.add_argument("--release", required=True)
    args = parser.parse_args()

    layout = _read_layout(args.partition_csv)
    for name, expected in IMMUTABLE_LAYOUT.items():
        if layout.get(name) != expected:
            raise ValueError(
                f"unexpected {name} layout: {layout.get(name)!r}, expected {expected!r}"
            )

    ota_partition = layout.get("ota_0")
    if (
        ota_partition is None
        or ota_partition.type != "app"
        or ota_partition.subtype != "ota_0"
        or ota_partition.flags
    ):
        raise ValueError(
            f"ota_0 must be a writable app/ota_0 partition: {ota_partition!r}"
        )
    ui_partition = layout.get("ui_apps")
    if args.ui_apps is not None and ui_partition is None:
        raise ValueError("ui_apps image provided but the partition is missing")
    if args.ui_apps is not None and (ui_partition.type != "data" or ui_partition.flags):
        raise ValueError(f"ui_apps must be a writable data partition: {ui_partition!r}")

    target_layout = _layout_sha256(args.partition_table)
    _require_image(args.application, "application", ota_partition.size)
    _require_image(
        args.bootloader, "bootloader", PARTITION_TABLE_OFFSET - BOOTLOADER_OFFSET
    )
    if args.ui_apps is not None:
        _require_image(args.ui_apps, "ui_apps", ui_partition.size)

    stage_dir = args.stage_dir.resolve()
    stage_dir.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(args.application, stage_dir / "ota_0.bin")
    shutil.copyfile(args.bootloader, stage_dir / "bootloader.bin")
    shutil.copyfile(args.partition_table, stage_dir / "partition-table.bin")
    if args.ui_apps is not None:
        shutil.copyfile(args.ui_apps, stage_dir / "ui_apps.bin")

    components = [
        {
            "id": 1,
            "kind": "partition_table",
            "target_offset": PARTITION_TABLE_OFFSET,
            "file": "partition-table.bin",
        },
        {
            "id": 2,
            "kind": "bootloader",
            "target_offset": BOOTLOADER_OFFSET,
            "file": "bootloader.bin",
        },
        {
            "id": 3,
            "kind": "application",
            "target_offset": ota_partition.offset,
            "file": "ota_0.bin",
        },
    ]
    if args.ui_apps is not None:
        components.append(
            {
                "id": 4,
                "kind": "data",
                "target_offset": ui_partition.offset,
                "file": "ui_apps.bin",
            }
        )

    manifest = {
        "schema": "esp-iris-system-update/v1",
        "release": args.release,
        "minimum_recovery_version": "2.4.0-recovery",
        "target": {"chip_id": 0x20, "flash_size": 16 * 1024 * 1024},
        "target_layout_sha256": target_layout,
        "components": components,
    }
    (stage_dir / "manifest.json").write_text(
        json.dumps(manifest, ensure_ascii=False, indent=2) + "\n",
        encoding="utf-8",
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
