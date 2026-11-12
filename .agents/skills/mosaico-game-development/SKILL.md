---
name: mosaico-game-development
description: Develop Mosaico games with Raylib Lite Engine as the preferred engine, validate visuals and gameplay in the simulator first, then follow the recovery-safe device workflow.
---

# Mosaico games

Read the [workspace entry](../../../docs/game-development_CN.md). Firmware uses
Registry component `espressif2022/raylib-lite-engine` pinned to `0.1.1`; default
manifests must not contain a local source override. Initialize utils and BSP,
then run `python mosaico.py game prepare` to verify/cache the matching Host,
and blank-game Board profiles. For released examples, run
`python mosaico.py game prepare --example last_zone_extraction --json`; this uses
`idf.py create-project-from-example` at the pinned Registry version and returns
the local source path. First download needs the SDK; cached copies are reusable. Read `docs/game-development.EN.md` in the returned
toolchain directory. A developer can explicitly configure `dependencies.raylib`
for local tools and add an application `override_path` when modifying the runtime.
Start a new game with
`python mosaico.py game create <name>` (the tools-owned `blank` template).
It provides shared C state/update/rendering and Host/device adapters without
example gameplay or assets. To extend a complete reference game, explicitly
select `--template sky-hop`, `--template tower-defense`, `--template shooter` or
`--template last-zone` (also `living-worlds` and `neon-rift-rally`).
User applications go under projects/; complete reference games, behavior replays,
runtime, renderers and implementation tests belong to Raylib Lite Engine.
Native example Board adapters use Board Manager; inspect their dependencies
separately from the product BSP.

Use `python mosaico.py game sim --project projects/<name>` and headless replay
for the shared C model and RGB565 view before device installation. Inspect
visuals, animation, input feedback and complete gameplay flows interactively;
use recorded input and headless replay for repeatable checks of relevant
collisions, scoring, win/loss and restart behavior. Fix and re-run affected
flows in the simulator before device validation. A successful headless launch
alone does not establish that the game looks or plays correctly.
Never implement a separate Python or browser renderer.
Generate Board Manager configuration in the effective build project as described
in the workspace guide, including the generated wrapper for an Iris target. The
current builder does not generate `gen_bmgr_codes` automatically.
Build the integrated blank template through `python mosaico.py game build --project ...`.
For a copied Engine example or a native game, use `game build --project ... --target iris`,
then install the emitted System Update bundle through the product CLI. The default
native build does not add Iris or the retained layout. The wrapper uses component
CMake files and a separate generated project, not the source's top-level partition table;
inspect real component APIs and preserve the retained Recovery contract.
Keep OTA writer only in Vibe Mode and mark healthy after the first successful frame.

Run the affected engine runtime/game behavior tests and generated-app builds.
Keep sprites, audio sources and license information with the game. Follow the
[CLI device workflow](../../../docs/mosaico-cli.md) for installation, diagnostics
and acceptance. Validate physical controls, display,
audio and device performance on hardware; do not claim hardware behavior from Host evidence.
