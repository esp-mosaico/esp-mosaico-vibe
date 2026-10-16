#!/usr/bin/env python3
"""Render an ornate but screen-readable 480x480 chess-board concept."""

from pathlib import Path

from PIL import Image, ImageChops, ImageDraw, ImageFilter


ROOT = Path(__file__).resolve().parents[1]
PIECES = ROOT / "pieces"
ALTERNATE = PIECES / "set-illustrate"
OUT = Path(__file__).resolve().parent / "piece_option_e_ornate.png"

FRAME = 20
CELL = 55
LIGHT = "#E9D5AE"
DARK = "#713743"
GOLD = "#E2B84A"
INK = "#110B08"

START = (
    "rnbqkbnr",
    "pppppppp",
    "........",
    "........",
    "........",
    "........",
    "PPPPPPPP",
    "RNBQKBNR",
)

FILES = {
    "r": "bR.png",
    "n": "bN.png",
    "b": "bB.png",
    "q": "bQ.png",
    "k": "bK.png",
    "p": "bP.png",
    "R": "wR.png",
    "N": "wN.png",
    "B": "wB.png",
    "Q": "wQ.png",
    "K": "wK.png",
    "P": "wP.png",
}

WIDTHS = {
    "r": 40,
    "n": 45,
    "b": 35,
    "q": 42,
    "k": 38,
    "p": 35,
}


def unify_black_material(source: Image.Image) -> Image.Image:
    """Map mixed black-piece bodies to one wine-black material."""
    pixels = source.load()
    for y in range(source.height):
        for x in range(source.width):
            red, green, blue, alpha = pixels[x, y]
            if alpha == 0:
                continue
            is_gold = (
                red > 90
                and green > 45
                and blue * 5 < green * 4
                and red * 20 > green * 23
            )
            if is_gold:
                continue
            light = (red * 54 + green * 183 + blue * 19) / (255 * 256)
            pixels[x, y] = (
                int(45 + 120 * light),
                int(25 + 80 * light),
                int(50 + 125 * light),
                alpha,
            )
    return source


def prepare_piece(piece: str) -> Image.Image:
    kind = piece.lower()
    root = ALTERNATE if kind in {"b", "q", "k"} else PIECES
    source = Image.open(root / FILES[piece]).convert("RGBA")
    box = source.getchannel("A").getbbox()
    if box is not None:
        source = source.crop(box)
    if piece.islower():
        source = unify_black_material(source)

    target_h = 49 if kind == "p" else 52
    source = source.resize((WIDTHS[kind], target_h), Image.LANCZOS)

    alpha = source.getchannel("A")
    rim = ImageChops.subtract(alpha.filter(ImageFilter.MaxFilter(3)), alpha)
    rim = rim.point(lambda value: value * 3 // 4)
    rim_color = "#3B1520" if piece.isupper() else "#F4D58C"
    outlined = Image.new("RGBA", source.size, rim_color)
    outlined.putalpha(rim)
    outlined.alpha_composite(source)

    shadow = alpha.filter(ImageFilter.GaussianBlur(1.2))
    shadow = shadow.point(lambda value: value // 3)
    result = Image.new("RGBA", (source.width + 4, source.height + 4))
    shadow_layer = Image.new("RGBA", result.size, "#080404")
    shadow_canvas = Image.new("L", result.size)
    shadow_canvas.paste(shadow, (3, 3))
    shadow_layer.putalpha(shadow_canvas)
    result.alpha_composite(shadow_layer)
    result.alpha_composite(outlined, (1, 1))
    return result


def render() -> Image.Image:
    image = Image.new("RGB", (480, 480), INK)
    draw = ImageDraw.Draw(image)
    for row, pieces in enumerate(START):
        for file, piece in enumerate(pieces):
            x = FRAME + file * CELL
            y = FRAME + row * CELL
            color = LIGHT if (file + (7 - row)) % 2 else DARK
            draw.rectangle((x, y, x + CELL - 1, y + CELL - 1), fill=color)
            if piece == ".":
                continue
            sprite = prepare_piece(piece)
            px = x + (CELL - sprite.width) // 2
            py = y + CELL - sprite.height
            image.paste(sprite, (px, py), sprite)

    draw.rectangle(
        (FRAME - 2, FRAME - 2, FRAME + 8 * CELL + 1, FRAME + 8 * CELL + 1),
        outline=GOLD,
        width=3,
    )
    return image


if __name__ == "__main__":
    render().save(OUT)
    print(f"wrote {OUT}")
