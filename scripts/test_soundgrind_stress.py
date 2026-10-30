import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest


SCRIPT = Path(__file__).with_name("soundgrind_stress.py")


class SoundGrindTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        self.runner = self.root / "runner"
        self.log = self.root / "failures.jsonl"

    def make_runner(self, output, exit_code=0):
        self.runner.write_text("#!/usr/bin/env python3\nimport sys\nif '--list' in sys.argv:\n print('1 tests')\nelse:\n print(" + repr(output) + ")\n sys.exit(" + str(exit_code) + ")\n")
        self.runner.chmod(0o755)

    def campaign(self):
        return subprocess.run([sys.executable, str(SCRIPT), "--runner", str(self.runner),
                               "--rounds", "1", "--suite", "Typo", "--log", str(self.log)],
                              capture_output=True, text=True)

    def test_zero_selected_tests_is_failure(self):
        self.make_runner(" ALL PASSED\n 0 tests, 0 checks, 0.0 s")
        result = self.campaign()
        self.assertNotEqual(result.returncode, 0)
        record = json.loads(self.log.read_text())
        self.assertIn("zero tests", record["output"].lower())

    def test_missing_summary_is_failure(self):
        self.make_runner("runner exited successfully without a test report")
        result = self.campaign()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("summary", json.loads(self.log.read_text())["output"].lower())

    def test_successful_nonempty_campaign(self):
        self.make_runner(" ALL PASSED\n 2 tests, 4 checks, 0.0 s")
        result = self.campaign()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(self.log.read_text(), "")

    def test_child_failure_is_retained(self):
        self.make_runner("assertion failed", 3)
        result = self.campaign()
        self.assertNotEqual(result.returncode, 0)
        record = json.loads(self.log.read_text())
        self.assertEqual(record["returncode"], 3)
        self.assertIn("assertion failed", record["output"])

    def test_child_timeout_is_retained(self):
        self.runner.write_text("#!/usr/bin/env python3\nimport sys,time\nif '--list' in sys.argv: print('1 tests')\nelse: time.sleep(2)\n")
        self.runner.chmod(0o755)
        result = subprocess.run([sys.executable, str(SCRIPT), "--runner", str(self.runner),
                                 "--rounds", "1", "--timeout", "0.2", "--suite", "Typo",
                                 "--log", str(self.log)], capture_output=True, text=True)
        self.assertNotEqual(result.returncode, 0)
        record = json.loads(self.log.read_text())
        self.assertEqual(record["returncode"], 124)
        self.assertIn("TIMEOUT", record["output"])

    def test_existing_log_is_not_overwritten_or_runner_executed(self):
        self.make_runner(" ALL PASSED\n 1 tests, 1 checks, 0.0 s")
        self.log.write_text("prior failure evidence\n")
        self.runner.unlink()  # A second runner writes a marker if it is executed.
        marker = self.root / "executed"
        self.runner.write_text("#!/usr/bin/env python3\nfrom pathlib import Path\nPath(" + repr(str(marker)) + ").touch()\n")
        self.runner.chmod(0o755)
        result = self.campaign()
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(self.log.read_text(), "prior failure evidence\n")
        self.assertFalse(marker.exists())

    def test_unselected_invalid_suite_is_rejected_before_campaign(self):
        self.runner.write_text("""#!/usr/bin/env python3
import sys
if '--list' in sys.argv:
    print('0 tests' if 'Typo' in sys.argv else '1 tests')
else:
    print(' ALL PASSED\\n 1 tests, 1 checks, 0.0 s')
""")
        self.runner.chmod(0o755)
        result = subprocess.run([sys.executable, str(SCRIPT), "--runner", str(self.runner),
                                 "--rounds", "1", "--seed", "1", "--suite", "Good",
                                 "--suite", "Typo", "--log", str(self.log)],
                                capture_output=True, text=True)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("Typo", self.log.read_text())


if __name__ == "__main__":
    unittest.main()
