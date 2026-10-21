#!/usr/bin/env python3
"""Check that the generated scene contract matches the native backend."""

from __future__ import annotations

import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SCENE = ROOT / "ui" / "main.json"
BACKEND = ROOT / "main" / "game_ui.c"


def macro_name(value: str) -> str:
    return re.sub(r"[^A-Za-z0-9]+", "_", value).strip("_").upper()


def fail(message: str) -> None:
    print(f"GSP contract error: {message}", file=sys.stderr)
    raise SystemExit(1)


def main() -> None:
    document = json.loads(SCENE.read_text(encoding="utf-8"))
    objects = document.get("objects")
    if not isinstance(objects, list):
        fail("scene has no objects array")

    callbacks = {
        item["callback"]
        for item in objects
        if isinstance(item, dict) and isinstance(item.get("callback"), str)
    }
    binds = {
        item["bind"]
        for item in objects
        if isinstance(item, dict) and isinstance(item.get("bind"), str)
    }
    names = {
        item["name"]
        for item in objects
        if isinstance(item, dict) and isinstance(item.get("name"), str)
    }

    source = BACKEND.read_text(encoding="utf-8")
    action_macros = set(re.findall(r"GSP_ACT_ID_([A-Z0-9_]+)", source))
    object_macros = set(re.findall(r"GSP_OBJ_KEY_([A-Z0-9_]+)", source))

    missing_callbacks = sorted(
        callback
        for callback in callbacks
        if macro_name(callback) not in action_macros
    )
    if missing_callbacks:
        fail(f"callbacks not handled by game_ui.c: {missing_callbacks}")

    scene_actions = {macro_name(callback) for callback in callbacks}
    stale_action_refs = sorted(action_macros - scene_actions)
    if stale_action_refs:
        fail(f"native action IDs absent from scene: {stale_action_refs}")

    scene_objects = {macro_name(value) for value in names | binds}
    stale_object_refs = sorted(object_macros - scene_objects)
    if stale_object_refs:
        fail(f"native object keys absent from scene: {stale_object_refs}")

    generated = ROOT / "build" / "esp-idf" / "main" / "gsp_gen_bundle"
    action_header = generated / "scene0" / "main_actions.h"
    object_header = generated / "scene0" / "main_objects.h"
    generated_is_current = (
        action_header.exists()
        and object_header.exists()
        and action_header.stat().st_mtime >= SCENE.stat().st_mtime
        and object_header.stat().st_mtime >= SCENE.stat().st_mtime
    )
    if generated_is_current:
        generated_actions = set(
            re.findall(
                r"GSP_ACT_ID_([A-Z0-9_]+)",
                action_header.read_text(encoding="utf-8"),
            )
        )
        generated_objects = set(
            re.findall(
                r"GSP_OBJ_KEY_([A-Z0-9_]+)",
                object_header.read_text(encoding="utf-8"),
            )
        )
        missing_generated_actions = sorted(
            macro_name(callback)
            for callback in callbacks
            if macro_name(callback) not in generated_actions
        )
        missing_generated_objects = sorted(object_macros - generated_objects)
        if missing_generated_actions:
            fail(
                "callbacks absent from generated action header: "
                f"{missing_generated_actions}"
            )
        if missing_generated_objects:
            fail(
                "native keys absent from generated object header: "
                f"{missing_generated_objects}"
            )

    print(
        "GSP contract OK: "
        f"{len(callbacks)} callbacks, {len(binds)} binds, {len(names)} names"
    )


if __name__ == "__main__":
    main()
