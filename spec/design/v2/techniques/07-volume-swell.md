# 07 Volume swell (roadmap 3.3)

**TQ IDs:** TQ-04, TQ-05
**ED:** 4
**Status:** missing. `VolumePedal` exists as an effect; no per-note swell gesture (no hits in `Source/Model/Playing` or `Source/DSP/Techniques`).
**Depends on:** `Source/DSP/String/Excitation.h` (`Material` enum), `StringEngine`, the amp input stage, `TechniqueTriggers.h`.

**Summary.** A `SwellGesture` per note fades the note in from silence over `vswell_rise` ms, using a closed-form curve. Chords swell together.

## 1. User-facing behaviour
- Technique card: Swell on/off, rise time (40-2000 ms), hold on/off. Tooltip: "Fade the note in from silence before the pick, as a volume swell".
- Swell is applied per note; chords share one gesture.

## 2. Engine / DSP design
- **Curve.** Normalised exponential ease, closed form, evaluated in double:
  `u = clamp(t / rise, 0, 1)`, `g(u) = (1 - exp(-5u)) / (1 - exp(-5))`. `g(0) = 0`, `g(1) = 1`. No table, no allocation.
- **Placement.** Applied to the note's output gain before the amp, as the roadmap states. The amp therefore responds to the swell as it does to the volume knob.
- **Source model (decision D-1, see section 8).** The swell needs sound before the pick, which the string model does not give for free. Recommended model: a `Material::Swell` excitation, a seeded band-limited noise drive whose amplitude is `g(u)`. It excites the string during the rise, so the string sounds from silence. At the rise's end the drive is handed off to the note's normal excitation.
- **Pick attack.** If a pick follows, the pick transient is not gated. The swell drive crossfades to zero over 20 ms starting at the pick time, so the swell's energy does not stack on the pick's attack.
- **Hold.** `vswell_hold` off: the note is swelled then picked (drive crossfades out at the pick). On: the drive holds the note at full until note-off and no pick transient is added; the note has no separate attack.
- **Chords.** The gesture is computed once per chord event and drives every string's gain with the same `g(u)`. No per-string phase shift.
- **RT.** Per block, the audio thread evaluates `g` for the active notes (at most 8 notes; budget below). State is the rise start time and a seed. `reset()` returns all swell state to 0.

## 3. Data model and parameters
| ID | Range | Default | Automatable | Control |
|---|---|---|---|---|
| `vswell_rise` | 40-2000 ms | 400 | yes | Technique card |
| `vswell_hold` | bool | off | no (structural) | Technique card |

- Per-note flag `swell` (bool, default off) is stored on the note record. Swell on/off for a note is structural and not automatable.

## 4. State, file format, migration
- Note records gain `swell` (default off) and the preset's technique block gains `vswell_rise` and `vswell_hold` only when a swell is used (roadmap 5). v1 notes are off.

## 5. Edition gating
- Free (roadmap 6).

## 6. Performance budget
- 0.01 units per swelling note; eight at once is 0.08 (roadmap 7). The noise drive is one seeded generator per voice, not per sample allocation.

## 7. Test plan
- **TQ-04.** Rise 400 ms: at 200 ms the gain is within 0.5 dB of `g(0.5)` (about -0.7 dB); at 400 ms the gain is 1 within 0.01 dB.
- **TQ-05.** `vswell_hold` off, picked note: the attack transient (first 20 ms after the pick) is within 1 dB of the no-swell render. Assert the peak in the first 20 ms.
- Chord: all notes' gain envelopes are identical (max difference below 1e-9).
- Swell off: bit-identical to v1.
- Seed: two renders with the same seed are identical (TQ-14).

## 8. Open decisions
- **D-1, source model.** The roadmap says the swell is "applied to output gain" and that swells do not apply to pick-attacked notes unless hold is on. Taken literally, that makes the default swell a no-op on picked notes, which contradicts user story U1. This spec uses the noise-driven source above. Confirm before build; if rejected, the alternative is a pure output fade from onset with the pick attack gated, which fails TQ-05 by design.
- Open question 1 in the roadmap (velocity to swell depth): independent in v2.0, as the roadmap recommends.
