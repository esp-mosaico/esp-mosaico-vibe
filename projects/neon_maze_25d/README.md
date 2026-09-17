# Neon Maze 2.5D

Native C raycasting tactical game for the ESP-Mosaico Game SDK. The Host preview
and device firmware share the same fixed-step model and RGB565 view. There is no
Wasm runtime.

```bash
python3 mosaico.py game sim projects/neon_maze_25d
```

Round 480×480 dual-touch loop:

- Tap the briefing overlay to deploy or redeploy.
- Left stick: move. Push the outer ring forward to sprint.
- Right drag: look / pitch. Vertical look is slower and settles after lift.
- Tap fire (or `F` / Ctrl on Host) to shoot or open a facing gate. The rifle is
  bolt-action: one shot per click, then wait for the bolt.
- `A/D` turn, `W`/`S` walk, `Shift` sprint, `Q`/`E` strafe.

Turn the corner out of the spawn bay — the first hostile is not in the doorway.
A west alley leads to an optional cache; the east hall is the main push. South
of the gold gate is a warehouse, then a wider extract compound with a cyan
ground pad. You start with 5 HP and 14 rounds. Hostiles still take two hits
and will take cover instead of stacking on one point. Medkits stay on the
ground if you are already full. The gold gate must be shot open. Windows punch
through to the sky. Fog, wall-edge shading, and muzzle flash light the
corridor. Radar marks explored cells, windows, crates, doors, loot, extract,
and last-known hostiles. Clear all opponents, then follow the extract
arrow onto the pad. BEST time, hit rate, damage taken, remaining HP/ammo, and
a grade show on the results screen. Redeploy after a win advances the campaign;
redeploy after a death retries the same mission. Walking plays boot steps;
sprinting shortens the stride; standing is silent.

The campaign now advances through three missions with separate map geometry,
cover, enemy posts, supplies, and difficulty. Dock starts with eight hostiles
and extra ammunition; Depot introduces ten hostiles and armor; Command starts
with twelve rounds and adds three armored elites. Death retries the current
mission, while a successful extraction advances to the next mission.

Ammo boxes restore six rounds, medkits restore one HP, and blue armor plates
absorb up to three incoming hits. Barrels detonate when shot and eliminate
hostiles within 2.5 map cells, allowing positioning to replace several shots.
