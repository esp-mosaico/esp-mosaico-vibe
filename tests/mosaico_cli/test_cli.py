from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import signal
import shutil
import subprocess
import sys
import tempfile
from types import SimpleNamespace
import unittest
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools"))

from mosaico_cli.cli import REPOSITORY, build_parser, _normalize_globals
from mosaico_cli.commands import list_models, monitor, recover
from mosaico_cli.errors import (
    DeviceError,
    EnvironmentError,
    OperationError,
    OutcomeUnknownError,
    SelectionError,
)
from mosaico_cli.gateway import (
    GatewaySession,
    _pid_alive,
    ensure_gateway,
    pause_managed_local_gateway,
    run_ota,
    select_device,
)
from mosaico_cli.project import resolve_project
from mosaico_cli.recovery import (
    _registered_recovery_ports,
    load_bundle,
    recovery_is_verified,
)
from mosaico_cli.registry import load_registry, select_model
from mosaico_cli.runtime import RunContext, run_idf_target


class ParserTests(unittest.TestCase):
    def parse(self, *argv: str) -> argparse.Namespace:
        return build_parser().parse_args(_normalize_globals(argv))

    def test_install_defaults(self) -> None:
        value = self.parse("install")
        self.assertEqual(value.validation, "elf-sha256")
        self.assertEqual(value.timeout, 600)
        self.assertFalse(value.skip_build)

    def test_recover_defaults(self) -> None:
        value = self.parse("recover")
        self.assertEqual(value.source, "reviewed")
        self.assertEqual(value.timeout, 180)
        self.assertFalse(value.dry_run)

    def test_monitor_defaults(self) -> None:
        value = self.parse("monitor")
        self.assertEqual(value.timeout, 0)
        self.assertFalse(value.snapshot)

    def test_global_flags_work_after_command(self) -> None:
        value = self.parse("list", "--details", "--json", "--verbose")
        self.assertTrue(value.json)
        self.assertTrue(value.verbose)
        self.assertTrue(value.details)

    def test_no_public_recovery_port(self) -> None:
        options = build_parser().format_help()
        recover_help = build_parser()._subparsers._group_actions[0].choices["recover"].format_help()
        self.assertNotIn("--port", options + recover_help)

    def test_recover_has_no_confirmation_option(self) -> None:
        with self.assertRaises(SystemExit) as caught:
            self.parse("recover", "--yes")
        self.assertEqual(caught.exception.code, 2)


class RegistryAndSelectionTests(unittest.TestCase):
    def test_registry_has_supported_default(self) -> None:
        models = load_registry()
        self.assertEqual(len(models), 1)
        self.assertEqual(select_model(None).target, "esp32s31")

    def test_list_has_no_runtime_dependency(self) -> None:
        result = list_models(REPOSITORY, False)
        self.assertEqual(result["models"][0]["id"], "esp-mosaico")

    def test_device_selection(self) -> None:
        item = {"device_id": "one"}
        self.assertIs(select_device([item], None), item)
        with self.assertRaises(SelectionError):
            select_device([item, {"device_id": "two"}], None)
        with self.assertRaises(DeviceError):
            select_device([], None)


class GatewayTests(unittest.TestCase):
    def test_zombie_gateway_is_not_alive(self) -> None:
        with (
            mock.patch("mosaico_cli.gateway.os.kill"),
            mock.patch.object(
                Path,
                "read_text",
                return_value="1234 (python) Z 1 1234 1234 0 -1",
            ),
        ):
            self.assertFalse(_pid_alive(1234))

    def test_unreachable_explicit_profile_never_starts_local_gateway(self) -> None:
        context = mock.Mock(repository=REPOSITORY)
        with (
            mock.patch("mosaico_cli.gateway.locate_iris_tools", return_value=(Path("python"), Path("iris"))),
            mock.patch("mosaico_cli.gateway._probe", return_value=False),
            mock.patch("mosaico_cli.gateway.subprocess.Popen") as start,
        ):
            with self.assertRaises(DeviceError):
                ensure_gateway(context, "remote")
        start.assert_not_called()

    def test_recovery_pauses_only_owned_local_gateway(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            state = Path(temporary)
            (state / "gateway.pid").write_text("1234\n", encoding="ascii")
            context = mock.Mock(repository=REPOSITORY)
            with (
                mock.patch("mosaico_cli.gateway._state_home", return_value=state),
                mock.patch(
                    "mosaico_cli.gateway.locate_iris_tools",
                    return_value=(Path("python"), Path("iris")),
                ),
                mock.patch("mosaico_cli.gateway._owned_local_gateway", return_value=True),
                mock.patch("mosaico_cli.gateway._pid_alive", side_effect=[True, False, False]),
                mock.patch("mosaico_cli.gateway.os.killpg") as terminate,
            ):
                self.assertTrue(pause_managed_local_gateway(context))
            terminate.assert_called_once_with(1234, signal.SIGTERM)
            self.assertFalse((state / "gateway.pid").exists())

    def test_unknown_ota_result_is_not_replayed(self) -> None:
        context = mock.Mock()
        context.log_path = Path("run.log")
        context.run.return_value = subprocess.CompletedProcess(
            [], 1, json.dumps({"operation": {"status": "outcome_unknown"}}), ""
        )
        session = GatewaySession(Path("python"), Path("iris"), (), None, False)
        with self.assertRaises(OutcomeUnknownError):
            run_ota(
                context,
                session,
                device_id="device",
                image=Path("app.bin"),
                elf=Path("app.elf"),
                map_file=Path("app.map"),
                validation="elf-sha256",
                timeout=600,
            )
        self.assertEqual(context.run.call_count, 1)

    def test_known_ota_failure_is_operation_error(self) -> None:
        context = mock.Mock()
        context.log_path = Path("run.log")
        context.run.return_value = subprocess.CompletedProcess(
            [], 1, json.dumps({"operation": {"status": "failed"}}), ""
        )
        session = GatewaySession(Path("python"), Path("iris"), (), None, False)
        with self.assertRaises(OperationError):
            run_ota(
                context,
                session,
                device_id="device",
                image=Path("app.bin"),
                elf=Path("app.elf"),
                map_file=Path("app.map"),
                validation="version",
                timeout=600,
            )

    def test_ota_progress_is_polled_and_reported(self) -> None:
        context = mock.Mock()
        context.log_path = Path("run.log")
        context.run.return_value = subprocess.CompletedProcess(
            [],
            0,
            json.dumps(
                {
                    "operation": {
                        "operation_id": "operation-1",
                        "status": "running",
                        "progress": {
                            "stage": "waiting_recovery",
                            "progress_permille": 50,
                        },
                    }
                }
            ),
            "",
        )
        completed = {
            "operation": {
                "operation_id": "operation-1",
                "status": "succeeded",
                "progress": {
                    "stage": "succeeded",
                    "progress_permille": 1000,
                },
            }
        }
        session = GatewaySession(Path("python"), Path("iris"), (), None, False)
        with (
            mock.patch("mosaico_cli.gateway.gateway_json", return_value=completed) as poll,
            mock.patch("mosaico_cli.gateway.time.sleep"),
        ):
            result = run_ota(
                context,
                session,
                device_id="device",
                image=Path("app.bin"),
                elf=Path("app.elf"),
                map_file=Path("app.map"),
                validation="elf-sha256",
                timeout=600,
            )
        self.assertEqual(result["operation"]["status"], "succeeded")
        poll.assert_called_once_with(context, session, "ota-status", "operation-1")
        messages = [call.args[0] for call in context.status.call_args_list]
        self.assertTrue(any("waiting_recovery" in message for message in messages))
        self.assertTrue(any("succeeded" in message for message in messages))

    def test_monitor_forces_unbuffered_child_output(self) -> None:
        arguments = argparse.Namespace(
            gateway_profile=None,
            device_id=None,
            snapshot=True,
            timeout=1,
            grep=None,
        )
        context = mock.Mock(repository=REPOSITORY)
        session = GatewaySession(Path("python"), Path("iris"), (), None, False)
        read_fd, write_fd = os.pipe()
        os.close(write_fd)
        process = mock.Mock()
        process.stdout = os.fdopen(read_fd, "rb", buffering=0)
        process.poll.return_value = 0
        process.wait.return_value = 0
        try:
            with (
                mock.patch("mosaico_cli.commands.ensure_gateway", return_value=session),
                mock.patch(
                    "mosaico_cli.commands.connected_devices",
                    return_value=[{"device_id": "device"}],
                ),
                mock.patch("mosaico_cli.commands.subprocess.Popen", return_value=process) as start,
            ):
                self.assertEqual(monitor(REPOSITORY, arguments, context, False), 0)
            self.assertEqual(start.call_args.kwargs["env"]["PYTHONUNBUFFERED"], "1")
        finally:
            process.stdout.close()


class ProjectTests(unittest.TestCase):
    def _project(self, root: Path, name: str) -> Path:
        path = root / "projects" / name
        path.mkdir(parents=True)
        (path / "CMakeLists.txt").write_text("project(example)\n", encoding="utf-8")
        return path

    def test_single_user_project_is_selected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            expected = self._project(root, "demo")
            self._project(root, "factory")
            self.assertEqual(resolve_project(root, None, root), expected)

    def test_factory_is_not_implicitly_selected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            self._project(root, "factory")
            with self.assertRaises(SelectionError):
                resolve_project(root, None, root)

    def test_multiple_projects_require_selection(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            self._project(root, "a")
            self._project(root, "b")
            with self.assertRaises(SelectionError) as caught:
                resolve_project(root, None, root)
            self.assertEqual(len(caught.exception.details["candidates"]), 2)


class RecoveryBundleTests(unittest.TestCase):
    def test_reviewed_bundle_hashes(self) -> None:
        model = select_model(None)
        value = load_bundle(REPOSITORY / model.recovery_dir, model.target)
        self.assertEqual(value["schema_version"], 2)
        self.assertEqual(set(value["images"]), {"bootloader", "partition_table", "ota_data", "recovery"})

    def test_corrupt_bundle_is_rejected(self) -> None:
        source = REPOSITORY / select_model(None).recovery_dir
        with tempfile.TemporaryDirectory() as temporary:
            target = Path(temporary) / "recovery"
            shutil.copytree(source, target)
            with (target / "ota_data_initial.bin").open("ab") as stream:
                stream.write(b"corrupt")
            with self.assertRaises(EnvironmentError):
                load_bundle(target, "esp32s31")

    def test_manifest_hashes_match_files(self) -> None:
        directory = REPOSITORY / select_model(None).recovery_dir
        value = json.loads((directory / "manifest.json").read_text(encoding="utf-8"))
        for item in value["images"].values():
            digest = hashlib.sha256((directory / item["file"]).read_bytes()).hexdigest()
            self.assertEqual(digest, item["sha256"])

    def test_live_recovery_must_match_reviewed_version(self) -> None:
        status = {
            "firmware_mode": "recovery",
            "app_version": "2.0.0-recovery",
            "capability_names": ["ota"],
        }
        self.assertFalse(
            recovery_is_verified("device", status, "2.1.0-recovery")
        )
        status["app_version"] = "2.1.0-recovery"
        self.assertTrue(
            recovery_is_verified("device", status, "2.1.0-recovery")
        )


class RecoveryCommandTests(unittest.TestCase):
    def test_registered_esp32s31_recovery_device_is_detected(self) -> None:
        port = SimpleNamespace(
            device="/dev/ttyACM-test",
            vid=0x303A,
            pid=0x0020,
            product="ESP32-S31",
        )
        with mock.patch("serial.tools.list_ports.comports", return_value=[port]):
            self.assertEqual(
                _registered_recovery_ports(select_model(None)), ["/dev/ttyACM-test"]
            )

    def test_unregistered_serial_device_is_rejected(self) -> None:
        port = SimpleNamespace(
            device="/dev/ttyACM9",
            vid=0x1234,
            pid=0x5678,
            product="Other device",
        )
        with mock.patch("serial.tools.list_ports.comports", return_value=[port]):
            self.assertEqual(_registered_recovery_ports(select_model(None)), [])

    def test_current_dry_run_does_not_build_or_flash(self) -> None:
        arguments = argparse.Namespace(
            model=None,
            source="current",
            device_id=None,
            gateway_profile=None,
            timeout=180,
            dry_run=True,
            yes=False,
        )
        context = mock.Mock(repository=REPOSITORY)
        context.log_path = REPOSITORY / ".codex-runs" / "test.log"
        context.note = mock.Mock()
        with (
            mock.patch("mosaico_cli.commands.ensure_gateway", side_effect=DeviceError("offline")),
            mock.patch("mosaico_cli.commands.provisioning_candidate", return_value="internal-device"),
            mock.patch("mosaico_cli.commands.resolve_idf_path", return_value=Path("/idf")),
            mock.patch("mosaico_cli.commands.run_idf_target") as target,
        ):
            result = recover(REPOSITORY, arguments, context)
        self.assertEqual(result["status"], "dry_run")
        target.assert_not_called()
        messages = [call.args[0] for call in context.status.call_args_list]
        self.assertTrue(any(message.startswith("recovery: model=") for message in messages))
        self.assertTrue(any(message.startswith("device: recovery interface") for message in messages))
        self.assertTrue(any(message.startswith("validation:") for message in messages))

    def test_idf_wrapper_invokes_only_named_target(self) -> None:
        context = mock.Mock()
        context.repository = REPOSITORY
        context.log_path = Path("run.log")
        context.run.return_value = subprocess.CompletedProcess([], 0, "", "")
        run_idf_target(
            context,
            idf_path=Path("/idf"),
            project=Path("/project"),
            build_dir=Path("/build"),
            target="mosaico-recover-flash",
            definitions={"MOSAICO_RECOVERY_SOURCE": "reviewed"},
            port="internal-device",
            timeout=180,
        )
        argv = context.run.call_args.args[0]
        environment = context.run.call_args.kwargs["env"]
        self.assertIn("mosaico-recover-flash", argv)
        self.assertNotIn("-p", argv)
        self.assertEqual(environment["ESPPORT"], "internal-device")
        self.assertNotIn("esptool", " ".join(str(item) for item in argv))
        self.assertNotIn("erase", " ".join(str(item) for item in argv))
        progress = context.run.call_args.kwargs["output_status"]
        self.assertEqual(progress("[10/100] compiling\n"), "idf: building 10% (10/100)")
        self.assertEqual(
            progress("Writing at 0x00002000... ( 50 % )\n"),
            "flash: writing 50% at 0x00002000",
        )

    def test_busy_recovery_port_is_reported_without_retry(self) -> None:
        context = mock.Mock(repository=REPOSITORY, log_path=Path("run.log"))
        context.run.return_value = subprocess.CompletedProcess(
            [], 1, "Could not exclusively lock port: port is busy", ""
        )
        with self.assertRaises(DeviceError):
            run_idf_target(
                context,
                idf_path=Path("/idf"),
                project=Path("/project"),
                build_dir=Path("/build"),
                target="mosaico-recover-flash",
                port="internal-device",
                timeout=180,
            )
        self.assertEqual(context.run.call_count, 1)

    def test_run_context_streams_selected_child_status(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            context = RunContext(Path(temporary), "stream-test")
            with mock.patch.object(context, "status") as status:
                result = context.run(
                    [sys.executable, "-c", "print('streamed child output')"],
                    output_status=lambda line: f"child: {line.strip()}",
                )
            self.assertEqual(result.returncode, 0)
            status.assert_called_once_with("child: streamed child output")
            self.assertIn("streamed child output", context.log_path.read_text())


if __name__ == "__main__":
    unittest.main()
