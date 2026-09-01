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
    if requested:
        path = Path(requested).expanduser()
        if not path.is_absolute():
            path = (cwd / path).resolve()
        else:
            path = path.resolve()
        if not _is_idf_project(path):
            raise SelectionError(f"不是有效的 ESP-IDF 工程：{path}")
        return path

    current = cwd.resolve()
    while current == repository or repository in current.parents:
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
            "未找到用户工程；请使用 --project PATH 指定工程（factory 不会被自动选择）"
        )
    raise SelectionError(
        "存在多个用户工程，请使用 --project PATH 指定",
        details={"candidates": [str(item) for item in candidates]},
    )


def discover_artifacts(project: Path) -> BuildArtifacts:
    build_dir = project / "build"
    description_path = build_dir / "project_description.json"
    try:
        description = json.loads(description_path.read_text(encoding="utf-8"))
    except FileNotFoundError as error:
        raise BuildError(f"没有可复用的构建：{description_path}") from error
    except (OSError, json.JSONDecodeError) as error:
        raise BuildError(f"构建描述无效：{description_path}") from error

    name = str(description.get("project_name") or project.name)
    image = build_dir / str(description.get("app_bin") or f"{name}.bin")
    elf = build_dir / str(description.get("app_elf") or f"{name}.elf")
    map_file = build_dir / f"{name}.map"
    missing = [str(path) for path in (image, elf, map_file) if not path.is_file()]
    if missing:
        raise BuildError(
            "构建产物不完整",
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
