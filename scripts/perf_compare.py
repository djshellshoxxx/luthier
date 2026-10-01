#!/usr/bin/env python3
"""Compare two performance logs (performance-budget.md 0.3, PB-4).

    scripts/perf_compare.py BASELINE.log CURRENT.log [--warn 10] [--block 20] [--floor 0.02]
    scripts/perf_compare.py --selftest

The logs are the output of `LuthierTests` run with LUTHIER_PERF=1, which prints one
line per measurement:

        CouplingMatrix            0.212 units (budget 0.40)

A measurement that got more than --warn percent worse than the baseline is
flagged; more than --block percent fails the run (exit status 1). Differences
under --floor units are noise on a shared runner and are never flagged. A
measurement present in only one log is reported, not failed. The same rule
applies to any line of the form `<name> <number> units`.
"""
import re
import sys

LINE = re.compile(r"^\s*(?P<name>\S.*?)\s+(?P<units>\d+(?:\.\d+)?)\s+units\b")


def parse(text):
    found = {}
    for line in text.splitlines():
        m = LINE.match(line)
        if m:
            found[m.group("name")] = float(m.group("units"))
    return found


def compare(base, cur, warn=10.0, block=20.0, floor=0.02):
    """Returns (rows, worst) where worst is 'ok', 'warn' or 'block'."""
    rows, worst = [], "ok"
    for name in sorted(set(base) | set(cur)):
        if name not in base:
            rows.append((name, None, cur[name], None, "new"))
            continue
        if name not in cur:
            rows.append((name, base[name], None, None, "missing"))
            continue
        b, c = base[name], cur[name]
        pct = (c - b) / b * 100.0 if b > 0 else 0.0
        status = "ok"
        if c - b >= floor:
            if pct > block:
                status = "block"
            elif pct > warn:
                status = "warn"
        if status == "block" or (status == "warn" and worst == "ok"):
            worst = status
        rows.append((name, b, c, pct, status))
    return rows, worst


def report(rows):
    for name, b, c, pct, status in rows:
        bs = "-" if b is None else f"{b:.3f}"
        cs = "-" if c is None else f"{c:.3f}"
        ps = "" if pct is None else f"{pct:+.1f}%"
        print(f"  {status:<8}{name:<28}{bs:>9} -> {cs:<9}{ps}")


def selftest():
    base = parse("    A   1.000 units (budget 2)\n    B   0.500 units\n    C 0.010 units\n    gone 1.0 units\n")
    cur = parse("    A   1.150 units (budget 2)\n    B   0.700 units\n    C 0.030 units\n    fresh 1.0 units\nnoise\n")
    rows, worst = compare(base, cur)
    got = {r[0]: r[4] for r in rows}
    assert got == {"A": "warn", "B": "block", "C": "ok", "fresh": "new", "gone": "missing"}, got
    assert worst == "block"
    rows, worst = compare(base, parse("    A 1.05 units\n    B 0.5 units\n"))
    assert worst == "ok", rows
    assert compare({"A": 1.0}, {"A": 1.12})[1] == "warn"
    assert compare({"A": 1.0}, {"A": 0.5})[1] == "ok"
    print("perf_compare selftest: ok")


def main(argv):
    if "--selftest" in argv:
        selftest()
        return 0
    args, opts = [], {"--warn": 10.0, "--block": 20.0, "--floor": 0.02}
    it = iter(argv[1:])
    for a in it:
        if a in opts:
            opts[a] = float(next(it))
        else:
            args.append(a)
    if len(args) != 2:
        print(__doc__)
        return 2
    base = parse(open(args[0], errors="replace").read())
    cur = parse(open(args[1], errors="replace").read())
    if not base or not cur:
        print("no measurements found in one of the logs (was LUTHIER_PERF=1 set?)")
        return 2
    rows, worst = compare(base, cur, opts["--warn"], opts["--block"], opts["--floor"])
    report(rows)
    print(f"\nperf_compare: {worst}")
    return 1 if worst == "block" else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
