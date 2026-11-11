# mosaico.py command reference

[简体中文](mosaico-cli_CN.md) | [Documentation index](README.md)

Run `python mosaico.py --help` from the workspace root to view the top-level entry
points. Use `python mosaico.py iris --help`, `python mosaico.py iris takeover --help`,
or `--help` on a specific command to inspect its arguments.

```text
mosaico.py
├── doctor
├── project init / sim / package / validate / upload
├── account login / status / logout
├── iris
│   ├── run / status
│   ├── list / claim / release / reconcile
│   ├── takeover start / status / resume / abort / reconcile
│   ├── logs / memory / crash / rpc
│   ├── device-status / screenshot / operation-status
│   ├── operation-reconcile
│   ├── flash / app-flash / run-app / restart
│   ├── app-update
│   ├── system-update
│   └── test enter-recovery / recovery-wifi / bridge-code
├── recover
└── game create/new/sim/run/build (blank template, engine examples and Host)
```

For mode definitions and the AI/Boot button distinction, see
[device modes](device-modes.md). `recover` writes base firmware;
`iris test enter-recovery` only enters the existing Vibe Mode.

## Command responsibilities

| Command | Purpose |
| --- | --- |
| `doctor` | Check the host environment without starting a Gateway |
| `project init <name>` | Create an application from the workspace template without starting a Gateway |
| `project sim` | Run the application's native C UI backend; explicit scene/dump options render only the scene |
| `project package/validate/upload` | Package an Ideas ZIP offline, validate it, or upload the existing ZIP as a draft |
| `account login/status/logout` | Manage Mosaico Ideas browser login and credentials |
| `iris run` | Retain a client on the shared Gateway; on initial creation, try to connect the sole device. Ctrl+C releases only this client |
| `iris status [--all]` | Passively query this project's Gateway, or Gateways across workspaces for the same OS user, including clients and device ownership; neither start nor keep a Gateway alive |
| `iris list` | Enumerate endpoints/devices through the selected project Gateway, starting it if needed; discovery does not claim or connect a device |
| `iris claim/release/reconcile` | Claim or release a device, or explicitly clean up ordinary ownership confirmed to be stale |
| `iris takeover start/status/resume/abort/reconcile` | Request takeover, query its record, resume verification, abort, or reconcile ownership |
| `iris logs` | Show retained logs and keep following new output; `--snapshot` reads only retained logs |
| `iris memory` | Read memory status; `--follow` samples continuously |
| `iris crash` | Inspect crash information; `--archive` archives and decodes the Core Dump |
| `iris rpc` | Call the specified application RPC |
| `iris device-status` | Read live device identity, Boot ID and firmware state; reject stale or mismatched responses |
| `iris screenshot <output>` | Save the device image directly and return image/operation metadata; verify the same Device ID and Boot ID before and after capture |
| `iris operation-status <operation-id>` | Query an existing operation, preserving its actual status; does not acquire a device or replay the operation |
| `iris operation-reconcile <operation-id>` | Continue verification; ROM recovery reconnects to its recorded identity without rewriting or resetting |
| `iris flash` / `iris app-flash` | Run the corresponding native IDF flash target through Gateway, then optionally resume the console; a development path, not product update acceptance |
| `iris run-app` | Ask Vibe Mode to retry the installed application over USB or UART |
| `iris restart` | Restart the selected device and verify that it returns with a new Boot ID |
| `iris app-update` | Update normal application code only; requires the complete partition table to match the device |
| `iris system-update` | Recommended entry for a new application, partition layout changes or resource changes; writes according to the update bundle manifest |
| `recover` | Provision or restore the device's base firmware, including when ESP-Iris is unreachable |
| `game` | Create blank games or engine examples and invoke Host simulation or native/Iris builds; see [game development](game-development.md) |

Commands under `iris test` exercise individual Vibe Mode workflows:

| Command | Preconditions and success criteria |
| --- | --- |
| `enter-recovery` | A normal application responds through ESP-Iris and reboots into Vibe Mode; wait for the same Device ID to reconnect with a new Boot ID. If already in Vibe Mode, return its current status |
| `recovery-wifi` | Requires Vibe Mode with ESP-Iris USB available; submit the Wi-Fi network name and password, then wait for connectivity |
| `bridge-code` | Requires Vibe Mode, available USB, a configured Bridge service and network connectivity; exercise the legacy Bridge maintenance pairing interface and return its code, validity period and website URL; this is separate from the current six-digit Ideas lookup |

## Ideas file releases

Prepare `mosaico-ideas.json` using `mosaico-ideas/project-release/1`, including a
valid site `project.category_id`, and build the application's `.irisfw` first.
Packaging produces an immutable ZIP with website content, firmware and
Chinese/English device presentation resources:

```sh
python mosaico.py project package --project projects/my_app --output dist/release.zip
python mosaico.py project validate dist/release.zip --json
python mosaico.py account login
python mosaico.py project upload dist/release.zip --create --yes --json
# Replace an unsubmitted draft using its cloud public_id:
python mosaico.py project upload dist/release.zip --update PUBLIC_ID --yes --json
```

Package/validate are offline. Upload takes an existing ZIP, does not rebuild it,
and synchronously creates or replaces a draft; publishing is separate. Upload has
no idempotency receipt or task polling. If the response is uncertain, inspect the
website before retrying. Ordinary releases cannot contain Vibe Mode or bootloader
components. See the [file release guide](https://github.com/esp-mosaico/esp-mosaico-utils/blob/main/mosaico-tools/docs/ideas-file-release/README.md)
for metadata, resources, authentication and server validation.

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

GSP applications bootstrap the pinned standalone GSPC compiler without requiring
the Python `gsp` module. An uncached first run needs a download; offline builds
can set `GSPC_EXECUTABLE` to an absolute path to a compatible compiler. Application
versions are GSPC 0.6.1 and ESP-GSP 1.5.1. Vibe Mode uses its own lightweight UI
and does not require GSP/GSPC. Shared tool acquisition lives in utils' `shared/gsp`;
application pins remain in `mosaico-tools/gsp-toolchain.json`.

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
in the bundle. Use `--bundle PATH` for an existing bundle. The reviewed base bundle
and ROM recovery behavior are documented in the
[Utils guide](https://github.com/esp-mosaico/esp-mosaico-utils/blob/main/mosaico-tools/README.md#rom-recovery-operations).

If the partition tables differ, `app-update` returns `partition_layout_mismatch`,
the device and build SHA-256 hashes, and a `system-update` recommendation. It does
not automatically expand the write scope or modify the project's partition table.
Reassess the update method; never reshape the intended partition layout to bypass this check.
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

When a ROM write completed but Vibe Mode acceptance timed out, continue with
`python mosaico.py iris operation-reconcile <operation-id> --project projects/my_app --json`.
This uses the original target and recorded write result, reconnects through the
Gateway's ownership checks, and verifies Vibe Mode identity, boot, firmware and OTA
readiness. It never repeats the write or reset. The original status is retained;
the new `reconciliation.outcome` reports `observed_success` only with sufficient
evidence. Missing evidence or an active writer cannot be treated as success.
If a Gateway crash left orphaned ownership, wait for the writer to finish, then
use `iris reconcile --device-id <Device-ID> --project projects/my_app` to clear
that confirmed stale claim before retrying `operation-reconcile`.

Use browser automation and CLI/Web comparison for workbench testing or an explicit
user request. See the [Gateway guide](project-gateway.md) for lifecycle, device
ownership and takeover rules.

After connection checks, use `recover` when neither the normal application nor
Vibe Mode is reachable. If manual ROM entry is required, the developer follows
the [physical entry steps](device-modes.md#physical-entry); the agent verifies
the ROM connection, resumes `recover`, and checks the device and target application. Never erase the whole flash merely to restore
connectivity, or overwrite credentials, identity, Vibe Mode data or partitions without authorization.

Do not open a UART or USB Serial/JTAG endpoint directly while the Gateway owns it.
For native development flashing over a supported UART or USB Serial/JTAG connection,
use `python mosaico.py iris flash` or `app-flash`; Gateway coordinates the endpoint
and can resume console capture. These commands do not replace product installation
and acceptance through `app-update`, `system-update` or `recover`. Vibe Mode always
assigns High-Speed USB to ESP-Iris; normal applications do so by default. A normal
application whose product function requires that interface must document the exception
and preserve ESP-Iris device operations and recovery through another available transport.

## Legacy command compatibility

Legacy entry points temporarily remain as compatibility aliases. Use the formal
entry points above in new documentation and automation:

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

`recover`, `doctor` and the workspace's `game` entry remain. Internal operation
identifiers in operation records, JSON result formats and evidence directory formats
remain unchanged. `iris status` adds the `running` field.

For GSP previews, use `python mosaico.py project sim --project projects/my_app`.
The workspace has no pre-created application, so run `project init` first. The
public implementation belongs to utils.
