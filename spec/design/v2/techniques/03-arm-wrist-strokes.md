# 03 Arm versus wrist strokes (roadmap 3.7)

**TQ IDs:** TQ-09
**ED:** 3 (includes the 3.10 gap, which is the same work)
**Status:** missing. No arm or wrist hit in `Source/` or `spec/`.
**Depends on:** `Source/Rhythm/StrumGesture.h`, `strum-dynamics.md` 2 (acceleration) and 3 (per-string force), `rhythm-engine.md`.

**Summary.** `StrumGesture` gets a stroke source. Arm strokes are broad and slow with more string spread. Wrist strokes are small and quick with tighter timing. The per-string force model is kept.

## 1. User-facing behaviour
- Strum card: stroke source selector, Mixed / Arm / Wrist. Tooltip: "Arm strokes are broad and slow; wrist strokes are small and quick".
- Mixed (default) sounds exactly as v1.
- Arm: longer down-stroke, larger crossing spread, looser timing.
- Wrist: short down-stroke, tighter timing, less string-to-string variation.

## 2. Engine / DSP design
- Add `enum class StrokeSource { mixed, arm, wrist }` to `StrumGesture`, read from `strum_stroke_source` when the gesture is built (message thread).
- **Mixed = v1 path.** The v1 acceleration profile and timing spread are used unchanged. This is the compatibility guarantee for TQ-13.
- **Arm profile.** Down-stroke duration `D_arm` = 60-120 ms (default 90 ms). Acceleration profile (`strum-dynamics.md` 2) uses a longer, flatter ramp so crossing velocity is higher at the top of the stroke. Timing spread (onset jitter across strings) is the v1 spread times 1.5.
- **Wrist profile.** `D_wrist` = 25-50 ms (default 35 ms). Acceleration profile uses a short, steep ramp. Timing spread is the v1 spread times 0.4.
- Per-string force (`strum-dynamics.md` 3) is unchanged: the stroke source only changes the acceleration profile and the timing spread, as the roadmap states.
- Duration is in double precision; the gesture is precomputed into per-string onset and velocity arrays on the message thread. The audio thread reads them; no allocation.
- Mixed inside a bar (roadmap leaves it open): the gesture draws, per down-stroke, arm or wrist with a seeded salt, p(arm) = 0.3. This is a constant chosen here; it is not a parameter in v2.0.

## 3. Data model and parameters
| ID | Range | Default | Automatable | Control |
|---|---|---|---|---|
| `strum_stroke_source` | choice: mixed (0), arm (1), wrist (2) | mixed | no (structural) | Strum card |

## 4. State, file format, migration
- Strum records gain an optional `strokeSource` field, default mixed. v1 strums load as mixed.
- The seeded draw for mixed uses salt `0x5A17` (table in `INDEX.md`, cross-cutting).

## 5. Edition gating
- Arm and wrist: Pro. Mixed is the v1 path and stays available on Free, so a Free preset loads with mixed and a notice if it stored arm or wrist.

## 6. Performance budget
- Under 0.01 units. Gesture precompute only.

## 7. Test plan
- **TQ-09.** Arm down-stroke duration 60-120 ms (assert the measured stroke length lies in range); wrist 25-50 ms; wrist string spread (max minus min onset across the strum) is at most half the arm spread for the same strum.
- Mixed equals v1 bit-identically (TQ-13 fixture).
- Per-string force is identical between arm and wrist for the same velocity input (assert the force array, not the timing).
- Seed: mixed draws are reproducible across two renders with the same seed (TQ-14).

## 8. Open decisions
- p(arm) = 0.3 in mixed is a chosen constant. Confirm with the rhythm owner before 2.0.
