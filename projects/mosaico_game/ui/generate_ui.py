#!/usr/bin/env python3
"""Generate projects/mosaico_game/ui/main.json (S00-S30, one scene, layers)."""

from __future__ import annotations

import json
from pathlib import Path

_HERE = Path(__file__).resolve().parent
OUT = _HERE / "main.json" if _HERE.name == "ui" else _HERE / "ui" / "main.json"

INK = "#110B08"
VERM = "#D41E3C"
GOLD = "#E2B84A"
PAPER = "#F6E6C8"
PAPER_HI = "#FFF4DC"
SQ_A = "#E9D5AE"
SQ_B = "#713743"
AMBER = "#E8A04A"
BTN = "#1A0E0C"
STAMP = "#E85A6C"
SOON = "#8D8578"

FRAME = 20
CELL = 55
PW, PH = 39, 64
PPW, PPH = 51, 84

FONT = "fonts/cjk.ttf"
PIECE_SET = "pieces/set-illustrate"

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

RESULTS = {
    "s50": {
        "art": "pieces/art_win.png",
        "art_box": (114, 29, 252, 252),
        "ribbon": "pieces/result_ribbon_win.png",
        "title": "胜 利",
        "color": PAPER_HI,
    },
    "s51": {
        "art": "pieces/art_loss.png",
        "art_box": (114, 29, 252, 252),
        "ribbon": "pieces/result_ribbon_loss.png",
        "title": "败 北",
        "color": STAMP,
    },
    "s52": {
        "art": "pieces/art_draw.png",
        "art_box": (80, 0, 320, 320),
        "ribbon": "pieces/result_ribbon_draw.png",
        "title": "和 棋",
        "color": GOLD,
    },
}


def L(**kw):
    return kw


def go(target: str) -> list:
    return [{"event": "click", "action": "show", "target_name": target}]


def hide(target: str) -> list:
    return [{"event": "click", "action": "hide", "target_name": target}]


def switch(fr: str, to: str) -> list:
    return [
        {"event": "click", "action": "hide", "target_name": fr},
        {"event": "click", "action": "show", "target_name": to},
    ]


def corners(parent: str) -> list:
    # HTML .d only draws top-left and bottom-right L marks.
    specs = [
        (16, 16, 26, 3),
        (16, 16, 3, 26),
        (438, 461, 26, 3),
        (461, 438, 3, 26),
    ]
    bars = []
    for x, y, w, h in specs:
        bars.append(
            L(
                type="shape",
                parent_name=parent,
                x=x,
                y=y,
                w=w,
                h=h,
                shape="rect",
                bg_color=VERM,
            )
        )
    return bars


def heat_bg(name: str, hidden: bool = False) -> list:
    # Runtime vertical gradients posterize on RGB565 (about nine red
    # steps over 480px). A Floyd–Steinberg PNG keeps the same wash
    # without visible banding. One shared file; gspc interns by path.
    obj = L(
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
    )
    if hidden:
        obj["hidden"] = True
    wash = L(
        type="image",
        parent_name=name,
        x=0,
        y=0,
        w=480,
        h=480,
        image="pieces/heat_bg.png",
        codec="lossless",
    )
    return [obj, wash]


def gold_shadow(parent: str, x: int, y: int, w: int, h: int, d: int = 4) -> dict:
    return L(
        type="container",
        parent_name=parent,
        x=x + d,
        y=y + d,
        w=w,
        h=h,
        bg_color=GOLD,
    )


def verm_shadow(parent: str, x: int, y: int, w: int, h: int, d: int = 5) -> dict:
    return L(
        type="container",
        parent_name=parent,
        x=x + d,
        y=y + d,
        w=w,
        h=h,
        bg_color=VERM,
    )


def mark_title(parent: str, text: str, x: int, y: int, w: int, h: int, size: int) -> list:
    return [
        L(
            type="label",
            parent_name=parent,
            x=x + 4,
            y=y + 5,
            w=w,
            h=h,
            text=text,
            font_size=size,
            text_align="center",
            fg_color=VERM,
        ),
        L(
            type="label",
            parent_name=parent,
            x=x,
            y=y,
            w=w,
            h=h,
            text=text,
            font_size=size,
            text_align="center",
            fg_color=PAPER_HI,
        ),
    ]


def wait_lock(parent: str, title: str, bar_mode: str, note: str, note_cb: list, note_call: str | None = None) -> list:
    objs = mark_title(parent, title, 20, 168, 440, 48, 36)
    track_x, track_y, track_w, track_h = 120, 230, 240, 8
    thumb_w = 80
    if bar_mode == "run":
        objs.append(
            L(
                type="container",
                parent_name=parent,
                x=track_x,
                y=track_y,
                w=track_w,
                h=track_h,
                bg_color="#4A1418",
            )
        )
        if parent == "s20":
            objs.append(
                L(
                    type="container",
                    parent_name=parent,
                    name="waitbar",
                    bind="waitbar",
                    bind_target="visible",
                    x={"default": track_x, "min": track_x, "max": track_x + track_w - thumb_w},
                    y={"default": track_y, "min": 0, "max": 480 - track_h},
                    w=thumb_w,
                    h=track_h,
                    bg_color=VERM,
                )
            )
    elif bar_mode == "done":
        objs.append(
            L(
                type="container",
                parent_name=parent,
                x=track_x,
                y=track_y,
                w=track_w,
                h=track_h,
                bg_color=VERM,
            )
        )
    else:
        objs.append(
            L(
                type="container",
                parent_name=parent,
                x=120,
                y=230,
                w=240,
                h=8,
                bg_color="#00000000",
            )
        )
    btn = L(
        type="button",
        parent_name=parent,
        x=140,
        y=254,
        w=200,
        h=34,
        text=note,
        font_size=18,
        bg_color="#00000000",
        fg_color=GOLD,
    )
    if note_cb:
        btn["events"] = note_cb
    if note_call:
        btn["callback"] = note_call
    objs.append(btn)
    return objs


def picker_page(layer: str, x: int, kind: str) -> list:
    objs = [
        L(
            type="layer",
            parent_name="picker",
            name=layer,
            x=x,
            y=0,
            w=480,
            h=480,
            bg_color="#00000000",
        ),
            L(
                type="label",
                parent_name=layer,
                x=36,
                y=36,
                w=200,
                h=28,
                text="选游戏",
                font_size=16,
                fg_color=VERM,
            ),
        ]
    if kind == "chess":
        objs += [
            verm_shadow(layer, 156, 92, 168, 168, 5),
            L(
                type="container",
                parent_name=layer,
                x=156,
                y=92,
                w=168,
                h=168,
                bg_color=BTN,
                border_color=GOLD,
                border_width=3,
            ),
            L(
                type="image",
                parent_name=layer,
                # 80x160 canvas centred in the 168x168 frame at (156,92);
                # the king inside keeps the source 154:307 aspect (68x136).
                x=200,
                y=96,
                w=80,
                h=160,
                image="pieces/anim_wk.gif",
                # 0 = uncapped. Values below ~24 make gspc decimate the GIF
                # (12 -> 8 frames); the file is authored at 12fps so this does
                # not speed playback up, it only keeps every frame.
                max_fps=0,
            ),
            L(
                type="label",
                parent_name=layer,
                x=40,
                y=278,
                w=400,
                h=48,
                text="国际象棋",
                font_size=40,
                text_align="center",
                fg_color=PAPER_HI,
            ),
            L(
                type="label",
                parent_name=layer,
                x=40,
                y=328,
                w=400,
                h=28,
                text="贴合对决 也可战电脑",
                font_size=16,
                text_align="center",
                fg_color=GOLD,
            ),
            gold_shadow(layer, 36, 372, 408, 72, 4),
            L(
                type="button",
                parent_name=layer,
                name="enter_chess",
                callback="enter_chess",
                x=36,
                y=372,
                w=408,
                h=72,
                text="进入",
                font_size=22,
                bg_color=VERM,
                fg_color=PAPER_HI,
                events=switch("s00", "s10"),
            ),
        ]
    else:
        objs += [
            L(
                type="container",
                parent_name=layer,
                x=156,
                y=92,
                w=168,
                h=168,
                bg_color=BTN,
                border_color=GOLD,
                border_width=3,
            ),
            L(
                type="label",
                parent_name=layer,
                x=156,
                y=148,
                w=168,
                h=56,
                text="未",
                font_size=40,
                text_align="center",
                fg_color=GOLD,
            ),
            L(
                type="label",
                parent_name=layer,
                x=40,
                y=278,
                w=400,
                h=48,
                text="即将推出",
                font_size=40,
                text_align="center",
                fg_color=PAPER_HI,
            ),
            L(
                type="label",
                parent_name=layer,
                x=40,
                y=328,
                w=400,
                h=28,
                text="这一页还没有游戏",
                font_size=16,
                text_align="center",
                fg_color=GOLD,
            ),
            L(
                type="button",
                parent_name=layer,
                x=36,
                y=372,
                w=408,
                h=72,
                text="不能进",
                font_size=22,
                bg_color="#2A1814",
                fg_color=SOON,
                border_color="#3A2410",
                border_width=2,
                enabled=False,
                events=[{"event": "click", "action": "hide", "target_name": "settings"}],
            ),
        ]
    return objs


def piece_box(ch: str, file: int, rank: int) -> tuple[int, int, int, int]:
    cell_x = FRAME + file * CELL
    cell_y = FRAME + (7 - rank) * CELL
    w, h = (PPW, PPH) if ch in "pP" else (PW, PH)
    x = cell_x + (CELL - w) // 2
    y = cell_y + CELL - h
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


def build() -> dict:
    objects: list[dict] = []

    # ----- S00 -----
    objects.extend(heat_bg("s00"))
    objects += corners("s00")
    # Single-game production home: no fake pagination or empty settings page.
    objects += [
        L(
            type="container",
            parent_name="s00",
            x=156,
            y=92,
            w=168,
            h=168,
            bg_color=BTN,
            border_color=GOLD,
            border_width=3,
        ),
        L(
            type="image",
            parent_name="s00",
            x=200,
            y=96,
            w=80,
            h=160,
            image="pieces/anim_wk.gif",
            max_fps=0,
        ),
        L(
            type="label",
            parent_name="s00",
            x=40,
            y=272,
            w=400,
            h=56,
            text="国际象棋",
            font_size=40,
            text_align="center",
            fg_color=PAPER_HI,
        ),
        gold_shadow("s00", 36, 372, 408, 64, 4),
        L(
            type="button",
            parent_name="s00",
            name="home_enter",
            callback="enter_chess",
            x=36,
            y=372,
            w=408,
            h=64,
            text="进入游戏",
            font_size=22,
            bg_color=VERM,
            fg_color=PAPER_HI,
            events=switch("s00", "s10"),
        ),
    ]

    # ----- S10 -----
    objects.extend(heat_bg("s10", hidden=True))
    objects += corners("s10")
    objects += [
        L(
            type="image",
            parent_name="s10",
            x=160,
            y=64,
            w=160,
            h=160,
            image="pieces/anim_war.gif",
            max_fps=0,
        ),
        L(
            type="label",
            parent_name="s10",
            x=36,
            y=36,
            w=200,
            h=24,
            text="对决",
            font_size=16,
            fg_color=VERM,
        ),
        L(
            type="label",
            parent_name="s10",
            x=36,
            y=68,
            w=300,
            h=24,
            text="国际象棋",
            font_size=16,
            fg_color=GOLD,
        ),
        gold_shadow("s10", 36, 114, 408, 100, 5),
        L(
            type="container",
            parent_name="s10",
            name="btn_pvp",
            callback="pvp_developing",
            x=36,
            y=114,
            w=408,
            h=100,
            bg_color=BTN,
            border_color=VERM,
            border_width=3,
        ),
        L(
            type="label",
            parent_name="s10",
            callback="pvp_developing",
            x=56,
            y=128,
            w=360,
            h=44,
            text="双人对决",
            font_size=36,
            fg_color=PAPER_HI,
        ),
        L(
            type="label",
            parent_name="s10",
            callback="pvp_developing",
            x=56,
            y=176,
            w=360,
            h=24,
            text="真机贴合与同步开发中",
            font_size=16,
            fg_color=GOLD,
        ),
        L(
            type="button",
            parent_name="s10",
            name="hit_pvp",
            callback="pvp_developing",
            x=36,
            y=114,
            w=408,
            h=100,
            text="",
            bg_color="#00000000",
            fg_color=GOLD,
        ),
        gold_shadow("s10", 36, 230, 408, 100, 5),
        L(
            type="container",
            parent_name="s10",
            name="btn_pve",
            callback="go_pve",
            x=36,
            y=230,
            w=408,
            h=100,
            bg_color=BTN,
            border_color=VERM,
            border_width=3,
        ),
        L(
            type="label",
            parent_name="s10",
            callback="go_pve",
            x=56,
            y=244,
            w=360,
            h=44,
            text="对战电脑",
            font_size=36,
            fg_color=PAPER_HI,
        ),
        L(
            type="label",
            parent_name="s10",
            callback="go_pve",
            x=56,
            y=292,
            w=360,
            h=24,
            text="你执白 单机",
            font_size=16,
            fg_color=GOLD,
        ),
        L(
            type="button",
            parent_name="s10",
            name="hit_pve",
            callback="go_pve",
            x=36,
            y=230,
            w=408,
            h=100,
            text="",
            bg_color="#00000000",
            fg_color=GOLD,
        ),
        L(
            type="button",
            parent_name="s10",
            x=36,
            y=420,
            w=120,
            h=36,
            text="回选游戏",
            font_size=16,
            bg_color="#00000000",
            fg_color=GOLD,
            callback="back_s00",
            events=switch("s10", "s00"),
        ),
    ]

    # ----- S20-S23 -----
    objects.extend(heat_bg("s20", hidden=True))
    objects += corners("s20")
    objects += wait_lock("s20", "等待连接中", "run", "取消", None, "wait_cancel")
    objects += [
        L(
            type="button",
            parent_name="s20",
            name="sim_ok",
            callback="sim_pair_ok",
            x=36,
            y=418,
            w=408,
            h=34,
            text="模拟贴合",
            font_size=18,
            bg_color="#00000000",
            fg_color=GOLD,
            hidden=True,
        )
    ]

    objects.extend(heat_bg("s21", hidden=True))
    objects += corners("s21")
    objects += wait_lock("s21", "对决成立", "done", "你执白", switch("s21", "s24"))
    objects[-1]["callback"] = "s21_continue"
    objects[-1]["name"] = "s21_note"

    objects.extend(heat_bg("s22", hidden=True))
    objects += corners("s22")
    objects += wait_lock("s22", "未能对上", "idle", "再贴一次", None, "go_pvp")
    objects += [
        L(
            type="button",
            parent_name="s22",
            callback="wait_cancel",
            x=36,
            y=418,
            w=408,
            h=34,
            text="回菜单",
            font_size=18,
            bg_color="#00000000",
            fg_color=GOLD,
        )
    ]

    objects.extend(heat_bg("s23", hidden=True))
    objects += corners("s23")
    objects += wait_lock(
        "s23", "双设备对决开发中", "idle", "返回", None, "wait_cancel"
    )

    # ----- S24 对局开始 -----
    objects.extend(heat_bg("s24", hidden=True))
    objects += corners("s24")
    objects += [
        L(
            type="container",
            parent_name="s24",
            x=380,
            y=28,
            w=72,
            h=72,
            bg_color="#00000000",
            border_color=VERM,
            border_width=4,
        ),
        L(
            type="label",
            parent_name="s24",
            x=380,
            y=36,
            w=72,
            h=56,
            text="白",
            font_size=36,
            text_align="center",
            fg_color=VERM,
        ),
        L(
            type="label",
            parent_name="s24",
            x=40,
            y=128,
            w=400,
            h=24,
            text="国际象棋",
            font_size=16,
            text_align="center",
            fg_color=VERM,
        ),
    ]
    objects += mark_title("s24", "对局开始", 20, 168, 440, 80, 40)
    objects += [
        verm_shadow("s24", 100, 300, 280, 72, 12),
        gold_shadow("s24", 100, 300, 280, 72, 7),
        L(
            type="button",
            parent_name="s24",
            name="s24_go",
            callback="s24_go",
            x=100,
            y=300,
            w=280,
            h=72,
            text="你执白",
            font_size=36,
            bg_color=PAPER_HI,
            fg_color=INK,
        ),
    ]

    def result_page(name: str) -> None:
        result = RESULTS[name]
        layer_objs = heat_bg(name, hidden=True)
        layer_objs[0]["bind"] = name
        layer_objs[0]["bind_target"] = "visible"
        objects.extend(layer_objs)
        art_x, art_y, art_w, art_h = result["art_box"]
        objects.extend(
            [
                # Result pages use paired top corner marks from the approved
                # heraldic-banner design instead of the generic diagonal pair.
                L(type="shape", parent_name=name, x=16, y=16, w=26, h=3, shape="rect", bg_color=VERM),
                L(type="shape", parent_name=name, x=16, y=16, w=3, h=26, shape="rect", bg_color=VERM),
                L(type="shape", parent_name=name, x=438, y=16, w=26, h=3, shape="rect", bg_color=VERM),
                L(type="shape", parent_name=name, x=461, y=16, w=3, h=26, shape="rect", bg_color=VERM),
                L(
                    type="image",
                    parent_name=name,
                    x=art_x,
                    y=art_y,
                    w=art_w,
                    h=art_h,
                    image=result["art"],
                    codec="lossless",
                ),
                L(
                    type="image",
                    parent_name=name,
                    x=112,
                    y=285,
                    w=256,
                    h=65,
                    image=result["ribbon"],
                    codec="lossless",
                ),
                L(
                    type="label",
                    parent_name=name,
                    x=137,
                    y=297,
                    w=206,
                    h=48,
                    text=result["title"],
                    font_size=42,
                    text_align="center",
                    fg_color=result["color"],
                ),
                L(
                    type="label",
                    parent_name=name,
                    name=f"{name}_reason",
                    bind=f"{name}_reason",
                    bind_target="text",
                    x=40,
                    y=360,
                    w=400,
                    h=28,
                    text="",
                    font_size=16,
                    font_charset="将死认输逼和双方同意棋三次重复局面五十回合规则子力不足",
                    text_align="center",
                    fg_color=GOLD,
                ),
                L(
                    type="button",
                    parent_name=name,
                    callback="result_menu",
                    x=16,
                    y=408,
                    w=215,
                    h=48,
                    text="回菜单",
                    font_size=18,
                    bg_color=BTN,
                    fg_color=GOLD,
                    border_color=GOLD,
                    border_width=2,
                ),
                gold_shadow(name, 241, 408, 215, 48, 5),
                L(
                    type="button",
                    parent_name=name,
                    callback="result_again",
                    x=241,
                    y=408,
                    w=215,
                    h=48,
                    text="再来一局",
                    font_size=18,
                    bg_color=VERM,
                    fg_color=PAPER_HI,
                ),
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
            type="container",
            parent_name="play",
            x=0,
            y=0,
            w=480,
            h=480,
            bg_color="#3A1818",
            bg_gradient=INK,
            gradient_dir="vertical",
            border_color=GOLD,
            border_width=3,
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
            border_color=VERM,
            border_width=2,
        )
    )
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
                )
            )
    for name in ("last_from", "last_to"):
        objects.append(
            L(
                type="container",
                parent_name="board",
                name=name,
                bind=name,
                bind_target="visible",
                **board_pos(),
                w=CELL,
                h=CELL,
                bg_color="#E8A04A66",
                border_color=AMBER,
                border_width=3,
                hidden=True,
            )
        )
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
            bg_color="#FF3B5566",
            border_color="#FF3B55",
            border_width=5,
            hidden=True,
        )
    )
    # Checkmate mark. Sits on the mated king's square, under the piece sprites,
    # so the king stays visible and the mating piece is not indicated.
    objects.append(
        L(
            type="container",
            parent_name="board",
            name="mate",
            bind="mate",
            bind_target="visible",
            **board_pos(4 * CELL, 0),
            w=CELL,
            h=CELL,
            bg_color="#FF3B55",
            hidden=True,
        )
    )
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
    sel_xy = pos(FRAME, FRAME + 7 * CELL, CELL, CELL)
    objects.append(
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
            bg_color="#00000000",
            border_color=VERM,
            border_width=6,
            hidden=True,
        )
    )
    for i in range(64):
        file = i % 8
        rank = i // 8
        cx = FRAME + file * CELL + (CELL - 17) // 2
        cy = FRAME + (7 - rank) * CELL + (CELL - 17) // 2
        objects.append(
            L(
                type="container",
                parent_name="play",
                name=f"dot{i:02d}",
                bind=f"dot{i:02d}",
                bind_target="visible",
                x=cx,
                y=cy,
                w=17,
                h=17,
                radius=8,
                bg_color=GOLD,
                hidden=True,
            )
        )
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
    for notice_name, notice_text in (("notice_wait", "等待对手"),):
        objects.append(
            L(
                type="label",
                parent_name="play",
                name=notice_name,
                x=110,
                y=214,
                w=260,
                h=52,
                text=notice_text,
                font_size=18,
                text_align="center",
                bg_color="#1A0E0CEE",
                fg_color=PAPER_HI,
                border_color=GOLD,
                border_width=2,
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
            y=210,
            w=480,
            h=270,
            bg_color=BTN,
            border_color=GOLD,
            border_width=2,
            hidden=True,
        ),
        L(
            type="label",
            parent_name="promotion",
            x=28,
            y=28,
            w=300,
            h=34,
            text="选择升变棋子",
            font_size=24,
            fg_color=PAPER_HI,
        ),
    ]
    for index, (piece, label) in enumerate(
        (("Q", "后"), ("R", "车"), ("B", "象"), ("N", "马"))
    ):
        x = 38 + index * 104
        objects += [
            L(
                type="button",
                parent_name="promotion",
                name=f"promote_{piece.lower()}",
                callback=f"promote_{piece.lower()}",
                x=x,
                y=92,
                w=92,
                h=148,
                text="",
                bg_color="#2A1814",
                fg_color=PAPER_HI,
                border_color=GOLD,
                border_width=2,
            ),
            L(
                type="image",
                parent_name="promotion",
                name=f"promotion_w{piece.lower()}_image",
                bind=f"promotion_w{piece.lower()}_image",
                bind_target="visible",
                x=x + 26,
                y=104,
                w=39,
                h=64,
                image=f"{PIECE_SET}/w{piece}.png",
            ),
            L(
                type="image",
                parent_name="promotion",
                name=f"promotion_b{piece.lower()}_image",
                bind=f"promotion_b{piece.lower()}_image",
                bind_target="visible",
                x=x + 26,
                y=104,
                w=39,
                h=64,
                image=f"{PIECE_SET}/b{piece}.png",
                hidden=True,
            ),
            L(
                type="label",
                parent_name="promotion",
                x=x,
                y=198,
                w=92,
                h=28,
                text=label,
                font_size=18,
                text_align="center",
                fg_color=PAPER_HI,
            ),
        ]
    # Confirmation and PvP draw response layers.
    objects += [
        L(
            type="container",
            parent_name="play",
            name="resign_confirm",
            bind="resign_confirm",
            bind_target="visible",
            x=0,
            y=258,
            w=480,
            h=222,
            bg_color=BTN,
            border_color=GOLD,
            border_width=2,
            hidden=True,
        ),
        L(
            type="label",
            parent_name="resign_confirm",
            x=28,
            y=34,
            w=424,
            h=34,
            text="确认认输？",
            font_size=24,
            fg_color=PAPER_HI,
        ),
        L(
            type="button",
            parent_name="resign_confirm",
            callback="cancel_resign",
            x=28,
            y=110,
            w=198,
            h=62,
            text="继续对局",
            font_size=18,
            bg_color=PAPER,
            fg_color=INK,
        ),
        L(
            type="button",
            parent_name="resign_confirm",
            callback="confirm_resign",
            x=254,
            y=110,
            w=198,
            h=62,
            text="确认认输",
            font_size=18,
            bg_color=VERM,
            fg_color=PAPER_HI,
        ),
        L(
            type="container",
            parent_name="play",
            name="draw_request",
            bind="draw_request",
            bind_target="visible",
            x=0,
            y=258,
            w=480,
            h=222,
            bg_color=BTN,
            border_color=GOLD,
            border_width=2,
            hidden=True,
        ),
        L(
            type="label",
            parent_name="draw_request",
            x=28,
            y=34,
            w=424,
            h=34,
            text="对方请求和棋",
            font_size=24,
            fg_color=PAPER_HI,
        ),
        L(
            type="button",
            parent_name="draw_request",
            callback="draw_reject",
            x=28,
            y=110,
            w=198,
            h=62,
            text="拒绝",
            font_size=18,
            bg_color=PAPER,
            fg_color=INK,
        ),
        L(
            type="button",
            parent_name="draw_request",
            callback="draw_accept",
            x=254,
            y=110,
            w=198,
            h=62,
            text="接受",
            font_size=18,
            bg_color=VERM,
            fg_color=PAPER_HI,
        ),
        L(
            type="container",
            parent_name="play",
            name="connection_lost",
            bind="connection_lost",
            bind_target="visible",
            x=0,
            y=258,
            w=480,
            h=222,
            bg_color=BTN,
            border_color=GOLD,
            border_width=2,
            hidden=True,
        ),
        L(
            type="label",
            parent_name="connection_lost",
            x=28,
            y=34,
            w=424,
            h=34,
            text="连接已断开",
            font_size=24,
            fg_color=PAPER_HI,
        ),
        L(
            type="label",
            parent_name="connection_lost",
            x=28,
            y=72,
            w=424,
            h=24,
            text="正在等待对手重新连接",
            font_size=16,
            fg_color=GOLD,
        ),
        L(
            type="button",
            parent_name="connection_lost",
            callback="dev_sync_error",
            x=28,
            y=120,
            w=198,
            h=62,
            text="同步失败",
            font_size=18,
            bg_color=PAPER,
            fg_color=INK,
        ),
        L(
            type="button",
            parent_name="connection_lost",
            callback="dev_reconnect",
            x=254,
            y=120,
            w=198,
            h=62,
            text="模拟重连",
            font_size=18,
            bg_color=VERM,
            fg_color=PAPER_HI,
        ),
        L(
            type="container",
            parent_name="play",
            name="sync_error",
            bind="sync_error",
            bind_target="visible",
            x=0,
            y=258,
            w=480,
            h=222,
            bg_color=BTN,
            border_color=GOLD,
            border_width=2,
            hidden=True,
        ),
        L(
            type="label",
            parent_name="sync_error",
            x=28,
            y=34,
            w=424,
            h=34,
            text="同步失败",
            font_size=24,
            fg_color=PAPER_HI,
        ),
        L(
            type="label",
            parent_name="sync_error",
            x=28,
            y=72,
            w=424,
            h=24,
            text="对局状态无法确认",
            font_size=16,
            fg_color=GOLD,
        ),
        L(
            type="button",
            parent_name="sync_error",
            callback="dev_retry_sync",
            x=28,
            y=120,
            w=424,
            h=62,
            text="重新同步",
            font_size=18,
            bg_color=VERM,
            fg_color=PAPER_HI,
        ),
    ]
    objects += [
        L(
            type="container",
            parent_name="play",
            name="menu",
            x=0,
            y=220,
            w=480,
            h=260,
            bg_color=PAPER,
            hidden=True,
        ),
        L(
            type="label",
            parent_name="menu",
            x=36,
            y=20,
            w=408,
            h=32,
            text="对局菜单",
            font_size=18,
            fg_color=INK,
        ),
        L(
            type="button",
            parent_name="menu",
            name="resign",
            callback="resign",
            x=36,
            y=64,
            w=196,
            h=52,
            text="认输",
            font_size=18,
            bg_color=VERM,
            fg_color=PAPER_HI,
        ),
        L(
            type="button",
            parent_name="menu",
            name="draw",
            callback="offer_draw",
            x=248,
            y=64,
            w=196,
            h=52,
            text="求和",
            font_size=18,
            font_charset="求和仅双人可",
            bg_color="#E4D4B8",
            fg_color=INK,
            hidden=True,
        ),
        L(
            type="button",
            parent_name="menu",
            x=36,
            y=128,
            w=196,
            h=52,
            text="继续下",
            font_size=18,
            bg_color="#E4D4B8",
            fg_color=INK,
            events=[{"event": "click", "action": "hide", "target_name": "menu"}],
        ),
        L(
            type="button",
            parent_name="menu",
            name="dev_disconnect",
            callback="dev_disconnect",
            x=248,
            y=128,
            w=196,
            h=52,
            text="模拟断线",
            font_size=18,
            bg_color="#E4D4B8",
            fg_color=INK,
            hidden=True,
        ),
        L(
            type="button",
            parent_name="menu",
            callback="leave_game",
            x=36,
            y=192,
            w=408,
            h=52,
            text="返回主菜单",
            font_size=18,
            bg_color="#E4D4B8",
            fg_color=INK,
        ),
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
            events=[{"event": "click", "action": "show", "target_name": "menu"}],
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
