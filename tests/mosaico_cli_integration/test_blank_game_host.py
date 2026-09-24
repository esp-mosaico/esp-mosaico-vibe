"""Exercise the generated template against the pinned engine's native C Host."""
import ctypes
import importlib.util
import io
import json
from pathlib import Path
import shutil
import subprocess
import sys

import pytest

ROOT = Path(__file__).resolve().parents[2]
ENGINE = ROOT / "submodule/raylib-lite-engine"


@pytest.fixture
def game(tmp_path):
    if not (ENGINE / "host/run_game.py").is_file():
        pytest.skip("initialize Raylib Lite Engine for native Host integration")
    if not any(shutil.which(name) for name in ("cc", "gcc", "clang")):
        pytest.skip("native Host integration needs a C compiler")
    pytest.importorskip("PIL")
    spec = importlib.util.spec_from_file_location("blank_game_host", ENGINE / "host/run_game.py")
    host = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(host)
    config = json.loads((ROOT / ".mosaico.json").read_text())
    config["dependencies"] = {key: str((ROOT / value).resolve())
                              for key, value in config["dependencies"].items()}
    (tmp_path / ".mosaico.json").write_text(json.dumps(config))
    generated = subprocess.run([sys.executable, str(ROOT / "mosaico.py"),
        "--workspace", str(tmp_path), "game", "create", "snake_seed", "--json"],
        capture_output=True, text=True)
    assert generated.returncode == 0, generated.stdout + generated.stderr
    project = Path(json.loads(generated.stdout)["project"])
    runtime = host.GenericHostRuntime(project, tmp_path)
    try:
        yield runtime
    finally:
        runtime.close()


def test_generated_blank_game_renders_and_handles_input(game):
    from PIL import Image
    state = game.metadata()
    assert (state["game_id"], state["title"]) == ("snake_seed", "snake_seed")
    initial_hash = state["state_hash"]
    frame = Image.open(io.BytesIO(game.frame())).convert("RGB")
    assert frame.size == (480, 480)
    assert frame.getbbox() is None  # Blank means no leftover example graphics.

    game.pointer(0, 120, 240, True)
    state = game.metadata()
    assert (state["pointer_x"], state["pointer_y"], state["pointer_down"]) == (120, 240, True)
    game.pointer(1, 400, 400, True)  # The blank template declares one pointer.
    assert game.metadata()["pointer_x"] == 120
    game.pointer(0, -10, 900, False)
    state = game.metadata()
    assert (state["pointer_x"], state["pointer_y"], state["pointer_down"]) == (0, 479, False)
    for _ in range(10):
        game.step(False, False, False)
    assert game.metadata()["tick"] == 10
    game.control(1)  # pause
    game.step(False, False, False)
    assert (game.metadata()["phase"], game.metadata()["tick"]) == ("paused", 10)
    game.control(2)  # resume
    game.step(False, False, False)
    assert (game.metadata()["phase"], game.metadata()["tick"]) == ("running", 11)
    game.control(3)  # reset restores pointer, clock and pause state
    assert game.metadata()["state_hash"] == initial_hash
    assert game.metadata()["tick"] == 0
    game.action(3, True)  # Host keyboard pause action
    assert game.metadata()["phase"] == "paused"
    game.action(4, True)  # Host keyboard restart action
    assert game.metadata()["state_hash"] == initial_hash
    short = ctypes.create_string_buffer(8)
    assert game.api.mosaico_host_game_state_json_v1(game.context, short, len(short)) < 0
