#!/usr/bin/env python3
"""Generate projects/mosaico_chess/ui/main.json (S00-S30, one scene, layers)."""

from __future__ import annotations

import json
from pathlib import Path

UI = Path(__file__).resolve().parents[1]
OUT = UI / "main.json"

INK = "#110B08"
VERM = "#D41E3C"
GOLD = "#E2B84A"
PAPER = "#F6E6C8"
PAPER_HI = "#FFF4DC"
SQ_A = "#FAF3E6"
SQ_B = "#C46C68"
AMBER = "#F2A41C"
BTN = "#1A0E0C"
STAMP = "#E85A6C"
SOON = "#8D8578"

FRAME = 20
CELL = 55
PW, PH = 34, 68

FONT = "fonts/DejaVuSans.ttf"
PIECE_SET = "designs/pieces_redesigned"

START = [
    "rnbqkbnr",
    "pppppppp",
    "........",
    "........",
    "........",
    "........",
    "PPPPPPPP",
    "RNBQKBNR",
]

IMG = {
    "r": f"{PIECE_SET}/bR.png",
    "n": f"{PIECE_SET}/bN.png",
    "b": f"{PIECE_SET}/bB.png",
    "q": f"{PIECE_SET}/bQ.png",
    "k": f"{PIECE_SET}/bK.png",
    "p": f"{PIECE_SET}/bP.png",
    "R": f"{PIECE_SET}/wR.png",
    "N": f"{PIECE_SET}/wN.png",
    "B": f"{PIECE_SET}/wB.png",
    "Q": f"{PIECE_SET}/wQ.png",
    "K": f"{PIECE_SET}/wK.png",
    "P": f"{PIECE_SET}/wP.png",
}

def L(**kw):
    return kw


def switch(fr: str, to: str) -> list:
    return [
        {"event": "click", "action": "hide", "target_name": fr},
        {"event": "click", "action": "show", "target_name": to},
    ]

def piece_box(ch: str, file: int, rank: int) -> tuple[int, int, int, int]:
    cell_x = FRAME + file * CELL
    cell_y = FRAME + (7 - rank) * CELL
    w, h = PW, PH
    x = cell_x + (CELL - w) // 2
    y = cell_y + CELL - h - 4
    return x, y, w, h


def pos(x: int, y: int, w: int, h: int) -> dict:
    """Bindable x/y so esp_gsp_component_set_position() can move the object."""
    return {
        "x": {"default": int(x), "min": 0, "max": 480 - w},
        "y": {"default": int(y), "min": 0, "max": 480 - h},
    }


def board_pos(x: int = 0, y: int = 0) -> dict:
    """Bindable board-local x/y for feedback overlays."""
    return {
        "x": {"default": int(x), "min": 0, "max": 7 * CELL},
        "y": {"default": int(y), "min": 0, "max": 7 * CELL},
    }

def approved_screen(
    name: str,
    image: str,
    *,
    hidden: bool = True,
) -> list[dict]:
    return [
        L(
            type="layer",
            parent=-1,
            name=name,
            bind=name,
            bind_target="visible",
            x=0,
            y=0,
            w=480,
            h=480,
            bg_color=INK,
            hidden=hidden,
            block_scene_swipe=True,
        ),
        L(
            type="image",
            parent_name=name,
            x=0,
            y=0,
            w=480,
            h=480,
            image=image,
            codec="lossless",
        ),
    ]


def localized_screen(
    name: str,
    zh_image: str,
    en_image: str,
    *,
    hidden: bool = True,
) -> list[dict]:
    objects = approved_screen(name, zh_image, hidden=hidden)
    objects[1]["name"] = f"{name}_bg_zh"
    objects[1]["bind"] = f"{name}_bg_zh"
    objects[1]["bind_target"] = "visible"
    objects.append(
        L(
            type="image",
            parent_name=name,
            name=f"{name}_bg_en",
            bind=f"{name}_bg_en",
            bind_target="visible",
            x=0,
            y=0,
            w=480,
            h=480,
            image=en_image,
            codec="lossless",
            hidden=True,
        )
    )
    return objects


def approved_animation_screen(name: str, image: str) -> list[dict]:
    return [
        L(
            type="layer",
            parent=-1,
            name=name,
            bind=name,
            bind_target="visible",
            x=0,
            y=0,
            w=480,
            h=480,
            bg_color=INK,
            hidden=True,
            block_scene_swipe=True,
        ),
        L(
            type="image",
            parent_name=name,
            x=0,
            y=0,
            w=480,
            h=480,
            image=image,
            max_fps=0,
        ),
    ]


def corner_masks(parent: str) -> list[dict]:
    return [
        L(
            type="image",
            parent_name=parent,
            x=x,
            y=y,
            w=60,
            h=60,
            image=f"runtime/screens/corner_mask_{corner}.png",
            codec="lossless",
        )
        for corner, x, y in (
            ("tl", 0, 0),
            ("tr", 420, 0),
            ("br", 420, 420),
            ("bl", 0, 420),
        )
    ]


def localized_animation_screen(
    name: str, zh_image: str, en_image: str
) -> list[dict]:
    objects = approved_animation_screen(name, zh_image)
    objects[1]["name"] = f"{name}_bg_zh"
    objects[1]["bind"] = f"{name}_bg_zh"
    objects[1]["bind_target"] = "visible"
    objects.append(
        L(
            type="image",
            parent_name=name,
            name=f"{name}_bg_en",
            bind=f"{name}_bg_en",
            bind_target="visible",
            x=0,
            y=0,
            w=480,
            h=480,
            image=en_image,
            max_fps=0,
            hidden=True,
        )
    )
    objects.extend(corner_masks(name))
    return objects


def transparent_hit(
    parent: str,
    callback: str,
    x: int,
    y: int,
    w: int,
    h: int,
    *,
    name: str | None = None,
    events: list | None = None,
) -> dict:
    obj = L(
        type="button",
        parent_name=parent,
        x=x,
        y=y,
        w=w,
        h=h,
        bg_color="#00000000",
    )
    if callback:
        obj["callback"] = callback
    if name is not None:
        obj["name"] = name
    if events is not None:
        obj["events"] = events
    return obj


def build() -> dict:
    objects: list[dict] = []

    # ----- S00 -----
    objects += localized_screen(
        "s00",
        "runtime/screens/s00_round_zh.png",
        "runtime/screens/s00_round_en.png",
        hidden=False,
    )
    objects.append(
        transparent_hit(
            "s00", "enter_chess", 42, 367, 396, 68,
            name="home_enter", events=switch("s00", "s10")
        )
    )
    objects.append(
        transparent_hit(
            "s00", "toggle_language", 199, 24, 82, 32,
            name="language_toggle",
        )
    )

    # ----- S10 -----
    objects += localized_screen(
        "s10",
        "runtime/screens/s10_round_zh.png",
        "runtime/screens/s10_round_en.png",
    )
    objects += [
        transparent_hit("s10", "go_pve", 48, 122, 174, 258, name="hit_pve"),
        transparent_hit(
            "s10", "pvp_developing", 250, 122, 181, 258, name="hit_pvp"
        ),
        transparent_hit(
            "s10", "back_s00", 178, 404, 124, 36,
            events=switch("s10", "s00")
        ),
    ]

    # ----- S11 difficulty -----
    objects += localized_screen(
        "s11",
        "runtime/screens/s11_round_zh.png",
        "runtime/screens/s11_round_en.png",
    )
    objects += [
        transparent_hit("s11", "difficulty_easy", 26, 130, 134, 250),
        transparent_hit("s11", "difficulty_normal", 173, 130, 134, 250),
        transparent_hit("s11", "difficulty_hard", 320, 130, 132, 250),
        transparent_hit("s11", "back_s10", 178, 404, 124, 36),
    ]

    # ----- S20-S22 pairing -----
    objects += localized_screen(
        "s20",
        "runtime/screens/s20_round_zh.png",
        "runtime/screens/s20_round_en.png",
    )
    objects += [
        L(
            type="container",
            parent_name="s20",
            x=123,
            y=339,
            w=234,
            h=12,
            bg_color="#160B0AFF",
            border_color="#E2B84AFF",
            border_width=2,
        ),
        L(
            type="container",
            parent_name="s20",
            x=127,
            y=342,
            w=226,
            h=6,
            bg_color="#3A1012FF",
        ),
        L(
            type="container",
            parent_name="s20",
            name="waitbar",
            bind="waitbar",
            bind_target="visible",
            x={"default": 127, "min": 127, "max": 273},
            y=342,
            w=80,
            h=6,
            bg_color="#E9B93DFF",
            border_color="#FFF0A0FF",
            border_width=1,
        ),
        transparent_hit("s20", "wait_cancel", 178, 404, 124, 36),
        transparent_hit(
            "s20", "sim_pair_ok", 102, 150, 276, 164, name="sim_ok"
        ),
    ]
    objects += localized_animation_screen(
        "s20_success_anim",
        "runtime/screens/s20_to_s21_round_zh.gif",
        "runtime/screens/s20_to_s21_round_en.gif",
    )
    objects += [
        transparent_hit(
            "s20_success_anim",
            "s21_continue",
            178,
            404,
            124,
            36,
            name="s20_success_continue",
        ),
    ]
    objects += localized_animation_screen(
        "s20_fail_anim",
        "runtime/screens/s20_to_s22_round_zh.gif",
        "runtime/screens/s20_to_s22_round_en.gif",
    )

    objects += localized_screen(
        "s21",
        "runtime/screens/s21_round_zh.png",
        "runtime/screens/s21_round_en.png",
    )
    objects += [
        L(
            type="image",
            parent_name="s21",
            name="s21_bg_black_zh",
            bind="s21_bg_black_zh",
            bind_target="visible",
            x=0,
            y=0,
            w=480,
            h=480,
            image="runtime/screens/s21_round_black_zh.png",
            codec="lossless",
            hidden=True,
        ),
        L(
            type="image",
            parent_name="s21",
            name="s21_bg_black_en",
            bind="s21_bg_black_en",
            bind_target="visible",
            x=0,
            y=0,
            w=480,
            h=480,
            image="runtime/screens/s21_round_black_en.png",
            codec="lossless",
            hidden=True,
        ),
        transparent_hit(
            "s21", "s21_continue", 178, 404, 124, 36, name="s21_note"
        ),
    ]

    objects += localized_screen(
        "s21_wait",
        "designs/s21_wait_opponent_start_480.png",
        "runtime/overlays/localized/s21_wait_opponent_start_en.png",
    )
    objects += [
        transparent_hit(
            "s21_wait",
            "sim_peer_ready",
            70,
            207,
            340,
            66,
            name="sim_peer_ready",
        ),
    ]
    objects[-1]["hidden"] = True

    objects += localized_screen(
        "s22",
        "runtime/screens/s22_round_zh.png",
        "runtime/screens/s22_round_en.png",
    )
    objects += [
        transparent_hit("s22", "go_pvp", 178, 404, 124, 36),
    ]

    def result_page(name: str) -> None:
        objects.extend(
            localized_screen(
                name,
                f"runtime/screens/{name}_round_zh.png",
                f"runtime/screens/{name}_round_en.png",
            )
        )
        objects.extend(
            [
                transparent_hit(name, "result_menu", 103, 404, 124, 36),
                transparent_hit(name, "result_again", 254, 404, 124, 36),
            ]
        )

    result_page("s50")
    result_page("s51")
    result_page("s52")

    # ----- S30 -----
    objects.append(
        L(
            type="layer",
            parent=-1,
            name="play",
            bind="play",
            bind_target="visible",
            x=0,
            y=0,
            w=480,
            h=480,
            bg_color=INK,
            hidden=True,
            block_scene_swipe=True,
        )
    )
    objects.append(
        L(
            type="image",
            parent_name="play",
            x=0,
            y=0,
            w=480,
            h=480,
            image="runtime/overlays/board_frame.png",
            codec="lossless",
        )
    )
    objects.append(
        L(
            type="container",
            parent_name="play",
            name="board",
            x=FRAME,
            y=FRAME,
            w=440,
            h=440,
            bg_color="#2A1014",
            border_color="#5E301E",
            border_width=1,
        )
    )
    # The approved board image contains the initial top-rank crowns in the
    # frame strip above y=20. Rebuild that strip before placing runtime
    # sprites, otherwise moving a top-rank king/queen leaves baked-in ghosts.
    objects += [
        L(
            type="container",
            parent_name="play",
            x=FRAME,
            y=0,
            w=440,
            h=FRAME,
            bg_color=INK,
        ),
        L(
            type="container",
            parent_name="play",
            x=FRAME,
            y=3,
            w=440,
            h=1,
            bg_color="#704322FF",
        ),
        L(
            type="container",
            parent_name="play",
            x=FRAME,
            y=16,
            w=440,
            h=2,
            bg_color="#C08A35FF",
        ),
    ]
    for rank_from_top in range(8):
        rank = 7 - rank_from_top
        for file in range(8):
            light = (file + rank) % 2 == 1
            x = file * CELL
            y = rank_from_top * CELL
            objects.append(
                L(
                    type="button",
                    parent_name="board",
                    name=f"hit{rank * 8 + file:02d}",
                    callback=f"hit{rank * 8 + file:02d}",
                    x=x,
                    y=y,
                    w=CELL,
                    h=CELL,
                    text="",
                    bg_color=SQ_A if light else SQ_B,
                    fg_color="#00000000",
                    border_color="#6A3A28",
                    border_width=1,
                )
            )
    for name in ("last_from", "last_to"):
        objects += [
            L(
                type="container",
                parent_name="board",
                name=name,
                bind=name,
                bind_target="visible",
                **board_pos(),
                w=CELL,
                h=CELL,
                bg_color="#BC560CD9",
                border_color="#3A1C0E",
                border_width=2,
                hidden=True,
            ),
            L(
                type="container",
                parent_name=name,
                x=2,
                y=2,
                w=CELL - 4,
                h=CELL - 4,
                bg_color="#F2A41CA6",
                border_color="#FFDE4C",
                border_width=2,
            ),
            L(
                type="container",
                parent_name=name,
                x=5,
                y=5,
                w=CELL - 10,
                h=CELL - 10,
                bg_color="#F2A41C40",
                border_color="#A85212",
                border_width=1,
            ),
        ]
    objects.append(
        L(
            type="container",
            parent_name="board",
            name="check",
            bind="check",
            bind_target="visible",
            **board_pos(4 * CELL, 0),
            w=CELL,
            h=CELL,
            bg_color="#F52420A6",
            border_color="#FF7860",
            border_width=5,
            hidden=True,
        )
    )
    # Selection glow stays below piece sprites so the selected piece remains
    # crisp while the full square keeps its high-contrast molten-gold frame.
    sel_xy = pos(FRAME, FRAME + 7 * CELL, CELL, CELL)
    objects += [
        L(
            type="container",
            parent_name="play",
            name="sel",
            bind="sel",
            bind_target="visible",
            x=sel_xy["x"],
            y=sel_xy["y"],
            w=CELL,
            h=CELL,
            bg_color="#DA6C10A6",
            border_color="#26120C",
            border_width=2,
            hidden=True,
        ),
        L(
            type="container",
            parent_name="sel",
            x=2,
            y=2,
            w=CELL - 4,
            h=CELL - 4,
            bg_color="#FFD44066",
            border_color="#FFF494",
            border_width=3,
        ),
        L(
            type="container",
            parent_name="sel",
            x=6,
            y=6,
            w=CELL - 12,
            h=CELL - 12,
            bg_color="#FFD44033",
            border_color="#983818",
            border_width=1,
        ),
    ]
    slot = 0
    for rank_from_top, row in enumerate(START):
        rank = 7 - rank_from_top
        for file, ch in enumerate(row):
            if ch == ".":
                continue
            x, y, w, h = piece_box(ch, file, rank)
            xy = pos(x, y, w, h)
            objects.append(
                L(
                    type="image",
                    parent_name="play",
                    name=f"p{slot:02d}",
                    bind=f"p{slot:02d}",
                    bind_target="visible",
                    x=xy["x"],
                    y=xy["y"],
                    w=w,
                    h=h,
                    image=IMG[ch],
                )
            )
            slot += 1
    extra_xy = pos(0, 0, PW, PH)
    objects += [
        L(
            type="image",
            parent_name="play",
            name="extra_wq",
            bind="extra_wq",
            bind_target="visible",
            x=extra_xy["x"],
            y=extra_xy["y"],
            w=PW,
            h=PH,
            image=f"{PIECE_SET}/wQ.png",
            hidden=True,
        ),
        L(
            type="image",
            parent_name="play",
            name="extra_bq",
            bind="extra_bq",
            bind_target="visible",
            x=extra_xy["x"],
            y=extra_xy["y"],
            w=PW,
            h=PH,
            image=f"{PIECE_SET}/bQ.png",
            hidden=True,
        ),
    ]
    # Promotion sprite pool. Base pieces above cover the initial material;
    # eight additional sprites per promotable type and color cover the legal
    # maximum without changing image sources at runtime.
    for color in ("w", "b"):
        for piece in ("Q", "R", "B", "N"):
            for index in range(8):
                key = f"prom_{color}{piece.lower()}{index}"
                pool_xy = pos(0, 0, PW, PH)
                objects.append(
                    L(
                        type="image",
                        parent_name="play",
                        name=key,
                        bind=key,
                        bind_target="visible",
                        x=pool_xy["x"],
                        y=pool_xy["y"],
                        w=PW,
                        h=PH,
                        image=f"{PIECE_SET}/{color}{piece}.png",
                        hidden=True,
                    )
                )
    # The visible board squares are the click targets. Keeping hit testing on
    # the opaque square itself avoids invisible controls being culled by GSP.
    for i in range(64):
        file = i % 8
        rank = i // 8
        cx = FRAME + file * CELL + (CELL - 30) // 2
        cy = FRAME + (7 - rank) * CELL + (CELL - 30) // 2
        objects += [
            L(
                type="container",
                parent_name="play",
                name=f"dot{i:02d}",
                bind=f"dot{i:02d}",
                bind_target="visible",
                x=cx,
                y=cy,
                w=30,
                h=30,
                radius=15,
                bg_color="#381C10",
                hidden=True,
            ),
            L(
                type="container",
                parent_name=f"dot{i:02d}",
                x=3,
                y=3,
                w=24,
                h=24,
                radius=12,
                bg_color="#FFC440",
                border_color="#E08A24",
                border_width=2,
            ),
        ]
        objects.append(
            L(
                type="container",
                parent_name="play",
                name=f"cap{i:02d}",
                bind=f"cap{i:02d}",
                bind_target="visible",
                x=FRAME + file * CELL + 4,
                y=FRAME + (7 - rank) * CELL + 4,
                w=CELL - 8,
                h=CELL - 8,
                bg_color="#00000000",
                border_color=GOLD,
                border_width=4,
                hidden=True,
            )
        )
    # Mandatory promotion chooser. There is deliberately no close action:
    # the pending pawn move is not committed until one of these is chosen.
    objects += [
        L(
            type="container",
            parent_name="play",
            name="promotion",
            bind="promotion",
            bind_target="visible",
            x=0,
            y=0,
            w=480,
            h=480,
            bg_color="#09050399",
            hidden=True,
        ),
        L(
            type="image",
            parent_name="promotion",
            name="promotion_panel_zh",
            bind="promotion_panel_zh",
            bind_target="visible",
            x=32,
            y=132,
            w=416,
            h=236,
            image="runtime/overlays/promotion_panel.png",
            codec="lossless",
        ),
        L(
            type="image",
            parent_name="promotion",
            name="promotion_panel_en",
            bind="promotion_panel_en",
            bind_target="visible",
            x=32,
            y=132,
            w=416,
            h=236,
            image="runtime/overlays/localized/promotion_panel_en.png",
            codec="lossless",
            hidden=True,
        ),
    ]
    for index, piece in enumerate(("q", "r", "b", "n")):
        objects.append(
            transparent_hit(
                "promotion", f"promote_{piece}",
                55 + index * 96, 201, 82, 144,
                name=f"promote_{piece}",
            )
        )

    # Confirmation and connection-state layers.
    for name, panel in (
        ("resign_confirm", "resign_panel.png"),
        ("connection_lost", "connection_lost_panel.png"),
        ("sync_error", "sync_error_panel.png"),
    ):
        objects += [
            L(
                type="container",
                parent_name="play",
                name=name,
                bind=name,
                bind_target="visible",
                x=0,
                y=0,
                w=480,
                h=480,
                bg_color="#09050399",
                hidden=True,
            ),
            L(
                type="image",
                parent_name=name,
                name=f"{name}_panel_zh",
                bind=f"{name}_panel_zh",
                bind_target="visible",
                x=52,
                y=142,
                w=376,
                h=206,
                image=f"runtime/overlays/{panel}",
                codec="lossless",
            ),
            L(
                type="image",
                parent_name=name,
                name=f"{name}_panel_en",
                bind=f"{name}_panel_en",
                bind_target="visible",
                x=52,
                y=142,
                w=376,
                h=206,
                image=(
                    "runtime/overlays/localized/"
                    + {
                        "resign_confirm": "resign_panel_en.png",
                        "connection_lost": "connection_lost_panel_en.png",
                        "sync_error": "sync_error_panel_en.png",
                    }[name]
                ),
                codec="lossless",
                hidden=True,
            ),
        ]

    objects += [
        transparent_hit(
            "resign_confirm", "cancel_resign", 99, 282, 130, 36
        ),
        transparent_hit(
            "resign_confirm", "confirm_resign", 251, 282, 130, 36
        ),
        transparent_hit(
            "connection_lost", "connection_menu", 99, 282, 130, 36
        ),
        transparent_hit(
            "connection_lost", "dev_reconnect", 251, 282, 130, 36
        ),
        transparent_hit(
            "sync_error", "connection_menu", 99, 282, 130, 36
        ),
        transparent_hit(
            "sync_error", "dev_retry_sync", 251, 282, 130, 36
        ),
        L(
            type="container",
            parent_name="play",
            name="menu",
            x=0,
            y=0,
            w=480,
            h=480,
            bg_color="#09050399",
            hidden=True,
        ),
        L(
            type="image",
            parent_name="menu",
            name="menu_panel_zh",
            bind="menu_panel_zh",
            bind_target="visible",
            x=56,
            y=148,
            w=368,
            h=184,
            image="runtime/overlays/game_menu_panel.png",
            codec="lossless",
        ),
        L(
            type="image",
            parent_name="menu",
            name="menu_panel_en",
            bind="menu_panel_en",
            bind_target="visible",
            x=56,
            y=148,
            w=368,
            h=184,
            image="runtime/overlays/localized/game_menu_panel_en.png",
            codec="lossless",
            hidden=True,
        ),
        transparent_hit(
            "menu", "", 94, 237, 136, 42,
            events=[{"event": "click", "action": "hide", "target_name": "menu"}],
        ),
        transparent_hit("menu", "resign", 250, 237, 136, 42, name="resign"),
        L(
            type="button",
            parent_name="play",
            name="open_menu",
            callback="open_menu",
            x=0,
            y=0,
            w=48,
            h=20,
            text="",
            bg_color="#00000000",
            fg_color=GOLD,
        ),
    ]

    return {
        "screen": "main",
        "w": 480,
        "h": 480,
        "screen_bg": INK,
        "font": FONT,
        "default_font_size": 18,
        "swipe": False,
        "objects": objects,
    }


def main() -> None:
    scene = build()
    OUT.parent.mkdir(parents=True, exist_ok=True)
    OUT.write_text(json.dumps(scene, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(f"wrote {OUT} objects={len(scene['objects'])}")


if __name__ == "__main__":
    main()
