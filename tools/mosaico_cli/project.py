"""Select public projects and discover their build artifacts."""

from __future__ import annotations

from dataclasses import dataclass
import json
from pathlib import Path

from .errors import BuildError, SelectionError


@dataclass(frozen=True)
class BuildArtifacts:
    project: Path
    build_dir: Path
    description: Path
    image: Path
    elf: Path
    map_file: Path
    project_name: str
    project_version: str
    target: str


def _is_idf_project(path: Path) -> bool:
    cmake = path / "CMakeLists.txt"
    if not cmake.is_file():
        return False
    try:
        return "project(" in cmake.read_text(encoding="utf-8", errors="replace")
    except OSError:
        return False


def resolve_project(repository: Path, requested: str | None, cwd: Path) -> Path:
    repository = repository.resolve()
    recovery_project = (repository / "projects" / "factory").resolve()
    if requested:
        path = Path(requested).expanduser()
        if not path.is_absolute():
            path = (cwd / path).resolve()
        else:
            path = path.resolve()
        if not _is_idf_project(path):
            raise SelectionError(f"Not a valid ESP-IDF project: {path}")
        if path == recovery_project:
            raise SelectionError(
                "The factory project contains Recovery firmware only and cannot "
                "be installed as an application."
            )
        return path

    current = cwd.resolve()
    while current == repository or repository in current.parents:
        if current != repository and current == recovery_project:
            raise SelectionError(
                "The factory project contains Recovery firmware only; select an "
                "application project with --project PATH."
            )
        if current != repository and _is_idf_project(current):
            return current
        if current == repository:
            break
        current = current.parent

    projects_root = repository / "projects"
    candidates = sorted(
        path for path in projects_root.iterdir()
        if path.is_dir() and path.name != "factory" and _is_idf_project(path)
    ) if projects_root.is_dir() else []
    if len(candidates) == 1:
        return candidates[0]
    if not candidates:
        raise SelectionError(
            "No application project was found. Specify one with --project PATH; "
            "the Recovery-only factory project is never selected automatically."
        )
    raise SelectionError(
        "Multiple application projects were found; specify one with --project PATH.",
        details={"candidates": [str(item) for item in candidates]},
    )


def discover_artifacts(project: Path) -> BuildArtifacts:
    build_dir = project / "build"
    description_path = build_dir / "project_description.json"
    try:
        description = json.loads(description_path.read_text(encoding="utf-8"))
    except FileNotFoundError as error:
        raise BuildError(f"No reusable build was found: {description_path}") from error
    except (OSError, json.JSONDecodeError) as error:
        raise BuildError(f"Invalid build description: {description_path}") from error

    name = str(description.get("project_name") or project.name)
    image = build_dir / str(description.get("app_bin") or f"{name}.bin")
    elf = build_dir / str(description.get("app_elf") or f"{name}.elf")
    map_file = build_dir / f"{name}.map"
    missing = [str(path) for path in (image, elf, map_file) if not path.is_file()]
    if missing:
        raise BuildError(
            "Build artifacts are incomplete.",
            details={"missing": missing, "build_dir": str(build_dir)},
        )
    return BuildArtifacts(
        project=project,
        build_dir=build_dir,
        description=description_path,
        image=image,
        elf=elf,
        map_file=map_file,
        project_name=name,
        project_version=str(description.get("project_version") or ""),
        target=str(description.get("target") or ""),
    )
