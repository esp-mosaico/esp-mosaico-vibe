# Settings surface: apply without fighting GSP

These are the device-side steps for controls the user included. The GSP
simulator records the same queued values and does not call them.

## Clicks

One hidden object carries `callback`. Buttons do not. Each button has:

```json
"events": [{
  "event": "click",
  "action": "call",
  "target_name": "settings_action",
  "arg": 20
}]
```

The render task maps `arg` to a preference and queues it. The module task
writes NVS and applies hardware.

## Scene install

The bundle from `mosaico_gsp_add_ui_bundle` is the `ui_apps` partition, not
bytes inside the application ELF. `iris app-update` updates the ELF only, so
the panel keeps the previous settings scene. Ship a scene, font, or list
change with `iris system-update`, then screenshot that page through Iris.

## Network list

Copy the Vibe Mode Wi-Fi page: status card, Forget, scan, then a list.

The row template height and the list `item_height` are the same value.
`GSPC-RS-ROW-TEMPLATE-SIZE` means they differ and the list will not compile.
Place the template off the 480×480 screen, not `hidden: true`; a hidden
template is not registered. Keep `item_count` at 0 and set the total from the
scan. The viewport is shorter than that total so the list scrolls. A row tap
reports `event->item` as the network index.

`esp_gsp_list_t` is a `uint16_t`. The first `esp_gsp_list_bind_component`
returns 0. `ESP_GSP_LIST_NONE` is `UINT16_MAX`. Store that sentinel until bind
returns, and update the list only when `handle != ESP_GSP_LIST_NONE`.
`if (handle)` treats 0 as unbound, so `set_total` and `refresh` never run.
The scan can log a non-zero count and the status line can say to choose a
network while the viewport stays empty. That empty viewport is a missed list
update, not a failed radio scan.

Fill row text with the generated slot constants. For a template whose first
two labels are the SSID and the open/secured line, those slots are 0 and 1.
Publish the scanned rows on the render task, then `set_total` and `refresh`.

## Wi-Fi scan

One STA. Boot may already be connecting to the saved SSID.
`esp_wifi_scan_start` returns `ESP_ERR_WIFI_STATE` while that connect is in
progress, and treating the error as a bad password clears the list.

If `esp_wifi_sta_get_ap_info` fails, disconnect, wait briefly, then scan. An
associated station can scan in place. On `ESP_ERR_WIFI_STATE`, or when an
in-place scan returns no records, disconnect and scan once more, then
reconnect the saved network without erasing the results. Call
`esp_wifi_scan_stop` first and pass an explicit active `wifi_scan_config_t`
for all channels. Run the blocking scan on the settings task, then publish
the view. Log the error name and the count only. A scan failure has its own
detail string, and that string's glyphs belong in the font subset.

The password page uses a GSP `keyboard`. Define `GSP_BUNDLE_ENABLE_RAW_IDS`
before the generated bundle header so the password field's bind id is
visible. The action enum is `GSP_<SCENE>_ACTION_<NAME>`, not
`GSP_<SCENE>_ACT_ID_<NAME>`. Attach the keyboard with that action id and the
bind id. Confirm reads `esp_gsp_keyboard_text`, checks the password (empty,
8–63 bytes, or 64 hex digits), writes the two shared keys, and connects. Do
not log the password.

## Font

`set_text` can only draw glyphs that were subset into the face. Treat
`GSPC-RS-TEXT-MISSING` for a visible string as a broken label. Rebuild the
subset from a full Noto Sans SC that contains every character the scene and
the language switch will show, including runtime strings such as 删除 and
扫描失败. Save the subset as TTF. Setting `font.flavor` to `otf` makes
fontTools reject the file.

## Rotation

GSP presents through `esp_display_presenter`. Create that presenter and pass
it in `esp_gsp_esp_lcd_config_t.presenter`.

`rotate` 1 follows how the device is held. `rotate` 0, and a dimmed panel,
stay at 0 degrees. Read acceleration, debounce a new orientation for 300 ms,
and skip the change while the panel is dimmed. When `|z|` is at least 0.8,
keep the current angle. Otherwise if `|x|` exceeds `|y|` by 1.7, use 90 when
x is positive and 270 when it is negative. Otherwise if `|y|` exceeds `|x|`
by 1.7, use 0 when y is positive and 180 when it is negative.

On a change, pause GSP, call `esp_display_presenter_set_rotation`, apply the
matching touch swap and mirror, then resume. With touch base identity the
mapping is: 0 none, 90 swap and mirror Y, 180 mirror X and Y, 270 swap and
mirror X. `bsp_display_set_rotation` changes panel MADCTL while the presenter
keeps drawing angle 0, so the picture does not follow.

## Brightness

The backlight command shares the frame SPI. Pause GSP, call
`bsp_display_brightness_set`, then resume. Screen-off dims to 10% and does
not blank to 0.

## Haptics

`bsp_motor_set(true)` forces duty 100. `bsp_motor_set(false)` forces duty 0.
Neither call keeps a chosen strength. Drive the motor with
`bsp_motor_set_strength`. Hold that duty before any speaker write, then
return it to 0. A 16 ms pulse does not spin the motor up. The short bead
tick, about 70 ms and capped near 40% duty, is also too small on the Wi-Fi,
password, and display controls. Those taps use the motor-preview hold: the
full chosen strength for about 280 ms.

`vibe_prefs/haptics` gates every pulse. When the stored value is 0, load
motor strength as 0 so no page vibrates until the user picks a non-zero
strength. That choice writes `haptics` 1 and plays the preview. A settings
page that never queues this hold stays silent even when the motor page
itself vibrates. Queue the hold from the tap, including the menu buttons
that open Wi-Fi and Display, before the page index changes.
