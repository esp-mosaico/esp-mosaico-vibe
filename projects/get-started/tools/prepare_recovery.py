#!/usr/bin/env python3
"""Validate and stage a retained factory recovery image."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys
from typing import Any


SCHEMA_VERSION = 1
PROJECT_NAME = "iris_get_started"
PARTITION_NAME = "factory"
ESP_IMAGE_MAGIC = 0xE9
RECOVERY_SOURCE_PATHS = (
    "CMakeLists.txt",
    "main",
    "partitions.csv",
    "sdkconfig.defaults",
    "sdkconfig.recovery.defaults",
)


class RecoveryImageError(RuntimeError):
    """Raised when a recovery artifact is missing or incompatible."""


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
            [
                "git",
                "-C",
                str(source_root),
                "status",
                "--porcelain",
                "--",
                *RECOVERY_SOURCE_PATHS,
            ],
            check=True,
            capture_output=True,
            text=True,
        ).stdout.strip()
    except (FileNotFoundError, subprocess.CalledProcessError):
        return "unknown", True
    return commit or "unknown", bool(changes)


def build_manifest(
    image: Path,
    description_path: Path,
    partition_offset: int,
    partition_size: int,
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
        raise RecoveryImageError(
            f"{image} was not produced by BUILD_PROFILE=recovery"
        )

    source_commit, source_dirty = source_state(source_root)

    return {
        "schema_version": SCHEMA_VERSION,
        "project": description.get("project_name", ""),
        "profile": "recovery",
        "version": description.get("project_version", ""),
        "target": description.get("target", ""),
        "idf_version": description.get("git_revision", ""),
        "source": {
            "commit": source_commit,
            "dirty": source_dirty,
        },
        "partition": {
            "name": PARTITION_NAME,
            "offset": f"0x{partition_offset:x}",
            "size": f"0x{partition_size:x}",
        },
        "security": {
            "secure_boot": enabled_config(config, "CONFIG_SECURE_BOOT"),
            "flash_encryption": enabled_config(
                config, "CONFIG_SECURE_FLASH_ENC_ENABLED"
            ),
        },
        "image": {
            "file": image.name,
            "size": image.stat().st_size,
            "sha256": sha256(image),
        },
    }


def require_mapping(manifest: dict[str, Any], key: str) -> dict[str, Any]:
    value = manifest.get(key)
    if not isinstance(value, dict):
        raise RecoveryImageError(f"manifest field {key!r} must be an object")
    return value


def validate(
    image: Path,
    manifest: dict[str, Any],
    target: str,
    partition_offset: int,
    partition_size: int,
    secure_boot: bool,
    flash_encryption: bool,
) -> None:
    if not image.is_file():
        raise RecoveryImageError(f"recovery image does not exist: {image}")

    image_size = image.stat().st_size
    if image_size == 0:
        raise RecoveryImageError(f"recovery image is empty: {image}")
    if image_size > partition_size:
        raise RecoveryImageError(
            f"recovery image is {image_size} bytes, larger than the "
            f"factory partition ({partition_size} bytes)"
        )
    with image.open("rb") as stream:
        if stream.read(1) != bytes([ESP_IMAGE_MAGIC]):
            raise RecoveryImageError(f"invalid ESP application image magic: {image}")

    if manifest.get("schema_version") != SCHEMA_VERSION:
        raise RecoveryImageError(
            f"unsupported recovery manifest schema: {manifest.get('schema_version')!r}"
        )
    if manifest.get("project") != PROJECT_NAME:
        raise RecoveryImageError(
            f"recovery project mismatch: expected {PROJECT_NAME!r}, "
            f"got {manifest.get('project')!r}"
        )
    if manifest.get("profile") != "recovery":
        raise RecoveryImageError("manifest does not describe a recovery build")
    if manifest.get("target") != target:
        raise RecoveryImageError(
            f"recovery target mismatch: expected {target!r}, "
            f"got {manifest.get('target')!r}"
        )

    partition = require_mapping(manifest, "partition")
    if partition.get("name") != PARTITION_NAME:
        raise RecoveryImageError(
            f"recovery partition mismatch: expected {PARTITION_NAME!r}"
        )
    if parse_int(partition.get("offset", "")) != partition_offset:
        raise RecoveryImageError(
            f"factory partition offset changed; rebuild recovery for "
            f"0x{partition_offset:x}"
        )
    if parse_int(partition.get("size", "")) != partition_size:
        raise RecoveryImageError(
            f"factory partition size changed; rebuild recovery for "
            f"0x{partition_size:x} bytes"
        )

    security = require_mapping(manifest, "security")
    if security.get("secure_boot") is not secure_boot:
        raise RecoveryImageError(
            "recovery secure-boot configuration differs from the current build"
        )
    if security.get("flash_encryption") is not flash_encryption:
        raise RecoveryImageError(
            "recovery flash-encryption configuration differs from the current build"
        )

    image_info = require_mapping(manifest, "image")
    if image_info.get("size") != image_size:
        raise RecoveryImageError(
            f"recovery image size mismatch: manifest={image_info.get('size')!r}, "
            f"actual={image_size}"
        )
    actual_hash = sha256(image)
    if image_info.get("sha256") != actual_hash:
        raise RecoveryImageError(
            f"recovery image SHA-256 mismatch: expected "
            f"{image_info.get('sha256')!r}, got {actual_hash}"
        )


def atomic_copy(source: Path, destination: Path) -> None:
    destination.parent.mkdir(parents=True, exist_ok=True)
    temporary = destination.with_name(destination.name + ".tmp")
    shutil.copyfile(source, temporary)
    temporary.replace(destination)


def atomic_write_json(destination: Path, value: dict[str, Any]) -> None:
    destination.parent.mkdir(parents=True, exist_ok=True)
    temporary = destination.with_name(destination.name + ".tmp")
    temporary.write_text(
        json.dumps(value, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )
    temporary.replace(destination)


def parser() -> argparse.ArgumentParser:
    result = argparse.ArgumentParser(
        description="Validate and stage a retained factory recovery image"
    )
    result.add_argument("--image", type=Path, required=True)
    metadata = result.add_mutually_exclusive_group(required=True)
    metadata.add_argument("--manifest", type=Path)
    metadata.add_argument("--project-description", type=Path)
    result.add_argument("--source-root", type=Path, default=Path.cwd())
    result.add_argument("--target", required=True)
    result.add_argument("--partition-offset", required=True)
    result.add_argument("--partition-size", required=True)
    result.add_argument("--secure-boot", required=True)
    result.add_argument("--flash-encryption", required=True)
    result.add_argument("--output-image", type=Path, required=True)
    result.add_argument("--output-manifest", type=Path, required=True)
    return result


def main() -> int:
    arguments = parser().parse_args()
    try:
        partition_offset = parse_int(arguments.partition_offset)
        partition_size = parse_int(arguments.partition_size)
        secure_boot = parse_bool(arguments.secure_boot)
        flash_encryption = parse_bool(arguments.flash_encryption)
        if arguments.manifest:
            manifest = read_json(arguments.manifest)
        else:
            manifest = build_manifest(
                arguments.image,
                arguments.project_description,
                partition_offset,
                partition_size,
                arguments.source_root,
            )

        validate(
            arguments.image,
            manifest,
            arguments.target,
            partition_offset,
            partition_size,
            secure_boot,
            flash_encryption,
        )
        atomic_copy(arguments.image, arguments.output_image)
        atomic_write_json(arguments.output_manifest, manifest)
    except (OSError, RecoveryImageError) as error:
        print(f"recovery image error: {error}", file=sys.stderr)
        return 1

    print(
        f"recovery image ready: {arguments.output_image} "
        f"({arguments.image.stat().st_size} bytes)"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
