#!/usr/bin/env python3
"""Run the destructive ESP-Iris crash diagnosis matrix on one ESP-Mosaico."""

from __future__ import annotations

import argparse
import json
import secrets
import subprocess
import sys
import time
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[2]
MOSAICO = ROOT / "mosaico.py"
FIXTURE = ROOT / "tests" / "firmware" / "iris_crash"
HELLO_WORLD = ROOT / "projects" / "hello_world"
SERVICE_ID = "0x6a03"


class AcceptanceFailure(RuntimeError):
    pass


class Runner:
    def __init__(self, device_id: str | None, timeout: float) -> None:
        stamp = time.strftime("%Y%m%d-%H%M%S")
        self.evidence = ROOT / ".codex-runs" / "iris-crash" / stamp
        self.evidence.mkdir(parents=True)
        self.device_id = device_id
        self.timeout = timeout
        self.results: dict[str, Any] = {
            "schema": "esp-mosaico-iris-crash-acceptance/v1",
            "started_ns": time.time_ns(),
            "evidence_dir": str(self.evidence),
            "cases": [],
        }

    def command(
        self, name: str, *arguments: str, timeout: float | None = None
    ) -> dict[str, Any]:
        argv = [sys.executable, str(MOSAICO), "--json", *arguments]
        completed = subprocess.run(
            argv,
            cwd=ROOT,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            timeout=timeout or self.timeout,
            check=False,
        )
        record = {
            "argv": argv[2:],
            "returncode": completed.returncode,
            "stdout": completed.stdout,
            "stderr": completed.stderr,
        }
        (self.evidence / f"{name}.command.json").write_text(
            json.dumps(record, ensure_ascii=False, indent=2) + "\n",
            encoding="utf-8",
        )
        output = completed.stdout if completed.returncode == 0 else completed.stderr
        value = None
        for line in reversed(output.splitlines()):
            try:
                candidate = json.loads(line)
            except json.JSONDecodeError:
                continue
            if isinstance(candidate, dict):
                value = candidate
                break
        if value is None:
            raise AcceptanceFailure(
                f"{name} returned non-JSON output; see {name}.command.json"
            )
        if completed.returncode != 0 or value.get("ok") is not True:
            raise AcceptanceFailure(
                f"{name} failed: {value.get('message', value)}"
            )
        (self.evidence / f"{name}.json").write_text(
            json.dumps(value, ensure_ascii=False, indent=2) + "\n",
            encoding="utf-8",
        )
        return value

    def list_device(self, name: str = "list") -> dict[str, Any]:
        value = self.command(name, "list", "--details", timeout=20)
        devices = [
            item
            for item in value.get("devices", [])
            if item.get("online") and (
                self.device_id is None or item.get("device_id") == self.device_id
            )
        ]
        if len(devices) != 1:
            raise AcceptanceFailure(
                f"expected one selected online device, found {len(devices)}"
            )
        self.device_id = str(devices[0]["device_id"])
        return devices[0]

    def wait_for_boot(
        self,
        before: int | str,
        name: str,
        *,
        mode: str = "normal",
        timeout: float | None = None,
    ) -> dict[str, Any]:
        deadline = time.monotonic() + (timeout or self.timeout)
        last: Any = None
        while time.monotonic() < deadline:
            try:
                device = self.list_device(name)
            except AcceptanceFailure as error:
                last = str(error)
                time.sleep(0.5)
                continue
            last = device
            if (
                str(device.get("boot_id")) != str(before)
                and device.get("firmware_mode") == mode
            ):
                return device
            time.sleep(0.5)
        raise AcceptanceFailure(
            f"device did not reach a new {mode} boot; last={last}"
        )

    def wait_for_device(self, name: str) -> dict[str, Any]:
        deadline = time.monotonic() + self.timeout
        last: str | None = None
        while time.monotonic() < deadline:
            try:
                return self.list_device(name)
            except AcceptanceFailure as error:
                last = str(error)
                time.sleep(0.5)
        raise AcceptanceFailure(f"selected device did not come online; last={last}")

    def rpc(
        self, name: str, method: int, *, token: int | None = None
    ) -> dict[str, Any]:
        arguments = [
            "rpc",
            SERVICE_ID,
            str(method),
            "--device-id",
            str(self.device_id),
            "--deadline-ms",
            "5000",
        ]
        if token is not None:
            arguments.extend(("--payload-hex", token.to_bytes(8, "little").hex()))
        return self.command(name, *arguments, timeout=15)

    @staticmethod
    def crash_report(value: dict[str, Any]) -> dict[str, Any]:
        reports = value.get("report", {}).get("reports", [])
        if len(reports) != 1:
            raise AcceptanceFailure("ESP-Iris did not return one crash report")
        return reports[0]

    def inspect_crash(self, name: str) -> tuple[dict[str, Any], dict[str, Any]]:
        core = self.evidence / f"{name}.core.bin"
        value = self.command(
            f"{name}-crash",
            "crash",
            "--device-id",
            str(self.device_id),
            "--archive",
            "--save-core",
            str(core),
            timeout=150,
        )
        return self.crash_report(value), value.get("archive") or {}

    def assert_decoded_crash(
        self,
        name: str,
        before: dict[str, Any],
        report: dict[str, Any],
        archive: dict[str, Any],
        expected_reset_reason: int,
    ) -> None:
        diagnosis = archive.get("diagnosis") or {}
        checks = {
            "failed_boot_matches": str(report.get("crash_failed_boot_id"))
            == str(before.get("boot_id")),
            "reset_reason_matches": report.get("crash_origin_reset_reason")
            == expected_reset_reason,
            "core_valid": report.get("core_dump_valid") is True,
            "elf_complete": report.get("core_dump_elf_sha256_complete") is True,
            "decode_succeeded": diagnosis.get("status") == "succeeded",
            "source_confirmed": diagnosis.get("source_location_confirmed") is True,
            "incident_confirmed": diagnosis.get("incident_identity_confirmed") is True,
        }
        case = {
            "name": name,
            "boot_before": before,
            "report": report,
            "archive": archive,
            "checks": checks,
        }
        self.results["cases"].append(case)
        if not all(checks.values()):
            raise AcceptanceFailure(f"{name} diagnosis checks failed: {checks}")

    def reset_counter(self, name: str) -> None:
        self.rpc(f"{name}-reset", 7)
        report, _ = self.inspect_crash(f"{name}-after-reset")
        if report.get("crash_count") != 0:
            raise AcceptanceFailure(f"{name} crash counter did not reset")

    def run(self, refresh_recovery: bool) -> None:
        self.command("doctor", "doctor", timeout=120)
        if refresh_recovery:
            recovery_arguments = [
                "recover",
                "--source",
                "current",
                "--timeout",
                str(int(max(self.timeout, 240))),
            ]
            if self.device_id is not None:
                recovery_arguments.extend(("--device-id", self.device_id))
            self.command(
                "recover-current",
                *recovery_arguments,
                timeout=max(self.timeout, 900),
            )
        initial = self.wait_for_device("initial-device")
        self.results["device_id"] = self.device_id
        self.results["initial"] = initial
        fixture_installed = False
        try:
            fixture_installed = True
            install = self.command(
                "install-fixture",
                "install",
                "--project",
                str(FIXTURE),
                "--device-id",
                str(self.device_id),
                "--timeout",
                str(int(max(self.timeout, 180))),
                timeout=max(self.timeout, 900),
            )
            self.results["fixture_install"] = install
            current = self.list_device("fixture-device")

            baseline, _ = self.inspect_crash("baseline")
            token = secrets.randbits(63) or 1
            self.rpc("planned-restart-rpc", 5, token=token)
            restarted = self.wait_for_boot(
                current["boot_id"], "planned-restart-device"
            )
            planned_report, _ = self.inspect_crash("planned-restart")
            planned_checks = {
                "boot_changed": str(restarted.get("boot_id"))
                != str(current.get("boot_id")),
                "crash_count_unchanged": planned_report.get("crash_count")
                == baseline.get("crash_count"),
            }
            self.results["cases"].append(
                {
                    "name": "planned_restart",
                    "boot_before": current,
                    "boot_after": restarted,
                    "report": planned_report,
                    "checks": planned_checks,
                }
            )
            if not all(planned_checks.values()):
                raise AcceptanceFailure(
                    f"planned restart checks failed: {planned_checks}"
                )

            current = restarted
            for name, method, reset_reason in (
                ("assert", 2, 4),
                ("illegal_access", 3, 4),
                ("task_watchdog", 4, 6),
            ):
                token = secrets.randbits(63) or 1
                self.rpc(f"{name}-rpc", method, token=token)
                after = self.wait_for_boot(
                    current["boot_id"], f"{name}-device", timeout=90
                )
                report, archive = self.inspect_crash(name)
                self.assert_decoded_crash(
                    name, current, report, archive, reset_reason
                )
                self.results["cases"][-1]["boot_after"] = after
                self.reset_counter(name)
                current = after

            token = secrets.randbits(63) or 1
            self.rpc("startup-loop-rpc", 6, token=token)
            recovery = self.wait_for_boot(
                current["boot_id"],
                "startup-loop-recovery",
                mode="recovery",
                timeout=120,
            )
            report, archive = self.inspect_crash("startup_loop")
            diagnosis = archive.get("diagnosis") or {}
            checks = {
                "recovery_mode": recovery.get("firmware_mode") == "recovery",
                "loop_triggered": report.get("crash_loop_triggered") is True,
                "recovery_pending": report.get("crash_recovery_pending") is True,
                "crash_count": report.get("crash_count") == 3,
                "decode_succeeded": diagnosis.get("status") == "succeeded",
                "source_confirmed": diagnosis.get("source_location_confirmed") is True,
                "incident_confirmed": diagnosis.get(
                    "incident_identity_confirmed"
                ) is True,
            }
            self.results["cases"].append(
                {
                    "name": "startup_loop",
                    "boot_before": current,
                    "boot_after": recovery,
                    "report": report,
                    "archive": archive,
                    "checks": checks,
                }
            )
            if not all(checks.values()):
                raise AcceptanceFailure(
                    f"startup loop checks failed: {checks}"
                )
        finally:
            if fixture_installed:
                restore = self.command(
                    "restore-hello-world",
                    "install",
                    "--project",
                    str(HELLO_WORLD),
                    "--device-id",
                    str(self.device_id),
                    "--timeout",
                    str(int(max(self.timeout, 180))),
                    timeout=max(self.timeout, 900),
                )
                restored = self.list_device("restored-device")
                self.results["restore"] = {
                    "install": restore,
                    "device": restored,
                    "healthy": restored.get("project_name") == "hello_world"
                    and restored.get("firmware_mode") == "normal",
                }
                if not self.results["restore"]["healthy"]:
                    raise AcceptanceFailure("hello_world restoration was not verified")

    def save_result(self, status: str, error: str | None = None) -> Path:
        self.results["status"] = status
        self.results["finished_ns"] = time.time_ns()
        if error:
            self.results["error"] = error
        path = self.evidence / "result.json"
        path.write_text(
            json.dumps(self.results, ensure_ascii=False, indent=2) + "\n",
            encoding="utf-8",
        )
        return path


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--device-id")
    parser.add_argument("--timeout", type=float, default=240.0)
    parser.add_argument(
        "--skip-recovery-refresh",
        action="store_true",
        help="Keep the currently installed Recovery firmware",
    )
    arguments = parser.parse_args()
    runner = Runner(arguments.device_id, arguments.timeout)
    try:
        runner.run(not arguments.skip_recovery_refresh)
    except (AcceptanceFailure, subprocess.TimeoutExpired) as error:
        path = runner.save_result("failed", str(error))
        print(f"ESP-Iris crash acceptance FAILED: {error}", file=sys.stderr)
        print(f"Evidence: {path}", file=sys.stderr)
        return 1
    path = runner.save_result("passed")
    print("ESP-Iris crash acceptance PASSED")
    print(f"Evidence: {path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
