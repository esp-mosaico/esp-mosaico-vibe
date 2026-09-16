#!/usr/bin/env python3
"""Deterministically isolate the generated drone and weapon cells."""
from pathlib import Path
import os
from PIL import Image, ImageDraw, ImageFilter, ImageOps

root = Path(__file__).resolve().parent

def save_atomic(image, destination):
    temporary = destination.with_name(f"{destination.name}.tmp.{os.getpid()}.png")
    image.save(temporary, format="PNG")
    os.replace(temporary, destination)

enemy_cell = 156
enemy_sheet = Image.new("RGBA", (enemy_cell * 3, enemy_cell))
weapon_cell = 280
weapon_sheet = Image.new("RGBA", (weapon_cell, weapon_cell))
resampling = getattr(Image, "Resampling", Image)
run_strip = Image.open(root / "enemy_run_strip_source.png").convert("RGBA")
cell_width = run_strip.width // 3
sprites = [run_strip.crop((i * cell_width, 0,
                          (i + 1) * cell_width if i < 2 else run_strip.width,
                          run_strip.height)) for i in range(3)]
for index, sprite in enumerate(sprites):
    alpha_bounds = sprite.getchannel("A").getbbox()
    if not alpha_bounds:
        raise RuntimeError(f"generated sprite cell {index} has no alpha content")
    sprite = sprite.crop(alpha_bounds)
    sprite.thumbnail((enemy_cell - 8, enemy_cell - 8), resampling.LANCZOS)
    sprite = sprite.filter(ImageFilter.UnsharpMask(radius=.7, percent=105, threshold=2))
    enemy_sheet.alpha_composite(sprite,
        (index * enemy_cell + (enemy_cell - sprite.width) // 2,
         enemy_cell - 4 - sprite.height))
rifle = Image.open(root / "k98_rifle_source.png").convert("RGBA")
alpha_bounds = rifle.getchannel("A").getbbox()
if not alpha_bounds:
    raise RuntimeError("98K sprite has no alpha content")
rifle = rifle.crop(alpha_bounds)
rifle.thumbnail((weapon_cell - 8, weapon_cell - 8), resampling.LANCZOS)
weapon_sheet.alpha_composite(rifle, ((weapon_cell - rifle.width) // 2,
                                     (weapon_cell - rifle.height) // 2))
save_atomic(enemy_sheet, root / "enemy_run.png")
save_atomic(weapon_sheet, root / "weapon.png")

# Binary-alpha stippling preserves the translucent HUD feel while avoiding
# per-pixel RGB565 alpha blending on the device.
controls = Image.new("RGBA", (192, 96))
pixels = controls.load()
draw = ImageDraw.Draw(controls)
draw.ellipse((4, 4, 92, 92), outline=(158, 192, 183, 255), width=3)
draw.ellipse((17, 17, 79, 79), outline=(72, 106, 99, 255), width=2)
draw.ellipse((100, 4, 188, 92), outline=(221, 130, 92, 255), width=3)
draw.ellipse((136, 40, 152, 56), fill=(246, 224, 201, 255))
draw.line((144, 17, 144, 34), fill=(255, 239, 214, 255), width=2)
draw.line((144, 62, 144, 79), fill=(255, 239, 214, 255), width=2)
draw.line((113, 48, 130, 48), fill=(255, 239, 214, 255), width=2)
draw.line((158, 48, 175, 48), fill=(255, 239, 214, 255), width=2)
save_atomic(controls, root / "controls.png")

# Keep the panorama in a square atlas cell while preserving a wide horizon.
# The lower area is never sampled by the renderer.
panorama_source = Image.open(root / "tactical_panorama_source.png").convert("RGB")
panorama = ImageOps.fit(panorama_source, (508, 205), method=resampling.LANCZOS)
environment = Image.new("RGB", (512, 256), panorama.getpixel((254, 204)))
environment.paste(panorama, (2, 2))
save_atomic(environment, root / "tactical_panorama.png")

materials_source = Image.open(root / "tactical_materials_source.png").convert("RGB")
materials = ImageOps.fit(materials_source, (384, 128), method=resampling.LANCZOS)
save_atomic(materials, root / "tactical_materials.png")
