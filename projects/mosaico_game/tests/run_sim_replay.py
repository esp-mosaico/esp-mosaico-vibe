#!/usr/bin/env python3
"""Drive the official GSP simulator over its public WebSocket RPC."""

from __future__ import annotations

import argparse
import base64
import hashlib
import json
import os
import socket
import struct
import time
from pathlib import Path

from PIL import Image


class Simulator:
    def __init__(self, host: str, port: int) -> None:
        self.socket = socket.create_connection((host, port), timeout=5)
        key = base64.b64encode(os.urandom(16)).decode()
        request = (
            f"GET /ws HTTP/1.1\r\nHost: {host}:{port}\r\n"
            "Upgrade: websocket\r\nConnection: Upgrade\r\n"
            f"Sec-WebSocket-Key: {key}\r\nSec-WebSocket-Version: 13\r\n\r\n"
        )
        self.socket.sendall(request.encode())
        response = self._read_http_header()
        expected = base64.b64encode(
            hashlib.sha1(
                (key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11").encode()
            ).digest()
        ).decode()
        if "101 Switching Protocols" not in response or expected not in response:
            raise RuntimeError(f"WebSocket handshake failed: {response}")
        self.rpc_id = 1
        self.pending: dict[int, dict] = {}
        self.width = 0
        self.height = 0
        self.rgba = bytearray()

    def _read_http_header(self) -> str:
        data = bytearray()
        while b"\r\n\r\n" not in data:
            chunk = self.socket.recv(4096)
            if not chunk:
                raise RuntimeError("simulator closed during handshake")
            data.extend(chunk)
        return data.decode(errors="replace")

    def _read_exact(self, size: int) -> bytes:
        data = bytearray()
        while len(data) < size:
            chunk = self.socket.recv(size - len(data))
            if not chunk:
                raise RuntimeError("simulator WebSocket closed")
            data.extend(chunk)
        return bytes(data)

    def _receive_frame(self) -> tuple[int, bytes]:
        first, second = self._read_exact(2)
        opcode = first & 0x0F
        length = second & 0x7F
        if length == 126:
            length = struct.unpack(">H", self._read_exact(2))[0]
        elif length == 127:
            length = struct.unpack(">Q", self._read_exact(8))[0]
        masked = (second & 0x80) != 0
        mask = self._read_exact(4) if masked else b""
        payload = bytearray(self._read_exact(length))
        if masked:
            for index in range(length):
                payload[index] ^= mask[index & 3]
        return opcode, bytes(payload)

    def _send_frame(self, opcode: int, payload: bytes) -> None:
        mask = os.urandom(4)
        length = len(payload)
        header = bytearray([0x80 | opcode])
        if length < 126:
            header.append(0x80 | length)
        elif length <= 0xFFFF:
            header.append(0x80 | 126)
            header.extend(struct.pack(">H", length))
        else:
            header.append(0x80 | 127)
            header.extend(struct.pack(">Q", length))
        encoded = bytearray(payload)
        for index in range(length):
            encoded[index] ^= mask[index & 3]
        self.socket.sendall(header + mask + encoded)

    def _send_json(self, value: dict) -> None:
        self._send_frame(1, json.dumps(value, separators=(",", ":")).encode())

    def _handle_gspf(self, data: bytes) -> None:
        if len(data) < 36 or data[:4] != b"GSPF":
            return
        pixel_format = data[5]
        width, height = struct.unpack_from("<II", data, 16)
        rect_count, payload_size = struct.unpack_from("<II", data, 28)
        offset = 36
        rects = []
        for _ in range(rect_count):
            rects.append(struct.unpack_from("<iiii", data, offset))
            offset += 16
        payload = memoryview(data)[offset : offset + payload_size]
        if width != self.width or height != self.height:
            self.width, self.height = width, height
            self.rgba = bytearray(width * height * 4)
        position = 0
        for x1, y1, x2, y2 in rects:
            for y in range(y1, y2):
                for x in range(x1, x2):
                    destination = (y * width + x) * 4
                    if pixel_format == 0:
                        value = payload[position] | payload[position + 1] << 8
                        self.rgba[destination] = ((value >> 11) & 31) * 255 // 31
                        self.rgba[destination + 1] = ((value >> 5) & 63) * 255 // 63
                        self.rgba[destination + 2] = (value & 31) * 255 // 31
                        self.rgba[destination + 3] = 255
                        position += 2
                    else:
                        self.rgba[destination] = payload[position + 2]
                        self.rgba[destination + 1] = payload[position + 1]
                        self.rgba[destination + 2] = payload[position]
                        self.rgba[destination + 3] = (
                            payload[position + 3] if pixel_format == 2 else 255
                        )
                        position += 4 if pixel_format == 2 else 3

    def poll(self, duration: float) -> None:
        deadline = time.monotonic() + duration
        while time.monotonic() < deadline:
            self.socket.settimeout(max(0.01, deadline - time.monotonic()))
            try:
                opcode, payload = self._receive_frame()
            except TimeoutError:
                break
            if opcode == 1:
                message = json.loads(payload)
                if "id" in message:
                    self.pending[message["id"]] = message
            elif opcode == 2:
                self._handle_gspf(payload)
            elif opcode == 8:
                raise RuntimeError("simulator closed")
            elif opcode == 9:
                self._send_frame(10, payload)

    def rpc(self, method: str, params: dict | None = None) -> dict:
        identifier = self.rpc_id
        self.rpc_id += 1
        self._send_json(
            {
                "jsonrpc": "2.0",
                "id": identifier,
                "method": method,
                "params": params or {},
            }
        )
        deadline = time.monotonic() + 5
        while identifier not in self.pending and time.monotonic() < deadline:
            self.poll(0.2)
        message = self.pending.pop(identifier, None)
        if message is None:
            raise RuntimeError(f"RPC timeout: {method}")
        if "error" in message:
            raise RuntimeError(f"RPC {method}: {message['error']}")
        return message.get("result", {})

    def click(self, x: int, y: int, settle: float = 0.25) -> None:
        self.rpc("feed_pointer", {"x": x, "y": y, "pressed": True})
        # The host acknowledges queueing before the native backend polls it.
        # Keep the press observable for at least one backend tick.
        self.poll(0.12)
        self.rpc("feed_pointer", {"x": x, "y": y, "pressed": False})
        self.poll(settle)

    def capture(self, path: Path) -> None:
        self.poll(0.3)
        if not self.rgba:
            raise RuntimeError("simulator produced no framebuffer")
        path.parent.mkdir(parents=True, exist_ok=True)
        Image.frombytes("RGBA", (self.width, self.height), bytes(self.rgba)).save(
            path
        )


def square_center(name: str) -> tuple[int, int]:
    file = ord(name[0]) - ord("a")
    rank = int(name[1]) - 1
    return 20 + file * 55 + 27, 20 + (7 - rank) * 55 + 27


def run(simulator: Simulator, output: Path) -> None:
    capabilities = simulator.rpc("capabilities")
    if not capabilities.get("browser_input_allowed"):
        raise RuntimeError("simulator did not grant input control")
    simulator.poll(0.5)
    simulator.capture(output / "native_home.png")

    simulator.click(240, 404, settle=1.0)  # S00 enter
    simulator.click(240, 280, settle=1.5)  # S10 PvE
    simulator.capture(output / "native_pve_start.png")
    simulator.click(*square_center("e2"))
    simulator.capture(output / "native_pve_legal.png")
    simulator.click(*square_center("e4"), settle=1.0)
    simulator.capture(output / "native_pve_after_ai.png")
    simulator.click(10, 10)
    simulator.capture(output / "native_game_menu.png")
    simulator.click(134, 310)  # resign
    simulator.capture(output / "native_resign_confirm.png")
    simulator.click(353, 399, settle=0.5)
    simulator.capture(output / "native_pve_result.png")

    simulator.click(120, 432, settle=1.0)  # result menu -> S00
    simulator.click(240, 404, settle=1.0)  # S00 enter -> S10
    simulator.click(240, 164, settle=1.0)  # development PvP -> S20
    simulator.click(240, 435, settle=1.0)  # simulated pairing -> S21
    simulator.click(240, 271, settle=1.0)  # continue -> S24
    simulator.click(240, 336, settle=1.5)  # start as white
    simulator.capture(output / "native_pvp_start.png")
    simulator.click(10, 10)
    simulator.click(346, 374, settle=0.5)  # development disconnect
    simulator.capture(output / "native_pvp_disconnected.png")
    simulator.click(127, 409, settle=0.5)  # development sync failure
    simulator.capture(output / "native_pvp_sync_error.png")
    simulator.click(240, 409, settle=0.5)  # development resync
    simulator.capture(output / "native_pvp_reconnected.png")
    simulator.click(*square_center("d2"))
    simulator.click(*square_center("d4"), settle=1.0)
    simulator.click(10, 10)
    simulator.click(346, 310)  # offer draw
    simulator.capture(output / "native_pvp_draw_request.png")
    simulator.click(353, 399, settle=0.5)  # accept
    simulator.capture(output / "native_pvp_draw_result.png")


def run_promotion(simulator: Simulator, output: Path) -> None:
    capabilities = simulator.rpc("capabilities")
    if not capabilities.get("browser_input_allowed"):
        raise RuntimeError("simulator did not grant input control")
    simulator.poll(1.0)
    simulator.capture(output / "native_promotion_start.png")
    simulator.click(*square_center("a7"))
    simulator.click(*square_center("a8"), settle=0.8)
    simulator.capture(output / "native_promotion_choices.png")
    simulator.click(396, 376, settle=1.0)  # knight
    simulator.capture(output / "native_promotion_knight.png")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--url", required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--promotion-only", action="store_true")
    args = parser.parse_args()
    host_port = args.url.removeprefix("http://").rstrip("/").split(":")
    simulator = Simulator(host_port[0], int(host_port[1]))
    if args.promotion_only:
        run_promotion(simulator, args.output)
    else:
        run(simulator, args.output)
    print(f"native simulator replay evidence written to {args.output}")


if __name__ == "__main__":
    main()
