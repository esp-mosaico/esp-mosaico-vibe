# Device modes

[简体中文](device-modes_CN.md) | [Documentation index](README.md)

**Vibe Mode** is ESP-Mosaico's built-in application installation and device
maintenance mode. It runs ESP-Iris and provides application updates, Wi-Fi setup
and **Download Ideas**. Download Ideas retrieves applications over the network;
it does not put the chip in ROM Download Mode.

| Mode | What runs | When to use it |
| --- | --- | --- |
| Normal application | Your application; ESP-Iris is available when integrated and started | Run, debug and verify your application |
| Vibe Mode | Retained firmware in the Flash `vibe_mode` partition (`app/test`), including ESP-Iris | Install/update applications and maintain the device |
| ROM Download Mode | The chip's built-in firmware download program, without ESP-Iris | Provision a blank device or restore unusable base firmware |

Agents can connect to either a normal application with ESP-Iris or Vibe Mode.
An offline device or failed handshake does not establish ROM mode or damaged
firmware. Check ownership, active operations and the connection first, following
the [live evidence rules](project-gateway.md#live-evidence-and-next-steps).

## Choose an operation

`python mosaico.py iris test enter-recovery` enters the existing Vibe Mode from
a reachable normal application without installing firmware. Application update
commands manage this transition automatically.

`python mosaico.py recover` writes base firmware and then verifies Vibe Mode.
It can start from a connected device; manual ROM entry is only needed when the
workflow requests it. For provisioning conditions and application updates, follow
[update selection](mosaico-cli.md#select-an-update-method); Wi-Fi and Download Ideas
commands are in the [CLI reference](mosaico-cli.md#command-responsibilities).

## Physical entry

- **Vibe Mode — AI button (GPIO7):** with a working Mosaico bootloader and retained
  firmware, hold AI while powering on, then release after Vibe Mode starts.
  The bootloader selects `vibe_mode` (`app/test`) for this boot without changing OTA selection.
- **ROM Download Mode — Boot button (GPIO61):** only when `recover` requests
  manual entry, power off, hold Boot (to the left of the USB-C port), and power
  on while holding it. Release Boot after ROM Download Mode entry and tell the
  agent the physical steps are complete. The agent verifies the ROM connection
  and resumes `recover`; Vibe Mode and ESP-Iris do not run in this mode.

These are different buttons and startup paths. After a one-boot AI-button entry,
restarting with AI released follows the existing OTA selection, if it is valid.
Application updates select and boot the installed app; verify its identity,
health and intended behavior. After ROM restoration, verify Vibe Mode readiness
before installing the intended application.

## Ideas and firmware maintenance

Download Ideas accepts a six-digit application number, preserving leading zeros.
View the app's name, icon, description and compatible release, then confirm the
download to NAND. Downloading and selecting the boot application are separate
operations. My Ideas manages local downloads; About provides a confirmed online
Vibe Mode firmware update. The legacy Bridge pairing workflow is a separate
maintenance interface. For the workspace's reviewed base bundle and ROM recovery
procedure, see [recovery operations](https://github.com/esp-mosaico/esp-mosaico-utils/blob/main/mosaico-tools/README.md#rom-recovery-operations).

The 0.2 product uses Recovery ABI 2 and the retained test-partition layout;
normal firmware runs in `main_app` (`app/ota_0`). The pinned Utils revision
contains a reviewed prebuilt Vibe Mode bundle, not its firmware source project.
See [workspace migration](workspace-migration.md) before using an older device or workspace.
