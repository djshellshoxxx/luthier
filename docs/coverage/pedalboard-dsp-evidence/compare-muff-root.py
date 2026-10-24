#!/usr/bin/env python3
"""Compare baseline/candidate traces from measure-muff-prepared-root.cpp."""
from array import array
import math
from pathlib import Path
import sys

if len(sys.argv) != 3:
    raise SystemExit("usage: compare-muff-root.py baseline.raw candidate.raw")
traces = []
for name in sys.argv[1:]:
    values = array("d")
    values.frombytes(Path(name).read_bytes())
    if len(values) != 262144:
        raise SystemExit("unexpected trace length")
    traces.append(values)
print("model,rate,stereo_values,max_abs_delta,rms_delta,delta_dbfs")
for model_index, model in enumerate(("NYC", "Wicker")):
    for rate_index, rate in enumerate((44100, 48000, 96000, 192000)):
        first = (model_index * 4 + rate_index) * 32768
        delta = [a - b for a, b in zip(*(t[first:first + 32768] for t in traces))]
        peak = max(map(abs, delta))
        rms = math.sqrt(sum(x * x for x in delta) / len(delta))
        db = 20 * math.log10(rms) if rms else -math.inf
        print(f"{model},{rate},{len(delta)},{peak:.17g},{rms:.17g},{db:.9f}")
