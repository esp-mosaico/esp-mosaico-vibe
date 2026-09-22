# ESP-Mosaico repository skills

These shared skills are versioned with the workspace in `.agents/skills/`.
Read [AGENTS.md](../../AGENTS.md) for repository-wide rules and task-level routing.
Select each skill from the user's task and its stated scope. A task may use several
skills, but each skill must remain independent: no peer names, discovery, loading,
or links to another skill's instructions or resources. Keep its own prerequisites,
workflow and outputs explicit; reference shared docs, source and tools directly.
Developer-facing guides start at the [documentation index](../../docs/README.md).

## Skill list

| Skill | Purpose |
|-------|---------|
| [`mosaico-ui`](mosaico-ui/SKILL.md) | Create or improve device UI, prioritizing simulator validation after design confirmation and before device validation. |
| [`espressif-env-setup`](espressif-env-setup/SKILL.md) | Install a fresh ESP-IDF, ESP-AT, or ESP-ADF environment and verify the first build. |
| [`idf-low-noise-build`](idf-low-noise-build/SKILL.md) | Run low-noise ESP-IDF builds with complete logs and focused failure diagnostics. |
| [`mosaico-device-operations`](mosaico-device-operations/SKILL.md) | Diagnose live devices, install/update applications, or recover through mosaico.py with identity and outcome verification. |
| [`gsp-sim`](gsp-sim/SKILL.md) | Preview GSP apps on PC with sim_bridge by default (esp-gsp 1.4.0). |
| [`mosaico-game-development`](mosaico-game-development/SKILL.md) | Prefer Raylib Lite Engine for games; validate visuals and gameplay in the simulator before device validation. |
