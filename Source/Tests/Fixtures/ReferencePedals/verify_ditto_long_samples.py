"""Independent scalar reference for the sparse full-duration sample diagnostics.

Reads the explicit native fixture log; no asset files or host state are needed.
This checks long-range integer phase/page indexing, not a replacement for T8.
"""
import math
import re
import struct
import sys

def f32(value):
    return struct.unpack('<f', struct.pack('<f', value))[0]

def source(frame, layer):
    bits = (frame + (0x12345678 if layer else 0x87654321)) & ((1 << 64) - 1)
    bits = ((bits ^ (bits >> 30)) * 0xbf58476d1ce4e5b9) & ((1 << 64) - 1)
    bits = ((bits ^ (bits >> 27)) * 0x94d049bb133111eb) & ((1 << 64) - 1)
    bits ^= bits >> 31
    noise = ((bits & 0xffff) / 65535.0 - 0.5) * 0.00001
    phase = 2 * math.pi * (4000.0 if layer else 1000.0) * (frame % 44100) / 44100
    return f32((0.03125 if layer else 0.125) * math.sin(phase) + noise)

def i0(value):
    terms = [1.0]
    for index in range(1, 80):
        terms.append(terms[-1] * value * value / (4 * index * index))
    return math.fsum(terms)

def reference(frame, layer):
    # The mathematical FIR specification evaluated directly at this sample,
    # rather than the product's cached table, page access or unrolled dot loop.
    first, fraction = divmod(frame * 147, 640)
    fraction /= 640
    half = math.ceil(0.5 * 1.25 * 172 / (2.285 * 2 * math.pi * 2050 / 44100))
    cutoff = (20000 + 22050) * 0.5 / 44100
    weights, samples = [], []
    denominator = i0(19.0)
    for offset in range(-half, half + 1):
        distance = offset - fraction
        if abs(distance) > half:
            weight = 0.0
        else:
            argument = 2 * cutoff * distance
            sinc = 1.0 if argument == 0 else math.sin(math.pi * argument) / (math.pi * argument)
            window = i0(19 * math.sqrt(max(0.0, 1 - (distance / half) ** 2))) / denominator
            weight = 2 * cutoff * sinc * window
        weights.append(weight)
        samples.append(source((first + offset) % 13230000, layer))
    return f32(math.fsum(weight * sample for weight, sample in zip(weights, samples)) / math.fsum(weights))

if __name__ == '__main__':
    text = open(sys.argv[1], encoding='utf-8').read()
    rows = re.findall(r'long sample frame=(\d+) base=([-\d.e+]+) undo=([-\d.e+]+)', text)
    assert len(rows) == 8, f'Expected eight full-duration sample diagnostics, got {len(rows)}'
    for frame, base, undo in rows:
        for layer, actual in [(False, float(base)), (True, float(undo))]:
            expected = reference(int(frame), layer)
            assert struct.pack('<f', actual) == struct.pack('<f', expected), (frame, layer, actual, expected)
    print('Full-duration independent scalar reference:16 float32 samples bit-exact')
