# RIFF LIBRARY SPEC

A browsable library of original riffs, licks, strum parts and bass lines
for guitar and bass. Each one carries the full technique data (bends,
slides, hammer-ons, pull-offs, palm mutes, harmonics, vibrato) that the
Luthier MIDI profile can express. The user browses, filters and auditions
riffs through the current guitar sound, in any key and at any tempo. From
there a riff can be dragged into the DAW as a `.mid` file, dropped into
the Tune Builder, sent to the looper, or opened in the practice tab reader
to learn.

Audiences:
- Players who want a starting idea, or something to practise, in their
  own genre.
- Producers who want a playable, articulated guitar or bass part in the
  DAW in ten seconds, one that sounds like a guitarist played it.
- Beginners who want to hear what "a hammer-on" or "a half-step bend"
  actually sounds like on this guitar.

Added 2026-09-24. Additive to `gui-integration.md`, `midi-export.md`,
`practice-tools.md`, `tune-builder.md` and `file-formats.md`. It moves
nothing that is already placed.

## 0. Ground rules

1. **Original content only.** Every factory riff is written for Luthier.
   There are no transcriptions, and nothing is "in the style of" a named
   artist or song. Names are generic, and each one is a descriptor plus
   a number ("Delta Turnaround 3", "Palm-Mute Gallop 2"). The
   trademark scan and a name deny-list (section 12) enforce this in CI.
2. **Techniques are explicit.** A riff says exactly which string, fret
   and technique each note uses. Playback never guesses. The
   auto-articulation of `auto-articulation.md` (being written in
   parallel) must leave riff notes alone (section 5.4).
3. **One source of truth.** The `.luthierriff` JSON is the stored form.
   The `.mid` file (in either profile) is always derived from it by the
   plugin's own `MidiProfiles` writer. That way there is only one Luthier
   profile encoder, and it is never duplicated in Python.
4. **Content is reviewable text.** Factory riffs are written in a compact
   tab-like notation under `Tools/riffs/` and compiled by a deterministic
   generator. A reviewer reads a diff of the notation, not of JSON.
5. **Audition is a MIDI-level player.** It adds no audio module, uses no
   samples, and does not allocate or lock on the audio thread. What you
   hear is the engine playing the notes.
6. **No new automatable parameters.** The library is for browsing and
   composing. Everything it sets is UI or session state (section 8).

## 1. User stories

- **U1** "I play blues. Show me beginner licks in A with bends." The user
  picks Blues, difficulty 1-2 and the technique Bend in the filters,
  clicks a row, and presses Space. The lick plays through the current
  preset.
- **U2** "Same lick, but in E, at my song's tempo." The user sets Key to
  E. With the host playing, audition locks to the host's tempo and starts
  on the next bar.
- **U3** "Put it in my DAW." The user drags the row's handle onto a MIDI
  track. The track gets a Luthier-profile `.mid` file, and played back
  through Luthier it keeps its bends and slides.
- **U4** "Build my verse from it." The user drops the riff onto the TUNE
  tab's melody strip, or presses Add to Tune. The notes land in the
  selected section, transposed to the tune's key.
- **U5** "Teach me this." Learn It opens the practice drawer's TAB tab
  with the riff loaded, looping, at 70% tempo, with the speed trainer
  armed.
- **U6** "Jam over it." Send to Looper renders the riff as a looper layer,
  which the user then plays over.
- **U7** "I just played something good." The user marks the phrase in the
  performance capture and chooses Save as Riff. It becomes a user riff,
  with its key, tempo, techniques and difficulty filled in automatically.

## 2. Content

### 2.1 Factory counts (300 items at 1.0)

| Genre | Riff | Lick | Strum | Bass | Total |
|---|---|---|---|---|---|
| Rock | 10 | 8 | 4 | 6 | 28 |
| Blues | 6 | 12 | 4 | 6 | 28 |
| Metal | 12 | 6 | 2 | 6 | 26 |
| Funk | 6 | 6 | 6 | 8 | 26 |
| Country | 4 | 10 | 4 | 6 | 24 |
| Jazz | 2 | 10 | 4 | 8 | 24 |
| Folk / Acoustic | 4 | 6 | 10 | 4 | 24 |
| Reggae / Ska | 4 | 4 | 6 | 6 | 20 |
| Latin | 4 | 4 | 8 | 6 | 22 |
| Pop | 6 | 6 | 6 | 6 | 24 |
| Punk / Indie | 8 | 6 | 6 | 6 | 26 |
| Soul / R&B | 6 | 8 | 6 | 8 | 28 |
| **Total** | **72** | **86** | **66** | **76** | **300** |

The generator enforces these coverage rules and fails the build if one
is broken:
- At least 20 items per genre.
- Every genre has at least one item at each difficulty from 1 to 4.
  Difficulty 1-2 makes up at least 40% of the total.
- Every technique token of `docs/MIDI_EXPORT_LUTHIER_PROFILE.md` 6 (NOTE
  flags, plus bend, prebend, bendrelease, the slide kinds and vibrato)
  appears in at least 3 items. The exceptions are `artificial`,
  `tapharm` and `whammy`, which need at least 2 each.
- Metal has at least 4 items in Drop D and at least 2 for 7-string.
- Bass has at least 6 items for 5-string. Funk and Soul each have at
  least 2 slap / pop lines (BASS_TECH).
- Every item is 1 to 8 bars long, and no longer than 64 beats.

Riff = a repeating rhythm figure. Lick = a melodic phrase. Strum = a
fixed chord part with explicit voicings and strum strokes. Bass = a line
for a bass family instrument.

A **strum** item is a performance: fixed voicings with strokes. A
rhythm-engine `.luthierpattern` is different, because it follows
whatever chord is held. The two are not converted into each other.

**Free set:** 60 items flagged `free` in the source, 5 per genre. Each
genre's 5 include at least one bass line, and all are difficulty 3 or
below (section 11).

### 2.2 Authoring notation (`Tools/riffs/<genre>.riffdef`)

One file per genre. Each item is a header block followed by a `play:`
body:

```
riff blues.lick.delta-turnaround-3 "Delta Turnaround 3"
  type lick   instrument guitar6   tuning standard   capo 0
  key A minor_pentatonic   tempo 84   meter 4/4   feel shuffle
  difficulty 2   tags turnaround box-1   free yes
  chords 0:A7 4:E7
  play:
    t8: 1.8b2~ 1.8 1.5 | 2.8b1r 2.5 3.7/9 3.9~ ~ r |
```

Grammar (strings are numbered 1 = highest, as tab is read):
- **Duration** prefixes are sticky until changed: `w h q e s t` (whole
  to 32nd). `.` adds a dot. `t8` / `t16` are triplet eighths or
  sixteenths. `r` is a rest, and `~` alone holds the previous note.
- **Note:** `<string>.<fret>`. A dead note is `<string>.x`.
- **Chord:** `[6.3 5.2 4.0 3.0 2.0 1.3]`, or a shape string
  `[x32010]` written low string to high.
- **Suffixes:**
  - `b<n>` bend by n semitones (0.5-3), with `r` for release
  - `pb<n>` prebend
  - `~` vibrato
  - `/<f>` legato slide to fret f, and `\<f>` a downward legato slide
  - `s/<f>` shift slide
  - `/in` `\out` slide in and out
  - `h` hammer-on and `p` pull-off, applied to the note they prefix:
    `h1.7`
  - `pm` palm mute, and `pm<0-1>` for a given depth
  - `<>` natural harmonic, `*` pinch harmonic, `ah` artificial harmonic
  - `T` tap, `tr<f>` trill to fret f
  - `!` accent, `'` staccato, `lr` let ring, `g` ghost note
  - `v<1-127>` velocity
- **Strum:** a `D` / `U` / `Dx` (muted chuck) prefix on a chord, with
  `cv<n>` giving the crossing velocity in strings per second (default
  200, strum-dynamics.md).
- **Bass:** the prefixes `sl` (slap), `po` (pop), `th` (double thump) and
  `lh` (left-hand slap) become BASS_TECH events.
- **Bar lines:** each bar must add up to the meter exactly, or the
  generator fails and names the line and column.

### 2.3 Generator (`Tools/generate_factory_riffs.py`)

Run it from the repo root, in the same style as
`generate_factory_parts.py`. It:
1. Parses every `.riffdef`.
2. Validates the grammar, the bar sums, the fret ranges (0-24; 0-36 for
   tap), string counts per instrument and the technique legality below.
3. Checks the coverage rules of 2.1, and the name rules of section 12.
4. Writes `Resources/Riffs/<Genre>/<id>.luthierriff`.
5. Writes `Resources/Riffs/catalog.json`: every item's metadata, with no
   notes.
6. Writes `Resources/Riffs/free.txt`, the Free manifest.

It is deterministic, so a rerun gives byte-identical files. Keys are
sorted and reals are formatted with `repr` rounded to 6 decimal places.
`created` and `modified` are fixed to the `.riffdef` header's `date`,
never to the clock. CI runs the generator and then
`git diff --exit-code Resources/Riffs`.

Technique legality:
- Hammer, pull, trill and legato slide need a sounding note before them
  on the same string.
- Bends are limited to 3 semitones (1.5 on bass).
- Harmonics must be on a node fret (natural harmonics: 3, 4, 5, 7, 9, 12,
  16, 19, 24).
- A palm mute and a tap cannot be on the same note.

Batch `.mid` packs, for marketing and for review, come from
`luthier-render --export-riffs <dir> [--profile luthier|generic]`, a new
flag in `Tools/RenderCli.cpp`. It uses the same C++ path as drag-out.

## 3. File format: `.luthierriff`

Add this row to file-formats.md's section 1 table:

| Extension | Type | Magic | Owner |
|---|---|---|---|
| `.luthierriff` | Riff / lick / strum / bass line | `"magic": "luthier.riff"` | RiffLibrary |

```json
{
  "schema": 1,
  "magic": "luthier.riff",
  "meta": { "id": "blues.lick.delta-turnaround-3", "name": "Delta Turnaround 3",
            "author": "Factory", "origin": "original", "tags": ["turnaround", "box-1"],
            "created": "2026-09-01T00:00:00Z", "modified": "2026-09-01T00:00:00Z",
            "version_created": "1.3.0", "version_modified": "1.3.0", "notes": "" },
  "riff": { "type": "lick", "genre": "blues", "instrument": "guitar6",
            "tuning": [64, 59, 55, 50, 45, 40], "tuning_name": "Standard", "capo": 0,
            "key": { "root": "A", "scale": "minor_pentatonic" },
            "tempo_bpm": 84, "meter": [4, 4], "length_beats": 8, "feel": "shuffle",
            "difficulty": 2, "techniques": ["bend", "bendrelease", "slidelegato", "vibrato"],
            "chords": [ { "beat": 0, "symbol": "A7" }, { "beat": 4, "symbol": "E7" } ] },
  "notes": [ { "beat": 0, "dur": 0.333333, "str": 0, "fret": 8, "vel": 0.8,
               "tech": [ { "type": "bend", "value": 2, "curve": [[0, 0], [0.4, 2], [1, 2]] },
                         { "type": "vibrato", "value": 5.5, "second": 30 } ] } ],
  "strums": [ { "beat": 0, "dir": "down", "cv": 200, "mask": 63, "striker": "pick", "mute": 0 } ],
  "bass_tech": [ { "beat": 0, "str": 3, "tech": "slap", "pos": 0.5, "force": 0.8 } ],
  "source": "t8: 1.8b2~ 1.8 1.5 | ..."
}
```

- `str` is 0-based, with 0 the highest string (the PerformanceScore
  convention). The `.riffdef` notation's string 1 is `str` 0.
- `tech[].type` uses the wire tokens of `docs/MIDI_EXPORT_LUTHIER_PROFILE.md`
  section 6 and `MidiPerformance.cpp`'s `kTechniques`. `value`,
  `second` and `curve` have the meanings `ScoreTechnique` gives them.
  This one-to-one mapping is why the Luthier export is lossless
  (test RL-06).
- `riff.techniques` must equal the set of tech tokens in `notes`, plus
  `strum` and the bass techniques. The loader recomputes it and warns if
  it does not match. Filtering uses the recomputed set.
- **Instrument** is one of `guitar6`, `guitar7`, `guitar12`, `bass4`,
  `bass5`, `bass6`.
- **Scale** is a `ScaleType` name (`Practice/Trainers.h`: ionian ...
  blues), or `chromatic` for none.
- **Feel** is one of `straight`, `shuffle`, `swing`, `half_time`,
  `laid_back`, `driving`. Feel is a tag only: the stored timings already
  contain the feel.
- **Limits** (above them, the file is refused per file-formats 14):
  - at most 4096 notes
  - at most 64 beats
  - tempo 30-300
  - frets 0-36

**User riffs** live in `~/Documents/Luthier/Riffs/User/`. They use the
same schema, with `author` set to the user and `origin` set to `capture`
or `import`. Saves are atomic (file-formats 13). A user riff's `id` is
`user.<type>.<uuid>`.

`~/Documents/Luthier/Riffs/library.json` holds user-global data:
favourites (a list of ids), recents (the last 50 ids) and per-riff play
counts. It is not a preset.

Factory ids never change. A rename changes `name` only.

## 4. Library model (message thread and worker)

Proposed files: `Source/Riffs/` with `Riff.{h,cpp}`,
`RiffLibrary.{h,cpp}`, `RiffCompiler.{h,cpp}`, `RiffPlayer.{h,cpp}`,
`RiffTransposer.{h,cpp}` and `RiffAnalysis.{h,cpp}`.

- **`Riff`** is the metadata plus a `PerformanceScore` with one track
  (`Notation/PerformanceScore.h`), with `strums` and `bass_tech` kept
  alongside. Using PerformanceScore means the tab reader,
  `MidiPerformance::fromScore`, the notation exporters and the TAB view
  all take a riff without an adapter.
- **`RiffLibrary`** owns the index: `catalog.json`, plus a scan of the
  user folder on a `juce::ThreadPool` job at editor open, never at plugin
  construction. The index holds metadata only, and full riffs load
  lazily, with an LRU cache of 32.
  - Its source is `IrLibrary::getResourcesFolder().getChildFile ("Riffs")`
    for factory riffs, and the user folder.
  - Search and filter run on the message thread, over precomputed
    `std::bitset<64>` masks for technique, genre, type and instrument,
    plus a lower-cased name and tag string. Text search is a
    case-insensitive substring match over name, tags and genre, with
    AND between words.
- **`RiffAnalysis`** fills in the metadata for user riffs:
  - **Key:** Krumhansl-Schmuckler over duration-weighted pitch classes.
  - **Tempo:** from the capture.
  - **Techniques:** from the notes.
  - **Difficulty:**
    `clamp(1..5, round(1 + nps/3 + distinctTechniques/3 + max(0, fretSpan-4)/4))`,
    where nps is notes per second at the riff's tempo. For factory riffs
    the generator warns when the hand-set value differs from this
    estimate by more than 2.

## 5. Playback (audition) design

### 5.1 Compile (message thread)

`RiffCompiler::compile (const Riff&, const RiffPlaySettings&, const
GuitarSpecSummary&) -> std::shared_ptr<const CompiledRiff>` is pure and
deterministic. It:
1. Transposes and re-frets (5.2) for the target key and scale and the
   loaded guitar's string count, tuning and capo.
2. Turns beats into a sorted, immutable array of POD `RiffEvent`s, all
   positioned in beats:
   - **NoteOn:** string, fret, pitch, velocity, `Technique`,
     harmonicPartial, slideFrom / slideBeats, and palm-mute depth.
   - **NoteOff:** carries letRing.
   - **Bend points:** per string, in cents. Bends, bend releases,
     prebends, whammy and vibrato are all expanded into breakpoint
     curves here, and vibrato becomes a sine at its rate and depth after
     its delay.
   - **Strums:** staggered into per-string onsets, 1/cv seconds apart in
     stroke order. This spacing is converted to beats at play time,
     because it is fixed in seconds.
   - **BASS_TECH:** carried as `NoteOnEvent::bassTechnique`.
3. Maps techniques onto the engine's `Technique` enum:
   - hammer → HammerOn, pull → PullOff, tap → Tap
   - pm → PalmMute, dead → MutedPick
   - natural / artificial / tapharm → NaturalHarmonic, with
     harmonicPartial set from the fret
   - pinch → PinchHarmonic
   - slidelegato / slideshift / slideup / slidedown → Slide, with
     slideFromFret set
   - slidein → Slide from fret-3 over 60 ms
   - slideout → a Slide note-on to max(1, fret-5) over the last 25% of
     the note, marked no-pluck
   - ghost, accent and staccato → a velocity of ×0.45, ×1.2 or a
     duration of ×0.5
   - trill → alternating HammerOn / PullOff at 12 Hz

   Anything not listed plays as Pluck.

Two additions to the engine's `NoteOnEvent` (`PlayingEvents.h`), both
appended to the struct:
- `double palmMuteDepth = -1.0`. At -1 the engine uses
  `TechniqueEngine::getPalmMuteAmount()`. The engine reads it at the
  `Technique::PalmMute` damping switch in
  `LuthierEngine::triggerNote` (around line 1008).
- `bool explicitArticulation = false`, which is true for riff notes.

### 5.2 Transposition and re-fretting (`RiffTransposer`)

**Target key.** The target is a root (from 12) plus an optional scale.
The shift d is the smallest one in -6..+6 that takes the riff's root to
the target root.

**Scale remapping** happens only when both scales have 7 notes, or both
are pentatonic, and the user chose "Map scale" (default off):
- A note on a scale degree keeps its degree.
- A note off the scale keeps its semitone offset from the nearest degree
  below it.
- Chromatic riffs never remap.

**Placement** is deterministic and runs in this order:
1. For each candidate shift in {d, d-12, d+12}, move every note along
   its own string. Accept the first candidate where every fret is in
   0..maxFret of the loaded guitar, and prefer the smallest |shift|.
2. If no candidate fits, move whole legato chains (notes joined by
   hammer, pull, legato slide or trill) to the neighbouring string that
   keeps the pitch and puts the fret in range. The string with the
   smaller fret wins.
3. If some notes still do not fit, drop them. The UI then shows "N notes
   did not fit this guitar" (section 7.5).

**Tuning, string count, capo and family.** Placement always runs against
the loaded guitar, by pitch. For example, a Drop D riff on a standard
tuned guitar re-frets the low string by +2.
- A bass line played on a guitar goes up an octave, onto strings 3-6.
- A guitar riff played on a bass goes down an octave, onto the strings
  that fit.

Both show a one-line notice. The "Fits this instrument" filter
(default on) hides riffs that would need either.

**Whammy** events on a guitar with no whammy fitted play as bends. The
notice says "played as bends: no whammy fitted".

### 5.3 Play (audio thread): `RiffPlayer`

`LuthierEngine` owns the `RiffPlayer` and a `PlayEventQueue riffEvents`.
It is hooked into `LuthierEngine::processSubBlock`, straight after the
direct events are scheduled (around the
`midi.processBlock (*directForSubBlock, ...)` line, 1927):

```cpp
riffEvents.clear();
riffPlayer.renderSubBlock (numSamples, hostPpq, hostPlaying, tempoBpm, riffEvents);
scheduleEvents (riffEvents, numSamples);
```

- **Hand-over.** It follows `TunePlayer`'s pattern:
  - The message thread calls `setCompiled (shared_ptr)`, which fills a
    waiting slot under a `SpinLock`.
  - The audio thread swaps it in with `ScopedTryLock` at the next beat
    (or at once while stopped).
  - The displaced riff goes to a retired slot, which the processor's
    timer empties (`collectGarbage`).
  - The audio thread never frees anything. Transport commands (play,
    stop, loop, start quantise) are atomics.
- **Clock.** `Auto` is the default: the host clock while the host plays
  (the engine's hostPpq and tempo, which the processor already sets from
  the host or from a running tune, PluginProcessor.cpp around 1136),
  otherwise the player's own clock at the chosen tempo. `Own` always
  uses the player's own clock.
  - On the host clock, a start waits for the next bar line of the host
    (next beat if "Start: next beat" is chosen). A loop wraps on
    whole-bar boundaries.
  - A host stop ends the riff.
  - A host jump ends every riff note and re-locates by ppq.
- **Tempo** comes from the host in Auto. Otherwise it is the riff's own
  tempo × a tempo factor (0.25-2.0, default 1.0), or an absolute bpm.
  Strum spacing stays fixed in seconds.
- **Bends** are sent as `BendEvent`s: at most one per bending string per
  64 samples, linearly interpolated from the breakpoints. Riff events
  may use at most 96 of the queue's 192 slots per sub-block. Beyond
  that, bend points are dropped (never notes), and an overflow counter
  goes to Diagnostics.
- **Stopping never leaves a note hanging.** Stop, panic, a new riff, a
  preset load (`engine.reset()`) or a guitar change ends every note the
  player started, on its own string. `LuthierAudioProcessor::panic()`
  also calls `riffPlayer.stop()`.
- **Capture and output.** Riff notes go through `triggerNote`, so the
  performance capture, fretboard, piano roll, chord name and meters all
  see them exactly as they see played notes. They do not reach the
  live MIDI out (a preview is not a performance). They are merged after
  MIDI Learn, so they can never be learned.
- **Level.** Audition plays at the riff's velocities through the full
  rig. An "Audition level" trim of -24..0 dB (UI state) scales velocity,
  not audio, so the tone of the rig is unchanged.

### 5.4 Interactions at the engine level

- **Rhythm engine:** riff events bypass the interpreter and the voicer,
  like the tune's direct notes. So with the rhythm engine on, the riff
  plays over the strumming, and a riff played this way does not change
  the detected chord.
- **Auto-articulation:** it must skip every note with
  `explicitArticulation` set. For imported Luthier-profile files, the
  NOTE flags and CC67/68 count as explicit.
- **Technique cascade** (technique-cascade.md) still resolves conflicts
  on a string. A riff note is a normal note, so a user slide or tap on
  the same string overrides it, last event wins.
- **Playing mode** (Mono / Poly / Chord) does not affect riffs, because
  riff notes are already placed.

## 6. Destinations

1. **Drag to DAW.** The row has a drag handle, and the preview has a
   "Drag .mid" tile. On drag start:
   1. The riff is compiled at the current key and the tempo shown (the
      file's tempo map carries it).
   2. It becomes a `MidiPerformance` through `MidiPerformance::fromScore`,
      plus the STRUM and BASS_TECH events. `addScoreNote` must also
      write CC67 for `pm`, CC72 for `pinch` and CC73 for `natural`, as
      `TuneMidi` does for pm. This is a coordination point: today the
      pm / pinch / harmonic flags are metadata only, and a DAW playing
      the file back would lose them.
   3. It is written with `MidiProfiles::exportToMemory` to
      `~/Documents/Luthier/Riffs/Drag/<Name> - <Key> <bpm>.mid`, then
      handed to `performExternalDragDropOfFiles`, as
      `MidiOutPanel.cpp:144` does.
   4. The profile comes from a "Drag as" toggle (Luthier / Generic) that
      defaults to the Options -> MIDI default profile. Alt forces
      Generic (midi-export 4.2). The split is `single` and the PPQ is
      the Options default.
   5. Files in `Drag/` older than 30 days are pruned on startup.

   The keyboard equivalent is "Save .mid..." (Ctrl+E while the RIFFS tab
   has focus).
2. **Add to Tune.** Available from the button, or by dragging the row
   onto the TUNE tab's melody strip (a `DragAndDropTarget` inside the
   editor).
   - Guitar riffs go into the selected section's `MelodyTrack` and bass
     lines into its `BassTrack`, which is set to `manual`. They start at
     the drop beat or at the section's start, transposed to the tune's
     key, and are clipped to the section length.
   - Technique mapping:
     - bend, slide, hammer, pull, vibrato and harmonic map to
       `NoteTechnique`
     - pm, letring and staccato map to `NoteArticulation`
     - ghost and accent go into velocity
   - The exact string, fret and full tech list go into
     `MelodyNote::extra` (`riff_str`, `riff_fret`, `riff_tech` as JSON).
     This is a coordination point: TuneMidi should prefer `extra` when
     it is present, so tune playback matches the audition.
   - Unlocked notes in the covered range are replaced. Locked notes stay,
     and riff notes that overlap a locked note are dropped.
   - It is one `TuneSession::edit (TuneEditClass::melodyEdit, "Insert
     riff <name>", ...)`.
3. **Send to Looper.** Calls `MidiImportTargets::importPerformance
   (processor, performance, MidiImportTarget::looper, name)`, the same
   path as midi-export 5's looper import, looped to whole bars at the
   current tempo.
4. **Learn It.** Opens the practice drawer (it expands if collapsed) on
   the TAB tab, through a new `TabReaderTab::openScore (const
   PerformanceScore&, const juce::String& title)`, with section loop on,
   tempo at 70%, and the speed trainer armed to +5% per clean pass.
5. **Piano roll** (piano-roll-chord-display.md): audition notes light
   the keys through `SoundingNotes`, with nothing extra needed. The RIFFS
   preview also has a static notes view of its own (7.2).

## 7. UI

### 7.1 Location

- **Advanced:** a new Col 4 tab, **RIFFS**, placed between TUNE and LIVE:
  `WORKSHOP | MOD | RHYTHM | TUNE | RIFFS | LIVE | ...`. It sits next to
  the composition tab it feeds. Tabs are remembered by name (DECISIONS:
  "workspace tab is now remembered by name"), so inserting it moves no
  saved state.
- **Easy:** a **Riff drawer**. It slides in from the right over the rig
  strip: 320 px wide, the full height of the main area, with a 150 ms
  ease (none under reduced motion). It opens from a new **Riffs** button
  in the Easy rhythm strip (3.5) and closes with Escape or the button.
- **Shortcut:** `R` (rebindable). In Advanced it selects the RIFFS tab,
  and in Easy it toggles the drawer. `R` is currently unbound
  (gui-integration 17).
- Options -> FILE LOCATIONS gains "Riffs folder".

Rows for gui-integration.md section 19 (for the coordinator):

| Feature | Backend | Primary | Secondary | Shortcut |
|---|---|---|---|---|
| Riff library browse / audition | RiffLibrary / RiffPlayer | Col 4 RIFFS | Easy Riff drawer | R |
| Riff drag-out / Add to Tune / Send to Looper / Learn It | RiffLibrary | Col 4 RIFFS preview | Easy drawer (all except Add to Tune) | Ctrl+E (save .mid) |
| Save phrase as riff | RiffLibrary | Col 4 RIFFS "+" | Col 4 NOTATION capture | - |

### 7.2 RIFFS tab layout (Col 4, at least 480 px wide)

```
+--------------------------------------------------------------------+
| RIFFS  [search..........]  [Fits this instrument v] [+ Save riff]  |
| Genre chips: Rock Blues Metal Funk Country Jazz Folk Reggae ...    |
| Type [All v] Diff [1..5] Tempo [40..240] Key [Any v] Tech [+ v] *Fav|
+-------------------------------+------------------------------------+
| LIST (virtualised ListBox)    | PREVIEW                            |
| * Delta Turnaround 3  lick  A | Delta Turnaround 3   Blues - lick  |
|   84  ##---  bend vib slide   | [tab view, 4-6 lines, playhead]    |
| ...                           | [Play/Stop] [Loop] Start[next bar] |
|                               | Key [A v] Scale[map] Tempo[Auto 84]|
|                               | Level [-6 dB]                      |
|                               | [Drag .mid][Add to Tune][Looper]   |
|                               | [Learn It]  Drag as [Luthier|Gen]  |
+-------------------------------+------------------------------------+
| 312 riffs - 41 shown                                    status line |
+--------------------------------------------------------------------+
```

- **List columns:** favourite star, name, type, key, tempo, a difficulty
  bar (5 pips plus the number) and technique glyphs, each with a text
  tooltip.
  - Sort by name, tempo, difficulty, key or recent. The default is
    genre, then name.
  - Below 640 px, the preview stacks under the list.
- **Tab view** is the new `RiffTabView`, which draws the compiled
  placement (so the frets are the ones that will sound), with technique
  marks in the notation-export ASCII style. The playhead is driven by
  the player's atomic beat position, drained at 30 Hz. The stale rule is
  gui-engine-dataflow's: the playhead hides after 250 ms without an
  update.
- **Controls:**
  - Clicking a row selects it. Space, Enter or Play auditions it. With
    "Audition on select" on (Options -> General, default off), selecting
    a row also auditions it.
  - Up and Down while playing move to the next riff and audition it.
- **States:**
  - **Playing:** Play shows Stop, and the row shows a speaker glyph.
  - **Waiting for the bar:** Play pulses. Under reduced motion it shows
    the text "waiting" instead.
  - **Loading:** a spinner row.
  - **Locked (Free):** section 11.

### 7.3 Easy Riff drawer

The same controls in one column:
- search
- a genre dropdown, a Type dropdown and a Difficulty chip row
- a list of 8 visible rows
- the preview: name, Play / Loop, Key, Tempo Auto/own
- Drag .mid, Looper and Learn It

Add to Tune is absent because Easy has no TUNE tab (gui-integration 0.7:
present or absent).

### 7.4 Save as riff

Opened from "+ Save riff" (RIFFS) or "Save as riff" (NOTATION capture).

- **Source:** the capture's marked region (`PerformanceCapture::
  getMarkedRegion`), or else the last N bars (default 2) of the take.
- **Quantise:** default 1/16 at strength 1.0, `CaptureScoreOptions`.
- **Fields:** name (required), type, genre, instrument (from the
  guitar), key and scale (auto, editable), tempo (from the capture),
  difficulty (auto), feel and tags.
- Save writes the file atomically and selects the new riff. User riffs
  add Edit info, Duplicate, Reveal and Delete to the preview. Delete
  asks for confirmation and moves the file to the OS trash.
- **Import .mid as riff...** sits in the "+" menu. Luthier-profile files
  keep their techniques. Generic files get string placement from
  `RubricVoicer`.

### 7.5 Empty states and errors

- **No match:** "No riffs match. Clear filters" (the text is a link).
- **User filter with no user riffs:** "Play something, mark it in the
  capture, then + Save riff."
- **Riffs folder missing:** "Factory riffs not found. Reinstall or check
  Options -> File Locations." Logged under `ErrorLog` category Content.
- **Unreadable user riffs:** one banner, "N riffs could not be read"
  with a Reveal action. The files are left untouched (file-formats 14).
- **Placement notices** (5.2) appear in the preview's status line, not
  as banners.
- **Drag write failure:** a banner giving the path and the OS error.

## 8. State and serialization

- **Plugin state:** nothing in presets or snapshots. A preset or snapshot
  load never changes the library view or the selected riff.
- **Per instance** (`uiState`, saved with the host project):
  - selected riff id
  - filters and search text
  - sort
  - key, scale-map, clock mode, tempo factor or bpm, loop, start
    quantise and level
  - Drag-as profile
  - Easy drawer open
  - preview split

  An unknown id on restore selects nothing, silently.
- **Session:** whether audition is playing is never restored. A session
  reopens stopped.
- **User-global:** `library.json` (3) and the Options toggle.
- **Offline render:** audition is excluded from `AudioExporter` renders
  and `luthier-render`, so it cannot make a render non-deterministic. A
  riff sent to the tune or the looper is ordinary data there. Compiling
  is pure: the same riff, settings and guitar give byte-identical
  `CompiledRiff` bytes and an identical `.mid` file (RL-04, RL-07).

## 9. Undo (action-and-undo.md)

**Not undoable** (these are view or session actions, 3.17 / 7):
browsing, search, filters, audition play or stop, key, tempo and loop
settings, favourites and drag-out.

**Undoable:**
- **Add to Tune:** one `tune-melody-edit` entry, "Insert riff <name>",
  never grouped.
- **Send to Looper:** follows the looper's layer rules (3.14). The new
  layer is removed with the layer's own undo, not the global stack.

**File operations** (save, edit info, delete user riff) are file
operations like preset save: not on the stack, with delete protected by
confirmation and the trash.

## 10. Accessibility

- **List:** a `ListBox` with the table role. Each row's accessible name
  reads "Delta Turnaround 3, lick, Blues, A minor pentatonic, 84 bpm,
  difficulty 2 of 5, bend, vibrato, slide, favourite". Typing a letter
  jumps to the next name that starts with it.
- **Filter chips** are toggle buttons that announce their state.
  Difficulty and tempo are dual-thumb sliders with value text.
- **Tab view:** its accessible description is a text rendering of the
  notes: "Bar 1: string 1 fret 8 bend whole step, ...". It is capped at
  2 bars per announcement, with Ctrl+Down to read the next bars.
- **Audition** start and stop are polite announcements: "Playing Delta
  Turnaround 3 in E, 96 bpm, looping".
- **Drag has keyboard equivalents:** Save .mid..., Add to Tune and Send
  to Looper are buttons, and all reachable by Tab.
- The Easy drawer takes focus when it opens and returns it to the Riffs
  button on close (accessibility 1).
- Every string, the tag vocabulary, genre names and technique names are
  in the locale catalog. Factory riff names are keyed
  `riff.<id>.name`, with the English `meta.name` as fallback. The
  generator writes `Resources/Riffs/strings.en.json`.
- Nothing relies on colour alone: the difficulty pips carry a number,
  and the technique glyphs carry tooltip text.

## 11. Editions (editions.md)

**Free-limited.** Browsing and hearing riffs is a fundamental, and it
sells the sound. The destinations that depend on headline features stay
Pro.

| Capability | Free | Pro |
|---|---|---|
| Browse the full catalog (names, tags) | Yes. Non-free rows show the D-2 lock glyph and open the upsell panel | Yes |
| Audition, transpose, tempo, loop | The 60-item free set | All |
| Send to Looper | Yes, within the Free looper limit (1 layer, 60 s) | Yes |
| Drag / Save .mid | No (MIDI export is H5) | Luthier and Generic |
| Add to Tune | No (H2) | Yes |
| Learn It | No (tab reader is Pro) | Yes |
| User riffs (save / import) | No | Yes |

Implementation:
- Append `riffLibraryFull`, `riffExport` and `userRiffs` to
  `edition::Feature`, and `freeRiffs = 60` to `Limits`.
- The Free installer ships `catalog.json`, `strings.en.json` and only the
  files in `free.txt`, through the content manifest of editions 3.
- A Pro user riff opened in Free (for example from a synced folder) is
  listed as locked, never modified.

## 12. Legal and naming

- Every `.riffdef` item carries `origin original` and an author initial.
  The generator refuses anything else.
- **Names** follow `[Descriptor] [Figure] [n]`, under 32 characters:
  - no artist, band, song or album names
  - no "in the style of"
  - no trademark (`Tools/trademark_scan.py` also scans
    `Resources/Riffs/catalog.json`)

  An artist and title deny-list in `Tools/riffs/deny.txt` (about 400
  entries, maintained by legal review) is checked case-insensitively
  against names and tags.
- **Similarity guard.** No two factory items may share more than 80% of
  their interval-and-rhythm 6-grams. This catches copy-paste within the
  library. It does not replace the human legal sign-off that
  factory-content 13 already requires for every named entry.

## 13. Performance budget

- **RiffPlayer:** at most 0.02 units (event walk plus bend interpolation)
  for a 16th-note riff with 3 simultaneous bends. It allocates nothing
  and locks nothing (it only uses `ScopedTryLock` for the swap).
- **Compile:** at most 2 ms per 8-bar riff on the message thread.
- **Library:** index load at most 150 ms for 300 factory plus 1000 user
  riffs (worker). Filter or search at most 8 ms for 1300 items (message
  thread). List repaint within the 2 ms live-overlay budget.
- **Memory:** index at most 3 MB for 1300 items. The cache holds 32
  riffs at up to 64 KB each. Factory content is about 1.2 MB on disk.
- **Boot:** nothing loads until the RIFFS tab or the drawer first opens,
  so performance-budget 5 does not change.

## 14. Interactions

- **Tune Builder:** section 6.2. A riff auditioned while a tune plays
  follows the tune's clock through the engine transport. "Key: Tune"
  becomes an option in the Key menu whenever a tune is loaded.
- **Jam mode** (written in parallel): jam mode may call `RiffLibrary::
  query (RiffQuery)` and `RiffPlayer` with `Key: Jam`, following the
  band's clock and key through the same engine transport. Riffs over
  the band use exactly the audition path.
- **Auto-articulation:** 5.4.
- **MIDI export / import:** 6.1 and 7.4. RL-06 tests round-trip
  fidelity.
- **Snapshots / presets / host automation:** a riff keeps playing
  through a snapshot recall, so the player can A/B tones on the same
  riff. On a preset load the riff's notes end (engine reset) and
  playback continues from the next event. No parameter is touched.
- **Techniques tab / slide mode:** slide notes in a riff are fretted
  slides (`Technique::Slide`), not bottleneck slides. With Slide Mode on,
  riffs still play fretted.
- **Practice:** the metronome follows the same tempo when it is on.
  Learn It uses the tab reader. The looper takes riffs as layers.
- **Workshop:** a guitar swap recompiles the current audition for the
  new string count, tuning and fret count at the next beat.
- **Piano roll / chord name:** automatic through string activity.

## 15. Failure modes

| Failure | Response |
|---|---|
| Corrupt or oversized `.luthierriff` | Skipped; one aggregated banner; ErrorLog entry; file untouched |
| `catalog.json` missing but files present | Rebuilt in memory by scanning (slower), one diagnostics line |
| Riff does not fit the guitar | Placement fallbacks (5.2); notice with the dropped-note count |
| Event queue pressure | Bend points dropped first, never notes; overflow counter |
| Host tempo 0 or missing | Own clock at the riff's tempo |
| Sample-rate change mid-audition | Compiled data is in beats; the player re-locates, no retime needed |
| Drag target rejects the file | The file stays in `Riffs/Drag/`; status line offers Reveal |
| Free build with a Pro-only riff id in uiState | Selection cleared; no banner |

## 16. Tests

Unit tests go in `LuthierTests` in the new `Source/Tests/RiffTests.cpp`.
GUI tests under xvfb go in `Source/Tests/RiffPanelTests.cpp`, in the
style of `EditorTests.cpp`.

- **RL-01 Generator determinism:** running
  `Tools/generate_factory_riffs.py` twice gives byte-identical
  `Resources/Riffs`, and the committed tree equals the output (CI
  `git diff --exit-code`).
- **RL-02 Coverage:** the catalog has at least 300 items and matches the
  per-genre and per-type table of 2.1 exactly. It meets every rule of
  2.1: difficulty spread, technique minimums, Drop D / 7-string / 5-string
  counts and at least 20 per genre.
- **RL-03 Schema:** every factory file loads. The recomputed techniques
  equal `riff.techniques`. Save then load round-trips byte-identical
  (file-formats 16). A 10 000-mutation fuzz either loads or refuses
  cleanly, with no crash.
- **RL-04 Compile purity:** compiling the same riff, settings and guitar
  twice gives equal `CompiledRiff`s. Compiling on two threads at once
  gives equal results.
- **RL-05 Technique mapping:** for a fixture that has each tech token,
  the compiled NoteOn has the expected `Technique`, `harmonicPartial`,
  `slideFromFret` and `palmMuteDepth`. The bend curve reaches ±1 cent of
  the stated semitones at the stated positions.
- **RL-06 Luthier round trip:** for every factory riff, compile, then
  `fromScore` and Luthier-profile export, then import and `toScore`.
  Every note's string, fret, beat (±1 tick at 960 PPQ) and technique set
  is equal. The render of the imported file and of the audition, on the
  same preset, nulls to -60 dBFS RMS or better.
- **RL-07 Drag file:** the drag produces a valid SMF in `Riffs/Drag/`.
  It has a LUTHIER header in the Luthier profile and none in Generic
  (Alt), and the same inputs give identical bytes. CC67 is present for
  pm notes in both profiles.
- **RL-08 Transpose:** an A minor-pentatonic lick to E gives d = -5, all
  frets in range, same strings. An open-string riff (`[x32010]`) moved
  up 1 takes the +1 candidate. A riff that forces the octave fallback
  gives the d-12 or d+12 candidate. Every result is deterministic.
- **RL-09 Scale map:** an ionian lick mapped to aeolian lowers degrees
  3, 6 and 7 by one semitone and keeps chromatic passing tones at their
  offsets. Chromatic riffs are unchanged.
- **RL-10 Re-fret:** a Drop D riff on a standard guitar sounds the same
  pitches. A bass line on a guitar is exactly +12 semitones. A 7-string
  riff on a 6-string moves string-7 chains or drops notes, and reports
  the count.
- **RL-11 No allocation / no lock:** 10 minutes of looped audition with
  riff swaps every 500 ms on a message-thread thread shows zero
  audio-thread allocations (heap hook) and no blocking lock.
- **RL-12 Clock, host:** with a host playhead at 120 bpm, a start
  requested at beat 2.3 sounds its first note at the sample of beat 5.0
  (the next bar), ±1 sample. Loops restart on bar lines for 16 bars
  with no drift beyond 1 sample.
- **RL-13 Clock, own:** host stopped, tempo factor 0.5 on an 84 bpm riff
  gives note onsets at 42 bpm spacing (±1 sample). Strum stagger stays
  1/cv seconds.
- **RL-14 Stop hygiene:** stop, panic, a new riff mid-note, a preset
  load and a guitar swap each leave zero sounding riff strings within
  one block, and no bend offset remains on any string.
- **RL-15 Queue pressure:** a synthetic riff with 12 strings, each
  bending, at a 4096-sample block uses at most 96 queue slots, keeps
  every note-on, and increments the overflow counter.
- **RL-16 Explicit articulation:** every riff note-on reaching
  `triggerNote` has `explicitArticulation == true`. Voicer and playing
  mode changes do not alter the string or fret played.
- **RL-17 Rhythm engine coexistence:** with the rhythm engine on and a
  chord held, riff notes sound, and the chord detector's symbol is
  unchanged by them.
- **RL-18 Capture:** auditioning a riff with the capture rolling produces
  a take whose score equals the compiled riff: string, fret and
  technique, with onsets ±1 ms. Live MIDI out receives no riff events.
- **RL-19 Add to Tune:** inserting into a section with two locked notes
  keeps them byte-identical and drops overlapping riff notes. It is one
  undo entry, and undo restores the section exactly. Bass lines land in
  `BassTrack` with mode `manual`. `extra` carries `riff_str`,
  `riff_fret` and `riff_tech`.
- **RL-20 Looper:** Send to Looper creates one layer whose length is the
  riff's whole bars at the current tempo (±1 sample), and whose audio is
  non-silent. In a Free build, the 60 s limit is respected.
- **RL-21 Learn It:** the practice drawer opens on TAB, and
  `TabReaderTab::getScore()` equals the compiled score. Loop is on and
  tempo is 70%.
- **RL-22 Save as riff:** from a fixture capture (C major phrase, 100 bpm,
  with a bend and a hammer-on), the saved riff has key C ionian, tempo
  100, techniques {bend, hammer}, and difficulty equal to the formula of
  section 4. It is atomic when killed mid-save (file-formats 16).
- **RL-23 Search and filter:** over 1300 items (300 factory plus 1000
  generated), filtering by genre, technique, difficulty and tempo
  returns exactly the brute-force set, in 8 ms or less. Text search
  "turn blu" matches "Delta Turnaround 3" (AND over words).
- **RL-24 Legal:** no catalog name or tag matches `deny.txt` or the
  trademark list. Every item has `origin` original or factory. No pair of
  items exceeds 80% 6-gram overlap.
- **RL-25 GUI reach (xvfb):** in Advanced, `setWorkspaceTabNamed
  ("RIFFS")` shows the panel, which sits between TUNE and LIVE. `R`
  selects it. In Easy, `R` and the Riffs button open the drawer, and
  Escape closes it with focus returned to the button. At 1280 x 800 and
  at the minimum sizes of gui-integration 13, nothing clips.
- **RL-26 Keyboard and screen reader:** Tab visits search, then chips,
  then filters, then the list, then the preview controls with no skip.
  Space toggles audition, and Ctrl+E opens Save .mid. Every row and
  control has a non-empty accessible name and value, and the row name
  contains type, key, tempo and difficulty.
- **RL-27 Empty and error states:** a forced empty filter, a missing
  Riffs folder and a corrupt user file each show their exact hint or
  banner of 7.5. A corrupt file keeps its original bytes.
- **RL-28 State:** selected riff, filters, key, tempo mode and Drag-as
  survive editor close and reopen and a host state save and restore.
  They are absent from a saved `.luthierpreset`. Playback is stopped
  after a restore.
- **RL-29 Edition gate:** in a Free build, 60 rows are playable and the
  rest show the lock glyph and open the upsell. Drag, Add to Tune,
  Learn It and "+" are locked. Send to Looper works.
- **RL-30 Performance:** RiffPlayer costs at most 0.02 units, measured
  per performance-budget 0.2. Compiling an 8-bar riff takes at most 2 ms.
  The index loads 1300 items in at most 150 ms.
- **RL-31 Combination:** with Tune playing, rhythm engine on, a snapshot
  recall every 2 s, and Slide Mode toggled, 60 s of riff audition show
  no stuck notes, no NaN, and no dropped note-ons. The riff's onsets
  stay locked to the tune's bars (±1 sample).
