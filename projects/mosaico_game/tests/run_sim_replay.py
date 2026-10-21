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

    def swipe(
        self, start: tuple[int, int], end: tuple[int, int],
        steps: int = 5, settle: float = 0.5
    ) -> None:
        self.rpc(
            "feed_pointer",
            {"x": start[0], "y": start[1], "pressed": True},
        )
        self.poll(0.12)
        for step in range(1, steps + 1):
            x = start[0] + (end[0] - start[0]) * step // steps
            y = start[1] + (end[1] - start[1]) * step // steps
            self.rpc("feed_pointer", {"x": x, "y": y, "pressed": True})
            self.poll(0.04)
        self.rpc(
            "feed_pointer",
            {"x": end[0], "y": end[1], "pressed": False},
        )
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
    if capabilities.get("browser_input_allowed") is False:
        raise RuntimeError("simulator did not grant input control")
    simulator.poll(0.5)
    simulator.capture(output / "native_home.png")

    simulator.click(240, 404, settle=4.0)  # S00 enter
    simulator.capture(output / "native_mode_select.png")
    simulator.click(137, 250, settle=4.0)  # S10 PvE -> S11
    simulator.capture(output / "native_difficulty.png")
    simulator.click(240, 250, settle=4.0)  # S11 normal
    simulator.capture(output / "native_pve_start.png")
    simulator.click(*square_center("e2"))
    simulator.capture(output / "native_pve_legal.png")
    simulator.click(*square_center("e4"), settle=4.0)
    simulator.capture(output / "native_pve_after_ai.png")
    simulator.swipe((240, 380), (240, 120))
    simulator.capture(output / "native_game_menu.png")
    simulator.click(327, 263)  # resign
    simulator.capture(output / "native_resign_confirm.png")
    simulator.click(331, 302, settle=4.0)
    simulator.capture(output / "native_pve_result.png")

    simulator.click(120, 432, settle=4.0)  # result menu -> S10
    simulator.click(240, 422, settle=4.0)  # S10 back -> S00
    simulator.click(240, 404, settle=4.0)  # S00 enter -> S10
    simulator.click(340, 250, settle=4.0)  # PvP card -> S20
    simulator.capture(output / "native_pvp_wait_a.png")
    simulator.poll(0.6)
    simulator.capture(output / "native_pvp_wait_b.png")
    simulator.click(240, 240, settle=4.0)  # Host-only simulated pairing
    simulator.capture(output / "native_pvp_pair_success_anim.png")
    # Full-screen animation decoding can delay the one-shot transition on a
    # cold 2 MiB host image cache; wait until the S21 hit target is active.
    simulator.poll(8.0)
    simulator.capture(output / "native_pvp_paired.png")
    simulator.click(240, 422, settle=4.0)  # local player is ready
    simulator.capture(output / "native_pvp_wait_opponent_start_a.png")
    simulator.poll(0.35)
    simulator.capture(output / "native_pvp_wait_opponent_start_b.png")
    # Host-only peer-ready seam; production reaches the same transition from
    # CHESS_PEER_EVENT_STARTED after both devices have sent READY.
    simulator.click(240, 240, settle=6.0)
    simulator.capture(output / "native_pvp_start.png")
    simulator.swipe((240, 380), (240, 120), settle=1.5)
    simulator.capture(output / "native_pvp_game_menu.png")
    simulator.click(327, 263, settle=1.5)  # resign
    simulator.capture(output / "native_pvp_resign_confirm.png")
    simulator.click(331, 302, settle=4.0)
    simulator.capture(output / "native_pvp_resign_result.png")


def run_pair_failure(simulator: Simulator, output: Path) -> None:
    capabilities = simulator.rpc("capabilities")
    if capabilities.get("browser_input_allowed") is False:
        raise RuntimeError("simulator did not grant input control")
    simulator.poll(0.5)
    simulator.click(240, 404, settle=4.0)  # S00 enter
    simulator.click(340, 250, settle=4.0)  # PvP card -> S20
    # Pairing times out after 12 seconds. Capture the approved transition
    # before its one-shot display timer advances to the S22 terminal frame.
    simulator.poll(12.0)
    simulator.capture(output / "native_pvp_pair_fail_anim.png")
    simulator.poll(10.0)
    simulator.capture(output / "native_pvp_pair_failed.png")


def run_wait_animation(simulator: Simulator, output: Path) -> None:
    capabilities = simulator.rpc("capabilities")
    if capabilities.get("browser_input_allowed") is False:
        raise RuntimeError("simulator did not grant input control")
    simulator.poll(0.5)
    simulator.click(240, 404, settle=0.3)  # S00 enter
    simulator.click(340, 250, settle=0.05)  # PvP card -> S20
    simulator.capture(output / "native_pvp_wait_a.png")
    simulator.poll(0.35)
    simulator.capture(output / "native_pvp_wait_b.png")


def run_promotion(simulator: Simulator, output: Path) -> None:
    capabilities = simulator.rpc("capabilities")
    if capabilities.get("browser_input_allowed") is False:
        raise RuntimeError("simulator did not grant input control")
    simulator.poll(6.0)
    simulator.capture(output / "native_promotion_start.png")
    simulator.click(*square_center("a7"))
    simulator.click(*square_center("a8"), settle=2.0)
    simulator.capture(output / "native_promotion_choices.png")
    simulator.click(384, 273, settle=4.0)  # knight
    simulator.capture(output / "native_promotion_knight.png")


def run_checkmate(simulator: Simulator, output: Path) -> None:
    capabilities = simulator.rpc("capabilities")
    if capabilities.get("browser_input_allowed") is False:
        raise RuntimeError("simulator did not grant input control")
    simulator.poll(6.0)
    simulator.capture(output / "native_pvp_mate_start.png")
    simulator.click(*square_center("g6"))
    # The Host simulator intentionally keeps its default 2 MiB image cache,
    # so allow the cold full-screen result asset to decode.
    simulator.click(*square_center("g7"), settle=8.0)
    simulator.capture(output / "native_pvp_mate_result.png")


def run_top_piece_move(simulator: Simulator, output: Path) -> None:
    capabilities = simulator.rpc("capabilities")
    if capabilities.get("browser_input_allowed") is False:
        raise RuntimeError("simulator did not grant input control")
    simulator.poll(6.0)
    simulator.capture(output / "native_pvp_top_piece_after.png")


def run_bilingual(simulator: Simulator, output: Path) -> None:
    capabilities = simulator.rpc("capabilities")
    if capabilities.get("browser_input_allowed") is False:
        raise RuntimeError("simulator did not grant input control")
    simulator.poll(0.8)
    simulator.capture(output / "native_lang_home_zh.png")
    simulator.click(240, 40, settle=2.0)
    simulator.capture(output / "native_lang_home_en.png")
    simulator.click(240, 404, settle=4.0)
    simulator.capture(output / "native_lang_mode_en.png")
    simulator.click(137, 250, settle=4.0)
    simulator.capture(output / "native_lang_difficulty_en.png")
    simulator.click(240, 422, settle=4.0)
    # The first English full-screen decode is cold on the 2 MiB Host cache.
    simulator.click(340, 250, settle=4.0)
    simulator.capture(output / "native_lang_pair_wait_en.png")
    simulator.click(240, 240, settle=4.0)
    simulator.capture(output / "native_lang_pair_transition_en.png")
    # Allow the one-shot transition and first cold decode of the English
    # terminal frame to finish before pressing START.
    simulator.poll(8.0)
    simulator.capture(output / "native_lang_pair_ready_en.png")
    simulator.click(240, 422, settle=4.0)
    simulator.capture(output / "native_lang_wait_opponent_start_en.png")
    simulator.click(240, 240, settle=6.0)
    simulator.swipe((240, 380), (240, 120), settle=1.5)
    simulator.capture(output / "native_lang_game_menu_en.png")
    simulator.click(327, 263, settle=1.5)
    simulator.capture(output / "native_lang_resign_en.png")
    simulator.click(331, 302, settle=4.0)
    simulator.capture(output / "native_lang_result_en.png")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--url", required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--promotion-only", action="store_true")
    parser.add_argument("--pair-failure-only", action="store_true")
    parser.add_argument("--wait-only", action="store_true")
    parser.add_argument("--mate-only", action="store_true")
    parser.add_argument("--top-piece-only", action="store_true")
    parser.add_argument("--bilingual-only", action="store_true")
    args = parser.parse_args()
    host_port = args.url.removeprefix("http://").rstrip("/").split(":")
    simulator = Simulator(host_port[0], int(host_port[1]))
    if args.promotion_only:
        run_promotion(simulator, args.output)
    elif args.pair_failure_only:
        run_pair_failure(simulator, args.output)
    elif args.wait_only:
        run_wait_animation(simulator, args.output)
    elif args.mate_only:
        run_checkmate(simulator, args.output)
    elif args.top_piece_only:
        run_top_piece_move(simulator, args.output)
    elif args.bilingual_only:
        run_bilingual(simulator, args.output)
    else:
        run(simulator, args.output)
    print(f"native simulator replay evidence written to {args.output}")


if __name__ == "__main__":
    main()
