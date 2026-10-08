# 01 Seven- and eight-string guitar (roadmap 3.5)

Covers: **IN-07** (IN-12, IN-13, IN-14 shared; see INDEX.md).

## Summary

Seven- and eight-string electrics exist as presets with single-scale necks.
v2 adds multi-scale (fanned) necks, which are owned by `extended-range-bass.md`
1.1. This file covers only the delta on the guitar side and a correction to the
roadmap's tuning parameters.

## Status

**exists** (single scale). Multi-scale: spec only, out of this roadmap.

Evidence:
- `Source/Model/Playing/TuningEngine.cpp` 61-62: `SevenString` = B1 E2 A2 D3 G3 B3 E4
  (61.735, 82.407, 110.000, 146.832, 195.998, 246.942, 329.628 Hz). `EightString`
  adds 46.249 Hz (F#1).
- `TuningEngine.cpp` 527-528: preset names "7-String B", "8-String F#".
- `Source/Model/Guitar/GuitarLibrary.cpp` 143: "7-String, extended range with a low B,
  26.5 in scale".
- Parts: `Electric/7-String Modern`, `Electric/8-String Modern`,
  `Parts/Strings/Seven-String NPS`, `Parts/Strings/Eight-String NPS`.

## User-facing behaviour

- The tuning selector already offers 7-String B and 8-String F#. Choosing them
  sets the low string (B1 or F#1) with no extra control.
- Until multi-scale ships, the tooltip says: "Single scale: fanned frets arrive
  with multi-scale necks."
- With multi-scale on (later, Pro), the low string gets a longer scale and the fret
  positions change. That is data, not DSP (see 3).

## Roadmap correction: the tuning parameters duplicate presets

Roadmap 4 defines `seven_low_b` (default off) and `eight_low_fsharp` (default
off) as "adds a low B / low F# below the low E". The existing presets already do
this. With the flags off, the 7-string has no low B, which contradicts the
existing "7-String B" preset and `GuitarLibrary` ("extended range with a low B").

**Decision in this spec (D):** do not append `seven_low_b` or `eight_low_fsharp`.
The "Low B" / "Low F#" control is the existing tuning preset selector, and the
tooltip in roadmap 2 binds to it. The new-ID count becomes eight, not ten.
Owner to confirm (INDEX open item).

## Engine and model design

- No new DSP. The string set is the existing `Seven-String NPS` / `Eight-String NPS`
  with `TuningEngine` presets.
- Multi-scale (for reference; the owner is `extended-range-bass.md` 1.1): fret n on
  string s sits at `d(n) = L_s * (1 - 2^(-n/12))` from the nut, where `L_s` is that
  string's scale. The v2 change is only that `L_s` varies per string. The formula
  is recorded here so IN-07 can be checked against it; it is not implemented here.
- RT-safety: nothing new on the audio thread.
- Doubles only; no state beyond the existing tuning.

## Data model and parameters

- Parameters: none added (see the correction above).
- Multi-scale fields: owned by `extended-range-bass.md` 1.1 (external dependency).

## State, file format, migration

- Tuning is already a preset and is saved in the `.luthierguitar`. No format change.
- Presets saved with `seven_low_b`/`eight_low_fsharp` (none exist, since they are not
  shipped) need no migration. The decision above removes the need.
- Single-scale presets load unchanged; multi-scale fields default to "single scale".

## Edition

| Item | Free | Pro |
|---|---|---|
| 7 and 8-string, single scale | Free (as today) | Free |
| Multi-scale | Pro | Pro |

Keep today's Free status for the existing 7/8-string guitars. Verify against
`Source/Edition.h` before shipping.

## Performance budget

0 units. Multi-scale changes positions only (roadmap 7).

## Test plan

- **IN-07a (presets).** Selecting 7-String B yields the lowest string at
  61.735 Hz (MIDI 35, B1); 8-String F# yields 46.249 Hz (MIDI 30, F#1). Each within
  5 cents of the equal-tempered target.
- **IN-07b (single-scale default).** With multi-scale off, every string's scale equals
  the preset's single scale (26.5 in for 7-String), bit-equal to v1.
- **IN-07c (tooltip).** The single-scale tooltip shows while multi-scale is absent.
- **IN-07d (no duplicate IDs).** The V2-INST block contains no `seven_low_b` or
  `eight_low_fsharp` (guards the decision above).
- Shared: IN-12 (existing presets bit-identical), IN-13, IN-14.

## Effort and dependencies

- **ED 2:** preset wiring, tooltip, the IN-07 test set.
- Dependencies: `extended-range-bass.md` 1.1 for multi-scale. **Out of this
  roadmap and not counted here** (roadmap 9 counts it as 0).
- `TuningEngine.cpp`, `GuitarLibrary.cpp`, `Parts/Strings/Seven-String NPS`.
