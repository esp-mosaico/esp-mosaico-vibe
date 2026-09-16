# Content pipeline

## Source and generated assets

Put editable inputs and deterministic generators in `projects/<game>/assets_src`. Put build outputs in `projects/<game>/assets/generated`. The device must consume only generated `.atlas`, `.map`, and `.sound` files through `esp_mmap_assets`.

`submodule/raylib-lite-engine/tools/pack_game_assets.py` consumes a `mosaico-game-assets/v1`
`game_assets.json` manifest containing only the content a project uses:

- `atlases`: source image, frame config and output filename, or an explicit generator;
- `maps`: finite orthogonal Tiled source and output filename;
- `sounds`: PCM WAV source or glob and optional output filename.

It emits the declared files plus `assets_ids.h`, a report, and a deterministic digest.
Do not create dummy maps or keep project-specific output names in shared tooling.

## Atlas decisions

Use `binary` alpha for crisp pixel art and `smooth` A8 for soft edges. Each manifest frame corresponds to one uniform grid cell. Trim excessive cell whitespace before packing or the visible sprite will become too small after `output_cell` scaling. Inspect the processed atlas and a real device screenshot.

Reference frames through generated `MOSAICO_ASSET_ID_*` symbols and use `MosaicoAnimationFrameAt` for tick-based animation. Never duplicate numeric hashes by hand.

When generating art, require an original design and avoid recognizable copyrighted characters, logos, music, or level layouts. Keep the final project-consumed source in the workspace.

## Audio decisions

The packer normalizes WAV to 24 kHz mono. Clips shorter than one second remain PCM16; longer clips use deterministic IMA-ADPCM. Keep BGM short enough to fit the 1 MiB resource budget and design the loop boundary intentionally.

Load sound objects once after mounting assets; never load during update. Trigger cues from state transitions captured around `game_update`, not from rendering. Use music volume low enough that event cues remain clear.

Before considering audio verified, confirm codec initialization on device and exercise jump/action, reward, damage, win/game-over and BGM paths relevant to the game.
