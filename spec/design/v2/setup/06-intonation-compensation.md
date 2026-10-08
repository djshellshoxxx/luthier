# 06 Intonation compensation

**IDs:** SU-01, SU-02. **ED:** 6. **Status:** partial. `TuningEngine.h` 76 has a per-string intonation slope (cents per fret, default 0.30) and 112 its setter; `TuningEngine.h` 113 adds aging. `intonation_error` (`Parameters.h` 66) is the player's random error. No saddle length offset exists.

**Summary.** Six per-string saddle offsets in millimetres, plus a 12th-fret check that reports cents per string and solves the offset that corrects a sharp note.

## 1. User-facing behaviour

- Setup panel: six offset fields, `intonation_comp_1` to `_6`, in mm, -4 to +4, default 0, in 0.1 mm steps (D).
- **Check intonation** button: for each string, plays the open 12th-fret harmonic and the fretted 12th-fret note, and reports cents for each string. Positive is sharp.
- A "Suggested offset" beside each string gives the offset that brings the check within 0.5 ct.
- Changing an offset changes the open-string pitch too (section 2). The panel says so and points to the tuner.

## 2. Engine and model

Nut-to-fret distance: `d(n) = L (1 - 2^(-n/12))`. The fretted speaking length at fret `n` is `l_n = L - d(n)`. A saddle offset `d_s` (positive is longer) changes it to `l_n + d_s`.

Pitch change at fret `n`:

```
dc_n = 1200 x log2( l_n / (l_n + d_s) )
```

Longer means flatter. This is the sign the physics requires.

At the twelfth fret `l = L/2`:

```
dc_12 = -1200 x log2( 1 + d_s / (L/2) )
```

| Scale | +1 mm at 12th | +2 mm at 12th |
|---|---|---|
| 648 mm | -5.34 ct | -10.65 ct |
| 628 mm | -5.50 ct | -11.0 ct |

Sensitivity is fret dependent. At the fifth fret on 648 mm, +1 mm gives about -3.6 ct. The check uses only the 12th fret; the per-fret slope model (`TuningEngine.h` 76) stays on top.

Errata: the roadmap formula has the opposite sign (lengthening sharpens). The roadmap SU-02 "+10.6 cents (sharp)" for a string set 2 mm long is wrong. A 2 mm longer string is 10.65 ct **flat**.

Open-string effect: with the nut fixed, `l_0 = L`, so `dc_0 = 1200 x log2( L / (L + d_s) )`. On 648 mm, +1 mm gives about -2.66 ct at the open string. Changing an offset therefore needs a retune (the wizard, `04`, step 3, handles this).

Check measurement: the check renders both notes through the engine offline, not from the microphone. Pitch is read by the engine's own analysis. `c` is the cents of the fretted 12th above the 12th harmonic of the open string (`c > 0` sharp).

Solve for the offset (the Suggested offset):

```
d_s = (L/2) x ( 2^(c / 1200) - 1 )        for a sharp note (c > 0)
```

For `c = +10.65 ct` on 648 mm this gives `d_s = +2.0 mm` (lengthen, flatten by 10.65 ct). Clamp to -4 to +4 mm.

Application: TuningEngine computes each string's speaking length `L_s = L + d_s` on the message thread before the delay line is retuned. The audio thread reads the precomputed delay length. No per-block cost.

## 3. Data model and parameters

- `intonation_comp_1` to `_6`: state, mm, -4 to +4, default 0. Not automatable.
- `intonation_error`: unchanged, still the player's error.
- The per-fret slope (`TuningEngine.h` 76) is unchanged.

## 4. State, file format, migration

- Setup block fields `intonation_comp[1..6]`. Missing means 0.
- v1 presets: all zero, so the render is bit-identical (SU-M01).

## 5. Edition

Free and Pro (both).

## 6. Performance budget

- One `log2` or `pow` per string per setup change, on the message thread. Zero per block.
- Audio thread: reads a precomputed double. Under 0.001 units.
- Double precision for all cents and lengths.

## 7. Test plan

- **SU-01.** +1 mm at the 12th on 648 mm gives -5.34 ct; on 628 mm gives -5.50 ct. Each within 0.2 ct of the formula, and the sign is negative (flat).
- **SU-02.** A string with +2 mm reads -10.65 ct (flat) at the 12th, within 0.5 ct. A string measured at +10.65 ct sharp, after applying the Suggested offset (+2.0 mm), reads within 0.5 ct of 0.
- Solve round-trip: for any `c` in -20 to +20 ct, applying `d_s` from the solve brings the 12th-fret error within 0.5 ct.
- Open-string shift: +1 mm on 648 mm changes open pitch by -2.66 ct, within 0.2 ct.
- Clamp: an offset of +5 mm is stored as +4 mm.
- All zero: output bit-identical to v1.

## 8. Effort and dependencies

- ED 6 (roadmap 9).
- Depends on: `TuningEngine` (speaking length and the intonation slope), `part-acoustics.md` (scale), `fret-buzz.md` (not required; the check is independent).
- Used by: `05` (stopped strings), `04` (check step).

## 9. Open

- Confirm the sign convention with a player before the panel shows numbers.
