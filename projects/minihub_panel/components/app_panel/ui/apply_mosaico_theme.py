#!/usr/bin/env python3
"""Bind MiniHub scene colors to GSP dynamic themes. Default values are OLED dark."""

import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parent

THEMES = {
    "bg": {"type": "color", "default": "#000000", "dynamic": True},
    "card": {"type": "color", "default": "#191B1D", "dynamic": True},
    "key": {"type": "color", "default": "#272A2D", "dynamic": True},
    "ink": {"type": "color", "default": "#F3F4F2", "dynamic": True},
    "muted": {"type": "color", "default": "#B2B7BA", "dynamic": True},
    "accent": {"type": "color", "default": "#FF4C01", "dynamic": True},
    "cta": {"type": "color", "default": "#FF4C01", "dynamic": True},
}

COLOR_FIELDS = (
    "bg_color",
    "fg_color",
    "knob_color",
    "key_color",
    "function_color",
    "ok_color",
    "ok_text_color",
    "space_color",
    "space_text_color",
    "delete_color",
    "delete_text_color",
    "function_text_color",
    "border_color",
)

HEX_TOKEN = {
    "#000000": "bg",
    "#090A0B": "bg",
    "#F6F6F3": None,  # field-dependent
    "#F4F6F8": "key",
    "#FFFFFF": None,
    "#F3F4F2": "ink",
    "#ECECF0": "ink",
    "#101010": None,
    "#17212B": "ink",
    "#191B1D": "card",
    "#272A2D": "key",
    "#70706C": "muted",
    "#7A8793": "muted",
    "#4E5965": "muted",
    "#33404D": "muted",
    "#B2B7BA": "muted",
    "#CBCBC6": "key",
    "#CECECA": "key",
    "#DCE5EE": "key",
    "#E5EAF0": "key",
    "#E4F5EE": "card",
    "#FFF0E8": "card",
    "#FF4C01": "accent",
    "#E84400": "accent",
    "#1677FF": "accent",
    "#8BB4FF": "accent",
    "#22A06B": "accent",
    "#126145": "accent",
    "#C23B22": "accent",
}


def token_ref(name):
    return {"theme": name}


def hex_token(field, value):
    color = value.upper()
    mapped = HEX_TOKEN.get(color)
    if mapped:
        return mapped
    if color == "#F6F6F3":
        return None if field == "fg_color" else "bg"
    if color == "#FFFFFF":
        return "ink" if field == "fg_color" else "card"
    if color == "#101010":
        return "ink" if field == "fg_color" else "cta"
    return None


def rewrite_color(obj, field, value):
    if isinstance(value, dict) and "theme" in value:
        return value
    if not isinstance(value, str) or not value.startswith("#"):
        return value
    token = hex_token(field, value)
    if token is None:
        return value
    if obj.get("type") == "keyboard":
        if field in ("ok_color",):
            token = "cta"
        elif field in ("ok_text_color",):
            return "#F3F4F2"
        elif field in ("key_color", "function_color", "space_color", "delete_color"):
            token = "key"
        elif field in ("fg_color", "function_text_color", "space_text_color",
                       "delete_text_color"):
            token = "ink"
        elif field == "bg_color":
            token = "bg"
    if field == "knob_color":
        token = "accent"
    if obj.get("name") in {"lang_continue", "scene_create", "automation_create",
                           "detail_power", "detail_reconnect", "reboot_ok"}:
        if field == "bg_color":
            token = "cta"
    if obj.get("name") in {"lang_continue_text", "scene_create_text",
                           "automation_create_text", "detail_power_text",
                           "detail_reconnect_text", "reboot_ok_text"}:
        if field == "fg_color":
            return "#F3F4F2"
    if obj.get("name") in {"detail_delete", "add_cancel", "scene_0_delete",
                           "scene_1_delete", "scene_2_delete", "scene_3_delete",
                           "rule_0_delete", "rule_1_delete", "rule_2_delete",
                           "rule_3_delete", "reboot_cancel"}:
        if field == "bg_color":
            token = "card"
    return token_ref(token)


def insert_theme_control(objects):
    reboot = next(i for i, obj in enumerate(objects) if obj.get("name") == "system_reboot")
    objects[reboot]["y"] = 328
    objects[reboot]["h"] = 52
    for obj in objects:
        if obj.get("name") == "system_reboot_text":
            obj["y"] = 12
        if obj.get("name") == "system_update_hint":
            obj["y"] = 386
            obj["h"] = 28
    theme_btn = {
        "type": "button",
        "parent_name": "system_page",
        "x": 20,
        "y": 270,
        "w": 440,
        "h": 50,
        "bg_color": token_ref("card"),
        "radius": 16,
        "name": "system_theme",
        "callback": "system_theme",
    }
    theme_text = {
        "type": "label",
        "parent_name": "system_theme",
        "x": 20,
        "y": 11,
        "w": 400,
        "h": 28,
        "text": "Appearance  Dark",
        "font_size": 18,
        "fg_color": token_ref("ink"),
        "font": "fonts/NotoSansSC-Regular.otf",
        "font_charset_file": "fonts/cjk.txt",
        "name": "system_theme_text",
        "bind": "system_theme_text",
    }
    objects[reboot:reboot] = [theme_btn, theme_text]


def intended_parent(name, unnamed_group):
    if name in {"hub_status_dot", "status"}:
        return "wifi"
    match = re.match(r"(bank_[01]_card_[0-3])_(name|meta|value)$", name or "")
    if match:
        return match.group(1)
    match = re.match(r"(bank_[01])_card_[0-3]$", name or "")
    if match:
        return match.group(1)
    named = {
        "previous_text": "previous",
        "refresh_text": "refresh",
        "next_text": "next",
        "detail_back": "detail",
        "detail_title": "detail",
        "detail_meta": "detail",
        "detail_power": "detail",
        "detail_power_text": "detail_power",
        "detail_brightness_row": "detail",
        "detail_hue_row": "detail",
        "detail_saturation_row": "detail",
        "detail_cct_row": "detail",
        "detail_brightness": "detail_brightness_row",
        "detail_brightness_value": "detail_brightness_row",
        "detail_hue": "detail_hue_row",
        "detail_hue_value": "detail_hue_row",
        "detail_saturation": "detail_saturation_row",
        "detail_saturation_value": "detail_saturation_row",
        "detail_cct": "detail_cct_row",
        "detail_cct_value": "detail_cct_row",
        "detail_delete": "detail",
        "detail_delete_text": "detail_delete",
    }
    if name in named:
        return named[name]
    return unnamed_group


def fix_numeric_parents(objects):
    """Numeric parent indices break when objects are inserted. Bind by name."""
    unnamed_group = None
    leftover = []
    for obj in objects:
        name = obj.get("name")
        if name == "detail_back":
            unnamed_group = "detail_back"
        elif name in {"detail_brightness_row", "detail_hue_row",
                      "detail_saturation_row", "detail_cct_row"}:
            unnamed_group = name
        parent = obj.get("parent")
        if not (isinstance(parent, int) and parent >= 0):
            continue
        target = intended_parent(name, unnamed_group)
        if not target:
            leftover.append((name or obj.get("type"), parent))
            continue
        obj.pop("parent")
        obj["parent_name"] = target
    if leftover:
        raise SystemExit(f"unmapped numeric parents: {leftover}")


def polish_shell(objects):
    tab_x = {"tab_devices": 0, "tab_scenes": 112,
             "tab_automation": 224, "tab_settings": 336}
    for obj in objects:
        name = obj.get("name")
        if name in {"bank_0", "bank_1", "detail"}:
            obj["bg_color"] = token_ref("bg")
        if name == "detail_back":
            obj["bg_color"] = token_ref("card")
        if name == "tab_bar":
            obj["x"] = 16
            obj["y"] = 422
            obj["w"] = 448
            obj["h"] = 46
            obj["radius"] = 12
        if name in tab_x:
            obj["x"] = tab_x[name]
            obj["y"] = 0
            obj["w"] = 112
            obj["h"] = 46
            obj["radius"] = 0
            obj["bg_color"] = token_ref("card")
        if name in {"tab_devices_text", "tab_scenes_text",
                    "tab_automation_text", "tab_settings_text"}:
            obj["w"] = 112
            obj["h"] = 46
        if name in {"tab_devices_indicator", "tab_scenes_indicator",
                    "tab_automation_indicator", "tab_settings_indicator"}:
            obj["x"] = 42
            obj["y"] = 40
            obj["w"] = 28
            obj["h"] = 3


def split_welcome(objects):
    mark = next(i for i, obj in enumerate(objects) if obj.get("name") == "welcome_mark")
    objects[mark]["image"] = "assets/minihub-dark.png"
    objects[mark]["bind"] = "welcome_mark"
    objects[mark]["bind_target"] = "visible"
    light = {
        "type": "image",
        "parent_name": "welcome_page",
        "x": 0,
        "y": 0,
        "w": 480,
        "h": 480,
        "image": "assets/minihub-light.png",
        "name": "welcome_mark_light",
        "bind": "welcome_mark_light",
        "bind_target": "visible",
        "hidden": True,
    }
    objects.insert(mark + 1, light)


def main():
    src = json.loads((ROOT / "main.json").read_text())
    src["screen_bg"] = "#000000"
    src["themes"] = THEMES
    for obj in src["objects"]:
        for field in COLOR_FIELDS:
            if field in obj:
                obj[field] = rewrite_color(obj, field, obj[field])
    if not any(obj.get("name") == "system_theme" for obj in src["objects"]):
        insert_theme_control(src["objects"])
    if not any(obj.get("name") == "welcome_mark_light" for obj in src["objects"]):
        split_welcome(src["objects"])
    fix_numeric_parents(src["objects"])
    polish_shell(src["objects"])
    (ROOT / "main.json").write_text(
        json.dumps(src, ensure_ascii=False, indent=2) + "\n"
    )
    remaining = []
    for obj in src["objects"]:
        for field in COLOR_FIELDS:
            value = obj.get(field)
            if isinstance(value, str) and value.startswith("#"):
                remaining.append((obj.get("name"), field, value))
    print(f"objects={len(src['objects'])} leftover_hex={remaining}")


if __name__ == "__main__":
    main()
