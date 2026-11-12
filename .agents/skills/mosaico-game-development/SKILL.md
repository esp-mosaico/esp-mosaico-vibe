---
name: mosaico-game-development
description: Develop Mosaico games with Raylib Lite Engine, validate shared gameplay in the simulator, and integrate with the retained firmware workflow.
---

# Mosaico games

Use the [workspace game guide](../../../docs/game-development_CN.md) as the
single entry for creation, pinned Engine technical references, simulation,
Board Manager setup and product builds. It applies to Registry `0.1.1` without
requiring a component update. Keep ordinary app manifests pinned; local Engine
overrides are an explicit development choice.

User games belong in `projects/`; runtime and reusable reference games belong
to the Engine. Use shared C gameplay/rendering on Host and device, never a
separate Python or browser renderer. Inspect real APIs before implementation.

Confirm unsettled visuals and interactions before implementing them. Exercise
complete gameplay and actual browser input before device delivery; headless
success alone is insufficient. Follow the guide's preview troubleshooting and
hardware coverage notes when those paths apply.

For inherited brightness, Wi-Fi or language, follow
[settings reuse](../../../docs/vibe-settings.md). Product integration must
preserve the [application contract](https://github.com/esp-mosaico/esp-mosaico-utils/blob/cf46fbe2d2601b6569360611e07da8a22bb2e83b/mosaico-tools/docs/application-integration.md).
Install through the [product CLI](../../../docs/mosaico-cli.md); keep the OTA
writer in Vibe Mode and mark healthy after the first successful frame.
