#!/usr/bin/env python3
"""The ComboHarness phrases (Source/Tests/ComboHarness.h) and a legato melody
as a type-0 MIDI file at 120 bpm, for scripts/assist_off_golden_check.sh."""
import struct, sys

ev = []
def t(sec): return int(round(sec * 960))          # 480 ticks per quarter at 120 bpm
def on(s, n, v): ev.append((t(s), bytes([0x90, n, v])))
def off(s, n): ev.append((t(s), bytes([0x80, n, 0])))
def cc(s, c, v): ev.append((t(s), bytes([0xB0, c, v])))
def pw(s, val): ev.append((t(s), bytes([0xE0, val & 0x7f, (val >> 7) & 0x7f])))

b = 0.0
on(b, 52, 100); off(b + 0.6, 52); b += 2.0
for n in [40, 47, 52, 56, 59, 64]: on(b, n, 96)
for n in [40, 47, 52, 56, 59, 64]: off(b + 0.8, n)
b += 2.5
on(b, 60, 100)
for i in range(21): pw(b + 0.1 + 0.015 * i, 8192 + int(8191 * i / 20))
for i in range(21): pw(b + 0.5 + 0.015 * i, 16383 - int(8191 * i / 20))
off(b + 0.85, 60); b += 2.0
on(b, 55, 100); on(b + 0.2, 57, 40); off(b + 0.21, 55); on(b + 0.4, 55, 40); off(b + 0.41, 57); off(b + 0.7, 55); b += 2.0
cc(b, 67, 127)
for i in range(6): on(b + 0.12 * i, 40, 110); off(b + 0.12 * i + 0.1, 40)
cc(b + 0.75, 67, 0); b += 2.0
cc(b, 73, 127); on(b + 0.001, 64, 100); cc(b + 0.3, 72, 127); on(b + 0.301, 57, 120)
off(b + 0.7, 64); off(b + 0.7, 57); cc(b + 0.701, 73, 0); cc(b + 0.701, 72, 0); b += 2.0
for i in range(16): on(b + 0.04 * i, 57, 70 + (i % 3) * 20); off(b + 0.04 * i + 0.035, 57)
b += 2.0
cc(b, 64, 127); on(b + 0.001, 45, 90); on(b + 0.002, 52, 90); off(b + 0.2, 45); off(b + 0.2, 52); cc(b + 0.9, 64, 0); b += 2.0
for i, n in enumerate([64, 66, 67, 69, 71, 72, 71, 69, 67]): on(b + 0.15 * i, n, 90); off(b + 0.15 * i + 0.17, n)

ev.sort(key=lambda x: x[0])
def vlq(x):
    out = [x & 0x7f]; x >>= 7
    while x: out.insert(0, (x & 0x7f) | 0x80); x >>= 7
    return bytes(out)
trk = vlq(0) + b'\xff\x51\x03' + struct.pack('>I', 500000)[1:]
last = 0
for tk, m in ev: trk += vlq(tk - last) + m; last = tk
trk += vlq(0) + b'\xff\x2f\x00'
open(sys.argv[1], 'wb').write(b'MThd' + struct.pack('>IHHH', 6, 0, 1, 480) + b'MTrk' + struct.pack('>I', len(trk)) + trk)
