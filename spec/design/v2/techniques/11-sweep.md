# 11 Sweep picking (roadmap 3.5)

**TQ IDs:** TQ-07
**ED:** 6
**Status:** missing. No `sweep` gesture in `Source/Model/Playing` or `Source/DSP`.
**Depends on:** `Source/Model/Playing/ChordVoicer.cpp` (chord shape), `StrumGesture` / rhythm scheduler (`spec/strum-dynamics.md`, `spec/rhythm-engine.md`), `Muting::dampingFor`, `TechniqueTriggers.h`.

**Summary.** A `SweepGesture` plays a chord shape as one pick stroke across its strings, one onset per string, at `sweep_speed_ms` apart, up or down.

## 1. User-facing behaviour
- Technique card: sweep on, direction (up/down), speed (3-40 ms per string). Tooltip: "Sweep the chord shape in one stroke".
- A sweep takes the current chord voicing as its shape. The order of strings is the sweep order.
- Sweeps are not taps: they sound like a pick across.

## 2. Engine / DSP design
- **Shape.** The chord voicer (`ChordVoicer.cpp`) supplies an ordered list of (string, fret). The sweep order is string order: ascending string number for down (low-to-high pitch order is not used; the sweep follows string index), descending for up. Direction is `sweep_direction`.
- **Minimum.** Four or more strings (roadmap open question 2, recommended yes). Smaller shapes fall back to an arpeggio using the same onsets (no damping rule).
- **Onsets.** String `i` in sweep order has onset `t0 + i * sweep_speed_ms / 1000` (double). Assert within 1 ms (TQ-07).
- **Excitation.** Each string uses the pick excitation (existing path) with velocity from the note. No new excitation.
- **Damping.** When string `i+1` sounds, string `i` is handed to `Muting::dampingFor(MuteType::fretMute, ...)` (light). The roadmap states the previous string is damped as the pick moves on. A real sweep often lets the chord ring; this light damping is the roadmap's rule and is kept. Confirm.
- **Trigger.** The roadmap does not allocate a keyswitch for sweep. This spec proposes keyswitch **24** (sweep, with direction from `sweep_direction`), appended after 23. Confirm. Also proposes `Technique::Sweep`, appended after `TappedHarmonic` (file 09).
- **Scheduling.** Onsets are precomputed on the message thread (`RhythmEngine` / strum scheduler). The audio thread reads the onset list. Fixed maximum of 6 strings: no allocation.
- **RT.** `reset()` clears pending onsets and damping targets.

## 3. Data model and parameters
| ID | Range | Default | Automatable | Control |
|---|---|---|---|---|
| `sweep_speed_ms` | 3-40 ms | 12 | yes | Technique card |
| `sweep_direction` | choice: up (0), down (1) | up | no (structural) | Technique card |

- Sweep on/off is by keyswitch 24, not a parameter.

## 4. State, file format, migration
- The sweep is a chord voicing plus string order, saved with the riff (roadmap 5). No new file format.
- Riff note records use `Technique::Sweep` for the sweep events. v1 riffs have none.
- Presets gain `techniques.sweep` only when used.

## 5. Edition gating
- Pro (roadmap 6).

## 6. Performance budget
- 0.03 units per sweep, scheduler only (roadmap 7).

## 7. Test plan
- **TQ-07.** Three-string sweep at 12 ms per string: onsets within 1 ms of 0, 12, 24 ms. Each earlier string is damped as the next sounds (the earlier string's envelope falls by 9 dB or more within 20 ms of the next onset; assert by measuring).
- Four-string sweep up and down: order reversed between directions.
- Shape with three strings falls back to an arpeggio with the same onsets and no damping.
- Sweep speed 3 ms and 40 ms bounds accepted.

## 8. Open decisions
- Keyswitch 24 and `Technique::Sweep` are proposals (section 2).
- Damping on previous strings (section 2): confirm the roadmap's rule.
- Four-string minimum (roadmap open question 2): confirm.
