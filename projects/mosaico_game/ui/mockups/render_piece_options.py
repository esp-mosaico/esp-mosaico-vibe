#!/usr/bin/env python3
"""Render 480x480 chess-piece readability options without changing GSP."""

from pathlib import Path

from PIL import Image, ImageChops, ImageColor, ImageFilter


ROOT = Path(__file__).resolve().parents[1]
PIECES = ROOT / "pieces"
OUT = Path(__file__).resolve().parent

SIZE = 480
FRAME = 20
CELL = 55
LIGHT = "#F0D9A6"
DARK = "#7A3A46"
BORDER = "#E2B84A"
WHITE_OUTLINE = "#4A1620"
BLACK_OUTLINE = "#F0D28A"

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


def outlined_piece(path: Path, piece: str) -> Image.Image:
    source = Image.open(path).convert("RGBA")
    alpha_box = source.getchannel("A").getbbox()
    if alpha_box is not None:
        source = source.crop(alpha_box)

    target_h = 47 if piece.lower() == "p" else 51
    target_w = 39 if piece.lower() == "p" else 43
    source = source.resize((target_w, target_h), Image.LANCZOS)

    alpha = source.getchannel("A")
    dilated = alpha.filter(ImageFilter.MaxFilter(5))
    outline_alpha = ImageChops.subtract(dilated, alpha)
    color = WHITE_OUTLINE if piece.isupper() else BLACK_OUTLINE
    outline = Image.new("RGBA", source.size, color)
    outline.putalpha(outline_alpha)
    outline.alpha_composite(source)
    return outline


def render(piece_root: Path) -> Image.Image:
    image = Image.new("RGB", (SIZE, SIZE), "#110B08")
    for row, pieces in enumerate(START):
        for file, piece in enumerate(pieces):
            x = FRAME + file * CELL
            y = FRAME + row * CELL
            color = LIGHT if (file + (7 - row)) % 2 else DARK
            tile = Image.new("RGB", (CELL, CELL), color)
            image.paste(tile, (x, y))
            if piece == ".":
                continue
            sprite = outlined_piece(piece_root / FILES[piece], piece)
            px = x + (CELL - sprite.width) // 2
            py = y + CELL - sprite.height - 1
            image.paste(sprite, (px, py), sprite)

    # Keep the existing gold perimeter treatment.
    for offset in range(3):
        x0 = FRAME - offset
        y0 = FRAME - offset
        x1 = FRAME + 8 * CELL - 1 + offset
        y1 = FRAME + 8 * CELL - 1 + offset
        for x in range(x0, x1 + 1):
            image.putpixel((x, y0), ImageColor.getrgb(BORDER))
            image.putpixel((x, y1), ImageColor.getrgb(BORDER))
        for y in range(y0, y1 + 1):
            image.putpixel((x0, y), ImageColor.getrgb(BORDER))
            image.putpixel((x1, y), ImageColor.getrgb(BORDER))
    return image


if __name__ == "__main__":
    option_a = render(PIECES)
    option_b = render(PIECES / "set-illustrate")
    option_a.save(OUT / "piece_option_a.png")
    option_b.save(OUT / "piece_option_b.png")
    print("wrote piece_option_a.png and piece_option_b.png")
