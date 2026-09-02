"""Convert verbose ESP-IDF build output into bounded progress events."""

from __future__ import annotations

import json
import re
import time
from typing import Callable


PROGRESS_PREFIX = "@@MOSAICO_BUILD_PROGRESS@@"
ANSI_RE = re.compile(r"\x1b(?:\[[0-?]*[ -/]*[@-~]|\][^\x07]*(?:\x07|\x1b\\))")
NINJA_PROGRESS_RE = re.compile(r"^\[\s*(\d+)\s*/\s*(\d+)\s*\]\s*(.*)$")
CONFIGURATION_DONE_RE = re.compile(r"^-- Configuring done(?: \(([^)]+)\))?")
DEPENDENCIES_RE = re.compile(r"^(?:NOTE:\s*)?Processing (\d+) dependencies:", re.I)


def encode_progress(message: str) -> str:
    """Encode an internal progress event for the parent mosaico process."""

    return PROGRESS_PREFIX + json.dumps(
        {"message": message}, ensure_ascii=True, separators=(",", ":")
    )


def decode_progress(raw_line: str) -> str | None:
    """Decode a progress event, ignoring ordinary low-noise runner output."""

    line = raw_line.strip()
    if not line.startswith(PROGRESS_PREFIX):
        return None
    try:
        value = json.loads(line[len(PROGRESS_PREFIX) :])
    except (json.JSONDecodeError, TypeError):
        return None
    message = value.get("message") if isinstance(value, dict) else None
    return message if isinstance(message, str) and message else None


def format_size(size_bytes: int) -> str:
    if size_bytes >= 1024 * 1024:
        return f"{size_bytes / (1024 * 1024):.2f} MiB"
    if size_bytes >= 1024:
        return f"{size_bytes / 1024:.1f} KiB"
    return f"{size_bytes} bytes"


class BuildProgressReporter:
    """Emit stable phase changes, coarse Ninja progress, and quiet-period heartbeats."""

    def __init__(
        self,
        emit: Callable[[str], None],
        *,
        heartbeat_interval: float = 10.0,
        clock: Callable[[], float] = time.monotonic,
    ) -> None:
        self.emit = emit
        self.heartbeat_interval = heartbeat_interval
        self.clock = clock
        now = clock()
        self.phase = "preparing"
        self.phase_started = now
        self.last_emit = now
        self.last_bucket = -1
        self.outer_total = 0
        self.seen_dependencies = False
        self.seen_link = False
        self.seen_image = False
        self.seen_size_check = False

    def _publish(self, message: str) -> None:
        self.emit(message)
        self.last_emit = self.clock()

    def _set_phase(self, phase: str, message: str | None = None) -> None:
        if phase == self.phase:
            return
        self.phase = phase
        self.phase_started = self.clock()
        if message:
            self._publish(message)

    def consume(self, raw_line: str) -> None:
        clean = ANSI_RE.sub("", raw_line).replace("\r", "\n")
        for candidate in clean.splitlines():
            line = candidate.strip()
            if line:
                self._consume_line(line)

    def _consume_line(self, line: str) -> None:
        if "Re-running CMake" in line or line.startswith("Running cmake in directory"):
            self._set_phase("configuring", "configuring (CMake)")
            return

        dependencies = DEPENDENCIES_RE.match(line)
        if dependencies and not self.seen_dependencies:
            self.seen_dependencies = True
            if self.phase == "preparing":
                self._set_phase("configuring", "configuring (CMake)")
            self._publish(f"resolving {dependencies.group(1)} dependencies")
            return

        configured = CONFIGURATION_DONE_RE.match(line)
        if configured:
            duration = f" ({configured.group(1)})" if configured.group(1) else ""
            self._publish(f"configuration complete{duration}")
            self._set_phase("generating")
            return

        ninja = NINJA_PROGRESS_RE.match(line)
        if ninja:
            completed, total = (int(value) for value in ninja.groups()[:2])
            description = ninja.group(3)
            if "Re-running CMake" in description:
                self._set_phase("configuring", "configuring (CMake)")
                return

            # ESP-IDF invokes a nested one-task Ninja build for the bootloader.
            # Keep the outer application's denominator so progress never jumps
            # to 100% and then moves backwards.
            if self.outer_total > 1 and total <= 1:
                return
            if not self.outer_total or total > self.outer_total:
                self.outer_total = total
                self.last_bucket = -1
            elif total != self.outer_total:
                return

            self._set_phase("building")
            self._consume_milestone(description)
            percent = min(100, completed * 100 // max(total, 1))
            bucket = percent // 10
            if bucket != self.last_bucket or completed == total:
                self.last_bucket = bucket
                self._publish(f"building {percent}% ({completed}/{total})")
            return

        self._consume_milestone(line)

    def _consume_milestone(self, description: str) -> None:
        lower = description.lower()
        if "linking " in lower and ".elf" in lower and not self.seen_link:
            self.seen_link = True
            self._set_phase("linking", "linking application")
        if "generating binary image" in lower and not self.seen_image:
            self.seen_image = True
            self._set_phase("image", "generating firmware image")
        if "check_sizes.py" in description and " partition " in description:
            if not self.seen_size_check:
                self.seen_size_check = True
                self._set_phase("size-check", "checking firmware size")

    def heartbeat(self, now: float | None = None) -> None:
        current = self.clock() if now is None else now
        if current - self.last_emit < self.heartbeat_interval:
            return
        labels = {
            "preparing": "preparing build environment",
            "configuring": "configuring",
            "generating": "generating build files",
            "building": "building",
            "linking": "linking application",
            "image": "generating firmware image",
            "size-check": "checking firmware size",
        }
        elapsed = max(0, int(current - self.phase_started))
        self._publish(f"{labels.get(self.phase, 'building')} ({elapsed}s elapsed)")

    def complete(
        self,
        *,
        duration_seconds: float,
        warnings: int,
        artifact_size: int | None,
    ) -> None:
        message = f"complete in {duration_seconds:.1f}s, warnings: {warnings}"
        if artifact_size is not None:
            message += f", firmware: {format_size(artifact_size)}"
        self._set_phase("complete", message)

    def failed(self, *, duration_seconds: float) -> None:
        self._set_phase("failed", f"failed after {duration_seconds:.1f}s")
