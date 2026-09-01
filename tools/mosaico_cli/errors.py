"""Stable public error categories used by the CLI."""

from __future__ import annotations


class MosaicoError(RuntimeError):
    exit_code = 5
    category = "operation_failed"

    def __init__(self, message: str, *, details: dict | None = None) -> None:
        super().__init__(message)
        self.details = details or {}


class SelectionError(MosaicoError):
    exit_code = 2
    category = "selection_error"


class EnvironmentError(MosaicoError):
    exit_code = 3
    category = "environment_error"


class BuildError(EnvironmentError):
    category = "build_failed"


class DeviceError(MosaicoError):
    exit_code = 4
    category = "device_unavailable"


class RecoveryRequiredError(DeviceError):
    category = "recovery_required"


class OperationError(MosaicoError):
    exit_code = 5
    category = "operation_failed"


class OutcomeUnknownError(OperationError):
    category = "outcome_unknown"
