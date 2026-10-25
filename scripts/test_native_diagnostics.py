"""Local contract tests: synthetic tools only; no native build or remote calls."""
import importlib.util
import json
import hashlib
import os
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('diagnostics', ROOT / 'scripts/native_diagnostics.py')
diag = importlib.util.module_from_spec(spec)
spec.loader.exec_module(diag)


class DiagnosticTests(unittest.TestCase):
    def test_selection_requires_every_requested_filter(self):
        listing = '\n'.join(f'{name} :: regression' for name in diag.FILTERS)
        self.assertEqual(diag.missing_filters(listing), [])
        self.assertEqual(diag.missing_filters(listing.replace('GuiReach :: regression', '')), ['GuiReach'])
        self.assertEqual(diag.missing_filters('0 tests'), list(diag.FILTERS))

    def test_timeout_is_failure_with_retained_log(self):
        with tempfile.TemporaryDirectory() as directory, patch.object(diag, 'LOGS', Path(directory)):
            with patch.object(diag.subprocess, 'run', side_effect=subprocess.TimeoutExpired('fixture', 1)):
                self.assertEqual(diag.run_logged(['fixture'], 'timeout.log', 1), 124)
            self.assertIn('TIMEOUT', (Path(directory) / 'timeout.log').read_text())

    def test_failed_command_keeps_failure(self):
        with tempfile.TemporaryDirectory() as directory, patch.object(diag, 'LOGS', Path(directory)):
            with patch.object(diag.subprocess, 'run', return_value=subprocess.CompletedProcess([], 9)):
                self.assertEqual(diag.run_logged(['fixture'], 'failed.log'), 9)

    def test_wrong_source_sha_refuses_artifact(self):
        with patch.dict(os.environ, {'GITHUB_SHA': 'a' * 40}), patch.object(
                diag.subprocess, 'check_output', return_value='b' * 40 + '\n'):
            with self.assertRaisesRegex(RuntimeError, 'immutable'):
                diag.bundle()

    def test_diagnostic_validator_requires_both_and_never_calls_au(self):
        if os.name == 'nt':
            self.skipTest('Shell fixture executes on Unix; Windows uses existing PowerShell validator')
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            tools = root / 'tools'
            tools.mkdir()
            uname = tools / 'uname'
            uname.write_text('#!/bin/sh\necho Darwin\n')
            uname.chmod(0o755)
            pluginval = tools / 'pluginval.app/Contents/MacOS/pluginval'
            pluginval.parent.mkdir(parents=True)
            clapval = tools / 'clap-validator'
            for tool in [pluginval, clapval]:
                tool.write_text('#!/bin/sh\nprintf "%s\\n" "$0" "$@" >> "$DIAG_CALLS"\n')
                tool.chmod(0o755)
            artifacts = root / 'build/Luthier_artefacts/Release'
            vst = artifacts / 'VST3/Luthier Pro.vst3'
            clap = artifacts / 'CLAP/Luthier Pro.clap'
            vst.mkdir(parents=True)
            clap.mkdir(parents=True)
            calls = root / 'calls'
            env = dict(os.environ, PATH=str(tools) + os.pathsep + os.environ['PATH'],
                       BUILD_DIR=str(root / 'build'), TOOLS_DIR=str(tools), JOBS='4',
                       DIAG_CALLS=str(calls), LUTHIER_EDITION='PAID', PLUGINVAL_STRICTNESS='5')
            cmd = ['bash', str(ROOT / 'scripts/ci_build.sh'), 'validate-diagnostic']
            result = subprocess.run(cmd, env=env, capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            recorded = calls.read_text()
            self.assertIn('--strictness-level\n5', recorded)
            self.assertIn('Luthier Pro.vst3', recorded)
            self.assertIn('clap-validator', recorded)
            self.assertNotIn('.component', recorded)
            before = recorded
            clap.rmdir()
            self.assertNotEqual(subprocess.run(cmd, env=env, capture_output=True).returncode, 0)
            self.assertEqual(calls.read_text(), before)
            clap.mkdir()
            env['SKIP_VALIDATORS'] = '1'
            self.assertNotEqual(subprocess.run(cmd, env=env, capture_output=True).returncode, 0)
            # Normal validation must retain AU/auval. Intercept every installer
            # command so this synthetic check never touches user plugin folders.
            env.pop('SKIP_VALIDATORS')
            for name in ['mkdir', 'rm', 'cp', 'killall', 'auval']:
                tool = tools / name
                tool.write_text('#!/bin/sh\nprintf "%s\\n" "$0" "$@" >> "$DIAG_CALLS"\n')
                tool.chmod(0o755)
            calls.write_text('')
            normal = subprocess.run(cmd[:-1] + ['validate'], env=env, capture_output=True, text=True)
            self.assertEqual(normal.returncode, 0, normal.stdout + normal.stderr)
            self.assertIn('Luthier Pro.component', calls.read_text())
            self.assertIn('auval', calls.read_text())
            self.assertIn('clap-validator', calls.read_text())

    def test_bundle_preserves_failure_and_complete_checksums(self):
        with tempfile.TemporaryDirectory() as directory:
            old = Path.cwd()
            try:
                os.chdir(directory)
                for name in ['dist/windows/Luthier Pro.clap', 'LICENSE', 'THIRD_PARTY_LICENCES.txt',
                             'Resources/THIRD_PARTY_LICENCES.txt', 'ThirdParty/JUCE/LICENSE.md',
                             'ThirdParty/clap-juce-extensions/LICENSE.md',
                             'build-ci/LuthierTests_artefacts/Release/LuthierTests.exe',
                             'build-ci/logs/focused-tests.log']:
                    path = Path(name)
                    path.parent.mkdir(parents=True, exist_ok=True)
                    path.write_text('synthetic fixture')
                def archive(args, **kwargs):
                    target = next(arg.split('=', 1)[1] for arg in args if arg.startswith('--output='))
                    Path(target).write_bytes(b'synthetic source archive')
                with patch.dict(os.environ, {'GITHUB_SHA': 'a' * 40, 'DIAGNOSTIC_FOCUS': 'failure'}), \
                     patch.object(diag.platform, 'system', return_value='Windows'), \
                     patch.object(diag.subprocess, 'check_output', return_value='a' * 40 + '\n'), \
                     patch.object(diag.subprocess, 'run', side_effect=archive):
                    self.assertEqual(diag.bundle(), 0)
                out = Path('diagnostic-artifacts')
                manifest = json.loads((out / 'UNQUALIFIED-DIAGNOSTIC.json').read_text())
                self.assertEqual(manifest['outcomes']['focus'], 'failure')
                self.assertEqual(manifest['qualification'], 'UNQUALIFIED-DIAGNOSTIC')
                checksums = (out / 'SHA256SUMS').read_text().splitlines()
                for line in checksums:
                    digest, name = line.split('  ', 1)
                    self.assertEqual(hashlib.sha256((out / name).read_bytes()).hexdigest(), digest)
                self.assertEqual(len(checksums), len([p for p in out.rglob('*') if p.is_file()]) - 1)
            finally:
                os.chdir(old)

    def test_workflow_is_scoped_and_failure_artifacts_are_not_success_claim(self):
        text = (ROOT / '.github/workflows/native-diagnostics.yml').read_text()
        self.assertIn('branches: [codex/luthier-native-diagnostics]', text)
        self.assertNotIn('needs: windows', text)
        self.assertEqual(text.count('ref: ${{ github.sha }}'), 2)
        self.assertNotIn('continue-on-error', text)
        self.assertNotIn('gh release', text)
        self.assertNotIn('contents: write', text)
        self.assertNotIn('--skip', text)
        self.assertEqual(text.count('timeout-minutes: 105'), 2)
        self.assertEqual(text.count("steps.stage.outcome == 'success'"), 2)
        self.assertIn('UNQUALIFIED-DIAGNOSTIC', text)
        script = (ROOT / 'scripts/ci_build.sh').read_text()
        self.assertIn('validate)  do_validate ;;', script)
        self.assertIn('[ "$diagnostic" != 1 ] && plugins+=("$ARTEFACTS/AU/', script)


if __name__ == '__main__':
    unittest.main()
