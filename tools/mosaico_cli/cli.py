"""Argument parsing and stable output for the public mosaico.py command."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import sys
from typing import Any, Never, Sequence
from urllib.parse import urlsplit

from . import __version__
from .commands import install, list_devices, monitor, recover, start_http_system_update
from .doctor import diagnose_host, print_diagnosis
from .errors import MosaicoError
from .runtime import RunContext


REPOSITORY = Path(__file__).resolve().parents[2]


class MosaicoArgumentParser(argparse.ArgumentParser):
    json_errors = False

    def error(self, message: str) -> Never:
        if self.json_errors:
            print(
                json.dumps(
                    {
                        "ok": False,
                        "error": "selection_error",
                        "message": message,
                        "exit_code": 2,
                    },
                    ensure_ascii=False,
                    sort_keys=True,
                ),
                file=sys.stderr,
            )
            raise SystemExit(2)
        super().error(message)


def positive_timeout(value: str) -> float:
    try:
        result = float(value)
    except ValueError as error:
        raise argparse.ArgumentTypeError("must be a number") from error
    if result <= 0:
        raise argparse.ArgumentTypeError("must be greater than 0")
    return result


def monitor_timeout(value: str) -> float:
    try:
        result = float(value)
    except ValueError as error:
        raise argparse.ArgumentTypeError("must be a number") from error
    if result < 0:
        raise argparse.ArgumentTypeError("must not be less than 0")
    return result


def http_manifest_url(value: str) -> str:
    parsed = urlsplit(value)
    if parsed.scheme not in {"http", "https"} or not parsed.netloc:
        raise argparse.ArgumentTypeError("must be an absolute HTTP(S) URL")
    if not parsed.path or parsed.path.endswith("/"):
        raise argparse.ArgumentTypeError("must name a manifest file")
    if len(value.encode("utf-8")) >= 512:
        raise argparse.ArgumentTypeError("must be shorter than 512 UTF-8 bytes")
    return value


def build_parser() -> argparse.ArgumentParser:
    parser = MosaicoArgumentParser(
        prog="mosaico.py",
        description="Unified ESP-Mosaico device command-line tool",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
    )
    parser.add_argument(
        "--json", action="store_true", help="Emit stable JSON; monitor emits NDJSON"
    )
    parser.add_argument(
        "--verbose", action="store_true", help="Show internal stages and full log paths"
    )
    parser.add_argument("--version", action="version", version=f"%(prog)s {__version__}")
    commands = parser.add_subparsers(dest="command", required=True)

    commands.add_parser(
        "doctor",
        help="Check the host environment without building or writing a device",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
    )

    install_parser = commands.add_parser(
        "install",
        help="Install a normal application through ESP-Iris",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
    )
    install_parser.add_argument(
        "--project", help="ESP-IDF application path; selected automatically by default"
    )
    install_parser.add_argument(
        "--device-id", help="Target Device ID; selected automatically when only one is available"
    )
    install_parser.add_argument(
        "--gateway-profile", help="ESP-Iris profile; use the current profile by default"
    )
    install_parser.add_argument(
        "--skip-build", action="store_true", help="Reuse a complete existing build"
    )
    install_parser.add_argument(
        "--validation",
        choices=("elf-sha256", "version"),
        default="elf-sha256",
        help="Firmware identity validation method after installation",
    )
    install_parser.add_argument(
        "--timeout", type=positive_timeout, default=600.0, help="Installation timeout in seconds"
    )

    system_update_parser = commands.add_parser(
        "system-update",
        help="Start a Recovery HTTP(S) full-system update",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
    )
    system_update_parser.add_argument(
        "--manifest-url",
        required=True,
        type=http_manifest_url,
        help="Absolute URL of the exploded bundle manifest.json",
    )
    system_update_parser.add_argument(
        "--device-id",
        help="Target Device ID; selected automatically when only one is available",
    )
    system_update_parser.add_argument(
        "--gateway-profile", help="ESP-Iris profile; use the current profile by default"
    )

    recover_parser = commands.add_parser(
        "recover",
        help="Restore the device base firmware",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
    )
    recover_parser.add_argument(
        "--model", help="Device model; selected automatically when only one is supported"
    )
    recover_parser.add_argument(
        "--source",
        choices=("reviewed", "current"),
        default="reviewed",
        help="Reviewed base bundle or current-source candidate bundle",
    )
    recover_parser.add_argument(
        "--device-id", help="Device ID used to correlate identity before and after recovery"
    )
    recover_parser.add_argument(
        "--timeout", type=positive_timeout, default=180.0,
        help="Recovery and validation timeout in seconds",
    )
    recover_parser.add_argument(
        "--dry-run", action="store_true", help="Check only; do not build or write firmware"
    )

    monitor_parser = commands.add_parser(
        "monitor",
        help="View retained ESP-Iris logs",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
    )
    monitor_parser.add_argument(
        "--device-id", help="Target Device ID; selected automatically when only one is available"
    )
    monitor_parser.add_argument(
        "--gateway-profile", help="ESP-Iris profile; use the current profile by default"
    )
    monitor_parser.add_argument(
        "--timeout", type=monitor_timeout, default=0.0,
        help="Follow duration in seconds; 0 means no limit",
    )
    monitor_parser.add_argument(
        "--snapshot", action="store_true", help="Print retained logs and exit"
    )
    monitor_parser.add_argument("--grep", help="Client-side text filter")
    monitor_colors = monitor_parser.add_mutually_exclusive_group()
    monitor_colors.add_argument(
        "--force-color", action="store_true", help="Always emit ANSI log colors"
    )
    monitor_colors.add_argument(
        "--disable-auto-color",
        action="store_true",
        help="Disable automatic log coloring",
    )

    list_parser = commands.add_parser(
        "list",
        help="List devices visible through ESP-Iris",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
    )
    list_parser.add_argument(
        "--gateway-profile", help="ESP-Iris profile; use the local Gateway by default"
    )
    list_parser.add_argument(
        "--details",
        action="store_true",
        help="Show endpoint, ESP-IDF version, session, and capabilities",
    )
    return parser


def _normalize_globals(argv: Sequence[str]) -> list[str]:
    globals_found: list[str] = []
    rest: list[str] = []
    for value in argv:
        if value in {"--json", "--verbose", "--version"}:
            globals_found.append(value)
        else:
            rest.append(value)
    return [*globals_found, *rest]


def _print_device_table(result: dict[str, Any], details: bool) -> None:
    fields = [
        "device_id",
        "online",
        "connection",
        "alias",
        "project_name",
        "app_version",
        "firmware_mode",
        "boot_id",
    ]
    if details:
        fields.extend(["endpoint", "idf_version", "session_id", "capability_names"])
    headers = [field.upper() for field in fields]

    def cell(item: dict[str, Any], field: str) -> str:
        if field == "online":
            return "yes" if item.get(field) else "no"
        if field == "alias":
            return str(item.get("alias") or item.get("suggested_alias") or "")
        if field == "capability_names":
            values = item.get(field, [])
            return ",".join(str(value) for value in values) if isinstance(values, list) else ""
        return str(item.get(field, ""))

    rows = [
        [cell(item, field) for field in fields]
        for item in result["devices"]
    ]
    widths = [
        max(len(headers[index]), *(len(row[index]) for row in rows))
        for index in range(len(fields))
    ]
    print("  ".join(headers[index].ljust(widths[index]) for index in range(len(fields))))
    for row in rows:
        print("  ".join(row[index].ljust(widths[index]) for index in range(len(fields))))


def _emit_error(error: MosaicoError, json_output: bool, verbose: bool) -> None:
    if json_output:
        print(
            json.dumps(
                {
                    "ok": False,
                    "error": error.category,
                    "message": str(error),
                    "exit_code": error.exit_code,
                    "details": error.details,
                },
                ensure_ascii=False,
                sort_keys=True,
            ),
            file=sys.stderr,
        )
    else:
        print(f"mosaico: {error}", file=sys.stderr)
        candidates = error.details.get("candidates")
        if isinstance(candidates, list) and candidates:
            print("Available Device IDs:", file=sys.stderr)
            for candidate in candidates:
                print(f"  {candidate}", file=sys.stderr)
        diagnostic = error.details.get("diagnostic")
        if diagnostic:
            print(diagnostic, file=sys.stderr)
        build_log_dir = error.details.get("build_log_dir")
        if build_log_dir:
            print(f"Build logs: {build_log_dir}", file=sys.stderr)
        if verbose and error.details:
            print(json.dumps(error.details, ensure_ascii=False, indent=2), file=sys.stderr)


def main(argv: Sequence[str] | None = None) -> int:
    raw = list(argv or sys.argv[1:])
    MosaicoArgumentParser.json_errors = "--json" in raw
    arguments = build_parser().parse_args(_normalize_globals(raw))
    if sys.version_info < (3, 11):
        message = "mosaico.py requires Python 3.11 or newer."
        if arguments.json:
            print(
                json.dumps(
                    {
                        "ok": False,
                        "error": "environment_error",
                        "message": message,
                        "exit_code": 3,
                    },
                    sort_keys=True,
                ),
                file=sys.stderr,
            )
        else:
            print(f"mosaico: {message}", file=sys.stderr)
        return 3
    if arguments.command == "list":
        context = RunContext(
            REPOSITORY,
            arguments.command,
            arguments.verbose,
            arguments.json,
        )
        try:
            result = list_devices(context, arguments.gateway_profile)
        except MosaicoError as error:
            error.details.setdefault("log", str(context.log_path))
            _emit_error(error, arguments.json, arguments.verbose)
            return error.exit_code
        if arguments.json:
            print(json.dumps({"ok": True, **result}, ensure_ascii=False, sort_keys=True))
        else:
            _print_device_table(result, arguments.details)
        return 0

    if arguments.command == "doctor":
        result = diagnose_host(REPOSITORY)
        if arguments.json:
            print(
                json.dumps(
                    {"ok": result["exit_code"] == 0, **result},
                    ensure_ascii=False,
                    sort_keys=True,
                )
            )
        else:
            print_diagnosis(result)
        return int(result["exit_code"])

    context = RunContext(
        REPOSITORY,
        arguments.command,
        arguments.verbose,
        arguments.json,
    )
    if arguments.verbose and not arguments.json:
        print(f"Run log: {context.log_path}", file=sys.stderr)
    try:
        if arguments.command == "install":
            result = install(REPOSITORY, arguments, context)
        elif arguments.command == "system-update":
            result = start_http_system_update(arguments, context)
        elif arguments.command == "recover":
            result = recover(REPOSITORY, arguments, context)
        else:
            return monitor(REPOSITORY, arguments, context, arguments.json)
    except MosaicoError as error:
        error.details.setdefault("log", str(context.log_path))
        _emit_error(error, arguments.json, arguments.verbose)
        return error.exit_code
    except KeyboardInterrupt:
        return 0 if arguments.command == "monitor" else 5
    if arguments.json:
        print(json.dumps({"ok": True, **result}, ensure_ascii=False, sort_keys=True))
    else:
        status = result.get("status", "succeeded")
        print(f"{arguments.command}: {status}")
        if arguments.command == "recover" and status == "dry_run":
            print("Checks passed; no firmware was built or written.")
        if arguments.verbose:
            print(json.dumps(result, ensure_ascii=False, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
