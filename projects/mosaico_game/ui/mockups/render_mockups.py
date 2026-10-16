#!/usr/bin/env python3
"""Render static 480x480 design candidates; this does not generate GSP."""

from __future__ import annotations

from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]
OUT = Path(__file__).resolve().parent
PIECES = ROOT / "pieces"
FONT_CJK = ROOT / "fonts" / "cjk.ttf"
FONT_LATIN = ROOT / "fonts" / "DejaVuSans.ttf"

INK = "#110B08"
PANEL = "#1A0E0C"
VERM = "#D41E3C"
GOLD = "#E2B84A"
PAPER = "#F6E6C8"
PAPER_HI = "#FFF4DC"
SQ_LIGHT = "#E7C995"
SQ_DARK = "#68303A"
LAST = "#B8773A"
CHECK = "#FF3B55"
MUTED = "#9D8B78"

BOARD_X = 32
BOARD_Y = 64
CELL = 52


def font(size: int) -> ImageFont.FreeTypeFont:
    return ImageFont.truetype(str(FONT_CJK), size)


def latin(size: int) -> ImageFont.FreeTypeFont:
    return ImageFont.truetype(str(FONT_LATIN), size)


def text(
    draw: ImageDraw.ImageDraw,
    xy: tuple[int, int],
    value: str,
    size: int,
    color: str,
    anchor: str = "la",
) -> None:
    draw.text(xy, value, font=font(size), fill=color, anchor=anchor)


def base() -> Image.Image:
    image = Image.new("RGB", (480, 480), INK)
    draw = ImageDraw.Draw(image)
    draw.rectangle((0, 0, 479, 63), fill=PANEL)
    draw.line((0, 63, 479, 63), fill=GOLD, width=2)
    return image


def hud(image: Image.Image, status: str, mode: str, local: str) -> None:
    draw = ImageDraw.Draw(image)
    text(draw, (16, 10), mode, 15, GOLD)
    text(draw, (16, 37), f"{local}  {status}", 16, PAPER_HI)
    draw.rounded_rectangle((420, 8, 467, 55), radius=4, outline=GOLD, width=2)
    draw.line((432, 22, 455, 22), fill=GOLD, width=2)
    draw.line((432, 31, 455, 31), fill=GOLD, width=2)
    draw.line((432, 40, 455, 40), fill=GOLD, width=2)


def square_box(square: str) -> tuple[int, int, int, int]:
    file = ord(square[0]) - ord("a")
    rank = int(square[1]) - 1
    x = BOARD_X + file * CELL
    y = BOARD_Y + (7 - rank) * CELL
    return x, y, x + CELL - 1, y + CELL - 1


def board(image: Image.Image) -> None:
    draw = ImageDraw.Draw(image)
    for rank_top in range(8):
        for file in range(8):
            x = BOARD_X + file * CELL
            y = BOARD_Y + rank_top * CELL
            color = SQ_LIGHT if (file + rank_top) % 2 else SQ_DARK
            draw.rectangle((x, y, x + CELL - 1, y + CELL - 1), fill=color)
    draw.rectangle(
        (BOARD_X - 2, BOARD_Y - 2, BOARD_X + CELL * 8 + 1, 479),
        outline=VERM,
        width=2,
    )


def piece_image(name: str) -> Image.Image:
    source = Image.open(PIECES / f"{name}.png").convert("RGBA")
    maximum = (45, 59 if name[1] != "P" else 63)
    source.thumbnail(maximum, Image.LANCZOS)
    return source


def put_piece(image: Image.Image, square: str, name: str) -> None:
    piece = piece_image(name)
    box = square_box(square)
    x = box[0] + (CELL - piece.width) // 2
    y = box[3] + 1 - piece.height
    image.paste(piece, (x, y), piece)


def start_pieces(image: Image.Image) -> None:
    order = "RNBQKBNR"
    for file, kind in enumerate(order):
        letter = chr(ord("a") + file)
        put_piece(image, f"{letter}1", f"w{kind}")
        put_piece(image, f"{letter}8", f"b{kind}")
        put_piece(image, f"{letter}2", "wP")
        put_piece(image, f"{letter}7", "bP")


def render_play() -> Image.Image:
    image = base()
    hud(image, "轮到你", "对战电脑", "你执白")
    board(image)
    draw = ImageDraw.Draw(image, "RGBA")
    draw.rectangle(square_box("e7"), fill=LAST + "88")
    draw.rectangle(square_box("e5"), fill=LAST + "88")
    draw.rectangle(square_box("g1"), outline=VERM, width=5)
    for target in ("f3", "h3"):
        box = square_box(target)
        cx = (box[0] + box[2]) // 2
        cy = (box[1] + box[3]) // 2
        draw.ellipse((cx - 8, cy - 8, cx + 8, cy + 8),
                     fill=GOLD, outline=PAPER_HI, width=2)
    start_pieces(image)
    return image


def render_current_board() -> Image.Image:
    """Current production layout: 440x440 board, no visible HUD."""
    image = Image.new("RGB", (480, 480), INK)
    draw = ImageDraw.Draw(image)
    frame = 20
    cell = 55
    for rank_top in range(8):
        for file in range(8):
            x = frame + file * cell
            y = frame + rank_top * cell
            color = SQ_LIGHT if (file + rank_top) % 2 else "#52202A"
            draw.rectangle((x, y, x + cell - 1, y + cell - 1), fill=color)
    draw.rectangle((18, 18, 461, 461), outline=VERM, width=2)
    order = "RNBQKBNR"
    for file, kind in enumerate(order):
        for rank, prefix in ((0, "w"), (7, "b")):
            source = Image.open(PIECES / f"{prefix}{kind}.png").convert("RGBA")
            source.thumbnail((39, 64), Image.LANCZOS)
            x = frame + file * cell + (cell - source.width) // 2
            y = frame + (7 - rank) * cell + cell - source.height
            image.paste(source, (x, y), source)
        for rank, prefix in ((1, "w"), (6, "b")):
            source = Image.open(PIECES / f"{prefix}P.png").convert("RGBA")
            source.thumbnail((51, 84), Image.LANCZOS)
            x = frame + file * cell + (cell - source.width) // 2
            y = frame + (7 - rank) * cell + cell - source.height
            image.paste(source, (x, y), source)
    return image


def overlay(image: Image.Image) -> ImageDraw.ImageDraw:
    shade = Image.new("RGBA", image.size, (17, 11, 8, 190))
    image.paste(shade, (0, 0), shade)
    return ImageDraw.Draw(image)


def button(draw: ImageDraw.ImageDraw, box: tuple[int, int, int, int],
           label: str, primary: bool = False) -> None:
    fill = VERM if primary else PAPER
    fg = PAPER_HI if primary else INK
    draw.rounded_rectangle(box, radius=4, fill=fill,
                           outline=GOLD if not primary else VERM, width=2)
    text(draw, ((box[0] + box[2]) // 2, (box[1] + box[3]) // 2),
         label, 18, fg, "mm")


def render_home() -> Image.Image:
    image = base()
    draw = ImageDraw.Draw(image)
    piece = Image.open(PIECES / "wK.png").convert("RGBA")
    piece.thumbnail((172, 265), Image.LANCZOS)
    image.paste(piece, ((480 - piece.width) // 2, 72), piece)
    text(draw, (240, 300), "国际象棋", 40, PAPER_HI, "mm")
    button(draw, (36, 374, 443, 438), "进入游戏", True)
    return image


def render_mode_select() -> Image.Image:
    image = base()
    draw = ImageDraw.Draw(image)
    text(draw, (28, 36), "选择对局", 24, PAPER_HI, "lm")
    draw.rounded_rectangle((28, 88, 451, 250), radius=6,
                           fill=PANEL, outline=VERM, width=3)
    text(draw, (52, 125), "对战电脑", 32, PAPER_HI)
    text(draw, (52, 169), "你执白  完整标准棋规", 16, GOLD)
    button(draw, (286, 177, 427, 230), "开始", True)
    draw.rounded_rectangle((28, 274, 451, 402), radius=6,
                           fill="#17110F", outline="#604938", width=2)
    text(draw, (52, 307), "双设备对决", 27, "#B8A997")
    text(draw, (52, 348), "真机贴合与同步开发中", 15, MUTED)
    draw.rounded_rectangle((343, 294, 427, 330), radius=18,
                           fill="#35151A", outline=VERM, width=1)
    text(draw, (385, 312), "开发中", 14, VERM, "mm")
    text(draw, (28, 447), "返回", 16, GOLD, "lm")
    return image


def render_menu() -> Image.Image:
    image = render_play()
    draw = overlay(image)
    draw.rectangle((0, 226, 479, 479), fill=PANEL, outline=GOLD, width=2)
    text(draw, (28, 258), "对局菜单", 24, PAPER_HI)
    text(draw, (28, 291), "对战电脑  你执白", 14, MUTED)
    draw.rounded_rectangle((412, 240, 459, 287), radius=4,
                           outline=GOLD, width=2)
    text(draw, (435, 263), "关", 16, GOLD, "mm")
    button(draw, (28, 320, 451, 374), "认输")
    button(draw, (28, 390, 451, 444), "返回主菜单")
    text(draw, (240, 463), "两项操作均会二次确认并结束本局", 13, GOLD, "mm")
    return image


def render_promotion() -> Image.Image:
    image = base()
    hud(image, "请选择升变棋子", "双人对决", "你执黑")
    board(image)
    start_pieces(image)
    draw = overlay(image)
    draw.rectangle((0, 210, 479, 479), fill=PANEL, outline=GOLD, width=2)
    text(draw, (28, 243), "黑兵升变", 24, PAPER_HI)
    text(draw, (28, 276), "选择后立即完成着法", 14, MUTED)
    labels = (("bQ", "后"), ("bR", "车"), ("bB", "象"), ("bN", "马"))
    for index, (piece_name, label) in enumerate(labels):
        x = 38 + index * 104
        draw.rounded_rectangle((x, 302, x + 91, 449), radius=4,
                               fill="#2A1814", outline=GOLD, width=2)
        piece = piece_image(piece_name)
        px = x + (92 - piece.width) // 2
        image.paste(piece, (px, 318), piece)
        text(draw, (x + 46, 425), label, 17, PAPER_HI, "mm")
    return image


def render_draw_request() -> Image.Image:
    image = base()
    hud(image, "等待对手", "双人对决", "你执黑")
    board(image)
    start_pieces(image)
    draw = overlay(image)
    draw.rectangle((0, 258, 479, 479), fill=PANEL, outline=GOLD, width=2)
    text(draw, (28, 296), "对方请求和棋", 24, PAPER_HI)
    text(draw, (28, 330), "拒绝后对局继续", 15, MUTED)
    button(draw, (28, 368, 226, 430), "拒绝")
    button(draw, (254, 368, 452, 430), "接受", True)
    return image


def render_pvp_development() -> Image.Image:
    return render_mode_select()


def render_pvp_disconnected() -> Image.Image:
    image = base()
    hud(image, "连接已断开", "双人对决", "你执黑")
    board(image)
    start_pieces(image)
    draw = overlay(image)
    draw.rectangle((0, 244, 479, 479), fill=PANEL, outline=GOLD, width=2)
    text(draw, (28, 282), "与对手断开连接", 24, PAPER_HI)
    text(draw, (28, 316), "正在保留当前棋局", 15, MUTED)
    button(draw, (28, 354, 451, 410), "重新连接", True)
    text(draw, (240, 446), "返回菜单将按退出对局处理", 14, GOLD, "mm")
    return image


def main() -> None:
    OUT.mkdir(parents=True, exist_ok=True)
    images = {
        "home.png": render_home(),
        "mode_select.png": render_mode_select(),
        "current_board_no_hud.png": render_current_board(),
        "game_menu.png": render_menu(),
        "promotion.png": render_promotion(),
        "draw_request.png": render_draw_request(),
        "pvp_development.png": render_pvp_development(),
        "pvp_disconnected.png": render_pvp_disconnected(),
    }
    for name, image in images.items():
        path = OUT / name
        image.save(path, "PNG", optimize=True)
        print(f"wrote {path} {image.size}")


if __name__ == "__main__":
    main()
