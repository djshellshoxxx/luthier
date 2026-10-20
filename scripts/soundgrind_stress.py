#!/usr/bin/env python3
"""SoundGrind-style repeatable Luthier regression stress runner.

Run against an already-built Release test binary. Only failures are written.
This harness never modifies the plugin or suppresses existing test assertions.
"""
import argparse
import json
import os
from pathlib import Path
import random
import subprocess
import sys
import time

def run(command, timeout, env):
    started = time.monotonic()
    try:
        p = subprocess.run(command, text=True, stdout=subprocess.PIPE,
                           stderr=subprocess.STDOUT, timeout=timeout, env=env)
        return p.returncode, p.stdout, round(time.monotonic() - started, 3)
    except subprocess.TimeoutExpired as exc:
        output = exc.stdout or b""
        if isinstance(output, bytes):
            output = output.decode("utf-8", "replace")
        return 124, output + "\nTIMEOUT", round(time.monotonic() - started, 3)
    except OSError as exc:
        return 127, str(exc), round(time.monotonic() - started, 3)

def main():
    p = argparse.ArgumentParser()
    p.add_argument("--runner", required=True, type=Path)
    p.add_argument("--rounds", type=int, default=30)
    p.add_argument("--seed", type=int, default=75)
    p.add_argument("--timeout", type=float, default=240)
    p.add_argument("--log", type=Path, default=Path("soundgrind-failures.jsonl"))
    p.add_argument("--suite", action="append", default=[],
                   help="Optional LuthierTests name fragment; repeat for multiple suites")
    p.add_argument("--xvfb", action="store_true",
                   help="Use xvfb-run on Linux for GUI-dependent suites")
    args = p.parse_args()
    if args.rounds < 1 or args.timeout <= 0:
        p.error("rounds and timeout must be positive")
    if not args.runner.is_file():
        p.error("test runner not found: " + str(args.runner))
    rng = random.Random(args.seed)
    suites = args.suite or [""]
    failures = 0
    args.log.parent.mkdir(parents=True, exist_ok=True)
    # Start fresh: a stale failure must never be mistaken for this campaign.
    with args.log.open("w", encoding="utf-8") as log:
        for round_no in range(args.rounds):
            suite = suites[rng.randrange(len(suites))]
            cmd = [str(args.runner.resolve())] + ([suite] if suite else [])
            if args.xvfb and sys.platform.startswith("linux"):
                cmd = ["xvfb-run", "-a", "-s", "-screen 0 1920x1080x24"] + cmd
            env = dict(os.environ)
            env["SOUNDGRIND_SEED"] = str(args.seed + round_no)
            rc, output, seconds = run(cmd, args.timeout, env)
            if rc != 0:
                failures += 1
                record = {"round": round_no, "seed": args.seed + round_no,
                          "suite": suite or "all", "returncode": rc,
                          "duration_seconds": seconds, "command": cmd,
                          "output": output[-30000:]}
                log.write(json.dumps(record, ensure_ascii=False) + "\n")
                log.flush()
                print("FAIL round={} suite={} exit={} log={}".format(
                    round_no, suite or "all", rc, args.log), file=sys.stderr)
    return 1 if failures else 0

if __name__ == "__main__":
    sys.exit(main())
