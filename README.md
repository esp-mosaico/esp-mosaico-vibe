# ESP-Mosaico Vibe

[English](README.md) | [中文](README_CN.md)

This repository is the **Agent-led human-Agent collaborative development entry
point purpose-built for ESP-Mosaico**. It gives the Agent a unified engineering
workspace and device channel.

The user defines the goal and accepts the physical result. The Agent is the
default executor and advances the task through on-device validation. It asks
the user to intervene when authorization, physical action, or a high-risk
change is required.

## Start a project

Use [`projects/factory`](projects/factory) as the reference project.
Create each new application as its own directory under `projects/`; do not
turn `factory` into the application itself unless the task explicitly
changes the template.

Component repositories and other project material are provided as Git
submodules. Load or initialize only the submodules required by the current
task. Before implementing a feature, consult [`skills/README.md`](skills/README.md)
and read only the relevant `SKILL.md` guides.

## Unified device commands

Use the repository-level product commands for installation, logs, and recovery:

```sh
python mosaico.py doctor
python mosaico.py list
python mosaico.py recover
python mosaico.py install --project projects/<project>
python mosaico.py monitor
```

The CLI supports native Linux and macOS shells plus Windows PowerShell and
Command Prompt; WSL and Git Bash are not required. Use Python 3.11 or newer,
an ESP-IDF checkout satisfying the factory project's version constraint, and
an ESP-Iris environment installed from its `tools/requirements.txt`. Run
`doctor` first to verify Python, ESP-IDF, ESP32-S31 target support, ESP-Iris,
the host state directory, and current USB discovery. It does not build or
write firmware.

Host state follows platform conventions: `$XDG_STATE_HOME/esp-mosaico` (or
`~/.local/state/esp-mosaico`) on Linux, `~/Library/Application Support/esp-mosaico`
on macOS, and `%LOCALAPPDATA%\esp-mosaico` on Windows.

Run the same host compatibility smoke test from each native host:

```sh
python -m unittest discover -s tests/mosaico_cli -v
python mosaico.py --version
python mosaico.py --json list
python mosaico.py --json doctor
python mosaico.py monitor --timeout 1 --grep __mosaico_host_smoke__
```

`install` updates normal applications only through the **ESP-Iris Developer
Gateway**. An uninitialized device is told to run `recover`; the command never
silently falls back to a lower-level write. `recover` uses the reviewed bundle
by default and leaves the device Recovery-ready.

- A coding agent operates the device only through `mosaico.py`.
- A developer may keep the Gateway Web workbench open to watch the same logs,
  display output, jobs, restarts, and recovery progress.
- The CLI and Web workbench share the same stable Device ID, Boot ID, and
  structured operation records.
- The Gateway owns the USB session and persists both structured evidence and
  raw logs. Before OTA, preserve any valid core dump.

ESP-Mosaico has one High-Speed USB interface. Both normal firmware and
Recovery assign it to ESP-Iris, so the Gateway has exclusive
ownership of the session in either mode.

### Last-resort recovery

When neither normal nor Recovery firmware can be reached, continue to use
`python mosaico.py recover`. It preserves accessible evidence and asks the
developer for the required physical steps when necessary:

1. Power off the device.
2. Press and hold the **Boot** button, located to the left of the USB-C port.
3. Power on the device while continuing to hold **Boot**.
4. Release **Boot** after the device enters ROM download mode, then tell the
   agent that the physical sequence is complete.

The developer is responsible only for those button and power operations. The
agent then continues `recover` and verifies device identity, Recovery version,
and readiness. The normal application is installed later with `install`.

Manual ROM download mode is a last-resort recovery strategy, not the routine
development path. Do not erase the whole flash merely to restore connectivity,
or overwrite credentials, device identity, recovery data, or related
partitions without explicit user authorization.

## Repository layout

- `projects/` — developer applications; start from `projects/factory`.
- `skills/` — task-oriented integration guides for agents and humans. See
  [`skills/README.md`](skills/README.md).
- `docs/` — user-facing documentation.
- `tools/mosaico_cli/` — public product-command implementation for `mosaico.py`.
- `.agents/` — private agent-facing documentation and tools, not product CLI code.
- `AGENTS.md` — concise routing and operating rules for coding agents.
