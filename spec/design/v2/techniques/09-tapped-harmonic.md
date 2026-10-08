# 09 Tapped harmonic (roadmap 3.1)

**TQ IDs:** TQ-01
**ED:** 5
**Status:** partial. Tapped harmonic is parsed in `Source/Riffs/RiffCompiler.cpp:298` (`T::tapHarmonic`) and exported in `Source/Export/MidiPerformance.cpp:43`. There is no `Technique` value and no engine voice.
**Depends on:** `spec/harmonic-realism.md` (harmonic node model, `Technique::ArtificialHarmonic` voicing), `Source/Model/Playing/PlayingEvents.h` (`Technique` enum), `Source/Model/Playing/TechniqueTriggers.h` (`TechniqueKeyswitch`), `MIDI_EXPORT_LUTHIER_PROFILE.md`.

**Summary.** A new `Technique::TappedHarmonic` drives the string at its twelfth-fret node with a fast tap attack, sounding one octave above the fretted pitch.

## 1. User-facing behaviour
- Note inspector: tapped harmonic on/off per note. Tooltip: "Play this note as a tapped harmonic at the twelfth fret above".
- Keyswitch 22 selects the technique for the following notes (as other techniques do).
- The sounded pitch is 2 x the pitch of the fretted note. Over an open string the node is set by `harmonic-realism.md`.

## 2. Engine / DSP design
- **Enum.** Append `TappedHarmonic` to `Technique` in `PlayingEvents.h`, after the last existing value (roadmap: after `Strum`). Do not reorder existing values.
- **Keyswitch.** `TechniqueKeyswitch::tapHarmonic = 22` in `TechniqueTriggers.h`, appended after `slideGesture = 21`.
- **Pitch.** Sounding frequency is `f = 2 * f_fretted`. For an open string, `f_fretted` is the open pitch.
- **Voice.** Reuse the `ArtificialHarmonic` voicing for the node (`harmonic-realism.md`): the fundamental is at half the sounding length, i.e. the string is damped at the twelfth-fret node position on the sounding segment.
- **Tap attack.** A finger-noise burst added at the note's onset (`spec/pick-noise.md` style), seeded with the technique salt. Burst length 3-8 ms. Attack is faster than a natural harmonic.
- **Excitation.** One excitation through the existing `StringEngine` path; no new engine.
- **MIDI export.** The `tapharm` technique name is written to the export profile (roadmap open question 4). `MidiPerformance.cpp:43` already maps the note; the profile needs the name added.
- **RT.** The voice is a `StringEngine` excitation computed at note start; nothing new on the audio thread beyond that. `reset()` clears tap-burst state.

## 3. Data model and parameters
- No new automatable parameters. The technique is per note.
- Keyswitch 22 (structural).

## 4. State, file format, migration
- `Technique::TappedHarmonic` appended. Riff format stores technique by name: `tapharm` is the stored name. v1 riffs contain no `tapharm`, so nothing moves.
- Riff note record: a `tapped` flag is not needed if the technique name is stored. Use the technique value.
- Migration: RiffCompiler's existing `T::tapHarmonic` token maps to the new technique on load; before this change it was dropped (no value to map to). Riffs authored with the token now sound as tapped harmonics. This is a behaviour change for those riffs and must be listed in the changelog.

## 5. Edition gating
- Free (roadmap 6).

## 6. Performance budget
- 0.02 units (one excitation, shares `StringEngine`; roadmap 7).

## 7. Test plan
- **TQ-01.** Tapped note on an open string (fretted pitch = open pitch): the dominant partial is one octave above the fretted pitch within 5 cents. The fundamental at half the sounding length.
- Tapped note over a fretted note at fret 5: sounding pitch is 2 x the fret 5 pitch within 5 cents. (The roadmap's wording says "over an open string" only; this case is added to test the formula it states.)
- Tap burst energy is inside the first 8 ms.
- Keyswitch 22 sets the technique; keyswitch off returns to the prior technique.
- Export: the MIDI export of a tapped note carries `tapharm` (round-trip).

## 8. Open decisions
- Roadmap open question 4: confirm `tapharm` in the MIDI export profile.
- Dependency: this item is blocked on `harmonic-realism.md` for the node model. If that file changes the node position, this file's formula changes with it.
