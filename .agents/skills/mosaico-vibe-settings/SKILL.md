---
name: mosaico-vibe-settings
description: >
  Use when developing or changing an ESP-Mosaico app settings menu that should
  share Vibe Mode device preferences (设置栏, 配网, Wi-Fi, 亮度, 息屏,
  自动旋转, 触感, 马达, 音量, 语言, 共享信息). Ask which items to list.
  Simulate the settings pages on GSP before device install. Not for Vibe Mode
  firmware itself or factory reset.
---

# Vibe Mode settings in an app

Device preferences already live in `sysmeta`. The app menu reads and writes
that store. It does not keep a second copy of those keys.

The key table in [sysmeta-keys.md](references/sysmeta-keys.md) is the contract.
When the workspace contains
`esp-mosaico-vibe-mode/docs/vibe-mode-workflows.md`, also read its
设置与共享信息 section. Accepted writes are those checked by
`vibe_settings_request()` in that tree's
`firmware/vibe-mode/components/app_settings/src/vibe_settings.c`.
Device apply details that are easy to get wrong are in
[surface.md](references/surface.md).

## Ask which items to list

When the user asks to add, change, or review the settings bar (设置栏, 设置项,
配网, 亮度, 马达, 音量, 语言), ask which elements to show before editing the
scene. Offer this list and wait for the answer:

- Wi-Fi provisioning, copied from the Vibe Mode page
- Display: brightness, screen-off, auto-rotate
- Haptics or motor strength
- Volume
- Language: English / 简体中文
- Shared information

Use the list they name. Leave out a group they exclude. Do not ask again in
the same task after they have named it. Leave factory reset and
`open_confirm` in Vibe Mode. Volume and motor strength are app-only values:
store them in the app's own NVS namespace, and still write
`vibe_prefs/haptics` as 0 or 1 when strength is the haptics gate.

## What to build

One settings surface. How it opens stays with the app (AI key, drawer, or
button). Use the app's existing visual system.

Wi-Fi, when included, copies the Vibe Mode page rather than a status line or
a fixed grid of rows:

- Status card: saved SSID or not-connected, a detail line, and Forget.
- Scan control.
- One network row template and a list whose viewport is shorter than the
  scanned total, so the list scrolls. `item_height` equals the template
  height. A tap uses the row index. The list handle may be 0; only
  `ESP_GSP_LIST_NONE` means unbound. A successful scan with an empty
  viewport means the total was never applied.
- Password page: SSID, field, and the GSP keyboard. Confirm writes
  `shared/wifi_ssid` and `shared/wifi_password` and connects with the one STA
  this app owns. Do not start a second provisioner beside an existing stack.

Display, when included: brightness steps 20/40/60/80/100, timeout
0/60/180/300/600, and auto-rotate.

Haptics, when included: taps on the Wi-Fi page, the password page, and the
display page use a full-strength hold of about 280 ms, the same pulse as the
motor preview. The short bead tick is too brief to feel on those controls.
Queue it when the control is tapped, including the menu buttons that open
those pages, before the page index changes. Strength is the duty of that
hold. Zero strength writes `haptics` 0 and silences every page until the user
picks a non-zero strength.

Language, when included: `lang` 1 switches the settings strings to English
and 2 switches them to 简体中文. Do not keep a second language key.

Shared information, when included: one row template, not one object per key.
Opening the menu scans `sysmeta/shared` again on a task other than the render
task. Skip `wifi_ssid` and `wifi_password`. Show up to 16 other keys. A key
added later in Vibe Mode appears on the next open without a scene edit.

## Simulate the settings pages on GSP

Load [gsp-sim](../gsp-sim/SKILL.md) and run the project's native sim_bridge
before device install. The settings scene and the C that paints and handles
it must run there.

Exercise every included page: open the surface, tap each control, scroll the
network list, open the password keyboard, and switch language. A fake scan of
N networks must leave the list total at N. The host uses the same click
callbacks as the device. It records the queued preference and repaints. It
does not link BSP, NVS, Wi-Fi, or the motor.

A scene-only dump or a single static frame is not that check. State the gap
the simulator cannot close: backlight, IMU rotation, the motor, and a live
scan.

## Make the included controls work

Clicks use one hidden callback object. Each button sends
`events: [{event: click, action: call, target_name: <that object>, arg: <value>}]`.
A `callback` field by itself does not deliver the click.

`mosaico_gsp_add_ui_bundle` stores the scene in `ui_apps`. A scene, font, or
list change is installed with `iris system-update`. `app-update` leaves the
previous scene on the panel. An Iris screenshot of the new page is the check
that the scene arrived. A matching ELF hash is not.

Every glyph that a label or `set_text` will draw must be in the font subset.
`GSPC-RS-TEXT-MISSING` for a character on screen means that label is not done.
Subset the missing characters from a full Noto Sans SC. A small copied face
often lacks 配, 马, and 删, and those labels render as boxes.

Rotation, brightness, haptics, and the Wi-Fi list follow
[surface.md](references/surface.md).

## Writing back

`mosaico_shared_get()` does not write. An app does not call
`vibe_settings_request()`. Write the same partition and keys, with the same
checks as the key table. Commit on the app task. Never log values. Mask any
key whose name contains `key`, `token`, `secret`, `pass`, or `pwd`.

| Control | Write | Also do |
|---|---|---|
| Brightness | `vibe_prefs/brightness` | Pause the presenter, then `bsp_display_brightness_set()` |
| Screen off | `vibe_prefs/timeout` | Arm the idle timer; dim to 10% backlight, do not blank to 0 |
| Auto-rotate | `vibe_prefs/rotate` | Presenter rotation plus touch, following the IMU when on |
| Haptics | `vibe_prefs/haptics` | Gate pulses. Settings taps use the full-strength hold |
| Motor strength | app NVS, and `haptics` 0/1 | `bsp_motor_set_strength()` held about 280 ms |
| Volume | app NVS | Apply to the app speaker |
| Language | `vibe_prefs/lang` | Switch the settings strings |
| Wi-Fi | `shared/wifi_ssid`, `shared/wifi_password` | One STA. Disconnect before scan if still connecting |
| User key | `shared/<key>` | Same string the next `mosaico_shared_get()` returns |

Invalid values are rejected and the previous value stays. A missing key uses
the fallback in the key table and is not written until the user changes that
control.

## Module

One C module owns load, scan, validate, and save. The instance keeps the last
snapshot. Render and input callbacks only queue a change. The module task
writes NVS and applies brightness, language, and rotation.

## Out of scope

A request that only changes gameplay, audio, or frame rate does not add this
menu. A request that already has a settings page adds the groups the user
asked for and keeps the page's own layout. Do not put back a group the user
told you to remove.
