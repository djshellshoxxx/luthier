# 03 Lap steel and dobro/resonator (roadmap 3.2)

Covers: **IN-01** (lap steel), **IN-02** (dobro) (IN-12, IN-13, IN-14 shared; see INDEX.md).

## Summary

Lap steel is the `lapSteel` slide mode with no lap-steel part set. The dobro is
`SlideMode::dobro` on the existing resonator body, so it needs only a card and
tests. v2 ships a lap part set (six or eight strings, flat neck, high nut), tuning
presets, and a dobro cone toggle.

## Status

**partial.**
- Lap steel playing: `SlideMode::lapSteel` (`Source/DSP/Slide/SlideEngine.h` 26).
  Instrument: **missing** (no `lap` part, no guitar file).
- Dobro playing: `SlideMode::dobro` exists. Resonator body: `BodyShape::Resonator`
  (`PartAcoustics.cpp` 112-113, 154); `Resonator/Resonator Steel.luthierguitar`.

## User-facing behaviour

- **Lap steel:** pick a tuning (C6, E9, A6, custom). The bar sits on every string;
  there is no fretting hand, so the strings ring out with no fret-hand damping (U1).
- **Dobro:** the resonator body with the bar. A cone on/off control sets the cone's
  extra bite and sustain.
- Body choice on the lap card: resonator or solid (open question 2 in the roadmap;
  this spec recommends both, selected by the body card).

## Engine and model design

**Playing.** Reuse `SlideEngine` as is. `contactFret(s, barFret, numStrings,
scaleLengthMm)` (`SlideEngine.h` 154) already gives the bar's contact on each string.
A tuning only sets the open pitch of each string:

```
f_s(bar) = f_open[s] * 2^(bar / 12)      (bar in fret units, double)
```

**Tuning presets (D placeholders, replaced by `docs/research/INSTR_lap_steel.md`):**

| Preset | Strings low to high (MIDI) | Notes |
|---|---|---|
| E9 (default) | E2 B2 E3 G#3 B3 D#4 (40 47 52 56 59 63) | 6 strings |
| C6 | C3 E3 G3 A3 C4 E4 (48 52 55 57 60 64) | 6 strings |
| A6 | E2 A2 C#3 F#3 A3 C#4 (40 45 49 54 57 61) | 6 strings |
| custom | per-string MIDI array in the part data | up to 8 |

Presets are data in `Resources/`, not code.

**Part set (roadmap 3.2, with a decision).** The roadmap says "add a `lap` part
family under `acoustic`". Ground rule 3 adds a family only when the existing
families cannot express the instrument. This spec recommends: **no new family**.
A lap neck is a fretless neck with a high nut and flat radius. Use a `Parts/Frets/None`
(zero-fret) part and a flat `Parts/Neck` under `electric` (solid) or `resonator`
(resonator body). If `part-acoustics.md` cannot express a zero-fret neck, add the
`lap` family then. Decide in `INSTR_lap_steel.md`.

Scale 610-660 mm (D, default 625). Six strings default; eight optional. Gauge set
from the lap-steel string set in the research file (D).

**Dobro cone (roadmap gap).** The roadmap UI lists "Cone on/off (body)" but section 4
has no parameter for it. This spec proposes `dobro_cone` (bool, default on,
not automatable, body card), appended in the V2-INST block. Cone on is the shipped
`BodyShape::Resonator`. Cone off removes the cone's upper mode from the body config
(a body-config flag, so `BodyEngine::stageBodyConfig` / `commitStagedConfig`
applies it), which is the measurable difference IN-02 tests.

**RT-safety.**
- Tuning and body changes are structural: stage on the message thread and commit
  between notes, as `BodyEngine` does (`stageBodyConfig` 85, `commitStagedConfig` 90).
  No allocation or locking on the audio thread.
- Double precision for pitch (`f_s`) and the bar mapping.
- `reset()` on `SlideEngine` clears bar and clank state.

## Data model and parameters

| ID | Range | Default | Automatable | Control |
|---|---|---|---|---|
| `lap_tuning` | choice: C6, E9, A6, custom | E9 | no | Lap card |
| `dobro_cone` (added, see above) | bool | on | no | Dobro card |

Custom tuning data (per-string MIDI, 6-8 values) is part data in the `.luthierguitar`,
not an APVTS parameter.

## State, file format, migration

- Tuning preset and custom array are saved in the lap part set (`Parts/`,
  `.luthierguitar`). New file only; no existing file changes.
- `dobro_cone` defaults on, so the existing resonator presets render unchanged.
- A lap preset loaded on a build without the lap part set falls back to the nearest
  instrument with a notice naming the original (roadmap 5, IN-14).

## Edition

| Item | Free | Pro |
|---|---|---|
| Lap steel | Pro | Pro |
| Dobro (resonator + dobro mode) | Pro | Pro |

Dobro is Pro in the roadmap. The dobro mode and resonator body already exist, so
"Free keeps every instrument it has today" applies. Verify the Resonator guitar
is not in `isFreeGuitarIndex` (`Source/Edition.h`). If it is, keep Free.

## Performance budget

- Lap steel: six or eight strings through the existing string path. No new per-string
  cost beyond the string count (roadmap 7).
- Dobro: 0 units beyond the resonator body already in use.

## Test plan

- **IN-01 (lap steel, no fret damping).** Open E9, bar at fret 5: each of the six
  strings sounds `f_open * 2^(5/12)` within 5 cents. The fret-damping coefficient
  applied to each string is exactly its no-damping value (`slide-guitar.md` 3).
- **IN-02 (dobro cone).** Resonator body, dobro mode, cone on: the upper cone mode
  is present within 3 dB of the factory resonator reference (`BodyShape::Resonator`).
  Cone off: the same mode is at least 3 dB lower. The second check makes the toggle
  testable (a no-op toggle fails).
- **IN-01b (tunings).** C6, E9, A6 each produce the open pitches in the table above
  within 5 cents.
- Shared: IN-12, IN-13 (visible `lap_tuning` and `dobro_cone` controls), IN-14
  (missing lap part falls back with notice).

## Effort and dependencies

- **ED 10:** lap part set and presets 8, dobro card and tests 2.
- Dependencies: `slide-guitar.md` 1 and 3 (lap and dobro modes), `part-acoustics.md`
  (zero-fret neck, resonator body), `docs/research/INSTR_lap_steel.md` (missing; D
  values until it lands).
- Tooltip correction: the roadmap's lap tooltip says "frets are damped". Change it to
  "No fretting hand: the bar is the only contact" to match U1 and IN-01.
