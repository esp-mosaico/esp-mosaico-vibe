# sysmeta keys shared with Vibe Mode

Partition `sysmeta`. Preferences namespace `vibe_prefs` (`u32`). Shared
strings namespace `shared`. Source of the accepted writes:
`vibe_settings_request()` and `load()` in
`esp-mosaico-vibe-mode/firmware/vibe-mode/components/app_settings/src/vibe_settings.c`.
Public read of one shared string: `mosaico_shared_get()` in
`esp-mosaico-vibe-mode/components/esp_mosaico_app_recovery/include/mosaico_shared.h`.

A missing key uses the fallback below. Do not write the fallback until the
user changes that control.

## vibe_prefs

| Key | Accepted write | Missing-key fallback | App action |
|---|---|---|---|
| `lang` | `1` English, `2` 简体中文 | `1` | Switch the app's own strings |
| `brightness` | `20`, `40`, `60`, `80`, `100` | `80` | Pause the presenter, then `bsp_display_brightness_set()` |
| `timeout` | `0` never, `60`, `180`, `300`, `600` seconds | `0` | Idle dim to 10% backlight |
| `rotate` | `0` off, `1` on | `0` | Presenter rotation and touch; IMU follow when on |
| `haptics` | `0` off, `1` on | `1` | Gate every pulse. Settings taps hold full strength about 280 ms. Stored 0 forces motor strength 0 |
| `open_confirm` | Vibe Mode only | `1` | Do not put this on an app menu |

`confirm_cnt` is the Vibe Mode open-confirm counter. Apps do not write it.

Snap a stored brightness outside the five steps to the nearest step before
showing it. Only write one of the five. A stored timeout other than the five
accepted values is shown as never until the user picks a listed choice.

## shared

| Key | Rule |
|---|---|
| `wifi_ssid` | 1–32 bytes. Buffer at least 33. Shown on the Wi-Fi card, not as a user row |
| `wifi_password` | Empty, 8–63 bytes, or 64 hex digits. Buffer at least 65. Masked. Same card |
| other keys | 1–15 bytes of `[a-z0-9_]`. Value at most 256 bytes plus NUL. At most 16 such keys |

Wi-Fi fields do not consume the 16 user slots. Renaming a key onto
`wifi_ssid` or `wifi_password` is rejected. A name containing `key`, `token`,
`secret`, `pass`, or `pwd` (any case) is masked until the user reveals it.

`mosaico_shared_get()` returns `ESP_ERR_NVS_NOT_FOUND` when the key is absent
and `ESP_ERR_NVS_INVALID_LENGTH` when the buffer is short. It never writes,
erases, logs, or caches the value.

Scan user rows with `nvs_entry_find("sysmeta", "shared", NVS_TYPE_STR)` each
time the menu opens. Skip the two Wi-Fi keys. Do not bake the discovered names
into the scene.
