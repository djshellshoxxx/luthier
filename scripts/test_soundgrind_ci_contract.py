"""Static safety contract for the tiny, cross-platform SoundGrind test job.

The native audio/UI tests and acceptance thresholds belong to build.yml;
this test only ensures the runner's Python regressions cannot be silently
omitted when the runner is changed.
"""
from pathlib import Path
import unittest

WORKFLOW = Path(__file__).resolve().parents[1] / '.github/workflows/soundgrind-runner-tests.yml'


class SoundGrindCIContractTests(unittest.TestCase):
    def setUp(self):
        self.assertTrue(WORKFLOW.is_file(), 'SoundGrind Python regressions are not wired into CI')
        self.text = WORKFLOW.read_text(encoding='utf-8')

    def test_three_platform_matrix(self):
        self.assertIn('matrix:', self.text)
        self.assertIn('os: [ubuntu-latest, windows-latest, macos-latest]', self.text)
        self.assertIn('runs-on: ${{ matrix.os }}', self.text)

    def test_regressions_run_and_cannot_be_ignored(self):
        self.assertIn('python -m unittest discover -s scripts -p test_soundgrind_stress.py -v', self.text)
        self.assertIn('python -m unittest discover -s scripts -p test_soundgrind_ci_contract.py -v', self.text)
        self.assertNotIn('continue-on-error:', self.text)

    def test_no_publishing_and_scope_is_runner_only(self):
        self.assertIn('contents: read', self.text)
        self.assertNotIn('contents: write', self.text)
        self.assertNotIn('gh release', self.text)
        self.assertIn('pull_request:', self.text)
        self.assertIn('scripts/soundgrind_stress.py', self.text)
        self.assertIn('scripts/test_soundgrind_stress.py', self.text)
        self.assertIn('workflow_dispatch:', self.text)


if __name__ == '__main__':
    unittest.main()
