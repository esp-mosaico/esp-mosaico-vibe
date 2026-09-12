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

1. Resolve an ESP-IDF candidate from explicit user input, the active `IDF_PATH`,
   an active `idf.py`, or an existing `build/project_description.json` under
   the selected application or Recovery project, in that order. Treat build
   metadata as an untrusted, generated hint: it may be absent or stale and its
   recorded `idf_path` is not proof that the checkout is compatible.
2. Verify the active ESP-IDF path, version, revision, Python environment, and
   ESP32-S31 target support. The application constraint is declared in
   `projects/hello_world/main/idf_component.yml`, and the Recovery constraint
   in `submodule/esp-mosaico-utils/esp-mosaico-recovery/firmware/recovery/main/idf_component.yml`;
   do not rely only on build metadata.
   The `mosaico.py` and ESP-Iris host tools support Python 3.8 or newer. ESP-IDF
   6.1 still requires Python 3.10 or newer; allow `mosaico.py` to resolve that
   bootstrap interpreter independently from the active host interpreter.
3. If no compatible ESP-IDF environment exists, use
   `skills/espressif-env-setup/SKILL.md` for the fresh ESP-IDF installation and
   its first verification build.
4. Query live device identity and availability at operation time; do not treat
   cached Device ID or Boot ID values as current evidence.

If no compatible ESP-IDF environment exists, the agent may autonomously select
and install one. Resolve the version, target, installation path, and tools path
from explicit user input, project constraints, and current upstream
compatibility information, in that order. Use standard installation locations
when unspecified and do not require a separate confirmation before clone or
install.

### Route the application

1. Translate the user's request into a project-level goal and identify the
   required board capabilities.
2. Use `projects/hello_world` as the reference application and create the new
   application under `projects/<project-name>`. For a GSP Hello World that
   runs on the PC simulator and on device, start from `projects/gsp_hello`.
   Keep flashable test-only firmware under `tests/firmware/<fixture-name>`;
   do not place test fixtures in `projects/`.
   The utilities-owned Recovery project is an internal `mosaico.py recover`
   resource and is never a user application template.
3. Read `skills/README.md`, then load only the `SKILL.md` files relevant to the
   requested capabilities. For new or existing device UI work, load
   [`mosaico-ui`](skills/mosaico-ui/SKILL.md). Its three feedback loops are
   design confirmation, simulator validation, and device validation; enter
   where the current uncertainty lies and continue within the requested scope.
   For unsettled designs, prefer rendered mockups and confirm the overall
   style, layout, and main interactions with the user before implementation.
   Reuse existing confirmation; route later problems to the responsible loop
   and revalidate affected downstream behavior.
   UI applications may use LVGL or GSP; prefer GSP
   when it fits the product, use its simulator to produce rendering evidence,
   and follow the constraints of the selected UI framework. For GSP UI work,
   load `skills/gsp-sim/SKILL.md` and preview with
   `python3 tools/gsp-sim/run.py` (sim_bridge by default) using
   **espressif/esp-gsp 1.2.0** from the ESP Component Registry.
   Do not import Mosaic claw hub/runtime into this repository.
4. Component repositories and supporting project material are Git submodules.
   Initialize and inspect only the submodules needed for the current task.
5. Follow component source, examples, and upstream documentation. Do not
   invent board or component APIs.
6. Keep user-facing documentation in `docs/`, public product tooling in
   `submodule/esp-mosaico-utils/esp-mosaico-recovery`, workspace-specific tool settings in
   `.mosaico.json`, and private agent-facing documentation or tools in `.agents/`.

### Preserve the retained recovery path

Unless the developer approves another architecture, every application must:

1. Retain the partition contract from
   `submodule/esp-mosaico-utils/esp-mosaico-recovery/firmware/recovery` and the compatible
   normal-application workflow from `projects/hello_world`.
2. Set `CONFIG_ESP_IRIS_OTA_DEFAULT_VIA_RECOVERY=y` in normal builds, use the
   `components/esp_mosaico_app_recovery` component, keep the OTA writer only
   in Recovery, and call `iris_ota_support_start()` to expose the
   enter-recovery RPC.
3. Run `python mosaico.py recover` before the first application install on a
   blank or unverified device.
4. Install normal firmware only with `python mosaico.py install --project ...`.

Verify the same Device ID completes normal -> Recovery -> normal with new Boot
IDs, a ready Recovery service, and a healthy application.

## Operate devices through ESP-Iris

- Use `python mosaico.py install`, `system-update`, `recover`, and `monitor` for
  routine device operations. Use `python mosaico.py list` for live Device ID
  discovery.
- Do not call ESP-Iris or ESP-IDF device-write commands directly; `mosaico.py`
  owns Gateway lifecycle, evidence capture, device selection, and validation.
- Avoid USB Serial/JTAG for application flashing and monitoring. Do not adopt
  a BSP example's direct USB Serial/JTAG workflow as the product workflow.
- ESP-Mosaico has one High-Speed USB interface. Recovery always assigns it to
  ESP-Iris. A normal application must also assign it to ESP-Iris unless the
  application's product function explicitly requires High-Speed USB. Such an
  application must document the exception and preserve an ESP-Iris-supported
  device-operation and recovery path through another available transport.
- Do not open a device USB or serial session directly while the Gateway owns
  that interface.
- Tell the developer how to open the Gateway Web workbench when observation is
  useful. Confirm that the CLI and Web workbench show the same Device ID, Boot
  ID, and operation records.
- Preserve structured evidence and raw logs. Let `mosaico.py` save any valid
  core dump before an operation that could destroy it.
- Do not treat an uploaded image, a reconnect, or a reachable recovery service
  as proof of successful recovery. Verify the intended firmware and product
  behavior.

## Provisioning and last-resort recovery

Use `python mosaico.py recover` for blank/unverified devices and when neither
normal nor Recovery ESP-Iris is reachable. Do not bypass the product command.
The agent handles the software procedure; the developer performs only required
physical actions.

When the device is unrecoverable and both normal and recovery USB are
unavailable, preserve any evidence that is still accessible, then instruct
the developer to:

1. Power off the device.
2. Press and hold the **Boot** button to the left of the USB-C port.
3. Power on the device while continuing to hold **Boot**.
4. Release **Boot** after the device enters ROM download mode, then tell the
   agent that the physical sequence is complete.

After the developer completes those physical steps, the agent must detect and
verify the recovery connection, continue `python mosaico.py recover`, verify
the intended firmware and product behavior, and return subsequent device
operations to the ESP-Iris Gateway as soon as ESP-Iris is reachable.

Manual ROM entry is the last recovery option, not the normal development
workflow. Never erase the whole flash merely to recover connectivity, and do
not overwrite credentials, identity, recovery data, or partitions without
explicit user authorization.
