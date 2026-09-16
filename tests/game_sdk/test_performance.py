import importlib.util
from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location(
    "analyze_game_perf", ROOT / "submodule/raylib-lite-engine/tools/analyze_game_perf.py")
PERF = importlib.util.module_from_spec(SPEC)
assert SPEC.loader
SPEC.loader.exec_module(PERF)
MATRIX_SPEC = importlib.util.spec_from_file_location(
    "game_benchmark_matrix", ROOT / "submodule/raylib-lite-engine/tools/game_benchmark_matrix.py")
MATRIX = importlib.util.module_from_spec(MATRIX_SPEC)
assert MATRIX_SPEC.loader
MATRIX_SPEC.loader.exec_module(MATRIX)


class PerformanceReportTests(unittest.TestCase):
    def test_standard_matrix_is_complete_and_keeps_safe_clock(self):
        cases = MATRIX.cases()
        self.assertEqual(len(cases), 12)
        self.assertEqual(len({case["label"] for case in cases}), 12)
        self.assertTrue(all(case["qspi_hz"] == 40_000_000 for case in cases))

    def test_summarizes_versioned_debug_samples(self):
        line = ("logic=30.1 display=24.2 frame=90 dropped=10 busy=10 "
                "superseded=0 errors=0 input=12us update=30us acquire=4us "
                "render=8000us submit=7us release=23400us inflight=2/3 "
                "heap=210000 psram=6000000")
        report = PERF.summarize("\n".join((line, line.replace("30.1", "29.9"))))
        self.assertEqual(report["samples"], 2)
        self.assertEqual(report["logic"]["mean"], 30.0)
        self.assertEqual(report["acquire"]["p95"], 4.0)
        self.assertEqual(report["busy"], 10)
        self.assertEqual(report["peak_inflight"], 3)
        self.assertEqual(report["min_heap_bytes"], 210000)
        self.assertTrue(report["accepted"])

    def test_empty_log_is_explicit(self):
        report = PERF.summarize("unrelated retained boot log")
        self.assertEqual(report["samples"], 0)
        self.assertEqual(report["display"]["p50"], 0.0)
        self.assertFalse(report["accepted"])


if __name__ == "__main__":
    unittest.main()
