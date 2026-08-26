# esp-mosaico example

The project builds three firmware profiles in independent build directories:
`application`, `candidate`, and `recovery`.

Retain the factory recovery image and enter-recovery RPC. See the
[recovery-first workflow](../../docs/recovery-first-workflow.md).

## Recovery image source

Application and candidate builds prepare a recovery artifact at
`<build-dir>/recovery/iris_get_started.bin`. By default they validate and copy
the checked-in image, so recovery is not compiled during normal development:

```sh
idf.py -B build-app \
    -D BUILD_PROFILE=application \
    build
```

The source is configured through `sdkconfig`. Run menuconfig for the active
build directory and select `Get Started OTA -> Recovery image source`:

```sh
idf.py -B build-app menuconfig
```

The choices are `Use checked-in prebuilt recovery image` (the default) and
`Build recovery from current source`. The selection is stored in
`build-app/sdkconfig`. A missing, modified, oversized, or incompatible
prebuilt image is a hard build error; there is no silent fallback to compiling
recovery.

To publish a fresh recovery build back into the repository, configure any
non-recovery profile and run the explicit update target:

```sh
idf.py -B build-app \
    -D BUILD_PROFILE=application \
    update-recovery-prebuilt
```

The checked-in `manifest.json` records the image hash, target, factory
partition geometry, ESP-IDF version, source revision, and security settings.
The normal `flash` target is intentionally unchanged: the recovery artifact is
prepared for packaging or provisioning, but is not automatically written over
the application image.

Provision `BUILD_PROFILE=recovery` once. Never run the normal `flash` target;
use ESP-Iris recovery-mode OTA to install normal images in `ota_0`.
