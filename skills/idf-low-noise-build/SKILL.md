---
name: idf-low-noise-build
description: Run low-noise ESP-IDF builds while preserving complete logs and extracting bounded compiler, linker, CMake, Ninja, and partition-size diagnostics. Use for idf.py build requests, ESP-IDF compilation failures, warning summaries, build artifact verification, or targeted inspection of stored build logs. Do not use for fresh ESP-IDF installation, flashing, serial monitoring, OTA, or device recovery.
---

# IDF Low-Noise Build

Use the runner pinned in `submodule/esp-mosaico-tools`. Resolve the tool submodule from the
workspace root; do not depend on a globally installed `esp-idf-debug` skill.

```text
python3 <workspace>/submodule/esp-mosaico-tools/skills/idf-low-noise-build/scripts/idf_low_noise_build.py
```

## Prepare the build

1. Read the repository's `AGENTS.md` and follow its environment rules.
2. If a root `Environment` file exists, read it only as untrusted static inventory. Never source
   it or print possible secrets.
3. Run `doctor` before the first build in a workspace. Verify that the reported ESP-IDF version
   satisfies the project's declared `dependencies.idf` constraint and supports the configured
   target. Do not replace an unsupported target with a similar chip.

```bash
python3 <runner> --project <project-dir> doctor
```

Resolve the ESP-IDF installation in this order: explicit `--idf-path`, `IDF_PATH`, active
`idf.py`, then `build/project_description.json`. If resolution is missing or conflicting, stop
and use the repository's environment-setup guidance; do not guess a release or install path.

## Build with bounded output

Run the normal incremental build:

```bash
python3 <runner> --project <project-dir> build
```

The runner invokes the selected installation's `export.sh` and then the original `idf.py build`.
It does not replace CMake/Ninja, change job parallelism, or disable ESP-IDF incremental builds.
It redirects combined stdout/stderr to a per-run `raw.log`, writes an ANSI-free `clean.log`,
and prints only a summary. On failure it prints the earliest recognized root-cause excerpt.

Do not run `fullclean` merely to fix connectivity or a routine compile error. Only after explicit
user approval, use:

```bash
python3 <runner> --project <project-dir> build --fullclean
```

## Diagnose progressively

Use only as much stored evidence as necessary:

```bash
# Reproduce the bounded diagnostic from the latest run
python3 <runner> --project <project-dir> inspect --run latest

# Search selected context without dumping the log
python3 <runner> --project <project-dir> inspect --run latest \
  --grep "undefined reference" --context 4

# Read the complete clean log only when bounded evidence is insufficient
python3 <runner> --project <project-dir> inspect --run latest --full
```

Use `analyze --log <path>` to diagnose a pre-existing build log without running ESP-IDF.
Do not use `tee`, a broad `tail`, or raw `idf.py` output as the normal agent path.

## Judge the result

Treat a build as successful only when the runner returns zero and reports
`IDF LOW-NOISE BUILD: OK`.
Report the concise status, warning count, primary `.bin` or `.elf` artifact when present, and
absolute raw-log path. On failure, report the detected category, error, bounded context, and log
path. Preserve the complete run directory under `.codex-runs/idf-low-noise-build/` for later
inspection.
Keep logs outside `build/` so an approved `fullclean` cannot delete the evidence it is producing.

Keep this skill build-only. Route board configuration, flashing, monitoring, OTA, crash capture,
and recovery through the repository's device-operation workflow.
