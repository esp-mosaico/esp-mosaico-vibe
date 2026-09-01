"""Argument parsing and stable output for the public mosaico.py command."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import sys
from typing import Any, Sequence

from . import __version__
from .commands import install, list_models, monitor, recover
from .errors import MosaicoError
from .runtime import RunContext


REPOSITORY = Path(__file__).resolve().parents[2]


class MosaicoArgumentParser(argparse.ArgumentParser):
    json_errors = False

    def error(self, message: str) -> None:
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
        raise argparse.ArgumentTypeError("必须是数字") from error
    if result <= 0:
        raise argparse.ArgumentTypeError("必须大于 0")
    return result


def monitor_timeout(value: str) -> float:
    try:
        result = float(value)
    except ValueError as error:
        raise argparse.ArgumentTypeError("必须是数字") from error
    if result < 0:
        raise argparse.ArgumentTypeError("不能小于 0")
    return result


def build_parser() -> argparse.ArgumentParser:
    parser = MosaicoArgumentParser(
        prog="mosaico.py",
        description="ESP-Mosaico 统一设备命令行",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
    )
    parser.add_argument("--json", action="store_true", help="输出稳定 JSON；monitor 输出 NDJSON")
    parser.add_argument("--verbose", action="store_true", help="显示内部阶段和完整日志路径")
    parser.add_argument("--version", action="version", version=f"%(prog)s {__version__}")
    commands = parser.add_subparsers(dest="command", required=True)

    install_parser = commands.add_parser(
        "install",
        help="通过 ESP-Iris 安装普通应用",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
    )
    install_parser.add_argument("--project", help="ESP-IDF 用户工程路径；默认自动选择")
    install_parser.add_argument("--device-id", help="目标 Device ID；单设备时自动选择")
    install_parser.add_argument("--gateway-profile", help="ESP-Iris profile；默认使用当前 profile")
    install_parser.add_argument("--skip-build", action="store_true", help="复用完整的已有构建")
    install_parser.add_argument(
        "--validation",
        choices=("elf-sha256", "version"),
        default="elf-sha256",
        help="安装后的固件身份验证方式",
    )
    install_parser.add_argument("--timeout", type=positive_timeout, default=600.0, help="安装超时秒数")

    recover_parser = commands.add_parser(
        "recover",
        help="恢复设备基础固件",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
    )
    recover_parser.add_argument("--model", help="设备型号；单一适配型号时自动选择")
    recover_parser.add_argument(
        "--source",
        choices=("reviewed", "current"),
        default="reviewed",
        help="评审基础包或当前源码候选包",
    )
    recover_parser.add_argument("--device-id", help="用于恢复前后身份关联的 Device ID")
    recover_parser.add_argument("--gateway-profile", help="ESP-Iris profile；默认使用当前 profile")
    recover_parser.add_argument("--timeout", type=positive_timeout, default=180.0, help="恢复及验证超时秒数")
    recover_parser.add_argument("--dry-run", action="store_true", help="只检查，不构建或写入")

    monitor_parser = commands.add_parser(
        "monitor",
        help="查看 ESP-Iris 保留日志",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
    )
    monitor_parser.add_argument("--device-id", help="目标 Device ID；单设备时自动选择")
    monitor_parser.add_argument("--gateway-profile", help="ESP-Iris profile；默认使用当前 profile")
    monitor_parser.add_argument("--timeout", type=monitor_timeout, default=0.0, help="跟随秒数；0 表示不限制")
    monitor_parser.add_argument("--snapshot", action="store_true", help="输出保留日志后退出")
    monitor_parser.add_argument("--grep", help="客户端文本过滤")

    list_parser = commands.add_parser(
        "list",
        help="列出仓库适配的设备型号",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
    )
    list_parser.add_argument("--details", action="store_true", help="显示参考工程、BSP 和 Recovery 基线")
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


def _print_table(result: dict[str, Any], details: bool) -> None:
    fields = ["id", "name", "target", "status", "default"]
    if details:
        fields.extend(["reference_project", "bsp_revision", "recovery_version"])
    headers = [field.upper() for field in fields]
    rows = [
        ["yes" if item.get(field) is True else "" if item.get(field) is False else str(item.get(field, "")) for field in fields]
        for item in result["models"]
    ]
    widths = [max(len(headers[index]), *(len(row[index]) for row in rows)) for index in range(len(fields))]
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
        if verbose and error.details:
            print(json.dumps(error.details, ensure_ascii=False, indent=2), file=sys.stderr)


def main(argv: Sequence[str] | None = None) -> int:
    raw = list(argv or sys.argv[1:])
    MosaicoArgumentParser.json_errors = "--json" in raw
    arguments = build_parser().parse_args(_normalize_globals(raw))
    if arguments.command == "list":
        try:
            result = list_models(REPOSITORY, arguments.details)
        except MosaicoError as error:
            _emit_error(error, arguments.json, arguments.verbose)
            return error.exit_code
        if arguments.json:
            print(json.dumps({"ok": True, **result}, ensure_ascii=False, sort_keys=True))
        else:
            _print_table(result, arguments.details)
        return 0

    context = RunContext(REPOSITORY, arguments.command, arguments.verbose)
    if arguments.verbose and not arguments.json:
        print(f"运行日志：{context.log_path}", file=sys.stderr)
    try:
        if arguments.command == "install":
            result = install(REPOSITORY, arguments, context)
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
            print("检查通过；未构建、未写入设备。")
        if arguments.verbose:
            print(json.dumps(result, ensure_ascii=False, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
