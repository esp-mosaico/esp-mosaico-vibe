#!/usr/bin/env python3
"""Render the selected ornate 3D set without artificial piece outlines."""

from pathlib import Path

from PIL import Image, ImageDraw


ROOT = Path(__file__).resolve().parents[1]
PIECES = ROOT / "pieces" / "set-illustrate"
OUT = Path(__file__).resolve().parent / "piece_option_f_3d.png"

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
    "r": 39,
    "n": 39,
    "b": 39,
    "q": 39,
    "k": 39,
    "p": 51,
}


def prepare_piece(piece: str) -> Image.Image:
    source = Image.open(PIECES / FILES[piece]).convert("RGBA")
    kind = piece.lower()
    height = 84 if kind == "p" else 64
    return source.resize((WIDTHS[kind], height), Image.LANCZOS)


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
