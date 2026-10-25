"""Bounded diagnostic evidence; never publishes or edits test acceptance."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import shutil
import subprocess
import tarfile

FILTERS = ('HostState', 'GuiReach', 'JamPlugin', 'PluginBuses', 'Tune',
           'routing', 'MidiOutPanel', 'ModelGaps', 'RefPedalRetirement')
BUILD = Path('build-ci')
LOGS = BUILD / 'logs'


def run_logged(args, name, timeout=1200):
    LOGS.mkdir(parents=True, exist_ok=True)
    with (LOGS / name).open('w', encoding='utf-8') as log:
        try:
            rc = subprocess.run([str(x) for x in args], stdout=log,
                                stderr=subprocess.STDOUT, timeout=timeout).returncode
        except subprocess.TimeoutExpired:
            log.write('\nDIAGNOSTIC TIMEOUT: this is a failure, not a skipped gate.\n')
            rc = 124
    print(f'{name}: exit {rc}', flush=True)
    return rc


def runner_path():
    base = BUILD / 'LuthierTests_artefacts/Release/LuthierTests'
    if platform.system() == 'Windows':
        return base.with_suffix('.exe')
    app = Path(str(base) + '.app') / 'Contents/MacOS/LuthierTests'
    return app if app.exists() else base


def missing_filters(listing):
    entries = [line.lower() for line in listing.splitlines() if ' :: ' in line]
    return [word for word in FILTERS if not any(word.lower() in line for line in entries)]


def focus():
    runner = runner_path().resolve()
    rc = run_logged([runner, '--list', *FILTERS], 'focused-list.log', 60)
    missing = missing_filters((LOGS / 'focused-list.log').read_text(errors='replace'))
    if rc or missing:
        raise RuntimeError(f'Focused selection unavailable: exit={rc}, missing={missing}')
    return run_logged([runner, *FILTERS], 'focused-tests.log')


def architecture():
    if platform.system() != 'Darwin':
        raise RuntimeError('Universal architecture verification requires native macOS')
    artefacts = BUILD / 'Luthier_artefacts/Release'
    paths = [artefacts / f'{folder}/Luthier Pro.{ext}/Contents/MacOS/Luthier Pro'
             for folder, ext in [('VST3', 'vst3'), ('CLAP', 'clap'), ('Standalone', 'app')]]
    paths += [runner_path(), BUILD / 'LuthierRender_artefacts/Release/LuthierRender']
    failed = False
    for i, path in enumerate(paths):
        if not path.is_file():
            raise RuntimeError(f'Required native executable missing: {path}')
        failed |= bool(run_logged(['lipo', '-verify_arch', 'arm64', 'x86_64', path],
                                  f'universal-{i}.log', 60))
    record = {'host_machine': platform.machine(), 'runtime_machine': platform.machine(),
              'uname': platform.uname()._asdict(), 'verified_paths': [str(p) for p in paths],
              'both_architectures_present': not failed,
              'both_architectures_executed': False}
    (LOGS / 'architecture.json').write_text(json.dumps(record, indent=2))
    return int(failed)


def bundle():
    """Archive an already staged build, plus source, provenance, logs and licences."""
    source = subprocess.check_output(['git', 'rev-parse', 'HEAD'], text=True).strip()
    if source != os.environ['GITHUB_SHA'] or len(source) != 40:
        raise RuntimeError('Checkout does not match the immutable event SHA')
    system = 'windows' if platform.system() == 'Windows' else 'macos'
    staged = Path('dist') / system
    if not staged.is_dir():
        raise RuntimeError('Existing CI stage did not produce artifacts')
    out = Path('diagnostic-artifacts')
    out.mkdir(exist_ok=True)
    with tarfile.open(out / 'UNQUALIFIED-DIAGNOSTIC-binaries.tar.gz', 'w:gz') as archive:
        archive.add(staged, arcname=system)
        archive.add(runner_path().parent, arcname='test-runner')
    subprocess.run(['git', 'archive', '--format=tar.gz', f'--output={out / "source.tar.gz"}', source], check=True)
    licences = out / 'licensing'
    licences.mkdir(exist_ok=True)
    for name in ['LICENSE', 'THIRD_PARTY_LICENCES.txt']:
        shutil.copy2(name, licences / name)
    shutil.copy2('Resources/THIRD_PARTY_LICENCES.txt', licences / 'resources-THIRD_PARTY_LICENCES.txt')
    for name, directory in [('JUCE', Path('ThirdParty/JUCE')),
                            ('clap-juce-extensions', Path('ThirdParty/clap-juce-extensions'))]:
        candidates = list(directory.glob('LICENSE*')) + list(directory.glob('LICENCE*'))
        if not candidates:
            raise RuntimeError(f'Missing dependency licence: {directory}')
        for candidate in candidates:
            if candidate.is_file():
                shutil.copy2(candidate, licences / f'{name}-{candidate.name}')
    shutil.copytree(LOGS, out / 'logs', dirs_exist_ok=True)
    status = {name: os.environ.get(f'DIAGNOSTIC_{name.upper()}', 'not-recorded')
              for name in ['build', 'focus', 'validate', 'architecture', 'stage']}
    manifest = {'qualification': 'UNQUALIFIED-DIAGNOSTIC', 'source_sha': source,
                'system': system, 'runtime_arch': platform.machine(), 'edition': 'Pro',
                'dependency_shas': {name: subprocess.check_output(
                    ['git', '-C', f'ThirdParty/{name}', 'rev-parse', 'HEAD'], text=True).strip()
                    for name in ['JUCE', 'clap-juce-extensions']},
                'lto': False, 'filters': FILTERS, 'outcomes': status,
                'not_run': ['full unit suite', 'strictness10', 'macOS AU/auval',
                            'other macOS runtime architecture', 'Free', 'Linux',
                            'release installer/signing/notarization', 'render CLI smoke', 'full model acceptance'],
                'signing': 'No distribution signing claim; existing macOS recipe uses ad-hoc loadability signing only'}
    (out / 'UNQUALIFIED-DIAGNOSTIC.json').write_text(json.dumps(manifest, indent=2))
    (out / 'SOURCE_SHA').write_text(source + '\n')
    hashes = {p.relative_to(out).as_posix(): hashlib.sha256(p.read_bytes()).hexdigest()
              for p in sorted(out.rglob('*')) if p.is_file() and p.name != 'SHA256SUMS'}
    (out / 'SHA256SUMS').write_text(''.join(f'{digest}  {path}\n' for path, digest in hashes.items()))
    return 0


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('step', choices=['focus', 'architecture', 'bundle'])
    raise SystemExit(globals()[parser.parse_args().step]())
