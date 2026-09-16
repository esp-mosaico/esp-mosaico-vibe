#!/usr/bin/env python3
"""Deterministically isolate the generated drone and weapon cells."""
from pathlib import Path
import os
import random
from PIL import Image, ImageDraw, ImageFilter, ImageOps

root = Path(__file__).resolve().parent

def save_atomic(image, destination):
    temporary = destination.with_name(f"{destination.name}.tmp.{os.getpid()}.png")
    image.save(temporary, format="PNG")
    os.replace(temporary, destination)

enemy_cell = 96
enemy_sheet = Image.new("RGBA", (enemy_cell * 7, enemy_cell))
weapon_cell = 176
weapon_sheet = Image.new("RGBA", (weapon_cell * 2, weapon_cell))
resampling = getattr(Image, "Resampling", Image)
run_strip = Image.open(root / "enemy_run_strip_source.png").convert("RGBA")
cell_width = run_strip.width // 3
sprites = [run_strip.crop((i * cell_width, 0,
                          (i + 1) * cell_width if i < 2 else run_strip.width,
                          run_strip.height)) for i in range(3)]
processed = []
for index, sprite in enumerate(sprites):
    alpha_bounds = sprite.getchannel("A").getbbox()
    if not alpha_bounds:
        raise RuntimeError(f"generated sprite cell {index} has no alpha content")
    sprite = sprite.crop(alpha_bounds)
    sprite.thumbnail((enemy_cell - 8, enemy_cell - 8), resampling.LANCZOS)
    sprite = sprite.filter(ImageFilter.UnsharpMask(radius=.7, percent=105, threshold=2))
    processed.append(sprite)
    enemy_sheet.alpha_composite(sprite,
        (index * enemy_cell + (enemy_cell - sprite.width) // 2,
         enemy_cell - 4 - sprite.height))

def paste_variant(index, source, tint):
    variant = source.copy()
    pixels = variant.load()
    for y in range(variant.height):
        for x in range(variant.width):
            r, g, b, a = pixels[x, y]
            if a < 16:
                continue
            pixels[x, y] = (min(255, int(r * tint[0])), min(255, int(g * tint[1])),
                            min(255, int(b * tint[2])), a)
    enemy_sheet.alpha_composite(variant,
        (index * enemy_cell + (enemy_cell - variant.width) // 2,
         enemy_cell - 4 - variant.height))

paste_variant(3, processed[1], (1.15, 0.95, 0.72))
paste_variant(4, processed[0], (1.45, 0.55, 0.45))
paste_variant(5, processed[2], (1.20, 1.05, 0.65))
down = processed[1].rotate(90, expand=True, resample=resampling.BICUBIC)
down.thumbnail((enemy_cell - 8, enemy_cell // 2), resampling.LANCZOS)
paste_variant(6, down, (0.88, 0.78, 0.70))
rifle = Image.open(root / "k98_rifle_source.png").convert("RGBA")
alpha_bounds = rifle.getchannel("A").getbbox()
if not alpha_bounds:
    raise RuntimeError("98K sprite has no alpha content")
rifle = rifle.crop(alpha_bounds)
rifle.thumbnail((weapon_cell - 10, weapon_cell - 10), resampling.LANCZOS)
weapon_sheet.alpha_composite(rifle, (weapon_cell - rifle.width - 6,
                                     weapon_cell - rifle.height - 4))
bolted = rifle.rotate(-14, expand=True, resample=resampling.BICUBIC)
bolted.thumbnail((weapon_cell - 6, weapon_cell - 6), resampling.LANCZOS)
weapon_sheet.alpha_composite(bolted, (weapon_cell + 16,
                                      weapon_cell - bolted.height + 10))
save_atomic(enemy_sheet, root / "enemy_run.png")
save_atomic(weapon_sheet, root / "weapon.png")

# Binary-alpha HUD. Corners stay A=0 so the world shows through the rings.
controls = Image.new("RGBA", (192, 96), (0, 0, 0, 0))
draw = ImageDraw.Draw(controls)
draw.ellipse((4, 4, 92, 92), outline=(120, 168, 158, 255), width=4)
draw.ellipse((14, 14, 82, 82), outline=(48, 72, 68, 255), width=2)
draw.ellipse((100, 4, 188, 92), outline=(196, 118, 78, 255), width=4)
draw.ellipse((132, 36, 156, 60), fill=(246, 224, 201, 255))
draw.line((144, 18, 144, 32), fill=(255, 239, 214, 255), width=3)
draw.line((144, 64, 144, 78), fill=(255, 239, 214, 255), width=3)
draw.line((116, 48, 130, 48), fill=(255, 239, 214, 255), width=3)
draw.line((158, 48, 172, 48), fill=(255, 239, 214, 255), width=3)
save_atomic(controls, root / "controls.png")

# Keep the panorama in a square atlas cell while preserving a wide horizon.
# The lower area is never sampled by the renderer.
panorama_source = Image.open(root / "tactical_panorama_source.png").convert("RGB")
panorama = ImageOps.fit(panorama_source, (508, 205), method=resampling.LANCZOS)
environment = Image.new("RGB", (512, 256), panorama.getpixel((254, 204)))
environment.paste(panorama, (2, 2))
save_atomic(environment, root / "tactical_panorama.png")

def fill_tile(size, painter):
    tile = Image.new("RGB", (size, size))
    pixels = tile.load()
    for y in range(size):
        for x in range(size):
            pixels[x, y] = painter(x, y)
    return tile

tile_rng = random.Random(77)

def fence_color(x, y):
    grit = tile_rng.randint(-8, 8)
    post = x % 18 < 3
    rail = 26 <= (y % 40) <= 29
    panel = (y % 40) < 2
    if post:
        return (92 + grit, 96 + grit, 100 + grit)
    if rail or panel:
        return (118 + grit, 122 + grit, 126 + grit)
    return (148 + grit, 152 + grit, 156 + grit)

def brick_color(x, y):
    grit = tile_rng.randint(-10, 10)
    if x < 38:
        rivet = x % 9 == 3 and y % 14 == 6
        seam = y % 16 < 2 or x < 3 or x > 34
        if rivet:
            return (196, 168, 88)
        if seam:
            return (86 + grit, 58, 22)
        return (176 + grit // 2, 138 + grit // 3, 42)
    window = 22 < (x % 48) < 42 and 30 < y < 90
    mortar = (y % 16) < 2 or ((x + (0 if (y // 16) % 2 else 16)) % 32) < 2
    if window:
        return (36 + grit // 2, 58 + grit, 78 + grit)
    if mortar:
        return (72 + grit, 54 + grit, 46 + grit)
    return (152 + grit, 70 + grit // 2, 48 + grit // 3)

def container_color(x, y):
    grit = tile_rng.randint(-10, 10)
    rib = x % 10 < 3
    rust = 96 < y < 104
    band = y % 22 < 2
    if rust:
        return (118 + grit, 72 + grit, 42 + grit)
    if rib:
        return (48 + grit, 86 + grit, 58 + grit)
    if band:
        return (56 + grit, 96 + grit, 64 + grit)
    return (62 + grit, 108 + grit, 72 + grit)

def floor_color(x, y):
    grit = tile_rng.randint(-14, 14)
    if y < 64:
        seam = 18 if x % 28 == 0 or y % 22 == 0 else 0
        return (max(40, min(160, 126 + grit - seam)),
                max(24, min(96, 74 + grit - seam)),
                max(12, min(48, 32 + grit // 2 - seam)))
    plank = 22 if x % 18 == 0 else 0
    ring = 14 if y % 8 == 0 else 0
    return (max(110, min(210, 186 + grit - plank)),
            max(88, min(170, 142 + grit - plank - ring)),
            max(48, min(110, 78 + grit // 2 - plank)))

def finish_wall_tile(tile):
    """Drop full-height highlight columns and add a dark skirting band."""
    px = tile.load()
    size = tile.width
    for x in range(size):
        lo, hi = 255, 0
        for y in range(size):
            r, g, b = px[x, y][:3]
            lum = (r * 3 + g * 6 + b) // 10
            if lum < lo:
                lo = lum
            if lum > hi:
                hi = lum
        if lo >= 168 and (hi - lo) <= 28:
            for y in range(size):
                r, g, b = px[x, y][:3]
                px[x, y] = ((r * 3 + 78) // 4, (g * 3 + 74) // 4, (b * 3 + 68) // 4)
    footer = max(12, size * 14 // 100)
    start = size - footer
    for y in range(start, size):
        fade = (y - start + 1) / footer
        shade = 1.0 - 0.58 * fade
        if y >= size - 3:
            shade = 0.34
        for x in range(size):
            r, g, b = px[x, y][:3]
            px[x, y] = (max(22, int(r * shade)),
                        max(20, int(g * shade)),
                        max(18, int(b * shade)))
    return tile


def load_wall_tiles(size=128):
    source_path = root / "tactical_materials_source.png"
    if source_path.is_file():
        photo = Image.open(source_path).convert("RGB")
        third = max(1, photo.width // 3)
        tiles = []
        for index in range(3):
            left = index * third
            right = photo.width if index == 2 else left + third
            crop = photo.crop((left, 0, right, photo.height))
            tile = ImageOps.fit(crop, (size, size), method=resampling.LANCZOS)
            tiles.append(finish_wall_tile(tile))
        return tiles
    painters = (fence_color, brick_color, container_color)
    return [finish_wall_tile(fill_tile(size, painter)) for painter in painters]


materials = Image.new("RGB", (512, 128))
for index, tile in enumerate(load_wall_tiles(128)):
    materials.paste(tile, (index * 128, 0))
materials.paste(fill_tile(128, floor_color), (384, 0))
save_atomic(materials, root / "tactical_materials.png")

prop_cell = 32
props = Image.new("RGBA", (prop_cell * 4, prop_cell), (0, 0, 0, 0))
prop_draw = ImageDraw.Draw(props)

def box(x0, y0, x1, y1, fill, outline=None, width=1):
    prop_draw.rectangle((x0, y0, x1, y1), fill=fill, outline=outline, width=width)

# ammo crate
box(4, 10, 27, 29, (72, 92, 46, 255), (28, 38, 18, 255), 2)
box(6, 14, 25, 18, (214, 186, 64, 255))
box(12, 8, 19, 12, (58, 74, 36, 255))
# medkit
box(36, 10, 59, 29, (236, 236, 232, 255), (148, 48, 48, 255), 2)
box(44, 13, 50, 26, (196, 42, 48, 255))
box(39, 17, 55, 22, (196, 42, 48, 255))
# barrel
prop_draw.ellipse((68, 6, 91, 16), fill=(62, 58, 54, 255), outline=(28, 26, 24, 255))
box(68, 11, 91, 26, (74, 70, 64, 255))
prop_draw.ellipse((68, 22, 91, 30), fill=(48, 46, 42, 255), outline=(24, 22, 20, 255))
box(68, 16, 91, 18, (196, 118, 42, 255))
# sandbag
prop_draw.ellipse((100, 14, 123, 30), fill=(168, 132, 78, 255), outline=(96, 72, 40, 255))
prop_draw.ellipse((104, 10, 119, 20), fill=(186, 148, 88, 255))
save_atomic(props, root / "props.png")
