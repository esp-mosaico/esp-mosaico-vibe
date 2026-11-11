#!/usr/bin/env python3
"""Splice MiniHub onboarding pages into main.json. Run from this directory."""

import json
from pathlib import Path

ROOT = Path(__file__).resolve().parent
CJK = "fonts/cjk.txt"
SC = "fonts/NotoSansSC-Regular.otf"
BOLD = "fonts/DejaVuSans-Bold.ttf"
CJK_CHARSET = (
    " !\"#$%&'()*+,-./0123456789:;<=>?@"
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~"
    "，。！？：；·+"
    "中文简体设置语言默认继续网络正在搜索选择连接已密码输入错误"
    "模式就绪在这台上运行其它地址家里店里设备更换切换"
    "温度开关传感器"
    "添加码二维码发现入网完成失败请重试删除取消停除"
    "深浅色外观"
)

def theme(name):
    return {"theme": name}


BG = theme("bg")
INK = theme("ink")
ACCENT = theme("accent")
MUTED = theme("muted")
CARD = theme("card")
LINE = theme("key")
SOFT = theme("key")
CTA = theme("cta")
FACE = "#F3F4F2"
ERR = theme("accent")


def page(name, hidden=True):
    return {
        "type": "container",
        "parent": -1,
        "x": 0,
        "y": 0,
        "w": 480,
        "h": 480,
        "bg_color": BG,
        "name": name,
        "bind": name,
        "bind_target": "visible",
        "hidden": hidden,
    }


def label(parent, name, x, y, w, h, text, size, color, **extra):
    obj = {
        "type": "label",
        "parent_name": parent,
        "x": x,
        "y": y,
        "w": w,
        "h": h,
        "text": text,
        "font_size": size,
        "fg_color": color,
    }
    if name:
        obj["name"] = name
        obj["bind"] = name
    obj.update(extra)
    return obj


def button(parent, name, x, y, w, h, bg, radius=16):
    return {
        "type": "button",
        "parent_name": parent,
        "x": x,
        "y": y,
        "w": w,
        "h": h,
        "bg_color": bg,
        "radius": radius,
        "name": name,
        "callback": name,
        "bind": name,
        "bind_target": "visible",
    }


def box(parent, name, x, y, w, h, bg, radius=0, hidden=False):
    obj = {
        "type": "container",
        "parent_name": parent,
        "x": x,
        "y": y,
        "w": w,
        "h": h,
        "bg_color": bg,
        "radius": radius,
    }
    if name:
        obj["name"] = name
        obj["bind"] = name
        obj["bind_target"] = "visible"
    if hidden:
        obj["hidden"] = True
    return obj


def onboard():
    objects = []

    # The name is the mark. One orange tittle, MOSAICO stays in the same image.
    objects.append(page("welcome_page", hidden=False))
    objects.append({
        "type": "image",
        "parent_name": "welcome_page",
        "x": 0,
        "y": 0,
        "w": 480,
        "h": 480,
        "image": "assets/minihub-dark.png",
        "name": "welcome_mark",
        "bind": "welcome_mark",
        "bind_target": "visible",
    })
    objects.append({
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
    })

    # --- language ---
    objects.append(page("lang_page"))
    objects.append(button("lang_page", "lang_back", 36, 36, 56, 48, LINE, 12))
    objects.append(label("lang_back", None, 0, 0, 56, 48, "<", 24, INK,
                         text_align="center"))
    objects.append(label("lang_page", "lang_kicker", 36, 48, 408, 22, "SETUP", 13, MUTED,
                         font=SC, font_charset_file=CJK, text_align="center"))
    objects.append(label("lang_page", "lang_title", 36, 74, 408, 40, "Language", 32, INK,
                         font=SC, font_charset_file=CJK, text_align="center"))
    objects.append(button("lang_page", "lang_en", 36, 140, 408, 88, CARD, 18))
    objects.append(label("lang_en", "lang_en_text", 24, 18, 360, 32, "English", 22, INK,
                         font=SC, font_charset_file=CJK))
    objects.append(label("lang_en", "lang_en_meta", 24, 52, 360, 22, "Default", 14, MUTED,
                         font=SC, font_charset_file=CJK))
    objects.append(box("lang_en", "lang_en_mark", 364, 34, 20, 20, ACCENT, 10))
    objects.append(button("lang_page", "lang_zh", 36, 244, 408, 88, CARD, 18))
    objects.append(label("lang_zh", "lang_zh_text", 24, 18, 360, 32, "中文", 22, INK,
                         font=SC, font_charset_file=CJK, font_size=22))
    objects.append(label("lang_zh", "lang_zh_meta", 24, 52, 360, 22, "简体", 14, MUTED,
                         font=SC, font_charset_file=CJK))
    objects.append(box("lang_zh", "lang_zh_mark", 364, 34, 20, 20, ACCENT, 10, True))
    objects.append(button("lang_page", "lang_continue", 36, 380, 408, 56, CTA, 12))
    objects.append(label("lang_continue", "lang_continue_text", 0, 0, 408, 56, "Continue", 20,
                         FACE, font=SC, font_charset_file=CJK, text_align="center"))

    # --- wifi ---
    objects.append(page("wifi_page"))
    objects.append(button("wifi_page", "wifi_back", 36, 36, 56, 48, LINE, 12))
    objects.append(label("wifi_back", None, 0, 0, 56, 48, "<", 24, INK,
                         text_align="center"))
    objects.append(label("wifi_page", "wifi_kicker", 104, 36, 340, 20, "NETWORK", 13, MUTED,
                         font=SC, font_charset_file=CJK))
    objects.append(label("wifi_page", "wifi_title", 104, 56, 340, 36, "Wi-Fi", 28, INK,
                         font=SC, font_charset_file=CJK))
    objects.append(label("wifi_page", "wifi_status", 36, 104, 408, 24, "Scanning...", 15, MUTED,
                         font=SC, font_charset_file=CJK))
    objects.append(box("wifi_page", "wifi_dot_0", 36, 136, 10, 10, ACCENT, 5))
    objects.append(box("wifi_page", "wifi_dot_1", 54, 136, 10, 10, SOFT, 5))
    objects.append(box("wifi_page", "wifi_dot_2", 72, 136, 10, 10, SOFT, 5))
    objects.append({
        "type": "list",
        "parent_name": "wifi_page",
        "name": "wifi_list",
        "x": 36,
        "y": 160,
        "w": 408,
        "h": 276,
        "item_count": 16,
        "item_height": 64,
        "row_template": "wifi_list_row",
        "scroll_snapshot": True,
        "bg_color": CARD,
        "fg_color": INK,
        "radius": 16,
        "hidden": True,
    })
    objects.append({
        "type": "container",
        "parent": -1,
        "x": 0,
        "y": 0,
        "w": 408,
        "h": 64,
        "name": "wifi_list_row",
        "template": "wifi_list_row",
        "callback": "wifi_list",
        "bg_color": CARD,
    })
    objects.append(label("wifi_list_row", "wifi_list_row_text", 0, 0, 408, 64,
                         "Network", 17, INK, font=SC, font_charset_file=CJK))
    objects.append(box("wifi_page", "wifi_result", 36, 176, 408, 200, CARD, 18, True))
    objects.append(label("wifi_result", "wifi_result_title", 24, 48, 360, 40, "Connected", 28, INK,
                         font=SC, font_charset_file=CJK, text_align="center"))
    objects.append(label("wifi_result", "wifi_result_meta", 24, 100, 360, 48, "", 15, MUTED,
                         font=SC, font_charset_file=CJK, text_align="center"))

    # --- password ---
    password_page = page("wifi_password_page")
    password_page.pop("parent")
    password_page["parent_name"] = "wifi_page"
    objects.append(password_page)
    objects.append(button("wifi_password_page", "wifi_password_back", 36, 36, 56, 48, LINE, 12))
    objects.append(label("wifi_password_back", None, 0, 0, 56, 48, "<", 24, INK, text_align="center"))
    objects.append(label("wifi_password_page", "wifi_password_title", 104, 36, 340, 28, "Password",
                         20, INK, font=SC, font_charset_file=CJK))
    objects.append(label("wifi_password_page", "wifi_password_meta", 104, 64, 340, 22,
                         "Enter the Wi-Fi password", 13, MUTED, font=SC, font_charset_file=CJK))
    objects.append(box("wifi_password_page", None, 36, 100, 408, 48, CARD, 12))
    objects.append(label("wifi_password_page", "wifi_password", 52, 110, 376, 28, "", 16, INK,
                         input=True, bind_target="text",
                         font_charset=" !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~"))
    objects.append({
        "type": "keyboard",
        "parent_name": "wifi_password_page",
        "x": 20,
        "y": 164,
        "w": 440,
        "h": 288,
        "font_size": 14,
        "bg_color": BG,
        "fg_color": INK,
        "key_color": CARD,
        "function_color": LINE,
        "ok_color": CTA,
        "ok_text_color": FACE,
        "ok_label": "Join",
        "key_radius": 8,
        "name": "wifi_keyboard",
    })

    # --- role ---
    objects.append(page("role_page"))
    objects.append(button("role_page", "role_back", 36, 36, 56, 48, LINE, 12))
    objects.append(label("role_back", None, 0, 0, 56, 48, "<", 24, INK,
                         text_align="center"))
    objects.append(label("role_page", "role_kicker", 104, 36, 340, 20, "PRODUCT", 13, MUTED,
                         font=SC, font_charset_file=CJK))
    objects.append(label("role_page", "role_title", 104, 56, 340, 36, "Choose mode", 28, INK,
                         font=SC, font_charset_file=CJK))
    objects.append(label("role_page", "role_meta", 36, 108, 408, 22, "Wi-Fi is ready", 14, MUTED,
                         font=SC, font_charset_file=CJK))
    objects.append(button("role_page", "role_hub", 36, 148, 408, 120, CARD, 18))
    objects.append(box("role_hub", None, 24, 28, 36, 36, ACCENT, 8))
    objects.append(label("role_hub", "role_hub_title", 76, 24, 300, 32, "Hub", 22, INK,
                         font=SC, font_charset_file=CJK))
    objects.append(label("role_hub", "role_hub_meta", 76, 60, 300, 40,
                         "Run MiniHub on this Mosaico.", 14, MUTED, font=SC, font_charset_file=CJK))
    objects.append(button("role_page", "role_web", 36, 284, 408, 120, CARD, 18))
    objects.append(box("role_web", None, 24, 28, 36, 36, INK, 8))
    objects.append(label("role_web", "role_web_title", 76, 24, 300, 32, "Panel", 22, INK,
                         font=SC, font_charset_file=CJK))
    objects.append(label("role_web", "role_web_meta", 76, 60, 300, 40,
                         "Control another MiniHub.", 14, MUTED, font=SC, font_charset_file=CJK))

    # --- settings ---
    objects.append(page("settings_page"))
    objects.append(button("settings_page", "settings_back", 36, 36, 56, 48, LINE, 12))
    objects.append(label("settings_back", None, 0, 0, 56, 48, "<", 24, INK,
                         text_align="center"))
    objects.append(label("settings_page", "settings_title", 104, 40, 340, 36,
                         "Settings", 28, INK, font=SC, font_charset_file=CJK))
    objects.append(button("settings_page", "settings_wifi", 36, 108, 408, 96, CARD, 18))
    objects.append(label("settings_wifi", "settings_wifi_title", 24, 16, 360, 30,
                         "Wi-Fi", 22, INK, font=SC, font_charset_file=CJK))
    objects.append(label("settings_wifi", "settings_wifi_meta", 24, 52, 360, 26,
                         "Choose another network", 14, MUTED,
                         font=SC, font_charset_file=CJK))
    objects.append(button("settings_page", "settings_role", 36, 218, 408, 96, CARD, 18))
    objects.append(label("settings_role", "settings_role_title", 24, 16, 360, 30,
                         "Device mode", 22, INK, font=SC, font_charset_file=CJK))
    objects.append(label("settings_role", "settings_role_meta", 24, 52, 360, 26,
                         "Switch between Hub and Panel", 14, MUTED,
                         font=SC, font_charset_file=CJK))
    objects.append(button("settings_page", "settings_lang", 36, 328, 408, 96, CARD, 18))
    objects.append(label("settings_lang", "settings_lang_title", 24, 16, 360, 30,
                         "Language", 22, INK, font=SC, font_charset_file=CJK))
    objects.append(label("settings_lang", "settings_lang_meta", 24, 52, 360, 26,
                         "English / 中文", 14, MUTED,
                         font=SC, font_charset_file=CJK))

    # --- token (kept, restyled) ---
    objects.append(page("token_page"))
    objects.append(button("token_page", "token_back", 36, 36, 56, 48, LINE, 12))
    objects.append(label("token_back", None, 0, 0, 56, 48, "<", 24, INK, text_align="center"))
    objects.append(label("token_page", "token_title", 104, 36, 340, 28, "MiniHub token", 20, INK,
                         font=SC, font_charset_file=CJK))
    objects.append(label("token_page", "token_meta", 104, 64, 340, 22, "Enter the Bearer token",
                         13, MUTED, font=SC, font_charset_file=CJK))
    objects.append(box("token_page", None, 36, 100, 408, 48, CARD, 12))
    objects.append(label("token_page", "token", 52, 110, 376, 28, "", 16, INK,
                         input=True, bind_target="text",
                         font_charset=" !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~"))
    objects.append({
        "type": "keyboard",
        "parent_name": "token_page",
        "x": 20,
        "y": 164,
        "w": 440,
        "h": 288,
        "font_size": 14,
        "bg_color": BG,
        "fg_color": INK,
        "key_color": CARD,
        "function_color": LINE,
        "ok_color": CTA,
        "ok_text_color": FACE,
        "ok_label": "Save",
        "key_radius": 8,
        "name": "token_keyboard",
    })

    # --- add device (Matter commissioning) ---
    objects.append(page("add_page"))
    objects.append(button("add_page", "add_back", 36, 36, 56, 48, LINE, 12))
    objects.append(label("add_back", None, 0, 0, 56, 48, "<", 24, INK,
                         text_align="center"))
    objects.append(label("add_page", "add_title", 104, 36, 340, 28,
                         "Add Matter device", 20, INK,
                         font=SC, font_charset_file=CJK))
    objects.append(label("add_page", "add_meta", 104, 64, 340, 22,
                         "Enter Matter pairing code", 13, MUTED,
                         font=SC, font_charset_file=CJK))
    objects.append(box("add_page", "add_input_box", 36, 100, 408, 48, CARD, 12))
    objects.append(label("add_page", "add_code", 52, 110, 376, 28, "", 16, INK,
                         input=True, bind_target="text",
                         font_charset=" !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~"))
    objects.append({
        "type": "keyboard",
        "parent_name": "add_page",
        "x": 20,
        "y": 164,
        "w": 440,
        "h": 288,
        "font_size": 14,
        "bg_color": BG,
        "fg_color": INK,
        "key_color": CARD,
        "function_color": LINE,
        "ok_color": CTA,
        "ok_text_color": FACE,
        "ok_label": "Add",
        "key_radius": 8,
        "name": "add_keyboard",
    })
    objects.append(box("add_page", "add_status_page", 0, 100, 480, 380, BG, 0, True))
    objects.append(box("add_status_page", "add_result", 36, 40, 408, 192, CARD, 18))
    objects.append(label("add_result", "add_result_title", 24, 44, 360, 40,
                         "Discovering...", 26, INK,
                         font=SC, font_charset_file=CJK, text_align="center"))
    objects.append(label("add_result", "add_result_meta", 24, 96, 360, 48,
                         "", 15, MUTED,
                         font=SC, font_charset_file=CJK, text_align="center"))
    objects.append(button("add_status_page", "add_cancel", 36, 256, 408, 56, ERR, 14))
    objects.append(label("add_cancel", "add_cancel_text", 0, 0, 408, 56,
                         "Cancel pairing", 18, INK,
                         font=SC, font_charset_file=CJK, text_align="center"))
    return objects


def main():
    (ROOT / CJK).write_text(CJK_CHARSET + "\n")
    src = json.loads((ROOT / "main.json").read_text())
    kept = []
    for obj in src["objects"]:
        name = obj.get("name")
        parent = obj.get("parent_name")
        if obj.get("template") == "wifi_list_row" or name in {
            "welcome_page", "welcome_mark", "welcome_mark_light", "welcome_title",
            "lang_page", "lang_back", "lang_kicker", "lang_title",
            "lang_en", "lang_en_text", "lang_en_meta", "lang_en_mark",
            "lang_zh", "lang_zh_text", "lang_zh_meta", "lang_zh_mark",
            "lang_continue", "lang_continue_text",
            "wifi_page", "wifi_back", "wifi_kicker", "wifi_title", "wifi_status",
            "wifi_dot_0", "wifi_dot_1", "wifi_dot_2", "wifi_list",
            "wifi_list_row", "wifi_list_row_text",
            "wifi_result", "wifi_result_title", "wifi_result_meta",
            "wifi_password_page", "wifi_password_back",
            "wifi_password_title", "wifi_password_meta",
            "wifi_password", "wifi_keyboard",
            "token_page", "token_back", "token_title", "token_meta",
            "token", "token_keyboard",
            "role_page", "role_back", "role_kicker", "role_title", "role_meta",
            "role_hub", "role_hub_title", "role_hub_meta",
            "role_web", "role_web_title", "role_web_meta",
            "settings_page", "settings_back", "settings_title",
            "settings_wifi", "settings_wifi_title", "settings_wifi_meta",
            "settings_role", "settings_role_title", "settings_role_meta",
            "settings_lang", "settings_lang_title", "settings_lang_meta",
            "add_page", "add_back", "add_title", "add_meta",
            "add_input_box", "add_code", "add_keyboard",
            "add_status_page", "add_result", "add_result_title", "add_result_meta",
            "add_cancel", "add_cancel_text",
            "detail_delete", "detail_delete_text",
            "wifi_meta", "wifi_scan",
            "wifi_ap_0", "wifi_ap_0_name", "wifi_ap_0_meta",
            "wifi_ap_1", "wifi_ap_1_name", "wifi_ap_1_meta",
            "wifi_ap_2", "wifi_ap_2_name", "wifi_ap_2_meta",
            "wifi_ap_3", "wifi_ap_3_name", "wifi_ap_3_meta",
            "wifi_previous", "wifi_page_text", "wifi_next",
        }:
            continue
        if parent in {
            "welcome_page", "welcome_mark", "welcome_mark_light", "lang_page", "lang_back", "lang_en", "lang_zh", "lang_continue",
            "wifi_page", "wifi_back", "wifi_list_row", "wifi_result", "wifi_password_page",
            "wifi_password_back", "token_page", "token_back",
            "role_page", "role_back", "role_hub", "role_web",
            "settings_page", "settings_back", "settings_wifi", "settings_role", "settings_lang",
            "add_page", "add_back", "add_status_page", "add_result", "add_cancel",
            "detail_delete",
            "wifi_scan", "wifi_ap_0", "wifi_ap_1",
            "wifi_ap_2", "wifi_ap_3", "wifi_previous", "wifi_next",
        }:
            continue
        if obj.get("font") == SC and "font_charset" in obj:
            obj["font_charset"] = CJK_CHARSET
        if obj.get("text") == "Changes are applied locally through MiniHub":
            continue
        if obj.get("name") == "detail_power":
            obj["bind"] = "detail_power"
            obj["bind_target"] = "visible"
        kept.append(obj)

    kept.append({
        "type": "button",
        "parent_name": "detail",
        "x": 36,
        "y": 388,
        "w": 408,
        "h": 52,
        "bg_color": ERR,
        "radius": 14,
        "name": "detail_delete",
        "callback": "detail_delete",
        "bind": "detail_delete",
        "bind_target": "visible",
    })
    kept.append({
        "type": "label",
        "parent_name": "detail_delete",
        "x": 0,
        "y": 0,
        "w": 408,
        "h": 52,
        "text": "Delete device",
        "font_size": 16,
        "fg_color": FACE,
        "font": SC,
        "font_charset": CJK_CHARSET,
        "text_align": "center",
        "name": "detail_delete_text",
        "bind": "detail_delete_text",
    })

    src["screen_bg"] = "#000000"
    src.setdefault("themes", {})
    src["themes"].update({
        "bg": {"type": "color", "default": "#000000", "dynamic": True},
        "card": {"type": "color", "default": "#191B1D", "dynamic": True},
        "key": {"type": "color", "default": "#272A2D", "dynamic": True},
        "ink": {"type": "color", "default": "#F3F4F2", "dynamic": True},
        "muted": {"type": "color", "default": "#B2B7BA", "dynamic": True},
        "accent": {"type": "color", "default": "#FF4C01", "dynamic": True},
        "cta": {"type": "color", "default": "#FF4C01", "dynamic": True},
    })
    src["font"] = "fonts/DejaVuSans.ttf"
    onboarding = onboard()
    for obj in onboarding:
        parent = obj.get("parent")
        if isinstance(parent, int) and parent >= 0:
            obj["parent"] = parent + len(kept)
    src["objects"] = kept + onboarding
    (ROOT / "main.json").write_text(json.dumps(src, ensure_ascii=False, indent=2) + "\n")
    print(f"objects={len(src['objects'])}")


if __name__ == "__main__":
    main()
