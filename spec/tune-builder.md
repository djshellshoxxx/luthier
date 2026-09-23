# TUNE BUILDER SPEC

The composition layer on top of `rhythm-engine.md`. Sketch a chord
progression, draw or record a melody, pick a groove, hit play, and hear
a full arrangement. Export as audio or MIDI.

Written for the guitarist who wants to catch an idea in three minutes,
not a producer who wants to build a track in a DAW.

## 0. Ground rules

1. The Tune Builder is a **control-only** module. It writes MIDI into
   the rhythm engine and note engine; no new audio-path DSP.
2. Everything a user builds is **saveable and reloadable** as a single
   `.luthiertune` file, small (< 100 KB typical), portable, forward-
   compatible.
3. **Nothing is destructive.** Every edit is undoable. Regenerating a
   pattern preserves manual edits by marking edited notes as "locked".
4. It works **inside the plugin**, so no DAW is required. Standalone or
   inside a host: same UI, same behaviour.
5. **Export is a first-class action**, not an afterthought. Audio, MIDI
   (Luthier or Generic profile), and MusicXML / Guitar Pro all fall out
   of the same `PerformanceScore`.
6. **Easy first, deep second.** The default screen fits a whole tune;
   power users get a step-open editor for every element.
7. Melody is entered by any of five methods (section 5). The user picks;
   no method is the "official" one.

## 1. Data model

A tune is a `Tune` struct:

```
Tune {
  meta { title, artist, tempo_bpm, time_signature, key, mode,
         swing, feel_pct }
  arrangement {
    sections: [
      Section {
        name,           // "Verse", "Chorus", "Bridge", ...
        length_bars,
        chord_progression: [ChordCell],
        rhythm_pattern_id,      // from rhythm-engine.md's PatternLibrary
        genre_kit_id,           // from rhythm-engine.md
        melody_track: MelodyTrack | null,
        bass_track: BassTrack | null,
        active_layers: [Layer]  // pluggable, section 8
      }
    ],
    setlist: [SectionRef]  // e.g. "Intro, Verse x2, Chorus, Verse, Chorus x2, Outro"
  }
  variations: [ArrangementVariation]  // A/B ideas of the same tune
}
```

Every field is versioned. Older `.luthiertune` files load with
sensible defaults for missing fields.

### 1.1 ChordCell

```
ChordCell {
  root,           // C, D, ...
  quality,        // maj, min, 7, m7, sus4, ...
  bass,           // slash-chord bass; null = root
  extensions,     // 9, 11, 13, add9, ...
  duration_beats, // 1.0, 2.0, 0.5, ...
  strum_override, // per-cell strum pattern; null = section default
  emphasis        // normal, accent, ghost
}
```

Duration is in beats; time signature converts to bars. A section is
built from cells until `length_bars` is filled; the last cell can be
duration-underspecified and the builder holds it to fill.

### 1.2 MelodyTrack

```
MelodyTrack {
  notes: [MelodyNote],
  string_hint,     // "auto" | 1..6, biases voicer for melody placement
  articulation_default  // legato, staccato, palm-muted, ...
}

MelodyNote {
  start_beat,
  duration_beats,
  pitch,           // MIDI note number, or "chord tone N" (relative)
  velocity,
  articulation,    // overrides default
  technique,       // bend, slide, hammer-on, pull-off, vibrato, harmonic
  locked           // manual edit; regenerate does not touch
}
```

`pitch` accepts both absolute (MIDI 60 = C4) and relative (`root+7`,
`chord_tone_3`) so a melody follows chord changes when the user asks
for it.

## 2. The three-minute tune workflow

The default screen. New tune, hit go, done.

1. **Pick a genre kit** from the dropdown. Presets: Folk Strum,
   Country Boom-Chick, Blues Shuffle, Rock Ballad, Reggae Skank, Bossa,
   Funk 16th, Metal Chug, Jazz Comping. Every genre kit ships with a
   suggested tempo, feel, and a chord palette.
2. **Type a progression**. A single text field accepts standard chord
   notation: `Am F C G` or `| Am7 | D7 | Gmaj7 | Cmaj7 |` or
   `[Verse] Am F C G [Chorus] F C G Am`. Bars are pipe-separated;
   sections are bracketed. Cell duration defaults to one bar; suffixes
   like `Am*2` double the duration, `Am*0.5` halves it.
3. **Sketch a melody** (or skip). Four options are one click each:
   - **Auto** — generates a melody that follows the changes, biased
     toward the chord's guide tones. Uses the genre kit's melody
     profile.
   - **Draw** — click-drag on the piano-roll strip; snaps to key by
     default.
   - **Record** — play through MIDI in; quantise on release.
   - **Improvise** — the plugin generates a new melody every playback
     within the chord constraints.
5. **Hit play.** The tune loops. Every panel above the transport is
   live-editable; changes take effect on the next bar boundary (or
   immediately if paused).
6. **Save or export.** `Ctrl+S` saves the `.luthiertune`; `Ctrl+E`
   exports; the export dialog is one screen.

The whole flow fits on a single tab: TUNE (Column 4, section 3 of this
document, and gui-integration.md).

## 3. UI

Lives as a Column 4 tab labelled **TUNE**, placed between RHYTHM and
LIVE in the tab strip:

`WORKSHOP | MOD | RHYTHM | TUNE | LIVE | ROUTING | TONE MATCH | CHARACTER | PRACTICE | NOTATION | MIDI OUT | CONTROLLERS | HELP`

### 3.1 Default layout

```
+----------------------------------------------------------------+
| TUNE   [My New Tune]    [Save]  [Export]   Tempo [120] Key [C] |
+----------------------------------------------------------------+
| SECTION STRIP (click a section to open):                       |
|   [Intro 4] [Verse 8] [Chorus 8] [Verse 8] [Chorus 16] [Outro 4]|
+----------------------------------------------------------------+
| PROGRESSION (chord cells for active section):                  |
|   | Am 1 | F 1 | C 1 | G 1 |                                   |
|   ---- click any cell to edit ----                              |
+----------------------------------------------------------------+
| RHYTHM: [Folk Strum ▾]  Feel [•—•]  Strum [•—•]  On [x]        |
+----------------------------------------------------------------+
| MELODY:                                                        |
|                                                                |
|   [piano-roll strip, 1-4 bars visible, scrollable]             |
|                                                                |
|   [Auto] [Draw] [Record] [Improvise]  Quantise [1/8 ▾]         |
+----------------------------------------------------------------+
| TRANSPORT:  [<<] [play/pause] [>>]  Loop [x]  Metronome [x]    |
+----------------------------------------------------------------+
```

At 1280 px window width the layout fits comfortably. Below that, the
piano-roll shrinks vertically.

### 3.2 Progression editor

Chord cells rendered as coloured pills. Colours from the theme's palette,
mapped to chord function (I, ii, iii, IV, V, vi, vii°) in the current
key. Non-diatonic chords use a neutral pill.

Interactions:
- Click a cell to open a popover: root, quality, bass, extensions,
  duration, strum override, emphasis.
- Drag a cell's right edge to change duration in beats.
- Drag a cell left / right to reorder.
- Type into the text field above the pills to rewrite the progression
  in shorthand; the pills update live.
- Right-click a cell: Insert before / after, Duplicate, Delete, Copy /
  Paste, "Suggest substitution" (offers common substitutions like
  tritone sub, ii-V insertion, relative minor).

### 3.3 Section strip

Sections rendered as tabs. Click a section to load its editors. Drag
sections to reorder. Right-click:
- Rename.
- Duplicate.
- Delete.
- Repeat count (x1, x2, x4, custom).
- "Vary" (creates a subtle variation of the section as a new sibling).
- Set as intro / verse / chorus / bridge / outro (colour tag).

The setlist for the whole tune (repeats and section order) is edited
by dragging tabs into the timeline strip at the top of the section
list.

### 3.4 Melody editor

A piano roll, but simplified: shows the current section's melody only,
scaled to the section's bar count. Bar lines, beat lines, key-of-scale
row shading.

Notes:
- Draw with click-drag; height sets pitch, length sets duration.
- Snap to key by default. Toggle chromatic with `C`.
- Right-click a note: velocity, articulation, technique, "unlock" (a
  regenerate leaves this note alone if locked), delete.
- Multi-select with drag-box; shift-click adds.
- Cut / copy / paste (standard shortcuts).
- Nudge with arrow keys; larger nudge with Shift.

The four generate buttons run their algorithms on the current
section only, and preserve any locked notes.

### 3.5 Rhythm strip

Reuses the rhythm engine's genre kit, feel, and strum controls.
Editing here is editing the section's rhythm; other sections keep
their own settings unless linked (right-click a section: "Link rhythm
to X").

### 3.6 Transport

Play, pause, skip forward / back (whole section), loop, count-in,
metronome. The transport is independent of the host transport when
the plugin is standalone or when the host is stopped; when the host
plays, host transport wins.

Transport shortcut: spacebar (play / pause), Shift+space (play from
current section start).

## 4. Melody generation algorithms

Every generator obeys the chord progression and stays inside the
current section's harmonic constraints (scale + chord tones).

### 4.1 Auto

Rule-based, deterministic per seed:
- Start on a chord tone of the first chord (default third).
- Move by step 70% of the time, by chord tone leap 20%, by wider
  interval 10%.
- Rest at cadence points (last beat of a bar, and last bar of the
  section).
- Follow the genre kit's phrasing profile: country prefers pentatonic
  runs, jazz prefers guide-tone lines connecting chord thirds and
  sevenths, folk prefers stepwise motion around chord roots.
- Adhere to `melody_range` (default C3 to G5); wider on request.
- Adhere to `melody_density` (default 4 notes / bar in most kits).

Every run with the same seed produces the same melody; "Regenerate"
increments the seed.

### 4.2 Draw

No algorithm; user's hand.

### 4.3 Record

Quantise on release with the user-selected grid (1/4, 1/8, 1/8T,
1/16, 1/16T). Velocity is preserved. Held notes across chord changes
are optionally re-fitted to the new chord (toggle: "Follow chord
changes").

### 4.4 Improvise (live)

Same rule set as Auto, but reseeds every loop pass so the melody
varies. Locked notes still play as written. "Freeze" button captures
the current pass into the section (converts to written notes).

### 4.5 Style transfer (small, offline)

Optional per section: "Play this melody like a bluegrass fiddle" or
"like a jazz sax". Applies articulation and micro-timing profiles
from a small library. Does not change pitch, only phrasing.

## 5. Chord progression tools

- **Diatonic palette**: click chords from a wheel of the current key's
  I-vi degrees; they append to the progression.
- **Suggest next chord**: given the last chord, offers three common
  next moves for the current key and genre.
- **Reharmonize**: replaces the current progression with a
  reharmonization (tritone subs, secondary dominants, modal
  interchange). One-shot with undo.
- **Transpose**: shifts every chord by N semitones; melodies follow.
- **Modal shift**: swap the mode of the current key (Ionian ↔
  Aeolian ↔ Dorian ↔ ...); melodies stay on their absolute pitches
  unless "Follow mode" is toggled.

## 6. Bass line

Every section can carry a bass track. Options:
- **Off**: no bass.
- **Root**: whole notes on the root of each chord.
- **Root-Fifth**: country / rock pattern.
- **Walking**: jazz walking bass, generated to reach the next chord's
  root.
- **Genre**: use the genre kit's suggested bass pattern.
- **Manual**: same piano-roll editor as melody, targeting the bass
  range.

Bass plays through the plugin's own bass mode (bass-techniques.md) if
the current instrument is a bass, or through a companion bass instance
if the routing panel is set up for it. On a single guitar instance,
the bass line renders as MIDI on a separate output when export is
requested.

## 7. Layers

Optional per section, additive:
- **Pad**: sustained chord tones on a soft picked layer.
- **Arpeggio**: a fingerpick pattern from the rhythm engine.
- **Countermelody**: a second melody track, auto-generated to
  complement the main melody.
- **Percussion**: uses the plugin's chuck / palm-mute noise as
  rhythmic percussion; no drum sounds.

Every layer has its own on / off, volume, and pan.

## 8. Playback and rendering

- Tune Builder writes into the same rhythm engine and note engine the
  live-play path uses. Every audible detail from the realism specs
  (squeak, pick, buzz, slide, character) applies to the tune's
  playback exactly as it does to live play.
- Section boundaries write a "state boundary" so mod matrix envelopes
  and rhythm engine phase reset if the user wants (per-section toggle).
- Loop mode plays the setlist end-to-end and repeats.

## 9. Export

One dialog, four destinations:

### 9.1 Audio
- WAV / FLAC / MP3.
- Bit depth: 16 / 24 / 32-float.
- Sample rate: current host or user-selected.
- Stem export: main stereo, or every routing bus (aux 1-8) as its own
  file.
- Loop tail: 0 - 5 s of decay after the final beat.
- Destination: `~/Documents/Luthier/Renders/` by default.

### 9.2 MIDI
- Luthier profile or Generic profile (midi-export.md).
- Track split: single track, per section, per instrument (main +
  bass), per string.
- Export the melody with realism events (bends, slides, vibratos) or
  as plain note-on / note-off.

### 9.3 Notation
- MusicXML / Guitar Pro / ASCII TAB (notation-export.md).
- Section headings preserved.
- Chord symbols above the staff.
- Tab and standard both, or either alone.

### 9.4 Project
- `.luthiertune` file. Portable, opens in any Luthier instance.
- Optionally bundle the `.luthierpreset` and any referenced
  `.luthierguitar` so the file is self-contained.

## 10. Templates

Ship a template library. Each is a partly-filled `Tune` the user
starts from and modifies:

- **Blank** — no sections, no chords.
- **Verse/Chorus** — two 8-bar sections, empty chords.
- **12-bar blues in E** — classic form, three sections (head,
  middle, out).
- **AABA jazz standard** — 32 bars, chord palette pre-filled with
  a common progression.
- **Reggae one-drop in A** — verse + chorus with skank pattern.
- **Country waltz** — 3/4 with boom-chick pattern.
- **Rock ballad** — Am F C G verse, F G Am chorus.
- **Bossa Nova** — I-vi-ii-V in Ebmaj7.
- **Punk two-chord** — I-IV, downstroke pattern.
- **Instrumental fingerstyle** — no chord section, empty melody
  ready for capture.

## 11. `.luthiertune` file format

JSON with a signature header. Small, human-readable, forward-
compatible. Example (abridged):

```json
{
  "schema": 1,
  "meta": { "title": "First Sketch", "tempo_bpm": 96, "key": "Am",
            "time_sig": "4/4", "swing": 0, "feel_pct": 0 },
  "sections": [
    {
      "name": "Verse", "length_bars": 8, "rhythm_pattern": "folk_strum_1",
      "genre_kit": "Folk Fingerstyle",
      "chords": [
        { "root": "A", "quality": "min", "beats": 4 },
        { "root": "F", "quality": "maj", "beats": 4 },
        { "root": "C", "quality": "maj", "beats": 4 },
        { "root": "G", "quality": "maj", "beats": 4 }
      ],
      "melody": [
        { "start": 0.0, "dur": 1.0, "pitch": 69, "vel": 90 },
        { "start": 1.0, "dur": 0.5, "pitch": 72, "vel": 88 }
      ]
    }
  ],
  "setlist": [
    { "section": "Verse", "repeats": 2 },
    { "section": "Chorus", "repeats": 2 }
  ]
}
```

## 12. Standalone-friendly

The Tune Builder is where standalone Luthier stops being a plugin
demo and starts being a full sketchpad. A user launching the
standalone app to catch an idea should:
- Open the app.
- See the last tune loaded, or a blank one.
- Have MIDI in armed to the currently selected input.
- Have audio in armed if they want to hum a melody in for pitch
  detection (section 13).
- Be able to record, sketch, save and export without opening any
  other menu.

## 13. Optional: sung / hummed melody capture

If the standalone app has a mic input and the user chooses "Record →
Voice", the plugin runs a monophonic pitch tracker on the input and
converts the sung line into a melody track. Uses the same pitch
detection library the Bend Trainer uses (practice-tools.md 4).

Confidence threshold: rejects samples with confidence below 0.6,
snaps to key by default, quantises timing on release.

Not enabled by default; opt-in via a big "Sing" button on the
Melody strip when audio in is present.

## 14. Interaction with other specs

- **rhythm-engine.md** owns the per-section rhythm pattern and voicer.
- **modulation-matrix.md** works: mod routes can automate section
  parameters (feel, tempo drift) over the tune's timeline.
- **live-performance.md** snapshots capture the current section state,
  so a live rig can switch sections with a footswitch.
- **notation-export.md** and **midi-export.md** consume the Tune's
  `PerformanceScore`.
- **practice-tools.md** looper can capture a whole Tune render into a
  loop layer for practising over.
- **tone-match.md** applies to the tune's audio path unchanged.
- **advanced-ranges.md** applies to the tune's parameters unchanged.

## 15. Tests

- Progression parser: 100 shorthand strings parse to expected
  `ChordCell` arrays; malformed strings produce named errors.
- Auto melody determinism: same seed produces byte-identical melody
  1000 runs.
- Locked notes: regenerate leaves locked notes byte-identical.
- Section reorder: 1000 random reorders preserve total tune length
  and every note's beat position within its section.
- Loop mode: a 32-bar tune loops for 60 s without drift; final loop
  ends at the same sample offset as the first within 1 sample.
- Save / reload: round-trip of 100 random `.luthiertune` files
  produces byte-identical files.
- Export audio: offline render matches live render within -80 dBFS
  RMS null.
- Export MIDI: Luthier profile export re-imported produces byte-
  identical rendered audio (per midi-export.md tests).
- Sung capture: a fixture hum of a known melody is transcribed with
  pitch correct to the nearest semitone in 95% of notes and rhythm
  correct to the quantise grid.
- Standalone reload: launch standalone, save a tune, close, relaunch,
  the last tune loads and plays.
