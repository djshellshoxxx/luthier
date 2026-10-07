# Guitar TAB import / export (FEAT2-TAB)

Status: implemented on top of the existing feat2-notation infrastructure. This
document records the research behind the tab engine and the decisions taken, so
the reader knows *what* the plugin reads and writes and *why* the timing of an
imported ASCII tab is what it is.

The golden rule (notation-export 0, rule 2): a guitar score is not a list of
pitches. A note knows the **string** it was played on and the **fret** — E on
string 3 fret 9 and E on string 4 fret 14 are the same pitch and a different
piece of music. Every importer here preserves string+fret and never re-derives
it from pitch.

## 0. Reuse map (no duplication)

The tab engine is an adapter layer over systems that already existed:

| Concern | Existing system reused |
| --- | --- |
| Internal representation | `PerformanceScore` / `ScoreNote` / `ScoreTechnique` (`Source/Notation/PerformanceScore.h`) |
| ASCII / MusicXML / GP / MIDI export | `NotationExporter` (`Source/Notation/NotationExport.*`, `AsciiTabWriter.*`) |
| ASCII / MusicXML import | `NotationImporter` (`Source/Notation/NotationExport.cpp`) |
| Techniques (bend/tap/mute/slide, hammer/pull, harmonics, vibrato…) | `Source/DSP/Techniques/*`, driven through `Technique` on `RiffEvent` |
| Tuning + capo | `TuningEngine`, `GuitarSpecSummary` (`Source/Riffs/RiffTransposer.h`) |
| Playback / transport | `RiffCompiler::compile` → `RiffPlayer` (`Source/Riffs/*`); offline render via `RiffDestinations::render` |
| MIDI import ("Luthier profile") | `Source/Export/MidiPerformance.*`, `MidiProfiles.h`, `docs/MIDI_EXPORT_LUTHIER_PROFILE.md` |

The tab engine adds only: a hardened, technique-aware ASCII reader; a
`Riff::fromScore` adapter so a parsed score can be played and rendered; and a
"Play" control in the existing tab-reader panel. It does **not** add a new
scheduler, new technique DSP, or a second MusicXML/MIDI codec.

## 1. How guitar tabs work

A tab is N string lines (6 for guitar, 4 for bass, etc.), highest string on top,
lowest on the bottom. A number on a line is the fret to play on that string; `0`
is the open string. Read left to right in time.

### ASCII / plain-text tab (the ubiquitous web format)

ASCII tab is a *human-readable*, not machine-readable, convention: there is no
single grammar, and bar lines, rhythm, bends and chord names are present,
absent or spelled differently from tab to tab. The stable parts everyone
agrees on: hyphens (`-`) fill the string lines, digits are frets, `|` is a bar
line, and the lowest string is at the bottom.

Techniques, as seen across sources (Wikipedia "ASCII tab", Guitar Pro's ASCII
export, teaching sites):

| Symbol | Meaning |
| --- | --- |
| `h` | hammer-on (`7h9`) |
| `p` | pull-off (`9p7`) |
| `b` | bend (sometimes `^`); `b` may carry a target/amount |
| `r` | release of a bend |
| `/` | slide up |
| `\` | slide down |
| `~` | vibrato (sometimes `v`) |
| `x` | dead / muted note |
| `PM` / `P.M.` | palm mute, usually with a dotted span above the affected notes |
| `t` | tap |
| `tr` | trill |
| `*` / `<n>` | natural harmonic |
| `[n]` | pinch / artificial harmonic (convention varies) |
| `(n)` | ghost / grace note |
| chord name above the staff | e.g. `Am`, `G` |
| header lines | tuning (`Tuning: E A D G B E`), tempo, capo |

**Timing is approximated by horizontal spacing.** ASCII tab carries *no exact
durations*: two notes closer together on the line are meant to be closer in
time, and that is all. Any player must infer timing. The heuristic used here
(and the one Luthier's own writer round-trips against):

- The staff is read as a **grid of columns**. Luthier's writer lays a
  sixteenth-note grid (four columns per beat) and inserts extra columns for
  off-grid onsets; two-digit frets widen their slot on every string so columns
  stay aligned.
- The reader maps **column position within a measure → beat**: four columns to
  a beat, counting from the bar line, so a note's beat is
  `measureStartBeat + columnsSinceBar / 4`. Widened second-digit columns carry
  no time.
- Tempo defaults to **120 bpm** and metre to **4/4** unless a `Tempo:` /
  time-signature header says otherwise. The user changes tempo at playback time
  (own-clock BPM on the `RiffPlayer`), which is the only honest way to play a
  format that never stated a tempo.
- Note duration is not expressed by ASCII either; imported notes get a short
  default (a sixteenth) and let-ring / sustain is left to the engine. This is
  the documented lossy edge of the format.

### Formats WITH real timing

- **MusicXML tablature** — the reliable-playback format, and Luthier already has
  a MusicXML codec from feat2-notation, so it is reused. Notes carry
  `<duration>` against `<divisions>`, `<technical><string>`/`<fret>` for the
  fretboard position, and `<technical>` children for techniques: `<bend>` with
  `<bend-alter>` (steps; negative or `<pre-bend>`/`<release>` for prebends and
  releases; `<with-bar>` for whammy), `<hammer-on>`, `<pull-off>`, `<harmonic>`
  (`<natural>`/`<artificial>`), plus `<slide>`/`<glissando>` under `<notations>`.
  Fret 0 = open. This is the preferred import for anything that must play back
  with correct rhythm.
- **MIDI** — real timing, but string/fret is not in standard MIDI. Luthier's
  own **MIDI export ("Luthier profile")** encodes string/fret and techniques in
  a documented way (`docs/MIDI_EXPORT_LUTHIER_PROFILE.md`), and the existing
  `MidiPerformance` / MIDI importer round-trips it. Reused as-is; the tab engine
  does not touch MIDI.
- **Guitar Pro** (`.gp3/.gp4/.gp5/.gpx/.gp`) — what the target audience actually
  uses, but the format is proprietary. `.gp3/4/5` are version-specific binary;
  `.gpx/.gp` are zipped bundles of undocumented XML (GPIF). Byte-exact parsing
  means reverse-engineering a moving target.

## 2. Supported formats (decision)

| Format | Import | Export | Notes |
| --- | --- | --- | --- |
| ASCII / plain text (`.txt`, `.tab`) | ✅ (hardened here) | ✅ (`AsciiTabWriter`) | MUST. Timing inferred by column spacing; techniques parsed. |
| MusicXML tab (`.xml`, `.musicxml`) | ✅ (reused) | ✅ (reused) | Preferred for reliable playback: real durations. |
| MIDI (`.mid`) | ✅ Luthier profile (reused) + generic files fingered (§8) | ✅ (reused; bends, slides, vibrato as pitch bend, §8.3) | Real timing; string/fret via the Luthier profile, or guessed. |
| Guitar Pro 3/4/5 (`.gp3/.gp4/.gp5`) | ✅ `GuitarProLegacyReader` (one track, repeats unrolled, partial files kept) | ❌ | Binary; bounded reads, never crashes on damage. |
| Guitar Pro 7/8 (`.gp`) | ✅ (GPIF) | ⚠️ GPIF bundle (existing, best-effort) | |
| Compressed MusicXML (`.mxl`) | ✅ (container.xml, size-capped) | ❌ | |
| Guitar Pro 6 (`.gpx`), PowerTab (`.ptb`) | ❌ friendly error | ❌ | Proprietary containers; the message says to Save As `.gp`/`.gp5` or export MusicXML. |

**Guitar Pro.** `.gp3/.gp4/.gp5` (binary) are read by `GuitarProLegacyReader`: every
field read is bounds-checked, counts are capped, a truncated or damaged file keeps
the bars read so far (with a warning), and one track becomes the score (the first
pitched track, or `NotationImporter::setPreferredTrack`). Repeats and alternative
endings are unrolled. `.gp` (GP7/8) is read as GPIF. `.gpx` (GP6) and PowerTab
`.ptb` stay unread, with a message that says what to do instead.

### 2.1 Robust ASCII import

`TabTextSanitizer` runs first: box-drawing, en/em dashes, full-width bars, NBSP,
BOM and tabs become plain tab glyphs; numbered strings (`1|` ... `6|`) become
`e B G D A E`; systems written lowest string first are flipped; drum-kit rows are
refused with an explanation; lines are capped at 16384 characters and the whole
normalisation has a wall-clock budget. Rhythm lines (`q q e e h`) and count
lines (`1 e & a 2 ...`) above a staff place the notes in time. A page with
chords and no tab (Ultimate-Guitar `[ch]`, ChordPro `[Am]`, plain chord lines)
becomes one strummed bar per chord (`TabChordChart`). `NotationImporter` uses the
same pipeline for every text file (`.txt`, `.tab`, `.md`, `.html`, no extension).

## 3. Architecture

```
file ──► NotationImporter ──► PerformanceScore ──► AsciiTabWriter (preview/export)
                                     │
                                     ├─► Riff::fromScore ──► RiffCompiler::compile ──► RiffPlayer  (live playback)
                                     └─► Riff::fromScore ──► RiffDestinations::render (offline render / tests)
```

- **Parsing** targets `PerformanceScore` directly (the notation model). ASCII and
  MusicXML both fill track 0's notes with string+fret+techniques.
- **Technique mapping**: ASCII glyphs and MusicXML `<technical>` map to
  `ScoreTechnique::Type`. From there the existing `RiffCompiler` (5.1.3) maps
  each to the engine's `Technique` (HammerOn, PullOff, Tap, PalmMute, Slide,
  Bend curves, harmonics, vibrato, …). No new technique DSP.
- **Playback**: `Riff::fromScore` converts a parsed score's track 0 into a
  `Riff` (notes are already `ScoreNote`s with absolute beats; tuning/capo/tempo
  come from the score). The panel's Play button compiles it and hands it to the
  live engine's `RiffPlayer` — the same audition path riffs use. Offline the
  tests use `RiffDestinations::render` for a self-contained render.
- **Export**: `NotationExporter::renderAsciiTab` serializes the score back to
  ASCII. MusicXML and MIDI export already exist and are reused.

## 4. Robustness (rock solid)

The ASCII reader is the adversarial surface (arbitrary user text). It is written
to never crash or corrupt memory on malformed input:

- Every column / character index is bounds-checked; nothing indexes past a
  line's length.
- Frets are clamped to `[0, Riff::kMaxFret]` (36); string indices past the
  instrument's string count are skipped, not written out of range; MIDI notes
  are clamped to `[0, 127]`.
- Alternate tunings are recognised from a `Tuning:` header (Standard, Drop D/C,
  DADGAD, half-step-down, Open G/D, …) and applied to the track; capo from a
  `Capo:` header; tempo/metre from a `Tempo:` header.
- Multi-section tabs (repeated systems, `[Section]` labels) accumulate across
  the file; each "system" of ≥4 tab-shaped lines becomes measures.
- Mixed line endings, truncated files, wrong string counts, out-of-range frets,
  huge inputs and non-ASCII/unicode bytes are all handled: unrecognised lines
  are ignored rather than rejected, and a file with no readable tab returns a
  clean error instead of a partial/garbage score.

The `TabImport` test suite fuzzes the reader with random bytes, truncation,
huge inputs, mixed EOLs, wrong string counts, out-of-range frets and unicode,
asserting only that it returns without crashing and never produces a note on an
invalid string or with a non-finite/ out-of-range pitch.

## 5. Tests (`Source/Tests/TabImportTests.cpp`, suite `TabImport`)

- **Round-trip**: parse → export → parse produces a stable score (same notes,
  strings, frets).
- **Fixtures**: real-world-shaped ASCII tabs and a MusicXML tab parse to the
  expected notes/positions.
- **Technique mapping**: each ASCII glyph and MusicXML technique lands as the
  right `ScoreTechnique::Type` on the right note.
- **Playback smoke**: import → build riff → render → output is finite and
  bounded, every note on a valid string, pitches in MIDI range.
- **Fuzz**: adversarial inputs never crash and never yield invalid notes.

## 6. Known limitations / product decisions

- **ASCII timing is inferred, never exact.** Column spacing → sixteenth grid at a
  user tempo. Tabs with irregular spacing, triplets or no consistent grid play
  approximately. This is inherent to the format; MusicXML/MIDI import exists for
  anyone who needs correct rhythm.
- **Note durations from ASCII** default short (a sixteenth); sustain/let-ring is
  the engine's, not the tab's.
- **Guitar Pro 6 (`.gpx`) and PowerTab are not read** (see §2); `.gp3/4/5` and `.gp` are.
- **Tuning inference**: recognised from named headers; an unnamed exotic tuning
  falls back to standard (frets still parse; pitches may be off). A future
  improvement is deriving tuning from the per-string note-name prefixes the
  writer emits.

## 7. Dialect coverage and graceful failure (`AsciiTabReader`)

The ASCII reader lives in `Source/Notation/AsciiTabReader.{h,cpp}`;
`NotationImporter::readAsciiTab` delegates to it and keeps its
`TabImportDiagnostics` (`getLastDiagnostics()`). The rule is **read what can be
read, skip what cannot, and say which**: a page never fails because one line
of it was odd. `read` returns false only when no note at all was found.

### 7.1 What a page may contain

| Line | Read as |
| --- | --- |
| `Tuning: Drop D`, `Tuned down 1/2 step`, `Tuning: DADGAD`, `Tuning: Eb Ab Db Gb Bb Eb`, `Tuning: E2 A2 D3 G3 B3 E4`, `tuning = e B G D A E` | header: a named tuning (drop D/C/C#/B/A, double drop D, DADGAD, open G/D/E/A/C, Eb / D / C# / C standard, half/whole step down, 7- and 8-string, 4/5-string bass, ukulele) or a note list (§7.2) |
| a bare note list before the first staff with one name per string (`E A D G B e`) | header: the tuning |
| `Capo: 2`, `Capo on 3rd fret`, `no capo` | header |
| `Tempo: 120 bpm`, `BPM 96`, `♩ = 80`, `Time: 3/4`, `Tempo: 120 4/4` | header (tempo 20–300, metre N/2,4,8,16) |
| `e|---`, `D#|---`, `Eb:---`, `E2|---`, `E ---`, `S1|---`, `|---`, `---` | a staff line; the label may be a note name (used for §7.2), a generic label, or nothing |
| `PM------|`, `P.M. . . .`, `let ring`, `N.H.`, `full  1/2`, `T   T   P`, `~~~` | an annotation above a staff: PM spans palm-mute the notes under them, `let ring` marks them let-ring, the rest is skipped |
| `Am   G   C` | chord names: skipped (counted) |
| `x4`, `(x3)`, `play 4 times`, `3x` on the line after a system, or after a system's closing bar | a repeat count for the system (§7.4) |
| titles, lyrics, comments, anything else | skipped (counted, first 24 listed in `warnings`) |

A staff line is one whose body (after the label) is at least three characters,
at least a quarter fill (`-` or space) and at least 90% tab glyphs, and either
carries content (digits, `x`, a bar) or a label. The writer's beat ruler
(`   1---2---3---4---`), pure separators (`--------`) and rulers with no bar
are not staff lines. A run of consecutive staff lines is a **system**; the top
line is string 0. Systems of 4/5/6/7/8/12 lines set the string count (the
largest wins; unlisted lower strings continue down in fourths); a system with
fewer lines than the track is read as the top strings and warned about.

### 7.2 Tuning inference

Named tunings come from a table. A note list is read either way round: the
orientation with the smaller overall span is the one a guitarist wrote
(`E A D G B E` is 24 semitones low-to-high but 36 the other way), and a
lowercase first name with an uppercase last (`e B G D A E`) is the high-to-low
convention. Explicit octaves (`E2`) are taken as written. Without any header,
the string-name labels of the first full system are the tuning: the top string
is anchored to the nearest octave of its pitch class around E4 (G2 when a four-
or five-line set tops out on G, which is a bass), and every lower string is
the nearest lower octave of its name. Standard is assumed otherwise, and the
diagnostics say which of the three happened.

### 7.3 Symbols

| Glyph | Technique on the score | Played as (`RiffCompiler` 5.1.3) |
| --- | --- | --- |
| `7h9`, `7-h9`, `H` | `hammerOn` on the **arriving** note (9) | HammerOn |
| `9p7` | `pullOff` on the arriving note | PullOff |
| `7^9`, `9^7` | hammer or pull by direction | HammerOn / PullOff |
| `7b`, `7b9`, `7bfull`, `7b1/2`, `7b1 1/2`, `7^` (no digit) | `bend`, value in semitones (target−fret, `full`=2, `1/2`=1; 2 when unstated) | pitch curve |
| `7b9r7`, `7r` | `bendRelease` (value, secondValue=release) | pitch curve |
| `7pb9`, `7pb9r7` | `preBend` | pitch curve |
| `5/7`, `7\5`, `5s7` | `slideLegato` on the departing note, value = target fret; the departing note lasts exactly until the arrival | unpicked Slide arrival, or a glide off when the arrival is missing |
| `9/`, `9\` | `slideOut` towards five frets up/down | glide off |
| `/9`, `\9`, `s9` | `slideIn` (from below, or three frets above) | Pluck + glide in |
| `7~`, `7~~~`, `7v` | `vibrato` (compiler defaults: 5.5 Hz, 30 cents) | vibrato segment |
| `x`, `X` | `deadNote` | MutedPick |
| `PM` after a note, `P.M.` span above the staff | `palmMute` | PalmMute |
| `<12>`, `12*`, `12nh` | `naturalHarmonic` | NaturalHarmonic at the node |
| `[7]`, `7ah` | `artificialHarmonic` | ArtificialHarmonic |
| `7ph` | `pinchHarmonic` | PinchHarmonic |
| `7th` | `tapHarmonic` | Tap |
| `t12`, `T12`, `8t12`, `12t` | `tap` | Tap |
| `5S`, `S5` | `slap` (new type, append-only) | BASS_TECH thumb via `Riff::fromScore` |
| `7P`, `P7` | `pop` (new type, append-only) | BASS_TECH pop |
| `(7)` | `ghostNote` | ×0.45 velocity |
| `7tr9` | `trill`, value = the other fret | alternating hammer/pull |
| `7w` | `whammy` | pitch curve |
| `7>` / `7.` / `7LR` | `accent` / `staccato` / `letRing` | ×1.2 / half length / let ring |
| `=7`, `_7` | a tie: the previous note on the string is extended, nothing re-struck | — |
| `\|:` … `:\|`, `x4` | repeats (§7.4) | — |

Unknown glyphs inside a staff are skipped and counted (`ignoredGlyphs`). A
digit pair above 24 (`35`) is two notes (`splitFrets`).

### 7.4 Timing, durations and repeats

Within a bar closed by a bar line, the unwidened columns (a two-digit fret's
second column carries no time, on every string) are mapped to beats: when the
count divides evenly into 1/2/3/4/6/8/12/16 columns a beat, that grid is used
(so 16 columns in 4/4 is sixteenths, 12 is triplets); otherwise the bar is
stretched proportionally. An open bar (no closing line) is four columns a
beat and may spill into further measures. A note lasts until the next note on
its string (clamped to a quarter of a beat … four beats), or to the end of its
bar. `|: … :|` repeats the enclosed bars (twice unless a count says more), a
bare `x4` repeats the whole system; counts are capped at 16 and a system at
512 measures.

### 7.5 Diagnostics

`TabImportDiagnostics`: `totalLines`, `staffLines`, `headerLines`,
`annotationLines`, `skippedLines`, `systems`, `measures`, `notes`,
`numStrings`, `repeatsUnrolled`, `ignoredGlyphs`, `splitFrets`,
`tuningFromHeader` / `tuningFromStringNames`, `tempoFromHeader`,
`timeSignatureFromHeader`, `warnings` (one line each, capped at 24),
`isPartial()`, and `summary()`: *"Loaded 4 bars, 17 notes (6 strings); 2
lines skipped; 1 unknown symbol ignored; standard tuning assumed."* The tab
reader panel shows the summary in the warning colour whenever anything was
skipped, with the individual warnings as the tooltip.

Adversarial inputs (random bytes, truncated lines, `|||||`, 5000-line files,
mixed EOLs, unicode, out-of-range frets, absurd repeat counts) are covered by
`TabImport.fuzzNeverCrashes…` and the `TabDialect` suite; every loop advances,
every index is bounds-checked (`at()` returns 0 past the end), frets are
clamped to 36 and strings to `kMaxStrings`.

## 8. MIDI ↔ tab

### 8.1 MIDI → tab (`NotationImporter::readMidi`, `TabFingering`)

`readMidi` reads either MIDI profile through `MidiProfiles::importFromMemory`
and `MidiPerformance::toScore`. A **Luthier-profile** file carries string and
fret in its NOTE events and is kept as written. A **generic** file has only
pitches, so the instrument and the fingering are guessed:

- *Instrument*: a part that goes below E2 and never above C4 is a four-string
  bass; otherwise a six-string in standard tuning.
- *Plausibility*: if the channel-to-string read already sounds every pitch
  (fret 0–24 on its channel's string) on two or more strings — which is what
  `writeMidi`'s one-track-per-string export produces — those strings are kept.
- *Fingering* (`TabFingering::assign`): notes in time order; notes starting
  together are a chord. A single note goes to the string/fret with the lowest
  cost: distance from the hand position (a running average of recent fretted
  notes), +0.15 per fret (prefer the low end), +0.5 for an open string, +3 if
  the string is still sounding. A chord is fingered lowest pitch first, each
  note on a free string, with +2 per fret beyond a four-fret stretch. One
  string, one note at a time. A pitch the instrument cannot reach is moved to
  the nearest string's open note or highest fret and counted
  (`notesClamped`), and the diagnostics say so.

The diagnostics carry the note and bar counts and a warning that the
fingering was guessed; the result renders as ASCII tab and plays through
`Riff::fromScore` like any other score. Only part 0 of a multi-part file is
read.

### 8.2 Bridges for user-created content (§9)

Everything the user creates already converts to a `PerformanceScore`:
`Riff::toScore`, `PerformanceCapture::toScore` (the session take),
`MidiPerformance::fromCapture(...).toScore` (the raw MIDI input) and
`CompiledRiff::toScore`. `NotationExporter::write` then produces ASCII tab,
MIDI, MusicXML or Guitar Pro from any of them.

### 8.3 tab → MIDI (`NotationExporter::writeMidi`)

One track per string, string/fret RPN ahead of the notes, CC 68 legato for
hammer-ons and pull-offs, and pitch bend for everything pitched: bends,
bend-releases, pre-bends and whammy (curve points), legato / shift / up /
out slides (a five-point glide over the last quarter of the note, then back to
centre), slide-ins (a glide over the first quarter) and vibrato (a sine at the
technique's rate and depth, 8 points a cycle). The RPN range is two semitones,
so wider glides are clamped. Palm mute, dead notes, harmonics, slap/pop and
tap have no MIDI expression and are lost (the Luthier profile keeps them).

## 9. Panel (`TabReaderTab`, Practice → TAB)

One button row: **Open…** (tab, MusicXML or MIDI), **Live** (the session take,
or the MIDI capture fingered, as a score), **Play**, the format box (ASCII tab,
MusicXML, Guitar Pro, MIDI), **Export…**, the bar window and the status line.
The status says exactly how much of a page was read (§7.5). `openTab`,
`openLivePerformance`, `getStatusText` and `getScore` are the test surface
(`TabPanel` suite).

## 10. Universal tab player (tuning / key / chords / strums / follow / tap / MIDI)

The dialect catalogue behind this section is `docs/research/TAB_FORMATS.md`.

- **Tuning, capo, key headers** (`AsciiTabReader::parseTuningStatement`, shared with the
  normalizer): named tunings (drop/open/DADGAD/baritone/7-8 string/bass/uke/mandolin/banjo),
  note lists either way round with German `H` (`E A D G H E`), single-name shorthand
  (`Tuning: Eb`), and offsets ("half step down", "1 1/2 steps down", "3 semitones down")
  applied to any base; `Stimmung`/`Afinación`/`Accordage` keywords; Roman-numeral capo
  (`Capo III`); `Key: Am` / `Key of G` / `Tonart: A-Moll` (`parseKeyStatement`); `Tempo: 1/4 = 120`
  is a note value, not a metre. The canonical tuning name re-parses to the same list.
- **Key** (`TabKeyDetector`): the page's `Key:` wins; otherwise a Krumhansl-Kessler profile match
  over the duration-weighted notes (bass strings weighted up) with chord symbols voting. Stored in
  `Meta::key` as `Am` / `Bb`; `Riff::fromScore` carries the root as `keyRoot`.
- **Chord names over a staff** become `ScoreMeasure::chordSymbols` at the note column under the
  name; **strumming lines** (`D DU UDU`, `↓↑`, `v ^`, `x` mutes, `>` accents, aligned by column)
  mark the notes with `pickStrokeDown/Up`, `deadNote`, `accent`. `Riff::fromScore` turns every
  two-or-more-note onset into a `RiffStrum` (direction from the marks, muted when every note is
  dead), so the compiler staggers it like a riff strum; chords travel as `Riff::chords`.
- **Chord charts** (`TabChordChart`): inline diagrams (`Am: x02210`, `{define: ...}`) win over
  names; a non-standard tuning gets a searched voicing (`voicingFor`); `Strumming:` patterns
  (eight character cells, space = rest) and `Picking: p i m a` patterns shape each bar; Nashville
  numbers read as chords when a key is known.
- **Playback** uses `Riff::fromScore (score, 0, Riff::kMaxImportedBeats)` so long pages are not
  cut at 16 bars. **Tap tempo** (`TabReaderTab::tap`, `TapTempo`) sets the player's absolute
  BPM, which the player reads every block: a tap while playing changes the rate in place.
- **Follow the music**: `AsciiTabWriter::renderWindow` reports `TabColumnMark`s (beat → column);
  the reader scrolls the window to the sounding bar, draws a `v` playhead row, selects the
  sounding cell, shows the chord row and the chord sounding (`getHighlightedColumn`,
  `getNowChordText`).
- **Overrides**: tuning (notes keep string and fret, pitches follow) and key, with "As written" /
  "As read" restoring the import. **MIDI…** exports the shown score (`exportMidi`); a `.mid`
  opened in the reader is labelled "Experimental MIDI-to-tab (fingering guessed)".
- Tests: `TabUniversal` (parsers, fixtures, strums, adversarial) and `TabPlayer` (panel: follow,
  tap, overrides, MIDI round trip).

## Sources

- [Wikipedia — ASCII tab](https://en.wikipedia.org/wiki/ASCII_tab)
- [Guitar Pro 8 — Export ASCII](https://www.guitar-pro.com/docs/gp8/import-export/export/export-ascii)
- [MusicXML 4.0 — Tablature tutorial](https://www.w3.org/2021/06/musicxml40/tutorial/tablature)
- [MusicXML — technical element](https://usermanuals.musicxml.com/MusicXML/Content/EL-MusicXML-technical.htm)
- [MusicXML — bend element](https://usermanuals.musicxml.com/MusicXML/Content/EL-MusicXML-bend.htm)
- Internal: `docs/MIDI_EXPORT_LUTHIER_PROFILE.md`, `spec/notation-export.md`
