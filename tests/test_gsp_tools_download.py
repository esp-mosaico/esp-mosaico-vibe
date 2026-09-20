"""Release archives must resolve to executable filenames on each host."""
import importlib.util
import zipfile
from pathlib import Path
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("gsp_tools_download", ROOT / "tools/gsp-sim/fetch_gspc.py")
fetch = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(fetch)


def test_windows_release_extracts_and_reuses_exe(tmp_path):
    archive = tmp_path / "gspc-0.3.0-windows-x86_64.zip"
    with zipfile.ZipFile(archive, "w") as bundle:
        bundle.writestr("release/gspc.exe", b"windows executable")
    with patch.object(fetch, "host_os_arch", return_value=("windows", "x86_64")), \
            patch.object(fetch, "download") as download, \
            patch.dict(fetch.os.environ, {"GSPC_EXECUTABLE": ""}):
        executable = fetch.resolve_release(product="gspc", version="0.3.0", env_var="GSPC_EXECUTABLE", cache_dir=tmp_path)
        assert executable == tmp_path / "gspc.exe"
        assert executable.read_bytes() == b"windows executable"
        download.reset_mock()
        assert fetch.resolve_release(product="gspc", version="0.3.0", env_var="GSPC_EXECUTABLE", cache_dir=tmp_path) == executable
        download.assert_not_called()


def test_upgrade_bootstrap_ignores_old_managed_component():
    # Reconfigure resolves GSPC before the component manager installs the new
    # runtime. The upgrade command must not select that old runtime's compiler.
    for simulator, version in ((False, fetch.PINNED_GSPC_VERSION),
                               (True, fetch.PINNED_GSP_VERSION)):
        argv = ['fetch_gspc.py', '--pinned'] + (['--sim'] if simulator else [])
        with patch.object(fetch.sys, 'argv', argv), \
                patch.object(fetch, 'resolve_release', return_value=Path('/tmp/tool')) as release, \
                patch.object(fetch, 'resolve_gsp_root', side_effect=AssertionError('stale component consulted')):
            assert fetch.main() == 0
        assert release.call_args.kwargs['version'] == version
        assert release.call_args.kwargs['product'] == ('sim' if simulator else 'gspc')
