# CONTROLLER INTEGRATION SPEC

Fleshes out spec.md's one-sentence mentions of MIDI guitars, MPE controllers,
and specific hardware. Defines exact routing, latency compensation and
calibration for each supported controller family.

## 0. Ground rules

1. Every supported controller has a **named profile** loaded from
   `Resources/Controllers/*.json`. The user picks a profile in Options;
   default is "Generic MIDI".
2. Each profile declares its MIDI mode (standard, MPE zone, per-channel
   fixed), pitch-bend range default, latency budget, and CC map.
3. Per-source latency compensation is applied on the MIDI input path, not
   on the audio output. Late MIDI is quantized forward to the next block
   boundary; a leading offset can be applied to compensate for tracking
   latency.
4. Controller profiles are read-only in `Resources/`; user overrides live
   in `~/Documents/Luthier/Controllers/` and win when both exist.

## 1. Supported profiles at ship

### Generic MIDI
- Channel 1, standard MIDI.
- Pitch-bend range 2 semitones.
- No per-string routing.

### Generic MPE (Lower Zone)
- Master channel 1, member channels 2-16.
- Pitch-bend range 48 semitones on member channels, 2 on master.
- Per-note pitch, pressure, and CC 74 (timbre / Y-axis).

### ROLI Seaboard family
- MPE Lower Zone, pitch-bend 48.
- CC 74 mapped to whammy by default (user-overridable).
- Aftertouch mapped to vibrato depth.

### LinnStrument
- MPE Lower Zone.
- Per-row string emulation optional: rows map to strings if user enables
  "Guitar mode" in Options.

### Osmose
- MPE Lower Zone with continuous pitch curve; profile provides a look-up
  table for its non-linear pitch response.

### Roland GK-3 / GR-55 / GR-D
- Per-string on channels 11-16 by default (high E on 11).
- Pitch-bend range configurable per string, default 24.
- Includes profile latency budget of 3 ms (representative; user calibrates).

### Fishman TriplePlay
- Per-string on channels 1-6, but configurable.
- Pitch-bend range 24.
- Latency budget 5 ms.

### Jamstik / Jamstik+ / Jamstik Studio
- Per-string on channels 1-6.
- Pitch-bend range 12.
- Latency budget 8 ms.

### Yamaha EZ-EG and similar guitar-shaped MIDI toys
- Standard MIDI on channel 1.
- No per-string.
- Aftertouch and CC map varies; profile provides sensible defaults.

## 2. Profile file format

```json
{
  "id": "roland-gk",
  "display_name": "Roland GK Series",
  "mode": "per_channel",
  "per_string": {
    "1": { "channel": 11, "pitch_bend_semis": 24 },
    "2": { "channel": 12, "pitch_bend_semis": 24 },
    "3": { "channel": 13, "pitch_bend_semis": 24 },
    "4": { "channel": 14, "pitch_bend_semis": 24 },
    "5": { "channel": 15, "pitch_bend_semis": 24 },
    "6": { "channel": 16, "pitch_bend_semis": 24 }
  },
  "cc_map": {
    "74": "whammy_amount",
    "1":  "vibrato_depth"
  },
  "latency_ms_default": 3.0,
  "notes": "GK pickup latency typical for GR-55; calibrate for GR-D"
}
```

## 3. Per-source latency compensation

- User measurable via a "Latency wizard" in Options -> Controllers.
- Wizard plays a click, asks user to play along on the controller, measures
  the time delta from click to detected NoteOn.
- Result stored in profile as `latency_ms_measured`.
- Compensation is applied by shifting MIDI events earlier in the internal
  timeline by the measured amount. This works because the plugin renders
  block-by-block; events can be pre-scheduled up to one block ahead.
- If the compensation exceeds the current block size, the plugin
  interpolates. If it exceeds two blocks, the wizard warns and suggests a
  smaller block size in the host.

## 4. Hex-pickup routing (Modes B and C of the existing MIDI Interpreter)

Per-channel mode:
- Each channel is a string. Profile declares which channel maps to which
  string index.
- Notes on that channel play on that string, regardless of pitch, at the
  fret required to produce the pitch. If the pitch is unreachable on that
  string, event is dropped with a diagnostic log.
- Pitch bend on that channel bends only that string.
- Pressure on that channel drives vibrato depth on that string.

MPE mode:
- Master channel notes are ignored.
- Member channels each carry one note at a time.
- String assignment on note arrival: use the string-assignment algorithm
  from engine.md, biased toward the string used most recently on this
  member channel (sticky).

## 5. Calibration

For each profile, calibrate:
- Pitch-bend range: send max-up bend, user identifies target pitch on
  Luthier's UI to confirm.
- Note-off latency: some controllers send lazy note-offs; profile stores a
  minimum note duration.
- Pitch tracking noise: on some controllers, sustained notes emit constant
  low-magnitude pitch bend around the target. Profile stores a dead-zone
  in cents (default 5).

## 6. Multi-controller merge

Users can plug two controllers at once (e.g., MPE keyboard plus GK). The
plugin accepts MIDI from all sources, tags each event with its source, and
routes according to per-source profiles.

Priority rules when both sources trigger a string simultaneously:
- Most recent event wins on a per-string basis.
- Older string activity keeps its existing state until note-off.
- Diagnostic warning if the same string is being driven from two sources
  contentiously.

## 7. Tests

- Every ship profile loads cleanly and its cc_map values resolve to valid
  destinations.
- Latency wizard produces stable measurements across 10 runs to within
  0.5 ms sigma.
- Per-channel routing: fuzz 10 000 events across channels 1-16, verify
  correct string is triggered per profile.
- MPE stickiness: for a note on member channel 3, subsequent notes on the
  same channel prefer the same string until a non-adjacent pitch is
  requested.
- Multi-controller merge: dual-source stress test, verify no dropped
  strings and no coupling matrix corruption.
