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

Use [`projects/hello_world`](projects/hello_world) as the reference application.
Create each new application as its own directory under `projects/`.
[`projects/factory`](projects/factory) contains retained Recovery firmware only
and is not an application template.

Component repositories and other project material are provided as Git
submodules. Load or initialize only the submodules required by the current
task. Before implementing a feature, consult [`skills/README.md`](skills/README.md)
and read only the relevant `SKILL.md` guides.

GSP applications can preview 480×480 scenes on the PC before flashing. Use
[`tools/gsp-sim`](tools/gsp-sim/README.md) with the pinned
**espressif/esp-gsp 1.1.0** submodule (`submodule/esp-gsp`).
Start from [`projects/gsp_hello`](projects/gsp_hello) for a GSP Hello World
that runs in the PC simulator and on the device.

## Unified device commands

Use the repository-level product commands for installation, logs, and recovery:

```sh
python mosaico.py doctor
python mosaico.py list
python mosaico.py recover
python mosaico.py install --project projects/<project>
python mosaico.py monitor
```

The root launcher delegates to the pinned `submodule/esp-mosaico-tools`
checkout; the CLI is not installed into the active Python environment. The
workspace-owned [`.mosaico.json`](.mosaico.json) declares project, Recovery,
BSP, ESP-Iris, and build paths. Initialize the tool checkout with:

```sh
git submodule update --init submodule/esp-mosaico-tools
```

`list` connects to the Gateway and prints Device IDs, online state, connection
type, firmware identity, mode, and Boot ID. It includes cached offline devices;
use `list --details` for endpoint, ESP-IDF version, Session ID, and capabilities,
or `list --json` for the complete Gateway record.

The CLI supports native Linux and macOS shells plus Windows PowerShell and
Command Prompt; WSL and Git Bash are not required. Use Python 3.8 or newer,
an ESP-IDF checkout satisfying the workspace project version constraint, and
the pinned `submodule/esp-iris` checkout. The Gateway environment is prepared
for the active Python major/minor version, and the PEP 508 markers in ESP-Iris's
`components/esp_iris/tools/requirements.lock` select compatible packages automatically. ESP-IDF
6.1 still requires Python 3.10 or newer; when the CLI runs on Python 3.8 or 3.9,
it discovers and delegates ESP-IDF bootstrap commands to a compatible Python
interpreter independently. Set `MOSAICO_IDF_PYTHON` to an explicit compatible
interpreter when automatic discovery is not appropriate. Run
`doctor` first to verify Python, ESP-IDF, ESP32-S31 target support, ESP-Iris,
the host state directory, and current USB discovery. It does not build or
write firmware.

Host state follows platform conventions: `$XDG_STATE_HOME/esp-mosaico` (or
`~/.local/state/esp-mosaico`) on Linux, `~/Library/Application Support/esp-mosaico`
on macOS, and `%LOCALAPPDATA%\esp-mosaico` on Windows.

Run the tool tests and the same host compatibility smoke test from each native host:

```sh
python -m unittest discover -s submodule/esp-mosaico-tools/tests -v
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

Recovery is local-only. It prepares the complete bundle first, then asks the
local Gateway for a maintenance lease on the target device or physical USB
endpoint. This also covers a ROM or pre-HELLO endpoint already held by Gateway.
Only that endpoint is detached; other devices, logs, and operations remain
active. A remote `--gateway-profile` is intentionally not accepted for `recover`. A
running local Gateway must report the same ESP-Iris revision as the pinned
submodule; a mismatch fails without terminating that Gateway.

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

- `projects/hello_world` — reference application for new developer projects.
- `projects/gsp_hello` — GSP Hello World for PC simulation and device installation.
- `projects/factory` — retained Recovery firmware; never install it as a normal application.
- `components/esp_mosaico_app_recovery` — normal-application Recovery entry and health support.
- `submodule/esp-gsp/` — pinned ESP-GSP 1.1.0 (device prebuilts; sim/gspc fetched separately).
- `tools/gsp-sim/` — packs scenes and runs the standalone ESP-GSP `sim`.
- `submodule/esp-iris/` — pinned ESP-Iris firmware component and host runtime.
- `submodule/esp-mosaico-tools/` — pinned repository-local implementation of
  `mosaico.py`; no global CLI installation is required.
- `skills/` — task-oriented integration guides for agents and humans. See
  [`skills/README.md`](skills/README.md).
- `docs/` — user-facing documentation.
- `.mosaico.json` — workspace paths and supported-device configuration consumed
  by the tool submodule.
- `.agents/` — private agent-facing documentation and tools, not product CLI code.
- `AGENTS.md` — concise routing and operating rules for coding agents.
