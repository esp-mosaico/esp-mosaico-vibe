---
name: mosaico-game-development
description: Create and validate Mosaico games from BSP examples using the pinned Raylib Lite Engine and recovery-safe device workflow.
---

# Mosaico games

Read the [workspace entry](../../../docs/game-development.zh-CN.md) and
[BSP game guide](../../../submodule/esp-mosaico-bsp/docs/game-development.zh-CN.md).
Initialize utils, BSP and Raylib Lite Engine. Create with
`python mosaico.py game create <name> --template sky-hop|tower-defense|shooter`.
User applications go under projects/; complete reference games and behavior
replays belong to BSP examples/tests. Engine runtime, renderers and implementation
tests stay in Raylib Lite Engine.

Use `python mosaico.py game sim --project projects/<name>` and headless replay
for the shared C model and RGB565 view. Never implement a separate Python or
browser renderer. Build through `python mosaico.py game build --project ...`;
inspect real component APIs and preserve the retained Recovery contract.
Keep OTA writer only in Recovery and mark healthy after the first successful frame.

Run the affected engine tests, BSP game behavior tests, and generated-app builds.
Keep sprites, audio sources and license information with the game. Device work
uses only the workspace CLI and verifies the same live Device ID, new Boot IDs,
Recovery ready and healthy normal firmware. Do not borrow direct IDF flashing
from other BSP examples; do not claim hardware behavior from Host evidence.
