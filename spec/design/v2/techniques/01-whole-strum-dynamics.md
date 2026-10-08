# 01 Whole-strum dynamics (roadmap 3.10)

**TQ IDs:** none of its own (regression via TQ-09 and TQ-13)
**ED:** 0 (incremental work is counted under 03 arm/wrist, 3 ED)
**Status:** partial. Crossing velocity, acceleration and per-string force exist (`Source/Rhythm/StrumGesture.h`; `spec/strum-dynamics.md` 1-3).
**Depends on:** `strum-dynamics.md`, `rhythm-engine.md`.

**Summary.** No new behaviour in this item. The only missing piece, a stroke source (arm, wrist, mixed), is specified in 03. This file exists so the 3.10 entry has one owner and the v1 baseline is pinned.

## 1. User-facing behaviour
- Unchanged. A strum with no stroke-source override sounds exactly as v1.
- The stroke-source control is shown on the Strum card; its behaviour is in 03.

## 2. Engine / DSP design
- The `mixed` stroke source is defined as the current v1 acceleration profile (`strum-dynamics.md` 2) and timing spread, with no change in code path. This is the baseline that TQ-13 compares against.
- No change to crossing velocity (`strum-dynamics.md` 1) or per-string force (3).

## 3. Data model and parameters
- None new here. `strum_stroke_source` is added in 03.

## 4. State, file format, migration
- None. Riffs and presets are untouched.

## 5. Edition gating
- Existing engine behaviour: Free. Gating applies only to the arm and wrist overrides (03, Pro).

## 6. Performance budget
- 0 new cost (existing path).

## 7. Test plan
- Covered by TQ-13 (v1 riff and preset render bit-identically with all new parameters at defaults). The strum fixture used by TQ-13 must include at least one strum with a crossing spread, so the baseline is not trivially silent.
- Covered by TQ-09 in 03, which asserts that arm and wrist differ from mixed.

## 8. Open decisions
- None.
