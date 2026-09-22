"""Consumer checks: empty workspace, real public commands and portable projects."""
from __future__ import annotations

import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

REPOSITORY = Path(__file__).resolve().parents[2]
UTILS_ROOT = REPOSITORY / "submodule/esp-mosaico-utils"
TOOLS_ROOT = UTILS_ROOT / "mosaico-tools"
sys.path.insert(0, str(TOOLS_ROOT / "tools"))
from mosaico_cli.errors import SelectionError
from mosaico_cli.project import resolve_project
from mosaico_cli.workspace import load_workspace


class ToolSubmoduleIntegrationTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="mosaico workspace ")
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name) / "initial workspace"
        self.root.mkdir()
        shutil.copyfile(REPOSITORY / "mosaico.py", self.root / "mosaico.py")
        shutil.copyfile(REPOSITORY / ".mosaico.json", self.root / ".mosaico.json")
        # Only utils is present: application creation must not need the board,
        # engine, ESP-IDF, hardware, or a previous generated application.
        self.tools = self.root / "submodule/esp-mosaico-utils/mosaico-tools"
        shutil.copytree(TOOLS_ROOT, self.tools, ignore=shutil.ignore_patterns(
            "__pycache__", "build", "build-*", "managed_components", ".cache", ".codex-runs"))
        recovery = self.root / "submodule/esp-mosaico-utils/esp-mosaico-recovery"
        recovery.mkdir()
        shutil.copyfile(UTILS_ROOT / "esp-mosaico-recovery/product_contract.json", recovery / "product_contract.json")
        self.config = json.loads((self.root / ".mosaico.json").read_text())

    def cli(self, *args, cwd=None, success=True):
        env = os.environ.copy()
        for name in ("IDF_PATH", "MOSAICO_WORKSPACE", "ESP_GSP_COMPONENT_DIR"):
            env.pop(name, None)
        result = subprocess.run([sys.executable, str(self.root / "mosaico.py"), *args],
                                cwd=cwd or self.root, env=env, capture_output=True, text=True)
        if success:
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        else:
            self.assertNotEqual(result.returncode, 0, result.stdout)
        return result

    def workspace(self):
        return load_workspace(self.tools, explicit=str(self.root))

    def create(self, name="my_app", **kwargs):
        return json.loads(self.cli("project", "init", name, "--json", **kwargs).stdout)

    def test_launcher_and_command_tree(self):
        self.assertRegex(self.cli("--version").stdout, r"mosaico.py \d+\.\d+\.\d+")
        self.assertIn("game", self.cli("--help").stdout)
        self.assertIn("sim", self.cli("project", "--help").stdout)
        self.assertIn("mosaico.py iris rpc", self.cli("iris", "rpc", "1", "2", "--payload", "game", "--help").stdout)

    def test_empty_workspace_dry_run_does_not_write(self):
        before = sorted(str(p.relative_to(self.root)) for p in self.root.rglob("*") if '__pycache__' not in p.parts)
        payload = json.loads(self.cli("project", "init", "my_app", "--dry-run", "--json").stdout)
        after = sorted(str(p.relative_to(self.root)) for p in self.root.rglob("*") if '__pycache__' not in p.parts and p.suffix != '.pyc')
        self.assertEqual(payload["status"], "dry_run")
        self.assertEqual(before, after)
        self.assertFalse((self.root / "projects").exists())
        with self.assertRaisesRegex(SelectionError, "project init"):
            resolve_project(self.workspace(), None, self.root)

    def test_nested_creation_preserves_configuration_and_installs_resources(self):
        nested = self.root / "nested directory"
        nested.mkdir()
        payload = self.create(cwd=nested)
        project = Path(payload["project"])
        self.assertEqual(project, self.root / "projects/my_app")
        for filename in ("main/main.c", "main/hello_ui.c", "ui/main.json", "ui/fonts/LICENSE", "pc/CMakeLists.txt", "partitions.csv"):
            self.assertTrue((project / filename).is_file(), filename)
        self.assertIn("iris system-update", payload["install_command"])
        self.assertEqual(json.loads((self.root / ".mosaico.json").read_text()), self.config)
        self.assertFalse((project / "build").exists())
        self.assertFalse(self.workspace().run_dir.exists())
        self.assertEqual(resolve_project(self.workspace(), None, project / "main"), project)
        self.assertEqual(resolve_project(self.workspace(), None, self.root), project)

    def test_second_creation_is_rejected_without_overwriting(self):
        project = Path(self.create()["project"])
        marker = project / "main/main.c"
        marker.write_text("user content\n")
        self.cli("project", "init", "my_app", success=False)
        self.assertEqual(marker.read_text(), "user content\n")

    def test_multiple_projects_require_selection(self):
        first = Path(self.create("one")["project"])
        second = Path(self.create("two")["project"])
        with self.assertRaisesRegex(SelectionError, "Multiple"):
            resolve_project(self.workspace(), None, self.root)
        self.assertEqual(resolve_project(self.workspace(), "projects/one", self.root), first)
        self.config["workspace"]["default_project"] = "projects/two"
        (self.root / ".mosaico.json").write_text(json.dumps(self.config))
        self.assertEqual(resolve_project(self.workspace(), None, self.root), second)
        self.assertEqual(resolve_project(self.workspace(), None, first / "main"), first)

    def test_internal_recovery_is_never_an_application(self):
        rec = self.workspace().recovery_project
        rec.mkdir(parents=True)
        (rec / "CMakeLists.txt").write_text("project(recovery)\n")
        with self.assertRaises(SelectionError):
            resolve_project(self.workspace(), str(rec), self.root)
        with self.assertRaises(SelectionError):
            resolve_project(self.workspace(), None, rec)

    def test_workspace_move_preserves_generated_relative_references(self):
        self.create()
        old = str(self.root)
        moved = Path(self.temporary.name) / "relocated workspace"
        self.root.rename(moved)
        self.root = moved
        self.tools = moved / "submodule/esp-mosaico-utils/mosaico-tools"
        self.assertEqual(self.cli("--version").returncode, 0)
        self.create("after_move")
        for name in ("my_app", "after_move"):
            project = moved / "projects" / name
            for filename in ("CMakeLists.txt", "main/idf_component.yml", "README.md"):
                text = (project / filename).read_text()
                self.assertNotIn(old, text)
                self.assertNotIn(str(REPOSITORY), text)
            import re
            cmake = (project / "CMakeLists.txt").read_text()
            reference = re.search(r'set\(MOSAICO_UTILS_ROOT "\$\{CMAKE_CURRENT_LIST_DIR\}/([^"\n]+)"\)', cmake).group(1)
            self.assertEqual((project / reference).resolve(), self.tools.parent)

    def test_custom_template_is_still_supported(self):
        template = self.root / "custom"
        template.mkdir()
        (template / "message.txt").write_text("hello\n")
        (template / "template.json").write_text(json.dumps({"schema_version":1,"files":[{"source":"message.txt"}]}))
        self.config["workspace"]["init_template"] = "custom/template.json"
        (self.root / ".mosaico.json").write_text(json.dumps(self.config))
        project = Path(self.create("custom_app")["project"])
        self.assertEqual((project / "message.txt").read_text(), "hello\n")


if __name__ == "__main__":
    unittest.main()
