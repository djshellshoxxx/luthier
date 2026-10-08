# 10 Hybrid picking and chicken pickin' (roadmap 3.4)

**TQ IDs:** TQ-06
**ED:** 5
**Status:** partial. `RightHand.h` has `hybridSnap` (default 0.3, `RhStyle::hybrid`). `LuthierEngineRealismB.cpp:649` has a snap. `TuneMelody` has `countryChicken`. No per-string split and no muted-snap model.
**Depends on:** `spec/fingerstyle-attack.md` (Hybrid row; line 188 snap at 0.3 x velocity), `Excitation` path, `Muting::dampingFor`.

**Summary.** Hybrid picking gets a per-string split between pick and fingers. Chicken pickin' gets a short snap whose length and damping are controllable, belonging to the same strum event.

## 1. User-facing behaviour
- Right-hand card, hybrid: `hybrid_snap` (existing) and a split control. Tooltip: "How much the fingers snap the treble strings".
- Split: strings below the split are played by the fingers, strings at or above it by the pick. Default split 4 gives fingers on 1-3 and pick on 4-6 (string 1 = high e).
- Chicken pickin': snap length and mute amount. Tooltip: "The short muted snap on the treble strings".

## 2. Engine / DSP design
- **Split.** On the message thread, each note on a string is tagged `attack = finger` or `attack = pick` from `hybrid_split`. Pick notes use the existing pick path; finger notes use the existing fingerstyle path. Routing is compile-time.
- **Snap.** The chicken snap is a short `Excitation` (existing path) of length `chicken_snap_ms` at 0.3 x the note's velocity (the `hybrid_snap` rule, `fingerstyle-attack.md` 188). It is the same event as the strum or note: no new event, no sample.
- **Mute.** After the snap, the string is damped with `Muting::dampingFor(MuteType::fretMute, ...)` with a t60 set by `chicken_mute`:
  `t60 = lerp(0.150, 0.020, chicken_mute)` seconds.
  The endpoints are the existing `palmLight` (150 ms) and `palmExtreme` (20 ms) values (`Source/Tests/MutingTests.cpp`). The lerp is the spec's chosen mapping; it is not in the roadmap.
- **RT.** Snap and mute are per-note parameters computed on the message thread; the audio thread runs the existing excitation and damping. Double precision for t60 maths. `reset()` clears the snap state.

## 3. Data model and parameters
| ID | Range | Default | Automatable | Control |
|---|---|---|---|---|
| `chicken_snap_ms` | 15-60 ms | 30 | yes | Right-hand card |
| `chicken_mute` | 0-1 | 0.7 | yes | Right-hand card |
| `hybrid_split` | 1-6 (proposed, not in roadmap 4) | 4 | no (structural) | Right-hand card |

- `hybrid_split` is not in roadmap section 4 but is required by 3.4's per-string assignment. It is proposed here, appended to the V2-TECH block, and makes the V2-TECH count 17 (11 automatable, 6 structural). Confirm.
- `hybrid_split` = 1 means all strings are pick; 6 means fingers on 1-5, pick on 6.

## 4. State, file format, migration
- `RightHand` block gains `hybridSplit`, `chickenSnapMs`, `chickenMute`, appended. Defaults reproduce the v1 hybrid sound: `hybrid_split` default 4 must not change v1 output when hybrid is off. Implementation: the split is applied only when `RhStyle::hybrid` is active, so v1 presets (which are not hybrid-with-split) render unchanged.
- v1 presets with `RhStyle::hybrid` will change if their split default applies. Migration: presets stored before v2 load `hybrid_split` = 1 (all pick, v1 behaviour). New presets default to 4. Document in the migration notes.

## 5. Edition gating
- Roadmap 6: "Hybrid and chicken snap (existing `hybrid_snap`)" is Free. This spec applies Free to the new split and chicken parameters too, since they sit in the same row. Confirm.

## 6. Performance budget
- 0.02 units (short excitation, roadmap 7). Split is free.

## 7. Test plan
- **TQ-06.** Snap length 30 ms: the excitation's energy lies within 25-35 ms (assert at least 90 % inside that window). The string is damped by `chicken_mute` 0.7 such that the envelope falls by 9 dB or more within 10 ms after the snap ends. (The roadmap wording "damped within 10 ms" is under-specified; this is the measurable form.) At t60 = 59 ms, 10 ms gives about 10 dB.
- Split 4: strings 1-3 use the finger path, strings 4-6 the pick path (assert the excitation material per string).
- `chicken_mute` 0: t60 = 150 ms (free ring after snap). `chicken_mute` 1: t60 = 20 ms.
- Hybrid off: bit-identical to v1.

## 8. Open decisions
- The `chicken_mute` t60 lerp endpoints (section 2) are the spec's choice.
- Free/Pro for split and chicken parameters (section 5).
