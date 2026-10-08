# 02 Push and pull (roadmap 3.8)

**TQ IDs:** TQ-10
**ED:** 2
**Status:** missing. Timing jitter exists (`humanize.timing_ms`, `rhythm-engine.md` 135); there is no constant bias.
**Depends on:** `rhythm-engine.md` (onset scheduling, humanize), per-part rhythm track.

**Summary.** A per-part constant timing bias, `push_pull_ms`, moves every onset of that part earlier (push) or later (pull), before the random jitter is added.

## 1. User-facing behaviour
- Rhythm card: a bipolar slider, -30 to +30 ms, per part. Tooltip: "Ahead of the beat (push) or behind it (pull)".
- Positive values push (onset moves earlier). Negative values pull (onset moves later).
- Other parts are not affected.

## 2. Engine / DSP design
- Onsets are computed on the message thread when a riff or rhythm part is compiled (`RhythmEngine`). The bias is added there, in double-precision seconds. The audio thread only reads precomputed event times.
- Order of application for each onset of a part:
  1. grid time, with swing applied (`rhythm-engine.md` swing);
  2. add `push_pull_ms / 1000` (sign: positive is earlier, so subtract for the time offset);
  3. add the seeded timing jitter (`humanize.timing_ms`), unchanged;
  4. clamp so no onset in a part is earlier than the previous onset of the same part, plus 1 sample. This prevents reordering at high push values.
- A strum's start is the onset that is shifted. The internal string-to-string spread of the strum (`strum-dynamics.md`) is not shifted.
- At 0 jitter the offset is exact: no rounding beyond the sample grid used for event times.

## 3. Data model and parameters
| ID | Range | Default | Automatable | Control |
|---|---|---|---|---|
| `push_pull_ms` | -30 to +30 ms | 0 | yes | Rhythm card, per part |

- Stored on the rhythm part, not globally. The global parameter list in `Parameters.h` gets only the V2-TECH entry; the per-part value lives in the part's data.

## 4. State, file format, migration
- Riff part record gets `pushPullMs` with default 0, appended. v1 parts load with 0.
- No change to the MIDI export timing other than the shifted onsets (export uses the same compiled times).

## 5. Edition gating
- Pro (roadmap 6). A Free preset with a non-zero value loads at 0 with a notice.

## 6. Performance budget
- Under 0.01 units. Scheduling only.

## 7. Test plan
- **TQ-10.** Push +10 ms: every onset of the part is 10 ms earlier, within 0.1 ms, with `humanize.timing_ms` at 0. Pull -10 ms: every onset 10 ms later. Assert both directions.
- Per-part isolation: a second part at 0 keeps its onsets bit-identical.
- Clamp: push +30 ms on a 16th-note part at 240 BPM produces no onset earlier than its predecessor.
- Compat: all parts at 0 render identically to v1 (shared with TQ-13).

## 8. Open decisions
- None.
