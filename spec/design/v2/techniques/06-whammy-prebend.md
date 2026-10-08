# 06 Whammy bar pre-bend (roadmap 3.2)

**TQ IDs:** TQ-02, TQ-03
**ED:** 4
**Status:** partial. `WhammyEngine` (`Source/DSP/Whammy/WhammyEngine.h`) has dive, flutter and a 5 ms smoother. String pre-bend exists in `Source/DSP/Techniques/BendEngine.h` (`preBendCents`, keyswitch 20). No bar path.
**Depends on:** `WhammyEngine`, `BendEngine` (pattern for keyswitch and CC triggers), `TechniqueTriggers.h`.

**Summary.** The bar is raised `bar_prebend_depth` semitones before the pick, over `bar_prebend_time` ms, held until the pick, then released by the existing spring behaviour.

## 1. User-facing behaviour
- Whammy card: depth (0-4 st) and time (10-300 ms). Tooltip: "How far the bar is pulled before the pick, and what triggers it".
- Trigger: keyswitch 23, or the CC the whammy card assigns (same pattern as `BendEngine`).
- The bar rises, holds, and the note is struck. After the note, `whammy_springs` decides how far the bar returns.

## 2. Engine / DSP design
- **Pre-bend state.** `WhammyEngine` gets a pre-bend target `T = min(bar_prebend_depth, whammy_up_range)` semitones (the pre-bend never exceeds the up range).
- **Shape.** Raised-cosine (smoothstep) from 0 to `T` over `Tp = bar_prebend_time` ms, starting at the trigger time:
  `s(u) = 0.5 * (1 - cos(pi * clamp(u, 0, 1)))`, with `u = (t - t_trigger) / Tp`.
  The bar offset is `T * s(u)`. This reaches `T` exactly at `Tp` and has zero slope at both ends, so no click.
- **Hold.** After `Tp`, the offset holds at `T` until the note's strike time.
- **Strike and release.** The strike is at the note onset. If the keyswitch arrived less than `Tp` before the onset, the strike is not delayed: the pre-bend is partial at the strike and continues. This keeps the live path latency-free. The release at the note's end uses the existing spring return.
- **Pitch.** The offset is converted through the existing whammy pitch mapping so the range limits in `whammy_up_range` and `whammy_down_range` still apply. No pitch can leave the whammy range (TQ-03).
- **RT.** `s(u)` is evaluated per block in double precision on the audio thread from a trigger timestamp; no allocation. `reset()` clears the pre-bend state and returns the offset to 0.
- Pre-bend is additive with the existing flutter and dive state, and is applied before the 5 ms smoother so the smoother still removes zipper noise.

## 3. Data model and parameters
| ID | Range | Default | Automatable | Control |
|---|---|---|---|---|
| `bar_prebend_depth` | 0-4 st | 1 | yes | Whammy card |
| `bar_prebend_time` | 10-300 ms | 80 | yes | Whammy card |

- Keyswitch 23 is appended to `TechniqueKeyswitch` (after `slideGesture` = 21 and tapped harmonic 22).

## 4. State, file format, migration
- Riff notes that use pre-bend carry a `barPrebend` flag, appended, default off. Preset `whammy` block gains the two values, appended. v1 presets have depth 0 effective (pre-bend off).

## 5. Edition gating
- Free (roadmap 6).

## 6. Performance budget
- 0.01 units (one smoother's worth of state).

## 7. Test plan
- **TQ-02.** Depth 1 st, time 80 ms: at `t = 80 ms` the bar offset is within 5 cents of 100 cents. At `t = 75 ms` it is within 10 cents of 100 cents (smoothstep is near target before its end). The offset is 0 at `t = 0` and stays at target until the note's end, then returns.
- **TQ-03.** Depth 4 st with whammy up range of 2 st: the offset is clamped to 2 st. Across the whole render the pitch stays inside the whammy range.
- Keyswitch arriving 20 ms before onset with time 80 ms: strike not delayed; the bar offset at the strike is about 25 % of target. Assert the strike time equals the onset within 0.1 ms.
- Depth 0: output bit-identical to v1.

## 8. Open decisions
- Direction: the spec assumes pre-bend raises pitch (bar up). Roadmap wording says "pulled"; confirm the sign convention matches `WhammyEngine`'s up range.
