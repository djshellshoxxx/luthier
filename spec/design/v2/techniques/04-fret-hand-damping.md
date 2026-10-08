# 04 Fret-hand damping between phrases (roadmap 3.9)

**TQ IDs:** TQ-11
**ED:** 3
**Status:** partial. `MuteEngine` (`Source/DSP/Techniques/MuteEngine.h`) damps the string under the pick and the chuck. `Muting::dampingFor` exists (`spec/muting-rhythm.md`; `Source/Tests/MutingTests.cpp`). No release between phrases.
**Depends on:** `string-interaction.md` 3, `muting-rhythm.md`, `Muting::dampingFor`.

**Summary.** A release mode lets strings the fret hand is not holding stop ringing between phrases, so a phrase boundary sounds like the hand letting go.

## 1. User-facing behaviour
- Mute card: release mode, Off / Fret hand / All unplayed. Tooltip: "Stop the strings the fret hand is not holding between phrases".
- Off: strings ring on as v1.
- Fret hand: at a phrase boundary, strings not in the next chord are damped.
- All unplayed: at a phrase boundary, every string not sounding at the boundary is damped.

## 2. Engine / DSP design
- **Phrase boundary.** A gap between the last note-off and the next onset that is longer than `ftdamp_gap_ms` (default 250 ms). Detected on the message thread when events are compiled. This helper is `PhraseBoundaries` and is shared with 05 (mistakes exempts the first note of a phrase).
- **Engagement.** Damping is scheduled at the detected boundary instant: last note-off + `ftdamp_gap_ms`. From there, each target string is handed to `Muting::dampingFor(MuteType::fretMute, ...)` and the existing `MuteEngine` damping path runs. No new damping algorithm.
- **Target strings.**
  - Fret hand (mode 1): strings that are ringing at the boundary and are not in the next chord.
  - All unplayed (mode 2): every string ringing at the boundary. Note: the roadmap says "all unplayed strings" without defining it. This spec chooses "ringing at the boundary". Confirm.
- Damping runs on the audio thread through the existing voice state. Nothing is allocated; the target list is a fixed array of 6 indices built on the message thread.
- Double precision for all gap arithmetic. Times are in seconds.

## 3. Data model and parameters
| ID | Range | Default | Automatable | Control |
|---|---|---|---|---|
| `ftdamp_mode` | choice: off (0), fret hand (1), all unplayed (2) | off | no (structural) | Mute card |
| `ftdamp_gap_ms` | 50-1000 ms | 250 | yes | Mute card |

## 4. State, file format, migration
- Mode is stored in the preset's mute block, appended. Absent means off.
- Riffs do not store phrase boundaries; they are derived from the note stream on load.

## 5. Edition gating
- Pro. Free presets with a non-off mode load with off and a notice.

## 6. Performance budget
- Under 0.01 units. Scheduling plus existing damping.

## 7. Test plan
- **TQ-11.** Mode 1, gap 300 ms, two unplayed strings: both are damped. Assert that each string's envelope is 40 dB or more below its level at engagement within 50 ms of engagement. The engagement instant is last note-off + 250 ms, so the 300 ms gap is still a boundary.
- Mode 0, same riff: both strings keep ringing (envelope within 1 dB of the free-decay curve at 300 ms).
- A gap of 200 ms (below the threshold) does not damp.
- A string in the next chord is not damped in mode 1.

## 8. Open decisions
- The roadmap's TQ-11 says "silences both within 50 ms" without a start reference. This spec uses engagement as the reference and a 40 dB drop. Confirm the threshold.
- Mode 2 target definition (see section 2).
