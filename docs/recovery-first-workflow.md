# Retained factory recovery workflow

The factory image is the retained ESP-Iris OTA writer. Normal firmware runs
from `ota_0` and enters factory recovery before an update.

## Required application contract

- Retain `factory`, `ota_0`, the build profiles, and recovery staging from
  `projects/factory`.
- Normal builds set `CONFIG_ESP_IRIS_OTA_DEFAULT_VIA_RECOVERY=y` and disable
  `CONFIG_ESP_IRIS_OTA`.
- Compile `iris_ota_support.c` (or a reviewed equivalent) and call
  `iris_ota_support_start()` so RPC `0x7fff/2` can boot factory recovery.
- Keep the OTA writer only in the recovery image.

## Determine the live device state

Always query live identity and state:

```sh
IRIS=projects/factory/managed_components/esp_iris/tools/esp_iris.py
python "$IRIS" ctl --json devices
python "$IRIS" ctl --json status DEVICE_ID
```

## Provision factory recovery once

For a blank, missing, or unverified recovery partition, preserve available
evidence, stop the Gateway if it owns the programming port, then run:

```sh
cd projects/factory
idf.py -B build-recovery -D BUILD_PROFILE=recovery \
  --preview set-target esp32s31
idf.py -B build-recovery -D BUILD_PROFILE=recovery build
idf.py -B build-recovery -D BUILD_PROFILE=recovery \
  -p /dev/ttyACM0 flash
cd ../..
```

Use the verified port; never erase the whole flash. Restart the Gateway and
verify recovery mode, factory partition, version, OTA writer, Device ID, and
Boot ID. A build artifact alone does not prove recovery was provisioned.

## Install and update the normal application

From the repository root, install normal firmware through the Gateway:

```sh
IRIS=projects/factory/managed_components/esp_iris/tools/esp_iris.py
python "$IRIS" ctl --json devices
python "$IRIS" ctl ota DEVICE_ID projects/PROJECT/build/APP.bin \
  --elf projects/PROJECT/build/APP.elf \
  --map projects/PROJECT/build/APP.map \
  --execution-mode recovery --wait --interval 0.5
```

Replace the placeholders with live/built values. Never run the normal
application's `idf.py flash`; it can overwrite factory recovery.

## Validate the return path

1. Confirm the normal image and `ota_0` with `ctl --json status DEVICE_ID`.
2. With a valid return image ready, run `python "$IRIS" ctl factory DEVICE_ID`.
3. Confirm the same Device ID, a new Boot ID, factory recovery, and its writer.
4. Run recovery-mode OTA back to normal; confirm another Boot ID and healthy
   `ota_0` firmware.
5. Preserve the matching CLI/Workbench operation record and relevant logs.
