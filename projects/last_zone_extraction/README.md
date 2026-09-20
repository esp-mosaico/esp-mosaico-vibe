# Last Zone: Extraction

A compact battle-royale-inspired training-ground game for the ESP-Mosaico Game
SDK. Fight through five tactical drills, scavenge supplies, clear the final
hostile, and reach the extraction pad. The native C Host preview and device
firmware share the same fixed-step model and RGB565 view; there is no Wasm
runtime.

```bash
python3 mosaico.py game sim projects/last_zone_extraction
```

Round 480×480 dual-touch loop:

- Tap the briefing overlay to deploy or redeploy.
- Left stick: move. Push the outer ring forward to sprint.
- Right drag: look / pitch. Vertical look is slower and settles after lift.
- Tap fire (or `F` / Ctrl on Host) to shoot or open a facing gate. Hold fire to
  steady the bolt (tighter cone, slower walk). The modern tactical rifle is one
  shot per click, followed by a visible and audible bolt cycle.
- Drag the radar panel to park it away from the current sightline.
- `A/D` turn, `W`/`S` walk, `Shift` sprint, `Q`/`E` strafe.

Each mission now has its own spawn, facing, route, gate position, floor zones,
props, supplies, hostile posts, and extraction point. Dock is the teaching
mission: 5 hostiles, 5 HP, two armor plates, and 20 rounds — enough to miss and
still reach the gold gate. Later missions cut the spare ammo and add elites. Hostiles still take two hits
and will take cover instead of stacking on one point. Medkits stay on the
ground if you are already full. The gold gate must be shot open. Windows punch
through to the sky. Fog, wall-edge shading, and muzzle flash light the
corridor. Radar marks explored cells, windows, crates, doors, loot, extract,
and last-known hostiles. Clear all opponents, then follow the extract
arrow onto the pad. BEST time, hit rate, damage taken, remaining HP/ammo, and
a grade show on the results screen. Redeploy after a win advances the campaign;
redeploy after a death retries the same mission. Walking plays boot steps;
sprinting shortens the stride and can be heard; standing is silent.

The campaign advances through five distinct drills:

- **Dock — observe:** a short teaching route introduces windows, cover, the
  gate, and extraction without putting an enemy in the spawn doorway.
- **Depot — control:** two freight halls and a crate island reward deliberate
  barrel shots; bags are visual freight rather than pickups.
- **Command — flank:** windowed office wings surround a central court, with a
  short exposed approach and a longer route toward armor.
- **Ghost — ambush:** broken two-cell corridors repeatedly cut sightlines;
  walking preserves the first shot while sprinting can alert enemies through walls.
- **Run — assault:** the pad is visible early across an exposed yard, but the
  final elite screen must be cleared before it becomes active.

The extract pad is the cyan `5` cells and moves with each mission. Device NVS
keeps campaign layout, per-mission bests, and unlocks. Death retries the current
mission; extraction advances.

Difficulty rises across the campaign without relying on enemy count alone:
Dock has 5 standard hostiles, Depot 7, Command 8 including 2 elites, Ghost 8
standard hostiles with tighter sight and sound pressure, and Run 9 including
3 elites. Starting ammo is 20 / 18 / 17 / 17 / 16; starting armor is
2 / 2 / 1 / 1 / 0. Enemy aim and recovery also become progressively faster.

Each mission has its own seamless 360-degree horizon and HUD accent:
Dock overlooks a clear coastal port, Depot uses an amber freight-yard sunset,
Command is a cold predawn mountain base, Ghost is a misty overgrown compound,
and Run faces a magenta dusk airfield. Gameplay colors stay fixed: the gold
gate, cyan extract pad, pickups, enemies, weapon, and controls are not washed
by a full-screen grade overlay.

Ammo boxes restore six rounds, medkits restore one HP, and blue armor plates
absorb up to three incoming hits. Barrels detonate when shot and eliminate
hostiles within 2.5 visible map cells; walls now stop blast damage.
