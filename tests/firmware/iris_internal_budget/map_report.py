"""Sum admitted Iris/USB/network input sections in an ESP32-S31 linker map."""
import argparse
import json
import re
from pathlib import Path


def summarize(text):
    groups = {}
    output = ""
    for line in text.splitlines():
        if line.startswith("."):
            output = line.split()[0]
        match = re.match(
            r"^\s+(?:\.[^\s]+\s+)?(0x[0-9a-fA-F]+)\s+"
            r"(0x[0-9a-fA-F]+)\s+(.+\.a\([^)]+\))\s*$", line
        )
        if not match or not output.startswith((".dram0.", ".iram0.")):
            continue
        address, size = int(match[1], 16), int(match[2], 16)
        if not 0x2F000000 <= address < 0x2F080000 or size == 0:
            continue
        obj = match[3]
        group = None
        for token, name in (
            ("libesp_iris.a(", "iris"),
            ("libespressif__tinyusb.a(", "tinyusb"),
            ("libespressif__esp_tinyusb.a(", "tinyusb_adapter"),
            ("libesp_mosaico_app_recovery.a(", "recovery_adapter"),
            ("(iris_screen_mirror.c.obj)", "screen_backend"),
            ("libesp_hal_usb.a(", "usb_hal"),
            ("(usb_phy.c.obj)", "usb_phy"),
            ("liblwip.a(", "network"),
            ("libesp_netif.a(", "network"),
            ("libmdns.a(", "network"),
        ):
            if token in obj:
                group = name
                break
        if group is None:
            continue
        row = groups.setdefault(group, {"data_bytes": 0, "iram_bytes": 0,
                                         "alignment_charge_bytes": 0})
        # Product USB DMA/static objects use 4-byte alignment. Charge each
        # admitted input section's trailing padding too, conservatively.
        charged = (size + 3) & ~3
        row["alignment_charge_bytes"] += charged - size
        row["iram_bytes" if output.startswith(".iram0.text") else "data_bytes"] += charged
    return {
        "groups": groups,
        "static_internal_data_bytes": sum(x["data_bytes"] for x in groups.values()),
        "resident_iram_bytes": sum(x["iram_bytes"] for x in groups.values()),
        "static_alignment_charge_bytes": sum(x["alignment_charge_bytes"] for x in groups.values()),
        "scope": "Iris, complete TinyUSB adapters, USB HAL/PHY, linked network state, Recovery adapter and screenshot backend; excludes diagnostic hooks/table and shared OS/NVS implementation statics",
    }


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("map", type=Path)
    args = parser.parse_args()
    print(json.dumps(summarize(args.map.read_text()), indent=2, sort_keys=True))


if __name__ == "__main__":
    main()
