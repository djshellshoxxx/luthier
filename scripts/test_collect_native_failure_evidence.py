"""Synthetic filesystem fixtures; these do not establish macOS runtime success."""
import importlib.util
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import time
import unittest
from unittest import mock
import sys

SCRIPT = Path(__file__).with_name('collect_native_failure_evidence.py')
SPEC = importlib.util.spec_from_file_location('native_evidence', SCRIPT)
collector = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(collector)


def ips(process='LuthierTests', frames=True):
    return json.dumps({'name': process, 'bug_type': '309', 'incident_id': 'PRIVATE_INCIDENT'}) + '\n' + json.dumps({
        'procName': process, 'procPath': '/Users/PRIVATE_USER/secret.app',
        'crashReporterKey': 'PRIVATE_DEVICE', 'asi': {'license': 'PRIVATE_LICENSE'},
        'exception': {'type': 'EXC_CRASH', 'signal': 'SIGABRT', 'message': 'PRIVATE_MESSAGE'},
        'termination': {'namespace': 'SIGNAL', 'code': 6, 'indicator': 'PRIVATE_INDICATOR'},
        'faultingThread': 0, 'threads': [{'triggered': True, 'threadState': {'secret': 'PRIVATE_REGISTER'},
            'frames': [{'imageIndex': 0, 'imageOffset': 64, 'symbol': 'luthier::JamEngine::process()', 'symbolLocation': 8}] if frames else []}],
        'usedImages': [{'name': process, 'base': 4096, 'size': 8192, 'uuid': '12345678-1234-1234-1234-123456789abc', 'path': '/Users/PRIVATE_USER/secret.app'}],
    })


class NativeEvidenceTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name).resolve()
        self.reports = self.root / 'DiagnosticReports'; self.reports.mkdir()
        self.marker = self.root / 'start.json'
        self.output = self.root / 'evidence'
        self.now = time.time_ns(); self.start = self.now - 10_000_000_000
        collector.mark_start(self.marker, 'job1', self.start)

    def report(self, name, text=None, timestamp=None):
        path = self.reports / name; path.write_text(ips() if text is None else text)
        age = self.now - 1_000_000_000 if timestamp is None else timestamp
        os.utime(path, ns=(age, age)); return path

    def collect(self, logs=()):
        return collector.collect(self.marker, self.output, [self.reports], logs, self.now)

    def output_text(self):
        return '\n'.join(p.read_text() for p in self.output.iterdir() if p.is_file())

    def test_only_current_target_reports_are_read_and_staged(self):
        self.report('LuthierTests-current.ips')
        self.report('LuthierRender-current.ips', ips('LuthierRender'))
        self.report('LuthierTests-old.ips', timestamp=self.start-1)
        self.report('LuthierTests-future.ips', timestamp=self.now+1)
        self.report('Unrelated-current.ips')
        self.report('LuthierTestsOther-current.ips')
        self.report('LuthierTests-fake.ips', ips('Unrelated'))
        result = self.collect()
        self.assertEqual(result['reports'], 2); self.assertEqual(result['backtraces'], 2)
        self.assertEqual(len(list(self.output.glob('*-report-*.json'))), 2)
        for path in self.output.glob('*-report-*.json'):
            self.assertTrue(self.start <= json.loads(path.read_text())['sourceMtimeNs'] <= self.now)
        text = self.output_text()
        self.assertIn('luthier::JamEngine::process()', text)
        self.assertIn('imageOffset', text)
        self.assertNotIn('PRIVATE_', text)
        self.assertNotIn('Unrelated', text)

    def test_report_limit_is_explicit_and_malformed_candidates_are_not_called_absent(self):
        self.report('LuthierTests-malformed.ips','PRIVATE_MALFORMED')
        result=self.collect();self.assertEqual(result['candidates'],1);self.assertEqual(result['rejected'],1)
        self.assertIn('Matching current-job files existed',self.output_text());self.assertNotIn('PRIVATE_',self.output_text())
        for i in range(35):self.report('LuthierTests-'+str(i)+'.ips')
        result=self.collect();self.assertEqual(result['reports'],32);self.assertEqual(result['omittedByLimit'],4)

    def test_symlinked_start_marker_is_refused(self):
        self.marker.unlink();outside=self.root/'private-marker';outside.write_text('PRIVATE_MARKER')
        self.marker.symlink_to(outside)
        with self.assertRaises(OSError):collector.mark_start(self.marker,'job2',self.now)
        self.assertEqual(outside.read_text(),'PRIVATE_MARKER')
        self.assertEqual(self.collect()['reports'],0);self.assertNotIn('PRIVATE_',self.output_text())

    def test_unsupported_ips_type_is_rejected(self):
        self.report('LuthierTests-not-crash.ips',ips().replace('"309"','"288"'))
        self.assertEqual(self.collect()['reports'],0)

    def test_file_and_directory_symlinks_are_never_followed(self):
        outside = self.root / 'private.ips'; outside.write_text(ips())
        (self.reports / 'LuthierTests-linked.ips').symlink_to(outside)
        linked = self.root / 'linked-reports'; linked.symlink_to(self.reports, target_is_directory=True)
        retired=self.reports/'Retired';retired.mkdir();(retired/'LuthierTests-private.ips').write_text(ips())
        os.utime(retired/'LuthierTests-private.ips',ns=(self.now-1,self.now-1))
        result = collector.collect(self.marker, self.output, [linked], [], self.now)
        self.assertEqual(result['reports'], 0)
        self.assertIn('No current-job OS crash report', self.output_text())
        self.assertEqual(outside.read_text(), ips())

    def test_symlinked_parent_of_report_or_output_directory_is_refused(self):
        private=self.root/'private';private.mkdir();reports=private/'DiagnosticReports';reports.mkdir()
        path=reports/'LuthierTests-private.ips';path.write_text(ips());os.utime(path,ns=(self.now-1,self.now-1))
        linked=self.root/'linked-parent';linked.symlink_to(private,target_is_directory=True)
        self.assertEqual(collector.collect(self.marker,self.output,[linked/'DiagnosticReports'],[],self.now)['reports'],0)
        with self.assertRaises(OSError):collector.collect(self.marker,linked/'new-output',[self.reports],[],self.now)
        self.assertFalse((private/'new-output').exists())

    def test_retired_directory_symlink_is_not_followed(self):
        outside=self.root/'private-dir';outside.mkdir();path=outside/'LuthierTests-private.ips';path.write_text(ips())
        os.utime(path,ns=(self.now-1,self.now-1));(self.reports/'Retired').symlink_to(outside,target_is_directory=True)
        self.assertEqual(self.collect()['reports'],0)

    def test_repeat_collection_and_new_job_do_not_leave_old_report_artifacts(self):
        report=self.report('LuthierTests-current.ips');self.collect();report.unlink()
        self.collect();self.assertEqual(list(self.output.glob('*-report-*.json')),[])
        self.report('LuthierTests-current.ips');self.collect()
        collector.mark_start(self.marker,'job2',self.now,output=self.output)
        self.assertEqual(list(self.output.iterdir()),[])

    def test_marker_is_per_job_and_reuses_the_first_start(self):
        collector.mark_start(self.marker, 'job1', self.now)
        self.assertEqual(json.loads(self.marker.read_text())['started_ns'], self.start)
        collector.mark_start(self.marker, 'job2', self.now)
        self.assertEqual(json.loads(self.marker.read_text())['started_ns'], self.now)

    def test_missing_marker_refuses_historical_report_search(self):
        self.marker.unlink(); self.report('LuthierTests-current.ips')
        result = self.collect()
        self.assertEqual(result['reports'], 0)
        self.assertIn('job-start marker unavailable', self.output_text())

    def test_report_without_frames_records_exact_backtrace_absence(self):
        self.report('LuthierTests-current.ips', ips(frames=False))
        result = self.collect()
        self.assertEqual(result['reports'], 1); self.assertEqual(result['backtraces'], 0)
        self.assertIn('No backtrace available in selected OS reports', self.output_text())

    def test_classic_crash_report_retains_stack_but_no_private_sections(self):
        self.report('LuthierRender-current.crash', '''Process: LuthierRender [123]
Path: /Users/PRIVATE_USER/LuthierRender
Exception Type: EXC_CRASH (SIGABRT)
Application Specific Information:
PRIVATE_LICENSE
Crashed Thread: 0
Thread 0 Crashed:: Dispatch queue: com.apple.main-thread
0 LuthierRender 0x0000000000001040 luthier::Render::run() + 8
1 libsystem_c.dylib 0x0000000000002040 abort + 12
Thread 0 ARM Thread State:
x0: PRIVATE_REGISTER
''')
        result = self.collect()
        self.assertEqual(result['reports'], 1); self.assertEqual(result['backtraces'], 1)
        self.assertIn('luthier::Render::run()', self.output_text())
        self.assertNotIn('PRIVATE_', self.output_text())

    def test_host_diagnostics_are_reconstructed_and_full_state_is_not_copied(self):
        log = self.root / 'unit-tests.log'
        log.write_text('PRIVATE_LICENSE\n'+
            'HostState exact JSON mismatch: bytes=70/71 firstByte=32 path=$.referencePedals.mix reason=numeric-representation types=number/number tokens=-0.0/0.0\n'+
            'HostState exact JSON mismatch: bytes=70/71 firstByte=32 path=$.preset.parameters.PRIVATE_PARAM reason=numeric-representation types=number/number tokens=1122334455/9988776655 PRIVATE_TRAILER\n'+
            'HostState exact JSON mismatch: bytes=70/71 firstByte=32 path=$[property#0][property#0] reason=numeric-representation types=number/number tokens=1122334455/9988776655\n')
        os.utime(log, ns=(self.now-1,self.now-1))
        pair = self.root / 'luthier-restored-a.json'; pair.write_text('PRIVATE_FULL_STATE')
        self.collect([log]); text=self.output_text()
        self.assertIn('tokens=-0.0/0.0', text)
        self.assertNotIn('PRIVATE_', text); self.assertNotIn('1122334455', text)
        self.assertEqual(pair.read_text(), 'PRIVATE_FULL_STATE')
        self.assertFalse(any('restored' in p.name for p in self.output.iterdir()))

    def test_stale_and_symlinked_logs_are_ignored(self):
        stale = self.root / 'old.log'; stale.write_text('HostState exact JSON mismatch: bytes=1/2 firstByte=1 path=$ reason=length')
        os.utime(stale, ns=(self.start-1,self.start-1))
        linked = self.root / 'link.log'; linked.symlink_to(stale)
        self.collect([stale, linked])
        self.assertIn('No privacy-safe HostState diagnostic line', self.output_text())

    def test_unknown_exception_values_are_redacted_and_nonmac_cli_does_not_collect(self):
        payload=ips().replace('EXC_CRASH','EXC_PRIVATE_LICENSE').replace('SIGABRT','SIG_PRIVATE_LICENSE')
        self.report('LuthierTests-current.ips',payload);self.collect();self.assertNotIn('PRIVATE_',self.output_text())
        other=self.root/'never-created'
        with mock.patch.object(collector.platform,'system',return_value='Linux'), mock.patch.object(sys,'argv',['collector','collect','--marker',str(self.marker),'--output',str(other)]):
            self.assertEqual(collector.main(),0)
        self.assertFalse(other.exists())

    def test_output_symlink_is_rejected_without_touching_destination(self):
        outside = self.root / 'outside'; outside.mkdir(); (outside/'private').write_text('keep')
        self.output.symlink_to(outside, target_is_directory=True)
        with self.assertRaises(OSError): self.collect()
        self.assertEqual((outside/'private').read_text(), 'keep')


class CiCallTests(unittest.TestCase):
    def run_ci(self, unit_exit, collector_exit=0, render_exit=None, operating_system='Darwin'):
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder).resolve(); scripts=root/'scripts'; scripts.mkdir(); commands=root/'commands'; commands.mkdir()
            shutil.copyfile(SCRIPT.with_name('ci_build.sh'), scripts/'ci_build.sh')
            helper=scripts/SCRIPT.name
            helper.write_text('import pathlib,sys\np=pathlib.Path(__file__).parent.parent/"calls"\nwith p.open("a") as f:f.write(sys.argv[1]+"\\n")\nraise SystemExit('+str(collector_exit)+' if sys.argv[1]=="collect" else 0)\n')
            for name,body in [('uname','echo '+operating_system),('cmake','case "$*" in *-DRENDER=*) exit '+str(render_exit or 0)+';; esac\nexit 0')]:
                path=commands/name;path.write_text('#!/bin/bash\n'+body+'\n');path.chmod(0o755)
            runner=root/'build/LuthierTests_artefacts/Release/LuthierTests';runner.parent.mkdir(parents=True)
            runner.write_text('#!/bin/bash\nexit '+str(unit_exit)+'\n');runner.chmod(0o755)
            if render_exit is not None:
                render=runner.with_name('unused').parent.parent.parent/'LuthierRender_artefacts/Release/LuthierRender'
                render.parent.mkdir(parents=True);render.write_text('#!/bin/bash\nexit 0\n');render.chmod(0o755)
            env=dict(os.environ,DISPLAY='fixture',PATH=str(commands)+os.pathsep+os.environ['PATH'],BUILD_DIR=str(root/'build'),JOBS='1',GITHUB_RUN_ID='fixture',GITHUB_RUN_ATTEMPT='1',GITHUB_JOB='test')
            result=subprocess.run(['bash',str(scripts/'ci_build.sh'),'test'],env=env,capture_output=True,text=True,timeout=10)
            return result,(root/'calls').read_text().splitlines() if (root/'calls').exists() else []

    def test_collection_is_failure_only_and_preserves_native_exit(self):
        result,calls=self.run_ci(0);self.assertEqual(result.returncode,0,result.stderr);self.assertEqual(calls,['mark'])
        result,calls=self.run_ci(139);self.assertEqual(result.returncode,139,result.stderr);self.assertEqual(calls,['mark','collect'])
        result,calls=self.run_ci(23,17);self.assertEqual(result.returncode,23,result.stderr);self.assertEqual(calls,['mark','collect'])
        result,calls=self.run_ci(0,render_exit=9);self.assertEqual(result.returncode,1,result.stderr);self.assertEqual(calls,['mark','collect'])
        result,calls=self.run_ci(13,operating_system='Linux');self.assertEqual(result.returncode,13,result.stderr);self.assertEqual(calls,[])


if __name__=='__main__': unittest.main()
