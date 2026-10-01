#!/usr/bin/env python3
"""Renders the six factory practice backing tracks (onboarding.md 6; factory-content.md 8).

Everything here is original: the chord charts and parts are written in this file, the
guitar and bass are played by Luthier itself (LuthierRender renders the generated MIDI
through a factory preset), and the drum kit is synthesised below from noise and sine
sweeps. No third-party audio, MIDI or samples are used, so there is nothing to license.

Usage (after building LuthierRender):

    pip install numpy soundfile
    python3 scripts/make_backing_tracks.py --render build/LuthierRender_artefacts/Release/LuthierRender \
        --out Resources/Practice/BackingTracks

Output is deterministic for a given Luthier build (fixed random seed, no timestamps).
Each track is a 32 kHz 16-bit stereo FLAC loop, about 30-50 s, so the whole set is a
few megabytes.
"""
import argparse, os, struct, subprocess, sys, tempfile
import numpy as np
import soundfile as sf

SR = 32000
PPQ = 480

# --- guitar voicings (MIDI note numbers, low string to high) -------------------------------
V = {
    "A7": [45, 52, 55, 61, 64], "D7": [50, 57, 60, 66], "E7": [40, 47, 50, 56, 59, 64],
    "G": [43, 47, 50, 55, 59, 67], "D": [50, 57, 62, 66], "Em": [40, 47, 52, 55, 59, 64],
    "C": [48, 52, 55, 60, 64], "Am": [45, 52, 57, 60, 64], "F": [41, 48, 53, 57, 60, 65],
    "E9": [40, 47, 50, 56, 59, 66], "A9": [45, 52, 55, 59, 61],
    "Cmaj7": [48, 52, 55, 59, 64], "Fmaj7": [41, 48, 52, 57, 64], "G6": [43, 47, 50, 55, 64],
    "Am7": [45, 52, 55, 60, 64],
    "D5": [38, 45, 50], "Bb5": [46, 53, 58], "C5": [48, 55, 60], "F5": [41, 48, 53],
    "A5": [45, 52, 57], "Em5": [40, 47, 52], "G5": [43, 50, 55], "D5h": [50, 57, 62],
}
ROOT = {"A7": 33, "D7": 38, "E7": 28, "G": 31, "D": 38, "Em": 28, "C": 36, "Am": 33, "F": 29,
        "E9": 28, "A9": 33, "Cmaj7": 36, "Fmaj7": 29, "G6": 31, "Am7": 33,
        "D5h": 38, "D5": 38, "Bb5": 34, "C5": 36, "F5": 41, "A5": 33, "Em5": 28, "G5": 31}


# --- a tiny SMF writer -------------------------------------------------------------------
def vlq(n):
    out = [n & 0x7F]
    n >>= 7
    while n:
        out.append((n & 0x7F) | 0x80)
        n >>= 7
    return bytes(reversed(out))


def write_midi(path, bpm, events):
    """events: (beat, on(bool), note, velocity, channel-less). Single track, channel 1."""
    ev = []
    for beat, on, note, vel in events:
        ev.append((int(round(beat * PPQ)), 1 if on else 0, note, vel))
    ev.sort(key=lambda e: (e[0], e[1]))   # note-offs before note-ons at the same tick
    data = b"\x00\xff\x51\x03" + struct.pack(">I", int(60_000_000 / bpm))[1:]
    data += b"\x00\xff\x58\x04\x04\x02\x18\x08"
    last = 0
    for tick, on, note, vel in ev:
        data += vlq(tick - last) + bytes([0x90 if on else 0x80, note, vel if on else 0])
        last = tick
    data += b"\x00\xff\x2f\x00"
    with open(path, "wb") as f:
        f.write(b"MThd" + struct.pack(">IHHH", 6, 0, 1, PPQ) + b"MTrk" + struct.pack(">I", len(data)) + data)


# --- parts ---------------------------------------------------------------------------------
class Part:
    def __init__(self):
        self.ev = []

    def note(self, beat, dur, note, vel):
        self.ev.append((beat, True, note, vel))
        self.ev.append((beat + dur, False, note, 0))

    def strum(self, beat, dur, chord, vel, spread=0.02, down=True):
        notes = V[chord] if down else V[chord][::-1]
        for i, n in enumerate(notes):
            self.note(beat + i * spread, dur, n, max(1, vel - (i if down else 0)))


def shuffle_comp(chords_by_bar, bars):
    p = Part()
    for bar in range(bars):
        c = chords_by_bar[bar % len(chords_by_bar)]
        b = bar * 4
        for beat in range(4):
            p.strum(b + beat, 0.55, c, 84 if beat % 2 == 0 else 96)
            p.strum(b + beat + 2 / 3, 0.28, c, 70, down=False)
    return p


def strum_comp(chords_by_bar, bars):   # pop: D, D U, U D U
    p = Part()
    for bar in range(bars):
        c = chords_by_bar[bar % len(chords_by_bar)]
        b = bar * 4
        for pos, down, vel, dur in [(0, True, 96, 1.0), (1.0, True, 78, 0.5), (1.5, False, 70, 0.5),
                                    (2.5, False, 72, 0.5), (3.0, True, 88, 0.5), (3.5, False, 70, 0.5)]:
            p.strum(b + pos, dur, c, vel, 0.015, down)
    return p


def power_eighths(chords_by_bar, bars, dur=0.4, accent=(0,)):
    p = Part()
    for bar in range(bars):
        c = chords_by_bar[bar % len(chords_by_bar)]
        for i in range(8):
            p.strum(bar * 4 + i * 0.5, dur, c, 100 if i in accent else 84, 0.004)
    return p


def funk_comp(chords_by_bar, bars):
    p = Part()
    hits = [0, 0.75, 1.5, 2.0, 2.75, 3.25, 3.5]   # sixteenth-note chops
    for bar in range(bars):
        c = chords_by_bar[bar % len(chords_by_bar)]
        for h in hits:
            p.strum(bar * 4 + h, 0.16, c, 100 if h in (0, 2.0) else 80, 0.006, down=(h % 1 == 0))
    return p


def arpeggio_comp(chords_by_bar, bars):
    p = Part()
    for bar in range(bars):
        c = chords_by_bar[bar % len(chords_by_bar)]
        notes = V[c]
        order = [0, 2, 3, 4, 3, 2, 1, 2] if len(notes) >= 5 else [0, 1, 2, 1, 2, 1, 2, 1]
        for i, k in enumerate(order):
            p.note(bar * 4 + i * 0.5, 1.6 if i == 0 else 1.0, notes[k % len(notes)], 74 if i else 84)
    return p


def metal_chug(chords_by_bar, bars):
    p = Part()
    pattern = [1, 1, 0, 1, 1, 0, 1, 1, 1, 1, 0, 1, 0, 1, 1, 0]   # sixteenths of a bar
    for bar in range(bars):
        c = chords_by_bar[bar % len(chords_by_bar)]
        for i, on in enumerate(pattern):
            if on:
                p.strum(bar * 4 + i * 0.25, 0.2, c, 112 if i in (0, 8) else 92, 0.002)
    return p


# --- bass ----------------------------------------------------------------------------------
def bass_line(kind, chords_by_bar, bars):
    p = Part()
    for bar in range(bars):
        c = chords_by_bar[bar % len(chords_by_bar)]
        r = ROOT[c]
        b = bar * 4
        if kind == "blues":       # walking shuffle: 1 3 5 6 (in semitones 0 4 7 9)
            for i, step in enumerate((0, 4, 7, 9)):
                p.note(b + i, 0.9, r + step, 88)
        elif kind == "pop":
            for i, beat in enumerate((0, 1.5, 2, 3.5)):
                p.note(b + beat, 0.8 if i != 1 else 0.45, r + (12 if i == 3 else 0), 84)
        elif kind == "rock":
            for i in range(8):
                p.note(b + i * 0.5, 0.45, r, 92)
        elif kind == "funk":
            for beat, semis, dur in ((0, 0, 0.4), (0.75, 0, 0.2), (1.5, 12, 0.2), (2.0, 0, 0.4),
                                     (2.75, 7, 0.2), (3.25, 0, 0.2), (3.5, 10, 0.3)):
                p.note(b + beat, dur, r + semis, 96 if beat in (0, 2.0) else 78)
        elif kind == "ballad":
            p.note(b, 2.0, r, 80)
            p.note(b + 2, 1.9, r + 7, 70)
        elif kind == "metal":
            for i in range(16):
                if i % 4 != 2:
                    p.note(b + i * 0.25, 0.2, r, 100 if i in (0, 8) else 88)
    return p


# --- drums (numpy synthesis) ---------------------------------------------------------------
def env(n, decay):
    return np.exp(-np.arange(n) / (decay * SR))


def kick(vel=1.0):
    n = int(0.28 * SR)
    t = np.arange(n) / SR
    f = 48 + 90 * np.exp(-t * 28)
    return vel * np.sin(2 * np.pi * np.cumsum(f) / SR) * env(n, 0.09)


def snare(rng, vel=1.0):
    n = int(0.22 * SR)
    t = np.arange(n) / SR
    tone = np.sin(2 * np.pi * 190 * t) * env(n, 0.05)
    noise = rng.standard_normal(n)
    noise = noise - np.concatenate(([0], noise[:-1])) * 0.7   # brighten
    return vel * (0.45 * tone + 0.55 * noise * env(n, 0.07))


def hat(rng, vel=1.0, open_=False):
    n = int((0.25 if open_ else 0.05) * SR)
    noise = rng.standard_normal(n)
    noise = noise - np.concatenate(([0], noise[:-1]))
    return vel * 0.35 * noise * env(n, 0.09 if open_ else 0.014)


def sidestick(rng, vel=1.0):
    n = int(0.05 * SR)
    t = np.arange(n) / SR
    return vel * (0.6 * np.sin(2 * np.pi * 420 * t) + 0.3 * rng.standard_normal(n)) * env(n, 0.012)


def drums(kind, bpm, bars, rng):
    beat = 60.0 / bpm
    total = int((bars * 4 * beat + 2.0) * SR)
    out = np.zeros(total)

    def put(sample, beats):
        i = int(beats * beat * SR)
        if i < total:
            m = min(len(sample), total - i)
            out[i:i + m] += sample[:m]

    for bar in range(bars):
        b = bar * 4
        if kind == "blues":       # shuffle: triplet-swung hats, backbeat
            for k in range(4):
                put(hat(rng, 0.8), b + k)
                put(hat(rng, 0.5), b + k + 2 / 3)
            for k in (0, 2.0 + 2 / 3):
                put(kick(0.9), b + k)
            put(snare(rng, 0.8), b + 1)
            put(snare(rng, 0.85), b + 3)
        elif kind == "pop":
            for k in range(8):
                put(hat(rng, 0.7 if k % 2 == 0 else 0.45), b + k * 0.5)
            for k in (0, 2.5):
                put(kick(0.9), b + k)
            put(snare(rng, 0.85), b + 1)
            put(snare(rng, 0.85), b + 3)
        elif kind == "rock":
            for k in range(8):
                put(hat(rng, 0.8 if k % 2 == 0 else 0.55, open_=(k == 7)), b + k * 0.5)
            for k in (0, 1.5, 2.0):
                put(kick(1.0), b + k)
            put(snare(rng, 0.95), b + 1)
            put(snare(rng, 0.95), b + 3)
        elif kind == "funk":
            for k in range(16):
                put(hat(rng, 0.75 if k % 4 == 0 else (0.5 if k % 2 == 0 else 0.3)), b + k * 0.25)
            for k in (0, 0.75, 2.5, 3.25):
                put(kick(0.95), b + k)
            put(snare(rng, 0.9), b + 1)
            put(snare(rng, 0.9), b + 3)
            put(snare(rng, 0.25), b + 1.75)   # ghost notes
            put(snare(rng, 0.25), b + 3.5)
        elif kind == "ballad":
            for k in range(8):
                put(hat(rng, 0.45 if k % 2 == 0 else 0.3), b + k * 0.5)
            put(kick(0.85), b)
            put(kick(0.7), b + 2.5)
            put(sidestick(rng, 0.9), b + 1)
            put(sidestick(rng, 0.9), b + 3)
        elif kind == "metal":     # double-time feel, driving kick
            for k in range(8):
                put(hat(rng, 0.7 if k % 2 == 0 else 0.5), b + k * 0.5)
            for k in range(16):
                if k % 4 != 2:
                    put(kick(0.85), b + k * 0.25)
            put(snare(rng, 1.0), b + 1)
            put(snare(rng, 1.0), b + 3)
            if bar % 4 == 3:
                put(hat(rng, 0.9, open_=True), b)
    return out


# --- mixing ---------------------------------------------------------------------------------
def load_mono(path):
    x, sr = sf.read(path, dtype="float64")
    assert sr == SR, (path, sr)
    return x.mean(axis=1) if x.ndim == 2 else x


def fit(x, n):
    return x[:n] if len(x) >= n else np.pad(x, (0, n - len(x)))


TRACKS = [
    # file, chart (one bar per entry), bars, bpm, guitar comp, bass kind, drum kind, guitar preset, bass preset
    dict(file="01-twelve-bar-blues-in-a", bpm=120, bars=24, kind="blues", guitar=shuffle_comp,
         chart=["A7", "D7", "A7", "A7", "D7", "D7", "A7", "A7", "E7", "D7", "A7", "E7"]),
    dict(file="02-pop-i-v-vi-iv-in-g", bpm=100, bars=16, kind="pop", guitar=strum_comp,
         chart=["G", "D", "Em", "C"]),
    dict(file="03-minor-rock-in-e", bpm=120, bars=16, kind="rock", guitar=lambda c, b: power_eighths(c, b),
         chart=["Em5", "Em5", "C5", "D5h", "Em5", "Em5", "C5", "D5h"]),
    dict(file="04-funk-in-e9", bpm=100, bars=16, kind="funk", guitar=funk_comp,
         chart=["E9", "E9", "E9", "E9", "E9", "E9", "A9", "E9"]),
    dict(file="05-ballad-in-c", bpm=68, bars=12, kind="ballad", guitar=arpeggio_comp,
         chart=["C", "G6", "Am7", "Fmaj7"]),
    dict(file="06-metal-riff-in-drop-d", bpm=140, bars=24, kind="metal", guitar=metal_chug,
         chart=["D5", "D5", "Bb5", "C5"]),
]


def render_stem(exe, midi, preset, out_wav, tail):
    subprocess.run([exe, "--midi", midi, "--preset", preset, "--rate", str(SR), "--depth", "24",
                    "--tail", str(tail), "--out", out_wav], check=True, stdout=subprocess.DEVNULL)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--render", required=True, help="path to the LuthierRender binary")
    ap.add_argument("--out", required=True)
    ap.add_argument("--guitar-preset", default=None, help="override the per-track guitar preset")
    ap.add_argument("--only", default=None)
    args = ap.parse_args()
    os.makedirs(args.out, exist_ok=True)
    presets = {
        "blues": ("Semi-Hollow Chime", "P-Bass Flatwound"), "pop": ("Strummed Dreadnought", "P-Bass Flatwound"),
        "rock": ("Single-Cut Crunch", "P-Bass Flatwound"), "funk": ("Clean Double-Cut Funk", "J-Style Fingerstyle"),
        "ballad": ("Fingerstyle Folk", "P-Bass Flatwound"), "metal": ("Modern Metal Chug", "P-Bass Flatwound"),
    }
    with tempfile.TemporaryDirectory() as tmp:
        for t in TRACKS:
            if args.only and args.only not in t["file"]:
                continue
            gpreset, bpreset = presets[t["kind"]]
            if args.guitar_preset:
                gpreset = args.guitar_preset
            bpm, bars = t["bpm"], t["bars"]
            seconds = bars * 4 * 60.0 / bpm
            n = int(seconds * SR)
            gm, bm = os.path.join(tmp, "g.mid"), os.path.join(tmp, "b.mid")
            write_midi(gm, bpm, t["guitar"](t["chart"], bars).ev)
            write_midi(bm, bpm, bass_line(t["kind"], t["chart"], bars).ev)
            gw, bw = os.path.join(tmp, "g.wav"), os.path.join(tmp, "b.wav")
            render_stem(args.render, gm, gpreset, gw, 2)
            render_stem(args.render, bm, bpreset, bw, 2)
            rng = np.random.default_rng(1234)
            g, b = fit(load_mono(gw), n), fit(load_mono(bw), n)
            d = fit(drums(t["kind"], bpm, bars, rng), n)
            # Balance: normalise each stem to a target RMS, then mix (guitar left-of-centre, hats via drums).
            def rms_to(x, target):
                r = np.sqrt(np.mean(x ** 2)) + 1e-9
                return x * (target / r)
            g, b, d = rms_to(g, 0.10), rms_to(b, 0.09), rms_to(d, 0.08)
            left = 0.9 * g + b + d
            right = 0.6 * g + b + d
            mix = np.stack([left, right], axis=1)
            # A loop must not click on the way round: fade the last 40 ms into the first sample.
            fade = int(0.04 * SR)
            mix[-fade:] *= np.linspace(1, 0, fade)[:, None]
            mix[:fade] *= np.linspace(0, 1, fade)[:, None]
            peak = np.max(np.abs(mix))
            mix *= 0.89 / peak   # about -1 dBFS
            path = os.path.join(args.out, t["file"] + ".flac")
            sf.write(path, mix, SR, subtype="PCM_16", format="FLAC")
            print(f"{t['file']}: {seconds:.1f} s, {os.path.getsize(path) / 1e6:.2f} MB (guitar '{gpreset}', bass '{bpreset}')")


if __name__ == "__main__":
    sys.exit(main())
