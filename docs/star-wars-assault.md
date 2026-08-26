# Star Wars: Mosaico Assault

`projects/star-wars-assault` is a single-player arcade shooter for the
ESP-Mosaico 480×480 touch display.

## Controls

- Touch `<` and `>` to move the X-Wing.
- Touch and hold `FIRE`, or hold the on-board **AI** button, to fire.
- Stop TIE fighters before they cross the Rebel line. The mission ends after
  three shield hits.
- Touch `FIRE` or press **AI** to start and to replay after a mission.

## Development workflow

Build with the workspace ESP-IDF environment:

```sh
. /home/lishenhang/esp/idf-gitlab/esp-idf-master-1/export.sh
idf.py -C projects/star-wars-assault --preview set-target esp32s31
idf.py -C projects/star-wars-assault build
```

Routine device updates, logs, restart, recovery, and validation go through the
ESP-Iris Developer Gateway. Open the local Workbench at
<http://127.0.0.1:8443/> to observe the same device and operation records used
by the CLI.

## First provisioning of a blank device

A blank board has no ESP-Iris firmware for the Gateway to contact. Build and
provision the retained recovery profile once. This selective `flash` writes
the recovery image to the factory partition; it does not place the game there.

```sh
. /home/lishenhang/esp/idf-gitlab/esp-idf-master-1/export.sh
cd projects/get-started
idf.py -B build-recovery -D BUILD_PROFILE=recovery \
  --preview set-target esp32s31
idf.py -B build-recovery -D BUILD_PROFILE=recovery build
idf.py -B build-recovery -D BUILD_PROFILE=recovery \
  -p /dev/ttyACM0 flash
```

This writes only the bootloader, partition table, initial OTA metadata, and
factory recovery image. It does not erase the whole flash. Once recovery
appears in the Gateway, install `build/star_wars_mosaico.bin` through the
Gateway OTA workflow so the game runs from `ota_0` while recovery remains
available.

Bind the Gateway to the stable device node rather than a product-name-based
`by-id` path during the first OTA. The USB product name changes from recovery
to the game after restart:

```sh
python managed_components/esp_iris/tools/esp_iris.py web \
  --usb /dev/ttyACM0 --listen 127.0.0.1 --port 8443
```

From another terminal at the repository root, obtain the live Device ID with
`ctl devices`, then perform the recovery-writer OTA:

```sh
IRIS=projects/get-started/managed_components/esp_iris/tools/esp_iris.py
python "$IRIS" ctl devices
python "$IRIS" ctl ota \
  --elf projects/star-wars-assault/build/star_wars_mosaico.elf \
  --map projects/star-wars-assault/build/star_wars_mosaico.map \
  --execution-mode recovery --wait --interval 0.5 \
  DEVICE_ID projects/star-wars-assault/build/star_wars_mosaico.bin
```

After restart, verify that `ctl status DEVICE_ID` reports
`project_name=star_wars_mosaico` and that its firmware SHA-256 matches the
built ELF. Do not run `idf.py flash` from the game project: routine game
updates belong in `ota_0`, while the factory partition is reserved for the
recovery image.
