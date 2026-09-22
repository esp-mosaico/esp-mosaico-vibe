# Project Gateways, clients and device ownership

[简体中文](project-gateway_CN.md) | [Documentation index](README.md)

The same OS user, workspace and application path share one Gateway. Different
projects have separate addresses, logs and operation records, and coordinate device
ownership through a user-level registry and operating-system locks. This information
describes host project sessions; device identity and Boot ID still require live
handshakes and queries.

## Start and end debugging

```sh
python mosaico.py iris run --project projects/my_app
```

Each `iris run` registers an independent client and stays in the foreground even
when a Gateway already exists. Open the printed URL to access the Web workbench.
Device commands from other terminals or agents automatically reuse the same project's
instance. Ordinary commands retain a client while running, including application
builds, installation and reconnection waits. The workbench's event connection holds
one client; multiple panels within the page do not register additional clients.

Ctrl+C ends the current `iris run` client without closing a Gateway used by others.
Ordinary commands likewise release only their own client when they finish. Once all
clients have left and all active requests, updates, device Jobs, host operations and
stream tasks have ended, the Gateway exits after **10 idle seconds**. New clients or
work cancel the countdown. There is no separate stop command, and device ownership
alone does not keep the Gateway alive indefinitely.

The CLI renews its client every 5 seconds. After 20 seconds without renewal, the
client expires, followed by the 10-second idle wait. Workbench connections use
WebSocket heartbeats to track liveness. Refreshes and brief disconnections do not
immediately end the Gateway. Closing a client does not cancel a submitted device
write; processing continues in the background and preserves the operation result.

After the Gateway exits, the workbench cannot wake it automatically. Run `iris run`
or a device command again and use the newly printed workbench URL. The Gateway uses
a dynamic port, so a previous address is not guaranteed to remain valid.

## Check who is using a device

```sh
python mosaico.py iris status --project projects/my_app
python mosaico.py iris status --all
python mosaico.py iris status --all --json
```

These queries do not start a Gateway, install the host environment, register a
client or extend the idle countdown. `--all` and `--project` are mutually exclusive.
You can query other workspaces even when the current project has no Gateway.

Each instance reports its project and workspace paths, URL, source version, session
ID, owned devices, clients, keepalive reasons and idle countdown. Client information
includes type, command, PID where applicable, connection time and latest keepalive.
Credentials and command arguments containing passwords are not displayed, and
process identifiers are not interpreted as people's real names.

In JSON, `running` indicates whether the session process lock is still held;
`reachable` indicates whether the HTTP query succeeded. `state` can be `running`,
`idle`, `draining`, `unreachable`, `legacy` or `orphaned`. `lifecycle.clients`
lists clients; `lifecycle.keepalive` lists work that requires the Gateway to remain
running. With no instance, a single-project query returns
`{"running": false, "session": null}`; retained ownership is listed separately.

`device_ids` deduplicates devices by identity. Endpoints whose identities have not
been verified are listed separately. Ownership can persist across device reboots
and disconnections, so ownership does not imply that a device is online. Old registry
sessions do not prove the device's current identity.

## Device discovery and selection

`iris list` passively enumerates candidate endpoints and known devices without
claiming them. Discovery records may contain cached, offline devices. The actual
Device ID, Boot ID and online state must be verified by a handshake at operation time.

```sh
python mosaico.py iris list --project projects/my_app
python mosaico.py iris claim --project projects/my_app --endpoint '<discovered-endpoint>'
python mosaico.py iris logs --project projects/my_app --device-id '<Device-ID>'
```

For a single device, selectors can be omitted. Selection follows this order:

1. Explicit `--device-id` / `--endpoint` takes priority. Failure does not select
   another device.
2. Use the current project's sole connected device.
3. If none is connected, follow the current project's sole existing ownership,
   including waiting for an offline device to reconnect.
4. With no existing ownership, enumerate local USB devices live and claim the sole
   available candidate. Cached or disconnected TCP/mDNS endpoints, ROM interfaces
   and USB Serial/JTAG interfaces do not participate in this automatic selection.
5. With multiple candidates, list them and require an explicit target. Ownership by
   another session, takeover reservations and orphaned ownership are never preempted
   or cleared automatically. Once the handshake verifies a Device ID, the operation
   continues to follow that identity.

`iris run` attempts automatic connection only once, when the session is first created.
With no device or an ambiguous selection, the Gateway remains running; later device
commands or `iris claim` initiate a connection. A deliberately released device is
not reclaimed in the background. In a shared session, `iris claim` can omit device
arguments. `iris release` automatically releases the sole owned device, even if it
is temporarily offline. USB/TCP ownership for the same device is counted together.
`iris takeover start` requires an explicit `--device-id` or `--endpoint`; retries
reuse the same `--takeover-id`. `reconcile` and commands for takeover records still
require an explicit target or record ID.

`recover` first tries to connect the sole ESP-Iris device. Only when no available
target is found does it continue with ROM interface detection. If a target is owned,
selection is ambiguous, or connection to an existing target fails, recovery does not
switch to another board. Explicit `--hardware-mac` retains the hardware-identity
selection workflow.

`iris list` / `iris status` still do not claim devices. `--gateway-profile` operates
only on devices already connected to the specified external Gateway; it does not
automatically claim USB devices from the current computer.

`--device-id` can select a device on its own. The Gateway first reuses a verified
connection; otherwise, it tries online USB before verifying other candidate endpoints.
The HELLO identity must match, and a failed new connection releases the ownership
acquired by that attempt. `--endpoint` is strict: it does not preempt another
workspace's ownership. Candidate connection retries occur only before a write is
submitted.

When upgrading from a legacy random Device ID to an identity derived from the eFuse
Base MAC, refresh saved selectors with `iris list`. Operation history for the old ID
is retained. Normal and Recovery firmware should use compatible identity rules.

## Coordination across projects

The shared registry is `esp-mosaico/ownership/ownership.sqlite3` under the current
user's state directory. Linux uses `XDG_STATE_HOME`, or the user's `.local/state`
when unset; macOS uses Application Support; Windows uses LOCALAPPDATA. Different
users, hosts or isolated state directories do not share this coordination view.
Independent or remote Gateways do not participate automatically.

A device belongs to one project session at a time. Other projects cannot preempt it
automatically, and an HTTP timeout does not prove that the owner has died. Project
locks prevent duplicate instances, session locks determine liveness, and endpoint
locks protect physical connections.

The receiving project can request takeover. The command starts or reuses the
receiver's Gateway:

```sh
python mosaico.py iris takeover start --project projects/my_app --endpoint /dev/ttyACM0
python mosaico.py iris takeover start --project projects/my_app --device-id '<Device-ID>' --force --timeout 120
```

By default, only idle devices are handed off. A busy response identifies the active
operations, mirrors or Jobs. `--force` prevents the old Gateway from accepting new
operations for that device, cancels operations that have not started, waits for the
current write to finish safely, then stops screen, image and audio mirrors and
requests cancellation of background Jobs. Handoff occurs only after termination
is confirmed. Other devices and clients on the old Gateway continue running.
An open Web log view or a retained `iris run` client alone does not block handoff.

A timeout preserves the original ownership and restores acceptance of new requests.
Stopped mirrors and canceled tasks are not restarted automatically, and in-progress
writes are not interrupted. The command prints a takeover ID. After a communication
failure, query it first:

```sh
python mosaico.py iris takeover status --project projects/my_app --takeover-id '<ID>'
python mosaico.py iris takeover resume --project projects/my_app --takeover-id '<ID>'
```

`status` is passive and does not start a Gateway. `resume` continues identity
verification in the original receiving session. If no reservation record was created,
run `start` again with the same ID. Active requests keep the receiving Gateway alive
until verification ends, and reservations protect the interval between disconnect
and reconnect. Without an active request, the record itself does not keep a Gateway
alive indefinitely.

To withdraw an unfinished handoff, run `iris takeover abort --takeover-id ...` in
the **original owning project**. A completed takeover cannot be rolled back.
An abort also cannot be forced while the receiving session is still alive.
If both original sessions have exited, either participating project can run
`iris takeover reconcile --takeover-id ...` to check the port lock and reclaim the
reservation. Ordinary ownership left after a crash still uses `iris reconcile`.

Takeover coordinates only local project sessions under the same OS user using the
new tools. The CLI's `iris transfer` command group, its legacy aliases and the old
HTTP transfer endpoints have been removed. The workbench offers takeover only.
A Gateway restart never automatically replays device writes.

## Common issues

| Symptom | Action |
| --- | --- |
| An old workbench address no longer opens | Retain a session with `iris run --project ...` and open the newly printed URL |
| Another project owns the device | Find the owner with `iris status --all`, then run `iris takeover start` from the receiving project without terminating other clients |
| An explicitly selected device is offline | Wait for that identity to reconnect and check the connection; do not select another device after failure |
| Ordinary ownership remains after a crash | Confirm the original session is no longer alive, then run `iris reconcile`; use `iris takeover` to query and recover takeover records |
| An updated application is unresponsive | Preserve logs and valid core dumps, then follow the [CLI recovery entry](mosaico-cli.md#debugging-and-recovery-entry-points) |

## Legacy instances and external Gateways

Legacy temporary or persistent instances can still be queried. If client details
are unavailable, this is reported explicitly. New device commands require compatible
APIs and shared lifecycle capabilities. End an incompatible instance from the terminal
that originally held it; the tools do not automatically terminate another user's session.

The shared registry retains its old table layout and adds tables for new metadata,
allowing older tools in other workspaces to coexist. With `--gateway-profile`,
lifecycle remains externally managed. Connection failure does not fall back to
a local instance.

## Component boundaries and version policy

The host product tools live in `submodule/esp-mosaico-utils/mosaico-tools`. Recovery
firmware, reviewed images and the shared persistent ABI belong to `esp-mosaico-recovery`.
Legacy tool entry points retain forwarding compatibility. The CLI queries local
state through the public ESP-Iris host API, without reading its SQLite tables or
private lock structures.

By default, instance reuse depends on the Gateway API and capabilities required by
the command, not on matching the entire tools repository's Git commit. To pin the
actual running source, add `"gateway": {"source_policy": "exact"}` to `.mosaico.json`.
This mode fingerprints Python sources, dependency locks and workbench build artifacts,
including uncommitted changes. A mismatch raises an error without terminating other
users' Gateways.

The product CLI submits the required Recovery version and partition hash to the
Gateway. Within one operation, the Gateway performs the transition, reconnection,
validation, writing and health verification. Failed validation prevents writing.
See [component boundaries](../submodule/esp-mosaico-utils/docs/component-boundaries.md)
for detailed responsibilities and interfaces.

## Device state and ROM recovery

Devices expose five states: offline, connecting, idle, busy and needs recovery.
Project ownership and firmware mode (Normal / Recovery / ROM / unknown) are shown
separately. Log pages and client keepalives do not make a device busy. Mirroring,
background Jobs and active operations report specific reasons for being busy.

### Live evidence and next steps

Before device operations, inspect host ownership and passive discovery results:

```sh
python mosaico.py iris status --all --json
python mosaico.py iris list --details --json
```

After coordinating ownership, establish a live handshake with
`iris run --project <project>` or `iris claim`. Open the Gateway Web workbench URL
printed by the command, confirm that `GET /v1/devices/<device-id>` returns
`stale=false`, and match Device ID and Boot ID against CLI evidence. If the device
is offline or the handshake failed, old query results do not establish its current state.

The following table describes decisions for different situations. Check firmware
mode, connection state and active operations separately; these situations do not
form a single state enumeration:

| Situation | Required evidence and next step |
| --- | --- |
| Normal application | Live `firmware_mode=normal`, Device ID, Boot ID, and expected project/version. Installation acceptance also requires healthy status and the intended product behavior |
| Recovery | Live `firmware_mode=recovery` with the same Device ID. Before updating, confirm the expected Recovery version and `ota` in `capability_names`; a USB reconnect alone is insufficient |
| Updating, rebooting or handing off | Inspect the active operation or takeover record, follow that record and the selected Device ID, and wait for completion. Do not repeat writes or select another board |
| ROM download | The `mosaico.py recover` workflow confirms a live ROM endpoint. No ESP-Iris handshake or Boot ID is available; verify hardware identity before provisioning |
| Offline or unknown | No successful live handshake, or only cached discovery/ownership. Check the owner, active transitions and connection first; this does not establish ROM mode, blank flash or damaged hardware |

Gateway `running` / `reachable` describes the host process, not device health.
Ownership can persist after disconnection. After an actual reboot, require the same
Device ID and a new Boot ID. Never determine firmware mode from a port name or
screen alone. Before recovery that could destroy evidence, use the product tools
to save raw logs, structured evidence and valid core dumps. Full update acceptance
criteria are in [CLI update selection](mosaico-cli.md#select-an-update-method).

### ROM recovery operations

After preparing the firmware, `python mosaico.py recover` submits one ROM recovery
operation through the Gateway. The operation covers exclusive port access, evidence
preservation, ROM flashing and reconnection verification, using the normal operation
records and query interfaces. The executing process holds the physical port lock;
closing the CLI or an unexpected Gateway exit does not prematurely release a port
whose flashing operation is still running. If a wait times out, check progress using
the printed operation ID instead of automatically retrying the write.

There is no separate maintenance state, maintenance lease or renewal interface.
After an operation, results and logs are retained and temporary ownership is released.
Expected disconnections during flashing still appear as busy. Only live device
evidence can establish that recovery is needed; an ordinary communication timeout
does not imply damaged firmware. Legacy lease workflows are no longer compatible;
use the new host tools consistently.
