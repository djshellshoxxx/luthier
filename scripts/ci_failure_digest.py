#!/usr/bin/env python3
"""Summarise downloaded CI log artifacts into a short Markdown digest.

Usage: ci_failure_digest.py <dir-with-logs-*-artifacts> <run-url> <sha>

Reads only the text logs that build.yml already uploads (logs-<edition>-<platform>-<sha>):
unit-tests.log, build.log, pluginval/clap-validator/auval logs. Prints Markdown on stdout,
capped well below GitHub's comment limit. It never changes a result; it only makes the
uploaded evidence readable from the pull-request conversation.
"""
import json
import pathlib
import re
import sys

LIMIT = 60000


def tail_matches(text, pattern, after=6, cap=40):
    lines = text.splitlines()
    out = []
    for i, line in enumerate(lines):
        if re.search(pattern, line):
            out.extend(lines[i:i + 1 + after])
            if len(out) >= cap:
                out.append("... (truncated)")
                break
    return out


def digest_artifact(path):
    parts = []
    tests = list(path.rglob("unit-tests.log"))
    if tests:
        text = tests[0].read_text(errors="replace")
        summary = [l for l in text.splitlines() if re.search(r"tests failed|tests passed|^ (PASSED|FAILED)|checks\), [0-9.]+ s", l)]
        parts.append("unit tests: " + (" / ".join(s.strip() for s in summary[-3:]) or "no summary line (crash or timeout?)"))
        fails = tail_matches(text, r"\[FAIL\]", after=5, cap=60)
        if fails:
            parts.append("```\n" + "\n".join(fails) + "\n```")
        if not summary:
            # Where the runner stopped: the last suite and test rows before the exit.
            parts.append("unit-tests.log tail:\n```\n" + "\n".join(text.splitlines()[-20:]) + "\n```")
        # Sharded runs (ci_build.sh TEST_SHARDS): one line per shard, and where
        # a shard that printed no summary line stopped.
        for shard in sorted(path.rglob("unit-tests-shard-*.log")):
            stext = shard.read_text(errors="replace")
            slines = [l for l in stext.splitlines() if re.search(r"^ [0-9]+ tests, |tests failed", l)]
            if slines:
                parts.append(f"{shard.name}: {slines[-1].strip()}")
            else:
                parts.append(f"{shard.name}: no summary line (crash or timeout?); tail:\n```\n"
                             + "\n".join(stext.splitlines()[-12:]) + "\n```")
    else:
        parts.append("unit tests: no unit-tests.log (build failed or step skipped)")
    build = list(path.rglob("build.log"))
    if build:
        text = build[0].read_text(errors="replace")
        errs = tail_matches(text, r"(error[: ]|fatal error|FAILED:|LINK : fatal|undefined reference)", after=3, cap=60)
        if errs:
            parts.append("build errors:\n```\n" + "\n".join(errs) + "\n```")
    for report in sorted(path.rglob("*-report-*.json")):
        # Sanitised OS crash reports (collect_native_failure_evidence.py): exception and top frames.
        try:
            data = json.loads(report.read_text(errors="replace"))
        except ValueError:
            continue
        images = {i.get("imageIndex"): i.get("name", "?") for i in data.get("images", []) if isinstance(i, dict)}
        frames = [f"{images.get(f.get('imageIndex'), '?')} {f.get('symbol', hex(f.get('imageOffset', 0)))}"
                  for f in data.get("frames", [])[:16] if isinstance(f, dict)]
        head = " ".join(str(data.get(k, "")) for k in ("process", "type", "signal", "terminationNamespace")).strip()
        parts.append(f"{report.name}: {head}\n```\n" + "\n".join(frames) + "\n```")
    for log in sorted(path.rglob("*-lldb.log")):
        # Validator crash re-run under lldb (ci_build.sh validate, macOS): the stop reason and frames.
        lines = [l for l in log.read_text(errors="replace").splitlines() if re.search(r"stop reason|frame #", l)]
        parts.append(f"{log.name}:\n```\n" + "\n".join(lines[:40]) + "\n```")
    for log in sorted(path.rglob("*.log")):
        name = log.name.lower()
        if name.endswith("-lldb.log"):
            continue
        if any(k in name for k in ("pluginval", "clap-validator", "auval")):
            text = log.read_text(errors="replace")
            keep = [l for l in text.splitlines() if re.search(r"SUCCESS|FAILED|failed|passed|skipped|Bus error|Segmentation|crash", l)]
            parts.append(f"{log.name}: " + (" | ".join(k.strip() for k in keep[-6:]) or "(no summary lines)"))
            crash = tail_matches(text, r"pluginval received|Segmentation fault|Bus error", after=0, cap=4)
            if crash:
                # The validator test running when the process died.
                lines = text.splitlines(); at = lines.index(crash[0]) if crash[0] in lines else len(lines)
                parts.append("```\n" + "\n".join(lines[max(0, at - 8):at + 1]) + "\n```")
    return parts


def main():
    root = pathlib.Path(sys.argv[1])
    run_url, sha = sys.argv[2], sys.argv[3]
    out = [f"### Native CI digest for `{sha}`", f"Run: {run_url}", ""]
    artifacts = sorted(p for p in root.iterdir() if p.is_dir()) if root.exists() else []
    if not artifacts:
        out.append("No log artifacts were downloaded.")
    for art in artifacts:
        out.append(f"#### {art.name}")
        out.extend(digest_artifact(art))
        out.append("")
    text = "\n".join(out)
    if len(text) > LIMIT:
        text = text[:LIMIT] + "\n... (digest truncated)"
    print(text)


if __name__ == "__main__":
    main()
