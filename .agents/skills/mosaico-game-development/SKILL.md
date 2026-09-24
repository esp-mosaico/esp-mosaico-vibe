---
name: mosaico-game-development
description: Develop Mosaico games with Raylib Lite Engine as the preferred engine, validate visuals and gameplay in the simulator first, then follow the recovery-safe device workflow.
---

# Mosaico games

Read the [workspace entry](../../../docs/game-development_CN.md) and
[BSP game guide](../../../submodule/esp-mosaico-bsp/docs/game-development.zh-CN.md).
Prefer the workspace's pinned Raylib Lite Engine for game applications.
Initialize utils, BSP and Raylib Lite Engine. Start a new game with
`python mosaico.py game create <name>` (the tools-owned `blank` template).
It provides shared C state/update/rendering and Host/device adapters without
example gameplay or assets. To extend a complete reference game, explicitly
select `--template sky-hop`, `--template tower-defense` or `--template shooter`.
User applications go under projects/; complete reference games and behavior
replays belong to BSP examples/tests. Engine runtime, renderers and implementation
tests stay in Raylib Lite Engine.

Use `python mosaico.py game sim --project projects/<name>` and headless replay
for the shared C model and RGB565 view before device installation. Inspect
visuals, animation, input feedback and complete gameplay flows interactively;
use recorded input and headless replay for repeatable checks of relevant
collisions, scoring, win/loss and restart behavior. Fix and re-run affected
flows in the simulator before device validation. A successful headless launch
alone does not establish that the game looks or plays correctly.
Never implement a separate Python or browser renderer.
Build through `python mosaico.py game build --project ...`;
inspect real component APIs and preserve the retained Recovery contract.
Keep OTA writer only in Recovery and mark healthy after the first successful frame.

Run the affected engine tests, BSP game behavior tests, and generated-app builds.
Keep sprites, audio sources and license information with the game. Follow the
[CLI device workflow](../../../docs/mosaico-cli.md) for installation, diagnostics
and acceptance. Validate physical controls, display,
audio and device performance on hardware; do not claim hardware behavior from Host evidence.
