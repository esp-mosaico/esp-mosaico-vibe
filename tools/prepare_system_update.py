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
FIXED_LAYOUT = {
    "otadata": (0x9000, 0x2000),
    "phy_init": (0xB000, 0x1000),
    "sysmeta": (0xC000, 0x14000),
    "factory": (0x20000, 0x200000),
    "coredump": (0x220000, 0xD0000),
    "nvs": (0x2F0000, 0x10000),
}
LEGACY_OTA_LAYOUT = (0x300000, 0xD00000)
GSP_OTA_LAYOUT = (0x300000, 0xC00000)
GSP_UI_APPS_LAYOUT = (0xF00000, 0x100000)
# Layout hash observed on the existing development device before migration to
# the hello_world partition contract. Keep this compatibility value explicit;
# the target hash is always calculated from the current build output below.
COMPATIBLE_SOURCE_LAYOUTS = (
    "1c8c4109ee5232ee43508eef3f60bd127de9349fab991b7722a84a4f25889f4c",
    "068246e2e1f05b063b0c5cef5ee80633b1a9e0dee834dd8eb91249b1e84b0ed2",
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
    parser.add_argument("--ui-apps", type=Path)
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
    for name, expected in FIXED_LAYOUT.items():
        if layout.get(name) != expected:
            raise ValueError(
                f"unexpected {name} layout: {layout.get(name)!r}, expected {expected!r}"
            )

    has_ui_partition = "ui_apps" in layout
    expected_ota = GSP_OTA_LAYOUT if has_ui_partition else LEGACY_OTA_LAYOUT
    if layout.get("ota_0") != expected_ota:
        raise ValueError(
            f"unexpected ota_0 layout: {layout.get('ota_0')!r}, "
            f"expected {expected_ota!r}"
        )
    if has_ui_partition and layout.get("ui_apps") != GSP_UI_APPS_LAYOUT:
        raise ValueError(
            f"unexpected ui_apps layout: {layout.get('ui_apps')!r}, "
            f"expected {GSP_UI_APPS_LAYOUT!r}"
        )
    if args.ui_apps is not None and not has_ui_partition:
        raise ValueError("ui_apps image provided but the partition is missing")

    target_layout = _layout_sha256(args.partition_table)
    _require_image(args.application, "application", layout["ota_0"][1])
    _require_image(args.bootloader, "bootloader", PARTITION_TABLE_OFFSET - BOOTLOADER_OFFSET)
    if args.ui_apps is not None:
        _require_image(args.ui_apps, "ui_apps", layout["ui_apps"][1])

    stage_dir = args.stage_dir.resolve()
    stage_dir.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(args.application, stage_dir / "ota_0.bin")
    shutil.copyfile(args.bootloader, stage_dir / "bootloader.bin")
    shutil.copyfile(args.partition_table, stage_dir / "partition-table.bin")
    if args.ui_apps is not None:
        shutil.copyfile(args.ui_apps, stage_dir / "ui_apps.bin")

    if args.source_layout:
        source_layouts = list(dict.fromkeys(args.source_layout))
    elif args.ui_apps is not None:
        # Recovery resolves a data component against its current partition
        # table, so the complete UI bundle is valid only after migration.
        source_layouts = [target_layout]
    else:
        source_layouts = list(
            dict.fromkeys([*COMPATIBLE_SOURCE_LAYOUTS, target_layout])
        )
    components = [
        {
            "id": 1,
            "kind": "application",
            "target_offset": layout["ota_0"][0],
            "file": "ota_0.bin",
        },
    ]
    if args.ui_apps is not None:
        components.append(
            {
                "id": 2,
                "kind": "data",
                "target_offset": layout["ui_apps"][0],
                "file": "ui_apps.bin",
            }
        )
    next_id = len(components) + 1
    components.extend(
        [
            {
                "id": next_id,
                "kind": "bootloader",
                "target_offset": BOOTLOADER_OFFSET,
                "file": "bootloader.bin",
            },
            {
                "id": next_id + 1,
                "kind": "partition_table",
                "target_offset": PARTITION_TABLE_OFFSET,
                "file": "partition-table.bin",
            },
        ]
    )

    manifest = {
        "schema": "esp-iris-system-update/v1",
        "release": args.release,
        "minimum_recovery_version": "2.2.0-recovery",
        "target": {"chip_id": 0x20, "flash_size": 16 * 1024 * 1024},
        "source_layout_sha256": source_layouts,
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
