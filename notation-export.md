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

## 6. Tests

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
