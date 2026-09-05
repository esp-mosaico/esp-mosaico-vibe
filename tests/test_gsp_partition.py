from __future__ import annotations

import hashlib
import importlib.util
from pathlib import Path
import struct
import tempfile
import unittest


REPOSITORY = Path(__file__).resolve().parents[1]
MODULE_PATH = REPOSITORY / "tools" / "pack_gsp_partition.py"
SPEC = importlib.util.spec_from_file_location("pack_gsp_partition", MODULE_PATH)
assert SPEC is not None and SPEC.loader is not None
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


class GspPartitionImageTests(unittest.TestCase):
    def test_pack_partition_image_adds_aligned_verified_header(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            bundle = bytearray(128)
            bundle[:4] = b"GSPB"
            struct.pack_into("<I", bundle, 8, len(bundle))
            bundle_path = root / "bundle.gspb"
            image_path = root / "ui_apps.bin"
            bundle_path.write_bytes(bundle)

            MODULE.pack_partition_image(bundle_path, image_path, 4096)

            image = image_path.read_bytes()
            magic, version, header_size, bundle_size, digest, reserved = (
                MODULE.HEADER.unpack_from(image)
            )
            self.assertEqual(magic, MODULE.MAGIC)
            self.assertEqual(version, MODULE.HEADER_VERSION)
            self.assertEqual(header_size, 64)
            self.assertEqual(bundle_size, len(bundle))
            self.assertEqual(digest, hashlib.sha256(bundle).digest())
            self.assertEqual(reserved, bytes(16))
            self.assertEqual(image[64:], bundle)

    def test_pack_partition_image_rejects_oversize_bundle(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            bundle = bytearray(128)
            bundle[:4] = b"GSPB"
            struct.pack_into("<I", bundle, 8, len(bundle))
            bundle_path = root / "bundle.gspb"
            bundle_path.write_bytes(bundle)

            with self.assertRaisesRegex(ValueError, "partition capacity"):
                MODULE.pack_partition_image(bundle_path, root / "ui.bin", 128)


if __name__ == "__main__":
    unittest.main()
