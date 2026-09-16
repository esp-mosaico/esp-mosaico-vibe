---
name: esp-iris-device-debugging
description: Diagnose ESP-Mosaico devices through the managed ESP-Iris Gateway, including disappearing or cached devices, boot loops, panics, missing or mismatched core dumps, failed OTA/system updates, Recovery fallback, and Device ID/Boot ID correlation. Use for device-side failures and evidence capture; do not use for ordinary compile errors or direct serial/esptool workflows.
---

# ESP-Iris Device Debugging

Diagnose the device as a sequence of boots and managed Gateway sessions. Keep the Device ID constant across the timeline; use Boot ID, firmware mode, project/version, firmware SHA, reset reason, and operation ID to distinguish each state.

## Preserve evidence first

Before install, recovery, or another reboot:

1. Run `python mosaico.py list` and record only a device with `connected=true`; cached entries are history, not proof of availability.
2. Run `python mosaico.py crash --device <device-id>` while the affected firmware or its Recovery fallback is reachable.
3. Preserve the command run directory under `.codex-runs/mosaico/`, especially `raw.log`, `crash-index.json`, and `core-dump.bin` when present.
4. Record the candidate application ELF and its firmware SHA before rebuilding it.

For a reproducible early-boot failure, start `python mosaico.py monitor --device <device-id> --timeout <seconds>` before triggering the reboot or install. Keep the unfiltered `raw.log`; use `--grep` only as a viewing aid. A later `--snapshot` is not a substitute for a live capture of a short-lived normal session.

Read [references/crash-evidence.md](references/crash-evidence.md) for panic and core-dump interpretation. Read [references/update-recovery.md](references/update-recovery.md) for OTA, system-update, and Recovery failures.

## Reconstruct the state transition

Build a compact timeline such as:

```text
Device ID D, Boot A normal -> update operation O -> session closes
Device ID D, Boot B normal -> panic before healthy mark
Device ID D, Boot C Recovery -> previous-boot crash evidence
```

Treat these as separate facts:

- USB enumeration means a transport exists; it does not prove a healthy ESP-Iris session.
- `connected=false`, even with `stale=false`, is not a live device.
- `ESP-Iris session closed` means the operation lost its session; it is neither success nor proof that flash contents remained intact.
- Reaching Recovery proves recoverability, not successful application deployment.
- A new Boot ID proves a new boot, not the intended firmware or healthy behavior.

After any interrupted update, query live identity again and verify the intended mode, project/version, firmware SHA, and system inventory before choosing the next write.

## Apply a bounded retry policy

Retry once only when reconnection and live state suggest a transient transport loss. If the same operation fails twice at the same phase, stop retrying and preserve evidence. Switch to diagnosis or a verified safe fallback; repeated writes can erase the best evidence and leave data partitions absent or partial.

Use only `python mosaico.py list`, `crash`, `monitor`, `recover`, and `install` for routine device work. Never open USB serial directly while the Gateway owns it, and never use raw ESP-IDF, ESP-Iris, or esptool write commands.

## Finish with behavioral proof

For recovery or deployment, verify the same Device ID completes the intended mode transition with new Boot IDs, then confirm project/version, firmware SHA, healthy state, logs, and visible product behavior. Do not claim success from upload completion, reconnect, or a reachable Recovery service alone.
