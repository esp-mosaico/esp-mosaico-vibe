from __future__ import annotations

import importlib.util
import json
from pathlib import Path
import struct
import tempfile
import unittest
import wave

from PIL import Image


ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location(
    "pack_game_assets", ROOT / "submodule/raylib-lite-engine/tools/pack_game_assets.py")
assert SPEC and SPEC.loader
assets = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(assets)


class GameAssetCompilerTests(unittest.TestCase):
    def test_asset_ids_are_stable(self) -> None:
        self.assertEqual(assets.asset_id("tower_pulse"), 0xC57E1570)
        self.assertEqual(assets.asset_id("tower_pulse"), assets.asset_id("tower_pulse"))

    def test_atlas_output_is_deterministic_and_rejects_overflow(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = root / "atlas.png"
            Image.new("RGBA", (8, 8), (255, 0, 0, 255)).save(source)
            manifest = {"columns": 1, "rows": 1, "output_cell": 8,
                        "frames": [{"name": "hero"}]}
            first, second = root / "first.atlas", root / "second.atlas"
            assets.write_atlas(source, manifest, first)
            assets.write_atlas(source, manifest, second)
            self.assertEqual(first.read_bytes(), second.read_bytes())
            manifest["alpha_mode"] = "binary"
            binary = root / "binary.atlas"
            report = assets.write_atlas(source, manifest, binary)
            header = assets.ATLAS_HEADER.unpack_from(binary.read_bytes())
            self.assertEqual(report["alpha_mode"], "binary")
            self.assertTrue(header[4] & assets.ATLAS_FLAG_BINARY_ALPHA)
            manifest["frames"].append({"name": "overflow"})
            with self.assertRaisesRegex(ValueError, "exceeds"):
                assets.write_atlas(source, manifest, root / "bad.atlas")

    def test_opaque_atlas_keeps_light_edge_fill(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = root / "tiles.png"
            Image.new("RGB", (16, 8), (214, 218, 222)).save(source)
            output = root / "tiles.atlas"
            assets.write_atlas(source, {
                "columns": 2, "rows": 1, "output_cell": 8,
                "alpha_mode": "opaque",
                "frames": [{"name": "wall_a"}, {"name": "wall_b"}],
            }, output)
            blob = output.read_bytes()
            header = assets.ATLAS_HEADER.unpack_from(blob)
            self.assertTrue(header[4] & assets.ATLAS_FLAG_OPAQUE)
            rgb_off = assets.ATLAS_HEADER.size + 2 * assets.ATLAS_FRAME.size
            def lum(x: int, y: int) -> int:
                pixel = struct.unpack_from("<H", blob, rgb_off + (y * header[1] + x) * 2)[0]
                r, g, b = (pixel >> 11) & 31, (pixel >> 5) & 63, pixel & 31
                return r + g + b
            self.assertGreater(lum(0, 0), 40)
            self.assertGreater(lum(7, 0), 40)
            self.assertGreater(lum(0, 7), 40)
            self.assertGreater(lum(8, 0), 40)

    def test_neon_wall_tiles_darken_seams_and_keep_floor_in_bounds(self) -> None:
        path = ROOT / "projects/neon_maze_25d/assets_src/tactical_materials.png"
        image = Image.open(path).convert("RGB")
        self.assertEqual(image.size, (512, 128))
        pixels = image.load()
        for cell in range(3):
            x0 = cell * 128
            mid, foot = [], []
            for x in range(x0, x0 + 128):
                column = [(pixels[x, y][0] * 3 + pixels[x, y][1] * 6 +
                           pixels[x, y][2]) // 10 for y in range(128)]
                body = column[:116]
                self.assertFalse(min(body) >= 168 and (max(body) - min(body)) <= 28,
                                 "full-height highlight column in wall cell")
                mid.append(sum(column[40:80]) / 40)
                foot.append(sum(column[116:128]) / 12)
            self.assertLess(sum(foot) / len(foot), sum(mid) / len(mid) * 0.85)
        for y in range(64, 128):
            for x in range(384, 448):
                self.assertNotEqual(pixels[x, y], (0, 0, 0))

    def test_generated_ids_include_validated_animation(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / "assets_ids.h"
            assets.write_asset_ids(output, {"hero_idle", "hero_run"}, [{
                "name": "hero", "frames": ["hero_idle", "hero_run"],
                "frame_ticks": 3}])
            text = output.read_text(encoding="ascii")
            self.assertIn("MOSAICO_ASSET_ID_HERO_IDLE", text)
            self.assertIn("MOSAICO_ANIMATION_HERO_FRAME_COUNT 2U", text)
            self.assertIn("MOSAICO_ANIMATION_HERO_FRAME_TICKS 3U", text)
            with self.assertRaisesRegex(ValueError, "unknown frame"):
                assets.write_asset_ids(output, {"hero_idle"}, [{
                "name": "broken", "frames": ["missing"]}])

    def test_atlas_packs_native_size_variants(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = root / "atlas.png"
            Image.new("RGBA", (8, 8), (255, 0, 0, 255)).save(source)
            output = root / "variants.atlas"
            report = assets.write_atlas(source, {
                "columns": 1, "rows": 1, "output_cell": 8,
                "frames": [{"name": "hero"}],
                "variants": [
                    {"name": "hero_small", "source": "hero", "width": 5, "height": 6},
                    {"name": "hero_wide", "source": "hero", "width": 12, "height": 4},
                ],
            }, output)
            blob = output.read_bytes()
            header = assets.ATLAS_HEADER.unpack_from(blob)
            self.assertEqual(header[3], 3)
            self.assertEqual(report["variants"], 2)
            frames = [assets.ATLAS_FRAME.unpack_from(blob, assets.ATLAS_HEADER.size +
                      index * assets.ATLAS_FRAME.size) for index in range(3)]
            self.assertEqual(frames[1][3:5], (5, 6))
            self.assertEqual(frames[2][3:5], (12, 4))

            with self.assertRaisesRegex(ValueError, "unknown frame"):
                assets.write_atlas(source, {
                    "columns": 1, "rows": 1, "output_cell": 8,
                    "frames": [{"name": "hero"}],
                    "variants": [{"name": "bad", "source": "missing",
                                  "width": 4, "height": 4}],
                }, root / "bad.atlas")

    def test_tilemap_checks_shape_and_tile_references(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            document = {"orientation": "orthogonal", "infinite": False,
                "width": 2, "height": 1, "tilewidth": 32, "tileheight": 32,
                "tileset": "terrain.atlas",
                "layers": [{"type": "tilelayer", "name": "ground", "data": [1, 4]}]}
            source = root / "map.tmj"
            source.write_text(json.dumps(document), encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "tile reference"):
                assets.write_tilemap(source, root / "map.bin")

    def test_wav_is_normalized_to_24khz_pcm16(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = root / "tone.wav"
            with wave.open(str(source), "wb") as wav:
                wav.setnchannels(2); wav.setsampwidth(2); wav.setframerate(48000)
                wav.writeframes(struct.pack("<8h", 1000, -1000, 2000, 0,
                                             3000, 1000, 4000, 2000))
            output = root / "tone.sound"
            report = assets.write_sound(source, output)
            self.assertEqual(report["rate"], 24000)
            self.assertEqual(report["samples"], 2)
            self.assertEqual(output.read_bytes()[:4], b"MSN1")

    def test_long_audio_uses_deterministic_ima_adpcm(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = root / "music.wav"
            with wave.open(str(source), "wb") as wav:
                wav.setnchannels(1); wav.setsampwidth(2); wav.setframerate(24000)
                samples = [((index * 97) % 12000) - 6000 for index in range(24000)]
                wav.writeframes(struct.pack("<24000h", *samples))
            first, second = root / "one.sound", root / "two.sound"
            report = assets.write_sound(source, first)
            assets.write_sound(source, second)
            self.assertEqual(report["type"], "audio_ima_adpcm")
            self.assertEqual(report["bits"], 4)
            self.assertEqual(first.read_bytes(), second.read_bytes())
            self.assertLess(first.stat().st_size, source.stat().st_size // 3)

    def test_project_manifest_uses_declared_outputs_without_dummy_map(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source, output = root / "source", root / "output"
            source.mkdir()
            Image.new("RGBA", (8, 8), (20, 80, 160, 255)).save(source / "sprites.png")
            (source / "atlas.json").write_text(json.dumps({
                "image": "sprites.png", "columns": 1, "rows": 1,
                "output_cell": 8, "frames": [{"name": "hero"}],
            }), encoding="utf-8")
            manifest = source / "game_assets.json"
            manifest.write_text(json.dumps({
                "schema": "mosaico-game-assets/v1",
                "atlases": [{"config": "atlas.json", "output": "sprites.atlas"}],
            }), encoding="utf-8")
            report = assets.compile_assets(source, output, manifest)
            self.assertEqual([item["file"] for item in report["files"]],
                             ["sprites.atlas"])
            self.assertTrue((output / "sprites.atlas").is_file())
            self.assertFalse((output / "level01.map").exists())
            self.assertIn("MOSAICO_ASSET_ID_HERO",
                          (output / "assets_ids.h").read_text(encoding="ascii"))

    def test_project_manifest_rejects_escaping_source(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source, output = root / "source", root / "output"
            source.mkdir()
            manifest = source / "game_assets.json"
            manifest.write_text(json.dumps({
                "schema": "mosaico-game-assets/v1",
                "atlases": [{"config": "../outside.json", "output": "bad.atlas"}],
            }), encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "escapes"):
                assets.compile_assets(source, output, manifest)


if __name__ == "__main__":
    unittest.main()
