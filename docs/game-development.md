# Game creation and simulation

[简体中文](game-development_CN.md) | [Documentation index](README.md)

Prefer **Raylib Lite Engine** (`submodule/raylib-lite-engine`) for game applications.
Validate visuals and gameplay in the simulator, fix problems, then install on the
device. BSP `examples/` maintains the three complete games; the engine maintains
the shared runtime, rendering, asset tools and Host simulation. BSP examples can
be built from standalone clones: each example declares its pinned dependencies
and fetches them automatically.

```sh
git submodule update --init submodule/esp-mosaico-utils submodule/esp-mosaico-bsp submodule/raylib-lite-engine
python mosaico.py game create my_game --template sky-hop
python mosaico.py game sim --project projects/my_game
python mosaico.py game sim --project projects/my_game --headless --frames 120
python mosaico.py game build --project projects/my_game
python mosaico.py iris system-update --project projects/my_game
```

Available templates are `sky-hop`, `tower-defense` and `shooter`. `game new` is
equivalent to `game create`. Creation supports `--dry-run` and refuses to overwrite
existing targets. The Host requires a C compiler and Pillow; firmware requires an
ESP-IDF version that satisfies the project's constraints and supports ESP32-S31.
Omit `--headless` for interactive simulation.

Interactive simulation uses the C gameplay model and rendering code shared with
the device. Check visuals, animation, input feedback and complete gameplay flows,
including collisions, scoring, win/loss conditions and restart behavior as relevant
to the game. Recorded input can be replayed headlessly for repeatable checks.
Starting successfully or running for a fixed number of frames does not prove that
the game looks or plays correctly. Inspect the visuals and input results, and
re-run affected flows after fixes.

On the device, validate physical buttons, touch, display, audio and actual
performance. Host simulation cannot replace these hardware checks.

- [Sky Hop](../submodule/esp-mosaico-bsp/examples/sky_hop/README.md)
- [Tower Defense](../submodule/esp-mosaico-bsp/examples/tower_defense/README.md)
- [Raylib Shooter](../submodule/esp-mosaico-bsp/examples/raylib_shooter/README.md)
- [Detailed game development guide (Chinese)](../submodule/esp-mosaico-bsp/docs/game-development.zh-CN.md)
- [Engine interfaces and Host](../submodule/raylib-lite-engine/README.md)
