# Reusing Vibe Mode settings in applications

[简体中文](vibe-settings_CN.md) | [Documentation index](README.md)

Persisted Vibe Mode preferences are not automatically applied by another firmware
application. Select the settings the app needs and read them in its device adapter;
keep NVS and Wi-Fi driver access out of portable game or UI models.

## Current storage and ownership

These are the current utils implementation details, not a new stable public API.
Recheck types and defaults when upgrading utils. Prefer an existing public reader;
otherwise centralize read-only access in the app adapter. The partition comes from
`MOSAICO_SYSMETA_PARTITION` in `mosaico_recovery_contract.h`, currently `sysmeta`.

| Setting | Namespace / key | Type and meaning |
| --- | --- | --- |
| Wi-Fi credentials | `shared` / `wifi_ssid`, `wifi_password` | NVS strings; validate the pair and lengths; open networks can have an empty password |
| Brightness | `vibe_prefs` / `brightness` | `u32`, currently 20–100, fallback 80; panel percentage, not software RGB gain |
| Language | `vibe_prefs` / `lang` | `u32`, 1 English, 2 Simplified Chinese; Vibe migrates legacy `settings/language`; apps should not duplicate that write migration |
| Screen timeout | `vibe_prefs` / `timeout` | `u32` seconds, 0 disables automatic sleep; current upper bound 600, fallback 0 |
| Rotation, haptics | `vibe_prefs` / `rotate`, `haptics` | `u32` booleans; currently default off and on respectively |
| Custom shared entries | `shared` / user keys | NVS strings; read only keys the app needs |

`open_confirm` and `confirm_cnt` govern Vibe's application launch confirmation,
not ordinary in-app interactions. Vibe's network service migrates legacy `wifi`
credentials to `shared`; apps should not reverse the migration or create another
global credential store. Stored credentials do not prove a live connection;
obtain connection state from the app's own network service.

## Application adapter

- Initialize the existing NVS partition as needed and open namespaces with
  `NVS_READONLY`. Handle initialization/read errors, close handles and apply the
  app's documented fallback. Never erase or format `sysmeta` to recover a read.
- Check NVS types, string capacity/termination, return codes and numeric ranges.
  Missing credentials mean unconfigured; errors or oversized passwords do not
  mean an open network. Do not hardcode or log passwords or tokens.
- Apply panel brightness through the board interface after panel initialization.
  Preserve the user's value across temporary dimming, AOD and sleep; do not persist
  temporary power settings. Keep screen timeout separate from animation/DFS idle.
- Inject equivalent settings snapshots in simulation. On hardware, validate a
  Vibe preference change followed by app launch, missing/invalid values, network
  failures and wake restoration. Verify global settings remain unchanged.

Implementation sources in utils:
[`vibe_settings.c`](https://github.com/esp-mosaico/esp-mosaico-utils/blob/587005cee2db759deb357ec24cf92ece667a25a2/esp-mosaico-vibe-mode/firmware/vibe-mode/components/app_settings/src/vibe_settings.c),
[`factory_network.c`](https://github.com/esp-mosaico/esp-mosaico-utils/blob/587005cee2db759deb357ec24cf92ece667a25a2/esp-mosaico-vibe-mode/firmware/vibe-mode/components/app_network/src/factory_network.c),
and [`mosaico_recovery_contract.h`](https://github.com/esp-mosaico/esp-mosaico-utils/blob/587005cee2db759deb357ec24cf92ece667a25a2/esp-mosaico-vibe-mode/include/mosaico_recovery_contract.h).
Vibe's private settings/network components own firmware tasks and state; do not
link them wholesale into an app just to read configuration. See the
[CLI command guide](mosaico-cli.md#command-responsibilities) for provisioning;
device writes continue to use only `mosaico.py`.
