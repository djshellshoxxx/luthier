# 03 Finger reach

**IDs:** SU-08. **ED:** 4. **Status:** partial.

**Summary.** Replace the voicer's fixed fret span with a physical reach in millimetres, and flag chords the hand cannot reach at their position.

**Current state (corrected from roadmap).** A player setting already exists for a fret count, not a reach:

- `RhythmEngine.h` 286: `handSpan`, default 5, user range 3 to 7.
- `RhythmPanel.h` 199: `handSpanSlider`.
- `RhythmEngine.cpp` 347: `voicer->setMaxFretSpan(getHandSpan() + wide)`.
- `RubricVoicer.h` 196-200: `setMaxFretSpan`, a constant per voicer.

So the roadmap's "no player setting" is wrong. The gap is that the setting counts frets, not millimetres, and does not depend on the position on the neck.

## 1. User-facing behaviour

- Setup panel: "Finger reach (index to little finger)", slider 80 to 150 mm, default 110 mm.
- Chord inspector, when the player has set a reach: "Needs a wider reach than your fingers (N frets) here." where N is the frets the chord needs.
- Until the player sets a reach, the chord inspector behaves exactly as v1 (no warnings from this feature).

## 2. Engine and model

Nut-to-fret distance, scale `S` (648 mm):

```
d(n) = S x (1 - 2^(-n/12))
```

Frets covered from position `p` (the lowest fretted fret), reach `R`:

```
frets_covered(p) = max n such that  d(p + n - 1) - d(p - 1) <= R
```

Worked values at S = 648 mm, R = 110 mm (computed from the formula):

| Position p | frets_covered |
|---|---|
| 1 | 3 |
| 5 | 4 |
| 12 | 6 |

Errata: the roadmap gives 4, 5 and 8. The formula does not produce them. The spacing values in the roadmap are also off: fret 1 to 2 is 34.3 mm (the 36 mm figure is nut to fret 1, `d(1)` = 36.4 mm); fret 4 to 5 is 28.9 mm; fret 12 to 13 is 18.3 mm.

Chord check: the chord needs `F = highest fretted fret - lowest fretted fret + 1` frets (open strings excluded). Flag if `F > frets_covered(p)`, with `p` the chord's lowest fretted fret.

Voicer integration:

- When no reach is set (`reach_set` false), the voicer uses its v1 constant path. Output is unchanged (SU-M01).
- When a reach is set, the voicer uses `frets_covered(p)` per position in place of its constant.
- The rhythm engine's `handSpan` slider is unchanged in v2.0. Whether rhythm should read the reach is open (section 9).

Precompute: a table of `frets_covered(p)` for p = 1 to 24, recomputed on the message thread when the reach changes. 24 integers. Cost: 24 x 24 operations, under 1 microsecond.

## 3. Data model and parameters

- `player_reach_mm`: state, 80 to 150, default 110. Not automatable.
- `reach_set`: state, bool, default false. Tells the voicer whether to use the reach. Added to the setup block; the roadmap does not list it.

## 4. State, file format, migration

- Both fields in the setup block (`reach_mm`, `reach_set`). Missing means 110 and false.
- v1 presets: `reach_set` false. Voicer output identical, bit-identical renders (SU-M01).

## 5. Edition

Free and Pro (both). Matches roadmap section 6.

## 6. Performance budget

- Table rebuild on the message thread only. Zero per block.
- The voicer's audio-thread reads, if any, use the precomputed table. No allocation.

## 7. Test plan

- **SU-08.** S = 648, R = 110: `frets_covered` is 3, 4 and 6 for p = 1, 5 and 12. A chord needing 4 frets is flagged at p = 1 and not at p = 12. Assert against the formula values.
- Monotone: for fixed p, `frets_covered` is non-decreasing in R; for fixed R, non-decreasing in p (spacing shrinks up the neck).
- Range: R = 80 and R = 150 both produce valid tables (no zero-fret or overflow).
- `reach_set` false: voicer output equals v1 for the factory chord set.

## 8. Effort and dependencies

- ED 4 (roadmap 9).
- Depends on: `RubricVoicer.h` 196, `RhythmEngine` (`handSpan`) for the coexistence rule.

## 9. Open

- Roadmap SU-08 values (4, 5, 8) need reconciling with the formula (3, 4, 6). Confirm the intended definition of `p` before the test is frozen.
- Ask at first run or leave at default until the chord inspector flags a problem (roadmap 10.4).
- Should rhythm voicing read the reach? Proposed: no in v2.0.
