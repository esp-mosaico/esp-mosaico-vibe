# Crash evidence

## Evidence priority

Use the earliest durable evidence available:

1. An `ESP_ERROR_CHECK failed` or panic line with error value, source file, line, function, and expression.
2. A valid core dump plus the exact matching application ELF.
3. A retained crash report tied to the crashed Boot ID and firmware SHA.
4. Reset reason or `previous_boot_crash` without a dump.

An `ESP_ERROR_CHECK` abort can identify the root failure even when stack decoding is unavailable. Decode the `esp_err_t`, then inspect the named expression and source line before pursuing secondary register noise.

## Why a crash report may not decode

Keep these fields distinct:

- `previous_boot_crash=true` says an earlier boot ended abnormally; it does not say a core dump is present.
- `core_dump_present` says bytes were found; `core_dump_valid` says they passed validation.
- `firmware_sha_matches=false` means the currently supplied or discovered ELF is not the image that crashed. Never symbolize with a convenient but mismatched ELF.
- A Recovery session reports Recovery's current firmware identity. The crash may belong to the short-lived normal application boot immediately before it.

If the serial panic says the core dump was saved but a later query reports none, report the discrepancy rather than assuming either view is false. Possible lifecycle boundaries include another recovery/write operation, dump extraction or clearing, an invalid/incomplete stored image, or querying through a later firmware session. Inspect the preserved operation artifacts and Gateway implementation before naming one cause as proven.

## Capturing early-boot failures

Early application crashes are difficult to retain because normal firmware may enumerate, panic, reboot, and fall back to Recovery before a later monitor attaches. Start follow-mode monitoring before reproduction and retain its raw NDJSON log. Correlate records by Device ID, Boot ID, firmware mode, firmware SHA, and timestamps.

Do not rely on a large retained snapshot alone. It can mix multiple boots, include old logs, and hide the relevant tail in terminal or tool-output truncation. Search the saved `raw.log`, but keep it unfiltered as the source artifact.

## Before any mutating operation

- Save the crash report immediately.
- Export a valid core dump when the report says one exists.
- Preserve the exact build ELF and firmware SHA.
- Note whether the current session is normal or Recovery.
- Only then install, recover, or reproduce.
