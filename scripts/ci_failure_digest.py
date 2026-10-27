#!/usr/bin/env python3
"""Summarise downloaded CI log artifacts into a short Markdown digest.

Usage: ci_failure_digest.py <dir-with-logs-*-artifacts> <run-url> <sha>

Reads only the text logs that build.yml already uploads (logs-<edition>-<platform>-<sha>):
unit-tests.log, build.log, pluginval/clap-validator/auval logs. Prints Markdown on stdout,
capped well below GitHub's comment limit. It never changes a result; it only makes the
uploaded evidence readable from the pull-request conversation.
"""
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
    else:
        parts.append("unit tests: no unit-tests.log (build failed or step skipped)")
    build = list(path.rglob("build.log"))
    if build:
        text = build[0].read_text(errors="replace")
        errs = tail_matches(text, r"(error[: ]|fatal error|FAILED:|LINK : fatal|undefined reference)", after=3, cap=60)
        if errs:
            parts.append("build errors:\n```\n" + "\n".join(errs) + "\n```")
    for log in sorted(path.rglob("*.log")):
        name = log.name.lower()
        if any(k in name for k in ("pluginval", "clap-validator", "auval")):
            text = log.read_text(errors="replace")
            keep = [l for l in text.splitlines() if re.search(r"SUCCESS|FAILED|failed|passed|skipped|Bus error|Segmentation|crash", l)]
            parts.append(f"{log.name}: " + (" | ".join(k.strip() for k in keep[-6:]) or "(no summary lines)"))
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
