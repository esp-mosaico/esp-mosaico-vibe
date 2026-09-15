# Update and Recovery diagnosis

## Classify the update

An application OTA and a system update have different failure surfaces. A system update may contain application and data components and can modify a target partition before the managed session disappears.

For every update, retain:

- operation ID and final operation status;
- bundle manifest, component kinds, target offsets, sizes, and hashes;
- Device ID and Boot ID before and after;
- current firmware mode and firmware SHA;
- the command run's `raw.log`.

## Interpret a closed session safely

`ESP-Iris session closed` is an ambiguous transport-level outcome. Do not assume rollback and do not assume success. Re-list the device and reconstruct what booted next.

If a data-plus-application update was interrupted, treat every targeted data partition as unknown until inventory or application-level validation proves it. A previously successful asset deployment does not prove those bytes survived the failed attempt.

Do not follow a failed system update with application-only OTA when the new application requires its data component, unless one of these is true:

- the required partition and content hash were verified live; or
- the application has a tested embedded/default fallback and reports that fallback explicitly.

Otherwise return to Recovery and perform a complete managed install.

## Recovery decision

Use `python mosaico.py recover` for blank/unverified devices and when neither normal nor Recovery ESP-Iris is reachable. Let it preserve available crash evidence before changing flash.

Manual ROM entry is last resort. Ask the developer to power off, hold the Boot button left of USB-C, power on, and release after ROM download mode appears. Continue with `mosaico.py recover`; do not substitute raw flashing and do not erase the whole flash.

## Verification

Success requires the same Device ID, expected normal firmware, a new Boot ID, healthy application status, and product behavior. Recovery connectivity by itself is an intermediate state.
