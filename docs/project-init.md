# Project creation and selection

[简体中文](project-init_CN.md) | [Documentation index](README.md)

A fresh workspace has no pre-created applications. Use Python 3.10+ and initialize
only utils to create one:

```sh
git submodule update --init submodule/esp-mosaico-utils
python mosaico.py project init my_app --dry-run
python mosaico.py project init my_app
```

The template belongs to `esp-mosaico-utils/mosaico-tools/templates/hello_world/`.
New projects are generated in `projects/my_app` by default. Creation requires no
BSP, engine, ESP-IDF or device, and does not start a Gateway. On failure, it removes
files written by that creation attempt. It refuses to overwrite an existing target
and does not automatically change the default project.

In `.mosaico.json`, `workspace.init_template` can point to a custom descriptor.
`workspace.projects_dir` can change the output location, which must remain within
the workspace. See [application integration](../submodule/esp-mosaico-utils/mosaico-tools/docs/application-integration.md)
for the template format and path variables.

Project selection follows this order: explicit `--project`, the current project,
a valid user-configured default project, then the sole created project. With no
projects, the CLI prompts you to create one; with multiple projects, it requires
a selection. Internal Recovery is never a candidate. Explicit `--project` paths
are resolved relative to the calling directory. You can invoke the entry point
from any nested directory in the workspace, or select a workspace with
`--workspace PATH`.

Initialize BSP and prepare ESP-IDF `master` at the fixed commit
`7b9cc1ac79f865983f59bb8ff3ff43eb74ff1dbe` for target `esp32s31`. Build the
generated project, then run the preview below. Verify the full SHA with
`git -C "$IDF_PATH" rev-parse HEAD`; do not follow the latest `master`:

```sh
python mosaico.py project sim --project projects/my_app --interactive
```

Prefer GSP for UI applications. After design confirmation, use the simulator to
find and fix layout, clipping, resource display, click feedback, page transitions,
timers and state changes. Re-run the affected flows before device validation.
Simulation must run the native C UI and interaction logic shared with the device;
static screenshots do not replace interaction checks. Start diagnosis on hardware
only for issues that depend on real hardware or behavior the simulator does not
cover, and state the remaining validation gaps.

After simulator validation, install through the product CLI and verify the physical
display, input and hardware interactions:

```sh
python mosaico.py recover
python mosaico.py iris system-update --project projects/my_app
```

Use `recover` before the first installation on a blank or unverified device.
Use `system-update` for a new application, layout changes or external resource
changes. Use `app-update` only for code changes with an identical full partition
table and unchanged resources.

Generated source files do not contain absolute paths from the development machine.
After moving or cloning the entire workspace, initialize the pinned dependencies
and rebuild. Old build caches are not portable artifacts.
