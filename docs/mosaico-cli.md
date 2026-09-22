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
│   ├── app-update
│   ├── system-update
│   └── test enter-recovery / recovery-wifi / bridge-code
├── recover
└── game create/new/sim/run/build (BSP examples and engine Host)
```

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
| `iris app-update` | Update normal application code only; requires the complete partition table to match the device |
| `iris system-update` | Recommended entry for a new application, partition layout changes or resource changes; writes according to the update bundle manifest |
| `recover` | Provision or restore the device's base firmware, including when ESP-Iris is unreachable |
| `game` | Create games from BSP examples and invoke engine Host simulation or builds; see [game development](game-development.md) |

Commands under `iris test` exercise individual Recovery workflows:

| Command | Preconditions and success criteria |
| --- | --- |
| `enter-recovery` | A normal application responds through ESP-Iris and reboots into retained Recovery; wait for the same Device ID to reconnect with a new Boot ID. If already in Recovery, return its current status |
| `recovery-wifi` | Requires Recovery with ESP-Iris USB available; submit the Wi-Fi network name and password, then wait for connectivity |
| `bridge-code` | Requires Recovery, available USB, a configured Bridge service and network connectivity; open the device download page and return the pairing code, validity period and Bridge website URL |

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
| Blank or unverified device, or neither normal nor Recovery is reachable | `python mosaico.py recover` |
| New application, partition layout changes or external resource changes | `python mosaico.py iris system-update --project projects/my_app` |
| Application code changes only, with the complete partition table matching the device | `python mosaico.py iris app-update --project projects/my_app` |

`recover` prepares the reviewed base firmware and verifies that Recovery is ready.
The target application still needs to be installed and accepted afterward. It
supports only a local Gateway and also manages the underlying recovery process.

A `system-update` bundle built from a project contains the application, partition
table and resource images declared by the project. It preserves the fixed Recovery
prefix and bootloader. A reserved but unused `game_assets` partition needs no image;
applications using external resources declare their images through CMake for inclusion
in the bundle. Use `--bundle PATH` for an existing bundle. See the
[Recovery guide](../submodule/esp-mosaico-utils/esp-mosaico-recovery/firmware/recovery/README.md)
for updates to Recovery itself, HTTP(S)/NAND updates and base bundle constraints.

If the partition tables differ, `app-update` returns `partition_layout_mismatch`,
the device and build SHA-256 hashes, and a `system-update` recommendation. It does
not automatically expand the write scope or modify the project's partition table.
The effective build configuration, including with `--skip-build`, and the updated
application must pass role, product, board, layout contract and Recovery ABI checks.

Before updating, use the product tools to preserve valid core dumps, structured
evidence and raw logs. Acceptance requires the same Device ID to complete normal →
Recovery → normal with new Boot IDs, run the target firmware, report healthy, and
exhibit the intended product behavior. For a blank device, verify Recovery readiness
before installing the application. Upload completion or reconnection alone does not
establish acceptance.

## Debugging and recovery entry points

Run `python mosaico.py iris run --project projects/my_app` and open the printed URL
to observe the Gateway Web workbench continuously. See the
[Gateway guide](project-gateway.md) for lifecycle, device ownership and takeover
rules. The CLI and workbench should show the same Device ID, Boot ID and operation
records.

Continue to use `recover` when both normal and Recovery are unreachable. Only when
the command requires manual ROM entry should the developer perform these steps:

1. Power off the device.
2. Press and hold the Boot button to the left of the USB-C port.
3. Power on the device while continuing to hold Boot.
4. Release Boot after entering ROM download mode, then tell the agent that the
   physical steps are complete.

The agent then detects the recovery connection, continues `recover`, and verifies
the device and target application. Never erase the whole flash merely to restore
connectivity, or overwrite credentials, identity or Recovery data without authorization.

Avoid USB Serial/JTAG for application flashing and monitoring. Do not open the
interface concurrently while the Gateway owns it. High-Speed USB belongs to ESP-Iris
by default. A normal application whose product function requires that interface
must document the exception and preserve ESP-Iris device operations and recovery
through another available transport.

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
