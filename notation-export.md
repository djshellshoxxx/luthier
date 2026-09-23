# NOTATION AND TAB EXPORT SPEC

Extends spec.md's one-line "Tab display" note. Turns what the plugin plays
into readable notation that a guitarist can hand to another player.

## 0. Ground rules

1. Notation export is offline. It runs on a worker thread from the captured
   MIDI or from the plugin's own event stream, not on the audio thread.
2. Notation output is **guitar-aware**: strings and frets, not just pitches.
   A note played on string 3 fret 5 is not the same as string 4 fret 0 in
   the output.
3. Notation output preserves technique metadata: bends, slides, hammer-ons,
   pull-offs, palm mutes, harmonics, tapping, whammy events.
4. Exports round-trip: exporting to a format and re-importing produces the
   same on-screen notation, minus format-specific losses documented per
   format.

## 1. Data model

Internal representation is `PerformanceScore`:
```
PerformanceScore {
  meta { title, artist, tempo_bpm, time_signature, key, tuning }
  tracks: [
    Track {
      guitar_id,
      capo_fret,
      measures: [
        Measure {
          time_sig, tempo_change,
          voices: [
            Voice {
              notes: [
                ScoreNote {
                  start_beat, duration_beats,
                  string_index, fret,
                  pitch_hz, velocity,
                  techniques: [Bend{semitones, curve}, Slide{to_fret},
                              HammerOn, PullOff, PalmMute{depth},
                              NaturalHarmonic, PinchHarmonic, Tap,
                              Vibrato{rate, depth},
                              Whammy{semitones, curve}, ...]
                }
              ]
            }
          ]
        }
      ]
    }
  ]
}
```

The plugin builds this in real time from the string engine's activity plus
the MIDI interpreter's technique tags. Users can capture the last N minutes
(default 10) into a `PerformanceScore` and export.

## 2. Formats

### 2.1 MusicXML 4.0 (`.musicxml`)

- Full technique support via `<technical>` elements: bend, slide,
  hammer-on, pull-off, palm-mute, harmonic, tap, string, fret.
- Chord symbols from the chord detector, when Poly mode is active.
- Grace notes for hammer-on/pull-off ornaments.
- Multi-voice per staff when polyphonic material demands it.
- Round-trip loss: whammy events are approximated as pitch-bend text
  directions (MusicXML has no native whammy element).

### 2.2 Guitar Pro 8 (`.gp`)

- Native format for the target audience.
- Full technique fidelity: string, fret, bend curves, slides (legato,
  shift, out, in), palm mute, harmonics (natural, artificial, pinch, tap,
  semi), tapping, whammy points as bar events.
- Chord diagrams inserted at first occurrence of each detected chord.
- Round-trip loss: none for supported techniques.

### 2.3 ASCII tab (`.txt`)

- Six-line staff, one line per string, low string on bottom.
- Ruler line above with beat marks.
- Techniques as symbols: `b` bend, `r` release, `h` hammer, `p` pull,
  `/` slide up, `\` slide down, `~` vibrato, `PM-----` palm mute bar,
  `<12>` natural harmonic, `[12]` artificial harmonic.
- Line width configurable, default 80 chars.
- Section headings above each new part.
- Round-trip loss: subtle bend curves flattened to symbolic bends.

### 2.4 MIDI (`.mid`)

- Standard MIDI file with per-string tracks (16 max).
- Guitar Pro-compatible RPN messages for string/fret hints.
- Pitch bend for bends and whammy.
- CC 68 (legato) for hammer-ons/pull-offs.
- Round-trip loss: technique metadata beyond pitch bend and legato drops.

## 3. Live TAB view

A live view in the practice panel (see `practice-tools.md`) shows the last
N beats of ASCII tab, updated as the user plays. The plugin can also
render the current bar to the on-plugin fretboard as tablature dots.

Controls:
- Show/hide.
- Scroll speed: slow, medium, fast, freeze.
- Bar count on screen: 1-8.
- Symbol density toggle: full, minimal, notes-only.

## 4. Chord symbol extraction

When Poly mode is active, the chord detector's output is written into the
`PerformanceScore` as chord symbols at the beats where they change. This
requires no additional analysis.

In Mono mode, chord extraction runs offline on the captured `PerformanceScore`
using a template match against detected pitch classes per beat.

## 5. Export UI

New tab in the file menu: `Export -> Notation`.

Dialog:
- Format dropdown: MusicXML, Guitar Pro 8, ASCII tab, MIDI.
- Range: entire capture, last N seconds, marked region.
- Options per format (e.g., line width for ASCII, chord diagrams on/off
  for Guitar Pro).
- Destination path.
- Preview pane: shows the first bar of the output for MusicXML and ASCII.

Also expose "Export to Notation" from the MIDI-capture "Save last take"
button, alongside the existing MIDI export.

## 6. Performance capture

Sections 3 and 4 both assume a `PerformanceScore` that reflects what the
player just played - "updated as the user plays", "the captured
`PerformanceScore`". Nothing in the plugin produces one. `PerformanceScore`
is only ever filled by `NotationImporter` reading a file, which means the
live TAB view has nothing to display and the export dialog has nothing to
export.

`PerformanceCapture` is the module that fills it.

### 6.1 What it records

Every voiced note the engine actually played, after voicing and technique
resolution - not the incoming MIDI. The distinction matters: the score
should say which string and fret were used, and that is a decision the
`ChordVoicer` and `MidiInterpreter` make, not the player's keyboard.

Per note: string, fret, start time, duration, velocity, and the technique
flags already carried by `VoicedNote` (hammer, pull, slide, bend, ghost,
palm-mute, harmonic).

Plus, on their own tracks: chord symbols from the detector (section 4),
tempo and time-signature changes from the host transport, and the
`BASS_TECH` and slide events the realism specs produce.

### 6.2 The ring

Capture runs on the audio thread and must not allocate there.

- A lock-free ring of fixed-size note records, 8192 entries (about twelve
  minutes of dense playing), pre-allocated at `prepare()`.
- The audio thread writes; a message-thread drain at 10 Hz moves records
  into the `PerformanceScore`.
- Ring overflow drops the **oldest** and increments a counter the UI can
  show. Dropping oldest rather than newest keeps the live TAB view
  correct, which is the common case; a user capturing a long take is told
  the count.

This is the same shape as the session recorder's ring
(`practice-tools.md` 8), and it uses the same primitive.

### 6.3 Capture states

| State | Behaviour |
|---|---|
| `off` | Nothing written. Zero cost. |
| `rolling` | Continuous; the ring holds the last N minutes. Default. |
| `armed` | Cleared and recording from the next note, for a deliberate take |

`rolling` is the default because the live TAB view needs it and because
the most useful capture is the one a player did not know they wanted - you
play something good and then want it.

### 6.4 Timing

Note times are in **quarter notes against the host transport** when it is
running, and in seconds when it is not. Free-play capture is quantisable
afterwards; transport-locked capture is already in musical time.

`tune-builder.md`'s "capture what I just played into a section" flow reads
this score.

### 6.5 What it does not do

- It does not capture audio. That is the session recorder.
- It does not quantise on the way in. The score holds what happened;
  quantisation is an export and edit option.
- It does not run when `off`, and `off` costs nothing.

## 7. Tests

- MusicXML round trip: export, reimport into MuseScore fixture, compare
  note count and pitches, verify zero-loss on supported techniques.
- Guitar Pro round trip: export, reimport via a fixture parser, compare
  string/fret assignments byte-identical.
- ASCII tab: verify column alignment for a fixed-tempo test riff at 4/4.
- MIDI: exported file re-imported into the plugin re-renders audio within
  -60 dBFS null of the source performance (minor techniques may not
  survive, so tolerance is looser than DI null tests).
- Chord extraction: 100 known chord progressions, verify detection
  accuracy > 95%.

### 7.1 Capture tests

- **Capture records voiced notes, not MIDI.** Play a chord that the voicer
  moves to different strings; assert the score's string/fret pairs match
  the voicer's output, not a naive mapping.
- **No allocation on the audio thread** during 10 000 captured notes.
- **Ring overflow drops oldest and counts.** Fill past 8192 records;
  assert the oldest are gone, the newest are present, and the drop counter
  is exact.
- **Off costs nothing.** With capture off, assert no ring write occurs and
  the rendered audio is bit-identical to a build with capture removed.
- **Transport timing.** With the host rolling at 120 bpm, a note on beat 3
  is recorded at quarter-note position 2.0 within 1 ms.
- **Free-play timing** is recorded in seconds when the transport is
  stopped.
- **The live TAB view shows what was played.** Play a known phrase; assert
  the rendered ASCII tab matches the expected string/fret sequence.
- **Export round-trips a captured score.** Capture a phrase, export as
  Luthier-profile MIDI, re-import, assert the note list is identical.
