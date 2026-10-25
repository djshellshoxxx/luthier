#!/usr/bin/env python3
"""Failure-only macOS evidence, never raw host state or arbitrary process reports.

IPS field structure: https://developer.apple.com/documentation/xcode/interpreting-the-json-format-of-a-crash-report
Only target-process exception metadata and code frames survive the allowlist.
Application-specific text, registers, paths, device/incident IDs and unknown fields
are deliberately omitted. Fixture tests run on Linux; real macOS CI is still needed.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import re
import stat
import time
import uuid

TARGETS = ('LuthierTests', 'LuthierRender')
REPORT_NAME = re.compile(r'^(LuthierTests|LuthierRender)(?:[-_.].*)?\.(ips|crash)$')
MAX_BYTES = 16 * 1024 * 1024
MAX_REPORTS = 32
EXCEPTIONS = set('EXC_BAD_ACCESS EXC_BAD_INSTRUCTION EXC_ARITHMETIC EXC_EMULATION EXC_SOFTWARE EXC_BREAKPOINT EXC_SYSCALL EXC_MACH_SYSCALL EXC_RPC_ALERT EXC_CRASH EXC_RESOURCE EXC_GUARD EXC_CORPSE_NOTIFY'.split())
SIGNALS = set('SIGABRT SIGSEGV SIGBUS SIGILL SIGFPE SIGTRAP SIGKILL SIGTERM SIGQUIT SIGSTOP SIGINT SIGPIPE SIGSYS SIGEMT SIGXCPU SIGXFSZ'.split())
NUMBER = re.compile(r'-?(?:0|[1-9][0-9]*)(?:\.[0-9]+)?(?:[eE][+-]?[0-9]+)?')
KNOWN_KEYS = set('preset referencePedals ui normalization modulation metronome stability setlist routing midiLearn tune lockedParameters slotBActive presetMorphPosition formatVersion savedBy jamPlayValue jamFillNowValue clickToMain liveMode schema schemaVersion parameters params lfos envelopes sequencers followers points steps v g s p rate sync division phase depth symmetry shape retrigger smoothing bipolar delay attack hold decay sustain release loop curves source string length direction swing instances order enabled mix mixLaw limiterOn reportLatency cpuCapPct tempoBpm midiChan tempoSource abSelect abOut mode boardStyle window parallelNorm numLanes lanes gainDb phaseInvert loops used kind blend sendDb returnDb first last feedback on src dst gain kill lpHz runawayOk extras id rev placement lane slot inDb outDb x mounting name'.split())


def open_directory(path, create=False):
    """Pin each directory component; a symlink anywhere in the path is refused."""
    path = Path(os.path.abspath(path))
    fd = os.open(path.anchor, os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW)
    try:
        for part in path.parts[1:]:
            try:
                child = os.open(part, os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW, dir_fd=fd)
            except FileNotFoundError:
                if not create: raise
                os.mkdir(part, 0o700, dir_fd=fd)
                child = os.open(part, os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW, dir_fd=fd)
            os.close(fd); fd = child
        result = fd; fd = None
        return result
    finally:
        if fd is not None: os.close(fd)


def read_regular(path, start=None, end=None, directory_fd=None):
    if directory_fd is None:
        path = Path(path); parent = open_directory(path.parent)
        try: return read_regular(path.name, start, end, parent)
        finally: os.close(parent)
    fd = os.open(path, os.O_RDONLY | os.O_NOFOLLOW | os.O_NONBLOCK, dir_fd=directory_fd)
    with os.fdopen(fd, 'rb') as stream:
        before = os.fstat(stream.fileno())
        if not stat.S_ISREG(before.st_mode) or before.st_size > MAX_BYTES:
            raise ValueError('unsupported file')
        if start is not None and not start <= before.st_mtime_ns <= end:
            raise ValueError('outside current job')
        content = stream.read(MAX_BYTES + 1)
        after = os.fstat(stream.fileno())
        if len(content) > MAX_BYTES or before.st_size != after.st_size or before.st_mtime_ns != after.st_mtime_ns:
            raise ValueError('changing or oversized file')
    return content


def write_artifact(directory, name, content):
    fd = open_directory(directory, create=True)
    temporary = '.evidence-' + uuid.uuid4().hex
    try:
        try:
            if stat.S_ISLNK(os.stat(name, dir_fd=fd, follow_symlinks=False).st_mode):
                raise OSError('artifact symlink refused')
        except FileNotFoundError:
            pass
        out = os.open(temporary, os.O_WRONLY | os.O_CREAT | os.O_EXCL | os.O_NOFOLLOW, 0o600, dir_fd=fd)
        with os.fdopen(out, 'wb') as stream:
            stream.write(content.encode('utf-8'))
        os.replace(temporary, name, src_dir_fd=fd, dst_dir_fd=fd)
    finally:
        try:
            os.unlink(temporary, dir_fd=fd)
        except FileNotFoundError:
            pass
        os.close(fd)


def clear_owned_artifacts(directory):
    """Remove only this helper's previous products, without following links."""
    directory = Path(directory)
    try:
        fd = open_directory(directory)
    except FileNotFoundError:
        return
    try:
        with os.scandir(fd) as entries:
            names = [entry.name for entry in entries if entry.name in ('summary.json', 'host-state-diff.log')
                     or re.fullmatch(r'(?:LuthierTests|LuthierRender)-report-\d{4}\.json', entry.name)]
        for name in names:
            os.unlink(name, dir_fd=fd)
    finally:
        os.close(fd)


def mark_start(marker, job_key, now_ns=None, output=None):
    marker = Path(marker); now_ns = time.time_ns() if now_ns is None else now_ns
    key = hashlib.sha256(job_key.encode()).hexdigest()
    try:
        previous = json.loads(read_regular(marker))
        if previous.get('job_key_hash') == key and type(previous.get('started_ns')) is int and 0 < previous['started_ns'] <= now_ns:
            return
    except FileNotFoundError:
        pass
    except (ValueError, AttributeError):
        pass
    if output is not None:
        clear_owned_artifacts(output)
    write_artifact(marker.parent, marker.name, json.dumps({'started_ns': now_ns, 'job_key_hash': key}) + '\n')


def unsigned(value):
    return value if type(value) is int and 0 <= value <= 0xffffffffffffffff else None


def safe_symbol(value):
    # Code identifiers only, not arbitrary strings or application diagnostics.
    if not isinstance(value, str) or len(value) > 512 or not re.fullmatch(r'[A-Za-z0-9_:<>,*&()\[\]~+. -]+', value):
        return None
    if value.startswith(('luthier::', 'juce::', 'std::', '__cxxabiv1::', '__pthread_', 'pthread_')) or value in ('abort', 'raise', 'start', '_sigtramp', '__assert_rtn', 'std::terminate()'):
        return value
    return None


def safe_image(value):
    if value in TARGETS or value in ('dyld', 'AppKit', 'Foundation', 'CoreFoundation'):
        return value
    if isinstance(value, str) and re.fullmatch(r'lib(?:system_[A-Za-z0-9_]+|c\+\+|c\+\+abi|objc)\.dylib', value):
        return value
    return '<other-image>'


def sanitize_frames(frames):
    out = []
    if not isinstance(frames, list):
        return out
    for frame in frames[:256]:
        if not isinstance(frame, dict):
            continue
        clean = {key: frame[key] for key in ('imageIndex', 'imageOffset', 'symbolLocation') if unsigned(frame.get(key)) is not None}
        symbol = safe_symbol(frame.get('symbol'))
        if symbol:
            clean['symbol'] = symbol
        if clean:
            out.append(clean)
    return out


def sanitize_ips(text):
    decoder = json.JSONDecoder()
    metadata, position = decoder.raw_decode(text.lstrip())
    body = json.loads(text.lstrip()[position:])
    if not isinstance(metadata, dict) or str(metadata.get('bug_type')) != '309' or not isinstance(body, dict):
        raise ValueError('unsupported IPS report')
    process = body.get('procName')
    if process not in TARGETS or any(metadata.get(key, process) != process for key in ('name', 'app_name')):
        raise ValueError('different process')
    clean = {'process': process, 'format': 'ips', 'frames': []}
    if body.get('cpuType') in ('ARM-64', 'ARM', 'X86-64', 'X86'):
        clean['cpuType'] = body['cpuType']
    exception = body.get('exception', {})
    if isinstance(exception, dict):
        for key, allowed in [('type', EXCEPTIONS), ('signal', SIGNALS)]:
            if isinstance(exception.get(key), str) and exception[key] in allowed:
                clean[key] = exception[key]
    termination = body.get('termination', {})
    if isinstance(termination, dict):
        if termination.get('namespace') in ('SIGNAL', 'DYLD', 'CODESIGNING', 'LIBSYSTEM', 'RUNNINGBOARD', 'WATCHDOG'):
            clean['terminationNamespace'] = termination['namespace']
        if unsigned(termination.get('code')) is not None:
            clean['terminationCode'] = termination['code']
    threads = body.get('threads', [])
    faulting = unsigned(body.get('faultingThread'))
    if isinstance(threads, list):
        if faulting is not None and faulting < len(threads) and isinstance(threads[faulting], dict):
            clean['frames'] = sanitize_frames(threads[faulting].get('frames'))
        else:
            for thread in threads:
                if isinstance(thread, dict) and thread.get('triggered') is True:
                    clean['frames'] = sanitize_frames(thread.get('frames')); break
    clean['lastExceptionBacktrace'] = sanitize_frames(body.get('lastExceptionBacktrace'))
    images = body.get('usedImages', [])
    referenced = {frame['imageIndex'] for frame in clean['frames'] + clean['lastExceptionBacktrace'] if 'imageIndex' in frame}
    clean['images'] = []
    if isinstance(images, list):
        for index in sorted(referenced):
            if index >= len(images) or not isinstance(images[index], dict):
                continue
            image = images[index]; entry = {'imageIndex': index, 'name': safe_image(image.get('name'))}
            for key in ('base', 'size'):
                if unsigned(image.get(key)) is not None:
                    entry[key] = image[key]
            if isinstance(image.get('uuid'), str) and re.fullmatch(r'[0-9a-fA-F]{8}(?:-[0-9a-fA-F]{4}){3}-[0-9a-fA-F]{12}', image['uuid']):
                entry['uuid'] = image['uuid']
            clean['images'].append(entry)
    return clean


def sanitize_classic(text):
    process = re.search(r'^Process:\s*(LuthierTests|LuthierRender)\s+\[\d+\]\s*$', text, re.MULTILINE)
    if not process:
        raise ValueError('different process')
    clean = {'process': process[1], 'format': 'crash', 'frames': []}
    exception = re.search(r'^Exception Type:\s*(EXC_[A-Z0-9_]+)(?:\s*\((SIG[A-Z0-9]+)\))?', text, re.MULTILINE)
    if exception:
        if exception[1] in EXCEPTIONS: clean['type'] = exception[1]
        if exception[2] in SIGNALS: clean['signal'] = exception[2]
    in_stack = False
    for line in text.splitlines():
        if re.fullmatch(r'Thread \d+ Crashed:(?::)?(?:\s.*)?', line):
            in_stack = True; continue
        if in_stack:
            match = re.fullmatch(r'\s*\d+\s+(\S+)\s+(0x[0-9a-fA-F]+)\s+(.*?)\s+\+\s+(\d+)\s*', line)
            if not match:
                in_stack = False; continue
            entry = {'image': safe_image(match[1]), 'address': match[2], 'symbolLocation': int(match[4])}
            symbol = safe_symbol(match[3])
            if symbol: entry['symbol'] = symbol
            clean['frames'].append(entry)
            if len(clean['frames']) == 256: break
    return clean


def safe_host_line(line):
    prefix = 'HostState exact JSON mismatch: '
    if not line.startswith(prefix):
        return None
    reason = re.search(r'\breason=(type|numeric-representation|key-order|key-membership|length|string-representation|value|whitespace-or-encoding|diagnostic-parse-unavailable)(?:\s|$)', line)
    path_match = re.search(r'\bpath=(\$(?:\.[A-Za-z_][A-Za-z0-9_]*|\[(?:\d+|property#\d+)\])*)(?:\s|$)', line)
    if not reason or not path_match:
        return prefix + 'diagnostic unavailable; values=<redacted>'
    path = '$'; opaque = False
    for part in re.findall(r'\.[A-Za-z_][A-Za-z0-9_]*|\[(?:\d+|property#\d+)\]', path_match[1][1:]):
        if part.startswith('.'):
            key = part[1:]
            if opaque or key not in KNOWN_KEYS:
                path += '[redacted]'; opaque = True
            else:
                path += part
                opaque = key in ('parameters', 'params', 'extras', 'name')
        else:
            if part.startswith('[property#') or path == '$': opaque = True
            path += part
    fields = []
    for name in ('bytes', 'firstByte', 'lengths', 'tokenLengths'):
        match = re.search(r'\b'+name+r'=(\d{1,20}(?:/\d{1,20})?)(?:\s|$)', line)
        if match: fields.append(name+'='+match[1])
    fields.extend(['path='+path, 'reason='+reason[1]])
    types = re.search(r'\btypes=((?:object|array|string|number|null|boolean|unknown)/(?:object|array|string|number|null|boolean|unknown))(?:\s|$)', line)
    if types: fields.append('types='+types[1])
    tokens = re.search(r'\btokens=([^\s/]+)/([^\s]+)', line)
    if not opaque and reason[1]=='numeric-representation' and tokens and all(len(token)<=48 and NUMBER.fullmatch(token) for token in tokens.groups()):
        fields.append('tokens='+tokens[1]+'/'+tokens[2])
    else:
        fields.append('values=<redacted>')
    return prefix + ' '.join(fields)


def collect(marker, output, report_dirs, logs, now_ns=None):
    now_ns = time.time_ns() if now_ns is None else now_ns
    summary = {'reports': 0, 'backtraces': 0, 'candidates': 0, 'rejected': 0, 'hostDiagnostics': 0, 'directoriesExamined': 0, 'unavailableReportDirectories': 0, 'omittedByLimit': 0}
    notes = []
    try:
        metadata = json.loads(read_regular(marker)); start = metadata['started_ns']
        if type(start) is not int or not 0 < start <= now_ns: raise ValueError('invalid marker')
    except (OSError, ValueError, KeyError, TypeError):
        start = None; notes.append('No report search performed: job-start marker unavailable; historical reports not collected.')
    clear_owned_artifacts(output)
    candidates = []
    if start is not None:
        opened = []
        try:
            for root in report_dirs:
                try:
                    root_fd = open_directory(root)
                except OSError:
                    summary['unavailableReportDirectories'] += 1; continue
                opened.append(root_fd)
                directories = [root_fd]
                try:
                    retired = os.open('Retired', os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW, dir_fd=root_fd)
                    opened.append(retired); directories.append(retired)
                except OSError:
                    pass
                for fd in directories:
                    summary['directoriesExamined'] += 1
                    with os.scandir(fd) as entries:
                        for entry in entries:
                            if REPORT_NAME.fullmatch(entry.name) and entry.is_file(follow_symlinks=False):
                                info = entry.stat(follow_symlinks=False)
                                if start <= info.st_mtime_ns <= now_ns:
                                    candidates.append((info.st_mtime_ns, fd, entry.name))
            summary['candidates'] = len(candidates)
            summary['omittedByLimit'] = max(0, len(candidates)-MAX_REPORTS)
            for mtime, fd, name in sorted(candidates, key=lambda item:item[0], reverse=True)[:MAX_REPORTS]:
                try:
                    text = read_regular(name, start, now_ns, fd).decode('utf-8')
                    report = sanitize_ips(text) if name.endswith('.ips') else sanitize_classic(text)
                except (OSError, ValueError, TypeError, RecursionError):
                    summary['rejected'] += 1; continue
                report['sourceMtimeNs'] = mtime
                summary['reports'] += 1
                summary['backtraces'] += bool(report['frames'] or report.get('lastExceptionBacktrace'))
                write_artifact(output, report['process']+'-report-'+str(summary['reports']).zfill(4)+'.json', json.dumps(report, indent=2)+'\n')
        finally:
            for fd in opened:
                os.close(fd)
        if not candidates:
            notes.append('No current-job OS crash report for LuthierTests or LuthierRender was readable in accessible DiagnosticReports directories.')
        elif not summary['reports']:
            notes.append('Matching current-job files existed, but no supported target-process report could be extracted.')
        if not summary['backtraces']:
            notes.append('No backtrace available in selected OS reports (no supported frames extracted).')
    host_lines = []
    if start is not None:
        for log in logs:
            try: text = read_regular(log, start, now_ns).decode('utf-8', errors='replace')
            except (OSError, ValueError): continue
            for line in text.splitlines():
                safe = safe_host_line(line)
                if safe is not None and len(host_lines)<100: host_lines.append(safe)
    summary['hostDiagnostics'] = len(host_lines)
    if not host_lines: host_lines.append('No privacy-safe HostState diagnostic line was extracted from current-job logs.')
    write_artifact(output, 'host-state-diff.log', '\n'.join(host_lines)+'\n')
    write_artifact(output, 'summary.json', json.dumps(dict(summary, jobStartNs=start, collectionNs=now_ns, notes=notes), indent=2)+'\n')
    print('macOS native failure evidence: reports='+str(summary['reports'])+' backtraces='+str(summary['backtraces'])+' hostDiagnostics='+str(summary['hostDiagnostics']))
    for note in notes: print(note)
    return summary


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest='command', required=True)
    mark = commands.add_parser('mark'); mark.add_argument('--marker', type=Path, required=True); mark.add_argument('--job-key', required=True); mark.add_argument('--output', type=Path)
    scan = commands.add_parser('collect'); scan.add_argument('--marker', type=Path, required=True); scan.add_argument('--output', type=Path, required=True); scan.add_argument('--log', type=Path, action='append', default=[])
    args = parser.parse_args()
    if platform.system() != 'Darwin':
        print('Native macOS failure evidence skipped: this host is not macOS.'); return 0
    try:
        if args.command == 'mark': mark_start(args.marker, args.job_key, output=args.output)
        else: collect(args.marker, args.output, [Path.home()/'Library/Logs/DiagnosticReports', Path('/Library/Logs/DiagnosticReports')], args.log)
    except (OSError, ValueError, TypeError):
        print('Native failure evidence unavailable; no raw private data was staged.'); return 1
    return 0


if __name__ == '__main__': raise SystemExit(main())
