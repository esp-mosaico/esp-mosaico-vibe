from __future__ import annotations

import importlib.util
import json
import sys
import tempfile
import unittest
from pathlib import Path
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

HELLO_WORLD_PARTITIONS = PARTITIONS.replace(
    "ota_0,app,ota_0,0x300000,0xc00000,\nui_apps,data,0x40,0xf00000,0x100000,\n",
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

            manifest = json.loads((stage / "manifest.json").read_text(encoding="utf-8"))
            data = next(
                item for item in manifest["components"] if item["kind"] == "data"
            )
            self.assertEqual(data["target_offset"], 0xF00000)
            self.assertEqual(data["file"], "ui_apps.bin")
            self.assertEqual((stage / "ui_apps.bin").read_bytes(), b"ui apps")
            self.assertEqual(manifest["schema"], "esp-iris-system-update/v1")
            self.assertNotIn("source_layout_sha256", manifest)
            self.assertEqual(manifest["minimum_recovery_version"], "2.4.0-recovery")
            self.assertEqual(
                [item["kind"] for item in manifest["components"]],
                ["partition_table", "bootloader", "application", "data"],
            )

    def test_accepts_changes_outside_immutable_contract(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            partition_csv = root / "partitions.csv"
            partition_csv.write_text(
                PARTITIONS.replace(
                    "nvs,data,nvs,0x2f0000,0x10000,",
                    "nvs,data,nvs,0x2f0000,0x20000,",
                ).replace(
                    "ota_0,app,ota_0,0x300000,0xc00000,",
                    "ota_0,app,ota_0,0x310000,0xbf0000,",
                ),
                encoding="utf-8",
            )
            (root / "partition-table.bin").write_bytes(b"partition table")
            (root / "application.bin").write_bytes(b"application")
            (root / "bootloader.bin").write_bytes(b"bootloader")
            (root / "ui_apps.bin").write_bytes(b"ui apps")
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
            with mock.patch.object(sys, "argv", arguments):
                self.assertEqual(MODULE.main(), 0)

            manifest = json.loads(
                (root / "stage" / "manifest.json").read_text(encoding="utf-8")
            )
            application_component = next(
                item for item in manifest["components"] if item["kind"] == "application"
            )
            self.assertEqual(application_component["target_offset"], 0x310000)

    def test_rejects_changes_to_immutable_contract(self) -> None:
        variants = {
            "type": PARTITIONS.replace("factory,app,factory", "factory,data,factory"),
            "subtype": PARTITIONS.replace("sysmeta,data,nvs", "sysmeta,data,0x40"),
            "offset": PARTITIONS.replace(
                "coredump,data,coredump,0x220000",
                "coredump,data,coredump,0x230000",
            ),
            "size": PARTITIONS.replace(
                "phy_init,data,phy,0xb000,0x1000",
                "phy_init,data,phy,0xb000,0x2000",
            ),
            "flags": PARTITIONS.replace(
                "otadata,data,ota,0x9000,0x2000,",
                "otadata,data,ota,0x9000,0x2000,encrypted",
            ),
        }
        for field, partitions in variants.items():
            with self.subTest(field=field), tempfile.TemporaryDirectory() as temporary:
                root = Path(temporary)
                partition_csv = root / "partitions.csv"
                partition_csv.write_text(partitions, encoding="utf-8")
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
                    "--stage-dir",
                    str(root / "stage"),
                    "--release",
                    "1.0.0",
                ]
                expected_error = self.assertRaisesRegex(ValueError, "unexpected")
                with mock.patch.object(sys, "argv", arguments), expected_error:
                    MODULE.main()

    def test_stages_project_without_optional_data_partition(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            partition_csv = root / "partitions.csv"
            partition_table = root / "partition-table.bin"
            application = root / "application.bin"
            bootloader = root / "bootloader.bin"
            stage = root / "stage"
            partition_csv.write_text(HELLO_WORLD_PARTITIONS, encoding="utf-8")
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

            manifest = json.loads((stage / "manifest.json").read_text(encoding="utf-8"))
            self.assertEqual(
                [item["kind"] for item in manifest["components"]],
                ["partition_table", "bootloader", "application"],
            )


if __name__ == "__main__":
    unittest.main()
