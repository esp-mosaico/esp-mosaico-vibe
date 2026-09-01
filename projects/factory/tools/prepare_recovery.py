#!/usr/bin/env python3
"""Validate and atomically stage a complete ESP-Mosaico recovery bundle."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys
from typing import Any


SCHEMA_VERSION = 2
PROJECT_NAME = "factory"
ESP_IMAGE_MAGIC = 0xE9
IMAGE_FILES = {
    "bootloader": "bootloader.bin",
    "partition_table": "partition-table.bin",
    "ota_data": "ota_data_initial.bin",
    "recovery": "factory.bin",
}
RECOVERY_SOURCE_PATHS = (
    "CMakeLists.txt",
    "bootloader_components",
    "cmake",
    "main",
    "partitions.csv",
    "sdkconfig.defaults",
    "sdkconfig.recovery.defaults",
    "tools",
)


class RecoveryImageError(RuntimeError):
    """Raised when a recovery bundle is missing or incompatible."""


def parse_int(value: str | int) -> int:
    if isinstance(value, int):
        return value
    try:
        return int(value, 0)
    except (TypeError, ValueError) as error:
        raise RecoveryImageError(f"invalid integer value: {value!r}") from error


def parse_bool(value: str | bool) -> bool:
    if isinstance(value, bool):
        return value
    normalized = value.strip().lower()
    if normalized in {"1", "true", "yes", "on"}:
        return True
    if normalized in {"0", "false", "no", "off"}:
        return False
    raise RecoveryImageError(f"invalid boolean value: {value!r}")


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def read_json(path: Path) -> dict[str, Any]:
    try:
        value = json.loads(path.read_text(encoding="utf-8"))
    except FileNotFoundError as error:
        raise RecoveryImageError(f"file does not exist: {path}") from error
    except json.JSONDecodeError as error:
        raise RecoveryImageError(f"invalid JSON in {path}: {error}") from error
    if not isinstance(value, dict):
        raise RecoveryImageError(f"expected a JSON object in {path}")
    return value


def enabled_config(config: str, name: str) -> bool:
    return f"{name}=y" in config.splitlines()


def source_state(source_root: Path) -> tuple[str, bool]:
    try:
        commit = subprocess.run(
            ["git", "-C", str(source_root), "rev-parse", "HEAD"],
            check=True,
            capture_output=True,
            text=True,
        ).stdout.strip()
        changes = subprocess.run(
            ["git", "-C", str(source_root), "status", "--porcelain", "--", *RECOVERY_SOURCE_PATHS],
            check=True,
            capture_output=True,
            text=True,
        ).stdout.strip()
    except (FileNotFoundError, subprocess.CalledProcessError):
        return "unknown", True
    return commit or "unknown", bool(changes)


def build_manifest(
    images: dict[str, Path],
    description_path: Path,
    offsets: dict[str, int],
    partition_size: int,
    normal_offset: int,
    source_root: Path,
) -> dict[str, Any]:
    description = read_json(description_path)
    try:
        config_path = Path(description["config_file"])
        config = config_path.read_text(encoding="utf-8")
    except (KeyError, FileNotFoundError, OSError) as error:
        raise RecoveryImageError(
            f"cannot read sdkconfig referenced by {description_path}"
        ) from error
    if not enabled_config(config, "CONFIG_GET_STARTED_RECOVERY"):
        raise RecoveryImageError("bundle was not produced by BUILD_PROFILE=recovery")
    source_commit, source_dirty = source_state(source_root)
    return {
        "schema_version": SCHEMA_VERSION,
        "project": description.get("project_name", ""),
        "profile": "recovery",
        "version": description.get("project_version", ""),
        "target": description.get("target", ""),
        "idf_version": description.get("git_revision", ""),
        "source": {"commit": source_commit, "dirty": source_dirty},
        "layout": {
            "recovery_partition": {
                "name": "factory",
                "offset": f"0x{offsets['recovery']:x}",
                "size": f"0x{partition_size:x}",
            },
            "application_partition": {
                "name": "ota_0",
                "offset": f"0x{normal_offset:x}",
            },
        },
        "initial_boot": {
            "partition": "factory",
            "ota_data_image": IMAGE_FILES["ota_data"],
            "expected_mode": "recovery",
        },
        "security": {
            "secure_boot": enabled_config(config, "CONFIG_SECURE_BOOT"),
            "flash_encryption": enabled_config(config, "CONFIG_SECURE_FLASH_ENC_ENABLED"),
        },
        "images": {
            name: {
                "file": IMAGE_FILES[name],
                "offset": f"0x{offsets[name]:x}",
                "size": path.stat().st_size,
                "sha256": sha256(path),
            }
            for name, path in images.items()
        },
    }


def require_mapping(value: dict[str, Any], key: str) -> dict[str, Any]:
    result = value.get(key)
    if not isinstance(result, dict):
        raise RecoveryImageError(f"manifest field {key!r} must be an object")
    return result


def validate(
    images: dict[str, Path],
    manifest: dict[str, Any],
    target: str,
    offsets: dict[str, int],
    partition_size: int,
    normal_offset: int,
    secure_boot: bool,
    flash_encryption: bool,
) -> None:
    if manifest.get("schema_version") != SCHEMA_VERSION:
        raise RecoveryImageError(
            f"unsupported recovery manifest schema: {manifest.get('schema_version')!r}"
        )
    if manifest.get("project") != PROJECT_NAME or manifest.get("profile") != "recovery":
        raise RecoveryImageError("manifest does not describe the factory recovery build")
    if manifest.get("target") != target:
        raise RecoveryImageError(
            f"recovery target mismatch: expected {target!r}, got {manifest.get('target')!r}"
        )
    layout = require_mapping(manifest, "layout")
    recovery_partition = require_mapping(layout, "recovery_partition")
    application_partition = require_mapping(layout, "application_partition")
    if recovery_partition.get("name") != "factory":
        raise RecoveryImageError("recovery partition must be named factory")
    if parse_int(recovery_partition.get("offset", "")) != offsets["recovery"]:
        raise RecoveryImageError("factory partition offset changed; rebuild recovery bundle")
    if parse_int(recovery_partition.get("size", "")) != partition_size:
        raise RecoveryImageError("factory partition size changed; rebuild recovery bundle")
    if application_partition.get("name") != "ota_0" or parse_int(
        application_partition.get("offset", "")
    ) != normal_offset:
        raise RecoveryImageError("normal application partition layout changed")
    security = require_mapping(manifest, "security")
    if security.get("secure_boot") is not secure_boot:
        raise RecoveryImageError("recovery secure-boot configuration differs")
    if security.get("flash_encryption") is not flash_encryption:
        raise RecoveryImageError("recovery flash-encryption configuration differs")

    entries = require_mapping(manifest, "images")
    for name, path in images.items():
        if not path.is_file() or path.stat().st_size == 0:
            raise RecoveryImageError(f"missing or empty recovery bundle image: {path}")
        item = require_mapping(entries, name)
        if item.get("file") != IMAGE_FILES[name]:
            raise RecoveryImageError(f"unexpected file name for {name}")
        if parse_int(item.get("offset", "")) != offsets[name]:
            raise RecoveryImageError(f"offset mismatch for {name}")
        if item.get("size") != path.stat().st_size or item.get("sha256") != sha256(path):
            raise RecoveryImageError(f"size or SHA-256 mismatch for {name}")
    if images["recovery"].stat().st_size > partition_size:
        raise RecoveryImageError("recovery application is larger than the factory partition")
    for name in ("bootloader", "recovery"):
        with images[name].open("rb") as stream:
            if stream.read(1) != bytes([ESP_IMAGE_MAGIC]):
                raise RecoveryImageError(f"invalid ESP image magic: {images[name]}")
    initial = require_mapping(manifest, "initial_boot")
    if initial.get("partition") != "factory" or initial.get("expected_mode") != "recovery":
        raise RecoveryImageError("bundle does not initialize into Recovery")


def atomic_stage(output_dir: Path, images: dict[str, Path], manifest: dict[str, Any]) -> None:
    output_dir.parent.mkdir(parents=True, exist_ok=True)
    temporary = output_dir.with_name(output_dir.name + ".tmp")
    previous = output_dir.with_name(output_dir.name + ".previous")
    shutil.rmtree(temporary, ignore_errors=True)
    shutil.rmtree(previous, ignore_errors=True)
    temporary.mkdir()
    for name, source in images.items():
        shutil.copyfile(source, temporary / IMAGE_FILES[name])
    (temporary / "manifest.json").write_text(
        json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )
    if output_dir.exists():
        output_dir.replace(previous)
    temporary.replace(output_dir)
    shutil.rmtree(previous, ignore_errors=True)


def parser() -> argparse.ArgumentParser:
    result = argparse.ArgumentParser(description=__doc__)
    result.add_argument("--bootloader", type=Path, required=True)
    result.add_argument("--partition-table", type=Path, required=True)
    result.add_argument("--ota-data", type=Path, required=True)
    result.add_argument("--recovery", type=Path, required=True)
    metadata = result.add_mutually_exclusive_group(required=True)
    metadata.add_argument("--manifest", type=Path)
    metadata.add_argument("--project-description", type=Path)
    result.add_argument("--source-root", type=Path, default=Path.cwd())
    result.add_argument("--target", required=True)
    result.add_argument("--bootloader-offset", required=True)
    result.add_argument("--partition-table-offset", required=True)
    result.add_argument("--ota-data-offset", required=True)
    result.add_argument("--recovery-offset", required=True)
    result.add_argument("--recovery-size", required=True)
    result.add_argument("--normal-offset", required=True)
    result.add_argument("--secure-boot", required=True)
    result.add_argument("--flash-encryption", required=True)
    result.add_argument("--output-dir", type=Path, required=True)
    return result


def main() -> int:
    arguments = parser().parse_args()
    try:
        images = {
            "bootloader": arguments.bootloader,
            "partition_table": arguments.partition_table,
            "ota_data": arguments.ota_data,
            "recovery": arguments.recovery,
        }
        offsets = {
            "bootloader": parse_int(arguments.bootloader_offset),
            "partition_table": parse_int(arguments.partition_table_offset),
            "ota_data": parse_int(arguments.ota_data_offset),
            "recovery": parse_int(arguments.recovery_offset),
        }
        partition_size = parse_int(arguments.recovery_size)
        normal_offset = parse_int(arguments.normal_offset)
        secure_boot = parse_bool(arguments.secure_boot)
        flash_encryption = parse_bool(arguments.flash_encryption)
        manifest = (
            read_json(arguments.manifest)
            if arguments.manifest
            else build_manifest(
                images,
                arguments.project_description,
                offsets,
                partition_size,
                normal_offset,
                arguments.source_root,
            )
        )
        validate(
            images,
            manifest,
            arguments.target,
            offsets,
            partition_size,
            normal_offset,
            secure_boot,
            flash_encryption,
        )
        atomic_stage(arguments.output_dir, images, manifest)
    except (OSError, RecoveryImageError) as error:
        print(f"recovery bundle error: {error}", file=sys.stderr)
        return 1
    print(f"recovery bundle ready: {arguments.output_dir}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
