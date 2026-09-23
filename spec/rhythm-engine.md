# RHYTHM ENGINE SPEC: strum, fingerpick, chord voicer

Extends spec.md sections "Playing modes" and "Additional features", and
engine.md's `ChordVoicer` and `MidiInterpreter`. Depends on the existing string
engine and does not require any new DSP primitives; this document is entirely
about how MIDI turns into per-string events over time.

## 0. Ground rules the Rhythm Engine must follow

1. The engine is a **MIDI transformer**, not a DSP module. It sits between the
   MidiInterpreter and the TechniqueEngine and rewrites the event stream.
2. It **never allocates in the audio callback**. All pattern buffers are
   pre-sized to `maxPatternSteps * maxStringCount * sizeof(StrumEvent)`.
3. It is **sample-accurate to the host transport**. On every block boundary,
   query `AudioPlayHead::getPosition()`, convert bars/beats/ppq to samples, and
   schedule events to the exact sample.
4. It is **silent when the host is not playing** unless "Free-run" is engaged.
   Freewheel behaviour must be an explicit user choice, not a default.
5. Every pattern is **quantized on read, humanized on write**. Store patterns
   as exact grid positions and pull humanization values from the existing
   `HumanizeMatrix` at schedule time.
6. Rhythm engine has **its own bypass**. Bypassing must revert to raw MIDI
   routing within one block, without artefacts.

## 1. Module layout

```
[MidiInterpreter]
      |
      v
[RhythmEngine]  ---- pattern library, chord voicer, humanize source
      |
      v
[TechniqueEngine]
```

Sub-modules:

- `ChordDetector` — infers a chord symbol from a stack of held notes.
- `ChordVoicer` — takes a chord symbol and produces a per-string fret map.
- `StrumScheduler` — turns a voicing plus a strum pattern into timed pluck
  events.
- `FingerpickScheduler` — turns a voicing plus a fingerpick pattern into
  per-finger, per-string pluck events.
- `PatternLibrary` — the loaded strum and fingerpick patterns.
- `GenreKit` — a bundle of default patterns, chord preferences and technique
  settings named after a style.

## 2. ChordDetector

**Input**: the current set of held MIDI notes on the incoming buffer.
**Output**: `ChordSymbol { root, quality, bass, extensions[], confidence }`.

Detection order:
1. Sort held notes into pitch classes.
2. Match against a lookup of 84 templates: maj, min, 7, maj7, m7, dim, dim7,
   m7b5, aug, sus2, sus4, 9, m9, 11, 13, add9, m6, 6, 6/9, and slash-chord
   inversions.
3. Bass note is the lowest sounding pitch, used as slash if it is not the
   root.
4. Extensions above the 7th are captured but only voiced if `voicing_density`
   allows.
5. Confidence is `matched / heldCount`; below 0.6, the chord is reported as
   `Unknown` and StrumScheduler falls through to literal per-string mapping.

Chord detector runs once per NoteOn burst, not per sample. A "burst" is any
group of NoteOns landing within a 30 ms window.

## 3. ChordVoicer

**Input**: `ChordSymbol`, guitar spec (string count, tuning, fret count,
capo, scale length), `voicing_style`, `voicing_density`, `hand_position_hint`.

**Output**: `Voicing { fretPerString[N], mutedStringMask, barreFret }`.

Constraints, in order (a voicing is rejected if any fail):
1. Every fret is within `[0, maxFret]` for that string.
2. Non-muted strings together span no more than `handSpanFrets` (default 5,
   user 3-7).
3. Root or bass note must be sounding on the lowest non-muted string.
4. No two adjacent non-muted strings may skip more than 4 semitones downward
   in pitch (avoids voice-crossing that reads wrong).
5. If a barre is required, at least 3 strings must sit on the barre fret.

Score for ranking valid voicings, higher is better:
```
score = 0
score += open_strings_used * open_bonus            // default open_bonus = 3
score -= sum(abs(fret - hand_position_hint))       // proximity to hint
score -= barre ? barre_penalty : 0                 // default 2
score -= muted_strings_used * mute_penalty         // default 4
score += style_bias[voicing_style]                 // see below
```

`voicing_style` values and their bias:
- `Open` — prefer voicings with open strings, root on string 5 or 6.
- `Barre` — prefer barre voicings around `hand_position_hint`.
- `Triad` — prefer three-note voicings on top three strings.
- `Shell` — root + 3rd + 7th only, jazz shell voicings.
- `Drop2`, `Drop3` — standard jazz drop voicings across the middle strings.
- `Power` — root + 5th (+ octave root) only.
- `Rootless` — omit root, useful over a bassist.
- `Wide` — spread voicing across all strings, one note per string.

`voicing_density` is 0-100. It caps how many notes of the incoming stack are
voiced. 100 uses all held notes plus doublings; 40 shell-voices; 20 power-chord.

`hand_position_hint` is updated by StrumScheduler from the previous chord's
average fret, so successive chords cluster on the neck instead of jumping.

## 4. StrumScheduler

**Input**: `Voicing`, `StrumEvent { direction, duration_ms, dynamic, string_mask }`.

A single strum expands to N per-string PluckEvents, spaced by
`duration_ms / (activeStringCount - 1)`, in the direction requested:
- `Down` — from lowest string index to highest, low-to-high pitch.
- `Up` — reverse.
- `DownMute`, `UpMute` — same order, palm-mute technique flag set.
- `Rake` — 3-5 muted strings before the target, used for country/blues rake.
- `Rasgueado` — flamenco: 4 sequential up-strums by index (i, m, a, e), each
  10-20 ms apart.

Per-string dynamic falls off across the strum: leading strings 100% velocity,
trailing strings 85-95% depending on `strum_evenness` (0-100).

`string_mask` is a bitmask that mutes strings inside the voicing for this
strum: allows patterns like "beat 1 all six strings, beat 3 top three only".

Strum patterns are 16-step by default, extendable to 32. Each step holds a
`StrumEvent` or a rest. Steps are quantized to 16th notes at the current host
tempo; triplet and dotted subdivisions selectable per pattern.

Humanization:
- Timing: pull from `humanize.timing_ms`, sample per event, gaussian sigma.
- Velocity: pull from `humanize.velocity_pct`.
- Miss probability: `humanize.miss_pct` chance that a scheduled event drops.
- Ghost probability: `humanize.ghost_pct` chance an extra muted stroke is
  inserted on the offbeat before a scheduled hit.

## 5. FingerpickScheduler

**Input**: `Voicing`, `FingerpickPattern { steps[], finger_assignment[N] }`.

`finger_assignment` maps each string to one of `{ p, i, m, a, e }` (thumb,
index, middle, ring, little). Each finger has its own excitation profile
pulled from the existing right-hand model.

`steps[]` is a list of `{ finger, subdivision, dynamic }` entries. On each
matching subdivision, the assigned finger's string is plucked.

Ship these factory patterns at minimum:
- Travis picking, alternating bass
- Classical p-i-m-a arpeggio ascending
- Classical p-i-m-a-m-i arpeggio return
- Boom-chick country (thumb-brush-thumb-brush)
- Piedmont blues alternating bass with syncopated top
- Modern folk quarter-note thumb, sixteenth-note top
- Fingerstyle folk pattern (5-3-4-2-3-4)

Patterns are user-editable in the pattern editor (see UI section).

## 6. PatternLibrary and file format

Patterns live under `Resources/Rhythm/` for factory and
`~/Documents/Luthier/Rhythm/` for user, in `.luthierpattern` JSON files:

```json
{
  "name": "Country Boom Chick",
  "type": "strum",
  "length_steps": 16,
  "subdivision": "16",
  "swing": 0.55,
  "steps": [
    { "step": 0,  "event": "Down",     "dynamic": 1.00, "mask": "111111" },
    { "step": 4,  "event": "UpMute",   "dynamic": 0.70, "mask": "000111" },
    { "step": 6,  "event": "Down",     "dynamic": 0.85, "mask": "000111" },
    { "step": 8,  "event": "DownMute", "dynamic": 0.75, "mask": "111000" },
    { "step": 12, "event": "Up",       "dynamic": 0.80, "mask": "000111" }
  ],
  "tags": ["country", "acoustic", "medium"]
}
```

`mask` is a bitstring, one bit per string, low-index first. Fingerpick
patterns use `event: "Pluck", "finger": "p|i|m|a|e"` instead of `Down/Up`.

## 7. GenreKit

A GenreKit bundles:
- Default `voicing_style` and `voicing_density`.
- A curated list of strum and fingerpick patterns.
- Humanization defaults.
- Preferred amp/effect preset (soft reference, user can override).

Ship at least these kits:
- Nashville Country, Modern Country, Bluegrass Flatpick
- Delta Blues, Chicago Blues, Modern Blues Rhythm
- Folk Fingerstyle, Contemporary Fingerstyle, Piedmont
- Bossa Nova, Samba, Latin Ballad
- Reggae Skank, Rocksteady, Dub
- Funk 16th, Funk Wah Rhythm
- Punk Downstroke, Metal Chug, Djent Palm-Mute Grid
- Jazz Comping (Freddie Green), Gypsy Jazz La Pompe
- Flamenco Rasgueado, Flamenco Alzapua
- Classical Etude, Classical Arpeggio
- Ambient Swell, Post-Rock Arpeggio

Each kit is a JSON file under `Resources/Genres/`, referencing patterns by
name and rig by preset id.

## 8. UI

Rhythm panel sits inside Advanced mode, Column 4, tab labelled RHYTHM.
Contents:

1. **Genre kit dropdown**, with a small "randomize within style" dice.
2. **Voicing controls**: style dropdown, density slider, hand-position knob,
   capo up/down.
3. **Strum pattern editor**: a 16- or 32-step grid, one row per subdivision.
   Each cell is a strum-direction button. Right-click a cell to set dynamic,
   mask, or delete.
4. **Fingerpick pattern editor**: 16-step grid, five rows for p/i/m/a/e.
5. **Feel controls**: swing (0-100%), timing jitter, velocity jitter, miss %,
   ghost %.
6. **Pattern browser**: list, filter by tag, load, save, export.
7. **Live indicators**: current chord symbol, current voicing shown as
   fretboard dots, next strum direction indicator that lights on the beat.

Easy Mode gets a compact strip: genre kit, feel knob, on/off switch. Nothing
else. Poly mode is the required playing mode; the switch shows a hint if the
user is in Mono.

## 9. Interaction with existing systems

- Sits after `MidiInterpreter`, before `TechniqueEngine`.
- Reads humanization values from the existing HumanizeMatrix without owning
  its own copy.
- Writes only `NoteOnEvent`, `NoteOffEvent`, and CC `PalmMuteAmount` events;
  never touches audio.
- Adds two new parameters to the preset: `rhythm_engine.enabled` and
  `rhythm_engine.state` (a compressed JSON blob of the current pattern,
  voicing settings and genre kit).
- MIDI-out (see `routing-io.md`) captures the transformed event stream, so
  users can bounce a rhythm performance to MIDI.

## 10. Tests

- Chord detector round-trip for all 84 templates, 12 keys, all inversions:
  1008 cases.
- Voicer produces a valid voicing (or explicit "unplayable") for every chord
  symbol on every ship-guitar tuning: minimum 25 guitars * 84 chords * 12
  keys = 25 200 assertions.
- Strum pattern quantization: at 120 bpm, 16th grid, verify sample-accurate
  scheduling within +/- 1 sample at 48 kHz.
- Humanization determinism: given the same seed, output is byte-identical.
- Bypass: enable/disable during playback, verify no click.
- Free-run mode: verify pattern advances at correct tempo when host is
  stopped and Free-run is on.
