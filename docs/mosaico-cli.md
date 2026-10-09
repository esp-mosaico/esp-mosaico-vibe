# mosaico.py command reference

[简体中文](mosaico-cli_CN.md) | [Documentation index](README.md)

Run `python mosaico.py --help` from the workspace root to view the top-level entry
points. Use `python mosaico.py iris --help`, `python mosaico.py iris takeover --help`,
or `--help` on a specific command to inspect its arguments.

```text
mosaico.py
├── doctor
├── project init
├── iris
│   ├── run / status
│   ├── list / claim / release / reconcile
│   ├── takeover start / status / resume / abort / reconcile
│   ├── logs / memory / crash / rpc
│   ├── device-status / screenshot / operation-status
│   ├── app-update
│   ├── system-update
│   └── test enter-recovery / recovery-wifi / bridge-code
├── recover
└── game create/new/sim/run/build (blank template, BSP examples and engine Host)
```

For mode definitions and the AI/Boot button distinction, see
[device modes](device-modes.md). `recover` writes base firmware;
`iris test enter-recovery` only enters the existing Vibe Mode.

## Command responsibilities

| Command | Purpose |
| --- | --- |
| `doctor` | Check the host environment without starting a Gateway |
| `project init <name>` | Create an application from the workspace template without starting a Gateway |
| `iris run` | Retain a client on the shared Gateway; on initial creation, try to connect the sole device. Ctrl+C releases only this client |
| `iris status [--all]` | Passively query this project's Gateway, or Gateways across workspaces for the same OS user, including clients and device ownership; neither start nor keep a Gateway alive |
| `iris list` | Enumerate visible endpoints and known devices; discovery does not claim or connect a device |
| `iris claim/release/reconcile` | Claim or release a device, or explicitly clean up ordinary ownership confirmed to be stale |
| `iris takeover start/status/resume/abort/reconcile` | Request takeover, query its record, resume verification, abort, or reconcile ownership |
| `iris logs` | Show retained logs and keep following new output; `--snapshot` reads only retained logs |
| `iris memory` | Read memory status; `--follow` samples continuously |
| `iris crash` | Inspect crash information; `--archive` archives and decodes the Core Dump |
| `iris rpc` | Call the specified application RPC |
| `iris restart` | Restart the selected device and verify a new Boot ID; application behavior requires separate checks. |
| `iris device-status` | Read live device identity, Boot ID and firmware state; reject stale or mismatched responses |
| `iris screenshot <output>` | Save the device image directly and return image/operation metadata; verify the same Device ID and Boot ID before and after capture |
| `iris operation-status <operation-id>` | Query an existing operation, preserving its actual status; does not acquire a device or replay the operation |
| `iris app-update` | Update normal application code only; requires the complete partition table to match the device |
| `iris system-update` | Recommended entry for a new application, partition layout changes or resource changes; writes according to the update bundle manifest |
| `recover` | Provision or restore the device's base firmware, including when ESP-Iris is unreachable |
| `game` | Create games from BSP examples and invoke engine Host simulation or builds; see [game development](game-development.md) |

Commands under `iris test` exercise individual Vibe Mode workflows:

| Command | Preconditions and success criteria |
| --- | --- |
| `enter-recovery` | A normal application responds through ESP-Iris and reboots into Vibe Mode; wait for the same Device ID to reconnect with a new Boot ID. If already in Vibe Mode, return its current status |
| `recovery-wifi` | Requires Vibe Mode with ESP-Iris USB available; submit the Wi-Fi network name and password, then wait for connectivity |
| `bridge-code` | Requires Vibe Mode, available USB, a configured Bridge service and network connectivity; open the device download page and return the pairing code, validity period and Bridge website URL |

## Build tool compatibility and diagnostics

`doctor` checks ESP-IDF constraints against `tools/cmake/version.cmake` first.
Its JSON identifies that version as `idf.version` and the source as
`idf.version_source`, while `idf.reported_version` keeps the Git description
from `idf.py --version`. The latter is a fallback when the source version is
unavailable or cannot be parsed. No local `v6.2-dev` tag is needed. The low-noise
build doctor uses the same rule; Python and target checks still run.

The `.mosaico.json` `build` object accepts `"configdep": "auto"` (default),
`"on"` or `"off"`. The pinned configdep 0.2.3 maps `CONFIG_*` references in
source files to paths that can contain Windows reserved names such as `AUX`.
Auto therefore disables this optimizer on Windows and retains IDF's default on
other hosts. Auto honors an existing `IDF_CONFIGDEP_ENABLE`; explicit on/off
overrides it. This applies to application, Vibe Mode and system-update builds.
The standalone low-noise runner uses the same platform default and accepts that
environment variable for explicit overrides. Disabling configdep retains Ninja
incremental builds, but configuration changes may recompile more source files.

Both the application template and Vibe Mode bootstrap the pinned standalone GSPC
compiler without requiring the Python `gsp` module. An uncached first run needs
a download; offline builds can set `GSPC_EXECUTABLE` to an absolute path to a
compatible compiler. Applications use GSPC 0.6.1 and ESP-GSP 1.5.1;
Vibe Mode independently retains GSPC 0.5.0 and ESP-GSP 1.4.0. An explicit compiler
override must match the project being built.

Build error summaries include configdep `touch_file` failures and their following
path details, falling back to `FAILED:` context when the cause is unrecognized.
Text errors go to stderr; JSON errors include `details.diagnostic`. Complete
output remains in the log named by the error.

## Select a project and device

`--workspace` selects the workspace; by default, the CLI searches upward for
`.mosaico.json`. `--project` selects the application and its Gateway session;
update commands also use that application as the build target. When omitted,
selection follows the current application directory, the workspace default
application, then the sole application candidate.

Creating a project does not change the default project, so the following commands
specify `--project` explicitly:

```sh
python mosaico.py iris list --project projects/my_app
python mosaico.py iris logs --project projects/my_app --timeout 20
python mosaico.py iris memory --project projects/my_app
python mosaico.py iris crash --project projects/my_app --archive
```

With a single available USB device, device selectors can be omitted. With multiple
devices, use `--device-id` or `--endpoint`. Use a Device ID verified through a live
connection. See [Gateway device selection](project-gateway.md#device-discovery-and-selection)
for automatic selection order, reconnection waits and ownership restrictions.

## Select an update method

| Scenario | Command |
| --- | --- |
| Blank or unverified device, or neither normal nor Vibe Mode is reachable after connection checks | `python mosaico.py recover` |
| New application, partition layout changes or external resource changes | `python mosaico.py iris system-update --project projects/my_app` |
| Application code changes only, with the complete partition table matching the device | `python mosaico.py iris app-update --project projects/my_app` |

`recover` prepares the reviewed base firmware and verifies that Vibe Mode is ready.
The target application still needs to be installed and accepted afterward. It
supports only a local Gateway and also manages the underlying recovery process.

A `system-update` bundle built from a project contains the application, partition
table and resource images declared by the project. It preserves the fixed Vibe Mode
prefix and bootloader. A reserved but unused `game_assets` partition needs no image;
applications using external resources declare their images through CMake for inclusion
in the bundle. Use `--bundle PATH` for an existing bundle. See the
[Vibe Mode guide](https://github.com/esp-mosaico/esp-mosaico-utils/blob/main/esp-mosaico-recovery/firmware/recovery/README.md)
for updates to Vibe Mode itself, HTTP(S)/NAND updates and base bundle constraints.

If the partition tables differ, `app-update` returns `partition_layout_mismatch`,
the device and build SHA-256 hashes, and a `system-update` recommendation. It does
not automatically expand the write scope or modify the project's partition table.
The effective build configuration, including with `--skip-build`, and the updated
application must pass role, product, board, layout contract and Recovery ABI checks.

Before updating, use the product tools to preserve valid core dumps, structured
evidence and raw logs. Acceptance requires the same Device ID to complete normal →
Vibe Mode → normal with new Boot IDs, run the target firmware, report healthy, and
exhibit the intended product behavior. For a blank device, verify Vibe Mode readiness
before installing the application. Upload completion or reconnection alone does not
establish acceptance.

## Debugging and recovery entry points

Run `python mosaico.py iris run --project projects/my_app` to retain a debugging
session. The printed workbench URL lets the developer observe it. Agents use CLI
output for device evidence:

```sh
python mosaico.py iris device-status --project projects/my_app --json
python mosaico.py iris screenshot device.png --project projects/my_app --json
python mosaico.py iris operation-status <operation-id> --project projects/my_app --json
```

Inspect the saved device image directly. Screenshot output includes its path,
Device ID, Boot ID, operation ID and Gateway artifact metadata. A reboot during
capture fails verification; any saved image is not proof of the current boot.
Operation queries can start/reuse the project's Gateway to read its retained
records, without connecting to a device. `ok: true` means the query succeeded;
read `operation.status` for the actual outcome, including `failed` or `outcome_unknown`.
Use browser automation and CLI/Web comparison for workbench testing or an explicit
user request. See the [Gateway guide](project-gateway.md) for lifecycle, device
ownership and takeover rules.

After connection checks, use `recover` when neither the normal application nor
Vibe Mode is reachable. If manual ROM entry is required, the developer follows
the [physical entry steps](device-modes.md#physical-entry); the agent verifies
the ROM connection, resumes `recover`, and checks the device and target application. Never erase the whole flash merely to restore
connectivity, or overwrite credentials, identity or Vibe Mode data without authorization.

The 0.2 implementation restores stock `idf.py flash monitor` and OpenOCD/JTAG.
Release Gateway's console endpoint before native monitoring or flashing. Mosaico
applications require a matching 0.2 base and the retained test-partition contract;
third-party ESP-IDF projects may use their own unchanged partition tables. Return
through `recover` to reinstall the Mosaico base after a third-party full flash.
Mosaico updates remain the default workflow. High-Speed USB provides separate
console and data interfaces; UART and USB Serial/JTAG provide control only.

## Migrate command paths to 0.2

The old command aliases are removed. Update existing scripts to the formal paths:

| Legacy entry | Current entry |
| --- | --- |
| `init` | `project init` |
| `session run/status` | `iris run/status` |
| `list`, `device claim/release/reconcile` | `iris list/claim/release/reconcile` |
| `monitor` | `iris logs` |
| `memory/crash/rpc` | `iris memory/crash/rpc` |
| `install` | `iris app-update` |
| `system-update` | `iris system-update` |
| `enter-recovery/recovery-wifi/bridge-code` | `iris test enter-recovery/recovery-wifi/bridge-code` |

`recover`, `doctor` and the workspace's `game` entry remain. `.mosaico.json` now
requires schema 2; 0.2 state and credentials are isolated from earlier versions.
See the [migration guide](../submodule/esp-mosaico-utils/mosaico-tools/docs/migration-0.2.md).
The reviewed 0.2.0 base bundle has passed device acceptance. See the
[validation record](../submodule/esp-mosaico-utils/esp-mosaico-recovery/docs/validation-0.2.md)
for tested coverage and remaining gaps: S31 High-Speed USB combined
`flash monitor` still has an initial enumeration race, and OpenOCD/JTAG hardware
validation remains pending. The 0.1 base is not accepted by 0.2 tools.

For GSP previews, use `python mosaico.py project sim --project projects/my_app`.
The workspace has no pre-created application, so run `project init` first. The
public implementation belongs to utils.
