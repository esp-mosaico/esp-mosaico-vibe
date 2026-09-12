---
name: mosaico-ui
description: >
  Design, implement, and improve ESP-Mosaico device UI pages and applications
  through design confirmation, simulator validation, and device validation.
  Use for new UI, visual or interaction changes, and UI performance issues.
  Not for browser applications or Gateway Web workbench UI.
---

# Mosaico UI

Use three feedback loops to resolve user intent, executable behavior, and
hardware experience. Choose the entry point from what remains uncertain;
new and existing applications share the same workflow.

## Choose the entry point

Identify the target page, the user's goal, and any existing design or runtime
evidence. For an existing UI, inspect its implementation and reproduce the
relevant state or input sequence when possible. Preserve its project,
framework, and conventions unless the requested change requires otherwise.

- **Design confirmation:** the overall appearance or intended interaction is
  unsettled, as in a new UI or a substantial redesign.
- **Simulator validation:** the design is already confirmed, the requested
  visual change is specific, or a UI logic defect needs correction.
- **Device validation:** real input, data, display, or performance behavior
  must be observed on the target hardware to understand the problem.

Continue through the affected downstream loops within the requested scope.
Design-only or review-only work does not authorize implementation or device
writes. Complete device delivery needs device evidence; a simulator-only task
can end with its runnable implementation and clearly stated hardware gaps.

## 1. Design confirmation

**Question:** What should the user see, and how should they operate it?

Understand the main task and display/input constraints well enough to propose
a design. Inspect repository capabilities for feasibility, then prefer an
early rendered mockup of the complete primary screen at the target size.
Show it to the user; use available image tools or their visual reference.
If rendering is unavailable, explain the limitation and offer a static visual
alternative. Do not start application code just to obtain the first picture.

Iterate from the user's feedback on layout, style, copy, control states,
navigation, and feedback. Change the image for visual disagreements, use a
short scenario or screen sequence for interaction questions, and inspect
component sources for technical questions. Add detail where it helps resolve
a consequential choice, rather than requiring an exhaustive questionnaire.

This loop defines **interaction meaning**: what an action does, which state
follows, how to return, and how relevant failures are communicated. Routine
visual adjustments and code structure can be resolved during implementation;
runtime uncertainty needs later evidence.

**Output:** a user-confirmed design baseline: the current visual and a concise
account of the purpose and main interactions. Begin production UI code, scene
JSON, and application scaffolding after confirmation of the overall style,
layout, and main interactions. Until then, work on inspection and design
artifacts. Existing confirmation counts; a specific color or spacing request
can proceed directly to loop 2. Silence or a general request to build a UI
does not confirm an unsettled design proposal.

## 2. Simulator validation

**Question:** Can the design become a complete, executable UI?

Implement or modify the real UI and logic, run them, inspect the result, and
revise. For GSP, use GSP_SIM with the shared implementation that will continue
onto the device, rather than a separate demonstration prototype.

This loop implements **interaction behavior**: event handling, state
transitions, data bindings, and screen updates. Exercise relevant complete
flows with simulated input and data, including meaningful failure cases.
Compare the running result with the confirmed design or requested change.

Match evidence to the issue: comparable captures for visual changes, input
and state sequences for interaction, and running updates for timers or
bindings. Static screenshots do not establish executable behavior. Inspect
other consumers when changing shared styles, and add logic regression tests
when warranted rather than a test suite for each cosmetic edit.

**Output:** a runnable UI implementation baseline, with native visual evidence
and exercised interactions consistent with the design. State which device
behaviors remain simulated. UI logic defects stay here; ambiguity about the
intended experience returns to loop 1.

## 3. Device validation

**Question:** Is the UI correct, responsive, and usable on the target device?

Deploy through the repository's device workflow, operate the real controls,
and iterate from device captures, runtime measurements, and user experience.
Verify real data and asynchronous events as well as rendering, relevant
resource use, performance, and stability under the intended usage.

This loop validates **actual interaction experience**: physical buttons,
touch, response times, and concurrent device events. Confirm the device runs
the intended program and UI resources. Compare captures in equivalent states
and performance under comparable load; successful upload or simulator results
alone do not establish device acceptance.

**Output:** a device version meeting the agreed behavior and relevant operating
requirements, supported by captures, logs, measurements, or observed input
results. If hardware is unavailable, report completed work and outstanding
device checks rather than declaring this loop complete.

## Route feedback to the responsible loop

Fix a problem where it belongs: visual intent or interaction meaning in loop 1,
UI logic in loop 2, hardware integration and device behavior in loop 3. A state
bug found on-device should be reproduced in simulation; a control obscured by
the user's finger may require a design change.

Confirm only changes to the agreed design, then repeat the affected downstream
checks. Internal fixes do not need a new design gate or a full workflow
restart. Keep the baseline and evidence current, explain changes and remaining
gaps, and show before/after evidence when useful. Save requested user-facing
documents under `docs/`. Scale artifacts to the task without mandatory forms,
image counts, or approval for decorative details.

## Repository execution support

- Follow [AGENTS.md](../../AGENTS.md) and the [skill index](../README.md).
  Preserve the Recovery contract and use `mosaico.py` for device operations,
  including live identity and evidence checks. Resolve the ESP-IDF environment
  before running its tools.
- For new apps, prefer GSP when it fits and start from
  [gsp_hello](../../projects/gsp_hello/README.md). New apps belong in `projects/`;
  a new page within an existing app inherits its framework and conventions.
- Load [gsp-sim](../gsp-sim/SKILL.md) for GSP runtime constraints, simulator
  commands, and the portable UI boundary. Run the target app's backend for
  logic, not scene-only rendering. Keep controls and dynamic text native
  rather than substituting the concept image for the working UI.
- For LVGL, use [hello_world](../../projects/hello_world/README.md) as the
  normal-app reference and a matching native preview where available. GSP_SIM
  does not run LVGL. If host execution is unavailable, exercise those behaviors
  on-device and disclose the simulator gap; do not migrate frameworks just to
  obtain a preview or treat a browser recreation as native evidence.
- Use [idf-low-noise-build](../idf-low-noise-build/SKILL.md) for firmware builds.
  Consult the selected project's update documentation for separately packaged
  scenes, fonts, and images so device checks cannot silently use stale assets.
