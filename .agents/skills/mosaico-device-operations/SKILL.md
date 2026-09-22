---
name: mosaico-device-operations
description: >
  Inspect ESP-Mosaico device state/logs, install or update applications, and
  recover devices through mosaico.py. Use for live-device diagnosis or delivery,
  not source-only, build-only or simulator-only tasks.
---

# Mosaico Device Operations

Complete the requested device operation through the workspace's `python mosaico.py`.
Follow [AGENTS.md](../../../AGENTS.md) for Recovery, transport and data-preservation
constraints. Read linked guide sections when their branch applies; command semantics
and device-state fields remain maintained in those guides.

## Establish scope and target

1. Identify the requested outcome: inspect/diagnose, install/update, or recover.
   Reuse the task's project, selected device and authorization. A status/log request
   ends with evidence and diagnosis; it does not authorize reboot, update or recovery.
   A recovery request need not include installing a different application.
2. For project-scoped operations, apply the [project-selection rules](../../../docs/mosaico-cli_CN.md#选择工程与设备).
   If the project or device is ambiguous, ask for the missing selection before
   dependent operations. Do not change configured defaults merely to run a command.
3. Inspect host ownership and discovery before connecting:

   ```sh
   python mosaico.py iris status --all --json
   python mosaico.py iris list --details --json
   ```

   These are passive queries. A discovery-only request can end here, labeling cached
   or unverified records as such. Live device claims require the next step.
4. If another project owns the device, follow [handoff rules](../../../docs/project-gateway_CN.md#跨项目协调).
   Do not kill its Gateway or clear ownership to obtain access. Interruption with
   `--force` must be within the authorized scope. A timeout does not grant ownership.
   Follow active operation/takeover records to resolution before starting another
   write; never change boards after failed explicit selection or during reconnect.
5. For a live operation, apply [device selection](../../../docs/project-gateway_CN.md#设备发现与选择)
   and [state evidence checks](../../../docs/project-gateway_CN.md#实时证据与下一步).
   After coordinating ownership, use `iris run --project <project>` or `iris claim`
   for a handshake. Confirm the live identity, Boot ID and firmware mode when Iris
   is reachable; ROM identity is established by the product recovery workflow.
   Do not infer ROM or damaged firmware from a failed handshake alone.

## Inspect and diagnose

Read the [CLI command reference](../../../docs/mosaico-cli_CN.md#命令职责) for
`iris logs`, `iris memory` and `iris crash`; choose only evidence relevant to the issue.
Use a snapshot or bounded observation unless continuous monitoring was requested.
For a debugging session across commands, follow [Gateway session usage](../../../docs/project-gateway_CN.md#开始和结束调试).
Share its Web URL when useful and match CLI/Web identity and operation records.

Report observed state separately from suspected causes. If there is no live handshake,
describe the ownership, active transition or connection evidence and what remains
unknown. Preserve available logs/core dumps; do not recover merely to obtain read-only
evidence. Finish with the diagnosis, evidence paths and any necessary next action.

## Install or update an application

1. Identify the intended project/version and whether code, partition layout or external
   resources changed. Use [update selection](../../../docs/mosaico-cli_CN.md#选择更新方式):
   `iris system-update` for new apps/layouts/resources; `iris app-update` only for
   code-only changes with an identical full partition table. Keep the intended layout.
2. Confirm affected simulator validation is complete for UI/game delivery before installation;
   retain the documented exceptions for hardware-dependent issues or missing coverage.
   Use the pinned build environment required by AGENTS.md.
   Preserve logs and valid core dumps before
   operations that could destroy them. Blank/unverified devices need provisioning first.
3. Execute the selected update through `mosaico.py` for the selected project/device.
   Let the product command manage the Recovery transition and compatibility checks.
   A layout mismatch calls for reassessing the update method, not editing partitions
   to bypass it. On disconnect or timeout, inspect the original operation record;
   do not start a competing write while its outcome is unknown.
4. Apply the linked acceptance criteria: same Device ID through normal -> Recovery ->
   normal, new Boot IDs after actual reboots, ready Recovery and healthy target firmware.
   A device starting in Recovery/ROM first needs the corresponding readiness/identity
   evidence, then the intended normal application. Verify relevant product behavior;
   an upload or reconnect alone does not complete delivery.

## Provision or recover

Use this branch for an authorized recovery or provisioning needed for the requested
installation. First distinguish ownership/connection failures and active transitions
from evidence requiring recovery, using the state checks above.

Follow [recovery entry and physical steps](../../../docs/mosaico-cli_CN.md#调试与恢复入口)
and [ROM operation tracking](../../../docs/project-gateway_CN.md#rom-恢复操作).
Use `python mosaico.py recover` for blank/unverified devices or when neither normal
nor Recovery Iris is reachable. Preserve accessible evidence before proceeding.
Manual ROM entry is the last resort: when required, give the documented physical
sequence, wait for the developer to complete it, then detect and verify the connection
and resume the product command. Never substitute direct flashing or whole-flash erase.

Verify the selected hardware identity, expected Recovery version and OTA capability;
return operations to the Iris Gateway when reachable. Install and verify the intended
application if delivery is part of the task. Otherwise report Recovery readiness and
the remaining application state without claiming that an application was delivered.

## Close the operation

Report the verified Device ID/Boot ID and firmware state when available, what was done,
the operation/takeover ID if present, evidence/log paths, and remaining validation gaps.
If blocked by ambiguity, ownership, physical actions or an unresolved write, name that
condition without guessing success or repeatedly starting recovery. Finish only this
task's monitoring client; do not release a shared device still needed by another client.
