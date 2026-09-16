# Game performance benchmark

Device performance is measured from a fixed 60-second Sky Hop run. Keep the
LCD at the supported 40 MHz QSPI clock and record the framebuffer count, TE
compose count and draw-buffer line count with every result.

Set `CONFIG_SKY_HOP_BENCHMARK_MODE=y`; the firmware then starts each phase and
drives movement/jumps from the same fixed tick sequence without external input.
Select `CONFIG_MOSAICO_GAME_FRAMEBUFFER_COUNT`,
`CONFIG_SKY_HOP_TE_COMPOSE_BUFFERS`, and `CONFIG_SKY_HOP_DRAWBUF_LINES` for each
case. Build and install only through `mosaico.py`, and retain the ESP-Iris monitor log. Summarize
one or more logs with:

```sh
python submodule/raylib-lite-engine/tools/analyze_game_perf.py --label fb3-te1-lines34 raw.log
```

The acceptance gate is logic `30.0 +/- 0.5 Hz`, acquire p95 below 1000 us, no
display errors, and displayed FPS no worse than the 24 FPS baseline. Busy or
superseded frames are allowed because the runtime intentionally keeps the
latest game state instead of blocking logic on LCD throughput.

The standard comparison matrix is framebuffer count 2/3/4, TE compose 1/2 and
draw-buffer lines 10/34. A result is valid only when the device ID, boot ID,
firmware hash, configuration and raw log are retained together.

Generate the complete machine-readable matrix with:

```sh
python submodule/raylib-lite-engine/tools/game_benchmark_matrix.py --output benchmark-matrix.json
```
