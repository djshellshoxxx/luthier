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
| MIDI (`.mid`) | ✅ (reused, Luthier profile) | ✅ (reused) | Real timing; string/fret via the Luthier profile. |
| Guitar Pro (`.gp*`) | ❌ deferred | ⚠️ GPIF bundle (existing, best-effort) | See below. |

**Guitar Pro import is deferred.** The binary `.gp3/4/5` and zipped `.gpx/.gp`
formats are proprietary and version-specific; a correct importer is a large,
fragile effort out of proportion to this task. The importer detects these
extensions and returns a clear message telling the user to export MusicXML from
Guitar Pro and open that instead (which imports with full timing). Guitar Pro
*export* already exists as a best-effort GPIF bundle in `NotationExporter` and is
untouched. Follow-up: a real `.gp5`/`.gpx` reader if demand justifies it.

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
- **Guitar Pro import deferred** (see §2) — decision needed if the audience
  demands native `.gp5`/`.gpx` reading.
- **Tuning inference**: recognised from named headers; an unnamed exotic tuning
  falls back to standard (frets still parse; pitches may be off). A future
  improvement is deriving tuning from the per-string note-name prefixes the
  writer emits.

## Sources

- [Wikipedia — ASCII tab](https://en.wikipedia.org/wiki/ASCII_tab)
- [Guitar Pro 8 — Export ASCII](https://www.guitar-pro.com/docs/gp8/import-export/export/export-ascii)
- [MusicXML 4.0 — Tablature tutorial](https://www.w3.org/2021/06/musicxml40/tutorial/tablature)
- [MusicXML — technical element](https://usermanuals.musicxml.com/MusicXML/Content/EL-MusicXML-technical.htm)
- [MusicXML — bend element](https://usermanuals.musicxml.com/MusicXML/Content/EL-MusicXML-bend.htm)
- Internal: `docs/MIDI_EXPORT_LUTHIER_PROFILE.md`, `spec/notation-export.md`
