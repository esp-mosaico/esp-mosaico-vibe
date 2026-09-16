# Neon Maze 2.5D

Native C raycasting tactical game for the ESP-Mosaico Game SDK. The Host preview
and device firmware share the same fixed-step model and RGB565 view. There is no
Wasm runtime.

```bash
python3 mosaico.py game sim projects/neon_maze_25d
```

- Fire or tap anywhere on the briefing overlay to start or redeploy.
- `A/D` or Left/Right: turn
- `W`/Up/Space: move forward; `S`/Down: move backward
- `F`/Ctrl: fire
- Drag the left joystick in any direction to move.
- Drag the right side of the scene to turn the view.
- Touch the crosshair button on the right to fire.

Walk next to gold doors to open them. Eliminate all ten opponents, then step
into the cyan extract in the far corner. Hostiles melee at close range; three
hits will down you.

Walls are lit RGB565 columns and the ground is a textured floor span. Enemy
slices are depth-tested against wall columns.
