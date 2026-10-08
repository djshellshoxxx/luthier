# 02 Fretless bass glide (roadmap 3.6)

Covers: **IN-08**, **IN-09** (IN-12, IN-13, IN-14 shared; see INDEX.md).

## Summary

A fretless bass exists. v2 adds a legato pitch glide between notes on the same
string, and proves that fret buzz is silent on a fretless neck.

## Status

**partial.** The instrument exists; the glide does not.

Evidence:
- `Resources/Guitars/Bass/Fretless Bass.luthierguitar`; `Parts/Frets/Fretless (unlined)`;
  `Parameters.h` `fretless`.
- `slide_*` parameters drive the slide model only, not a fretless glide.

## User-facing behaviour

- Legato note-on on the same string while the previous note sounds glides from the
  previous pitch to the new one over `fretless_glide_ms`.
- A plucked (new attack) note does not glide; it starts at its target pitch.
- Glide is pitch only. Finger noise (`fingerstyle-attack.md`) and string decay are
  unchanged.
- No fret buzz on a fretless part at any action (`fret-buzz.md`).

## Engine and model design

**Trigger.** The note event carries a legato flag (or, if the event path has none,
the voice-steal path with the string still sounding; decide at implementation,
verify the flag name in the input path). Only a legato note-on starts a glide.

**Curve.** Reuse `SlideCurve::easeIn` (`Source/DSP/Slide/SlideEngine.h` 70) for the
curve evaluation. Do not reuse `SlideEngine::startMove` or the bar model, which carry
clank and bar state a bass does not have.

With `T = fretless_glide_ms / 1000` seconds and `x = clamp(t / T, 0, 1)`:

```
cents(t) = c_start + (c_target - c_start) * x^2      (easeIn, quadratic)
f(t)     = f_ref * 2^(cents(t) / 1200)               (double precision)
```

`c_start` is the current pitch of the string in cents at the moment of the note-on
(so a glide that is interrupted restarts from the pitch heard). `c_target` is the
new note's pitch. The curve is monotone, so overshoot is zero (IN-08).

**Where.** On the per-string pitch path, before `StringEngine`. The string itself is
unchanged; only the pitch fed to it moves.

**RT-safety.**
- Per-string glide state is a fixed struct in an array sized to the string count,
  created in `prepare()`. No allocation on the audio thread.
- `fretless_glide_ms` is read once per block. It applies to glides that start after
  the change; an in-flight glide keeps its duration (no zipper, no jump).
- `reset()` clears all glide state.
- Sample count from `T * sampleRate`, computed in `prepare()` or at note-on.

## Data model and parameters

| ID | Range | Default (new) | Automatable | Unit |
|---|---|---|---|---|
| `fretless_glide_ms` | 20-300 (plus 0 = off, see state) | 90 | yes | ms |

Appended in `// ==== BEGIN V2-INST params ====`.

## State, file format, migration

**Roadmap correction.** Roadmap 4 gives a default of 90 ms. Existing fretless
presets have no glide, so a default of 90 would change them, and IN-12 requires them
to render bit-identically.

**Resolution (D):**
- The parameter's stored value defaults to **0 (glide off)** when a preset lacks the
  key. This is the migration default for every existing preset.
- The factory default for a *new* fretless preset is 90.
- 0 is the off value, outside the 20-300 range. The UI shows "Off" at 0.

Saved presets carry the value; no `.luthierguitar` format change.

## Edition

| Item | Free | Pro |
|---|---|---|
| Fretless with glide | Free | Free |

As roadmap 6. Confirm against `Source/Edition.h` whether the fretless bass is Free today.

## Performance budget

0.01 units per gliding note (roadmap 7). Per-string state is a few doubles.

## Test plan

- **IN-08a (arrival).** A legato note from 40 Hz to 55 Hz, glide 90 ms: at t = 90 ms
  +/-5 ms the pitch is within 5 cents of target. Measure from the rendered output
  (zero-crossing or STFT), not from internal state alone.
- **IN-08b (no overshoot).** Over the whole glide the pitch never exceeds the target
  by more than 10 cents. The spec expects 0 cents (monotone). The test asserts <= 10.
- **IN-08c (plucked note).** A new-attack note starts at its target pitch (no glide,
  first 10 ms within 5 cents).
- **IN-08d (glide off).** `fretless_glide_ms = 0` gives the v1 output bit-identically.
- **IN-09 (no fret buzz).** With `fret-buzz.md` enabled at its maximum action setting,
  the fret-buzz generator's output on a fretless part is exactly zero.
- Shared: IN-12 (existing fretless presets bit-identical with the migration default),
  IN-13 (`fretless_glide_ms` has a visible control), IN-14.

## Effort and dependencies

- **ED 4:** trigger detection, glide state, tests, card control.
- Dependencies: `bass-techniques.md` (fretless), `fret-buzz.md` (exemption),
  `fingerstyle-attack.md` (unchanged attack).
