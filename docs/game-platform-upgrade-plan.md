# Game Platform upgrade plan

## Outcome

Turn the current focused embedded game runtime into a repeatable 2D production
platform without sacrificing fixed memory, deterministic Host tests, RGB565
performance, or the Recovery-first install path.

## Milestone 1 — gameplay foundation

Deliver first because every game benefits and the device cost is bounded.

- Camera2D world/screen transforms and camera-aware fast drawing.
- Common rectangle, circle, and point collision queries; add segment and swept
  tile collision after profiling real games.
- Action-based input mapping for touch, button, joystick, keyboard, and IMU.
- Replace the single-pointer touch bridge with two-contact CST92xx events that
  preserve track IDs and down/move/up lifecycle; retain mouse as a primary-point
  compatibility view.
- Versioned NVS save/config helper with deferred writes.
- Minimal scene state, Tween helpers, and fixed-capacity particle pools.

Acceptance: Sky Hop uses world coordinates, all input sources share actions,
move plus jump works with two fingers without contact swaps or stuck releases,
pause/restart are deterministic, best score survives reboot, and Host tests plus
an ESP-IDF build pass.

## Milestone 2 — content iteration

- Upgrade the Host preview to a real-time browser loop with touch, keyboard,
  gamepad and IMU controls.
- Add pause, step, speed, metrics, screenshot and deterministic replay panels.
- Watch and hot-reload manifests, maps, Atlas images, UI layouts, text and tuning
  data on Host; never hot-swap live device pointers in this milestone.
- Add a visual level inspector for object layers, collision, paths, spawn points
  and safe-area overlays. Keep Tiled as the authoritative level editor first.

Acceptance: an asset or level edit is visible in under two seconds without an
ESP-IDF rebuild, and the same replay produces the same state hash.

## Milestone 3 — product UI and text

- Retained UI primitives: panel, label, button, slider, dialog, list and focus.
- UTF-8 shaping subset, build-time glyph collection, font fallback and bounded
  glyph Atlas pages for Chinese and multiple fonts.
- Reusable scene stack, transition library, Tween timelines and configurable
  particle emitters.
- General save/config component with schema migration, CRC, write coalescing and
  reset/import/export diagnostics.

Acceptance: a Chinese settings/menu flow fits declared RAM/flash budgets and
can be operated by touch and directional actions.

## Milestone 4 — compatibility and tooling

- Expand the optimized Raylib 2D surface by usage frequency: lines/circles,
  polygons, camera, collision, text/font, render textures, then selected audio.
- Maintain a machine-readable supported/emulated/unsupported API matrix. Do not
  pull the generic OpenGL path onto the device to claim compatibility.
- Add device-assisted resource refresh only after asset lifetime and rollback
  semantics are specified; retain `mosaico.py install` for firmware deployment.
- Build a visual scene/level tool only for gaps that Tiled plus the browser
  inspector cannot cover.

Acceptance: selected upstream Raylib 2D examples compile unchanged, performance
budgets are enforced in CI, and unsupported APIs fail clearly at build time.

## Efficiency targets

| Workflow | Current | Target |
| --- | --- | --- |
| Gameplay code-to-feedback | rebuild/run or limited Host replay | interactive preview under 2 s |
| New input source | game-specific handling | one platform mapping, zero gameplay changes |
| New menu/transition | bespoke code | reusable scene/UI/Tween composition |
| Level/resource tuning | rebuild assets and application | Host hot reload under 2 s |
| New small 2D prototype | several days to weeks | playable vertical slice in 1–3 days |

Each milestone must report frame time, PSRAM/internal RAM, flash/resource size,
test determinism and device behavior. A feature is not complete merely because
the desktop implementation works.
