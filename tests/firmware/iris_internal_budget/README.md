# Iris internal RAM acceptance fixture

Test-only firmware using the real `projects/hello_world/main` UI and normal
partition/recovery contract. Normal applications do not compile `budget.c`.

Build with the workspace low-noise IDF build runner and a compatible ESP32-S31
ESP-IDF environment, then install only through the product workflow:

```sh
python mosaico.py iris app-update --project tests/firmware/iris_internal_budget --skip-build
```

Discover the live Device ID with `python mosaico.py iris list`. Run `run_test.py`
using the prepared ESP-Iris host Python (with aiohttp), with the same
`MOSAICO_LOCAL_GATEWAY_URL` as `mosaico.py` when a non-default Gateway is used:

```sh
python tests/firmware/iris_internal_budget/run_test.py --device-id DEVICE_ID
```

The script uses only the existing managed Gateway HTTP/WebSocket API. It tests
1024-byte echo RPCs, built-in recovery-state/system-inventory RPCs, standalone screenshots, concurrent RPC/screenshot during
mirroring, complete 480x480 RGB565 frames, unchanged Boot ID, and error counters.
JSON results, operation IDs, PNGs and raw responses are retained under
`.codex-runs/mosaico/*-iris-budget-test/`. A failed request is not a passing run.
Restore normal GSP firmware afterwards with `mosaico.py iris app-update`.

Metrics also report the named TinyUSB task's minimum free stack; acceptance
requires at least 512 B. A test-only timer allocated before attribution emits
`iris_budget: EXIT current=... peak=... errors=...` after the enter-Recovery
task is created, before reset. Use managed `mosaico.py iris logs` to preserve
that record during the final normal -> Recovery -> normal installation.

After restoring production firmware, `--production-check --rpc-count 10
--screenshots 1 --seconds 5` checks its real state RPC/screenshot/mirror behavior
without calling diagnostic RPCs. Such a report explicitly sets
`budget_measured=false` and does not claim a production heap measurement.

Internal peak = charged internal heap high-water mark + admitted static DRAM +
resident IRAM. Allocation hooks charge actual internal addresses, allocator
rounding and a conservative 16 B/block overhead. They attribute Iris, service,
TinyUSB and tcpip tasks, bootstrap/recovery registration scopes, plus all ISR
allocations. Stacks and task control blocks are counted at creation, including
the normal application's recovery health task. Free hooks run regardless of
which task frees a tracked block. Trace errors invalidate a result.

`map_report.py` charges the complete Iris/TinyUSB/USB HAL/PHY linked input
sections, linked network state, recovery adapter and screen backend. Shared
OS/NVS implementation statics and the test-only hook/table are excluded;
bootstrap NVS heap allocations are included. Diagnostic RPC registration and
capability differences conservatively enlarge the fixture's footprint versus
normal GSP. This is a bounded workload measurement, not a bound on arbitrary
future RPC callbacks. Actual network connections and Recovery's writer runtime
are outside this acceptance scope.

Static input sections are rounded up to their product 4 B alignment. The
final pressure JSON captured before this conservative ledger update remains
unchanged; see the user-facing report for the additional 75 B charge.

PSRAM buffers use explicit SPIRAM capabilities without internal fallback.
Normal PSRAM log storage can lose crash-adjacent logs while flash cache is
disabled; Recovery retains its independent internal log profile.
