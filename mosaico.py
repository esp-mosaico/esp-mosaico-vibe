#!/usr/bin/env python3
"""Repository-local launcher for the pinned ESP-Mosaico utilities submodule."""

from __future__ import annotations

from pathlib import Path
import sys


REPOSITORY_ROOT = Path(__file__).resolve().parent
UTILS_ROOT = REPOSITORY_ROOT / "submodule" / "esp-mosaico-utils"
TOOLS_ROOT = UTILS_ROOT / "mosaico-tools"
PACKAGE_ROOT = TOOLS_ROOT / "tools"

def command_index(arguments):
    """Locate the root command without interpreting option values as commands."""
    index = 0
    while index < len(arguments):
        token = arguments[index]
        if token == "--workspace":
            index += 2
            continue
        if not token.startswith("-"):
            return index
        index += 1
    return None


root_command_index = command_index(sys.argv[1:])

# Game development is owned by this workspace because it compiles project
# sources into the local RGB565 simulator. Device and recovery commands remain
# pinned to esp-mosaico-utils below.
if root_command_index is not None and sys.argv[root_command_index + 1] == "game":
    game_index = root_command_index + 1
    sys.path.insert(0, str(REPOSITORY_ROOT / "submodule" / "raylib-lite-engine" / "tools"))
    from game_cli import main as game_main  # noqa: E402

    raise SystemExit(game_main(
        sys.argv[game_index + 1:],
        repository=REPOSITORY_ROOT,
        tool_root=TOOLS_ROOT,
        global_args=sys.argv[1:game_index],
    ))

if not (PACKAGE_ROOT / "mosaico_cli" / "cli.py").is_file():
    print(
        "mosaico: the esp-mosaico-utils submodule is unavailable; run "
        "'git submodule update --init submodule/esp-mosaico-utils'.",
        file=sys.stderr,
    )
    raise SystemExit(3)
sys.path.insert(0, str(PACKAGE_ROOT))

from mosaico_cli.cli import main  # noqa: E402


if __name__ == "__main__":
    try:
        raise SystemExit(main(tool_root=TOOLS_ROOT))
    except SystemExit as error:
        if error.code == 0 and root_command_index is None and any(value in {"-h", "--help"} for value in sys.argv[1:]):
            print("\nGame development:\n  game                Create, simulate, or build a Raylib game")
        raise
