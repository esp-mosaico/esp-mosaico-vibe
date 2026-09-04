#!/usr/bin/env python3
"""JSON-RPC client for the ESP-GSP simulator application backend."""

from __future__ import annotations

import json
import socket
import time
from collections import deque


class SimBackend:
    """Loopback backend channel used by official `sim --backend-listen`."""

    def __init__(self, host: str = "127.0.0.1", port: int = 8684, timeout: float = 10.0):
        deadline = time.monotonic() + timeout
        last_error: OSError | None = None
        while time.monotonic() < deadline:
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            try:
                sock.settimeout(1.0)
                sock.connect((host, port))
                sock.settimeout(None)
                self.sock = sock
                self._buf = b""
                self._pending: deque[dict] = deque()
                self.next_id = 1
                return
            except OSError as error:
                last_error = error
                sock.close()
                time.sleep(0.05)
        raise ConnectionError(f"simulator backend {host}:{port}: {last_error}")

    def close(self) -> None:
        try:
            self.sock.close()
        except OSError:
            pass

    def call(self, method: str, params: dict | None = None) -> object:
        request_id = self.next_id
        self.next_id += 1
        body = json.dumps(
            {
                "jsonrpc": "2.0",
                "id": request_id,
                "method": method,
                "params": params or {},
            }
        ).encode("utf-8")
        self.sock.sendall(f"Content-Length: {len(body)}\r\n\r\n".encode("ascii") + body)
        while True:
            message = self._read()
            if message.get("id") == request_id:
                if "error" in message:
                    raise RuntimeError(message["error"])
                return message.get("result")
            if message.get("method"):
                self._pending.append(message)

    def poll(self, timeout: float) -> dict | None:
        if self._pending:
            return self._pending.popleft()
        self.sock.settimeout(timeout)
        try:
            return self._read()
        except TimeoutError:
            return None
        finally:
            self.sock.settimeout(None)

    def _read(self) -> dict:
        while b"\r\n\r\n" not in self._buf:
            chunk = self.sock.recv(4096)
            if not chunk:
                raise ConnectionError("simulator backend closed")
            self._buf += chunk
        header, _, rest = self._buf.partition(b"\r\n\r\n")
        length = int(header.split(b":", 1)[1].strip())
        body = rest
        while len(body) < length:
            chunk = self.sock.recv(length - len(body))
            if not chunk:
                raise ConnectionError("simulator backend closed")
            body += chunk
        self._buf = body[length:]
        return json.loads(body[:length])
