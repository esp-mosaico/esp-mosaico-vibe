# Project creation and selection

[简体中文](project-init_CN.md) | [Documentation index](README.md)

A fresh workspace has no pre-created applications. This workspace requires Python 3.10+
and Git 2.28+, even if individual public tools support older versions. Initialize only
utils to create an application:

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

The workspace `.mosaico.json` uses `schema_version: 2`; template descriptors
retain schema 1. See [migration](workspace-migration.md) for existing workspaces.
`workspace.init_template` can point to a custom descriptor.
`workspace.projects_dir` can change the output location, which must remain within
the workspace. See [application integration](https://github.com/esp-mosaico/esp-mosaico-utils/blob/f58f4f9e5363f908b17a8e1ea8fdf4dd77561ef1/mosaico-tools/docs/application-integration.md)
for the template format and path variables.

Creation initializes an independent Git repository on `main`, supplements missing
Git/editor/quality configuration, and preserves template-owned settings. It does
not stage files, commit, configure a remote or install hooks. From the new project,
follow `tools/quality/README.md` to install and enable the pinned checks. The shared
checker stays in utils; the project contains its policy and relative references.

The Hello World layout separates responsibilities:

| Path | Purpose |
| --- | --- |
| `main/main.c` | Device startup and dependency composition |
| `components/app_hello/` | Portable C UI controller and public API |
| `components/app_hello/ui/` | Authored scene and fonts |
| `components/app_hello/host/` | Native desktop adapter using the same controller |
| `components/app_display/` | BSP display and touch policy |

`project sim` discovers the component's scene and Host backend. Older root
`ui/main.json` and `pc/` layouts remain supported. With multiple scenes use
`--scene PATH`, but this selects scene-only rendering without C callbacks;
omit it when validating application behavior.

Project selection follows this order: explicit `--project`, the current project,
a valid user-configured default project, then the sole created project. With no
projects, the CLI prompts you to create one; with multiple projects, it requires
a selection. Application selection excludes the Recovery staging project. Explicit `--project` paths
are resolved relative to the calling directory. You can invoke the entry point
from any nested directory in the workspace, or select a workspace with
`--workspace PATH`.

## First native preview

Activate ESP-IDF at exactly `7b9cc1ac79f865983f59bb8ff3ff43eb74ff1dbe` for
`esp32s31`; do not follow the latest `master`. From a fresh workspace root,
the complete sequence is below. Skip creation if `my_app` already exists:

```sh
git submodule update --init submodule/esp-mosaico-utils submodule/esp-mosaico-bsp
python mosaico.py project init my_app
git -C "$IDF_PATH" rev-parse HEAD
idf.py -C projects/my_app -D IDF_TARGET=esp32s31 reconfigure
python mosaico.py project sim --project projects/my_app --interactive
```

`reconfigure` resolves the pinned ESP-GSP 1.5.1 and other component dependencies;
it does not compile the firmware. The preview builds the application's native C
backend in `build-sim/` and obtains the matching GSP tools automatically. A host C
compiler, CMake and network access for uncached downloads are required. A full
firmware build is needed for device delivery, not for this first preview.
For a bounded smoke run, replace `--interactive` with `--headless --duration 3`.
This runs C callbacks; scene-only rendering is not equivalent.
For local/SSH browser access, see [preview troubleshooting](simulator-preview.md).

Prefer GSP for UI applications. After design confirmation, use the simulator to
find and fix layout, clipping, resource display, click feedback, page transitions,
timers and state changes. Re-run the affected flows before device validation.
Simulation must run the native C UI and interaction logic shared with the device;
static screenshots do not replace interaction checks. Start diagnosis on hardware
only for issues that depend on real hardware or behavior the simulator does not
cover, and state the remaining validation gaps.

After simulator validation, install through the product CLI and verify the physical
display, input and hardware interactions. The current Hello World template provides
Iris screenshots but does not register the optional pointer RPC; verify touch on
the physical screen. Generic RPC/input capabilities do not establish that a pointer
handler exists:

```sh
python mosaico.py recover
python mosaico.py iris system-update --project projects/my_app
```

Use `recover` before the first installation on a blank or unverified device;
it writes base firmware. To enter existing Vibe Mode, see [device modes](device-modes.md).
Use `system-update` for a new application, layout changes or external resource
changes. Use `app-update` only for code changes with an identical full partition
table and unchanged resources.

The generated Hello World sources do not contain absolute development-machine paths.
After moving or cloning the entire workspace, initialize the pinned dependencies
and rebuild. Old build caches are not portable artifacts.
