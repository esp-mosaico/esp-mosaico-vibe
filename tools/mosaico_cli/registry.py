"""Load the declarative ESP-Mosaico model registry."""

from __future__ import annotations

from dataclasses import dataclass
import json
from pathlib import Path

from .errors import EnvironmentError, SelectionError


@dataclass(frozen=True)
class DeviceModel:
    id: str
    name: str
    target: str
    status: str
    default: bool
    preview_target: bool
    reference_project: str
    bsp_path: str
    recovery_dir: str
    recovery_usb_ids: list[dict[str, str]]


def load_registry() -> list[DeviceModel]:
    path = Path(__file__).with_name("devices.json")
    try:
        value = json.loads(path.read_text(encoding="utf-8"))
        if value.get("schema_version") != 1:
            raise ValueError("unsupported schema")
        return [DeviceModel(**item) for item in value["devices"]]
    except (OSError, KeyError, TypeError, ValueError, json.JSONDecodeError) as error:
        raise EnvironmentError(f"Invalid device registry: {path}") from error


def select_model(model_id: str | None) -> DeviceModel:
    models = [item for item in load_registry() if item.status == "supported"]
    if model_id:
        for item in models:
            if item.id == model_id:
                return item
        raise SelectionError(
            f"Unknown or unsupported device model: {model_id}",
            details={"candidates": [item.id for item in models]},
        )
    defaults = [item for item in models if item.default]
    if len(models) == 1:
        return models[0]
    if len(defaults) == 1:
        return defaults[0]
    raise SelectionError(
        "Multiple supported device models are available; specify one with --model.",
        details={"candidates": [item.id for item in models]},
    )
