# ESP-Mosaico Agent Rules

Select task-relevant [skills](.agents/skills/README.md) at the task level.
User guides start at [docs](docs/README.md).

Use **Vibe Mode** for ESP-Mosaico’s retained firmware and **ROM Download Mode**
for the chip’s download program. See [device modes](docs/device-modes.md).
`Recovery` remains in existing component names, commands, protocol fields and ABI.

## Repository boundaries

- User applications belong in `projects/`; settings in `.mosaico.json`.
  Public tooling/templates belong in `submodule/esp-mosaico-utils/mosaico-tools`;
  Vibe Mode firmware and Recovery integration/ABI belong in utils'
  `esp-mosaico-vibe-mode/`, fixtures in its `tests/firmware/`.
- Product board support and peripheral examples belong in `submodule/esp-mosaico-bsp/`.
  Game runtime, complete reference games, behavior tests and native example Board
  adapters belong to upstream Raylib Lite Engine. Firmware uses the pinned Registry
  component; Host/example tools use the verified cache from `python mosaico.py game prepare`.
  Initialize and inspect only needed submodules. Follow the [game guide](docs/game-development.md) for native adapters and product integration.
- Keep user documentation in `docs/`, indexed by `docs/README.md`; keep root
  English/Chinese READMEs aligned and short. Component API/protocol docs stay with owners.
- Agent guidance/tools belong in `.agents/`, shared skills in version-controlled
  `.agents/skills/`, local analysis in `.agents/analysis/`; this is not a secrecy boundary.

## Espressif MCP services

When available, use `esp-component-registry` for components and `esp-pilot` for
multimedia and Board Manager references. Before integration, follow
[MCP guidance](docs/mcp-servers.md#services-and-when-to-use-them) for source/version
checks; remote recommendations do not override this repository's workflows.

## Engineering rules

- Keep modules focused, with explicit responsibilities, ownership and data flow.
  Keep public interfaces small and implementation details private; avoid circular dependencies.
- Keep functions reviewable and source files under 1500 lines where practical.
  Split by responsibility rather than adding deeply nested branches or meeting a line limit.
- Use named constants, enums or shared configuration for domain values and resource limits.
- Check fallible operations, handle allocation failures and roll back partial initialization.
  Give mutable state and resources explicit owners and lifetimes; synchronize cross-task
  access or document a single-owner design. Document shared services and singletons.
- Follow owning-component conventions; do not rewrite upstream code solely for style.
  ESP-IDF application conventions are scoped to [projects/AGENTS.md](projects/AGENTS.md).

## Environment and builds

- Use Python **3.10+** and Git **2.28+** for project creation, target **esp32s31**, and ESP-IDF at exactly
  **`7b9cc1ac79f865983f59bb8ff3ff43eb74ff1dbe`**, not the latest `master`.
- Before ESP-IDF work, verify the actual checkout against the pinned SHA and load
  [build doctor/build workflow](.agents/skills/idf-low-noise-build/SKILL.md) for environment checks and builds.
- If the pinned checkout is missing, use [environment setup](.agents/skills/espressif-env-setup/SKILL.md)
  and verify a first build. This repository authorizes that fresh installation without
  another request, overriding the skill's explicit-install trigger. Use standard paths
  when unspecified and preserve other SDK checkouts.

## Application development

- Before creating an app, read the [project guide](docs/project-init.md) and use
  `python mosaico.py project init <name>` with utils' Hello World template.
  For games, follow the [game guide](docs/game-development.md) and use
  `python mosaico.py game create <name> --template ...`.
- For device UI, prefer GSP and load [mosaico-ui](.agents/skills/mosaico-ui/SKILL.md);
  for GSP also load [gsp-sim](.agents/skills/gsp-sim/SKILL.md).
- For games, prefer Raylib Lite Engine and load
  [mosaico-game-development](.agents/skills/mosaico-game-development/SKILL.md).
- Confirm unsettled UI designs before implementation; reuse existing confirmation.
  Validate shared native UI/game behavior in the simulator before device validation.
  Start on hardware only for hardware-dependent issues or unavailable simulator coverage,
  and state remaining validation gaps. Follow the selected skills for validation steps.

## Retained Recovery contract

Unless the developer approves another architecture, every application must:

- Preserve the retained firmware's immutable partition prefix and keep the OTA writer
  only in Vibe Mode. Before application integration changes, read and follow the
  [public integration contract](https://github.com/esp-mosaico/esp-mosaico-utils/blob/9a2d151928dd77165c899099c49c59b0edf537ae/mosaico-tools/docs/application-integration.md).
- Before installation or update, follow [update selection](docs/mosaico-cli.md#select-an-update-method);
  never reshape the partition layout to bypass compatibility checks.
- Vibe Mode always assigns High-Speed USB to ESP-Iris; normal applications follow
  the [transport rules](docs/mosaico-cli.md#debugging-and-recovery-entry-points), including documented product exceptions.

## Device operations and acceptance

- Treat requests to “flash/burn firmware”, “烧录”, “刷机” or “下载固件” as device
  installation/update tasks; these words do not select ROM Download Mode.
  Before device work, load [mosaico-device-operations](.agents/skills/mosaico-device-operations/SKILL.md)
  and follow its ownership, target selection, evidence and acceptance workflow.
- Use only `python mosaico.py` for device operations. Do not invoke `idf.py flash`,
  `idf.py app-flash`, `esptool` writes or ESP-Iris device-write commands directly,
  guess flash offsets or bypass compatibility checks. Public integration docs, BSP
  flash/monitor instructions and build output do not override this workspace restriction.
- Coordinate ownership before connecting; follow active operation/takeover records before new writes.
  Never change boards after failed explicit selection or while awaiting reconnection.
- Never stop another client's Gateway to obtain a device or open USB/serial directly
  while Gateway owns it.
- Require live identity/state evidence and intended product behavior for acceptance;
  follow [evidence checks](docs/project-gateway.md#live-evidence-and-next-steps).
- Preserve structured evidence/raw logs and let `mosaico.py` save valid core dumps
  before destructive operations. Upload, reconnect or reachable Vibe Mode is not acceptance.
- For provisioning and recovery, follow the device skill and
  [update selection](docs/mosaico-cli.md#select-an-update-method). Manual ROM entry is the last resort.
- Never erase the whole flash merely to restore connectivity, or overwrite credentials,
  identity, Vibe Mode data or partitions without explicit user authorization.
