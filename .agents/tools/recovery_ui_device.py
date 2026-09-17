"""Capture Recovery UI evidence and send touches through the owning Gateway.

Firmware operations remain owned by mosaico.py. This helper never accesses USB.
"""
import argparse
import json
import time
import urllib.request
import urllib.error
import uuid
from pathlib import Path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--gateway", required=True)
    parser.add_argument("--device", required=True)
    parser.add_argument("--output", type=Path, required=True)
    commands = parser.add_subparsers(dest="command", required=True)
    capture = commands.add_parser("capture")
    capture.add_argument("name")
    tap = commands.add_parser("tap")
    tap.add_argument("x", type=int)
    tap.add_argument("y", type=int)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    base = args.gateway.rstrip("/") + "/v1/devices/" + args.device

    def request(suffix, body=None):
        req = urllib.request.Request(base + suffix)
        if body is not None:
            req.data = json.dumps(body).encode()
            req.add_header("Content-Type", "application/json")
            req.add_header("X-Operation-ID", str(uuid.uuid4()))
        try:
            with urllib.request.urlopen(req, timeout=30) as response:
                return response.read(), dict(response.headers)
        except urllib.error.HTTPError as error:
            detail = error.read().decode()
            raise RuntimeError("Gateway HTTP {}: {}".format(error.code, detail)) from error

    status = json.loads(request("")[0])
    # Individual status is a live RPC; never use a cached inventory entry alone.
    if status.get("device_id") != args.device or status.get("stale"):
        raise RuntimeError("Device identity is stale or mismatched")
    event = {"timestamp_ns": time.time_ns(), "command": args.command,
             "device_id": args.device, "boot_id": status.get("boot_id_text"),
             "firmware_sha256": status.get("firmware_sha256")}
    if args.command == "capture":
        if Path(args.name).name != args.name:
            raise ValueError("Capture name must be a filename")
        time.sleep(0.35)  # Allow the display and 250-ms status task to render.
        data, headers = request("/screenshot?save=true", {})
        if not data.startswith(b"\x89PNG\r\n\x1a\n"):
            raise RuntimeError("Expected PNG screenshot")
        (args.output / (args.name + ".png")).write_bytes(data)
        (args.output / (args.name + ".json")).write_text(json.dumps({
            "status": status, "headers": headers}, indent=2) + "\n")
        event.update(name=args.name, operation_id=headers.get("X-Operation-ID"))
    else:
        if not (0 <= args.x < 480 and 0 <= args.y < 480):
            raise ValueError("Touch coordinates must be inside the display")
        point = {"x": round(args.x * 10000 / 479), "y": round(args.y * 10000 / 479)}
        result = json.loads(request("/input", {"begin": point, "end": point})[0])
        if not result.get("input", {}).get("accepted"):
            raise RuntimeError("Touch was not accepted")
        event.update(x=args.x, y=args.y, operation=result.get("operation"))
    with (args.output / "ui-events.jsonl").open("a") as stream:
        stream.write(json.dumps(event) + "\n")
    print(json.dumps({key: value for key, value in event.items() if key != "operation"}))


if __name__ == "__main__":
    main()
