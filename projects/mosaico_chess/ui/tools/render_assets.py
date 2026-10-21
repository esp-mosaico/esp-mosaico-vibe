#!/usr/bin/env python3
"""Derive runtime UI assets from confirmed designs."""

from __future__ import annotations

from pathlib import Path

from PIL import Image, ImageDraw

from render_localized_assets import write_runtime_assets

UI = Path(__file__).resolve().parents[1]
DESIGNS = UI / "designs"
OVERLAYS = UI / "runtime" / "overlays"
LOCALIZED = OVERLAYS / "localized"

def render_approved_runtime_assets() -> None:
    OVERLAYS.mkdir(parents=True, exist_ok=True)
    crops = {
        "promotion_panel.png": (
            "s30_promotion_480.png", (32, 132, 448, 368)
        ),
        "game_menu_panel.png": (
            "s30_game_menu_480.png", (56, 148, 424, 332)
        ),
        "resign_panel.png": (
            "s30_resign_confirm_480.png", (52, 142, 428, 348)
        ),
        "connection_lost_panel.png": (
            "s30_connection_lost_480.png", (52, 142, 428, 348)
        ),
        "sync_error_panel.png": (
            "s30_sync_error_480.png", (52, 142, 428, 348)
        ),
    }
    for output_name, (source_name, box) in crops.items():
        panel = Image.open(DESIGNS / source_name).convert("RGBA").crop(box)
        path = OVERLAYS / output_name
        panel.save(path, "PNG", optimize=True)
        print(f"wrote {path} {path.stat().st_size} bytes {panel.size} {panel.mode}")

    frame = Image.open(
        DESIGNS / "s30_play_board_480.png"
    ).convert("RGBA")
    alpha = Image.new("L", frame.size, 255)
    ImageDraw.Draw(alpha).rectangle((20, 20, 459, 459), fill=0)
    frame.putalpha(alpha)
    frame_path = OVERLAYS / "board_frame.png"
    frame.save(frame_path, "PNG", optimize=True)
    print(
        f"wrote {frame_path} {frame_path.stat().st_size} bytes "
        f"{frame.size} {frame.mode}"
    )


def main() -> None:
    render_approved_runtime_assets()
    write_runtime_assets(LOCALIZED)


if __name__ == "__main__":
    main()
