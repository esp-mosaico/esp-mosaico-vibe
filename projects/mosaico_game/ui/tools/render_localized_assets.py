#!/usr/bin/env python3
"""Generate English runtime overlays from confirmed UI designs."""

from __future__ import annotations

from pathlib import Path

from PIL import Image, ImageDraw, ImageFilter, ImageFont

UI = Path(__file__).resolve().parents[1]
CONFIRMED = UI / "designs"
OUT = UI / "runtime" / "overlays" / "localized"
FONT = UI / "fonts" / "DejaVuSans.ttf"
FONT_CJK = UI / "fonts" / "cjk.ttf"

PAPER = "#FFF4DC"
GOLD = "#E2B84A"
VERM = "#D41E3C"
PANEL = "#2B0E0F"
DEVICE_CORNER_RADIUS = 48


def render_image(image: Image.Image) -> Image.Image:
    """Apply the physical display's rounded black corner mask."""
    output = Image.new("RGB", image.size, "black")
    mask = Image.new("L", image.size, 0)
    ImageDraw.Draw(mask).rounded_rectangle(
        (0, 0, image.width - 1, image.height - 1),
        radius=DEVICE_CORNER_RADIUS,
        fill=255,
    )
    output.paste(image.convert("RGB"), (0, 0), mask)
    return output


def font(size: int) -> ImageFont.FreeTypeFont:
    return ImageFont.truetype(str(FONT), size)


def cjk_font(size: int) -> ImageFont.FreeTypeFont:
    return ImageFont.truetype(str(FONT_CJK), size)


def erase(image: Image.Image, box: tuple[int, int, int, int]) -> None:
    """Replace text with a row-wise interpolation of its surrounding pixels."""
    x0, y0, x1, y1 = box
    px = image.load()
    for y in range(y0, y1):
        left = px[max(0, x0 - 3), y]
        right = px[min(image.width - 1, x1 + 3), y]
        width = max(1, x1 - x0 - 1)
        for x in range(x0, x1):
            t = (x - x0) / width
            px[x, y] = tuple(round(a * (1 - t) + b * t) for a, b in zip(left, right))


def erase_bright_glyphs(
    image: Image.Image, box: tuple[int, int, int, int]
) -> None:
    """Remove light title glyphs without blurring the entire title row."""
    x0, y0, x1, y1 = box
    source = image.crop(box)
    mask = Image.new("L", source.size, 0)
    source_pixels = source.load()
    mask_pixels = mask.load()
    for y in range(source.height):
        for x in range(source.width):
            r, g, b = source_pixels[x, y]
            if r > 150 and g > 120 and b > 90 and r + g + b > 430:
                mask_pixels[x, y] = 255
    mask = mask.filter(ImageFilter.MaxFilter(11))
    replacement = source.copy()
    pixels = replacement.load()
    known = [
        [mask.getpixel((x, y)) == 0 for x in range(source.width)]
        for y in range(source.height)
    ]
    remaining = sum(not value for row in known for value in row)
    while remaining:
        updates: list[tuple[int, int, tuple[int, int, int]]] = []
        for y in range(source.height):
            for x in range(source.width):
                if known[y][x]:
                    continue
                neighbors = [
                    pixels[nx, ny]
                    for ny in range(max(0, y - 1), min(source.height, y + 2))
                    for nx in range(max(0, x - 1), min(source.width, x + 2))
                    if (nx != x or ny != y) and known[ny][nx]
                ]
                if neighbors:
                    updates.append(
                        (
                            x,
                            y,
                            tuple(
                                sum(color[channel] for color in neighbors)
                                // len(neighbors)
                                for channel in range(3)
                            ),
                        )
                    )
        for x, y, color in updates:
            pixels[x, y] = color
            known[y][x] = True
        remaining -= len(updates)
    image.paste(replacement, (x0, y0), mask)


def label(
    image: Image.Image,
    box: tuple[int, int, int, int],
    value: str,
    size: int,
    color: str = PAPER,
) -> None:
    erase(image, box)
    draw = ImageDraw.Draw(image)
    x0, y0, x1, y1 = box
    draw.text(((x0 + x1) / 2, (y0 + y1) / 2), value, font=font(size),
              fill=color, anchor="mm")


def toggle(image: Image.Image, english: bool) -> None:
    draw = ImageDraw.Draw(image, "RGBA")
    box = (199, 24, 281, 56)
    draw.rounded_rectangle(box, radius=4, fill=(17, 11, 8, 224),
                           outline=GOLD, width=1)
    draw.text((219, 40), "中", font=cjk_font(15),
              fill=GOLD if not english else "#8D8578", anchor="mm")
    draw.line((239, 30, 239, 50), fill=GOLD, width=1)
    draw.text((260, 40), "EN", font=font(13),
              fill=GOLD if english else "#8D8578", anchor="mm")


def open_design(name: str) -> Image.Image:
    return Image.open(CONFIRMED / name).convert("RGB")


def render_home() -> tuple[Image.Image, Image.Image]:
    zh = open_design("s00_home_480.png")
    toggle(zh, False)
    en = open_design("s00_home_480.png")
    title_box = (120, 278, 360, 327)
    erase_bright_glyphs(en, title_box)
    draw = ImageDraw.Draw(en)
    draw.text((240, 302.5), "CHESS", font=font(40),
              fill=PAPER, anchor="mm")
    label(en, (70, 376, 410, 425), "PLAY", 24)
    toggle(en, True)
    return zh, en


def render_mode() -> Image.Image:
    image = open_design("s10_mode_select_480.png")
    label(image, (130, 43, 350, 76), "SELECT MODE", 29)
    label(image, (63, 322, 209, 358), "VS COMPUTER", 19)
    label(image, (263, 322, 418, 358), "TWO-PLAYER", 19)
    label(image, (188, 408, 292, 433), "<  BACK", 16)
    return image


def render_difficulty() -> Image.Image:
    image = open_design("s11_difficulty_480.png")
    label(image, (125, 43, 355, 76), "SELECT LEVEL", 28)
    label(image, (50, 331, 146, 360), "EASY", 19)
    label(image, (191, 331, 289, 360), "NORMAL", 18)
    label(image, (337, 331, 435, 360), "HARD", 19)
    label(image, (188, 408, 292, 433), "<  BACK", 16)
    return image


def render_pairing() -> dict[str, Image.Image]:
    wait = open_design("s20_wait_connect_480_v2.png")
    label(wait, (120, 43, 360, 76), "CONNECTING", 29)
    label(wait, (102, 357, 378, 385), "BRING DEVICES TOGETHER", 17)
    label(wait, (188, 408, 292, 433), "<  BACK", 16)

    ready = open_design("s21_pair_success_480.png")
    label(ready, (120, 43, 360, 76), "MATCH READY", 29)
    label(ready, (85, 357, 395, 385), "CONNECTED · YOU PLAY WHITE", 16)
    label(ready, (188, 408, 292, 433), "START  >", 16)

    failed = open_design("s22_pair_fail_480.png")
    label(failed, (110, 43, 370, 76), "PAIRING FAILED", 28)
    label(failed, (82, 357, 398, 385), "TIMED OUT · MOVE DEVICES CLOSER", 15)
    label(failed, (188, 408, 292, 433), "RETRY  >", 16)
    return {"s20_en.png": wait, "s21_en.png": ready, "s22_en.png": failed}


def render_wait_opponent_start() -> tuple[Image.Image, Image.Image]:
    """Render the design-only state after this player has pressed Start."""
    zh = open_design("s21_pair_success_480.png")
    en = open_design("s21_pair_success_480.png")

    for image in (zh, en):
        erase_bright_glyphs(image, (105, 40, 375, 76))
        # Remove the old status and active Start button without blurring the
        # paired-kings artwork or checkerboard background.
        erase(image, (82, 356, 398, 386))
        erase(image, (174, 400, 306, 442))
        draw = ImageDraw.Draw(image)
        draw.rectangle(
            (123, 339, 357, 351),
            fill="#160B0A",
            outline=GOLD,
            width=2,
        )
        draw.rectangle((127, 342, 353, 348), fill="#3A1012")
        # Static representative frame: this highlight moves continuously at
        # runtime and does not represent percentage completion.
        draw.rectangle(
            (183, 342, 263, 348),
            fill="#E9B93D",
            outline="#FFF0A0",
            width=1,
        )

    zh_draw = ImageDraw.Draw(zh)
    zh_draw.text(
        (240, 59),
        "等待对方",
        font=cjk_font(27),
        fill=PAPER,
        anchor="mm",
    )
    zh_draw.text(
        (240, 372),
        "等待对方开始",
        font=cjk_font(18),
        fill=PAPER,
        anchor="mm",
    )

    en_draw = ImageDraw.Draw(en)
    en_draw.text(
        (240, 59),
        "WAITING FOR OPPONENT",
        font=font(24),
        fill=PAPER,
        anchor="mm",
    )
    en_draw.text(
        (240, 372),
        "WAITING FOR OPPONENT TO START",
        font=font(15),
        fill=PAPER,
        anchor="mm",
    )
    return render_image(zh), render_image(en)

def render_game_overlays() -> dict[str, Image.Image]:
    menu = open_design("s30_game_menu_480.png")
    label(menu, (145, 168, 335, 197), "GAME MENU", 25)
    label(menu, (101, 244, 218, 270), "<  RESUME", 16)
    label(menu, (270, 244, 383, 270), "RESIGN  >", 16)

    promotion = open_design("s30_promotion_480.png")
    label(promotion, (115, 148, 365, 179), "CHOOSE PROMOTION", 25)
    for box, value in (
        ((62, 313, 130, 338), "QUEEN"),
        ((158, 313, 226, 338), "ROOK"),
        ((253, 313, 323, 338), "BISHOP"),
        ((350, 313, 420, 338), "KNIGHT"),
    ):
        label(promotion, box, value, 12)

    resign = open_design("s30_resign_confirm_480.png")
    label(resign, (145, 162, 335, 191), "RESIGN GAME", 24)
    label(resign, (105, 212, 375, 239), "RESIGNING ENDS THIS GAME", 17)
    label(resign, (105, 241, 375, 266), "ARE YOU SURE?", 15)
    label(resign, (111, 288, 218, 313), "<  CONTINUE", 14)
    label(resign, (267, 288, 369, 313), "RESIGN  >", 14)

    lost = open_design("s30_connection_lost_480.png")
    label(lost, (130, 162, 350, 191), "CONNECTION LOST", 24)
    label(lost, (105, 212, 375, 239), "WAITING FOR OPPONENT...", 17)
    label(lost, (90, 241, 390, 266), "MOVE DEVICES CLOSER TO RECONNECT", 13)
    label(lost, (111, 288, 218, 313), "<  MENU", 15)
    label(lost, (267, 288, 369, 313), "RECONNECT  >", 14)

    sync = open_design("s30_sync_error_480.png")
    label(sync, (145, 162, 335, 191), "SYNC FAILED", 24)
    label(sync, (105, 212, 375, 239), "GAME STATE COULD NOT BE VERIFIED", 14)
    label(sync, (105, 241, 375, 266), "TAP BELOW TO RESYNC", 14)
    label(sync, (111, 288, 218, 313), "<  MENU", 15)
    label(sync, (267, 288, 369, 313), "RESYNC  >", 15)
    return {
        "s30_menu_en.png": menu,
        "s30_promotion_en.png": promotion,
        "s30_resign_en.png": resign,
        "s30_connection_lost_en.png": lost,
        "s30_sync_error_en.png": sync,
    }


def render_results() -> dict[str, Image.Image]:
    specs = {
        "s50_win_480.png": ("s50_en.png", "VICTORY", "CHECKMATE · YOU WIN"),
        "s51_loss_480.png": ("s51_en.png", "DEFEAT", "CHECKMATED · YOU LOSE"),
        "s52_draw_480.png": ("s52_en.png", "DRAW", "EVEN MATCH · DRAW"),
    }
    out: dict[str, Image.Image] = {}
    for source, (name, title, reason) in specs.items():
        image = open_design(source)
        label(image, (120, 43, 360, 76), title, 29)
        label(image, (100, 357, 380, 385), reason, 17)
        label(image, (113, 408, 218, 433), "<  MENU", 15)
        label(image, (265, 408, 367, 433), "PLAY AGAIN  >", 13)
        out[name] = image
    return out


def contact_sheet(images: dict[str, Image.Image]) -> Image.Image:
    thumb = 240
    label_h = 24
    cols = 4
    rows = (len(images) + cols - 1) // cols
    sheet = Image.new("RGB", (cols * thumb, rows * (thumb + label_h)), "#110B08")
    draw = ImageDraw.Draw(sheet)
    for index, (name, image) in enumerate(images.items()):
        x = (index % cols) * thumb
        y = (index // cols) * (thumb + label_h)
        sheet.paste(image.resize((thumb, thumb), Image.Resampling.LANCZOS), (x, y))
        draw.text((x + thumb / 2, y + thumb + label_h / 2), name,
                  font=font(12), fill=GOLD, anchor="mm")
    return sheet


def candidate_images() -> dict[str, Image.Image]:
    zh_home, en_home = render_home()
    return {
        "s00_zh.png": zh_home,
        "s00_en.png": en_home,
        "s10_en.png": render_mode(),
        "s11_en.png": render_difficulty(),
        **render_pairing(),
        **render_game_overlays(),
        **render_results(),
    }


def write_runtime_assets(output: Path) -> None:
    output.mkdir(parents=True, exist_ok=True)
    images = candidate_images()
    for output_name, (source_name, box) in {
        "promotion_panel_en.png": (
            "s30_promotion_en.png", (32, 132, 448, 368)
        ),
        "game_menu_panel_en.png": (
            "s30_menu_en.png", (56, 148, 424, 332)
        ),
        "resign_panel_en.png": (
            "s30_resign_en.png", (52, 142, 428, 348)
        ),
        "connection_lost_panel_en.png": (
            "s30_connection_lost_en.png", (52, 142, 428, 348)
        ),
        "sync_error_panel_en.png": (
            "s30_sync_error_en.png", (52, 142, 428, 348)
        ),
    }.items():
        images[source_name].crop(box).save(
            output / output_name, "PNG", optimize=True
        )


def main() -> None:
    write_runtime_assets(OUT)
    print(f"wrote localized runtime overlays to {OUT}")


if __name__ == "__main__":
    main()
