#!/usr/bin/env python3
"""Render screen-first chess-piece concepts for a 55 px board cell."""

from pathlib import Path

from PIL import Image, ImageDraw, ImageFont


ROOT = Path(__file__).resolve().parents[1]
OUT = Path(__file__).resolve().parent
FONT = ROOT / "fonts" / "DejaVuSans.ttf"

FRAME = 20
CELL = 55
LIGHT = "#E9D2A4"
DARK = "#5D2934"
GOLD = "#E2B84A"
INK = "#180D0D"
IVORY = "#FFF0CF"
BLACK_PIECE = "#2A1720"

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

GLYPHS = {
    "k": "♚",
    "q": "♛",
    "r": "♜",
    "b": "♝",
    "n": "♞",
    "p": "♟",
}


def board() -> Image.Image:
    image = Image.new("RGB", (480, 480), INK)
    draw = ImageDraw.Draw(image)
    for row in range(8):
        for file in range(8):
            x = FRAME + file * CELL
            y = FRAME + row * CELL
            color = LIGHT if (file + (7 - row)) % 2 else DARK
            draw.rectangle((x, y, x + CELL - 1, y + CELL - 1), fill=color)
    draw.rectangle(
        (FRAME - 2, FRAME - 2, FRAME + 8 * CELL + 1, FRAME + 8 * CELL + 1),
        outline=GOLD,
        width=3,
    )
    return image


def fit_font(size: int) -> ImageFont.FreeTypeFont:
    return ImageFont.truetype(str(FONT), size)


def render_tokens() -> Image.Image:
    image = board()
    draw = ImageDraw.Draw(image)
    glyph_font = fit_font(34)
    for row, pieces in enumerate(START):
        for file, piece in enumerate(pieces):
            if piece == ".":
                continue
            cx = FRAME + file * CELL + CELL // 2
            cy = FRAME + row * CELL + CELL // 2
            white = piece.isupper()
            fill = IVORY if white else BLACK_PIECE
            glyph = DARK if white else IVORY
            draw.ellipse(
                (cx - 22, cy - 22, cx + 22, cy + 22),
                fill=fill,
                outline=GOLD,
                width=3,
            )
            draw.text(
                (cx, cy + 1),
                GLYPHS[piece.lower()],
                font=glyph_font,
                fill=glyph,
                anchor="mm",
                stroke_width=1,
                stroke_fill=GOLD if not white else "#7A3A46",
            )
    return image


def render_silhouettes() -> Image.Image:
    image = board()
    draw = ImageDraw.Draw(image)
    glyph_font = fit_font(48)
    for row, pieces in enumerate(START):
        for file, piece in enumerate(pieces):
            if piece == ".":
                continue
            cx = FRAME + file * CELL + CELL // 2
            cy = FRAME + row * CELL + CELL // 2 + 1
            white = piece.isupper()
            fill = IVORY if white else BLACK_PIECE
            stroke = "#651F2D" if white else GOLD
            draw.text(
                (cx, cy),
                GLYPHS[piece.lower()],
                font=glyph_font,
                fill=fill,
                anchor="mm",
                stroke_width=2,
                stroke_fill=stroke,
            )
    return image


if __name__ == "__main__":
    render_tokens().save(OUT / "piece_option_c_tokens.png")
    render_silhouettes().save(OUT / "piece_option_d_silhouettes.png")
    print("wrote piece_option_c_tokens.png and piece_option_d_silhouettes.png")
