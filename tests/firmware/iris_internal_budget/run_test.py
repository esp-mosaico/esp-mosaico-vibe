"""Exercise the fixture only through the Gateway managed by mosaico.py.

Run with the prepared ESP-Iris host Python (aiohttp required). No direct USB
access or firmware writes. Every API response/operation ID is retained.
"""
import argparse
import asyncio
import base64
import hashlib
import json
from pathlib import Path
import struct
import sys
import time

import aiohttp

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "submodule/esp-mosaico-utils/esp-mosaico-recovery/tools"))
from mosaico_cli.gateway import connected_devices, ensure_gateway, select_device
from mosaico_cli.runtime import RunContext
from mosaico_cli.workspace import load_workspace

from map_report import summarize


async def exercise(args, context, url, initial):
    device_id = initial["device_id"]
    prefix = url + "/v1/devices/" + device_id
    timeout = aiohttp.ClientTimeout(total=60)
    async with aiohttp.ClientSession(timeout=timeout) as client:
        async def request(path, body=None):
            async with client.request("GET" if body is None else "POST",
                                      prefix + path, json=body) as response:
                data = await response.read()
                if response.status != 200:
                    context.note(f"HTTP {response.status} {path}: {data.decode(errors='replace')}")
                    raise RuntimeError(f"HTTP {response.status}: {path}")
                if response.content_type.startswith("image/"):
                    filename = context.directory / f"screen-{time.time_ns()}.png"
                    filename.write_bytes(data)
                    result = {"bytes": len(data), "sha256": hashlib.sha256(data).hexdigest(),
                              "operation_id": response.headers.get("X-Operation-ID"),
                              "media": response.headers.get("X-ESP-Iris-Media"),
                              "artifact": str(filename)}
                else:
                    result = json.loads(data)
                context.note(json.dumps({"path": path, "result": result}, sort_keys=True))
                return result

        async def rpc(method, payload=b"", service=0x6A03):
            result = await request("/rpc/raw", {"service_id": service, "method_id": method,
                "payload_base64": base64.b64encode(payload).decode(), "deadline_ms": 10000})
            return base64.b64decode(result["payload_base64"], validate=True)

        async def metrics():
            if args.production_check:
                return {}  # Production has no diagnostic heap hook/RPC.
            raw = await rpc(2)
            return dict(zip(("current_heap_bytes", "peak_heap_bytes", "live_blocks",
                "allocations", "trace_errors", "service_stack_free_min_bytes",
                "usb_stack_free_min_bytes"), struct.unpack("<7I", raw)))

        snapshots = {"before": await metrics()}
        state = json.loads(await rpc(1, service=0x1200))
        if state.get("ota_writer") is not False or state.get("mode") != "normal":
            raise RuntimeError("normal application recovery state mismatch")
        await request("/system-inventory")
        snapshots["after_inventory"] = await metrics()
        payload = bytes(range(256)) * 4
        async def check_rpc():
            if args.production_check:
                value = json.loads(await rpc(1, service=0x1200))
                return value.get("project") == "gsp_hello" and value.get("mode") == "normal"
            return await rpc(1, payload) == payload
        for _ in range(args.rpc_count):
            if not await check_rpc():
                raise RuntimeError("1024-byte RPC echo mismatch")
        screenshots = []
        for _ in range(args.screenshots):
            screenshots.append(await request("/screenshot?save=true", {}))
        snapshots["after_screenshots"] = await metrics()

        chunks = frames = received = 0
        complete_frames = 0
        async with client.ws_connect(prefix + "/streams/screen") as websocket:
            await request("/mirror/start", {"channel": "screen", "fps": args.fps})
            async def collect():
                nonlocal chunks, frames, received, complete_frames
                until = time.monotonic() + args.seconds
                frame_id = None
                next_y = 0
                while time.monotonic() < until:
                    try:
                        message = await websocket.receive(timeout=1)
                    except asyncio.TimeoutError:
                        continue
                    if message.type == aiohttp.WSMsgType.BINARY:
                        n = int.from_bytes(message.data[:4], "little")
                        meta = json.loads(message.data[4:4+n])
                        tile = meta.get("description", {})
                        y = tile.get("y")
                        height = tile.get("height", 0)
                        chunks += 1
                        received += len(message.data) - 4 - n
                        if y == 0:
                            frames += 1
                            frame_id = meta.get("frame_id")
                            next_y = 0
                        if (frame_id == meta.get("frame_id") and y == next_y and
                                tile.get("width") == 480 and tile.get("stride") == 960 and
                                len(message.data) - 4 - n == 960 * height):
                            next_y += height
                            if next_y == 480:
                                complete_frames += 1
                        else:
                            frame_id = None
                    elif message.type in (aiohttp.WSMsgType.ERROR, aiohttp.WSMsgType.CLOSED):
                        raise RuntimeError("mirror websocket closed early")

            collector = asyncio.create_task(collect())
            try:
                for _ in range(args.rpc_count):
                    if not await check_rpc():
                        raise RuntimeError("RPC echo mismatch during mirror")
                screenshots.append(await request("/screenshot?save=true", {}))
                await request("/system-inventory")
                snapshots["during_mirror"] = await metrics()
                await collector
            finally:
                collector.cancel()
                await request("/mirror/stop", {"channel": "screen"})
        snapshots["after"] = await metrics()
        if chunks == 0 or complete_frames == 0:
            raise RuntimeError("mirror delivered no complete 480x480 RGB565 frame")
        status = await request("")
        live = status.get("device", status)
        if live.get("boot_id") != initial["boot_id"]:
            raise RuntimeError("device rebooted during stress test")
        if live.get("invalid_frames", 0) != 0 or live.get("crash_count", 0) != 0:
            raise RuntimeError("invalid frames or crash reported")
        if args.production_check:
            layout = total = None
            passed = True  # Behavior only; never claim a production heap measurement.
        else:
            layout = summarize(args.map.read_text())
            peak = max(x["peak_heap_bytes"] for x in snapshots.values())
            errors = max(x["trace_errors"] for x in snapshots.values())
            total = peak + layout["static_internal_data_bytes"] + layout["resident_iram_bytes"]
            passed = total < 25000 and errors == 0 and min(
                x["usb_stack_free_min_bytes"] for x in snapshots.values()) >= 512
        report = {"device_id": device_id, "boot_id": initial["boot_id"],
            "firmware_sha256": initial["firmware_sha256"], "snapshots": snapshots,
            "static": layout, "peak_internal_bytes": total,
            "limit_bytes": None if args.production_check else 25000,
            "budget_measured": not args.production_check, "passed": passed,
            "rpc_calls": args.rpc_count * 2,
            "rpc_payload_bytes": 0 if args.production_check else 1024,
            "screenshots": screenshots, "mirror_seconds": args.seconds,
            "mirror_chunks": chunks, "mirror_frame_starts": frames, "mirror_bytes": received,
            "mirror_complete_frames": complete_frames,
            "raw_log": str(context.log_path)}
        (context.directory / "result.json").write_text(json.dumps(report, indent=2, sort_keys=True))
        print(json.dumps(report, sort_keys=True))
        return 0 if report["passed"] else 1


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--device-id", required=True)
    parser.add_argument("--production-check", action="store_true",
                        help="Check normal GSP behavior only, without diagnostic RPCs")
    parser.add_argument("--map", type=Path, default=Path(__file__).parent / "build/iris_internal_budget.map")
    parser.add_argument("--rpc-count", type=int, default=30)
    parser.add_argument("--screenshots", type=int, default=3)
    parser.add_argument("--seconds", type=float, default=30)
    parser.add_argument("--fps", type=int, default=5)
    args = parser.parse_args()
    workspace = load_workspace(ROOT / "submodule/esp-mosaico-utils/esp-mosaico-recovery", explicit=ROOT)
    context = RunContext(workspace, "iris-budget-test", json_output=True)
    gateway = ensure_gateway(context, None)
    if gateway.connection_args[0] != "--url":
        raise RuntimeError("this fixture requires the managed loopback Gateway")
    device = select_device(connected_devices(context, gateway), args.device_id)
    expected = "gsp_hello" if args.production_check else "iris_internal_budget"
    if device.get("project_name") != expected:
        raise RuntimeError(f"install {expected} with mosaico.py first")
    return asyncio.run(exercise(args, context, gateway.connection_args[1], device))


if __name__ == "__main__":
    sys.exit(main())
