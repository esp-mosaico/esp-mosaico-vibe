# ESP-Mosaico Vibe

[English](README.md) | [中文](README_CN.md)

This repository is the starting point for vibe coding on the development
version of ESP-Mosaico. It gives developers and coding agents a consistent
place to create applications, load board resources, and develop, debug, and
recover devices.

## Start a project

Use [`projects/get-started`](projects/get-started) as the reference project.
Create each new application as its own directory under `projects/`; do not
turn `get-started` into the application itself unless the task explicitly
changes the template.

Component repositories and other project material are provided as Git
submodules. Load or initialize only the submodules required by the current
task. Before implementing a feature, consult [`skills/README.md`](skills/README.md)
and read only the relevant `SKILL.md` guides.

## Development, debugging, and recovery

The default path for routine logs, device control, OTA, crash evidence, and
recovery is the **ESP-Iris Developer Gateway** over USB High-Speed.

- A coding agent operates the device only through the CLI shipped in the
  ESP-Iris component source.
- A developer may keep the Gateway Web workbench open to watch the same logs,
  display output, jobs, restarts, and recovery progress.
- The CLI and Web workbench share the same stable Device ID, Boot ID, and
  structured operation records.
- The Gateway owns the USB session and persists both structured evidence and
  raw logs. Before OTA, preserve any valid core dump.

ESP-Mosaico has one High-Speed USB interface. Both the normal firmware and the
factory-recovery template assign it to ESP-Iris, so the Gateway has exclusive
ownership of the session in either mode.

### Last-resort reflashing

`idf.py flash` is a developer-operated provisioning fallback. Use it only
when no normal or recovery ESP-Iris firmware can be reached. If the device is
in an unrecoverable state and neither normal nor recovery USB is available,
ask the developer to enter ROM download mode and reflash the device:

1. Power off the device.
2. Press and hold the **Boot** button, located to the left of the USB-C port.
3. Power on the device while continuing to hold **Boot**.
4. Run the approved reflashing procedure, then return device operations to
   the ESP-Iris Gateway after ESP-Iris becomes reachable.

Manual ROM download mode is a last-resort recovery strategy, not the routine
development path. Preserve available crash evidence before reflashing and do
not erase the whole flash merely to restore connectivity.

## Repository layout

- `projects/` — developer applications; start from `projects/get-started`.
- `skills/` — task-oriented integration guides for agents and humans. See
  [`skills/README.md`](skills/README.md).
- `docs/` — user-facing documentation.
- `.agents/` — agent-facing documentation and tools.
- `AGENTS.md` — concise routing and operating rules for coding agents.

