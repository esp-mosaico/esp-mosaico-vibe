#!/usr/bin/env python3
"""Bake device-safe heat background and the 战 watermark animation."""

from __future__ import annotations

import math
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont
import numpy as np

HERE = Path(__file__).resolve().parent
PIECES = HERE / "pieces"

INK = np.array([0x11, 0x0B, 0x08], dtype=np.float64)
HEAT = np.array([0x5A, 0x18, 0x1C], dtype=np.float64)
STAMP = (0xE8, 0x5A, 0x6C, 255)
GOLD = (0xE2, 0xB8, 0x4A, 255)
LOSS = (0xEA, 0x51, 0x65, 255)
PAPER = (0xFA, 0xF1, 0xDB, 255)
RIBBON_BG = (0x1F, 0x0E, 0x0C, 246)
# Rotated vermillion stamp, not a book face. Kai (楷) reads as a
# carved seal on this heat/gold UI; Song/Ming print type does not.
FONT = "/usr/share/fonts/truetype/arphic/ukai.ttc"
FONT_INDEX = 0

# Displayed 1:1 so the baked wash matches the full-screen heat PNG.
WAR_X, WAR_Y, WAR_S = 160, 64, 160
WAR_FRAMES = 30
WAR_MS = 80


def quant_rgb565(r: np.ndarray, g: np.ndarray, b: np.ndarray) -> tuple[np.ndarray, np.ndarray, np.ndarray]:
    r5 = np.clip(np.rint(r / 255.0 * 31.0), 0, 31)
    g6 = np.clip(np.rint(g / 255.0 * 63.0), 0, 63)
    b5 = np.clip(np.rint(b / 255.0 * 31.0), 0, 31)
    return (r5 * 255.0 / 31.0, g6 * 255.0 / 63.0, b5 * 255.0 / 31.0)


def dither_rgb565(rgb: np.ndarray) -> Image.Image:
    """Floyd–Steinberg to RGB565 so a dark vertical heat wash does not band."""
    work = rgb.astype(np.float64).copy()
    h, w, _ = work.shape
    rng = np.random.default_rng(11)
    work += rng.normal(0.0, 1.35, work.shape)
    for y in range(h):
        for x in range(w):
            old = work[y, x].copy()
            qr, qg, qb = quant_rgb565(old[0], old[1], old[2])
            new = np.array([qr, qg, qb], dtype=np.float64)
            work[y, x] = new
            err = old - new
            if x + 1 < w:
                work[y, x + 1] += err * (7 / 16)
            if y + 1 < h:
                if x > 0:
                    work[y + 1, x - 1] += err * (3 / 16)
                work[y + 1, x] += err * (5 / 16)
                if x + 1 < w:
                    work[y + 1, x + 1] += err * (1 / 16)
    return Image.fromarray(np.clip(work, 0, 255).astype(np.uint8), "RGB")


def render_heat() -> Image.Image:
    t = np.linspace(0.0, 1.0, 480)[:, None]
    t = t * t * (3.0 - 2.0 * t)
    col = INK * (1.0 - t) + HEAT * t
    rgb = np.broadcast_to(col[:, None, :], (480, 480, 3)).copy()
    return dither_rgb565(rgb)


def render_war_glyph() -> Image.Image:
    font = ImageFont.truetype(FONT, 168, index=FONT_INDEX)
    canvas = Image.new("RGBA", (320, 320), (0, 0, 0, 0))
    draw = ImageDraw.Draw(canvas)
    draw.text((160, 168), "战", font=font, fill=STAMP, anchor="mm")
    rotated = canvas.rotate(-14, resample=Image.BICUBIC, expand=False)
    boxed = rotated.crop(rotated.getbbox())
    out = Image.new("RGBA", (WAR_S, WAR_S), (0, 0, 0, 0))
    x = (WAR_S - boxed.width) // 2
    y = (WAR_S - boxed.height) // 2
    out.paste(boxed, (x, y), boxed)
    return out


def render_war_gif(heat: Image.Image) -> None:
    crop = heat.crop((WAR_X, WAR_Y, WAR_X + WAR_S, WAR_Y + WAR_S)).convert("RGBA")
    glyph = render_war_glyph()
    rgb_frames: list[Image.Image] = []
    for i in range(WAR_FRAMES):
        t = i / WAR_FRAMES
        breath = 0.5 + 0.5 * math.cos(2.0 * math.pi * t)
        opacity = 0.42 + 0.20 * breath
        scale = 0.96 + 0.08 * breath
        size = max(8, int(round(WAR_S * scale)))
        scaled = glyph.resize((size, size), Image.LANCZOS)
        layer = Image.new("RGBA", (WAR_S, WAR_S), (0, 0, 0, 0))
        layer.paste(scaled, ((WAR_S - size) // 2, (WAR_S - size) // 2), scaled)
        r, g, b, a = layer.split()
        a = a.point(lambda p, o=opacity: int(p * o))
        layer = Image.merge("RGBA", (r, g, b, a))
        rgb_frames.append(Image.alpha_composite(crop, layer).convert("RGB"))
    palette = rgb_frames[0].quantize(colors=256, method=Image.MEDIANCUT)
    pal_frames = [im.quantize(palette=palette, dither=Image.NONE) for im in rgb_frames]
    path = PIECES / "anim_war.gif"
    pal_frames[0].save(
        path,
        save_all=True,
        append_images=pal_frames[1:],
        duration=WAR_MS,
        loop=0,
        optimize=False,
        disposal=2,
    )
    print(f"wrote {path} {path.stat().st_size} bytes frames={WAR_FRAMES} {WAR_S}x{WAR_S}")


def render_result_ribbon(path: Path, accent: tuple[int, int, int, int]) -> None:
    """Render the shared heraldic result banner at its 1:1 display size."""
    image = Image.new("RGBA", (256, 65), (0, 0, 0, 0))
    draw = ImageDraw.Draw(image)
    points = [(1, 1), (255, 1), (231, 42), (255, 64), (1, 64), (25, 42)]
    draw.polygon(points, fill=RIBBON_BG, outline=accent)
    draw.line((33, 10, 223, 10), fill=accent, width=1)
    image.save(path, "PNG", optimize=True)
    print(f"wrote {path} {path.stat().st_size} bytes {image.size} {image.mode}")


def main() -> None:
    PIECES.mkdir(parents=True, exist_ok=True)
    heat = render_heat()
    heat_path = PIECES / "heat_bg.png"
    heat.save(heat_path, "PNG", optimize=True)
    print(f"wrote {heat_path} {heat_path.stat().st_size} bytes {heat.size} {heat.mode}")
    render_war_gif(heat)
    render_result_ribbon(PIECES / "result_ribbon_win.png", PAPER)
    render_result_ribbon(PIECES / "result_ribbon_loss.png", LOSS)
    render_result_ribbon(PIECES / "result_ribbon_draw.png", GOLD)


if __name__ == "__main__":
    main()
