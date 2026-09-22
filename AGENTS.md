# ESP-Mosaico Agent Rules

Select task-relevant [skills](.agents/skills/README.md) at the task level.
User guides start at [docs](docs/README.md).

## Repository boundaries

- User applications belong in `projects/`; settings in `.mosaico.json`.
  Public tooling/templates belong in `submodule/esp-mosaico-utils/mosaico-tools`;
  Recovery firmware/integration/ABI in utils' `esp-mosaico-recovery/`, fixtures in its `tests/firmware/`.
  Never use Recovery as an application template or mix test fixtures into user apps.
- Board support and complete game examples belong in `submodule/esp-mosaico-bsp/`;
  generic game implementation/tests belong in `submodule/raylib-lite-engine/`.
  Initialize and inspect only needed submodules, including BSP before using its examples.
- Keep user documentation in `docs/`, indexed by `docs/README.md`; keep root
  English/Chinese READMEs aligned and short. Component API/protocol docs stay with owners.
- Agent guidance/tools belong in `.agents/`, shared skills in version-controlled
  `.agents/skills/`, local analysis in `.agents/analysis/`; this is not a secrecy boundary.
- Dependencies must not read vibe internals. Validate owning-repository examples from
  standalone clones with declared, resolvable dependencies, not workspace sibling paths.
  Generated apps use explicit dependency roots and relocatable paths; follow the
  [migration guide](docs/workspace-migration_CN.md) when moving or migrating a workspace.
- Inspect component source, examples and upstream docs; do not invent APIs.
  Do not import Mosaic claw hub/runtime.

## Environment and builds

- Use Python **3.10+**, target **esp32s31**, and ESP-IDF at exactly
  **`7b9cc1ac79f865983f59bb8ff3ff43eb74ff1dbe`**, not the latest `master`.
- Before ESP-IDF work, verify the checkout with `git -C <idf-path> rev-parse HEAD`
  and follow [build doctor/build workflow](.agents/skills/idf-low-noise-build/SKILL.md).
  Check Python, target support and application/Recovery manifest constraints;
  cached paths or version numbers alone are insufficient.
- If the pinned checkout is missing, use [environment setup](.agents/skills/espressif-env-setup/SKILL.md)
  and verify a first build. This repository authorizes that fresh installation without
  another request, overriding the skill's explicit-install trigger. Use standard paths
  when unspecified and preserve other SDK checkouts.

## Application development

- Create apps with `python mosaico.py project init <name>` using utils' Hello World
  template; create games with `python mosaico.py game create <name> --template ...`.
- For device UI, prefer GSP and load [mosaico-ui](.agents/skills/mosaico-ui/SKILL.md);
  for GSP also load [gsp-sim](.agents/skills/gsp-sim/SKILL.md) for its pinned Registry
  version, native C preview commands and runtime constraints.
- For games, prefer Raylib Lite Engine and load
  [mosaico-game-development](.agents/skills/mosaico-game-development/SKILL.md).
- Confirm unsettled UI designs before implementation, preferably with rendered mockups;
  reuse existing confirmation. Validate shared native UI/game behavior in the simulator,
  fixing and re-running affected flows before device validation. Screenshots alone
  do not validate interaction. Start on hardware only for hardware-dependent issues
  or unavailable simulator coverage, and state remaining validation gaps.

## Retained Recovery contract

Unless the developer approves another architecture, every application must:

- Preserve Recovery's immutable partition prefix and the utils Hello World workflow.
  Before application integration changes, read the
  [public integration contract](submodule/esp-mosaico-utils/mosaico-tools/docs/application-integration.md).
- Set `CONFIG_ESP_IRIS_OTA_DEFAULT_VIA_RECOVERY=y`, use utils' `esp_mosaico_app_recovery`,
  call `iris_ota_support_start()`, and keep the OTA writer only in Recovery.
  Include utils' `esp-mosaico-recovery/cmake/mosaico_idf_project.cmake` before `project()`.
- Use `python mosaico.py iris system-update --project ...` for new apps, changed
  layouts or external resources. Use `iris app-update` only for code-only changes
  with an identical full partition table; never reshape the layout to make it pass.

## Device operations and acceptance

- Before device work, load [mosaico-device-operations](.agents/skills/mosaico-device-operations/SKILL.md)
  for diagnosis, update/recovery decisions, evidence checks and task-specific guides.
- Use only `python mosaico.py` for device operations. Do not invoke ESP-IDF/ESP-Iris
  device-write commands directly or borrow BSP Serial/JTAG flashing/monitoring flows.
- Start with `python mosaico.py iris status --all --json` and
  `python mosaico.py iris list --details --json`; coordinate ownership before connecting.
  Prefer the project's existing device; omit selectors for a sole available USB device.
  Never change boards after failed explicit selection or while awaiting reconnection.
- Never stop another client's Gateway to obtain a device or open USB/serial directly
  while Gateway owns it. Follow active operation/takeover records before new writes.
- Recovery always assigns High-Speed USB to ESP-Iris. Normal apps do too unless the
  product requires it; document that exception and preserve Iris operations/recovery
  through another available transport.
- Require live identity/state evidence; discovery caches, host Gateway status,
  port names and screens do not establish firmware mode or health.
  Verify the same Device ID, new Boot IDs after reboots, ready Recovery and healthy
  intended application behavior across normal -> Recovery -> normal.
- Preserve structured evidence/raw logs and let `mosaico.py` save valid core dumps
  before destructive operations. Upload, reconnect or reachable Recovery is not acceptance.
- When observation helps, share the Gateway Web URL and verify CLI/Web agree on
  Device ID, Boot ID and operation records.

## Provisioning and last-resort recovery

- Run `python mosaico.py recover` before first install on blank/unverified devices,
  or when neither normal nor Recovery Iris is reachable; follow the guides' state checks.
- Manual ROM entry is the last resort. Follow the [physical recovery steps](docs/mosaico-cli_CN.md#调试与恢复入口):
  the developer handles physical actions; the agent resumes detection, recovery and
  firmware/behavior verification, returning to the Iris Gateway when reachable.
- Never erase the whole flash merely to restore connectivity, or overwrite credentials,
  identity, Recovery data or partitions without explicit user authorization.
