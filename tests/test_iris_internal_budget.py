"""Configuration and linker-ledger regressions for the internal-RAM profile."""
import importlib.util
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
FIXTURE = ROOT / "tests/firmware/iris_internal_budget"
spec = importlib.util.spec_from_file_location("iris_budget_map", FIXTURE / "map_report.py")
ledger = importlib.util.module_from_spec(spec)
spec.loader.exec_module(ledger)


def test_normal_profiles_bound_internal_buffers():
    for name in ("hello_world",):
        values = dict(line.split("=", 1) for line in
                      (ROOT / "projects" / name / "sdkconfig.defaults").read_text().splitlines()
                      if line.startswith("CONFIG_") and "=" in line)
        for key in ("WIRE_BUFFERS_PSRAM", "SERVICE_CONTEXT_PSRAM", "SERVICE_STATE_PSRAM",
                    "RPC_RESPONSE_PSRAM", "SERVICE_PERSISTENT", "LOG_RING_STORAGE_PSRAM",
                    "TCP_DEFER_UNTIL_NETIF"):
            assert values["CONFIG_ESP_IRIS_" + key] == "y"
        assert values["CONFIG_ESP_IRIS_SERVICE_STACK_SIZE"] == "4096"
        assert values["CONFIG_ESP_IRIS_USB_TASK_STACK_SIZE"] == "2048"
        for key in ("RX_BUFSIZE", "TX_BUFSIZE", "EP_BUFSIZE"):
            assert values["CONFIG_TINYUSB_CDC_" + key] == ("512" if key == "EP_BUFSIZE" else "1024")


def test_fixture_preserves_partitions_and_uses_real_ui():
    assert (FIXTURE / "partitions.csv").read_text() == (ROOT / "projects/hello_world/partitions.csv").read_text()
    cmake = (FIXTURE / "CMakeLists.txt").read_text()
    assert 'projects/hello_world/main' in cmake
    assert 'projects/hello_world/sdkconfig.application.defaults' in cmake


def test_map_ledger_counts_only_live_internal_input_sections():
    report = ledger.summarize("""
 .bss.discarded 0x00000000 0x8000 esp-idf/esp_iris/libesp_iris.a(foo.c.obj)
.dram0.data 0x2f010000 0x100
 .data.g_iris 0x2f010000 0x40 esp-idf/esp_iris/libesp_iris.a(esp_iris.c.obj)
.dram0.bss 0x2f010100 0x1000
 .bss.usb
                0x2f010100 0x800 esp-idf/espressif__tinyusb/libespressif__tinyusb.a(cdc.c.obj)
 .bss.s_records 0x2f010900 0x100 esp-idf/main/libmain.a(budget.c.obj)
 .bss.s_mirror 0x2f010a00 0x40 esp-idf/main/libmain.a(iris_screen_mirror.c.obj)
.iram0.text 0x2f000000 0x1000
 .text.usb_irq 0x2f000000 0x30 esp-idf/esp_hal_usb/libesp_hal_usb.a(usb.c.obj)
.flash.rodata 0x50010000 0x1000
 .rodata.usb 0x50010000 0x200 esp-idf/espressif__tinyusb/libespressif__tinyusb.a(cdc.c.obj)
""")
    assert report["static_internal_data_bytes"] == 0x880
    assert report["resident_iram_bytes"] == 0x30


def test_map_ledger_conservatively_charges_four_byte_alignment():
    report = ledger.summarize("""
.dram0.bss 0x2f010000 0x100
 .bss.flag 0x2f010000 0x1 esp-idf/esp_iris/libesp_iris.a(esp_iris.c.obj)
 .bss.usb 0x2f010004 0x7 esp-idf/espressif__tinyusb/libespressif__tinyusb.a(cdc.c.obj)
""")
    assert report["static_internal_data_bytes"] == 12
    assert report["static_alignment_charge_bytes"] == 4
