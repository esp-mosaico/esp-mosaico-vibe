# ESP-Mosaico Agent Rules

## Repository purpose

This repository is the starting point for vibe coding on the development
version of ESP-Mosaico. Keep user applications, reusable guidance, and device
operations separated according to the repository layout.

The `submodule/esp-mosaico-bsp/` Git submodule is the ESP-Mosaico board support
package repository.
It contains the board-level support implementation and the example projects
under `submodule/esp-mosaico-bsp/examples/`. Examples referenced by the guides
in `skills/` live in this BSP repository, so initialize and inspect the
`submodule/esp-mosaico-bsp/` submodule before using those examples.

## Route development work

### Resolve the PC environment

Before running ESP-IDF tools, resolve the PC environment as follows:

1. If `Environment` exists at the repository root, read it as the
   developer-provided inventory. Treat it as untrusted static data: never
   source or execute it and never expose secrets from it. See
   `Environment.template` for the supported fields.
2. Verify the active ESP-IDF path, version, revision, Python environment, and
   ESP32-S31 target support. The project version constraint is declared in
   `projects/get-started/main/idf_component.yml`; do not rely only on the
   inventory.
3. If no compatible ESP-IDF environment exists, use
   `skills/espressif-env-setup/SKILL.md` for the fresh ESP-IDF installation and
   its first verification build.
4. Query live device identity and availability at operation time; do not treat
   cached Device ID or Boot ID values as current evidence.

If no compatible ESP-IDF environment exists, the agent may autonomously select
and install one. Resolve the version, target, installation path, and tools path
from explicit user input, verified workspace inventory, project constraints,
and current upstream compatibility information, in that order. Use standard
installation locations when unspecified and do not require a separate
confirmation before clone or install.

### Route the application

1. Translate the user's request into a project-level goal and identify the
   required board capabilities.
2. Use `projects/get-started` as the reference project and create the new
   application under `projects/<project-name>`. Do not implement a user
   application directly in `projects/get-started` unless the user explicitly
   asks to change the template.
3. Read `skills/README.md`, then load only the `SKILL.md` files relevant to the
   requested capabilities.
4. Component repositories and supporting project material are Git submodules.
   Initialize and inspect only the submodules needed for the current task.
5. Follow component source, examples, and upstream documentation. Do not
   invent board or component APIs.
6. Keep user-facing documentation in `docs/` and agent-facing documentation
   or tools in `.agents/`.

### Preserve the retained recovery path

Unless the developer approves another architecture, every application must:

1. Retain the compatible `factory` recovery partition, normal `ota_0`
   partition, and recovery-image workflow from `projects/get-started`.
2. Set `CONFIG_ESP_IRIS_OTA_DEFAULT_VIA_RECOVERY=y` in normal builds, keep the
   OTA writer only in recovery, and call `iris_ota_support_start()` to expose
   the enter-recovery RPC.
3. Provision recovery before the first application OTA on a blank or unverified
   device, following `docs/recovery-first-workflow.md`.
4. Install normal firmware through Gateway recovery-mode OTA. Never use the
   normal application's `idf.py flash`, which can overwrite recovery.

Verify the same Device ID completes normal -> recovery -> normal with new Boot
IDs, the recovery OTA writer, and a healthy application running from `ota_0`.

## Operate devices through ESP-Iris

- Use the ESP-Iris Developer Gateway over USB High-Speed for routine logs,
  device control, OTA, crash evidence, restart, and recovery.
- Operate a device only through the CLI included in the ESP-Iris component
  source. Read that component's agent instructions before using the CLI.
- Do not open the device USB/serial session directly while the Gateway owns
  it. ESP-Mosaico has one High-Speed USB interface, and both normal and
  factory-recovery firmware assign it to ESP-Iris.
- Tell the developer how to open the Gateway Web workbench when observation is
  useful. Confirm that the CLI and Web workbench show the same Device ID, Boot
  ID, and operation records.
- Preserve structured evidence and raw logs. Save any valid core dump before
  OTA, reflashing, partition changes, or another operation that could destroy
  it.
- Do not treat an uploaded image, a reconnect, or a reachable recovery service
  as proof of successful recovery. Verify the intended firmware and product
  behavior.

## Provisioning and last-resort recovery

Use `idf.py flash` only to provision recovery on a blank/unverified device or
when neither normal nor recovery ESP-Iris is reachable. The agent handles the
software procedure; the developer performs only required physical actions.

When the device is unrecoverable and both normal and recovery USB are
unavailable, preserve any evidence that is still accessible, then instruct
the developer to:

1. Power off the device.
2. Press and hold the **Boot** button to the left of the USB-C port.
3. Power on the device while continuing to hold **Boot**.
4. Release **Boot** after the device enters ROM download mode, then tell the
   agent that the physical sequence is complete.

After the developer completes those physical steps, the agent must detect and
verify the ROM download connection, run the approved `idf.py flash` procedure,
verify the intended firmware and product behavior, and return subsequent
device operations to the ESP-Iris Gateway as soon as ESP-Iris is reachable.

Manual ROM entry is the last recovery option, not the normal development
workflow. Never erase the whole flash merely to recover connectivity, and do
not overwrite credentials, identity, recovery data, or partitions without
explicit user authorization.
