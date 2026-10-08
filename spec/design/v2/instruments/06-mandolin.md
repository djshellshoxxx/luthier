# 06 Mandolin (roadmap 3.4)

Covers: **IN-05** (courses), **IN-06** (tremolo) (IN-12, IN-13, IN-14 shared; see INDEX.md).

## Summary

Mandolin is eight strings in four courses. A course is two strings at one pitch,
detuned to beat (or set an octave apart). The tremolo is a rapid alternate-stroke
pattern on held notes. The bowl body is a new shape. The course topology reuses the
existing 12-string pairing.

## Status

**missing.** Tuning only, in `Source/Notation/AsciiTabReader.cpp` 278. No course model.

## User-facing behaviour

- Eight strings in four courses, tuned G3 D4 A4 E5 (MIDI 55, 62, 69, 76).
- **Course spread** (0-8 cents): detunes the two strings of each course so they beat.
  Tooltip: "Detunes the two strings of each course".
- **Octave courses** (on/off): the second string of each course is an octave from the
  first (see the correction below).
- **Tremolo rate** (8-20 Hz): alternate-stroke rate on held notes. Tooltip: "Rate of
  rapid alternate picking on held notes".

## Roadmap corrections

1. **Tuning is standard, not reentrant.** Roadmap 3.4 says "reentrant with the octave-up
   pairs". Mandolin is standard tuning (G3 D4 A4 E5). Reentrant is a ukulele term.
   Fixed here.
2. **Octave direction.** Roadmap says "octave-up pairs". Common octave mandolins are
   tuned an octave *lower* (G2 D3 A3 E4). The spec implements the roadmap's wording
   (second string +12 semitones) under `mando_octave_courses`. The owner must confirm
   the direction before the research file lands (see INDEX open items).
3. **Beat formula.** Roadmap IN-05 says "beats at 3 cents". The beat rate is
   `f_beat = f * (2^(c/1200) - 1)`, which is about `f * 0.001733` for c = 3. At A4
   (440 Hz) that is 0.76 Hz. The test asserts the formula, not a bare cent number.

## Engine and model design

**Courses.** Each course is a pair of strings sharing a fret. The 12-string pairing
(`Parameters.h` 207, "A 12-string's pairs share") gives the nut-slot and fret-sharing
topology. Reuse it.

Second string of course k, with first-string pitch `f_k`:

```
normal:        f2 = f_k * 2^(c / 1200)                  (c = mando_course_spread, cents)
octave course: f2 = f_k * 2                              (+12 semitones, roadmap wording)
```

The detune is applied as a pitch offset on the second string, not as a tension change.
Both strings are excited by the same stroke.

**Tremolo (rhythm scheduler + pattern).** The stroke scheduler is the existing rhythm
engine (`StrumGesture` stroke path). The new part is the pattern:

```
T = 1 / mando_trem_rate                      (s; 12 Hz gives 83.33 ms)
stroke k at t_k = k * T, direction alternates down / up
```

Each stroke is one `StrumGesture` event at alternate direction, with the stroke
duration at 0.6 T (D). The held note keeps its own decay; tremolo only re-excites
it. Releasing tremolo stops the strokes; the note then decays normally (correcting
the roadmap's "sustain", which means re-excitation, not a longer decay).

Rate changes apply from the next stroke, not mid-stroke.

**Body (bowl).** A new body shape, `BodyShape::Bowl` (appended to the enum; do not
reorder), built with the Modal bank. Values are D until `docs/research/INSTR_mandolin.md`
lands. The open question on the family (roadmap 10.1) is answered here: **no new
family**. A course flag inside `acoustic` is enough, because the bowl changes the body,
not the parts model.

**RT-safety.**
- Course state is a fixed array of four pairs, created in `prepare()`.
- The stroke queue is a fixed ring of at most 8 events. No allocation on the audio
  thread.
- Stroke times are double, computed in samples, rounded once.
- Spread and rate are read per block (no zipper: they change pitch offset and the next
  stroke only).
- Octave-course and bowl are structural: stage and commit between notes.

## Data model and parameters

| ID | Range | Default | Automatable | Unit | Control |
|---|---|---|---|---|---|
| `mando_course_spread` | 0-8 | 3 | yes | cents | Mandolin card |
| `mando_octave_courses` | bool | off | no | - | Mandolin card |
| `mando_trem_rate` | 8-20 | 12 | yes | Hz | Mandolin card |

Appended in `// ==== BEGIN V2-INST params ====`.

## State, file format, migration

- `mando_octave_courses` and the bowl body are saved in the mandolin part set
  (`.luthierguitar`). New file only.
- The three parameters default to values that render the course topology with a 3-cent
  detune. A non-mandolin preset never reads them.

## Edition

| Item | Free | Pro |
|---|---|---|
| Mandolin (courses, bowl, tremolo) | Pro | Pro |

## Performance budget

- Eight strings: the cost of eight voices at full chord, about 0.2 units (roadmap 7).
- Tremolo: scheduler only; counted with the rhythm engine's existing stroke cost.

## Test plan

- **IN-05a (course beat).** A course at `mando_course_spread = 3` beats at
  `f * (2^(3/1200) - 1)`. Measure the beat rate from the rendered output. The error is
  within 0.2 cent of the detune, expressed as a beat rate (about 0.1 Hz at 440 Hz).
- **IN-05b (octave course).** With `mando_octave_courses` on, the second string sounds
  exactly one octave (2:1) above the first, within 0.2 cent.
- **IN-05c (zero spread).** Spread 0 gives two strings at unison, zero beat.
- **IN-06 (tremolo).** At 12 Hz, 20 consecutive stroke onsets are spaced 83.33 ms
  apart, each within 1 ms of the spacing.
- **IN-06b (direction).** Strokes alternate down / up.
- **IN-06c (release).** When tremolo is released, no strokes occur after the last
  scheduled one.
- Shared: IN-12, IN-13 (`mando_course_spread`, `mando_trem_rate` visible), IN-14.

## Effort and dependencies

- **ED 14:** course model 4, bowl body and tuning 4, tremolo pattern 3, card and
  parameters 1, tests 2.
- Dependencies: `part-acoustics.md` (parts model), the existing 12-string pairing
  (`Parameters.h` 207), the rhythm engine's `StrumGesture` stroke path, and
  `docs/research/INSTR_mandolin.md` (missing; bowl and course values are D until it lands).
