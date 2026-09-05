from __future__ import annotations

import importlib.util
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest import mock


REPOSITORY = Path(__file__).resolve().parents[1]
MODULE_PATH = REPOSITORY / "tools" / "prepare_system_update.py"
SPEC = importlib.util.spec_from_file_location("prepare_system_update", MODULE_PATH)
assert SPEC is not None and SPEC.loader is not None
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


PARTITIONS = """\
otadata,data,ota,0x9000,0x2000,
phy_init,data,phy,0xb000,0x1000,
sysmeta,data,nvs,0xc000,0x14000,
factory,app,factory,0x20000,0x200000,
coredump,data,coredump,0x220000,0xd0000,
nvs,data,nvs,0x2f0000,0x10000,
ota_0,app,ota_0,0x300000,0xc00000,
ui_apps,data,0x40,0xf00000,0x100000,
"""

LEGACY_PARTITIONS = PARTITIONS.replace(
    "ota_0,app,ota_0,0x300000,0xc00000,\n"
    "ui_apps,data,0x40,0xf00000,0x100000,\n",
    "ota_0,app,ota_0,0x300000,0xd00000,\n",
)


class PrepareSystemUpdateTests(unittest.TestCase):
    def test_stages_ui_apps_as_data_component(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            partition_csv = root / "partitions.csv"
            partition_table = root / "partition-table.bin"
            application = root / "application.bin"
            bootloader = root / "bootloader.bin"
            ui_apps = root / "ui_apps.bin"
            stage = root / "stage"
            partition_csv.write_text(PARTITIONS, encoding="utf-8")
            partition_table.write_bytes(b"partition table")
            application.write_bytes(b"application")
            bootloader.write_bytes(b"bootloader")
            ui_apps.write_bytes(b"ui apps")

            arguments = [
                "prepare_system_update.py",
                "--partition-csv",
                str(partition_csv),
                "--partition-table",
                str(partition_table),
                "--application",
                str(application),
                "--bootloader",
                str(bootloader),
                "--ui-apps",
                str(ui_apps),
                "--stage-dir",
                str(stage),
                "--release",
                "1.0.0",
            ]
            with mock.patch.object(sys, "argv", arguments):
                self.assertEqual(MODULE.main(), 0)

            manifest = json.loads(
                (stage / "manifest.json").read_text(encoding="utf-8")
            )
            data = next(
                item for item in manifest["components"] if item["kind"] == "data"
            )
            self.assertEqual(data["target_offset"], 0xF00000)
            self.assertEqual(data["file"], "ui_apps.bin")
            self.assertEqual((stage / "ui_apps.bin").read_bytes(), b"ui apps")
            self.assertEqual(
                manifest["source_layout_sha256"],
                [manifest["target_layout_sha256"]],
            )

    def test_rejects_legacy_ota_size_for_new_contract(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            partition_csv = root / "partitions.csv"
            partition_csv.write_text(
                PARTITIONS.replace("0xc00000", "0xd00000"), encoding="utf-8"
            )
            arguments = [
                "prepare_system_update.py",
                "--partition-csv",
                str(partition_csv),
                "--partition-table",
                str(root / "partition-table.bin"),
                "--application",
                str(root / "application.bin"),
                "--bootloader",
                str(root / "bootloader.bin"),
                "--ui-apps",
                str(root / "ui_apps.bin"),
                "--stage-dir",
                str(root / "stage"),
                "--release",
                "1.0.0",
            ]
            with mock.patch.object(sys, "argv", arguments), self.assertRaisesRegex(
                ValueError, "unexpected ota_0 layout"
            ):
                MODULE.main()

    def test_preserves_legacy_layout_for_projects_without_ui_partition(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            partition_csv = root / "partitions.csv"
            partition_table = root / "partition-table.bin"
            application = root / "application.bin"
            bootloader = root / "bootloader.bin"
            stage = root / "stage"
            partition_csv.write_text(LEGACY_PARTITIONS, encoding="utf-8")
            partition_table.write_bytes(b"partition table")
            application.write_bytes(b"application")
            bootloader.write_bytes(b"bootloader")

            arguments = [
                "prepare_system_update.py",
                "--partition-csv",
                str(partition_csv),
                "--partition-table",
                str(partition_table),
                "--application",
                str(application),
                "--bootloader",
                str(bootloader),
                "--stage-dir",
                str(stage),
                "--release",
                "1.0.0",
            ]
            with mock.patch.object(sys, "argv", arguments):
                self.assertEqual(MODULE.main(), 0)

            manifest = json.loads(
                (stage / "manifest.json").read_text(encoding="utf-8")
            )
            self.assertEqual(
                [item["kind"] for item in manifest["components"]],
                ["application", "bootloader", "partition_table"],
            )


if __name__ == "__main__":
    unittest.main()
