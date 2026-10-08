# 05 Ukulele (roadmap 3.3)

Covers: **IN-03** (reentrant tuning), **IN-04** (body air resonance) (IN-12, IN-13, IN-14 shared; see INDEX.md).

## Summary

A ukulele is a four-string `acoustic` instrument with a small soundbox. The string
model needs no change, because `StringEngine` takes pitch per string. New work is the
part set (four scales), tuning data (reentrant, linear, baritone), and a soundbox body
tuned for an air resonance near 275 Hz.

## Status

**missing.** Tuning only, in `Source/Notation/AsciiTabReader.cpp` 276-277. No part,
no body.

## User-facing behaviour

- Pick a size (soprano, concert, tenor, baritone) and a tuning (reentrant GCEA,
  linear GCEA, baritone DGBE).
- Reentrant: the fourth string is high G. Tooltip: "Reentrant: the fourth string is
  high G".
- The instrument sits in a "small instruments" group in the picker, not with the
  guitars (roadmap open question 3, recommended; this spec adopts it).

## Engine and model design

**Tunings (data, in `Resources/`).** MIDI, strings 1-4 (1 = highest):

| Tuning | Strings 1-4 (MIDI) | Pitches |
|---|---|---|
| reentrant GCEA (default) | 69, 64, 60, 67 | A4 E4 C4 G4 |
| linear GCEA | 69, 64, 60, 55 | A4 E4 C4 G3 |
| baritone DGBE | 64, 59, 55, 50 | E4 B3 G3 D3 |

Reentrant is a per-string octave flag: the fourth string is written at 67 (G4), not
55 (G3). `StringEngine` already takes pitch per string, so the flag is only data.

**Scale lengths.** Soprano 330 mm, concert 381 mm, tenor 432 mm (roadmap 3.3, D
pending `docs/research/INSTR_ukulele.md`). Baritone is not in the roadmap. Use
about 480-500 mm (D); confirm in the research file.

**Body (soundbox).** Use `BodyEngine` Modal mode with a new body config. The air
(Helmholtz) resonance is

```
f_A = (c / (2 * pi)) * sqrt( S / (V * L_e) )
c = 343 m/s, S = soundhole area (m^2), V = box volume (m^3),
L_e = t + 1.7 r   (end correction for a hole of radius r in a plate of thickness t)
```

Choose `V` (top-to-back ratio and depth) and `S` so that `f_A = 275 Hz` (D). The
roadmap's 250-300 Hz is the accepted range. The body config is built at load time,
so the formula is evaluated once (double precision), not per block.

The air resonance gain is set with `BodyEngine::setAirResonanceGainDb` (existing).

**RT-safety.** Body and tuning are structural: stage and commit between notes
(`BodyEngine::stageBodyConfig`, `commitStagedConfig`). No allocation on the audio
thread. Pitch is double precision throughout.

## Data model and parameters

| ID | Range | Default | Automatable | Control |
|---|---|---|---|---|
| `uke_tuning` | choice: reentrant, linear, baritone | reentrant | no | Ukulele card |

Size (scale) is part data, not a parameter. Non-automatable: structural state.

## State, file format, migration

- Four new `.luthierguitar` part sets (soprano, concert, tenor, baritone) and the
  body. No existing file changes.
- `uke_tuning` is saved in the preset.
- On a build without the ukulele, a preset naming it falls back to the nearest
  instrument with a notice naming the ukulele (roadmap 5, IN-14).

## Edition

| Item | Free | Pro |
|---|---|---|
| Ukulele (all sizes, all tunings) | Pro | Pro |

## Performance budget

- Four strings: less than a six-string guitar. Body is one modal bank of the size the
  body config uses (same bank as the existing acoustic bodies; no new cost class).
- No new per-string cost beyond the string count (roadmap 7).

## Test plan

- **IN-03 (reentrant).** Strings 1-4 sound 69, 64, 60, 67. The lowest pitched string
  sounds 60 (C4). Each within 5 cents of the tab reader's reference (`AsciiTabReader`
  276).
- **IN-03b (linear and baritone).** Linear lowest pitched string is 55; baritone is
  50 (D3), each within 5 cents.
- **IN-04 (air resonance).** The body's air-mode frequency is between 250 and 300 Hz,
  and within 10 Hz of the 275 Hz design target. Check with the formula on the built
  config and with the rendered magnitude peak.
- **IN-04b (size).** Each size's string scale matches the table (330, 381, 432 mm).
- Shared: IN-12, IN-13 (`uke_tuning` visible), IN-14 (fallback notice).

## Effort and dependencies

- **ED 12:** part sets and tunings 4, soundbox body and air-resonance tuning 5, card
  and fallback 1, tests 2.
- Dependencies: `part-acoustics.md` (parts model), `BodyEngine` (existing),
  `docs/research/INSTR_ukulele.md` (missing; body and gauge values are D until it lands).
- Open: the body's measured values. Roadmap open question 5 asks who supplies them.
