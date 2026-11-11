# app_panel

Owns the 480×480 GSP scene, paging state, touch event callback, and a UI timer.
It borrows an `app_hub` client and renders fixed-size snapshots; it never performs
HTTP or Wi-Fi work on the UI loop.

The device tab shows four Point cards per page. Light, switch, plug, and
outlet cards (or Points exposing `props.params.power`) can queue a generic power
toggle. Other Points remain visible and read-only. The UI refresh timer copies
the current model every 50 ms, so network tasks never call GSP setters.
Device cards are painted even while Add/Wi-Fi overlays are open, and leaving
those overlays always reconciles the card bank with the current snapshot so
the device count cannot update while the cards stay on the previous page.

The Add card opens capability-driven Matter, Zigbee, BLE Mesh, MQTT, or Modbus
TCP field flows. Every flow ends in a cancellable asynchronous status screen.
Status/result/cancel widgets have independent GSP visibility slots; leaving Add
retries hiding them until the hide commands succeed. Hide failures no longer
clear the leftover flag. While Matter commissioning is running the 50 ms UI
tick does not call GSP setters, and the pairing-code cursor is detached, so BLE
GATT is not starved of internal DMA RAM by display commits.
The scene builder cycles each selected device through Skip/Off/On. The automation
builder selects a live trigger Point at its current value (onboard IMU defaults
to `shake`, tap again for `tilt`/`flip`), then a controllable target and desired
power state. Advanced editing and Store stay in the Web UI. IMU is not deletable.

The scene uses GSP dynamic color themes. OLED dark (`#000000` / `#191B1D` /
`#FF4C01`) is the default; Settings → Appearance toggles the light skin.
The choice is stored in NVS on device and `minihub_panel.theme` in the simulator.

All Chinese runtime labels use the bundled CJK subset. Dynamic names containing
glyphs outside that subset fall back to a localized `Device · <id>` label rather
than rendering tofu. The controller is process-lifetime; the borrowed GSP and
hub handles must outlive it.
