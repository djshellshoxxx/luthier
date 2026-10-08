# 08 Thumb-over bass (roadmap 3.6)

**TQ IDs:** TQ-08
**ED:** 4
**Status:** missing. `bass-techniques.md` and `SlapEngine` have thumb slap (`Excitation::Material::Thumb`, `Source/DSP/String/Excitation.h`), not thumb-over.
**Depends on:** `fingerstyle-attack.md` 14 (Thumb contact), `Excitation::Material::Thumb`, per-string muting, `thumb_palm_mute`, `thumb_position_offset`.

**Summary.** The thumb plays one fixed string while the fingers play above it. The thumb is a separate voice, rings through, and is not muted by the fingers.

## 1. User-facing behaviour
- Right-hand card: thumb string (1-6, default 4) and thumb weight (0-1, default 0.8). Tooltip: "Which string the thumb plays and how hard it hits".
- Notes on the thumb string are played by the thumb voice while thumb-over is on.
- Notes on other strings are played by the fingers, as v1.

## 2. Engine / DSP design
- **Routing.** On the message thread, when thumb-over is on, every note whose string equals `thumbover_string` is marked `voice = thumb`. Others stay `voice = finger`. Routing happens at compile time; the audio thread sees a voice index per event.
- **Excitation.** Thumb voice uses `Excitation::Material::Thumb` (`fingerstyle-attack.md` 14) with its force scaled by `thumbover_weight` relative to the finger force (weight 1 = same as a finger pluck of equal velocity; default 0.8).
- **Independence.** The thumb voice has its own string-damping state. Finger notes on other strings do not call `dampingFor` on the thumb string. Finger notes on the same string are not allowed in thumb-over mode (the thumb owns it), so the rule is enforced at routing.
- **Palm mute.** `thumb_palm_mute` (existing) applies to thumb-voice notes only.
- **Position.** `thumb_position_offset` (existing) is used unchanged for the thumb's contact point.
- **RT.** Voice index is a precomputed event field. No allocation. `reset()` clears the thumb voice state.

## 3. Data model and parameters
| ID | Range | Default | Automatable | Control |
|---|---|---|---|---|
| `thumbover_string` | 1-6 | 4 | no (structural) | Right-hand card |
| `thumbover_weight` | 0-1 | 0.8 | yes | Right-hand card |

- Thumb-over on/off is a structural switch in the right-hand card (the technique is on when the card is on). Stored in the right-hand block, appended.
- Roadmap 3.6 says 4-6 for bass. The range here is 1-6 so guitar works too. Default 4 is the D string on guitar and the E string on a 4-string bass.

## 4. State, file format, migration
- Right-hand block gets `thumbOver` (bool, default off) and the two values, appended. v1 presets: off.
- Riff notes need no change; voice is derived at compile time.

## 5. Edition gating
- Pro (roadmap 6).

## 6. Performance budget
- 0.02 units: one extra voice (roadmap 7).

## 7. Test plan
- **TQ-08.** Thumb on string 4, fingers on strings 1-3 playing a melody while the thumb plays a bass line: the thumb string keeps ringing for the note's full decay (envelope within 1 dB of a thumb-only render) while finger notes sound.
- Assert finger notes on strings 1-3 do not change the thumb string's envelope (bit-identical with and without them).
- Thumb weight 1.0 and 0.5 produce peak ratio about 2:1 (-6 dB) within 1 dB, for equal velocity.
- Thumb off: output bit-identical to v1.

## 8. Open decisions
- None beyond the roadmap's bass range wording (see section 3).
