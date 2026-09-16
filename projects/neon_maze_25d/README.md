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
A west alley leads to an optional cache; the east hall is the main push. You
start with 5 HP and 14 rounds. Hostiles still take two hits and will take cover
instead of stacking on one point. Medkits stay on the ground if you are already
full. The gold gate must be shot open. Fog, wall-edge shading, and muzzle flash
light the corridor. Radar marks explored cells, windows, crates, doors, loot,
extract, and last-known hostiles. Clear all ten opponents, then follow the
extract arrow into the cyan pad. BEST time, hit rate, damage taken, remaining
HP/ammo, and a grade show on the results screen. Redeploy after a win or death
rotates the hostile and loot posts across three shifts. Walking plays boot
steps; sprinting shortens the stride; standing is silent.
