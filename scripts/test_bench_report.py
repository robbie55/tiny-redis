"""Tests for bench_report.py, run against the committed 2026-11-06 Docker results.

    python3 -m unittest discover -s scripts -p 'test_*.py'
"""
import csv
import io
import pathlib
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parent.parent
SCRIPT = ROOT / "scripts" / "bench_report.py"
GOLDEN = ROOT / "bench" / "results" / "2026-11-06-1524-docker"


def run(*dirs):
    p = subprocess.run([sys.executable, str(SCRIPT), *map(str, dirs)], capture_output=True, text=True)
    return p.returncode, list(csv.DictReader(io.StringIO(p.stdout))), p.stderr


def row(rows, run_name, command, pipeline):
    [r] = [r for r in rows if (r["run"], r["command"], r["pipeline"]) == (run_name, command, str(pipeline))]
    return r


class Golden(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.code, cls.rows, cls.err = run(GOLDEN)
        cls.name = GOLDEN.name

    def test_exit_ok(self):
        self.assertEqual(self.code, 0, self.err)
        self.assertEqual(len(self.rows), 4)

    def test_medians_match_summary(self):
        with open(GOLDEN / "summary.csv") as f:
            for s in csv.DictReader(f):
                prefix = "tiny" if s["server"] == "tiny-redis" else "redis"
                r = row(self.rows, self.name, s["command"], s["pipeline"])
                for stat in ("median", "min", "max"):
                    self.assertEqual(r[f"{prefix}_rps_{stat}"], s[f"rps_{stat}"], (s["server"], s["command"], s["pipeline"], stat))
                self.assertEqual(float(r[f"{prefix}_p99_ms_median"]), float(s["p99_ms_median"]))

    def test_ratio_of_medians(self):
        for cmd, p, want in [("SET", 16, 1.478), ("GET", 16, 1.406), ("SET", 1, 1.117), ("GET", 1, 1.082)]:
            self.assertEqual(row(self.rows, self.name, cmd, p)["ratio_of_medians"], f"{want:.3f}", (cmd, p))

    def test_paired_ratios(self):
        r = row(self.rows, self.name, "SET", 16)
        self.assertEqual(r["trials"], "5")
        got = [round(float(r[f"paired_ratio_{k}"]), 2) for k in ("median", "min", "max")]
        self.assertEqual(got, [1.48, 1.42, 1.80])
        self.assertEqual(round(float(row(self.rows, self.name, "GET", 1)["paired_ratio_min"]), 2), 0.97)

    def test_short_run_warning(self):
        self.assertIn("tiny-redis-set-P16-t1", self.err)
        self.assertIn("9.77", self.err)


class TwoRuns(unittest.TestCase):
    def test_all_block(self):
        code, rows, err = run(GOLDEN, GOLDEN)
        self.assertEqual(code, 0, err)
        self.assertEqual(row(rows, "ALL", "SET", 16)["ratio_of_medians"], "1.478-1.478")
        self.assertEqual(len([r for r in rows if r["run"] == "ALL"]), 4)


class BadInput(unittest.TestCase):
    def setUp(self):
        self.tmp = pathlib.Path(tempfile.mkdtemp())
        self.addCleanup(shutil.rmtree, self.tmp)
        shutil.copytree(GOLDEN / "raw", self.tmp / "raw")

    def test_missing_file(self):
        (self.tmp / "raw" / "redis-get-P1-t3.csv").unlink()
        self.assertEqual(run(self.tmp)[0], 2)

    def test_extra_trial(self):
        for ext in ("csv", "cpu", "clientcpu"):
            shutil.copy(self.tmp / "raw" / f"tiny-redis-set-P1-t1.{ext}", self.tmp / "raw" / f"tiny-redis-set-P1-t6.{ext}")
        self.assertEqual(run(self.tmp)[0], 2)


if __name__ == "__main__":
    unittest.main()
