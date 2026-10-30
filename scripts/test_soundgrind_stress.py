import contextlib
import io
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

import soundgrind_stress as runner


class SoundGrindTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        self.binary = self.root / "runner"
        self.binary.touch()
        self.log = self.root / "failures.jsonl"

    def campaign(self, outcomes, suites=("Test",)):
        args = ["soundgrind_stress.py", "--runner", str(self.binary), "--rounds", "1",
                "--log", str(self.log)]
        for suite in suites:
            args += ["--suite", suite]
        with patch.object(sys, "argv", args), patch.object(runner, "run", side_effect=outcomes) as run:
            with contextlib.redirect_stderr(io.StringIO()):
                result = runner.main()
        return result, run

    def test_zero_selected_tests_is_failure(self):
        result, _ = self.campaign([(0, "1 tests\n", 0), (0, " ALL PASSED\n 0 tests, 0 checks, 0.0 s\n", 0)])
        self.assertNotEqual(result, 0)
        self.assertIn("zero tests", json.loads(self.log.read_text())["output"].lower())

    def test_missing_summary_is_failure(self):
        result, _ = self.campaign([(0, "1 tests\n", 0), (0, "no summary", 0)])
        self.assertNotEqual(result, 0)
        self.assertIn("summary", json.loads(self.log.read_text())["output"].lower())

    def test_successful_nonempty_campaign(self):
        result, _ = self.campaign([(0, "1 tests\n", 0), (0, " ALL PASSED\n 2 tests, 4 checks, 0.0 s\n", 0)])
        self.assertEqual(result, 0)
        self.assertEqual(self.log.read_text(), "")

    def test_child_failure_is_retained(self):
        result, _ = self.campaign([(0, "1 tests\n", 0), (3, "assertion failed", 0)])
        self.assertNotEqual(result, 0)
        record = json.loads(self.log.read_text())
        self.assertEqual(record["returncode"], 3)
        self.assertIn("assertion failed", record["output"])

    def test_child_timeout_is_retained(self):
        with patch.object(subprocess, "run", side_effect=subprocess.TimeoutExpired(["runner"], 1, output=b"partial")):
            rc, output, _ = runner.run(["runner"], 1, {})
        self.assertEqual(rc, 124)
        self.assertIn("partial\nTIMEOUT", output)
        result, _ = self.campaign([(0, "1 tests\n", 0), (rc, output, 1)])
        self.assertNotEqual(result, 0)
        self.assertEqual(json.loads(self.log.read_text())["returncode"], 124)

    def test_existing_log_is_not_overwritten_or_runner_executed(self):
        self.log.write_text("prior failure evidence\n")
        args = ["soundgrind_stress.py", "--runner", str(self.binary), "--rounds", "1",
                "--log", str(self.log)]
        with patch.object(sys, "argv", args), patch.object(runner, "run") as run:
            with contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit) as raised:
                runner.main()
            run.assert_not_called()
        self.assertNotEqual(raised.exception.code, 0)
        self.assertEqual(self.log.read_text(), "prior failure evidence\n")

    def test_unselected_invalid_suite_is_rejected_before_campaign(self):
        result, run = self.campaign([(0, "1 tests\n", 0), (0, "0 tests\n", 0)], ("Good", "Typo"))
        self.assertNotEqual(result, 0)
        self.assertEqual(run.call_count, 2)
        self.assertEqual(json.loads(self.log.read_text())["suite"], "Typo")


if __name__ == "__main__":
    unittest.main()
