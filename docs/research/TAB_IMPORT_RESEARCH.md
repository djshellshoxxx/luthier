# Tab import research notes: formats, languages, instruments

Status: **research notes only. No product or source code was written or changed.**
Date: 2026-10-09. Audience: whoever implements "import many kinds of guitar/bass/banjo/etc. tabs".

This document is the consolidated, implementation-oriented result of a research workflow
(`wf_9d284962-ab1`: one baseline agent that mapped the current reader, plus five research agents
on formats, regional/language notation, bass, banjo and other non-standard instruments), followed by
a verification pass against the source tree. Where the raw research and the code disagree, the code
wins and the disagreement is recorded in section 1.3.

Related documents that already exist and are authoritative for what they cover (read them first for the
ASCII pipeline; this file does not repeat them):

- `docs/research/TAB_FORMATS.md` - catalogue of ASCII tab dialects and which handler covers each.
- `docs/audit/TAB_IMPORT_RESEARCH_NOTES.md` - why the normalize -> lex -> semantics architecture exists.
- `spec/tab-import-export.md` - product spec (sections 2, 7, 8, 10 matter most).
- `spec/bass-techniques.md` - slap/pop/ghost gating on `GuitarSpec` bass family.
- `docs/superpowers/specs/2026-10-02-tab-*-design.md` - normalizer, lexer, semantic-import designs.
- `spec/instruments/*.md`, `docs/research/INSTR_*.md` - instrument specs (tenor guitar, extended-range bass, guitarron ...) that overlap with the tuning tables proposed here.

## 0. Conventions and confidence tags

- **MIDI numbers** use C4 = 60. Tuning arrays are written `{i0, i1, ...}` in **tab-row order**: index 0 is the
  top printed tab line (for guitar/bass that is also the highest-pitched string). The data model comment
  says "0 is the highest string" (`Source/Notation/PerformanceScore.h:61`, `:116`); for re-entrant instruments
  (banjo, high-G ukulele) the *implemented* meaning is "tab-row order", not "descending pitch" (section 5.1).
- `file:line` references are to the tree at the time of writing; lines drift, symbols do not.
- Tags: **[V]** verified in source by the author of this document; **[R]** reported by the research workflow and
  not re-verified; **[E]** external format/music knowledge (web sources cited by the research, or general
  knowledge) that must be checked against the cited spec or a real sample before coding; **[!]** a discrepancy
  between research inputs and the code, or a defect found by reading code (predicted, not executed).
- Effort scale (rough, includes tests and fixtures): **Low** about a day or less, **Med** 2-5 days, **High** 1-3 weeks.
- Nothing here was run. Every "today's behaviour" statement marked **[!]** is a prediction from reading code and
  should be confirmed by the failing test written first (section 7).

---

## 1. Executive summary

### 1.1 Bottom line

1. **ASCII/text tab is the broadest-coverage path and is already strong.** `TabImportPipeline` (normalize ->
   `TabSemanticAdapter` -> `AsciiTabReader` -> directives -> key detection, with a chord-chart fallback) handles
   4-12 string tabs, named/derived tunings, capo, half-step offsets, slap/pop, whammy, harmonics and most
   web damage. Most remaining work there is *coverage tables and disambiguation*, not plumbing.
2. **Structured formats**: read today are MusicXML (`.xml/.musicxml`), `.mxl`, Standard MIDI, Guitar Pro 3/4/5
   (`.gp3/.gp4/.gp5`) and Guitar Pro 7/8 (`.gp`). **Detected but refused with a friendly error**: Guitar Pro 6
   `.gpx` and Power Tab `.ptb` (`NotationExport.cpp:1455-1464`). **Not detected at all** (fall through to the
   ASCII reader and fail): TuxGuitar `.tg`, AlphaTex, VexTab. Drum tab is deliberately refused.
3. **The `.gp` (GP7/8) reader is much thinner than the research assumed** [!]. `readGpif` mirrors what Luthier's own
   exporter writes (private slide-flag encoding, first track only, no ties/tuplets/repeats/slap/pop). It needs
   hardening against real Guitar Pro exports before `.gpx` support is worth building on top of it (section 3.3).
4. **The biggest cross-cutting problem is the instrument model, not parsing.**
   (a) The string-order invariant is really "tab-row order" and the banjo/ukulele/Nashville/12-string cases violate
   the "highest string first" reading (section 5.1).
   (b) Instrument family is inferred by **at least five unrelated heuristics** that disagree (section 5.2); the worst is
   `Riff::fromScore` tagging every <=4-string import as `bass4` and every 5/6-string import as `guitar6`
   (`Riffs/Riff.cpp:537`), which makes ukulele/mandolin/tenor-banjo imports play an octave up on strings 3-6 and 5/6-string
   basses play as guitars when the string-count retune does not apply [!].
   (c) `TabPlaybackTuningSession::begin` only retunes when the tab's string count equals the loaded instrument's
   (`Practice/TabPlaybackTuningSession.cpp:32`), so every non-6-string import on a 6-string guitar goes through
   `RiffTransposer::place` re-fingering.
5. **Languages**: a tab *body* (fret digits) is language-neutral. Language matters in exactly three places: headers
   (tuning/capo/key/step-down/sections), **chord symbols** (chord-over-lyrics charts) and **pitch-encoded notations
   that are not tablature** (jianpu, solfege melodies, Nashville/Roman charts). The key parser is already the most
   multilingual piece; the **chord parser (`TabChordChart`) is English-only**, and German `B`/`H` is decided per line
   rather than per document. The recommended shape is one per-document *notation dialect* stage inside
   `TabDocumentNormalizer` (section 4.2).
6. **Non-standard instruments** mostly need *data* (named tuning rows, a family tag, a re-entrant/short-string model)
   plus *playback routing*, not new parsers. Bass is the most valuable (slap/pop already import; detection, octave and
   playback family are the gaps). Banjo needs the re-entrant drone model **and a short-5th-string pitch offset** that the
   raw research missed (section 5.4). Pedal steel is a separate, deferrable project.
7. **Licensing**: Luthier is proprietary commercial software with a commercial JUCE licence (`LICENSE`,
   `THIRD_PARTY_LICENCES.txt`). GPL/LGPL reference parsers (Power Tab Editor, ptabtools, TuxGuitar) are *documentation to
   clean-room from*, never code to copy; alphaTab (MPL-2.0) needs a legal decision before any direct port (section 3.12).

### 1.2 Recommended first moves (detail in section 6)

| # | Move | Why first |
|---|------|-----------|
| 1 | Write failing fixtures for every [!] item in section 7 | Locks behaviour before touching `AsciiTabReader` |
| 2 | One `InstrumentResolver` (family, string count, tab-row tuning, drone mask) replacing the scattered heuristics | Fixes bass 5/6, uke/mandolin/tenor-banjo mislabel, MIDI bass, GP/GPIF naming together |
| 3 | `matchTuning` hardening: specificity ordering, bass/banjo/uke/mandolin rows, observed-row-count reconciliation | Cheapest large coverage gain; fixes silent mis-tuning |
| 4 | Key parser fix (`D-Dur` reads as minor) and German note-name document dialect | Small, contained, high correctness value |
| 5 | Real-world GP7/8 hardening + a track picker | Most users bring Guitar Pro files, usually multi-track band files |
| 6 | Re-entrant/short-string model + playback routing (bass family, banjo) | Unblocks banjo/uke/bass "playable", not just "importable" |
| 7 | `.gpx` (BCFZ/BCFS container decoder feeding the hardened GPIF reader) | Large installed base of GP6 files |
| 8 | Language normalizer stage (chord roots, solfege, Cyrillic, Japanese, keywords) | Widest non-English reach, all remaps in one place |

### 1.3 Verified corrections and new findings

These supersede the corresponding statements in the raw research outputs.

| ID | Raw claim | What the code/format actually says | Evidence |
|----|-----------|-------------------------------------|----------|
| C1 | "Slap/pop have no ASCII glyph recognition" (baseline known gap) | **False for the production reader.** `AsciiTabReader` maps prefix `S`->slap, `P`->pop and suffix `S`/`P` the same way; `spec/tab-import-export.md` 7.3 documents `5S`/`7P`. It is true only of `TabDialectLexer` (`p/P`->pullOff at `TabDialectLexer.cpp:129`, `t/T`->tap at `:202`, no `S`). The real bass gaps are disambiguation and playback routing. | [V] `AsciiTabReader.cpp:1269-1275`, `:1365-1366`, `:1471-1478` |
| C2 | "ASCII lexer does not recognise whammy `w`" | Production reader recognises suffix `w` as `whammy` with a fixed `-2.0` and **no curve** (`AsciiTabReader.cpp:1510`); the lexer path does not. Contour->curve parsing is the real gap. | [V] |
| C3 | "Add a Nashville `TuningPreset`" | `TuningPreset::Nashville` exists (`TuningEngine.cpp:60`, name `:526`). More importantly **presets are irrelevant to the import retune**: `TabPlaybackTuningSession::begin` writes `openFrequencyHz` per string from `track.tuning[s]` directly. A missing preset is never the blocker; the string-count gate is. | [V] |
| C4 | "GPIF `Pitches` are highest-first" | Real GPIF stores **lowest string first**; the reader and writer both treat index 0 as the lowest and reverse (`NotationExport.cpp:2319`, `:2342`, `:2346-2347`, `:2428`). | [V]/[E] |
| C5 | ".gp (GP7/8): already implemented, effort low" | Minimal reader tailored to Luthier's own writer: first `Track` only (`:2322`), first bar id per master bar (`:2390`), slide flags decoded with Luthier's private bit assignment (`:2446-2451`, same as writer `:1182-1204`) which differs from Guitar Pro's, no `Slapped/Popped`, no `Tie`, no `PrimaryTuplet`, no repeats/alternate endings, no tempo automation, bend only as `BendDestinationValue/50`. | [V] code; real GP bit meanings [E] (alphaTab `GpifParser`) |
| C6 | "`.gpx` is free once BCFZ is decoded because it collapses onto the GPIF reader" | True only after the GPIF reader is hardened (C5); GP6's `score.gpif` also differs in places from GP7's. Effort is decoder (Med) + GPIF hardening (Med). | [V]/[E] |
| C7 | "GuitarProLegacyReader caps at 7 strings; 8-string GP files truncate" | The cap is the **binary format's** limit: GP3-5 stores exactly 7 tuning ints per track (`GuitarProLegacyReader.cpp:143`; `strings > 7` rejected at `:287`). There is no 8-string GP3-5 file. 8-string/extended-range GP files are GP6+/GPIF. | [V] |
| C8 | Tenor banjo `CGDA = {57,50,43,36}`, Irish `GDAE = {52,45,38,31}` (banjo research) | **Both are exactly one octave too low.** Tenor banjo CGDA is C3 G3 D4 A4 -> `{69,62,55,48}` (same as mandola and viola; matches `docs/research/INSTR_tenor_guitar.md`: "C3-G3-D4-A4"). Irish GDAE is G2 D3 A3 E4 -> `{64,57,50,43}`. `{57,50,43,36}` is actually the *mandocello* (C2 G2 D3 A3). Chicago `{64,59,55,50}` and plectrum `{62,59,55,48}` were right. | [V] against tenor-guitar research doc; [E] |
| C9 | 12-string interleave `{64,64,59,59,55,67,50,62,45,57,40,52}` (nonstandard research; its prose said "higher-octave string first") | The **array is right and the prose is wrong**: the engine lays a 12-string out as 12 strings, even index = course fundamental, odd index = the second string, +12 semitones for courses 2-5 and unison for courses 0-1 (`GuitarLibrary.h:106-107`, `GuitarLibrary.cpp:448-458`, `LuthierEngine.cpp applyTwelveStringTuning`). Use the engine's layout; see 5.9 for what that means for a 6-row tab. | [V] |
| C9b | (not found) | Because the engine models a 12-string as `numStrings = 12`, a normal **6-row tab loaded while the 12-string spec is active never matches** `TabPlaybackTuningSession` (6 != 12) and `RiffTransposer::place` then treats the 12 engine strings as 12 independent strings, so tab string `i` lands on engine string `i` (the wrong course/pitch). Predicted mangled playback; needs a course-aware mapping (5.9). | [!] |
| C10 | Lap steel C6 `{76,72,69,67,64,60}` (marked "?") | Unverified and likely an octave high. A common C6 is C E G A C E with the lowest C at C3, i.e. `{64,60,57,55,52,48}`; octave varies by builder, so prefer octave-anchoring from the tab's own note list. | [E] |
| C11 | VexTab durations are per-note (non-sticky) | VexTab tutorials show `:q` applying to following notes until changed (sticky). Verify against VexTab docs/tests before relying on either. | [E] |
| C12 | (not found by research) | `Riff::fromScore` instrument tag: `numStrings <= 4 ? "bass4" : "guitar6"` (`Riff.cpp:537`); consumed by `RiffTransposer::place` (`:144-146`: +12 and +2 strings if "bass" riff on non-bass guitar). Separate from `AsciiTabReader.cpp:1821` (`numStrings <= 5 && top <= 50` -> track name "Bass"). The two disagree for 5-string basses. | [V] |
| C13 | (not found) | `parseKeyStatement` strips a leading `-` quality as minor: `Tonart: D-Dur` -> quality `-dur` -> `minor = true` (`AsciiTabReader.cpp:2208`, `:2210`). Predicted to read D-Dur as D minor. Also `B-Dur`, `Es-Dur`, `Fis-Moll` (-es/-is names) fail (`:2192`). | [!] |
| C14 | (partly noted for bass) | `matchTuning` is a first-substring-match table (`:230-290`). Predicted misroutes: `Tuning: Open G banjo` -> 6-string Open G (row `:249` precedes banjo `:279`); `Baritone ukulele` -> 6-string baritone guitar (`:264` precedes `:276`); `tenor banjo` -> 5-string banjo (`:279`); `Drop D` on a 4-row bass tab -> 6-string guitar Drop D. When the named count (6) differs from the observed rows (5), the header count wins (`numStrings = max(header, rows)`, `:1800-1804`). | [!] |
| C15 | (not found) | The recovered tuning is **re-serialised as text and parsed a second time**: `TabSemanticAdapter::tuningHeader` emits `Tuning: <canonicalName>` (`TabSemanticAdapter.cpp:7-30`), where `canonicalName` is the table name or, for an explicit note list, the raw payload text (`TabDocumentNormalizer.cpp:195-197`); only an empty name falls back to **octave-less note names** low->high. `AsciiTabReader` then re-parses that text, so named rows round-trip but explicit lists re-enter `assignOctaves` (monotonic descent) and lose octave/re-entrant intent. | [V] |
| C16 | (not found) | Label-derived tuning (`header.tuning = assignOctaves(names)` at `:1775`) is monotonic by construction: a banjo labelled `D B G D g` reads the drone as G2 (43) not G4 (67); a ukulele labelled `A E C G` reads low-G. Only the named table preserves re-entrancy. | [V] |
| C17 | (not found) | A format must be registered in **six** places: `FileKind` (`NotationExport.h:153`), `detectKind` (`:1379`), `canRead` (`:1364`), `read()` switch (`:1436`), `TabImportPipeline` "structured" list (`TabImportPipeline.cpp:107`), and the Practice file chooser filter (`PracticePanel.cpp:2130`: `*.txt;*.tab;*.md;*.html;*.htm;*.musicxml;*.xml;*.mxl;*.gp;*.gp3;*.gp4;*.gp5;*.mid;*.midi`). | [V] |
| C18 | (not found) | Multi-track files import **one** track: `setPreferredTrack` / `GuitarProLegacyReader::getTrackNames` have no UI caller; MIDI `chooseMelodicPart` weights guitar programs (24-31) above bass (32-39) (`NotationExport.cpp:1616-1617`); GPIF reads the first `Track` only. A bass player opening a band `.gp5`/`.mid` cannot reach the bass part. | [V] |
| C19 | (not found) | Generic MIDI bass heuristic is hard-wired to a **4-string** `{43,38,33,28}` when `highest <= 60 && lowest < 40` (`NotationExport.cpp:1731-1737`); 5-string notes below E1 are clamped by `TabFingering`. | [V] |
| C20 | (research: "bend points position 0-60") | `readBend` keeps positions with `jlimit(0, 12, position)` and the score divides by 12 (`GuitarProLegacyReader.cpp:342`, `:1084`), while TuxGuitar and PyGuitarPro read GP3-5 bend positions as **0..60** (`GP_BEND_POSITION = 60`, value unit `GP_BEND_SEMITONE = 25`). If the file really stores 0..60, every point beyond 20% of the note collapses to the end, flattening the bend curve's timing. Value scaling (`/25` -> semitones) agrees. Verify with a real GP5 bend fixture before touching. | [!]/[E] |
| C21 | (banjo research mentioned only "short scale") | A banjo's 5th string is *short*: its effective nut is at fret 5, so a tab number `n >= 6` on that string sounds `open + (n - 5)`, not `open + n`. A mapper that does `tuning[4] + fret` is wrong by 5 semitones for every fretted 5th-string note. Needs a per-string nut-fret offset (section 5.4). | [E] strong recall; confirm with a banjo tab corpus |
| C22 | (languages research) "apply implied default qualities per degree (2/3/6 minor) for Nashville" | In the Nashville Number System a bare number is a **major** chord; minor is marked (`6m`, `6-`). Implied diatonic quality belongs to Roman-numeral analysis, not NNS. The existing `nashvilleLine` already keeps bare numbers major. | [V] `TabChordChart.cpp:215-239`; NNS convention [E] |

### 1.4 What the reader supports today

#### 1.4.1 Import surface

| Format | Extensions | Detection | Reader | Status |
|--------|-----------|-----------|--------|--------|
| ASCII / plain text, HTML/Markdown-wrapped tab, chord sheets | `.txt .tab .md .html .htm`, no extension | falls through `detectKind` to `asciiTab` (`NotationExport.cpp:1422`) | `TabImportPipeline::read` (`TabImportPipeline.cpp:9`, `:91`) -> `AsciiTabReader::read` (`AsciiTabReader.cpp:1678`); chord fallback `TabChordChart::read` | Primary path |
| MusicXML | `.xml .musicxml` | extension or `<?xml ... score-` sniff | `NotationImporter::readMusicXml` (`:1805`) | Supported |
| Compressed MusicXML | `.mxl` | zip + extension | `readCompressedMusicXml` (`:1501`) | Supported |
| Standard MIDI | `.mid .midi` | `MThd` magic | `readMidi` (`:1677`), Luthier-profile or generic + `TabFingering::assign` | Supported |
| Guitar Pro 3/4/5 | `.gp3 .gp4 .gp5` | `FICHIER GUITAR PRO` magic (`GuitarProLegacyReader.cpp:784`) | `readGuitarProLegacy` -> `GuitarProLegacyReader::read` | Supported, one track |
| Guitar Pro 7/8 | `.gp` (zip with `Content/score.gpif`) | zip sniff (`:1394-1403`) | `readGuitarPro` (`:2212`) -> `readGpif` (`:2241`) | Partial (minimal reader) |
| Guitar Pro 6 | `.gpx` (`BCFZ`/`BCFS`) | magic (`:793`) | none | **Refused** with advice to Save As `.gp`/`.gp5`/MusicXML |
| Power Tab | `.ptb` (`ptab`) | magic (`:799`) | none | **Refused** with advice to export MusicXML/MIDI/ASCII |
| TuxGuitar, AlphaTex, VexTab | `.tg`, `.atex`/inline, inline | none | none | Not handled |
| Drum tab | - | `TabTextSanitizer` | refused with message | By design |

Both UI-facing paths end in the same ASCII pipeline: `PracticeTabImporter::read` (`UI/PracticeTabImporter.h:17`) routes
`FileKind::asciiTab` to `TabImportPipeline` and everything else to `NotationImporter::read`, whose `readAsciiTab`
(`:1780`) also calls `TabImportPipeline`. `TabImportPipeline::read(File)` itself refuses every structured extension
(`:107-117`).

```
file -> PracticeTabImporter::read
          |- asciiTab -> TabImportPipeline::read(File)
          |                 TabDocumentNormalizer::normalize      (recovery, metadata, legends)
          |                 TabSemanticAdapter::buildLegacyReaderText   (re-serialise metadata + body)
          |                 AsciiTabReader::read                  (semantics + timing)  [production]
          |                 TabDirectiveApplier::apply, TabKeyDetector::apply
          |                 (fallback) TabChordChart::read        (chords over lyrics -> strummed bars)
          '- other    -> NotationImporter::read -> readMusicXml | readCompressedMusicXml | readMidi
                                                    | readGuitarPro/readGpif | readGuitarProLegacy
                                                    -> TabKeyDetector::apply
PerformanceScore -> TabReaderTab::startPlayback (PracticePanel.cpp:1884)
          -> Riff::fromScore (Riff.cpp:505) -> [TabPlaybackTuningSession::begin] -> RiffCompiler::compile
          -> RiffPlayer (own clock, absolute BPM)
```

`TabDialectLexer::lex` + `TabTechniqueCompiler::compile` (the token path) are referenced only by their own files and
unit tests [V: grep]. They are **not** a production route; `TabSemanticAdapter` deliberately funnels everything back
through `AsciiTabReader` ("the mature AsciiTabReader remains the semantic/timing backend until the token compiler
fully supersedes it", `TabImportPipeline.h`).

#### 1.4.2 Data model (`Source/Notation/PerformanceScore.h`)

```
PerformanceScore
  Meta { title, artist, tempoBpm, timeSignatureNumerator/Denominator, key, tuningName }
  vector<ScoreTrack>
ScoreTrack { name("Guitar"), guitarId, capoFret, numStrings(6),
             array<int,kMaxStrings> tuning   // open-string MIDI, "highest string first" (kMaxStrings = 12, DspCommon.h:22)
             vector<ScoreMeasure> }
ScoreMeasure { timeSigNum/Den, tempoChange, chordSymbols[(beat,name)], sectionName, vector<ScoreVoice> }
ScoreVoice { vector<ScoreNote> }
ScoreNote { startBeat, durationBeats, stringIndex(0 = highest), fret, midiNote, pitchHz, velocity,
            vector<ScoreTechnique>, autoRules, tiedFromPrevious }
ScoreTechnique { Type, value, secondValue, curve[(position 0..1, semitones)] }
```

Properties that matter for importers:

- The score is **capture-oriented** (`beginCapture / noteStarted / noteEnded / addTechnique / addChordSymbol / endCapture`);
  every reader fills **track 0** that way. `addTrack` exists, but no reader populates more than one track.
- A note stores **string + fret + midiNote**; the pitch is not recoverable from midi alone (the file header comment makes this
  "rule 2"). `pitchHz` is a double, so microtonal values are representable even though `midiNote` is an int.
- **Pitch is computed in at least six places** with the same formula `tuning[s] + capoFret + fret`:
  `AsciiTabReader.cpp:1981`, `Riff::pitchOf` (`Riff.cpp:411`), `GuitarSpecSummary::openPitch` (`RiffTransposer.h:44`),
  `TabFingering::assign`, the MusicXML reader (`NotationExport.cpp:2072`), the GP3-5 reader
  (`GuitarProLegacyReader.cpp:937`). Any instrument-specific pitch rule (short 5th string, partial capo, drone) must
  change all of them or first be centralised (section 5.1).
- `ScoreTrack` has **no** partial-capo mask, drone/re-entrant flag, course layout or instrument family. `Meta.tuningName`
  is a display string.
- `kMaxStrings = 12` is enough for 12-string if each string is its own entry, 8-string guitar, 7-string bass, pedal steel (10).

`ScoreTechnique::Type` (`PerformanceScore.h:28-39`): `bend, bendRelease, preBend, slideUp, slideDown, slideLegato, slideShift,
slideIn, slideOut, hammerOn, pullOff, palmMute, deadNote, naturalHarmonic, pinchHarmonic, artificialHarmonic, tapHarmonic, tap,
vibrato, trill, whammy, ghostNote, accent, staccato, letRing, pickStrokeUp, pickStrokeDown, slap, pop`. There is **no**
tremolo-picking, volume-swell, fade-in or pedal type.

#### 1.4.3 Tuning and instrument handling

- **Import-side tuning** lives in `AsciiTabReader`: `matchTuning` named table (`:230-290`: drop C#..G, Double Drop D, DADGAD,
  open D/Dm/E/Em/G/Gm/A/C, Nashville, Eb/D/C#/C/B standard, baritone, 7/8-string, 6/5-string bass, `bass` (4), ukulele,
  mandolin, banjo, standard), `parseTuningNames` + `tokeniseNames` + `assignOctaves` (note lists in either direction; German `H`;
  octave anchoring, with the bass-like rule "4/5 names topping on G -> reference 43 instead of 64", `:182-183`),
  `stepsDownSemitones` (`:296`, English-only), `kTuningKeywords` (`:391`: tuning/tuned/tune/stimmung/afina/accordage/accordatura;
  plus a katakana check at `:2044`), `parseTuningStatement`, `parseKeyStatement`. Rows below a header's named strings
  "continue down in fourths" (`:1812-1814`).
- **Engine-side tuning**: `TuningEngine` (kMaxStrings 12, per-string `StringTuning`, temperaments, concert A, capo as a fret position with
  a **partial-capo string mask** `setCapoStringMask/getCapoFretFor`). Presets include Standard, DropD/C/B, DADGAD, OpenG/D/E/C,
  Half/FullStepDown, Nashville, SevenString, EightString, BaritoneB, BassStandard, BassFiveString, Custom. There is **no** preset for
  6-string bass, ukulele, mandolin, banjo (`spec/instruments/extended-range-bass.md` already records the missing 6-string bass preset
  and a parts bug).
- **Non-6-string instruments**: `RubricVoicer` (chord voicing/strum) supports up to kMaxStrings, a `coursed` flag for 12-string
  (reads the first string of each course, `RubricVoicer.h:268`, `:366`) and a bass style (`RubricStyle::bass`,
  `RubricBassPattern`). `GuitarSpec` has `numStrings`, `twelveString` and `category` (Electric/Acoustic/Bass/Custom); there is no banjo, ukulele
  or mandolin spec.
- **Instrument family at import** (all heuristics, none authoritative): see section 5.2.

#### 1.4.4 Techniques: where each comes from

| Source | Mechanism | Coverage |
|--------|-----------|----------|
| `AsciiTabReader` (production) | prefix connectors `:1266-1287`, suffix glyph switch `:1422-1520`, two-letter glyphs first (`pb`, `PM`, `tr`, `LR`, `ph`, `ah`, `nh`, `th`), wrappers `< > [ ] ( )` for harmonics/ghost, annotation rows above the staff (`pm`, `let ring`, `n.h` ...) | h p ^ b r pb / \ s S P t T ~ v x X w > . * = _ tr, bend amounts (`full`, `1/2`, `7b9r7`), slide targets, PM spans, harmonics, ghost, ties |
| `TabDialectLexer` + `TabTechniqueCompiler` (token path, not production) | `lexRow` glyph table `:56-214`; `compile` maps `canonicalMeaning` strings 1:1 to every `Type` including slap/pop (`TabTechniqueCompiler.cpp:43-44`) | digits, x, h/H, p/P, b/B, ^, /, \, ~/v, t/T+digit only; legend redefinitions of `<fret>H`, `<fret>B`, `<fret>^<higher>`, `/`, `\` via `findDefinition` |
| `TabDirectiveApplier` | section-scoped prose directives -> `naturalHarmonic, trill, palmMute, letRing` only; everything else is a warning "preserved but not yet rendered" | narrow by design |
| Structured readers | GP3-5 maps almost everything incl. slap/pop/tap and bend curves (`GuitarProLegacyReader.cpp:1041-1089`); GPIF reader a subset (section 3.3); MusicXML hammer/pull/bend/harmonic/slide (`:2134-2169`); MIDI nothing (Luthier profile CCs only) | see section 3 |

#### 1.4.5 Playback

`TabReaderTab::startPlayback` (`PracticePanel.cpp:1884-1935`): `Riff::fromScore(score, 0, kMaxImportedBeats)` -> optionally
`tuningSession.begin(score.getTrack(0))` (exact retune only when `track.numStrings == engine string count`; stores and later restores
the engine's numStrings, capo, capo mask and every `StringTuning`) -> if the retune engaged, instrument summary =
`GuitarSpecSummary::forRiff(riff)` else `RiffDestinations::guitarSummary(processor)` (the loaded guitar) -> `RiffCompiler::compile`
(`Riffs/RiffCompiler.cpp:220` calls `RiffTransposer::place`) -> `RiffPlayer` with its own clock. When the retune does not engage, `RiffTransposer::place` re-frets *by pitch* against the loaded
instrument, may move legato chains to neighbouring strings and drops what does not fit. `Riff::fromScore` also emits `bassTech`
(`slap`/`pop`) events regardless of the loaded instrument (`Riff.cpp:566-577`); the spec gates their *rendering* on the loaded
guitar being a bass (`GuitarCategory::Bass`, `RiffDestinations.cpp:32`).

A parallel export/DAW path is `MidiPerformance::fromScore` (`Export/MidiPerformance.cpp:799`, per-note `addScoreNote :235`): bend/slide/vibrato/whammy ride as `LuthierEvent` carriers plus
CC67 palm mute, CC72 pinch harmonic, CC73 natural harmonic and CC68 legato for hammer/pull; `toScore` (`:852`), `PerformanceCapture::toScore`, `Riff::toScore` and `CompiledRiff::toScore` are
the reverse bridges. Importers never call these, but any new `ScoreTrack` field has to survive (or deliberately skip) them.

### 1.5 The baseline's known gaps, restated with verdicts

| # | Baseline gap | Verdict | Where addressed |
|---|--------------|---------|-----------------|
| 1 | Two parallel ASCII paths; the token path (`TabDialectLexer` + `TabTechniqueCompiler`) is built but `TabSemanticAdapter` still routes every import through `AsciiTabReader` | **Confirmed** [V]: the lexer/compiler are referenced only by their own files and unit tests. Recommendation: do not promote wholesale; share one glyph table first | 3.1 items 1-2, 5.12, R2.4 |
| 2 | Bass slap/pop have no ASCII glyph recognition | **Corrected**: true only for the token path; production reads `S`/`P` (C1). Real gaps: disambiguation (`T`, `s`, `SL`, `PO`) and playback routing | 5.7, 5.12 |
| 3 | Non-6-string imports get an exact engine retune only when the string count matches | **Confirmed** [V] (`TabPlaybackTuningSession.cpp:32`); presets are irrelevant (C3) | 5.13, R5.1 |
| 4 | Re-entrant tunings (banjo, ukulele) mis-modelled; no drone model | **Confirmed and sharpened**: label inference destroys re-entrancy (C16), the adapter drops octaves (C15), the short 5th string needs a nut offset (C21) | 5.1, 5.4, 5.5 |
| 5 | Coursed instruments: no doubled-course import for mandolin/12-string | **Confirmed**; the engine's 12-string is 12 strings, so a 6-row tab never matches (C9b) | 5.6, 5.9 |
| 6 | `.gpx`, `.ptb` detected but unread; drum tab refused; `TabImportPipeline::read(File)` refuses structured extensions | **Confirmed** [V] (`NotationExport.cpp:1455-1464`, `TabImportPipeline.cpp:107-117`) | 3.4, 3.5 |
| 7 | `GuitarProLegacyReader` caps at 7 strings | **Reframed**: a limit of the binary format, not a defect (C7) | 3.2 |
| 8 | Multilingual coverage thin | **Confirmed**; the chord parser is the biggest hole, the key parser the best-covered piece | section 4 |

---

## 2. Gap analysis

Legend: **Now** = supported today ([V] unless tagged). Effort per section 0. "Anchor" is where the change lands.

### 2.1 Formats

| # | Capability | Now? | What is missing | Effort | Anchor |
|---|-----------|------|-----------------|--------|--------|
| F1 | ASCII forum/UG/GP-export tab | Yes | Instrument-aware glyph dialect (S/T/P/SL/PO); legend remap for S/T/P; 1st/2nd endings (played once, warned); polyphony/multi-part (track 0 only); exact rhythm is inherent-no | Low-Med | `AsciiTabReader`, `TabDirectiveApplier` |
| F2 | Chord-over-lyrics / UG `[ch]` / ChordPro | Yes (English roots) | non-English roots, German B/H, quality words, slash-degree Nashville | Med | `TabChordChart` |
| F3 | GP3/4/5 | Yes (one track, repeats unrolled) | track picker UI; audit tuplets/grace/trem-pick/mix-change vs fixtures; bass auto-family | Low | `GuitarProLegacyReader`, UI |
| F4 | GP7/8 `.gp` | Partial | real-world GPIF: slide flags, ties, tuplets, repeats/alt endings, tempo automation, slap/pop, grace, dynamics, multi-bar-rest, all tracks | Med | `NotationImporter::readGpif` |
| F5 | GP6 `.gpx` | No (refused) | BCFZ/BCFS decoder + GPIF hardening (F4) | High | new `GpxContainer` + `readGpif` |
| F6 | Power Tab `.ptb` | No (refused) | full binary reader (clean-room) | High | new `PowerTabReader` |
| F7 | TuxGuitar `.tg` | No | detect + gunzip + versioned reader | Med | new `TuxGuitarReader` |
| F8 | MusicXML / `.mxl` | Yes | confirm bend/harmonic/ties/`<transpose>`; pitch-only parts rely on `TabFingering`; banjo/uke string numbering | Low | `readMusicXml` |
| F9 | Standard MIDI | Yes | GM program -> family; 5/6-string bass; pitch-bend -> curve; decode Luthier CCs on generic files; band-part picker | Low-Med | `readMidi`, `chooseMelodicPart` |
| F10 | AlphaTex | No | direct beat-oriented reader | Med | new `AlphaTexReader` |
| F11 | VexTab | No | small line reader; low value | Med (low priority) | new `VexTabReader` |
| F12 | Drum tab | Refused | by design | n/a | - |
| F13 | Unresearched: MuseScore `.mscz/.mscx`, TablEdit `.tef`, GP1/2 `.gtp`, ABC, LilyPond, Band-in-a-Box | No | out of scope for this pass; MuseScore is reachable via MusicXML today | - | - |
| F14 | Registration of a new format | manual in 6 places | a small reader registry would stop the `detectKind`/`read` switch growing | Low | `NotationExport.{h,cpp}` |

### 2.2 Instruments and tunings

| # | Capability | Now? | What is missing | Effort | Anchor |
|---|-----------|------|-----------------|--------|--------|
| I1 | 6-string std/drop/open/DADGAD/half-step/baritone | Yes | golden regression fixtures only | Low | tests |
| I2 | 7/8-string guitar | Yes (ASCII); GP<=7 strings by format | exact retune needs engine string-count match | Low | `TabPlaybackTuningSession` |
| I3 | 4-string bass E A D G | Yes | - | Done | table |
| I4 | 5-string bass low-B | Yes (table; letters give G-top rule) | - | Done | table |
| I5 | 5-string high-C, 6-string bass, BEAD, drop-D/C bass | Partial | `assignOctaves` bassLike only for 4 or 5 strings topping on G; name heuristic `<=5`; `drop d` keyword resolves to 6-string guitar | Med | `InstrumentResolver`, `matchTuning` |
| I6 | Bass family reaches playback | Partial | `Riff::fromScore` tags only <=4 strings as bass; loaded guitar must be bass for slap/pop; no cross-string-count retune | Med-High | `Riff.cpp:537`, `TabPlaybackTuningSession` |
| I7 | Banjo 5-string pitch | Partial | named row exists; label-derived and header-misroute paths wrong; short-5th-string nut offset absent | Med | `matchTuning`, pitch helper |
| I8 | Banjo alt tunings, tenor, plectrum | No | table rows (values in 5.3); `gCGCD` lowercase-drone spelling | Low | `matchTuning`, `parseTuningNames` |
| I9 | Banjo dialect (T/I/M/R rows, choke `C`, `T`=thumb) | No | row classification + family-scoped glyph map | Med | `AsciiTabReader::classify`, glyph dialect |
| I10 | 5th-string capo / spike | No (API exists) | directive recognition; partial-capo mask carried in `ScoreTrack`; retune session sets mask | Med | `TabDirectiveApplier`, `ScoreTrack`, session |
| I11 | Ukulele re-entrant gCEA | Partial | label-derived loses drone; `Baritone ukulele` misroutes; low-G/D-tuned rows; `bass4` mislabel at `Riff.cpp:537` | Med | resolver, table |
| I12 | Mandolin family | Pitch only | mandola/octave/mandocello rows; course doubling (flag + voicer); tremolo type | Med | table, `ScoreTrack`, `RubricVoicer` |
| I13 | 12-string guitar | Plays as 6-string | course-aware import (not 12 flat rows) | Med | `ScoreTrack`, `RubricVoicer` |
| I14 | Nashville high-strung | Table row yes | non-monotonic audit; keyword context gating | Low-Med | audit |
| I15 | Lap steel / dobro / Russian 7-string | No named tunings | rows (values in 5.3, notes in 5.10); slants need nothing | Low | table |
| I16 | Pedal steel | No | copedent legend parser + per-event pitch deltas | High | new `CopedentReader` |
| I17 | Capo (full) | Yes; frets relative to capo by default; `TabDocumentMetadata.fretsRelativeToCapo(+Explicit)` | `Kapo` and other localized words; the absolute-fret flag is defined but never read or written (5.11) | Low | `readHeader`, `TabSemanticAdapter` |
| I18 | Partial capo from tab text | No | parse "capo 2 on strings 3-5"; mask in `ScoreTrack` | Med | as I10 |
| I19 | Multi-part ASCII (Gtr I/II) | No (normalizer splits parts, reader keeps track 0) | tracks + picker | High | normalizer -> reader |
| I20 | Fretless / microtonal | Out of scope | `pitchHz` could carry it; see `spec/microtonal-bends.md` | - | - |

### 2.3 Techniques

| # | Capability | Now? | What is missing | Effort |
|---|-----------|------|-----------------|--------|
| T1 | hammer, pull, slides, bend/release/pre-bend, vibrato, PM, harmonics (4 kinds), tap, ghost, dead, trill, let ring, accent, staccato, ties | Yes | - | Done |
| T2 | slap `S` / pop `P` | Yes (ASCII, GP3-5, token compiler) | GPIF `Slapped/Popped`, MusicXML, AlphaTex mapping; `T`=thumb, lowercase `s`, `SL`/`PO`/`pop` words, ALL-CAPS tabs, legend remap | Med |
| T3 | whammy | glyph `w` only (fixed -2, no curve) | dive contours `12\0`, V shapes, "dive"/"scoop" prose, large negative range | Med |
| T4 | banjo choke `C` | No | digit-C-digit -> bend (default whole step) | Low |
| T5 | mandolin tremolo, tremolo picking | No type | new `ScoreTechnique::Type` or repeated-note expansion | Med |
| T6 | volume swell / fade-in | No type/CC | new type + CC11 ramp (or velocity ramp) | Med-High |
| T7 | sweep picking | Notes only (fine) | optional pick-stroke tags | Low |
| T8 | right-hand fingering rows (T/I/M/R, p i m a) | Not stripped for banjo | classify as non-string row | Med |
| T9 | pedal/lever changes | No | copedent (I16) | High |
| T10 | natural-harmonic *sounding* pitch (node vs stopped) | Flag only; pitch stays fretted | decide midiNote = sounding harmonic | Low-Med |

### 2.4 Languages and regions

| # | Capability | Now? | What is missing | Effort |
|---|-----------|------|-----------------|--------|
| L1 | German `H`=B in tuning | Same-line evidence only (`tokeniseNames :121-148`) | document-level dialect decision | Low |
| L2 | German `B`=Bb, `-is/-es`, lowercase minor, Moll/Dur chord charts | No (`TabChordChart::letterPc` English only, `:11`) | chord-root + quality remap behind dialect flag | Med |
| L3 | German/Romance keys | Partial: solfege table, moll/menor/mineur/majeur; **`D-Dur` bug**, `-es/-is` names fail | fix + extend | Low |
| L4 | Fixed-do solfege chords/tuning | Key only | chord roots, tuning tokens, accent folding, word accidentals | Med |
| L5 | Nashville numbers | Partial (`nashvilleLine`, needs known key) | key inference/default+diagnostic (bare numbers stay major, C22), slash degrees, diamonds | Low-Med |
| L6 | Roman numeral charts | No (`romanNumeralIn :363` lower-cases, only for capo) | case-preserving parser | Low |
| L7 | Jianpu / numbered notation | No | pitch-sequence reader + `TabFingering::assign` | High |
| L8 | Japanese iroha/katakana, 嬰/変 | Only `チューニング` keyword | transliteration + width folding + kana capo/key words | Med |
| L9 | Cyrillic names/keywords | No | transliteration + homoglyph folding | Med |
| L10 | CJK/Korean header words, section labels | No | keyword tables | Low-Med |
| L11 | Localized capo/step-down/section | Partial (`capo`, `capodastre`, `cejilla` substring) | `Kapo`, `capotraste` ok, `カポ`, `каподастр`, step-down phrases | Low-Med |
| L12 | Per-document dialect detection + diagnostics | No | new stage; `TabDocumentMetadata.dialect` | Med |
| L13 | Sargam / Arabic / Turkish / Indian systems, microtones | No | out of scope; same pitch-sequence core as L7 | - |

### 2.5 Playback and UX

| # | Capability | Now? | What is missing | Effort |
|---|-----------|------|-----------------|--------|
| P1 | Exact retune for any string count | No (equal count only) | policy + proof that the engine/voices tolerate `setNumStrings` change | Med-High |
| P2 | Re-entrant strings survive `RiffTransposer::place` | Only on exact-retune path | drone flag through `Riff`/`CompiledRiff`; "preserve string" | Med |
| P3 | One family resolver | No | section 5.2 | Med |
| P4 | Track/part picker | API only | UI list + `setPreferredTrack`, MIDI part list | Med |
| P5 | Dialect/guess diagnostics surfaced | partial (`TabImportDiagnostics.warnings`) | codes for "assumed key C", "B read as Bb", "copedent required" | Low |
| P6 | Format registration | manual x6 | registry (F14) | Low |

---

## 3. Per-format import notes

### 3.0 Rules every reader follows, and what the capture API cannot express

**Output contract** (all readers today):

- Fill `PerformanceScore` through the capture API (`beginCapture(tempo, num, den)`, `noteStarted/noteEnded/addTechnique/addChordSymbol`,
  `endCapture(finalBeat)`); `endCapture` writes **track 0 only** (`PerformanceScore.cpp:261-`).
- Tuning goes in `ScoreTrack.tuning` in tab-row order; `numStrings`, `capoFret` separately. **Never fold the capo into the tuning
  array** (design rule in `2026-10-02-tab-semantic-import-playback-design.md`, "Capo Semantics": tuning is open-string pitch before
  capo; sounding pitch = tuning + capo + fret).
- Repeats and alternate endings are **unrolled** by the reader (GP3-5 caps at `kMaxUnrolledMeasures = 6000`).
- Percussion/drum tracks are skipped, never imported as tab.
- Report through `TabImportDiagnostics` (`notes, measures, numStrings, tuningFromHeader, warnings`) and return `true` with warnings for
  partial reads; reserve `false` for "nothing usable".
- Offline only (worker/message thread). Nothing here may touch the audio thread.
- Binary readers copy the `GuitarProLegacyReader` discipline: every read is fallible, counts are capped
  (`kMaxFileBytes = 64 MiB`, `kMaxMeasures = 20000`, `kMaxTracks = 64`), a short read keeps what was parsed with a warning, never
  a crash/hang/unbounded allocation. Extend `TabRobustnessTests` fuzzing to each new reader.

**Capture API limits** [V] (they bound what *any* structured format can deliver, so decide per format whether to extend the API):

| Limit | Where | Consequence |
|-------|-------|-------------|
| One time signature and one tempo for the whole score | `beginCapture` stores `meta.timeSignature*`, `endCapture` writes it into every measure and sets only `measures[0].tempoChange` | Meter and tempo maps from GP/GPIF/MusicXML/MIDI are flattened. GP3-5 warns "bar lines use the first one" (`GuitarProLegacyReader.cpp:962-963`); MusicXML places notes at `measureIndex * beatsPerMeasure` (`NotationExport.cpp:1989`) so a mid-piece meter change shifts later bars |
| One sounding note per `stringIndex` | `noteStarted` first calls `noteEnded` on that string | Unison doubling (mandolin courses, 12-string unison pairs) must be separate string entries or a voicing flag; overlapping let-ring notes on one string are truncated |
| Track 0 only | `endCapture` | Multi-track/multi-part inputs need a picker or a multi-score return (2.5 P4, roadmap R3.2) |
| Min duration 1/16 beat | `noteEnded` `jmax(0.0625, ...)` | Fine for tab; relevant to grace-note handling |
| Voices derived from overlap, not from the source | `endCapture` voice assignment | Source voice identity (GP5 voices 1/2, MusicXML `<voice>`) is not preserved |
| `stringIndex < kMaxStrings (12)` | `noteStarted` | 12-string as 12 entries fits exactly; pedal steel 10 fits |

**Registration checklist for a new format** (C17): `NotationImporter::FileKind` (`NotationExport.h:153`) -> `detectKind` (magic first,
extension second, `NotationExport.cpp:1379`) -> `canRead` (`:1364`) -> `read()` switch (`:1436`) -> remove the extension from
`TabImportPipeline`'s refusal list only if the pipeline should accept it (it should not; it is the text path) -> `PracticePanel.cpp:2130`
file-chooser pattern -> help text/`spec/tab-import-export.md` section 2 table -> fixtures + `TabRobustnessTests` fuzz entry. New `.cpp/.h`
files under `Source/` are picked up by `file(GLOB_RECURSE ... CONFIGURE_DEPENDS)` (`CMakeLists.txt:113`); tests likewise (`:329`).

### 3.1 ASCII / plain text (`.txt .tab`, pasted, HTML/Markdown-wrapped)

- **Status**: production, mature. Catalogue of what is covered is `docs/research/TAB_FORMATS.md`; do not duplicate it here.
- **Encoding**: N monospace rows (4-12), one per string, usually highest string on top; optional `e|B|G|` labels; `|` bar lines; inline
  glyphs; prose headers (tuning, capo, tempo, key, time); legends; rhythm/count rows above the staff; chord names above or inline.
  Rhythm is **implicit** (column spacing): when a closed bar's columns divide evenly into 1/2/3/4/6/8/12/16 columns per beat that grid is used
  (so 16 columns in 4/4 = sixteenths, 12 = triplets), else the bar is stretched; notes last until the next on the string (`spec/tab-import-export.md` 7.4).
- **Parse approach (do not rebuild)**: `TabImportPipeline::read` -> `TabDocumentNormalizer` (sanitize, HTML/Markdown, wrap/collapse repair, inherited
  labels, legend and prose-directive collection, whole-document tuning/capo/tempo/time candidates incl. footers, multi-part split) ->
  `TabSemanticAdapter::buildLegacyReaderText` (re-emits unambiguous metadata ahead of the body) -> `AsciiTabReader::read` ->
  `TabDirectiveApplier` -> `TabKeyDetector` -> chord fallback.
- **Mapping**: row index -> `stringIndex` (`t.line`, `AsciiTabReader.cpp:1976`); digit pair <= 24 is one fret else two notes (`:1344-1351`);
  `midiNote = tuning[s] + capoFret + fret` (`:1981`); ties `=`/`_` extend the previous note; section headers -> `ScoreMeasure.sectionName`; chord
  names above the staff -> `chordSymbols`; strum lines -> `pickStrokeUp/Down`.
- **Work items** (all are coverage/disambiguation):
  1. **Instrument-aware glyph dialect.** The glyph switches are single-letter and case-sensitive (`:1266-1287`, `:1422-1520`). Real-world collisions:
     bass `T` (thumb) -> tap; lowercase `s` used as slap -> slideIn/slideLegato; `SL` (slide) -> `S` slap + ignored `L`; `PO` (pull-off) -> `P` pop + ignored `O`;
     all-caps tabs; banjo `T` (thumb finger) -> tap, `S` (slide) -> slap. Resolution order: **explicit legend > instrument-family default > guitar default;
     two-letter tokens before one-letter**. Family must be resolved *before* rows are read (header/tuning/row count -> family -> lex). The normalizer already
     collects legends as `TabNotationDefinition{pattern, canonicalMeaning}` (e.g. `<fret>H`, `<fret>B`); extend the pattern vocabulary to `S`, `T`, `P`, `SL`, `PO`.
  2. **Implementation shape for (1)**: either a pre-lex *length-preserving* glyph rewrite in `TabSemanticAdapter` (substitute `T`->`S`, `SL`->`s ` etc. so column
     alignment, which encodes time, never shifts) or a shared `GlyphDialect` table consulted by both `AsciiTabReader` and `TabDialectLexer`. Recommended: the shared table
     first (single source of truth), the rewrite as the cheap bridge. **Do not promote the token path wholesale yet**: `TabToken` has no durations or beats, the lexer
     lacks `S/P/w/tr/LR/ph/nh...`, and `AsciiTabReader` carries years of fuzz/regression fixes; promote only when the semantic stage in the 2026-10-02 design reaches parity tests.
  3. **Instrument detection without a header** (banjo/uke/mandolin/bass rows) - section 5.2.
  4. **Tuning table hardening** - section 5.3 (specificity order, bass/banjo/uke rows, reconcile named count vs observed rows, label-derived re-entrancy).
  5. **`TabSemanticAdapter::tuningHeader` must not drop octaves** (C15): serialise custom tunings with octave digits, or carry the resolved MIDI array to the reader
     out-of-band instead of round-tripping through text.
  6. Multi-part (Gtr I/II) -> tracks; 1st/2nd endings; fractional positions (`2.6`, currently preserved only in the token path).
- **Edge cases**: low-string-on-top tabs (`systemsReversed` exists); mixed row counts in one file; wrapped/collapsed rows; lyrics between systems (route to chord fallback);
  ALL-CAPS technique legends; a fingering row (`T I M R`) directly under a 5-row banjo block being mistaken for a 6th string (section 5.4).
- **Effort**: items 1/4/5 Low-Med each; 3 Med; 6 High.

### 3.2 Guitar Pro 3 / 4 / 5 (`.gp3 .gp4 .gp5`)

- **Status**: implemented (`GuitarProLegacyReader`, fixtures `multitrack_band.gp4/.gp5`, `riff_gp3.gp3`, `riff_gp4.gp4`).
- **Encoding** [R/E]: little-endian, length-prefixed strings, version string `FICHIER GUITAR PRO vX.YY` (detector accepts a length byte 18..30 then the
  magic). Header: info strings, triplet-feel flag (skipped), lyrics (GP4+), tempo, key, MIDI channel table, measure/track counts, measure headers
  (meter, repeat open/close, alternate ending, marker, key, double bar), per-track block (flags incl. percussion bit, name, **string count + 7 tuning ints**, port/channel,
  fret count, capo, colour; GP5 adds RSE/EQ blocks, version-dependent: `v50` branch), then measure x track x voice x beat: duration (signed log2 byte, tuplet),
  string bitmask, per-string note (flags, type normal/tie/dead, fret, effects incl. bend point list, grace, slide, harmonic, trill, hammer, let ring, PM, staccato, vibrato, tremolo
  picking, GP5 "tapping" field slap/pop/tap).
- **Mapping** [V]: `stringIndex = GP string - 1` (GP string 1 = highest, `:935`); `midiNote = tuning[s-1] + capo + fret` (`:937`); velocity from dynamic; hammer/pull from the
  `hammerFrom` bookkeeping; slap/pop/tap from `slapKind` 1/2/3 (`:1050-1052`); harmonics 1..4 (`:1054-1061`); slides (`:1063-1069`); bend -> `preBend|bend` +
  `bendRelease` with a `curve` (`:1071-1089`); chord diagram names -> `addChordSymbol`; repeats unrolled; time-signature change warns.
- **One track per import**: `preferredTrack` or "first pitched track with notes"; percussion = bit 0 of flags or channel 10 (`track.channel % 16 == 9`).
- **Gaps / audit list**:
  1. **Bend position clamp** (C20) - verify against a real GP5 bend; likely a latent timing bug.
  2. **Track picker** (C18): expose `getTrackNames()` + `setPreferredTrack()` in the UI; for a band file the bass player needs the bass track.
  3. **Family**: the MIDI channel table (parsed but skipped) holds each channel's GM program (0-based: 24-31 guitars, 32-39 basses incl. 35 fretless and 36/37 slap, 104 sitar, 105 banjo,
     106 shamisen); use it, plus track name, plus string count/tuning, as inputs to the `InstrumentResolver`.
  4. Meter/tempo maps flattened (section 3.0). Mid-piece tempo changes carried in GP mix-change events are not mapped to `ScoreMeasure.tempoChange` [verify].
  5. 7-string cap is a **format** limit (C7): do not "fix"; 8-string guitars arrive via GPIF.
  6. Text encoding of titles/track names (Latin-1/CP1252 vs UTF-8): check non-ASCII fixtures.
  7. Banjo/uke tracks: confirm on a real GP banjo file how the 5th-string frets are numbered (neck frets vs from the short nut) before applying the nut offset (section 5.4) [E].
- **Reference**: PyGuitarPro documents the format (`pyguitarpro.readthedocs.io`); there is no official spec, "these parsers are the spec" [R]. TuxGuitar's GP readers are LGPL (read as docs only).
- **Effort**: audit Low; picker Med (UI); family Low.

### 3.3 Guitar Pro 7 / 8 (`.gp` zip + GPIF XML)

- **Status**: implemented but **minimal** (C5). `readGuitarPro` unzips `Content/score.gpif` (64 MiB cap), `readGpif` indexes `Bars/Voices/Beats/Notes/Rhythms` by id and walks
  `MasterBars`.
- **Encoding** [E: alphaTab `GpifParser` is the working reference, MPL-2.0]: ZIP; `Content/score.gpif` XML rooted at `<GPIF>`; id-referenced graph:
  `MasterTrack` (tempo `Automations`), `Tracks/Track` (`Name`, `Instrument ref`, `Staves/Staff/Properties` with `Tuning` `Pitches` **lowest string first**, `CapoFret`,
  possibly partial-capo properties in GP 7.5+, `FretCount`), `MasterBars/MasterBar` (`Time`, `Key`, `Repeat start/end/count`, `AlternateEndings`, `Section`, `Bars` = one bar id per track),
  `Bars` (-> `Voices`), `Voices` (-> `Beats`), `Beats` (`Rhythm ref`, `Notes`, `Chord`, `FreeText`, `GraceNotes`, `Dynamic`, `Tremolo`, whammy properties), `Notes`
  (`Properties`: `String` (0 = lowest), `Fret`, `Tie origin/destination`, `Bended` + origin/middle/destination values and offsets, `Slide` flags, `HopoOrigin/Destination`, `HarmonicType` +
  `HarmonicFret`, `PalmMuted`, `Muted`, `LetRing`, `Tapped`, `Slapped`, `Popped`, `Vibrato`, `Trill`, `Accent`), `Rhythms` (`NoteValue`, `AugmentationDot`, `PrimaryTuplet num/den`).
- **What the reader does today** [V]: first `Track` only; first bar id of each master bar; tuning from any `Tuning` property found recursively and reversed (`:2319-2347`);
  `CapoFret`; note value x dots only (no tuplets); `String`/`Fret` -> `stringIndex = numStrings-1-gpString`; techniques: `BendDestinationValue/50`, `Slide` flags (private mapping),
  `HopoOrigin` -> hammerOn only, `PalmMuted`, `Muted`, `Tapped`, `Vibrato`, `LetRing`, `HarmonicType`; velocity fixed 0.8.
- **Hardening work items** (each is small; together they are the real "`.gp` support"):

| # | Item | Notes |
|---|------|-------|
| G1 | Slide flags to Guitar Pro's real meaning | [E] 1 shift, 2 legato, 4 out-down, 8 out-up, 16 in-from-below, 32 in-from-above (64/128 pick scrapes). Luthier's reader and writer share a private mapping (1 up, 2 down, 4 legato, 8 shift, 16 in, 32 out); **fix both together** and keep a one-release read-compat note for files Luthier wrote earlier, or version them via `GPRevision`/`Encoding`/a Luthier marker |
| G2 | Ties (`Tie origin/destination`) -> `tiedFromPrevious`/extend previous note | currently every tied note re-strikes |
| G3 | Tuplets (`PrimaryTuplet num/den`) -> scale `durationBeats` by den/num | currently ignored: triplets play as straight eighths |
| G4 | Repeats and alternate endings | unroll like the GP3-5 reader |
| G5 | Tempo `Automations` and per-bar `Time` into the score | blocked by the single-meter/tempo capture API (section 3.0) |
| G6 | `Slapped`, `Popped`, `LeftHandTapped`, `Accent`, `AntiAccent`(ghost), `Staccato`, grace notes, `Dynamic` -> velocity, `PickStroke`, whammy beat properties, `Trill` | also add the same to the writer (`NotationExport.cpp:1169-1286` has no slap/pop cases) |
| G7 | Bends: origin/middle/destination values **and offsets** (percent of note) -> `curve`, instead of destination only | value scale `/50` in code; confirm unit against a real file |
| G8 | Harmonics: honour `HarmonicFret` to set the sounding pitch (T10) | |
| G9 | Hammer vs pull: `HopoOrigin` currently always hammerOn; derive by comparing the destination fret | |
| G10 | Choose the track (picker); skip drum tracks (`Instrument ref`/`InstrumentSet`) | the first `Track` may be drums |
| G11 | Family from `Instrument ref` / `InstrumentSet` / track name | feeds `InstrumentResolver` |
| G12 | All voices already looped; verify 2-voice bars and empty/-1 voice ids, multi-bar rests | |

- **Effort**: Med total (G1-G4, G6, G9 first); G5 needs the API decision.
- **Fixtures**: hand-author GPIF or generate with a licensed Guitar Pro; do **not** use the exporter's own output as the only fixture (it round-trips the private slide mapping and
  hides G1).

### 3.4 Guitar Pro 6 (`.gpx` BCFZ/BCFS container)

- **Status**: detected (`BCFZ` or `BCFS` magic, `GuitarProLegacyReader.cpp:793`) and refused with advice (`NotationExport.cpp:1455-1459`).
- **Encoding** [E, from alphaTab's `GpxFileSystem`/`Gp6` loader and the research]: `BCFZ` + uncompressed length + a bit-packed LZ-style stream (MSB-first; flag bit selects literal
  run vs back-reference with variable word size) that decodes to a **`BCFS` virtual filesystem** of fixed-size sectors (about 4 KiB) holding a file table (name, size, sector
  chain). `BCFS` is the same filesystem stored uncompressed (the detector already accepts it). The payload of interest is `score.gpif` (XML like GP7's GPIF). The research also mentions a
  possible non-XML payload variant in some GP6 files; unverified, so the first task is to check real `.gpx` samples for it.
- **Approach**: (1) container: bit reader, back-reference decoder with a hard output-size cap and bounds checks, filesystem walk -> `score.gpif` bytes; (2) hand to the **hardened**
  `readGpif` (section 3.3). Container is about 300-500 lines [R]; the mapping reuse is only valuable after G1-G12.
- **Risks**: decompression bombs (cap output at 64 MiB like zip entries); bit-order and partial-tail handling are the classic traps; GP6 vs GP7 GPIF differences (property
  names, rhythm/tuplet encoding, `GPRevision`) - test with real GP6 samples, do not assume GP7 parity.
- **Licensing**: algorithm documented/implemented in alphaTab (MPL-2.0) and a Dart port (LGPLv3) [R]. Re-implement from the algorithm description; do not port files (section 3.12).
- **Test fixtures**: a `BCFS` fixture is trivial to hand-build; for `BCFZ` write a tiny literal-only encoder in the test plus one hand-built back-reference to exercise the bit reader.
- **Effort**: High (container Med + hardening Med).

### 3.5 Power Tab (`.ptb`)

- **Status**: detected (`ptab` magic, `:799`), refused with advice to export MusicXML/MIDI/ASCII.
- **Encoding** [R/E]: little-endian proprietary binary; header `ptab` + version; **two scores per file** (guitar score + bass score); score holds guitar list (name, string count, per-string MIDI
  tuning, capo, MIDI preset/pan/reverb), chord diagrams, floating text, rhythm slashes, directions, sections of staves, positions (duration power-of-two + flags dotted/rest/vibrato/let-ring/
  palm-mute/staccato/accent/tremolo-pick/pick-stroke/tap/arpeggio) holding notes (string, fret, tied/muted/ghost/hammer-pull/slide type/bend/trill/harmonic/octave). Bends are one
  encoded byte (type + pitch + release + duration) -> synthesise a `curve`. Power Tab Editor 2.x also has a newer native format (`.pt2`, unresearched) and imports `.ptb`.
- **Mapping**: sections/staves -> measures; position -> `startBeat/durationBeats` (explicit durations make rhythm reliable, unlike ASCII); note string/fret -> `stringIndex/fret`
  (verify orientation: Power Tab numbers strings from the high string [R]); guitar tuning -> `ScoreTrack.tuning` and `Meta.tuningName`; the bass score -> a second track (picker) with
  `family = bass`; chord diagrams -> `chordSymbols`; tempo from tempo-marker directions; repeats/endings via directions -> unroll.
- **Edge cases**: 1.0 vs 1.7 differences; two scores; mixed standard-notation+tab staves; position-based timing with multi-bar handling; bend byte decoding.
- **Licensing**: references are `ptabtools` (GPL) and Power Tab Editor 2.x (GPL-3.0). **Clean-room only**: write a format description in your own words (field order, types,
  version differences) from documentation and self-authored sample files; implementers work from that description and never from the GPL source. Not legal advice.
- **Effort**: High (about 600-900 lines [R]). **Priority**: after `.gpx`; value is legacy Usenet-era archives.

### 3.6 TuxGuitar (`.tg`)

- **Status**: not detected; would fall through to the ASCII reader and fail.
- **Encoding** [R, unverified]: gzip-wrapped versioned binary serialisation (`TGSongReader` family): version string, song -> measure headers (meter, tempo, repeats, markers, triplet feel), tracks
  (explicit `(string number, MIDI pitch)` list, channel, name), measures -> beats -> voices -> notes with `NoteEffect` (bend/tremolo-bar points, slide, hammer, vibrato, PM, let ring, harmonic{type,data},
  grace, trill, tremolo picking, tapping/slapping/popping, staccato, accent/heavy accent, ghost, fade-in). **Confirm the container and field order against TuxGuitar's reader source before
  committing** (the research's own source was a file-extension page).
- **Mapping**: nearly isomorphic to `PerformanceScore` (string 1 = highest -> `stringIndex = n-1`; tuning explicit; fade-in has no `Type`).
- **Pragmatic alternative**: TuxGuitar itself exports `.gp5`/MusicXML/MIDI/ASCII; document that route in the "format not supported" message and defer a native reader.
- **Licensing**: TuxGuitar is LGPL; clean-room. `juce::GZIPDecompressorInputStream` (bundled zlib) covers the gunzip step.
- **Effort**: Med. **Priority**: low (few files in the wild).

### 3.7 MusicXML (`.xml .musicxml`) and `.mxl`

- **Status**: implemented (`readMusicXml :1805`, `readCompressedMusicXml :1501` via `META-INF/container.xml`).
- **Reads** [V]: part choice (first non-percussion part with `<fret>`, else first pitched); `<divisions>`; `<transpose>` (chromatic + octave-change); `<staff-details>` `staff-lines`,
  `staff-tuning line` (line 1 = **lowest** string, mapped to `tuning[numStrings-line]`, `:1962-1982`), `<capo>`; `<note>` pitch/rest/chord/grace/tie/tied; `<technical>` `<string>` (1 = highest
  full-length string -> index n-1) and `<fret>`; hammer-on, pull-off, bend (`bend-alter`), harmonic natural/artificial, slide/glissando; first `<sound tempo>`; `<backup>/<forward>`.
  Notes without `<string>/<fret>` are fingered by `TabFingering::assign` (`:2200`).
- **Does not read** [V by grep]: `<harmony>` chord symbols, rehearsal marks/sections, repeats/endings, multiple tempo/meter changes (single `beatsPerMeasure`), slap/pop (no standard element),
  `<tap>`, `<other-technical>` palm mute.
- **Instrument notes**: banjo 5th string - MuseScore draws the short string on the **bottom** line (`staff-tuning line=1` = 5th string), which the reader's mapping turns into `tuning[4]`
  (tab-row order preserved) - good; but the short-string nut offset (5.4) is not applied. Standard-notation-only guitar/bass parts need `<transpose><octave-change>-1` or they import an
  octave high [E].
- **Work items**: confirm bend `bend-alter` -> semitone curve with `<pre-bend>/<release>`; chord symbols from `<harmony>`; tempo/meter maps (needs API decision); sections from rehearsal/words;
  harmonic sounding pitch (T10).
- **Effort**: Low.

### 3.8 Standard MIDI (`.mid .midi`)

- **Status**: implemented (`readMidi :1677`; `MidiProfiles::importFromMemory`). Two modes: Luthier profile (string/fret and CC-encoded techniques kept) vs generic (pitch+time only, fingered by
  `TabFingering::assign`, `:1749-1755`).
- **Reads** [V]: tempo/meter/key meta, notes, bass guess (**only** `highest <= 60 && lowest < 40` -> hard-coded 4-string `{43,38,33,28}`, `:1731-1737`), part choice `chooseMelodicPart`
  (GM guitar programs 24-31 outrank basses 32-39; drums ch.10 never; one part only).
- **Technique decode on generic files**: pitch bend -> bend/whammy curve is not decoded; Luthier's own CCs (67 palm mute, 72 pinch harmonic, 73 natural harmonic, 68 legato) are
  decoded only for the Luthier profile [R].
- **Work items**: (1) GM program -> family (0-based: 32-39 bass incl. 35 fretless and 36/37 slap; 24-31 guitars; 105 banjo); (2) 5/6-string bass when `lowest < 28`; (3)
  band-part picker (bass part reachable); (4) pitch-bend -> `curve` with RPN 0 range (default +/-2 st); (5) generic CC fallback; (6) overlap/legato heuristics optional.
- **Edge cases**: format 0 (split by channel) vs 1; running status; tempo-map changes (flattened); quantisation of human-played files; no capo concept.
- **Effort**: Low-Med.

### 3.9 AlphaTex (alphaTab text DSL)

- **Status**: not handled. Pure text, explicit string/fret/duration/tuning -> **more reliable than forum ASCII**.
- **Encoding** [E: verify exact syntax at alphatab.net/docs before coding]: metadata `\title "..." \tempo 120 \tuning e4 b3 g3 d3 a2 e2 \capo 2 \instrument 29`, a `.` line ends the header; body is
  beats `fret.string` (e.g. `3.3`), chords `(1.1 3.2)`, sticky duration `:4`, bars `|`, rests `r`, ties `-`, per-note effect braces `{h}` hammer/pull, `{b (0 4)}` bend points, `{sl}`/`{ss}`
  slides, `{v}` vibrato, `{t}` tap, `{pm}`, `{lr}`, `{nh}/{ah}/{ph}/{th}` harmonics, `{st}`, `{g}`, `{ac}`, `{tr 7 16}`; repeats `\ro`/`\rc n`; tracks `\track`, voices `\voice`; tuplets `{tu 3}`.
- **Approach**: a **direct beat-oriented builder** (like `readGpif`), not the `TabToken` path - the token model is row/column oriented and carries no durations, so it fits ASCII, not AlphaTex.
  (This deliberately differs from the research's "feed `TabTechniqueCompiler`" idea; reuse `TabTechniqueCompiler` only if a shared meaning->`Type` helper is extracted.)
- **Mapping**: string number `N` (1-based from the highest [E, verify]) -> `stringIndex = N-1`; `\tuning` tokens (highest first) -> `ScoreTrack.tuning` via the same note+octave parser as `tokeniseNames`;
  `:N` -> `durationBeats`; accumulate `startBeat`; `|` -> bar; effects -> `ScoreTechnique` (+`curve` for bends).
- **Edge cases**: sticky duration state; tuplets; multi-track/voice; octave notation in `\tuning`; alphaTab versions evolve - pin to a doc version and say so in the error text.
- **Detection**: no safe extension; accept `.atex`/`.alphatex`, or sniff `\title|\tempo|\tuning` in the first non-empty lines of a text file.
- **Licensing**: grammar is public; do not copy alphaTab's parser (MPL-2.0 file-level copyleft).
- **Effort**: Med. **Value**: niche (web authors, tests); also a convenient *test fixture authoring format*.

### 3.10 VexTab / VexFlow

- **Status**: not handled; display DSL, rarely used to distribute songs. **Priority low.**
- **Encoding** [R/E]: `tabstave notation=true tablature=true tuning=standard` then `notes` lines; note `fret/string` (string 1 = high e, `5/6` = fret 5 on low E), column `(5/4.5/5.5/6)`,
  bars `|`, `h p s b v t` between frets, `:q :8` durations (**check stickiness**, C11), `$text$` annotations ignored.
- **Mapping**: `stringIndex = string-1`; `tuning=` keyword -> named table, explicit `E/5,B/4,...` -> note parser; durations when present else even spacing (reuse the ASCII quantiser).
- **Effort**: Med for a small line reader. License MIT, safe to read directly.

### 3.11 Cross-format normalisation rules

| Topic | Rule |
|-------|------|
| String numbering | ASCII rows: index 0 = top row. GP3-5: string 1 = highest [V]. GPIF `String` 0 = **lowest** [V]. MusicXML `<string>` 1 = highest-pitched full-length string; `staff-tuning line` 1 = lowest line [V]. TuxGuitar 1 = highest [R]. VexTab 1 = highest [R]. AlphaTex 1 = highest [E]. Power Tab: verify [R]. MIDI: none. Always convert to tab-row order and never pitch-sort afterwards |
| Bend/whammy curves | Normalise to `curve = [(position 0..1 of the note, semitones relative to the fretted pitch)]` plus `value` = peak. Sources: GP3-5 (position 0..60?, value/25, C20), GPIF (origin/middle/destination + offsets, unit /50 in code), TuxGuitar/AlphaTex (point lists), MusicXML (`bend-alter` semitones + `pre-bend/release`), MIDI (14-bit bend, range +/-2 st unless RPN 0). Build **one** shared `BendCurve` helper with unit tests per source |
| Slap/pop | GP5 `tapping` field and GPIF `Slapped/Popped`, TuxGuitar slapping/popping, AlphaTex -> `Type::slap/pop`; `Riff::fromScore` converts them to `bassTech` events |
| Percussion | skip; GP flag/channel 10; GPIF instrument; MIDI ch.10; MusicXML percussion clef; ASCII drum rows (refused) |
| Repeats / endings | unroll in the reader, shared helper, same cap |
| Rhythm | explicit in every structured format; ASCII/VexTab implicit -> quantiser |
| Capo | separate field; frets relative to capo by default (`fretsRelativeToCapo`) |
| Meter/tempo changes | flattened until the capture API is extended (section 3.0) |
| Pitch formula | centralise (`ScoreTrack::pitchOf(string, fret)`), see 5.1 |

### 3.12 Licensing matrix

Luthier is proprietary (`LICENSE`), commercial JUCE licence (`THIRD_PARTY_LICENCES.txt`). Anything copied or ported must be listed in `THIRD_PARTY_LICENCES.txt`.

| Reference | Licence | Use |
|-----------|---------|-----|
| MusicXML 4.0, Standard MIDI File spec | open specs | use freely |
| VexFlow / VexTab | MIT | may read and reuse with attribution |
| alphaTab (GPIF, GPX/BCFZ, AlphaTex, GP3-5) | MPL-2.0 (file-level copyleft) | read for behaviour; a port is a derivative that stays MPL and source-available - legal decision before any direct port; default to clean re-implementation |
| PyGuitarPro (GP3-5 format doc) | LGPL-3.0 [E] | documentation only |
| TuxGuitar | LGPL | documentation only; clean-room |
| ptabtools, Power Tab Editor 2.x | GPL | documentation only; strict clean-room (separate spec author) |
| dart_gp_tab_reader | LGPLv3 (port of alphaTab) [R] | do not use |
| User/sample tab files | various | **never ship third-party tabs as fixtures**; write original, short fixtures (as `Source/Tests/Fixtures/TabCorpus` already does) |

### 3.13 Formats not researched in this pass

MuseScore `.mscz/.mscx` (zip of XML; reachable today via MusicXML export), TablEdit `.tef`, Guitar Pro 1/2 `.gtp`, ABC notation, LilyPond, Band-in-a-Box, Power Tab Editor 2 `.pt2`, Songsterr/Ultimate-Guitar
"Pro" files (proprietary, no export), Finale/Sibelius native (via MusicXML). Treat as backlog candidates; none is needed for the roadmap in section 6.

---

## 4. Different-language and regional notation

### 4.1 Where language actually matters

A tab *body* (fret digits on string rows) is the same everywhere. Language/region changes three things, and the work concentrates there:

1. **Headers**: tuning, capo, key, step-down, tempo wording, section labels.
2. **Chord symbols**: chord-over-lyrics charts and chord rows above a staff (letter names, solfege, German `H/B`, degree charts).
3. **Pitch-encoded notations that are not tablature**: jianpu/numbered notation, solfege or iroha melodies, sargam, etc. These have no string/fret grid and must bypass the fret-grid reader.

**What exists today** [V]:

| Area | Covered | Not covered |
|------|---------|-------------|
| Note letters in tuning headers / string labels | `pitchClassOfLetter` accepts A-G and `H`=11 (`AsciiTabReader.cpp:101-110`); `tokeniseNames` makes a bare `B` mean Bb **only if an `H`/`h` appears on the same line** (`:121-128`, `:148`); `classify` treats `H\|` as a string label | document-level German decision; `-is/-es` names (Cis, Es, As, Des); `B`=Bb in a tuning list without an `H` (e.g. `Stimmung: E A D G B E` is read as English B) |
| Tuning keywords | `tuning, tuned, tune, stimmung, afina, accordage, accordatura` (`:391`), katakana `チューニング` (`:2044`) | `調弦`, `строй/настройка`, `튜닝`, `调弦/定弦` (`afinação` is already covered through the `afina` substring) |
| Capo | substring `capo` (so capotasto, capotraste, capodastre), `cejilla` (`:737`) | `Kapo/Kapodaster` (no `c`), `カポ`, `каподастр`, `变调夹`, `카포` |
| Key | `parseKeyStatement` (`:2163`): `key of/key:/tonart/tonalidad/tonalite/tonalité/tonalità/in the key`, 17-entry solfege root table, `#/b/♯/♭`, `moll/minor/menor/mineur`, `dur/mayor/majeur` | `tom`, `тональность`, `キー`, `调`; **`D-Dur` reads minor** (C13); German `-is/-es` roots (`Fis-Moll`, `Es-Dur`, `B-Dur`) fail |
| Step-down phrases | English only (`stepsDownSemitones :296`) | `medio tono abajo`, `un ton plus bas`, `halber Ton tiefer`, `半音下げ`, ... |
| Chord symbols | English letters, `m/min/maj/dim/aug/sus/add`, `°`, `ø`, `Δ`, slash bass (`TabChordChart.cpp:11-164`) | `H`, lowercase German minor roots, solfege, `7M`, word qualities, Cyrillic |
| Nashville numbers | `nashvilleLine` (`:215`) when a key is known: degrees 1-7, `#/b`, `-` -> minor, quality suffix re-parsed by `parseChord` | key-less charts, slash degrees (`1/3`), diamonds/holds, split bars |
| Roman numerals | table exists but lower-cases (`romanNumeralIn :363`, capo only) | chord parsing |
| Jianpu, iroha/kana/Cyrillic names | - | everything |
| Corpus | `Source/Tests/Fixtures/TabCorpus/german_h_tuning.txt` (`Stimmung: E A D G H E`) and `AsciiDialectCorpus` | fixtures for every other dialect |

### 4.2 Architecture: one per-document notation-dialect stage

All remaps should live in **one** place, decided **once per document**, instead of being scattered through `pitchClassOfLetter / tokeniseNames / parseRoot / parseKeyStatement / readHeader`.

Proposal (notes only; names are suggestions):

```
// TabDocumentMetadata gains (append-only):
struct NotationDialect {
    enum class Letters { english, german /*H=B, B=Bb, -is/-es*/, nordic /*H=B, B=Bb, no -is/-es*/ };
    enum class Roots   { letters, fixedDoLatin, cyrillic, japanese /*iroha+kana*/ };
    enum class Charts  { letters, nashville, roman, jianpu };
    Letters letters = Letters::english;  Roots roots = Roots::letters;  Charts charts = Charts::letters;
    bool lowercaseRootIsMinor = false;   // German chord rows
    juce::String languageTag;            // "de","es","fr","it","pt","ru","ja","zh","ko" or empty
    TabConfidence confidence = TabConfidence::low;
    juce::StringArray evidence;          // every token that voted, for diagnostics
};
```

- **Where**: inside `TabDocumentNormalizer::normalize`, after `TabTextSanitizer` and before tuning/legend/chord candidate collection, so the existing tuning-ambiguity machinery
  (`tuningAmbiguous`, `TabTuningCandidate`) can carry spelling-driven alternatives (a German `B` can be Bb or B).
- **What it rewrites**: only lines classified as header, legend or chord rows. Never lyrics, never staff bodies. Transliterate to the canonical English forms the mature backend already understands
  (`Tuning:`, `Capo:`, `Key:`, `Am`, `Bb`) so `AsciiTabReader` and `TabChordChart` stay almost untouched. Keep `originalText`; for chord display keep the original glyph in the document, not in
  `ScoreMeasure.chordSymbols` (voicing needs the English name; the pair holds only one string).
- **Detection** (priority order): explicit directive (`language: de`, `Notation: German`, `1=C`) > script (Cyrillic/CJK/kana block counts) > keyword hits (`Stimmung`, `Tonart`, `moll/dur`,
  `Afinación`, `Accordage`, `Cejilla`, `Kapo`) > letter-name evidence (`H` as a root, `-is/-es` names, `Es/As` as roots) > default English. Record every vote in `evidence`.
- **Failure policy**: when guessing, emit a diagnostic and expose the alternative (e.g. "B read as Bb because German markers: Stimmung, H"), never commit silently. The two catastrophic failures
  are (1) German `B=Bb` applied to an English tab (every B wrong) and (2) solfege/roman words read out of lyrics.
- **Fixtures first**: add per-dialect fixtures before trusting detection (section 7.3).

### 4.3 Per-system notes

#### 4.3.1 German / Nordic / Central-Eastern European letter names

| Concept | Rule |
|---------|------|
| Letters | `C D E F G A H`: `H` = B natural, bare `B` = Bb. Sharp `-is` (Cis, Dis, Fis, Gis, Ais), flat `-es` (Des, Ges, Ces; `Es`=Eb, `As`=Ab); rare `Eis=F`, `His=C`, `Fes=E`, `Ces=B`, `Heses=A`. All resolve mod 12 |
| Nordic | `H`/`B` as German; accidental suffixes differ by language (Swedish `-iss/-ess`, Danish/Norwegian `-is/-es`) [E] - tolerate both |
| Dutch | `b`=B natural, `bes`=Bb, `cis/des` [E] |
| Quality | uppercase root = major; lowercase root (`a`, `h`, `cis`) = minor in German theory notation; also `moll` / `dur` (`A-Moll`, `a-moll`, `Fis-Dur`); `7` and `maj7` as English |
| Normalise to | English: `H`->`B`, `B`->`Bb`, `Cis`->`C#`, `Es`->`Eb`, `a`->`Am`, `moll`->`m` |
| Today | tuning-list `H` handled with same-line `B` flip; chords: `TabChordChart::letterPc` returns -1 for `H` (so `H7` is not a chord) and cannot read lowercase/`-is`; key: `H-Moll` ok, `D-Dur` wrong, `Fis/Es/B-` fail |
| Work | dialect flag; extend `parseRoot`/`normaliseQuality` (or feed transliterated tokens); fix hyphen handling and `-is/-es` in `parseKeyStatement` |
| Gotchas | bare `B` ambiguity (4.4 #1); lowercase-minor only on chord rows with German evidence; `Es/As` are also German words (chord-row gate); the hammer-on legend `H` inside a *staff body* is not a note |
| Effort | Low (tuning/key) + Med (chord path) |

#### 4.3.2 Fixed-do solfege (Spanish, Portuguese, Italian, French; Latin-American "cifrado latino")

| Concept | Rule |
|---------|------|
| Names | Do Re Mi Fa Sol La Si = C D E F G A B (older French `Ut`=Do; accents: `Ré`, `Mi♭`; Portuguese `Dó`, `Si`) |
| Accidentals | symbols `# b ♯ ♭`; words ES `sostenido/bemol`, FR `dièse/bémol`, IT `diesis/bemolle`, PT `sustenido/bemol` |
| Qualities | `m/min/-`, ES `menor/mayor`, FR `mineur/majeur`, IT `minore/maggiore`; Brazilian "cifra" uses **English letters** with `7M` (maj7), `5+` (aug), `5-` (b5) [E] |
| Chords | `Lam` = Am, `Rem7`, `Sol`, `Do#m`, `Solb`, `Re/Fa#` |
| Today | **key** side done (`parseKeyStatement` solfege table `:2186-2188`, `menor/mineur/mayor/majeur`); **chord** side absent (`parseRoot` never tries solfege), so a Spanish/Italian chord sheet parses as zero chords; tuning lists in solfege (`Afinación: Mi La Re Sol Si Mi`) not tokenised by `tokeniseNames` |
| Work | longest-match solfege roots before `letterPc` (reuse the 17-entry table); word accidentals/qualities -> symbols; accent folding (UTF-8-safe); per-document solfege flag; tokenise solfege tuning lists |
| Gotchas | `La, Mi, Do, Re, Si, Fa, Sol` are common words in ES/IT/FR/PT lyrics ("La vida es bella", "Si tú me dices"): accept only on **chord rows** (existing chord-vs-lyric classification) and only as a standalone token or token + quality; longest match (`Sol` before `So`, `Sib`/`Sim`/`Si`, `Reb`/`Rem`/`Re`); `Si` can mean B or Spanish "yes" in lyrics |
| Effort | Med |

#### 4.3.3 Nashville Number System

- **Encoding**: arabic 1-7 = scale degree of the song key; a bare number is a **major** chord (quality is *not* implied by degree in NNS; `6m`, `6-`, `2-7` mark minor/extension); suffixes `m/-`, `7`, `maj7/Δ`, `sus`, `o/°`, `+`; alterations `b3`, `#4`; slash `1/3`; diamond/bracket marks = held/rhythm; split bars; sections; key and time at top.
- **Correction** [!] (C22): the raw research proposed "applying implied default qualities per degree (2/3/6 minor)". That is the Roman-numeral/analysis convention, not NNS. The existing `nashvilleLine` is right to keep bare numbers major.
- **Today**: `TabChordChart::nashvilleLine` converts a line of `>= 2` tokens, each `1`-`7` + optional `#/b` + quality, to chords when `keyRoot >= 0` (from a `Key:` statement, `TabChordChart.cpp:679`); `degreeOffsets = {0,2,4,5,7,9,11}`.
- **Work**: key resolution fallback chain (header -> first/last chord -> default C **with diagnostic**, never fail); slash degrees (`1/3` -> `C/E`); `maj7/Δ/o/+` suffix coverage; ignore diamonds for pitch (optionally longer duration); with a capo the anchor key (sounding vs shape) is ambiguous, see 4.4 #14.
- **Gotchas**: must never eat ASCII-tab fret rows (digits 1-7): require whitespace-separated tokens on a chord row with **no `|` string rails and no `e|B|` labels**; `57` = V7 vs fret pair; relative-minor charts keep the stated key.
- **Effort**: Low-Med.

#### 4.3.4 Roman-numeral charts (I ii iii IV V vi vii°)

- **Encoding**: case = quality (upper major, lower minor), `°/o` dim, `+` aug, `ø`, `V7`, `bVII`, `#iv`, secondary `V/V`, figured-bass digits (`V6`, `V4/3`) = inversions.
- **Approach**: same resolver as Nashville; map numeral (`i..vii`) -> degree with the case read **before** lower-casing (the existing `romanNumeralIn` lower-cases and returns 1-12 for capo; do not reuse it as-is for chords). `V/V`: resolve the inner numeral as a temporary key.
- **Gotchas**: `I`, `V`, `X` vs words in lyrics (gate: chord row + known key); `6/64/65/43/42` are inversions when attached to a numeral but `7/9/11/13` are extensions, while popular charts also use `6` as "add 6" - pick one reading, document it; `bVII` vs flat sign spacing.
- **Effort**: Low.

#### 4.3.5 Jianpu / numbered notation (CN, ID "not angka", JP/KR variants)

- **Encoding**: digits `1-7` = **movable-do** degrees, `0` rest; octave by dots above/below; duration by trailing dashes (`1 - - -` whole), underlines (one = eighth, two = sixteenth), trailing dot = x1.5; accidentals `#/b` are *relative to the degree*, not the letter (no natural sign); `1=C`/`1=G`/`1=bB` header sets the key; time signature and tempo as usual. Indonesian "not angka" writes accidentals as slashes (`4/` raise, `7\` lower) [R]. Guitar Pro 8 has a jianpu display [R].
- **Not tablature**: no strings/frets. Must **not** go through the fret-grid reader.
- **ASCII dialect to define and document** (no standard exists): digit tokens separated by spaces; `-` = +1 beat; `_` after a digit halves per underscore; `.` after = dotted; octave up/down as `'` / `,` after the digit, or Unicode combining dot above U+0307 / below U+0323 (two dots = two octaves) when present; `(...)` beam groups; `|`, `||`, `|:`, `:|` bars; optional Latin chord names on the row above (feeds `TabChordChart`).
- **Decode**: `pc = (keyRoot + majorOffset[d-1] + accidental) mod 12`; `midi = tonicMidi + majorOffset[d-1] + accidental + 12*octaveShift`, where `tonicMidi` is the tonic in a documented reference octave (default tonic in C4..B4) and is shifted by +/-12 at fingering time to fit the instrument; duration = 1 beat + dashes - underlines x dot.
- **Map**: `ScoreNote.midiNote/pitchHz/startBeat/durationBeats` set, `stringIndex/fret` provisional, then `TabFingering::assign(score, 0, 24)` against the target tuning (the same path as generic MIDI); `Meta.key` from the header; ties -> `tiedFromPrevious`; slurs -> `slideLegato` or hammer/pull by direction.
- **Gotchas**: missing `1=X` -> default C **with a loud diagnostic**; scale-relative accidentals; plain-text octave marks are non-standard -> conservative parsing; never confuse a numeral row with Nashville (Nashville has no header `1=X`, no durations) or with a fret row (no `|`-railed string lines).
- **Entry**: normalizer classifies a block `numberedNotation` (append a `TabBlockKind`), pipeline routes it to a new `PitchSequenceReader`; solfege-melody and iroha-melody lines can share the core.
- **Effort**: High (new reader + dialect + fingering reuse). **Priority**: after the chord/header work.

#### 4.3.6 Japanese (iroha, kana fixed-do, kanji accidentals, full-width text)

| Concept | Rule |
|---------|------|
| iroha letters | ハ=C ニ=D ホ=E ヘ=F ト=G イ=A ロ=B; accidentals `嬰` sharp / `変` flat prefix: `嬰ハ`=C#, `変ロ`=Bb; key words `長調` major / `短調` minor (`ハ長調` C major, `イ短調` A minor) |
| Kana fixed-do | ド=C レ=D ミ=E ファ=F ソ=G ラ=A シ=B |
| Header words | `チューニング`, `調弦`, `カポ(タスト)`, `キー`, `半音下げ` / `全音下げ`, sections `イントロ, Aメロ, Bメロ, サビ, 間奏, アウトロ` [E] |
| Width | full-width digits `０-９` (U+FF10..FF19), letters `Ａ-Ｚ`, `＃ ♯ ♭`; circled digits for strings in some scores |
| Approach | Unicode transliteration + width folding in the normalizer; Japanese-mode flag so `ロ`/`イ` are not misread elsewhere |
| Gotcha | **`ロ`=B natural**; Japanese does not use the German `B=Bb` swap, so do not apply it; fret bodies are plain ASCII |
| Effort | Med |

#### 4.3.7 Russian / Cyrillic

- **Names**: до ре ми фа соль ля си = C D E F G A B; accidentals диез/бемоль; qualities мажор/минор (also дур/моль in theory); header words `строй/настройка`, `тональность`, `каподастр/капо`, `на полтона ниже`; sections `Куплет, Припев, Проигрыш, Вступление`; strum/pick patterns `Бой` / `Перебор` [E].
- **Homoglyph hazard**: Russian pages often contain Cyrillic look-alikes inside Latin chord names (`А`=A, `В`=B, `С`=C, `Е`=E, `Н`=H(!), `М`=M, `К`...). Fold only on chord rows. `Н` (looks like Latin `H`) interacts with the German rule: fold to `H` and then resolve per the document dialect.
- **Regional instrument**: the Russian 7-string guitar (open-G: Re Sol Si Re Sol Si Re) -> `{62,59,55,50,47,43,38}` [E]; add to the tuning table (5.3).
- **Gotchas**: `соль` contains `со`; longest match; the Veys relative-solfege syllables are out of scope unless a `1=X`-style anchor exists (then treat like jianpu).
- **Effort**: Med.

#### 4.3.8 Chinese and Korean headers (and playing key vs sounding key)

- Chinese: `调弦/定弦` tuning, `变调夹` (or `Capo`) capo, `原调` original/sounding key, `选调` the key the shapes are played in, `大调/小调`, sections `前奏 间奏 主歌 副歌 尾奏`; chord names are Latin letters. Example header `原调 E 选调 C 变调夹 4` means: sounding key E, play C shapes, capo 4. Store `capoFret = 4`; `Meta.key` = sounding key (E) and record the shape key in a diagnostic. Which key a *degree* chart (Nashville, jianpu `1=C` with a capo noted) anchors to when a capo is present is ambiguous: Western NNS charts normally use the sounding key, while jianpu guitar charts commonly write the shape key. Pick one documented rule and expose it in a diagnostic.
- Korean: `카포` capo, `튜닝` tuning, `키`/`조` key, `장조/단조`, `반음 내림` [E]. Hangul solfege (`도 레 미 파 솔 라 시`) behaves like fixed-do.
- **Effort**: Low (keyword tables), assuming the dialect stage exists.

#### 4.3.9 Out of scope but cheap on the same core

Sargam (Sa Re Ga Ma Pa Dha Ni, komal/tivra), Arabic/Turkish solfege with quarter-tone accidentals (koma), Georgian and other systems. Integer `midiNote` cannot hold microtones, but `ScoreNote.pitchHz` is a double; any microtonal support also needs MIDI-export carriers and rendering (see `spec/microtonal-bends.md`). Park until the pitch-sequence reader exists.

#### 4.3.10 Localised header vocabulary (all [E]; verify with native samples before shipping)

| Concept | DE | FR | ES | IT | PT | RU | JA | ZH | KO |
|---------|----|----|----|----|----|----|----|----|----|
| tuning | Stimmung | accordage | afinación | accordatura | afinação | строй, настройка | チューニング, 調弦 | 调弦, 定弦 | 튜닝 |
| capo | Kapo, Kapodaster | capodastre | cejilla, capo, capotraste | capotasto | capotraste | каподастр, капо | カポ, カポタスト | 变调夹, capo | 카포 |
| key | Tonart | tonalité | tonalidad | tonalità | tom, tonalidade | тональность | キー, 調 | 调, 原调 | 키, 조 |
| major / minor | Dur / Moll | majeur / mineur | mayor / menor | maggiore / minore | maior / menor | мажор / минор | 長調 / 短調 | 大调 / 小调 | 장조 / 단조 |
| half step down | einen Halbton tiefer | un demi-ton plus bas | medio tono abajo | mezzo tono sotto | meio tom abaixo | на полтона ниже | 半音下げ | 降半音 | 반음 내림 |
| whole step down | einen Ton tiefer | un ton plus bas | un tono abajo | un tono sotto | um tom abaixo | на тон ниже | 全音下げ | 降一个全音 | 온음 내림 |
| verse / chorus / bridge | Strophe / Refrain / Bridge | Couplet / Refrain / Pont | Estrofa / Estribillo (Coro) / Puente | Strofa / Ritornello / Ponte | Estrofe / Refrão / Ponte | Куплет / Припев / Проигрыш | Aメロ / サビ / 間奏 | 主歌 / 副歌 / 间奏 | 벌스 / 후렴 / 간주 |

Notes: the current capo test is `contains("capo")` (`:737`), so German `Kapo` is missed (no `c`). `tono` in Spanish can mean "whole step" or "key" (Latin America): require a step-down context. The step-down parser's number logic is delicate (it must not read a capo fret as a step count, `:316-342`); add localised unit words to the **same** logic via the normalizer's rewrite to English rather than editing it.

### 4.4 Ambiguity register

| # | Ambiguity | Example | Decision rule | Diagnostic |
|---|-----------|---------|---------------|------------|
| 1 | Bare `B`: Bb (German/Nordic) vs B natural | `B7`, tuning `... B E` | Bb only with strong German/Nordic evidence (H as a root, moll/dur, -is/-es, `Stimmung`, directive) and no English contradiction (`Bb`, `Key: ... major`) | "B read as B natural" / "B read as Bb (German spelling: ...)" |
| 2 | Lowercase = minor (`a`, `h`) vs lowercase string label (`e\|`) or English shorthand | `a  e  C` | only on chord rows, only with German evidence | - |
| 3 | `Es/As/Am` words vs chords | German lyrics "Es war..." | chord-row gate: most tokens must parse as chords | skipped-line count |
| 4 | Solfege words in lyrics | "Si tú me dices", "La vita" | chord rows only; no other lyric words on the line | - |
| 5 | Longest match | `Sol/So`, `Sib/Sim/Si`, `Reb/Rem/Re` | token boundary + longest match | - |
| 6 | Hyphen quality: `A-Moll`, `D-Dur`, Nashville `6-`, English `C-` (minor) | `D-Dur` | resolve `dur/moll` words first, then bare `-` | - |
| 7 | Nashville digits vs fret rows vs counts | `1 4 5 1` | chord row, no rails/labels, known or inferred key | "assumed key C" |
| 8 | `6` degree vs added-sixth suffix | `56` | first char is the degree | - |
| 9 | Roman `I/V/X` vs words/letters | lyric "I" | chord row + known key | - |
| 10 | Movable-do digits are key-dependent | jianpu | require `1=X`; default C | "no key given: assumed 1=C" |
| 11 | Jianpu accidentals are degree-relative | `#4` in C = F# | pitch = degree + accidental | - |
| 12 | Japanese `ロ` = B natural | `ロ短調` | do not apply German rule | - |
| 13 | Cyrillic homoglyphs in chord names | `Аm` | fold on chord rows only | count folded |
| 14 | Sounding vs shape key with a capo | `原调 E 选调 C 变调夹 4` | `Meta.key` = sounding, note shape key | key note |
| 15 | `H` is hammer-on in a staff body, B in headers/chords | legend `H = hammer` | context by block kind | - |
| 16 | Full-width digits/letters | `２` | fold in headers/chords (sanitizer folds bars/dashes only) | - |
| 17 | Spanish `tono` = whole step or key | `un tono abajo` | require step-down context | - |

### 4.5 Implementation map for the language work

| Piece | Where | Change |
|-------|-------|--------|
| Dialect detection + transliteration | new pure functions (`NotationDialect.{h,cpp}`) called from `TabDocumentNormalizer::normalize` | testable without the reader |
| Metadata | `TabDocument.h` `TabDocumentMetadata` | append `NotationDialect dialect` (+ diagnostics counters) |
| Tuning-list note names | `AsciiTabReader::tokeniseNames` | take the German decision from the document, keep same-line fallback |
| Key parsing | `AsciiTabReader::parseKeyStatement` | fix hyphen/`-Dur` (C13), `-is/-es` roots, more keywords |
| Chord roots/qualities | `TabChordChart::letterPc/parseRoot/normaliseQuality`, chord-row classifier | dialect parameter (default English) or pre-transliterated tokens |
| Nashville/Roman | `TabChordChart::nashvilleLine` + new roman resolver | key fallback chain, slash degrees, case-preserving roman |
| Header keywords | normalizer rewrite to English headers | no reader change |
| Jianpu / pitch sequences | new `PitchSequenceReader`, `TabBlockKind::numberedNotation` | reuse `TabFingering::assign` |
| Fixtures | `Source/Tests/Fixtures/TabCorpus` + `AsciiDialectCorpus` | one per dialect, original text only |

---

## 5. Non-standard instruments and tunings

Every instrument below is described the same way: **mapping** (rows/courses -> `ScoreTrack.tuning` in tab-row order, `numStrings`, family), **presets** (existing `TuningPreset`
or "Custom"; remember the import retune does not use presets, C3), **techniques** (-> `ScoreTechnique::Type`), **gotchas** (string->pitch mapper, `RiffTransposer::place` re-fingering).

### 5.1 The string/pitch model: what the invariant really is, and what to change

**Today's real invariant** [V]: `ScoreNote.stringIndex` == index into `ScoreTrack.tuning` == the printed tab row (0 = top row). Every reader keeps it (ASCII `t.line`; GP3-5 `string-1`; GPIF
reversed from lowest-first; MusicXML `staff-tuning line` reversed and `<string>-1`). For guitar and bass this equals "highest pitch first". For **banjo (5th string), high-G ukulele, Nashville
high-strung, guitarron, 12-string octave courses** it does not: the tuning array is **non-monotonic**. The doc comments (`PerformanceScore.h:61`, `:116`) should say "tab-row order".

**Importer rules** (cheap, high value):

1. A tuning from a named-table row or from a structured file (GP/GPIF/MusicXML/TuxGuitar/PowerTab/AlphaTex) is **authoritative**: never sort it, never re-run `assignOctaves` on it.
2. `assignOctaves` (note lists and label-derived tunings) is monotonic by construction (`:203-210` "largest note of this pitch class strictly below the previous string"). It is correct only
   for monotonic instruments. For re-entrant candidates (5 rows `D B G D g`, 4 rows `A E C G`, a lowercase final `g`) resolve through the instrument resolver (5.2) and the named table instead.
3. `TabSemanticAdapter::tuningHeader` must not round-trip a resolved tuning through octave-less text (C15).
4. `spanOf` (`front - back`) goes negative for re-entrant sets (`-5` for `{62,59,55,50,67}`); it is only used to pick note-list orientation (`:1662`), so keep re-entrant sets away from it.

**Consumers to audit for monotonic assumptions** (status: [V] read, [?] assumed from comments):

| Consumer | Assumption | Status |
|----------|------------|--------|
| `Riff::pitchOf`, `GuitarSpecSummary::openPitch`, `TabPlaybackTuningSession::begin` | none (per-index lookup) | [V] safe |
| `RiffTransposer::place` | chain move picks the neighbour string by **index distance** `abs(s - home)` (`:318`); family shift `stringOffset = +/-2` (`:146`) | [V] wrong for non-monotonic targets; family shift assumes 4-string bass <-> strings 3-6 |
| `TabFingering::assign` | chords "lowest pitch first, lowest string first" (header comment) | [?] check whether "lowest string" means highest index |
| `RubricVoicer` | "highest string", `occupiedBelow`; coursed reads first string of each course | [?] |
| `AsciiTabWriter` / MusicXML writer | label spelling from pitch class by index; `<string>` numbering | [?] banjo drone label `g`; `<string>` semantics "highest-pitched full-length" |
| `TuningEngine` UI string list | index order | [?] |

**Proposed minimal model additions** (append-only, defaulted so every existing reader and test keeps its behaviour; mirror into `Riff` only what playback needs):

```
struct ScoreTrack {   // existing fields unchanged
    // proposed:
    juce::String family;                      // "guitar","bass","banjo5","banjo4","ukulele","mandolin","steel","other"; empty = unknown
    juce::uint32 droneMask = 0;               // bit n: string n is a re-entrant/short drone (banjo 5th, ukulele high-G if re-entrant)
    std::array<juce::uint8, kMaxStrings> nutFret {};   // 0 normally; 5 for a banjo 5th string
    juce::uint32 partialCapoMask = 0xFFFFFFFFu;        // strings the capo clamps
    bool coursed = false;                     // a row is a course (12-string, mandolin, ...)
    int  pitchOf (int stringIndex, int fret) const;    // THE formula: open + capoFor(string) + (fret == 0 ? 0 : fret - nutFret[string])
};
```

`pitchOf` should replace the six copies of `tuning + capo + fret` (section 1.4.2). `Riff` already carries `tuning`, `capo`, `instrument`; add `droneMask` (and `nutFret`) only
if `RiffTransposer`/`RiffCompiler` need them, and keep the Riff JSON backward compatible (`Riff.cpp:909` reads `instrument` with a default; unknown instrument tags are
rejected at `:914`, so appending new tags to `RiffVocabulary::instruments()` has to be checked against `RiffLibrary.cpp:37`/`:426` and the browser UI first; alternative: keep
`Riff::instrument` for guitar/bass tags only and add a separate optional `family` field).

**Courses** (mandolin, 12-string, bouzouki, bandurria): keep **one row per course** in the score (that is what tabs print). Do not expand at parse time. The engine's own 12-string uses
12 strings (5.9), so the expansion belongs at playback (`RiffTransposer`/`GuitarSpecSummary`), not in the score. `RubricVoicer::coursed` already reads the first string of each course.

### 5.2 Instrument-family resolution: one resolver instead of ten heuristics

| # | Where | Rule | Failure |
|---|-------|------|---------|
| H1 | `TabTextSanitizer.cpp:268`, `:416-429` | numbered-label system: `count == 4`, or `count == 5`, or (`bassHint` and `count < 6`) -> bass names `G D A E B`; `bassHint` = the word "bass" anywhere in the text | numbered `1\|..4\|` ukulele/mandolin/tenor banjo and `1\|..5\|` banjo become **bass** label sets (then H2 octave-anchors them as bass) [!] |
| H2 | `AsciiTabReader.cpp:182` | `bassLike` = (n is 4 or 5) and the top pitch class is G -> top reference 43, else 64 | 6-string bass (tops on C), high-C 5-string, BEAD (tops on D), 7-string bass |
| H3 | `AsciiTabReader.cpp:1821` | `numStrings <= 5 && tuning[0] <= 50` -> `ScoreTrack.name = "Bass"` | 6-string bass named "Guitar"; display only |
| H4 | `Riffs/Riff.cpp:537` | `numStrings <= 4 ? "bass4" : "guitar6"` | **every 4-string non-bass tagged bass; 5/6-string bass tagged guitar** [!] |
| H5 | `NotationExport.cpp:1731-1737` | generic MIDI `highest <= 60 && lowest < 40` -> 4-string `{43,38,33,28}` | 5-string, basses with high notes |
| H6 | `NotationExport.cpp:1616-1617` | MIDI part weight: programs 24-31 (+1e6) over 32-39 (+5e5) | bass part unreachable in band files |
| H7 | `RiffDestinations.cpp:32`, `:288` | loaded guitar `category == GuitarCategory::Bass` | correct (describes the *loaded* instrument) |
| H8 | `RiffDestinations.cpp:428-429` | recording path: bass -> `bass6/bass5/bass4` by strings, else `guitar7/guitar6` | **the right pattern; `Riff::fromScore` should reuse it** |
| H9 | `Riff.cpp:468` | `track.name = isBass() ? "Bass" : "Guitar"` | follows H4 |
| H10 | GP3-5/GPIF/MusicXML readers | take the file's track name; no family | family info in the file (GM program, `Instrument ref`) unused |

**Proposed `InstrumentResolver`** (pure function, called by every reader at finalisation, by `TabSemanticAdapter`, by `Riff::fromScore`):

- **Inputs, in trust order**: (1) explicit words in header/title/part/track name and file metadata (GM program, GPIF `Instrument ref/InstrumentSet`, MusicXML `<score-instrument>`):
  bass/bajo/baixo, banjo, ukulele/uke, mandolin, steel/dobro/lap, "12 string"; (2) an explicit tuning (named row or array); (3) string count and tuning range; (4) string-label letters;
  (5) pitch range of the notes (MIDI/jianpu).
- **Outputs**: `family`, `numStrings`, tuning (tab-row order), `droneMask`, `nutFret`, `coursed`, display name, `confidence`, `reasons[]` (for diagnostics).
- **Decision sketch**:

| Family | Conditions (any strong signal, or the range test) |
|--------|---------------------------------------------------|
| bass | word `bass`; or `4 <= n <= 7` with `tuning[0] <= 53` (F3) **and** lowest `<= 33` (A1); never from string count alone (a baritone guitar and a 4-string guitar are not basses; `spec/bass-techniques.md` section 1 says detection is on the spec, not string count) |
| banjo5 | `n == 5` with word banjo/clawhammer/Scruggs, a banjo tuning row, labels `D B G D g`, or the re-entrant signature (last string higher than the second-lowest, `tuning[4] >= 66`) |
| banjo4 (tenor/plectrum) | word banjo + `n == 4` (ambiguous between CGDA/GDAE/DGBE/CGBD: keep `tuningAmbiguous` with candidates) |
| ukulele | `n == 4` with word uke/ukulele or labels `A E C G`; low-G / baritone from words |
| mandolin family | `n == 4` with word mandolin/mandola/bouzouki/octave, or top == E5 (76) with fifths spacing |
| steel | words steel/dobro/lap/pedal/Hawaiian; pedal steel `n == 10` |
| guitar12 | words "12 string" with `n == 6` (flag `coursed`) |
| guitar7/8 | `n == 7/8` with guitar range |
| guitar | default |

- **Consumers to switch**: H1, H2, H3, H4, H5 and the structured readers. `Riff::fromScore` should map family -> existing tags (`bass4/5/6`, `guitar6/7`, `guitar12`) and carry other families as guitar-class plus the optional drone info.

### 5.3 The named-tuning table (`matchTuning`): hardening rules and rows to add

**Rules** (all [V]-grounded in the current first-substring-match design, `AsciiTabReader.cpp:230-290`):

1. **Specificity order**: instrument-qualified keys before generic keys (`tenor banjo`, `baritone ukulele`, `open g banjo` before `banjo`, `baritone`, `open g`).
2. **Context gating**: a row may require a document context (`bass`, `banjo`, `ukulele`) or a string count; `drop d`/`drop c`/`d standard` in a bass context must select the bass rows, not the 6-string guitar rows.
3. **Count reconciliation**: when the matched row's count differs from the observed staff rows, an instrument-qualified row defers to the rows (re-resolve with instrument + row count, mark ambiguous
   candidates); a generic guitar row with fewer observed rows keeps the top strings and warns; more observed rows extend downward in fourths (existing) and warn. Today the header count simply wins (`:1800-1804`).
4. **Lowercase drone spelling**: accept `gDGBD`, `gCGCD`, `f#DF#AD` (leading lowercase = the 5th-string drone, listed first in the label but last in the rows) in `parseTuningNames`.
5. **Keyword hygiene**: bare substrings (`bass`, `uke`, `banjo`, `baritone`, `standard`) match inside prose; require a tuning-statement context (`looksLikeTuningStatement`) and word boundaries.
6. **Explicit list vs named row**: `parseTuningStatement` tries `matchTuning` first and only then the explicit note list (`:2074-2105`), so `Tuning: E A D G C (5 string bass)` takes the low-B row and ignores the listed notes. When the line carries an explicit list that is consistent with the instrument word (same count), the list should win, with the row as the fallback/octave anchor.

**Rows to add** (tab-row order; MIDI C4 = 60; "name" = display + `Meta.tuningName`):

| Family | Match keys | Name | n | Tuning `{...}` | Note |
|--------|-----------|------|---|----------------|------|
| banjo5 | `banjo` (exists) | Banjo (gDGBD) | 5 | `{62,59,55,50,67}` | C: `gCGBD` `{62,59,55,48,67}` |
| banjo5 | `double c`, `gcgcd` | Banjo Double C | 5 | `{62,60,55,48,67}` | clawhammer staple |
| banjo5 | `sawmill`, `mountain minor`, `g modal`, `gdgcd` | Banjo Sawmill | 5 | `{62,60,55,50,67}` | |
| banjo5 | `open d banjo`, `f#df#ad` | Banjo Open D | 5 | `{62,57,54,50,66}` | drone raised to f#4 |
| banjo5 | `double d`, `adade` | Banjo Double D | 5 | `{64,62,57,50,69}` | drone a4 |
| banjo5 | `g minor banjo`, `gdgbbd` | Banjo G minor | 5 | `{62,58,55,50,67}` | |
| banjo4 | `tenor banjo`, `cgda` | Tenor Banjo (CGDA) | 4 | `{69,62,55,48}` | **C8**: octave above the banjo research |
| banjo4 | `irish tenor`, `gdae` + banjo | Irish Tenor (GDAE) | 4 | `{64,57,50,43}` | |
| banjo4 | `chicago`, `dgbe` + banjo | Chicago (DGBE) | 4 | `{64,59,55,50}` | same as baritone uke |
| banjo4 | `plectrum`, `cgbd` | Plectrum Banjo (CGBD) | 4 | `{62,59,55,48}` | 22-fret neck |
| ukulele | `ukulele`, `uke` (exist) | Ukulele (gCEA) | 4 | `{69,64,60,67}` | re-entrant |
| ukulele | `low g` + uke | Ukulele Low G | 4 | `{69,64,60,55}` | linear |
| ukulele | `baritone uke(lele)` | Baritone Ukulele (DGBE) | 4 | `{64,59,55,50}` | **must precede `baritone`** |
| ukulele | `d tuning`, `adf#b` + uke | Ukulele D | 4 | `{71,66,62,69}` | |
| mandolin | `mandolin` (exists) | Mandolin (GDAE) | 4 | `{76,69,62,55}` | 4 courses |
| mandolin | `mandola`, `cgda` + mandolin family | Mandola (CGDA) | 4 | `{69,62,55,48}` | |
| mandolin | `octave mandolin`, `irish bouzouki` | Octave Mandolin (GDAE) | 4 | `{64,57,50,43}` | |
| mandolin | `mandocello` | Mandocello (CGDA) | 4 | `{57,50,43,36}` | |
| bass | `bass` (exists) | Bass (EADG) | 4 | `{43,38,33,28}` | |
| bass | `5 string bass` (exists) | 5-string bass (BEADG) | 5 | `{43,38,33,28,23}` | |
| bass | `high c`, `tenor bass`, `eadgc` | 5-string bass (EADGC) | 5 | `{48,43,38,33,28}` | tops on C |
| bass | `6 string bass` (exists) | 6-string bass (BEADGC) | 6 | `{48,43,38,33,28,23}` | |
| bass | `7 string bass` | 7-string bass | 7 | `{53,48,43,38,33,28,23}` | B E A D G C F |
| bass | `bead` | Bass BEAD | 4 | `{38,33,28,23}` | |
| bass | `drop d` (bass ctx) | Bass Drop D | 4 | `{43,38,33,26}` | |
| bass | `drop c` (bass ctx) | Bass Drop C | 4 | `{41,36,31,24}` | |
| bass | `d standard` (bass ctx) | Bass D Standard | 4 | `{41,36,31,26}` | |
| bass | `piccolo bass` | Piccolo bass | 4 | `{55,50,45,40}` | an octave up |
| steel | `c6` + lap | Lap steel C6 | 6 | `{64,60,57,55,52,48}` | **C10**: octave varies; prefer the tab's own note list |
| steel | `open e` (exists), `hawaiian` | Open E | 6 | `{64,59,56,52,47,40}` | |
| steel | `dobro`, `resonator`, `high bass g` | Open G (GBDGBD) | 6 | `{62,59,55,50,47,43}` | |
| steel | `e9` + pedal | E9 pedal steel | 10 | `{66,63,56,52,47,44,42,40,38,35}` | F# D# G# E B G# F# E D B; **copedent required** (5.10) |
| other | `russian`, `7 string open g` | Russian 7-string | 7 | `{62,59,55,50,47,43,38}` | D G B D G B D [E] |
| guitar | `nashville` (exists) | Nashville | 6 | `{64,59,67,62,57,52}` | non-monotonic; already present |

Existing data to **reorder**, not add: `open g` (`:249`), `baritone` (`:264`), `banjo` (`:279`), `drop d` (`:243`), `bass` (`:275`).

### 5.4 Banjo (5-string Scruggs/clawhammer, tenor, plectrum)

- **Rows**: 5-string tab prints string 1 (D4) on top ... string 4 (D3), and the short 5th string (g4) on the **bottom** line. Mapping `{62,59,55,50,67}`, `numStrings = 5`, `family = "banjo5"`, `droneMask = 1<<4`,
  `nutFret[4] = 5`. Tenor/plectrum are plain descending 4-string sets (table above), `family = "banjo4"`, no drone.
- **Presets / loaded instrument**: no `TuningPreset` and **no banjo `GuitarSpec`**; the exact-retune path additionally needs a loaded instrument with 5 strings (among the built-in `GuitarType`s, `GuitarLibrary.h:28-37`, only `FiveStringBass` has 5), so "correct banjo pitches with the
  current instrument's timbre" needs either the relaxed string-count policy (P1) or a banjo spec. A banjo *timbre* is a separate instrument-modelling project, not an import task.
- **Short 5th string pitch** (C21) [E]: the string starts at the 5th fret; a tab number `n >= 6` on it sounds `open + (n - 5)`; `0` is open; numbers 1-5 do not occur. Standard tuning: `7` on the 5th string = A4 (69). A 5th-string capo/"railroad
  spike" at neck fret `k` raises the open string by `k - 5` (k=7 -> A, 8 -> Bb, 9 -> B, 10 -> C). `TuningEngine`'s partial capo (`setCapoStringMask(1<<4)`, `getCapoFretFor`) is the right home but its capo math must also use the string's nut fret.
  Prose to recognise: `5th string capo at 7`, `spike at 7`, `capo 5th at 9`, `fifth string capo`; keep it separate from the global `capoFret`. Decide and document whether tab numbers on a spiked 5th string are neck frets (most common) or relative to the spike.
- **Dialect / row classification**: right-hand fingering rows `T I M R` (and `p i m a`) printed under the staff are annotations. The reader skips a plain fingering row today (it fails `isTabGlyph`/staff tests because `I` is not an allowed glyph), but a fingering row with bar lines or dashes aligned to the staff can be classified as another staff row and turn a 5-row block into 6 [!, predicted; fixture needed].
  Banjo glyph map (family-scoped): `T` = thumb (ignore, **not tap**), `S` = slide (**not slap**), `P` = pull-off (**not pop**), `H` hammer-on, `C` choke -> `bend` (value 2 semitones default, 1 for "half"; only when `C` sits between digits on a string row so chord-name `C` is not hijacked), `/ \ s` slides as usual.
  Rolls (forward, backward, alternating thumb) and clawhammer bum-ditty have **no glyphs**: a roll is the note sequence; a brush is simultaneous notes at one `startBeat`; do not synthesise strokes the tab omits. Clawhammer tabs are often melody-only.
- **Numbered labels**: `1|..5|` rows are relabelled as bass names by the sanitizer (H1) - must be fixed for banjo (5 rows + banjo context -> banjo labels).
- **Techniques**: hammer-on, pull-off, slides, choke(bend), harmonics (12th/7th/5th fret natural harmonics are common), vibrato rare. Existing `Type`s suffice.
- **Gotchas for `RiffTransposer::place`**: with the exact retune every note fits its own string (drone preserved). Without it, `place` fits "along the note's own string first": the high g4 (67) note lands on index 4 of a 6-string guitar (A2 string) at fret 22 - right pitch, absurd fingering - and an all-fit octave shift (+/-12) moves the whole part. Add a drone/pin mask to `Riff`/`GuitarSpecSummary` so drone notes are never relocated, and prefer failing loudly ("this tab needs a 5-string instrument") over silent mangling.
- **Effort**: tables Low; dialect + row classification Med; nut/drone model Med; playback High.

### 5.5 Ukulele (and tiple, cuatro, charango, cavaquinho, ...)

- **Mapping**: `A E C G` rows top->bottom. Re-entrant gCEA `{69,64,60,67}` (existing row; the 4th string is higher than the 3rd - non-monotonic but a **full-length** string, so `nutFret = 0`; set `droneMask` bit 3 only to mean "re-entrant" for the preserve-string logic); low-G `{69,64,60,55}`; baritone `{64,59,55,50}` (= Chicago/DGBE); D-tuned `{71,66,62,69}`. Soprano/concert/tenor share pitches.
- **Presets**: none exist. Unlike bass, a missing preset is not a blocker (C3); a 4-string loaded instrument is.
- **Detection**: header word; labels `A E C G`; numbered rows (fix H1); without any signal a 4-row `A E C G` set label-derives to **low-G** (`assignOctaves` forces G below C) - prefer the re-entrant reading for exactly that label pattern and say so in a diagnostic.
- **Techniques**: strums (`D U` rows -> `pickStrokeDown/Up`), fingerpicking, hammer/pull/slide, tremolo, "campanella". `TabChordChart::voicingFor` already searches voicings on the actual tuning, which is what uke chord sheets need.
- **Gotchas**: H4 tags it `bass4` (family shift +12 and strings 3-6 on a 6-string guitar [!]); `Baritone ukulele` misroutes to the 6-string baritone row; `spanOf` for gCEA is +2 (positive) but the array is still non-monotonic.
- **Related instruments to add as rows later** [E]: cuatro venezolano (4, re-entrant), charango (5 courses, re-entrant), tiple (coursed), cavaquinho (4, `D G B D`, not re-entrant), ukulele 6/8-string (coursed), balalaika (3 strings, two unisons).
- **Effort**: Med (resolver + rows); Low once the re-entrant handling exists.

### 5.6 Mandolin family and other coursed fretted instruments

- **Mapping**: 4 printed rows for 4 courses of 2 strings (unison): mandolin `{76,69,62,55}` (E5 A4 D4 G3), mandola `{69,62,55,48}`, octave mandolin/Irish bouzouki `{64,57,50,43}`, mandocello `{57,50,43,36}`. Greek bouzouki, bandurria, laud, cittern, tiple: coursed, to be added as rows when requested.
- **Courses**: keep 4 rows + `coursed = true`; the doubling (unison here, octave on a 12-string) is a voicing/synthesis concern (`RubricVoicer::coursed`). Expanding to 8 `stringIndex` entries is possible (`kMaxStrings` allows it) but doubles every note, breaks `MusicXML <string>` numbering on export and misleads `RiffTransposer`.
- **Presets**: none; mandolin is monotonic and works as a custom 4-string set once a 4-string instrument is loaded.
- **Techniques**: **tremolo** is the signature articulation (`===`, `trem`, wavy line, or `tr`). `tr` collides with the **trill** glyph (`AsciiTabReader.cpp:1407-1414`: `tr` + optional target fret, default fret+2) - in a mandolin context map `tr`/`===`/`trem` to tremolo. There is no tremolo `ScoreTechnique::Type`: either append one (touches `getTechniqueName`, `RiffVocabulary` tokens, `MidiPerformance` carriers, writers) or expand to repeated sixteenths at import (zero model change, loses semantics). Slides, hammers, pulls as usual; chop chords = dead notes.
- **Gotchas**: H4 tags 4 strings bass; the "Bass" display-name rule correctly does not fire (tuning[0] = 76 > 50).
- **Effort**: Med.

### 5.7 Bass guitar (4/5/6/7-string; slap and pop)

- **Mapping**: see the bass rows in 5.3. Top-down: 4-string `{43,38,33,28}` (G D A E), 5-string low-B `{43,38,33,28,23}`, 5-string high-C `{48,43,38,33,28}`, 6-string `{48,43,38,33,28,23}`, 7-string `{53,48,43,38,33,28,23}`, BEAD `{38,33,28,23}`, drop-D `{43,38,33,26}`, drop-C `{41,36,31,24}`.
- **Octave anchoring when only note letters are given**: one rule in bass context: top reference 43 for `n <= 6` (nearest octave to G2: EADG -> G2, BEADG -> G2, BEAD -> D2, EADGC -> C3, BEADGC -> C3), 53 for `n == 7`. This replaces the `n == 4 || 5 && top == G` test (`:182`). The consolidated bass predicate must feed anchoring, `track.name`/family, and `Riff.instrument`.
- **Presets**: `TuningPreset::BassStandard`, `BassFiveString` exist; 6-string and the drop/BEAD sets do not (`spec/instruments/extended-range-bass.md` proposes a six-string preset and documents a parts bug). Presets only matter for a *loaded* bass.
- **Techniques** (what the production reader already does, C1): `S` / `5S` -> `slap`, `P` / `7P` -> `pop` (prefix and suffix), `x` -> `deadNote`, `(n)` -> `ghostNote`, `h/p/^` -> hammer/pull, `/ \ s` slides, `<n>` natural harmonic, `t`/`T` -> `tap`. `Riff::fromScore` turns `slap`/`pop` into `bassTech` events (`Riff.cpp:566-577`).
  Real-world conflicts to resolve with the glyph dialect (5.12): `T` = thumb (slap), lowercase `s` = slap, `SL` = slide, `PO` = pull-off, ALL-CAPS tabs, "double thumb" (no ASCII standard - leave single slap). Alfred's book uses `S/P/H/PO/SL/X`; Guitar Pro and Soundslice use `S`/`P` (mostly above the staff) [E].
  Order: explicit legend > bass-context default > guitar default; two-letter (`SL`, `PO`, `PM`, `pb`, `nh/ah/ph/th`) before single-letter. Keep the existing palm-mute guard (`P`+`M/m/.` is palm mute, `:1270`, `:1401`).
- **Playback** (the decisive gap): slap/pop/ghost render as bass techniques only when the **loaded** guitar is a bass (`spec/bass-techniques.md` 0-1; `GuitarCategory::Bass`). What happens by case (today, predicted from `Riff.cpp:537`, `RiffTransposer.cpp:144-146`, `TabPlaybackTuningSession.cpp:32`):

| Loaded instrument | Imported tab | Today | Wanted |
|-------------------|--------------|-------|--------|
| 4-string bass | 4-string bass | exact retune (counts equal), slap/pop render | same |
| 5-string bass | 4-string bass | `place`: bass-vs-bass, no shift; top 4 of BEADG = G D A E | same |
| 6-string guitar | 4-string bass | tagged `bass4` -> +12 and strings 3-6, notice "Bass line played an octave up on this guitar"; bassTech present but guitar not bass -> no bass rendering | keep, but offer "load a bass" |
| 4-string bass | 5-string bass | tagged `guitar6` -> -12 and -2 string offset (guitar-on-bass) [!] | tag `bass5`; low-B notes below the loaded range are dropped with a notice |
| 6-string guitar | 6-string bass | tagged `guitar6` -> no family shift; notes below E2 unreachable and dropped [!] | tag `bass6` -> +12 |
| any | bass part inside a band `.gp5`/`.mid` | bass track unreachable (C18) | picker |

  Needed: tag family correctly (`Riff::fromScore`), a track picker, and either a "use the tab's instrument" policy (P1) or an automatic bass `GuitarSpec` swap; `TabPlaybackTuningSession` is non-destructive and must stay so.
- **GP/GPIF/MusicXML bass**: tunings arrive explicit; only the family tag is missing. GP3-5 bass tracks with 4-6 strings fit the 7-entry tuning; MIDI needs the 5/6-string rule (C19).
- **Effort**: detection/octave Med; legend/dialect Med; playback High.

### 5.8 Extended-range guitar, drop/open/DADGAD, Nashville, baritone, Russian 7-string, dobro

- **Mapping**: rows exist for 7-string (`{64,59,55,50,45,40,35}`), 8-string (`+30`), Drop A (7), Drop E (8), B Standard (7), baritone `{59,54,50,45,40,35}`, DADGAD `{62,57,55,50,45,38}`, open D/Dm/E/Em/G/Gm/A/C, double drop D, Eb/D/C#/C standard, drop C#..G. All monotonic.
- **Presets**: SevenString, EightString, BaritoneB, DropD/C/B, DADGAD, OpenG/D/E/C, Nashville exist; the import path does not need them (C3).
- **Gotchas**: `Drop A` is a 7-string row but a 6-line tab titled "Drop A" (baritone down a fourth) would be padded to 7 (count reconciliation, 5.3); `B standard` is 7 strings; substring order `double drop d` before `drop d` before `d standard` is already right; `Nashville` as a *tuning* keyword must be gated on a tuning statement (the word also names the number system); a 7/8-string tab on a 6-string loaded guitar is re-fingered and the lowest notes clamp/drop (exact retune needs matching counts, P1).
- **Nashville high-strung** `{64,59,67,62,57,52}`: non-monotonic (index 2 = G4 above index 0/1); the table row is correct, the label-derived path would flatten it.
- **Open tunings**: capo stacks on top (sounding = open + capo + fret). Bottleneck slide tab: slides (`/ \ s`), natural harmonics at 12/7/5.
- **Effort**: Low (regression fixtures + ordering).

### 5.9 12-string guitar and coursed guitars

- **Engine layout** [V]: a 12-string guitar is `numStrings = 12`; string `i` belongs to course `i/2` (0 = high E); the **odd** string of each course is the "octave string": +12 semitones for courses 2-5 (G, D, A, E), unison for courses 0-1 (`GuitarLibrary.h:106-107`, `GuitarLibrary.cpp:448-458`, `LuthierEngine.cpp applyTwelveStringTuning`).
  Engine open pitches for standard 12-string: `{64,64,59,59,55,67,50,62,45,57,40,52}`.
- **Import**: tabs print 6 rows. Import as a plain 6-string track (correct fundamentals), set `family = "guitar12"` / `coursed = true` when "12 string" is stated, and keep the Riff tag `guitar12` (already in `RiffVocabulary::instruments()`, currently unused). A literal 12-row tab maps 12 rows -> 12 strings directly.
- **Gotchas** (C9b) [!]: a 6-row tab with the 12-string spec loaded never retunes (6 != 12) and `place` maps tab string `i` to engine string `i` = the wrong course. Needs a course-aware mapping in `RiffTransposer`/`GuitarSpecSummary` (tab string `s` -> engine string `2s`, optionally also `2s+1`), and a check of how the engine excites a course when only one string gets a note event [verify].
- **Techniques**: standard. Let-ring/jangle is timbral.
- **Effort**: Med.

### 5.10 Lap steel, resonator/dobro, pedal steel

- **Lap steel / dobro**: ordinary open-tuned tab where numbers are bar positions. **Slants** (bar angled, different effective frets on different strings at the same instant) need nothing: they are simultaneous `ScoreNote`s with different frets (`startBeat` equal). Bar glides = slides, bar vibrato = `vibrato`, damping = `deadNote`/`palmMute`. Add tuning rows (5.3). Row orientation varies between authors: rely on labels/tuning-derived orientation.
- **Volume swell** has no `ScoreTechnique` type and no CC: approximate with a velocity ramp, or add a type + CC11 ramp later (T6). Flag as unsupported rather than dropping silently.
- **Pedal steel** (10-string E9 or C6): fret number **plus** a pedal/lever letter (`5B`, `8A`, `10E`, `7P6`). The letters are *not* standardised; each tab defines its own copedent. E9 open tuning F# D# G# E B G# F# E D B; commonly published E9 pedals: A raises strings 5 and 10 (B->C#, +2), B raises 3 and 6 (G#->A, +1), C raises 4 and 5 (E->F#, B->C#, +2) [E; tab must state its own].
  Contract: parse a **copedent legend** (`letter -> {stringIndex -> semitoneDelta}`) from the header; in each fret token split the trailing letters from the digit and apply the deltas to the affected strings at that event; pedal down at note start -> `preBend` (value = delta); pedal engaged mid-note -> `bend` with a `curve` (the curve field exists for this); no legend -> import bare frets and emit "copedent required". Pitch deltas for simultaneous multi-string pedal changes apply per string.
  Needs a new data structure; `GuitarProLegacyReader`'s 7-string cap and `kMaxStrings = 12` are fine for 10 strings.
- **Effort**: lap steel Low-Med; pedal steel High (defer; document the contract now).

### 5.11 Capo and partial capo

- **Full capo**: `readHeader` takes the first number 0-24 on a `capo` line, Roman numerals via `romanNumeralIn`, and `no capo/none/without` as 0 (`:737-743`); stored as `ScoreTrack.capoFret`; sounding pitch `tuning + capo + fret` (frets relative to the capo, which is also how `TuningEngine` defines fret 0 with a capo). Localised words: only `capo` (substring), `capodastre` (redundant), `cejilla`; German `Kapo` is missed (no `c`).
- **Absolute-fret override is dead code** [V]: `TabDocumentMetadata.fretsRelativeToCapo(+Explicit)` exists (`TabDocument.h:84-85`) but nothing sets or reads it; a tab that says "fret numbers are absolute" gets the relative interpretation, off by `capoFret` semitones. Wire the flag through `TabSemanticAdapter` (emit it) and the reader (honour it); default relative; diagnostics when stated.
- **Partial capo** (`Capo 2 on strings 3-5`, Esus-style, "cejilla parcial", spider/Kyser partial): `TuningEngine` already has `setCapoStringMask/getCapoFretFor`; `ScoreTrack` has no mask and `TabPlaybackTuningSession` always sets `kAllStrings`. Add `ScoreTrack::partialCapoMask`, parse the string range in the header, apply the mask in the session; GPIF 7+ can store partial capos in staff properties (property names to confirm from alphaTab) [E].
- **Rule**: never add the capo into the tuning array and then apply it again (design doc); capo stacks on top of any open/alternate tuning.
- **Effort**: Low (absolute flag) to Med (partial capo end-to-end).

### 5.12 Technique mapping by instrument and the glyph dialect

One `GlyphDialect` table (single source of truth, shared with `TabDialectLexer`), selected by resolved family, overridden by an in-document legend:

| Glyph | Guitar (default) | Bass | Banjo | Mandolin | Steel |
|-------|------------------|------|-------|----------|-------|
| `S` | slap (today) | slap | **slide** | slide | slide |
| `s` (between frets) | slide | slide (or slap if legend) | slide | slide | slide |
| `P` / `p` | pop / pull-off (today: case split) | pop / pull-off | **pull-off** / pull-off | pull-off | pull-off |
| `T` / `t` + digit | tap | **slap (thumb)** unless legend says tap | thumb finger (ignore) | tap | tap |
| `H` / `h` | hammer-on | hammer-on | hammer-on | hammer-on | hammer-on |
| `C` between digits | - | - | **choke -> bend** | - | - |
| `tr` | trill | trill | trill | **tremolo** | trill |
| `SL` / `PO` (two-letter) | slide / pull-off | slide / pull-off | - | - | - |
| `w` | whammy (-2, no curve today) | - | - | - | - |
| `x`, `(n)`, `<n>`, `[n]`, `~`, `PM` | dead / ghost / natural harm / artificial harm / vibrato / palm mute | same | same | same | same |

`ScoreTechnique` coverage gaps (T-list in 2.3): tremolo picking, volume swell, pedal changes, pick-scrape, fade-in.

### 5.13 String->pitch mapper and re-fingering checklist (for the implementer)

1. One `pitchOf(string, fret)` including capo (global and masked), `nutFret` and any per-string offset; replace the six copies.
2. Never sort or re-octave an authoritative tuning; `assignOctaves` only for note lists/labels of monotonic candidates.
3. Family tags from the resolver, not from string count (H4).
4. `RiffTransposer::place`: pin drone strings (never relocate), use **pitch adjacency** rather than index distance when searching neighbouring strings, make the bass-on-guitar `stringOffset = +2` conditional on a monotonic descending target, support a coursed target (12-string), and report "needs N-string instrument" instead of silently dropping notes when the unreachable fraction is large.
5. `TabPlaybackTuningSession`: decide the cross-string-count policy (P1) and prove that `setNumStrings` + `refreshStringPhysics` under `ScopedStructuralChange` is safe for the loaded `GuitarSpec` (pickups, string materials, voices are sized from the spec) [verify with a test before enabling]; keep restore exact.
6. `GuitarSpecSummary` gains `coursed`/`twelveString` and `droneMask` if the placement needs them.
7. Writers (`AsciiTabWriter`, MusicXML, GPIF, MIDI) must round-trip `family`, `droneMask`, `nutFret` or at least not corrupt the order.

---

## 6. Prioritised implementation roadmap

### 6.1 Principles

1. **Fixtures first.** Every item below starts with the failing/pending fixtures in section 7; expected-to-fail cases carry their C-number.
2. **Data before code.** Tuning rows, keyword tables and ordering changes are the cheapest coverage and the least likely to regress the (heavily fuzzed) `AsciiTabReader`.
3. **One resolver, one pitch formula, one glyph table.** Collapse the scattered heuristics (5.2), the six `tuning + capo + fret` copies (1.4.2) and the two glyph tables (3.1) before adding more instruments.
4. **Additive model changes only** (defaulted fields; Riff JSON compatible).
5. **Separate "importable" from "playable".** Correct pitches/string/fret in the tab view is the import goal; instrument timbre (a banjo model) and exact retune policy are playback projects.
6. **Verify the [E] facts on real samples before coding** (banjo 5th-string numbering, GPIF slide flags, TuxGuitar container, Power Tab string orientation, GP bend position units).

### 6.2 Phases

Effort per section 0. "Entry" is where the change lands in the existing pipeline.

| ID | Item | Entry | Effort | Depends | Unlocks / notes |
|----|------|-------|--------|---------|-----------------|
| **R0 Safety net** | | | | | |
| R0.1 | Add the section-7 fixtures/tests; mark current-known-failures | `Source/Tests` | Low | - | everything |
| **R1 Coverage by data and small fixes** | | | | | |
| R1.1 | `matchTuning`: specificity order, context gating, new rows (5.3) | `AsciiTabReader.cpp` `matchTuning` / `parseTuningStatement` (used by `TabDocumentNormalizer` candidates) | Low-Med | R0 | banjo/uke/mandolin/bass/steel tunings via header |
| R1.2 | Named-count vs observed-row reconciliation with `tuningAmbiguous` candidates | `AsciiTabReader::read` `:1800-1804`, normalizer | Med | R1.1 | tenor vs plectrum, `Drop A` 6-line |
| R1.3 | Numbered-label relabel: stop forcing bass for 4/5 rows without bass context; banjo/uke/mandolin label sets | `TabTextSanitizer.cpp:262-277` | Low | R1.1 | numbered banjo/uke tabs |
| R1.4 | `parseKeyStatement`: `D-Dur` bug, `-is/-es` names, `tom/Tonart/Tonalität` keywords | `AsciiTabReader.cpp:2163` | Low | - | German/Nordic keys |
| R1.5 | Document-level German/Nordic `H/B` decision for tuning lists | `tokeniseNames` + normalizer flag | Low | R1.4 | `Stimmung: E A D G B E` |
| R1.6 | Capo: `Kapo`, wire `fretsRelativeToCapo` end to end | `readHeader`, `TabSemanticAdapter`, reader | Low | - | absolute-fret tabs |
| R1.7 | Lowercase-drone spelling (`gDGBD`) and `parseTuningNames` guard for re-entrant candidates | `parseTuningNames`/`assignOctaves` | Low | R1.1 | label/letters banjo & uke |
| R1.8 | Right-hand fingering row (`T I M R`, `p i m a`) classified as annotation | `AsciiTabReader::classify` | Low-Med | - | banjo/fingerstyle |
| R1.9 | `TabSemanticAdapter::tuningHeader` octave-safe (C15) | `TabSemanticAdapter.cpp:7-30` | Low | - | custom tunings |
| **R2 Instrument resolver and family propagation** | | | | | |
| R2.1 | `InstrumentResolver` (5.2) replacing H1-H6 | new pure module; callers: sanitizer, reader, `TabSemanticAdapter`, `NotationImporter` readers | Med | R1 | correct bass 5/6, uke/mandolin/banjo family |
| R2.2 | `ScoreTrack` fields (`family`, `droneMask`, `nutFret`, `partialCapoMask`, `coursed`) + central `pitchOf` | `PerformanceScore.h/.cpp`; replace the six formula copies | Med | R2.1 | banjo pitch, partial capo, drones |
| R2.3 | `Riff::fromScore` tag from family (reuse `RiffDestinations.cpp:428-429` mapping); optional drone info | `Riffs/Riff.cpp:537` | Low-Med | R2.1 | bass5/6 correct, no more uke-as-bass |
| R2.4 | Shared `GlyphDialect` table + in-document legend remap for `S T P s SL PO C tr` (bass/banjo/mandolin) | `AsciiTabReader` glyph switches, `TabDialectLexer`, normalizer legends | Med | R2.1 | bass thumb/slap, banjo T/S/P/C, mandolin tremolo |
| R2.5 | MIDI: GM program -> family; 5/6-string rule; GP/GPIF/MusicXML family from instrument metadata | `readMidi`, `readGpif`, `GuitarProLegacyReader`, `readMusicXml` | Low | R2.1 | bass files |
| **R3 Structured formats and multi-track** | | | | | |
| R3.1 | GPIF hardening G1-G12 (3.3) | `NotationImporter::readGpif` (+ writer parity) | Med | R2.5 | real `.gp` files; prerequisite of `.gpx` |
| R3.2 | Track/part picker: `NotationImporter` lists tracks/parts (GP legacy names exist; add GPIF, MusicXML parts, MIDI parts) + Practice UI + `setPreferredTrack` | `NotationExport.{h,cpp}`, `PracticePanel.cpp` | Med | - | band files, bass parts |
| R3.3 | `.gpx`: `GpxContainer` (BCFZ/BCFS) -> hardened `readGpif`; register the format (C17) | new reader + `detectKind/read/canRead/FileKind` + file chooser | High | R3.1 | GP6 library |
| R3.4 | GP3-5 audit: bend position units (C20), meter/tempo maps, banjo numbering | `GuitarProLegacyReader.cpp` | Low | - | |
| R3.5 | MusicXML/MIDI polish (tempo/meter maps, `<harmony>`, rehearsal marks, pitch-bend decode) | `readMusicXml`, `readMidi` | Low-Med | R3.6 decision | |
| R3.6 | Decide/extend the capture API for tempo and meter maps | `PerformanceScore` (`beginCapture/endCapture`) | Med-High | owner decision D4 | GP/MIDI/XML fidelity |
| **R4 Language layer** | | | | | |
| R4.1 | `NotationDialect` detection + header/chord transliteration stage | `TabDocumentNormalizer` + new pure module; `TabDocumentMetadata` | Med | R1.4-R1.5 | all of R4 |
| R4.2 | Chord roots/qualities: German `H/B/-is/-es`, lowercase minor, solfege roots, Brazilian `7M`, Cyrillic homoglyphs | `TabChordChart` `letterPc/parseRoot/normaliseQuality` + classifier | Med | R4.1 | chord sheets in DE/ES/IT/FR/PT/RU |
| R4.3 | Localised keyword rewrite (tuning/capo/key/step-down/sections) incl. JA/ZH/KO/RU | normalizer (rewrite to English headers) | Low-Med | R4.1 | reader untouched |
| R4.4 | Nashville/Roman hardening (key fallback + diagnostic, slash degrees, case-preserving roman) | `TabChordChart::nashvilleLine` + new roman resolver | Low-Med | R4.1 | |
| R4.5 | Jianpu/pitch-sequence reader (+ iroha/solfege melodies) | new `PitchSequenceReader`, `TabBlockKind` append, pipeline branch, `TabFingering::assign` | High | R4.1 | numbered notation |
| **R5 Playback routing** (can start after R2) | | | | | |
| R5.1 | Decide policy P1 (retune across string counts) and prove `setNumStrings` + `refreshStringPhysics` safety for the loaded spec; implement "use the tab's instrument" | `TabPlaybackTuningSession.cpp:32`, `LuthierEngine` | Med-High | owner decision D1 | 7/8-string, banjo, uke, bass on any loaded guitar |
| R5.2 | Bass routing: correct tag, notice, optional bass-spec swap | `TabReaderTab::startPlayback`, `RiffDestinations` | Med | R2.3, R5.1 | slap/pop render |
| R5.3 | `RiffTransposer`: pin drone strings, pitch-adjacency neighbour search, conditional family `stringOffset` | `RiffTransposer.cpp:144-146`, `:318` | Med | R2.2 | banjo/uke on non-matching instrument |
| R5.4 | 12-string: course-aware mapping (tab string `s` -> engine `2s`/`2s+1`) | `GuitarSpecSummary`, `RiffTransposer` | Med | R2.2 | 12-string spec |
| R5.5 | Partial capo + banjo 5th-string capo (nut-aware capo math) end to end | `ScoreTrack`, session, `TuningEngine` capo math | Med | R2.2 | spikes, Esus capos |
| **R6 Long tail** | | | | | |
| R6.1 | AlphaTex reader (direct beat builder); also a convenient fixture-authoring format | new reader | Med | - | |
| R6.2 | TuxGuitar `.tg` (verify container first; else document the export route) | new reader | Med | - | |
| R6.3 | Power Tab `.ptb` (clean-room) | new reader | High | R3.2 | legacy archives |
| R6.4 | VexTab | new reader | Med | - | low value |
| R6.5 | Pedal steel copedent | new data structure + parser | High | R2.2 | |
| R6.6 | New techniques: tremolo (mandolin), volume swell | `ScoreTechnique::Type` append + writers/vocab | Med-High | owner decision D2 | |
| R6.7 | Multi-part ASCII (Gtr I/II) -> tracks | normalizer parts -> reader | High | R3.2 | |
| R6.8 | Sargam / maqam / microtones | after R4.5 | - | | |

### 6.3 Why this order (widest coverage first)

- **R1** makes the format with the most real-world material (forum ASCII) correct for the most instruments with almost no new code paths. It removes silent mis-tunings (C14-C16) that are worse than a refusal.
- **R2** is the structural fix the instrument work needs; without it bass 5/6, banjo, uke and mandolin keep fighting four heuristics.
- **R3** serves users who bring Guitar Pro files (the format "the target audience actually uses", `spec/tab-import-export.md` 2) and band files where the part they want is unreachable (C18).
- **R4** widens the audience linguistically; the dialect stage keeps all remaps in one place so it cannot rot the backend.
- **R5** turns "importable" into "playable on whatever is loaded" and is gated on a product decision (D1).
- **R6** is long tail: low usage or high cost.

### 6.4 Risks

| # | Risk | Mitigation |
|---|------|------------|
| 1 | Regression in the mature `AsciiTabReader` (large fixture + fuzz base) | table/ordering changes first; `TabRobustnessTests`, `AsciiDialectCorpusTests`, `TabUniversalTests`, `TabDialectTests` must stay green; before/after golden diff of `TabCorpus` |
| 2 | Wrong dialect guess (German `B`, solfege in lyrics) | evidence scoring, default English, explicit directive, diagnostics that name the vote, chord-row gate |
| 3 | Model creep / Riff JSON compatibility | append-only, defaulted fields; check `Riff.cpp:909-914`, `RiffLibrary.cpp:37/426`, riff panel UI before adding instrument tags |
| 4 | Changing the engine string count at runtime (P1) | prove under `ScopedStructuralChange` with sanitizer builds; keep exact restore; fall back to today's adaptation path on any failure |
| 5 | Licensing contamination (GPL/LGPL/MPL) | clean-room process, separate spec author, `THIRD_PARTY_LICENCES.txt`, legal review before any MPL port |
| 6 | Binary-parser security (zip/BCFZ bombs, OOB reads) | bounded reads, output caps (64 MiB), partial-import-with-warning, fuzz entries |
| 7 | Fixture provenance | original, short, self-authored fixtures only |
| 8 | Silent guesses confuse users | warnings via `TabImportDiagnostics`; picker; updated refusal messages; "needs N-string instrument" instead of dropping notes |
| 9 | Two glyph tables drift (`AsciiTabReader` vs `TabDialectLexer`) | single shared `GlyphDialect` |
| 10 | Unverified [E] assumptions | verify on real samples at the start of each phase (banjo numbering, GPIF slide flags, TuxGuitar container, PTB orientation, GP bend units) |
| 11 | Scope creep into instrument synthesis | keep banjo/uke timbre out of the import project |

### 6.5 Open decisions for the owner

| # | Decision | Options |
|---|----------|---------|
| D1 | Retune policy when the tab's string count differs from the loaded instrument | keep adaptation (today); opt-in "use the tab's instrument" via `setNumStrings`; auto-load a matching `GuitarSpec` |
| D2 | New `ScoreTechnique` types (tremolo picking, volume swell, pedal) | append types (touches vocab/writers) vs expand at import vs leave unsupported with diagnostics |
| D3 | MPL-2.0 (alphaTab) | clean-room only vs permitted port of specific algorithms (BCFZ) |
| D4 | Tempo/meter maps in `PerformanceScore` | extend capture API vs keep flattened with warnings |
| D5 | Where original chord glyphs (`Lam`, `H7`) live for display | document-level side table vs a second string in `ScoreMeasure.chordSymbols` |
| D6 | Ship banjo/ukulele/mandolin `GuitarSpec`s | import-only vs also model the instruments |
| D7 | Track picker UX | single-track picker vs multi-track import with a primary audition track |

---

## 7. Test ideas and sample tabs

Conventions: tests use `Source/Tests/TestFramework.h` (`LUTHIER_TEST(Suite, name)`, `CHECK`, `CHECK_MSG`, `CHECK_NEAR`). The table-driven tuning cases live in
`Source/Tests/TabUniversalTests.cpp` (around `:110-150`: `{ line, expected MIDI, expected name }` through `AsciiTabReader::parseTuningStatement`; note the existing row
`"Afinacion: Mi La Re Sol Si Mi", {}` with the comment "solfege lists are not read (documented)", which flips when R4 lands). Playback cases belong in
`TabPlaybackTuningSessionTests.cpp`, `TabPlaybackContractTests.cpp`, `TabPlaybackCompilationTests.cpp`, `RiffTests.cpp`. Fixtures: `Source/Tests/Fixtures/TabCorpus`
(existing: `bass_four.txt`, `bass_five_low_b.txt`, `german_h_tuning.txt`, `seven_string.txt`, `steps_down_dropd.txt`, `ug_dropd_capo.txt`, GP3/4/5, MusicXML) and `AsciiDialectCorpus/Corpus.h`.
"Today" is the **predicted** behaviour from reading code ([!]); confirm by running the test first. MIDI numbers C4 = 60.

### 7.1 Tuning statements (table-driven rows to add)

| Line | Expected MIDI (tab-row order) | Name | Today |
|------|-------------------------------|------|-------|
| `Tuning: Banjo` | `{62,59,55,50,67}` | Banjo | passes |
| `Tuning: Open G banjo` | `{62,59,55,50,67}` | Banjo | `{62,59,55,50,43,38}` (row order, C14) |
| `Tuning: Double C` / `gCGCD` | `{62,60,55,48,67}` | Banjo Double C | not recognised |
| `Tuning: Sawmill` | `{62,60,55,50,67}` | Banjo Sawmill | not recognised |
| `Tuning: Tenor banjo CGDA` | `{69,62,55,48}` | Tenor Banjo | 5-string banjo row |
| `Tuning: Irish tenor banjo (GDAE)` | `{64,57,50,43}` | | 5-string banjo row |
| `Tuning: Plectrum banjo` | `{62,59,55,48}` | | 5-string banjo row |
| `Tuning: Ukulele` | `{69,64,60,67}` | Ukulele | passes |
| `Tuning: Ukulele low G` | `{69,64,60,55}` | | re-entrant row |
| `Tuning: Baritone ukulele` | `{64,59,55,50}` | | 6-string baritone guitar (C14) |
| `Tuning: Mandolin` | `{76,69,62,55}` | Mandolin | passes |
| `Tuning: Mandola` | `{69,62,55,48}` | | not recognised |
| `Tuning: Bass, Drop D` | `{43,38,33,26}` | | guitar Drop D |
| `Tuning: BEAD bass` | `{38,33,28,23}` | | `{43,38,33,28}` (the generic `bass` substring row wins) |
| `Bass tuned down a half step` | `{42,37,32,27}` | | passes (offset) |
| `Tuning: 6 string bass` | `{48,43,38,33,28,23}` | | passes |
| `Tuning: E A D G C (5 string bass)` | `{48,43,38,33,28}` | | `{43,38,33,28,23}`: the named `5 string bass` (low-B) row wins over the explicit list; letters alone (no phrase) give `{60,55,50,45,40}` |
| `Tuning: C6 lap steel` | `{64,60,57,55,52,48}` | | not recognised |
| `Tuning: Nashville` | `{64,59,67,62,57,52}` | Nashville | passes |
| `Stimmung: E A D G B E` | `{64,59,55,50,45,40}` | | passes (English B) |
| `Afinación: Mi La Re Sol Si Mi` | `{64,59,55,50,45,40}` | | `{}` (documented) |
| `Accordage : Mi La Ré Sol Si Mi` | same | | `{}` |
| `Строй: Ми Ля Ре Соль Си Ми` | same | | `{}` |

Key-statement rows (new `parseKeyStatement` table): `Tonart: D-Dur` -> D major (today predicted **D minor**, C13); `Tonart: A-Moll` -> Am; `Tonart: Fis-Moll` -> F#m; `Tonart: Es-Dur` -> Eb;
`Tonart: B-Dur` -> Bb (German context only); `Tonalidad: Sol menor` -> Gm; `Tonalité: Ré majeur` -> D (predicted to fail today: the solfege table has no accent folding, so `ré` never matches `re`); `Key: B major` -> B (English control); `キー: ト長調` -> G; `Тональность: Ля минор` -> Am.

### 7.2 ASCII tabs: instruments

- **T-BASS-1 slap/pop, 4-string, header** (regression + family):

```
Bass line - Tuning: E A D G
G|------------------|
D|------5S----7P----|
A|--0S--------------|
E|------------------|
```
Expect `numStrings 4`, tuning `{43,38,33,28}`, family bass, notes: A-string fret 0 midi 33 `slap`; D-string fret 5 midi 43 `slap`; D-string fret 7 midi 45 `pop`. `Riff::fromScore(...).instrument == "bass4"`, `bassTech` has 2 slap + 1 pop. (Passes today except the family assertion.)

- **T-BASS-2 six-string bass, letters only** [today: `{60,55,50,45,40,35}`, name "Guitar", C-list H2/H3]:

```
C|----------|
G|----------|
D|--3-------|
A|--------5-|
E|----------|
B|----------|
```
Expect tuning `{48,43,38,33,28,23}`, name/family bass, `Riff` tag `bass6`, D-string fret 3 = midi 41, A-string fret 5 = midi 38.

- **T-BASS-3 five-string high-C** rows `C G D A E` -> `{48,43,38,33,28}` (today `{60,55,50,45,40}`, name "Guitar").
- **T-BASS-4 BEAD** rows `D A E B` + the word "bass" -> `{38,33,28,23}` (today `{62,57,52,47}`).
- **T-BASS-5 thumb legend** (the legend syntax must be defined first; today's normalizer legends are `<fret>X` patterns): lines `T = thumb (slap)  P = pop` above a 4-row bass staff with `3T`, `5T` -> `slap` (today `tap`); same tab on a *guitar* staff with `T` and no legend -> `tap`.
- **T-BASS-6 two-letter glyphs**: `5SL7` -> slideLegato 5->7 (today slap + ignored `L`); `7PO5` -> pullOff (today pop + ignored `O`); `PM` and `P.M.` keep meaning palm mute; ALL-CAPS tab legend `P = pull-off` honoured.
- **T-BASS-7 numbered strings**: `1|..4|` rows under a "Bass" title -> `{43,38,33,28}` (control); same rows with no bass word and a ukulele header -> ukulele (H1).
- **T-BASS-8 family tags** (unit): `Riff::fromScore` instrument for 4-string uke, mandolin, tenor banjo = **not** `bass*`; for 5-string bass `bass5`; 6-string `bass6`; 4-string bass `bass4` (C12).

- **T-BJ-1 banjo, header, drone, fingering row**

```
Tuning: Banjo
D|--0-----0--|
B|----1------|
G|--0--------|
D|-----------|
g|-------0---|
   T  I  M
```
Expect `numStrings 5`, tuning `{62,59,55,50,67}`, family banjo5, 5 staff rows (the `T I M` row is **not** a 6th string, not `tap`), drone note midi 67 on `stringIndex 4`.
- **T-BJ-2 header-less labels** `D B G D g` -> tuning as above (today drone becomes G2 = 43, C16).
- **T-BJ-3 numbered rows** `1|..5|` -> banjo (today bass labels `G D A E B` -> `{43,38,33,28,23}`, H1).
- **T-BJ-4 short 5th string**: `g|-0-7-|` -> midi 67 then **69** (A4) [E: confirm convention]; today 67 then 74.
- **T-BJ-5 spike**: prose `5th string capo at 7` + `g|-0-|` -> open drone = 69 (`partialCapoMask = 1<<4`, global `capoFret` stays 0).
- **T-BJ-6 choke** [E: confirm the real glyph placement in banjo tabs]: `7C9`/`5C` -> `bend` (value 2) on the string row; chord name `C` in a chord row unaffected.
- **T-BJ-7 tenor vs plectrum vs 5-string**: 4 rows + `banjo` -> ambiguous, candidates `{69,62,55,48}`, `{64,57,50,43}`, `{64,59,55,50}`, `{62,59,55,48}`; `tuningAmbiguous` true.
- **T-BJ-8 alt tunings**: `Tuning: Double C` rows -> `{62,60,55,48,67}`, drone 67.

- **T-UKE-1** `Tuning: Ukulele` + rows `A E C G` -> `{69,64,60,67}`; `Riff` tag not bass.
- **T-UKE-2** label-only `A E C G` -> `{69,64,60,67}` (today low-G `{69,64,60,55}`).
- **T-UKE-3** numbered rows `1|..4|` + "ukulele" -> `{69,64,60,67}` (today bass labels).
- **T-UKE-4** `Baritone ukulele` -> `{64,59,55,50}`; **T-UKE-5** `Low G` -> `{69,64,60,55}`.

- **T-MAN-1** `Tuning: Mandolin`, rows `E A D G`, `5tr` / `===` -> tremolo (today `tr` = trill to fret+2); `Riff` tag not bass.
- **T-12-1** header `12 string guitar`, 6 rows -> `family guitar12`, `coursed`, tuning standard; playback with the 12-string spec: tab string 3 (D3 = 50) -> engine strings 6 (D3) and 7 (D4).
- **T-STEEL-1** `Tuning: C6 lap steel`, 6 rows, a column with unequal frets on different strings -> notes with identical `startBeat`, no error. **T-STEEL-2** E9 pedal steel with a 10-row staff and a copedent legend (define the legend syntax first): `5B` on string 3 with `B = 3,6 +1` -> sounding fret 5 + 1 semitone, `preBend` value 1; without legend -> bare fret + "copedent required".
- **T-NS-1** `Tuning: Nashville` rows -> `{64,59,67,62,57,52}` preserved (non-monotonic) through `TabSemanticAdapter`; and "Nashville number system" in a chart header must not trigger the tuning row.
- **T-CAPO-1** `Capo 2` + Drop D + `0` on the low string -> midi 40 (exists in session tests); **T-CAPO-2** "Capo 2 (fret numbers are absolute)" -> pitch = tuning + fret (flag wired, C-list); **T-CAPO-3** `Capo 2 on strings 3-5` -> `partialCapoMask` bits 2,3,4; **T-CAPO-4** `Kapo 3`, `カポ 3`, `Каподастр 3`, `Cejilla 3` -> `capoFret 3`.
- **T-ROW-1** reconciliation: `Drop A` header + 6 rows -> 6 strings, warning; `Tuning: banjo` + 4 rows -> ambiguity not a 5-string with a dead row.
- **T-WH-1** whammy contour `12\0` / `12w` -> `whammy` with a curve `[(0,0),(0.5,-N),(1,0)]` (today `w` -> fixed -2, no curve).

Regression anchors that must not change: plain 6-line standard tab with no header -> `{64,59,55,50,45,40}` and `lastAsciiHadResolvedTuning == true`; chord-over-lyrics file with six lines of chord names must still fall through to `TabChordChart`, not parse as string rows; existing `german_h_tuning.txt`, `bass_four.txt`, `bass_five_low_b.txt`, `seven_string.txt`.

### 7.3 Language samples

Each is a short original file; "Expect" lists what the importer must produce.

- **L-DE-1 German chart**
```
Stimmung: E A D G H E
Tonart: D-Dur
Kapo 2
D            A          H7        Em
Ich geh' durch die Stadt
```
Expect tuning standard; key **D major**; capo 2; chords D, A, **B7**, Em (today predicted: key Dm, `H7` not a chord, C13).
- **L-DE-2 German Bb** `Tonart: B-Dur` + `B  Es  F7  B` -> key Bb, chords Bb Eb F7 Bb. Negative control: `Key: B major` + `B  E  F#m` stays B natural (no German evidence).
- **L-DE-3 lowercase minor** (German evidence present): `a  e  H7  e` -> Am Em B7 Em; without evidence these are not chords.
- **L-ES-1 Spanish**
```
Afinación: Mi La Re Sol Si Mi
Cejilla en el traste 2
Lam         Re7        Sol
La vida es bella
```
Expect tuning standard, capo 2, chords Am D7 G; the lyric line "La vida es bella" is **not** a chord line.
- **L-FR-1** `Accordage : Mi La Ré Sol Si Mi / Capodastre 3 / Do  Sol  Lam  Fa`; **L-IT-1** `Accordatura: Mi La Re Sol Si Mi / Capotasto 1 / Do Sol Lam Fa` with lyric "La vita è bella"; **L-PT-1** `Afinação ... Capotraste 2`, cifra `C7M  Dm7  G7(b9)  C7M` -> Cmaj7 Dm7 G7(b9) Cmaj7.
- **L-NS-1 Nashville**
```
Key: G
1   4   5   1
6-  2-  5   1
```
Expect chords G C D G / Em Am D G (bare numbers major, `-` minor). **L-NS-2** no key -> default C with diagnostic "assumed key C". **L-NS-3** `1/3  4  5` -> `G/B C D`. **L-NS-4** a 6-line ASCII tab whose first row is `e|--1--4--|` must stay a tab.
- **L-RN-1 Roman**
```
Key: C
I   vi   IV   V
ii  V7   I
```
-> C Am F G / Dm G7 C (case read before lower-casing; `vii°` -> diminished).
- **L-JP-1 Jianpu**
```
1=C  4/4  q=96
1 2 3 4 | 5 - 5 - | 6 5 4 3 | 2 - - - |
```
Expect 4 bars: quarters C D E F; G half, G half; A G F E; D whole. Midi (tonic at C4): 60 62 64 65 | 67 (2 beats) 67 (2) | 69 67 65 64 | 62 (4). Missing `1=C` -> default with diagnostic. `1=G` header shifts every pitch by +7. `#4` in C = F# (66); `b7` = Bb (70). Fingered through `TabFingering::assign` on the loaded/standard tuning. A fret-row control must not trigger it.
- **L-JA-1** `キー: ト長調` -> G major; `変ロ長調` -> Bb major; `嬰ヘ短調` -> F# minor; `カポ: 2`; full-width `Ｃ  Ａｍ  Ｆ` chords and `０-９` digits fold to ASCII; `ロ` stays B natural.
- **L-RU-1** `Строй: стандартный`, `Тональность: Ля минор`, `Каподастр 2`, chord row with Cyrillic look-alikes `Аm  Dm  Е` -> Am Dm E; the lyric line stays lyrics.
- **L-ZH-1** `原调 E 选调 C 变调夹 4` -> capo 4; `Meta.key` E (sounding), shape key C in a diagnostic.
- **L-KO-1** `카포 2`, `튜닝: 반음 내림` -> capo 2, half step down.
Dialect safety: an English tab containing the words `Es`, `As`, `Am`, `La`, `Do`, `Si` in lyrics yields no dialect switch.

### 7.4 Structured formats

| ID | Fixture | Expect |
|----|---------|--------|
| S-GP5-1 | band `.gp5` with guitar + bass + drums (extend `multitrack_band.gp5`) | picker lists tracks; selecting bass gives `{43,38,33,28}`, slap/pop notes; drums never selectable |
| S-GP5-2 | note with a mid-note bend point at file position 30 (of 60) | curve point at position 0.5 (today predicted clamped to the end, C20) |
| S-GP5-3 | 3/4 -> 4/4 meter change | document current flattening (warning) until R3.6 |
| S-GP5-4 | banjo track (5 strings, 5th-string fret 7) | pitch convention confirmed before coding (E) |
| S-GPIF-1 | GPIF with `Tuning` `40 45 50 55 59 64` | `tuning {64,59,55,50,45,40}`, `Fret/String` mapping |
| S-GPIF-2 | slide flags 1,2,4,8,16,32 individually | shift, legato, out-down, out-up, in-below, in-above (real GP semantics, G1) |
| S-GPIF-3 | `Tie origin/destination`; triplet `PrimaryTuplet 3/2`; repeat + 1st/2nd ending; `Slapped`/`Popped` | tied note extended; triplet durations 2/3 of the straight value; unrolled bars; slap/pop techniques |
| S-GPIF-4 | 3-track file (guitar, bass, drums first) | picker; drums skipped |
| S-GPX-1 | hand-built `BCFS` container with `score.gpif` | same score as the `.gp` of the same GPIF |
| S-GPX-2 | `BCFZ` literal-only + one back-reference | decoder bit-reader correct |
| S-GPX-3 | truncated/oversized/looping back-reference | error or partial import, no crash, output capped |
| S-XML-1 | banjo `staff-tuning` (line 1 = g4) | `{62,59,55,50,67}`; `<string>5</string><fret>0</fret>` = 67 |
| S-XML-2 | bass part without `<string>/<fret>` and with `<transpose><octave-change>-1` | fingered on bass range; without `<transpose>` documented behaviour |
| S-XML-3 | `<bend><bend-alter>2</bend-alter><pre-bend/>`, `<harmony>`, rehearsal marks | `preBend` curve; chordSymbols; sectionName |
| S-MID-1 | GM program 33 + lowest note B0 (23) | 5-string bass `{43,38,33,28,23}`; no clamping |
| S-MID-2 | band MIDI (guitar 27, bass 33, drums ch.10) | part list; bass selectable |
| S-MID-3 | pitch-bend +/-2 on a note | `bend` curve (RPN 0 range) |
| S-ATEX-1 | `\title "T" \tempo 100 \tuning e4 b3 g3 d3 a2 e2` `.` `:4 0.6 2.6 3.6 5.6 \|` (verify syntax) | string-6 notes frets 0 2 3 5, quarter durations, tuning standard |
| S-VEX-1 | `tabstave notation=false tuning=standard` / `notes :q 5/6 7/6 :h 5/5` (verify stickiness) | stringIndex 5,5,4; durations 1,1,2 |
| S-TG-1 / S-PTB-1 | files authored in TuxGuitar / Power Tab Editor 2 | after the container/orientation facts are verified |

Fixture authoring: GPIF, MusicXML, AlphaTex and `BCFS` can be hand-written or generated by a tiny script under `Tools/`; `.gp3-5` via TuxGuitar export; `.ptb`/`.tg` via their editors. **Do not** use Luthier's own GPIF exporter as the only `.gp` fixture (it hides G1).

### 7.5 Playback contract

| ID | Case | Expect |
|----|------|--------|
| P-1 | `Riff::fromScore` tags | uke/mandolin/tenor banjo not bass; bass4/5/6 by strings (C12) |
| P-2 | 4-string uke riff on a loaded 6-string guitar | no `+12`/`+2` family shift (or an explicit documented one) |
| P-3 | 5-string banjo with an exact 5-string retune | drone note stays `stringIndex 4`, midi 67; session restore leaves `TuningEngine` byte-identical |
| P-4 | banjo riff, no retune (6-string guitar) | drone not relocated to a non-pinned string; "needs 5-string instrument" notice, or pinned fingering |
| P-5 | 6-row tab with the 12-string spec loaded | tab string `s` -> engine string `2s` (+ `2s+1` if the engine doubles) |
| P-6 | 5-string bass tab on a loaded 4-string bass | `bass5` tag; low-B notes dropped with a notice, not shifted -12/-2 |
| P-7 | slap/pop on a loaded non-bass guitar | graceful fallback documented (accentuated pluck) |
| P-8 | partial capo `1<<4` with `nutFret[4] = 5`, capo at 7 | open 5th string sounds 69 |
| P-9 | cross-string-count retune (if D1 chooses it) | engine returns to its prior string count, tunings, capo and mask exactly; sanitizer builds clean |

### 7.6 Robustness

Every new reader gets entries in `TabRobustnessTests` fuzzing: random bytes; truncation at every offset of a good fixture; counts at `INT_MAX`; zip/BCFZ bombs; back-references beyond the output; nested/looping repeats; huge tracks; invalid UTF-8; mixed EOLs. Expected: no crash, bounded time and memory, partial import with warnings or a clear error. Text paths: lines of 16 k characters, 100 k lines, deeply nested legends, Cyrillic/CJK in every header position, mixed scripts, BOM + UTF-16.

### 7.7 Corpus hygiene

All fixtures original and short; record provenance in a comment; no third-party tabs or scores; every dialect fixture added to `AsciiDialectCorpus`/`TabCorpus` with its expected tuning, key, capo, chord list and diagnostics so dialect auto-detection can be trusted.

---

## Appendix A. Source reference index

| Area | File / symbol |
|------|---------------|
| Data model | `Source/Notation/PerformanceScore.h` (`ScoreTechnique :26`, `ScoreNote :56`, `ScoreTrack :108`, `PerformanceScore :123`), `PerformanceScore.cpp` (`beginCapture :159`, `noteStarted :176`, `endCapture :261`) |
| Importer entry | `NotationExport.h` (`FileKind :153`, `NotationImporter :143`), `NotationExport.cpp` (`canRead :1364`, `detectKind :1379`, `read :1425`, `readGuitarProLegacy :1480`, `readCompressedMusicXml :1501`, `chooseMelodicPart :1562`, `readMidi :1677`, `readAsciiTab :1780`, `readMusicXml :1805`, `readGuitarPro :2212`, `readGpif :2241`; GPIF writer slide flags `:1182-1204`) |
| ASCII pipeline | `TabImportPipeline.cpp :9/:91`, `TabDocumentNormalizer`, `TabTextSanitizer.cpp` (numbered labels `:255-285`, bass hint `:416-429`), `TabSemanticAdapter.cpp :7-30/:87`, `AsciiTabReader.cpp` (see below), `TabDirectiveApplier.cpp`, `TabKeyDetector`, `TabChordChart.cpp`, `TabFingering.{h,cpp}`, `UI/PracticeTabImporter.h :17` |
| `AsciiTabReader.cpp` | `pitchClassOfLetter :101`, `tokeniseNames :116`, `assignOctaves :172`, `spanOf :219`, `matchTuning :230`, `stepsDownSemitones :296`, `romanNumeralIn :363`, `kTuningKeywords :391`, `classify :453`, `readHeader :697`, glyph switches `:1266-1287` / `:1422-1520`, track build `:1797-1824`, emit `:1962-1992`, `looksLikeTuningStatement :2038`, `parseTuningStatement :2059`, `parseKeyStatement :2163` |
| Token path (not production) | `TabDialectLexer.{h,cpp}` (`lexRow :56`), `TabTechniqueCompiler.cpp` |
| GP3-5 | `GuitarProLegacyReader.{h,cpp}` (`looksLike* :784-802`, `readTracks :272`, `readBend :330`, build score `:991-1098`) |
| Playback | `UI/PracticePanel.cpp :1884 startPlayback`, `Practice/TabPlaybackTuningSession.{h,cpp}` (gate `:32`), `Riffs/Riff.cpp` (`pitchOf :406`, `fromScore :505`, instrument tag `:537`, bassTech `:566`), `Riffs/RiffTransposer.{h,cpp}` (`GuitarSpecSummary`, `place :130`, family shift `:144-146`, neighbour search `:318`), `Riffs/RiffDestinations.cpp :32, :288, :428` |
| Tuning / voicing | `Model/Playing/TuningEngine.{h,cpp}` (presets `:47-65`, capo `:132-170`), `Model/Playing/RubricVoicer.h` (`coursed :366`), `Model/Guitar/GuitarLibrary.{h,cpp}` (`GuitarCategory :41`, `GuitarSpec :44`, 12-string helpers `:106-111`, `:448`), `LuthierEngine.cpp` (`applyTwelveStringTuning :849`) |
| Specs | `spec/tab-import-export.md`, `spec/bass-techniques.md`, `spec/notation-export.md`, `spec/instruments/extended-range-bass.md`, `spec/instruments/tenor-guitar.md`, `spec/microtonal-bends.md` |
| Existing research | `docs/research/TAB_FORMATS.md`, `docs/audit/TAB_IMPORT_RESEARCH_NOTES.md`, `docs/superpowers/specs/2026-10-02-tab-*-design.md` |

## Appendix B. External references named by the research (verify before relying)

alphaTab (alphatab.net, github.com/CoderLine/alphaTab; MPL-2.0): GPIF, GPX/BCFZ, AlphaTex, GP3-5. PyGuitarPro (pyguitarpro.readthedocs.io; GP3-5 format doc). `dart_gp_tab_reader`, `gp_parser`
(ports). MuseScore forum "The Guitarpro 7 (.GP) file format". ptabtools (github.com/jelmer/ptabtools), Power Tab Editor 2.x (github.com/powertab/powertabeditor). TuxGuitar (github.com/helge17/tuxguitar).
MusicXML 4.0 (w3.org/2021/06/musicxml40). VexTab/VexFlow (vexflow.com/vextab, MIT). Instrument/notation sources: Soundslice bass slap/pop help, Alfred slap-bass legend (S, P, H, PO, SL, X),
Guitar Pro 8 slap tutorial, Zager (p/pu = pull-off), banjo tab guides (dummies.com, flat.io, mixingaband.com, nativeground.com, melbay), tenor/plectrum banjo threads, Nashville tuning
(Wikipedia, Premier Guitar), Soundslice pedal steel, ukulele/re-entrant guides, Wikipedia Jianpu / Solmization / Iroha / Letter notation / Key signature names and translations.


---

## Addendum — additional formats & languages (deep research)

Scope: formats and regional notations **not** covered in sections 3 and 4 (3.13 and 4.3.9 left them unresearched). Notes only; nothing was run or built.
Tags as in section 0, plus **[W]** = checked against a web source in this pass (list in A.7), **[E]** = recall/general knowledge, verify against a real sample first.
Search coverage was thin for proprietary formats (no public specs surfaced for `.tef`, `.musx`, `.capx`, `irealb://`); that absence is itself the finding. Vocabulary: `stringIndex 0` = top printed row, `tuning[]` in tab-row order (section 0).

### A.1 Cross-checks that touch existing sections (read first)

| ID | Finding | Evidence | Impact |
|----|---------|----------|--------|
| A1-1 | **GP3-5 bend value is 100 per whole tone, not 25 per semitone.** `GuitarProLegacyReader.cpp:1082-1088` divides by 25 (`peak / 25.0`, `p.second / 25.0`), so a full bend (100) imports as **4.0 semitones, double** [!]. GPIF is also 100 per whole tone (alphaTab: `1/25` per quarter-step); the GPIF reader's `/50` (`NotationExport.cpp:2439`) is right. | DGuitar javadoc `GPBendPoint`: value "bend height (100 per tone)", position "sixtieths of the note duration" [W]; alphaTab `GpifParser`: `_bendPointValueFactor = 1/25` "25 per quarternote", positions "GPX range 0-100" vs internal 0-60 [W] | Contradicts the "`/25` -> semitones agrees" remark in C20 and the 3.11 bend row; confirm with a GP5 fixture holding a known full bend, then fix with C20 (positions: GP3-5 `/60`, GPIF `/100`) |
| A1-2 | GPIF slide flag bits verified from alphaTab source: 1 shift, 2 legato, 4 out-down, 8 out-up, 16 in-from-below, 32 in-from-above, 64 pick-slide-down, 128 pick-slide-up | [W] | Confirms G1 |
| A1-3 | GPIF `HarmonicType` values: `noharmonic, natural, artificial, pinch, tap, semi, feedback` + `HFret` (float) | [W] | `semi` -> `artificialHarmonic` (approx), `feedback` has no `Type` (warn); G8 uses `HFret` |
| A1-4 | GPIF `Slapped`/`Popped` are **beat**-level (`Enable` child); Luthier techniques are per note | [W] | Apply to every note of the beat when mapping (G6) |

### A.2 Additional formats

#### A.2.0 Summary and verdicts

| # | Format | Encodes | Open spec / parser (licence) | Fit to model | Verdict |
|---|--------|---------|------------------------------|--------------|---------|
| 1 | ABC notation | pitch + rhythm melody text, chord symbols, tune books | spec public (abcnotation.com); abcjs MIT; abc2midi/abcm2ps GPL (doc only) [E] | pitch-only -> `TabFingering::assign`; chords -> `chordSymbols` | **Worth** (Low-Med): folk/trad, mandolin/banjo/fiddle |
| 2 | ChordPro / OnSong / songbook charts | lyrics + inline chords, `define` voicings, capo/key, tab blocks | ChordPro public docs, reference impl Artistic-2.0 [E] | text -> existing chord-chart + ASCII paths | **Worth** (Low) |
| 3 | Rocksmith 2014 arrangement XML | per-note string/fret/technique, chords, tuning offsets, capo (time in seconds) | no formal spec; community tools (Rocksmith2014.NET, RocksmithToTab; licences not stated in READMEs) [W] | near-isomorphic after seconds->beats | **Maybe** (Med): CDLC niche + legal; GP route exists |
| 4 | MuseScore `.mscz/.mscx` | full score + tab staves | no spec (code is GPLv3) [W/E] | string/fret kept in tab staves | **Maybe** (Med), after GPIF hardening; today: "export MusicXML" |
| 5 | TablEdit `.tef` | tab + notation, banjo/mandolin/folk | **none public** [W] | n/a | **Skip native**; detect + advise export |
| 6 | iReal Pro | chord grid only | de-facto, obfuscated URL; decoders exist [E] | chord symbols only | **Skip** v1 |
| 7 | Finale `.musx`/`.mus`, Sibelius, Dorico, Capella, Band-in-a-Box | scores/charts | proprietary (`.musx` zip+XML, community RE; Finale sunset Aug 2024) [W] | via MusicXML/MIDI | **Skip native** |
| 8 | Web scores: Soundslice, Flat, Noteflight, Songsterr | cloud notation | MusicXML/GP exchange; Songsterr no public API [W/E] | via MusicXML/GP | **Skip native**; document export routes |
| 9 | LilyPond, Humdrum `**kern` | engraving code / pitch text | GPL / BSD (humlib) [E] | pitch-only | **Skip** |
| 10 | MEI/TabMEI, TabCode (lute) | tablature encodings | open [W] | course/fret | **Skip** unless early-music |
| 11 | Drum-tab family (ASCII, DTX, GM MIDI) | percussion | n/a | no string/fret | **Keep refused**; see A.2.10 |
| 12 | Rock Band Pro Guitar MIDI | string+fret in MIDI | community docs [W] | maps cleanly | **Skip** (copyrighted DLC), useful precedent |
| 13 | Ultimate-Guitar saved-page JSON | tab text + tuning/capo/key meta | undocumented [E] | metadata | **Maybe** (Low): metadata first |
| 14 | "Bucket o' Tab" | - | no such format found [W] | - | treat as ASCII |

#### A.2.1 ABC notation (`.abc`, pasted)

- **Encodes**: header fields `X: T: M: L: Q: K:` (K last), body tokens: `C`=C4, `c`=C5, `C,`=C3, `^ _ =` accidentals, `L:` default length, `>` broken rhythm, `(3` tuplets, `-` ties, `()` slurs, `"Am"` chord symbols, `{g}` grace notes, `!trill!`/`T`, `|: :| [1 [2` repeats, `V:` voices, `%%MIDI program N`. Multi-tune files (`X:` per tune; session sites ship hundreds). Keys: `K:D`, `K:Ador`, bagpipe `K:HP/Hp` (pipe scale: implied F#/C#, G natural, none printed) [E].
- **Tab?** None in the text. abcjs renders tab at display time via a `tablature` option (instruments `violin, mandolin, fiddle, guitar, fiveString`), not from ABC syntax [W]. So ABC is pitch-only.
- **Parse**: small hand-written tokenizer (~300 lines): header -> key/mode accidental table (persist per bar, reset at barline) -> body. Do not copy abc2midi/abcm2ps (GPL).
- **Map**: `midiNote` set, then `TabFingering::assign` with an instrument chosen by user/`%%MIDI program`/`N:` text (fiddle/mandolin/tenor banjo GDAE, guitar melody); `"Am"` -> `chordSymbols`; `Q:` -> tempo; `M:` -> meter (single-meter limit, 3.0); ties -> `tiedFromPrevious`.
- **Gotchas**: ABC `~` is the Irish **roll** ornament, not vibrato; `.` staccato; `H` fermata, `T` trill (all collide with tab glyph meanings, but only inside ABC text, so keep the reader separate); tune picker needed (P4-like); repeats unrolled.
- **Detect**: `.abc` or first lines `X:`/`T:`/`K:`.

#### A.2.2 ChordPro / OnSong / songbook formats (`.cho .crd .chopro .chordpro .pro .onsong`)

- **Encodes** [W]: directives in braces on their own line; inline `[Am]` chords before the syllable; `{define Dm base-fret 1 frets x 0 3 2 3 1 fingers 0 0 3 2 1 4}` (ChordPro 6; older `{define: name base-fret N frets ...}`), positions enumerated **left=lowest to right=highest**; instrument selectors `{define-ukulele ...}`/`{define-guitar ...}`; `{capo: N}`, `{key: C}` (may repeat mid-song), `{start_of_tab}`/`{end_of_tab}` (`{sot}/{eot}`) wrapping monospaced tab. Section directives `{start_of_verse/chorus/bridge}` (`{sov} {soc} {sob}`), `{c: comment}` [E]. OnSong is a ChordPro superset with `Key: Capo: Tempo: Time:` metadata lines and `Verse 1:` headings [W/E].
- **Parse**: line-based, ~1 day. Convert to the chord-over-lyrics model already consumed by `TabChordChart`; hand `start_of_tab` blocks to the ASCII pipeline; `capo`/`key` into `TabDocumentMetadata`.
- **Map**: chord names -> `chordSymbols` (needs the dialect stage for non-English roots, 4.2). `define` gives the **author's real voicing**: frets lowest->highest, reverse into tab-row order, `x`/`-1` = muted string; use it for strum/arpeggio instead of the lookup voicer.
- **Verdict**: worth it; also the cleanest target format for exporting charts.

#### A.2.3 Rocksmith 2014 (arrangement `.xml`, compiled `.sng`, archive `.psarc`)

- **Encodes** [E unless marked]: `<song version=...>` metadata (`title, arrangement` Lead/Rhythm/Bass/Combo, `offset, centOffset, songLength, averageTempo`), `<tuning string0..string5>` = **semitone offsets from standard** (E A D G B E; bass E1 A1 D2 G2), `<capo>`, `<arrangementProperties>` flag soup, `<ebeats>` (`time` seconds, `measure` number), `<chordTemplates>`, `<levels>` (several `<level difficulty=n>` = dynamic difficulty; **take the highest**), per level `<notes>` with attributes `time sustain string fret bend slideTo slideUnpitchTo hammerOn pullOff harmonic harmonicPinch palmMute mute tremolo pluck slap tap vibrato accent linkNext ignore leftHand`, child `<bendValues><bendValue time step>`, plus `<chords><chord time chordId><chordNote ...>`, `<handShapes>`, `<anchors>`. **`string 0` = lowest** (reverse to tab-row order).
- **Time is in seconds**, no written rhythm: derive beats from `ebeats` (interpolate), tempo from ebeat spacing, meter from beats per `measure` number, duration from `sustain`, then quantise to a grid. This is the main cost (shares the quantiser with ASCII/VexTab).
- **Tools**: RocksmithToTab reads `.psarc` or XML (`-x`) and writes `.gp5/.gpx/.gpif`, "supports all Rocksmith techniques"; Rocksmith2014.NET offers XML/SNG/PSARC libraries and an XML<->SNG converter; neither README states a licence [W]. Documentation only.
- **Technique map**: hammerOn/pullOff -> same; bend+bendValues -> `bend` + `curve` (verify `step` unit, semitone vs whole tone, on a sample); harmonic -> `naturalHarmonic`, harmonicPinch -> `pinchHarmonic`; palmMute -> `palmMute`; mute -> `deadNote`; slideTo -> slide with target fret; slideUnpitchTo -> slide-in/out (approximate); slap/pluck -> `slap`/`pop` (bass); tremolo -> **no `Type` (T5)**; linkNext -> tie; ignore/leftHand -> drop. Ghost notes and grace notes do not exist in Rocksmith [W].
- **Legal**: official DLC is encrypted and copyrighted; community CDLC (CustomsForge) are third-party transcriptions. Accept only a user-supplied XML; never ship or fetch.
- **Verdict**: Med effort; build only on demand. Cheaper route: user converts to GP5 with the existing tools.

#### A.2.4 MuseScore (`.mscz` zip, `.mscx` XML)

- **Container** [E]: `.mscz` = zip with `META-INF/container.xml` -> `*.mscx` (+ thumbnail/images; 4.x adds JSON settings files). Same unzip+XML plumbing as `readCompressedMusicXml`/`readGuitarPro`.
- **Tab data** [E, verify on a sample]: tab staves hold `<Note>` with `<pitch> <tpc> <string> <fret>`; instrument `<StringData>` lists `<frets>` and open-string `<string>` pitches (lowest first); note `string` index counts from the top line (0 = highest) so it equals `stringIndex` directly. Techniques: `Tie`, slurs for hammer/pull, `Bend` point lists, harmonic/palm-mute/let-ring as articulations or line spanners (4.x), `Tuplet`, `Dynamic`, `Harmony` (chord symbols), `RehearsalMark`, repeats/voltas.
- **Spec**: none; handbook documents UI only; third-party readers report unsupported elements [W]. Source is GPLv3: documentation only. Format drifts 2 -> 3 -> 4, so pin test samples per major version and warn on unknown elements.
- **Why bother**: MuseScore's MusicXML export keeps `<string>/<fret>` but forum reports its **import** re-guesses strings (round-trip loss) [W]; native `.mscx` is the only lossless route for tab authored there. Today the message should say "Export MusicXML (File > Export)".
- **Verdict**: Maybe, Med, after GPIF hardening.

#### A.2.5 TablEdit `.tef` and commercial score apps

- **TablEdit**: proprietary binary; no public spec, no maintained open parser found; filext reports non-uniform internals (some start `00 00`, ~6% zip-like) [W]; free viewer TEFview and iOS TEFpad exist [W]. Large in banjo/mandolin/Irish/folk circles [E]. TablEdit exports MusicXML, MIDI, ABC, ASCII tab and GP [E]: make those paths excellent, detect `.tef` and answer with that advice (extend the `.gpx/.ptb` refusal pattern, `NotationExport.cpp:1455-1464`). Banjo caveat: a MusicXML export cannot carry the short 5th string (C21/5.4).
- **Finale** `.musx`: zip with `NotationMetadata.xml` + `score.dat`; legacy `.mus` is the "Enigma" binary; no official spec, community reverse engineering only; Finale reached end of life Aug 2024 [W]. **Sibelius** `.sib`, **Dorico** `.dorico`, **Capella** `.cap` (binary) / `.capx` (XML in zip; no schema surfaced in search [W]), **Band-in-a-Box** (`.sgu/.mgu`, chord/style engine, MIDI/MusicXML export) [E]: all proprietary. Route: MusicXML/MIDI.
- **Web apps**: Soundslice imports MusicXML, Guitar Pro, Power Tab, TuxGuitar; its API exports MusicXML only from its own exporter (not the uploaded original) and it advises uploading native GP rather than MusicXML-from-GP [W]. Flat: REST `api.flat.io/v2`, MusicXML/MIDI import-export, GP/MuseScore/Power Tab per third-party catalog [W]. Noteflight: MusicXML export (premium) [E]. Songsterr: proprietary, no public data API, scraping breaches ToS [E].
- **Implication**: the MusicXML reader is the universal back door; its gaps (3.7: `<harmony>`, repeats, meter changes, `<other-technical>` palm mute, `<transpose>`) matter more than any native reader. Keep "pitch-only parts -> `TabFingering`" robust for Noteflight/Sibelius/Dorico exports.

#### A.2.6 iReal Pro

- **Encodes** [E]: chord grid only (sections `*A *B`, bars, `{ }` repeats, `N1 N2` endings, `T44` meter, `Q` coda, `S` segno, `f` fermata, qualities `^7` maj7, `-7` min7, `h7` half-dim, `o7` dim7, `7alt`, style, tempo, key). URL schemes `irealb://` (character-scrambled) and legacy `irealbook://` (plain); hobbyist decoders exist, no authoritative spec found [W].
- **Map**: `ScoreMeasure.chordSymbols` only; no notes. **Verdict**: skip for v1 (no notes, charts of copyrighted standards, decoder licences unknown); revisit as a "chord chart" import mode.

#### A.2.7 LilyPond and Humdrum

- **LilyPond** `.ly` is Scheme-extensible (not statically parseable); `\new TabStaff`, string by `c\3`, `stringTunings = \stringTuning <e, a, d g b e'>` (lowest first) [E]. No MusicXML writer in core. **Skip.**
- **Humdrum `**kern`** [E]: tab-separated spines; `4c` quarter C4 (`c` C4, `cc` C5, `C` C3), `-` flat, `r` rest, `=` bar, `[ ]` ties; corpora (Essen folk, chorales). Pitch-only, BSD (humlib). **Skip**; if ever, reuse the pitch-sequence core (A.3.0).

#### A.2.8 MEI / TabMEI / TabCode (lute)

- TabCode is an ASCII encoding of lute tablature (Crawford; ECOLM corpus); MEI has a lute tablature customisation and TabMEI work extends it to guitar tab; `luteconv` converts TabCode/MEI/MusicXML/"Tab" [W]. Relevant only to historic tablature (A.3.12). **Skip.**

#### A.2.9 TuxGuitar export variants

- TuxGuitar writes native `.tg` plus GP3-5, MusicXML, MIDI, LilyPond, PDF, ASCII, and reads GP/Power Tab/MIDI/MusicXML [E]; Soundslice also accepts `.tg` [W]. Treat every TuxGuitar export as "just the target format", so the only unique reader is `.tg` itself (3.6). Skip beyond 3.6.

#### A.2.10 Drum-tab family (stays refused; if ever supported)

- Dialects: ASCII (`HH|x-x-x-x-|`, `SD|----o---|`, `BD|o-------|`; `x` hit, `o` normal, `O` accent, `g` ghost, `f` flam, `d` drag) [E]; GP percussion tracks (GM drum numbers; GP6 GPIF uses `Element`+`Variation`, GP7+ `InstrumentSet` [W]); MusicXML `<unpitched>` + `<midi-unpitched>`; MIDI channel 10 GM map (35/36 kick, 38 snare, 42 closed hat, 46 open hat, 49 crash, 51 ride) [E]; DTXMania `.dtx` text charts and Clone Hero `.chart` 5-lane files (no pitch, not tab) [E].
- If supported: add a `percussion` flag on `ScoreTrack`, store GM number in `midiNote`, never run `TabFingering`/`stringIndex`. Keep the "drum tab refused" diagnostic until then.

#### A.2.11 Rock Band Pro Guitar MIDI

- Expert notes 96-101 (easy 24-29, medium 48-53, hard 72-77), **velocity = 100 + fret** (100-117, or up to 122 for 22-fret variants) [W]; example note 97 / velocity 105 = A string fret 5, so 96 = lowest string [W]. Channel carries technique flags (ghost/bend/mute/tap/harmonic) [E]. Copyrighted game DLC: skip. Value: a precedent for "string+fret in MIDI" next to Luthier's own profile (3.8).

#### A.2.12 Ultimate-Guitar saved pages and "Bucket o' Tab"

- UG tab pages embed a JSON blob (a `js-store` element's `data-content`) holding the tab text with `[ch]`/`[tab]` markup and meta fields for tuning, capo, key ("tonality"), difficulty [E recall, check a saved page]. For user-saved HTML the importer could read these **before** heuristics (cheap win for the existing HTML path). "Official/Pro" tabs are proprietary. User-saved files only.
- "Bucket o' Tab": no such file format or spec found [W]; assume ASCII.

#### A.2.13 Guitar Pro `.gpif`: extra quirks beyond G1-G12 (from alphaTab `GpifParser`) [W]

- Pools are id-referenced (`Bars`, `Voices`, `Beats`, `Notes`, `Rhythms`), parents list child ids as space-separated text; `-1` = no voice [E].
- `Tuning` `Pitches` lowest first (alphaTab reverses); `String` is 0-based from the lowest (alphaTab adds 1); `CapoFret` child `Fret`; partial capo not seen in the parsed excerpt.
- `Tie` element with `destination`; `HopoOrigin` read, `HopoDestination` deliberately ignored ("calculated automatically"): derive hammer vs pull from frets (G9).
- Bend: `Bended` + `BendOriginValue/Offset`, `BendMiddleValue/Offset1/Offset2`, `BendDestinationValue/Offset`; defaults origin (0,0), destination at end; **a zero middle value is skipped**; no offsets -> middle at half. Offsets are percent (0-100) of the note.
- Whammy: `WhammyBar`, `WhammyBarExtend`, `...OriginValue/OriginOffset/MiddleValue/MiddleOffset1/2/DestinationValue/DestinationOffset`; `VibratoWTremBar`.
- `Rhythm`: `NoteValue` strings `Long, DoubleWhole, Whole, Half, Quarter, Eighth, 16th, 32nd, 64th, 128th, 256th`; `AugmentationDot count`; `PrimaryTuplet num/den`.
- Other: `Muted` = dead, `PalmMuted`, `LetRing`, `Tapped` (note) vs `LeftHandTapped`, `Vibrato` Slight/Wide, `Slapped/Popped` (beat). Direction targets contain a typo (`DaSegno` for `DalSegno`) and GP6 percussion used `Element`+`Variation`. Chromatic `Transpose` also shifts the key signature; sustain pedals live only in track automations; backing-track frame offsets assume 44100 Hz.

#### A.2.14 Suggested order for the new formats

1. ChordPro (Low) -> 2. ABC (Low-Med) -> 3. `.tef`/`.musx`/`.sib`/`.capx`/iReal "export first" messages (Low, one table) -> 4. UG JSON metadata (Low, verify) -> 5. MuseScore (Med) -> 6. Rocksmith XML (Med, on demand). Everything else: skip. Net effect: only two genuinely new parsers (ChordPro, ABC) plus data tables.

### A.3 Additional languages and regional notations

Section 4 already covers: German/Nordic/CEE letter names (4.3.1), fixed-do (4.3.2), Nashville (4.3.3), Roman numeral harmony (4.3.4), jianpu incl. Indonesian *not angka* (4.3.5), Japanese iroha/kana (4.3.6), Russian names + chord-row homoglyphs (4.3.7), Chinese/Korean headers (4.3.8), header vocabulary table (4.3.10), ambiguity #1-#17 (4.4). Below is only what those omit.

#### A.3.0 One pitch-sequence core for every non-tab melodic notation

Sargam, gongche, kepatihan, tonic sol-fa, jianpu, Korean/Japanese letter melodies and ABC all reduce to the same decode, so build `PitchSequenceReader` (4.3.5) around a table, not per-system code:

| System | Degree tokens | Variants / accidentals | Octave | Rhythm and rests | Tonic comes from |
|--------|---------------|------------------------|--------|------------------|------------------|
| Jianpu (4.3.5) | 1-7 | `#`/`b` relative to degree | dots | dashes, underlines | `1=X` |
| Sargam, Hindustani (A.3.2) | S R G M P D N (Sa Re Ga Ma Pa Dha Ni) | case: lower = komal (r g d n); `M` = tivra | dot under = lower, over = upper | `-`/`,` extend, bars = matras, tala marks | "Sa = X" / scale line, else C |
| Sargam, Carnatic (A.3.2) | S R G M P D N + digit | R1-3, G1-3, M1-2, D1-3, N1-3 | dots | commas, `;` | tonic (sruti) |
| Gongche (A.3.3) | 上尺工凡六五乙 (+ 合四一) | 凡/乙 flexible | radical prefix | 板眼 marks | mode (調) |
| Kepatihan (A.3.6) | 1-7 | none | dots | grouped by gatra of 4 | ensemble tuning |
| Tonic sol-fa, Welsh/Curwen (A.3.11) | d r m f s l t | -e raised / -a lowered syllables | `'` `,` or sub/superscript | `:` `|` `.` `,` | "Key X" |
| Letter melodies JA/KO (A.3.4, A.3.5) | ハニホヘトイロ / 가나다라마바사 | 嬰/変, 올림/내림 | notation-specific | - | fixed |
| Maqam names (A.3.7) | rast, dukah, ... | koma / quarter-tone | - | - | fixed names |

- **Decode**: `semitone = tonicPc + modeTable[degree] + accidentalDelta`, `midi = tonicMidi + semitone + 12*octaveShift`; reference octave: tonic in C4..B4 (default C4=60 with a diagnostic), shifted by +-12 to the instrument at fingering time. Exact intonation (just, koma, gamelan cents) goes in `pitchHz`; `midiNote` is the nearest semitone and `stringIndex/fret` are assigned by `TabFingering::assign`.
- **Detection (all of these)**: a block is a pitch-sequence line when >= 80% of whitespace tokens are in the system's token set, there are no `|`-railed string rows or `e|B|` labels, and (for sargam/gongche/kepatihan) a header cue or script evidence exists. Without a tonic, default to C **with a loud diagnostic** (same rule as jianpu).
- **Playback key sanity**: movable-do systems depend on the tonic; always echo "Sa/1/do = X" in the import summary.

#### A.3.1 Brazilian / Portuguese "cifra" (Cifra Club style)

- **Chord letters are English**; qualities are Portuguese-flavoured [W/E]: `7M` = maj7, `m7(b5)` or `m7(5-)` = half-diminished, `°`/`dim` = dim, `+`/`5+` = aug, `4` or `sus4` = sus4, `(add9)`, `7(9)`, `6/9`, `m(7M)` = minor-major 7, slash bass `C/G`, `C5` power chord. Normalise `º` (U+00BA, ordinal, commonly typed for the degree sign) and `°` (U+00B0) to dim; `ø` -> m7b5.
- **Headers**: `Tom: D` (key), `Afinação: E A D G B E`, `Capotraste na 2ª casa` (ordinal `ª/º` after the digit), `Intro 2x:`, section labels in brackets: `[Intro] [Primeira Parte] [Pré-Refrão] [Refrão] [Solo] [Ponte] [Final]`, repeat `2x`/`(2x)`/`(bis)` [W/E]. Chords sit above the syllable where they start [W].
- **Gotchas**: displayed `Tom` may already be a transposed version of the original; with `Capotraste na N` the written chords are shapes (same shape/sound rule as Chinese `选调`, 4.3.8). `bis` = repeat twice (also Italian). Tab bodies use the usual glyphs; Brazilian words `ligado` (hammer/pull), `palhetada` (picking), `dedilhado` (fingerpicking), `batida`/`levada` (strum pattern).

#### A.3.2 Indian sargam (Hindustani and Carnatic)

- **Hindustani (Bhatkhande)** [E]: Sa Re Ga Ma Pa Dha Ni (Devanagari सा रे ग म प ध नि). ASCII: `S r R g G m M P d D n N`, lowercase = komal (r g d n), `M` = tivra Ma, Sa and Pa fixed. Semitones from Sa: `0 1 2 3 4 5 6 7 8 9 10 11`. Octaves by dot under (mandra) / over (taar); ASCII approximations `S.`, `.S`, `S'`, `,S` conflict, so require the document's own convention or fall back to the middle octave with a warning. Rhythm in matras between `|`; `-`/`,` = hold; tala marks `X` sam, `0` khali, numbers = tali; grace *kan* and glide *meend* are ornaments (drop with a warning).
- **Carnatic** [E]: `S R1 R2 R3 G1 G2 G3 M1 M2 P D1 D2 D3 N1 N2 N3` (also Ri/Ra, Dha/Da spellings) -> `R1=1 R2=2 R3=3 G1=2 G2=3 G3=4 M1=5 M2=6 P=7 D1=8 D2=9 D3=10 N1=9 N2=10 N3=11`; `R2=G1`, `R3=G2`, `D2=N1`, `D3=N2` are the same pitch with different raga grammar: keep the written name in a diagnostic, play the pitch. Gamakas are not representable; drop.
- **Tonic**: Sa is movable. Look for `Sa = D`, `Scale: C#`, `Key:`; harmonium "kali/kattai" numbering is non-standard (do not guess a table). Default C with a diagnostic.
- **Collisions**: sargam letters overlap tab glyphs and chord letters (`S` slap, `P` pop, `g` ghost, `D`, `M`, `G`); route by line composition (A.3.0), never by glyph.
- **Tuning**: 12-TET default; shruti/just ratios could ride `pitchHz` later.

#### A.3.3 Chinese: gongche, guqin, and pop-guitar vocabulary

- **Gongche (工尺譜)** [W]: movable-do; kunqu series 合 四 一 上 尺 工 凡 六 五 乙 = jianpu 5̣ 6̣ 7̣ 1 2 3 4 5 6 7 (then 1̇ 2̇ 3̇), so 上=do, 尺=re, 工=mi, 凡=fa, 六=sol, 五=la, 乙=ti, 合=low sol, 四=low la, 一=low ti. Cantonese opera writes 反 for 凡; some sources swap 一/乙 [W]. Tonic by *diao*: a converter's default xiaogong (小工調) puts 上 on **D** (`上尺工凡六五乙` -> D E F# G A B C#); other keys: zhenggong G, yizi A, shangzi Bb, chizi C, fanzi E, liuzi F [W, one tool's convention; verify against a gongche text]. Higher octave marked with an 亻 radical prefix [E]. Rhythm marks 板 (ban, strong) / 眼 (yan, weak) [E]. Deviant degrees 凡/乙 can be inflected (ambiguity). Route: pitch-sequence core, tonic from the 調 name, else ask.
- **Guqin 減字譜 (jianzipu)** [E]: composite glyphs giving string (一..七), hui position (徽 1-13, fractions), hand technique; pitch via tuning and harmonic-node ratios; essentially no rhythm. **Out of scope.** pipa/ruan tabs use 品 numbers: skip.
- **Pop-guitar sheets (吉他谱)**: 六线谱 = six-line tab, 和弦谱 = chord chart, 弹唱谱 = strum-and-sing, 指弹谱 = fingerstyle; `原调 E 选调 C 变调夹 4` covered by 4.3.8. Technique words in A.3.13.

#### A.3.4 Japanese: beyond iroha (tab vocabulary and traditional tabs)

- **Chord forms** [E]: `△7`/`M7`/`maj7`, `m7(♭5)`/`ø`, `on` slash chords (`C on E`, オンコード, 分数コード = fraction chords), `add9`, `sus4`, `aug`, `dim`, full-width (`Ｃ`, `ｍ`, `＃`). Section words in 4.3.6/4.3.10.
- **Tab glyph collision**: Japanese tabs commonly use **`C` for choking/bend** (チョーキング; same letter as banjo choke, T4), `H` `P` `S` for hammer/pull/slide, `PM`/`ブ` for bridge mute, `○` natural harmonic; bass slap is **チョッパー** (chopper) and pop **プル** [E]. Resolve by explicit legend first (3.1 rule), family second.
- **Koto 数字譜** [E]: names the **string**, not a fret: 一..十, 斗 (11), 為 (12), 巾 (13); pitch from the tuning (平調子 etc.). Koto has 13 strings > `kMaxStrings = 12`; bass koto 17: needs the cap raised or a skip. **Shamisen 文化譜** [E]: 3 lines of fret numbers (like guitar tab), tunings 本調子 (1-4-1), 二上がり (1-5-1), 三下がり; reference pitch arbitrary (singer's key) so require an explicit tonic or default C with a diagnostic. **Shakuhachi** ロ ツ レ チ リ: flute-length-relative, skip.

#### A.3.5 Korean

- **Letter names (가나다)** [E]: Korean school system maps 가=A 나=B 다=C 라=D 마=E 바=F 사=G; keys `다장조` = C major, `가단조` = A minor, `올림` = sharp, `내림` = flat (`내림나장조` = Bb major). Same trap as Japanese `ロ`: **다 is C, not D**. Fixed-do 도레미파솔라시 also used (4.3.8).
- **Tab/chord vocabulary**: 카포, 튜닝, 코드, 전주 (intro), 간주 (interlude), 후렴 (chorus), 1절 (verse 1), 브릿지, 엔딩; technique words in A.3.13.
- **Gugak**: 정간보 (grid, one cell per time unit) with 律名 pitch names (황 대 태 협 고 중 유 임 이 남 무 응; pentatonic subset 황 태 중 임 남) [E]. **Skip.**

#### A.3.6 Indonesian kepatihan (gamelan numbered notation)

- [E] Digits 1-7 = **scale degrees of a specific gamelan tuning**, not pitches: slendro uses 1 2 3 5 6, pelog 1-7 (patet subsets); dot above/below = octave; beats grouped in *gatra* of 4, one line per gong cycle; `.` = rest/hold; gong ending marked with a circle/parentheses; part letters (`n` kenong, `p` kempul, `t` ketuk) under the line. Balinese vowel solfege *ding dong deng dung dang* (i o e u a) = 5 pelog degrees; Sundanese *da mi na ti la* [E].
- **Intonation**: ensembles are individually tuned; slendro is roughly 5 equal steps (~240 cents), pelog is 7 uneven steps. Offer a documented "approximate gamelan" profile (cents table) via `pitchHz`; default to the nearest 12-TET with a diagnostic, never claim accuracy.
- Same parser as jianpu/not angka (4.3.5) once a `kepatihan` dialect flag is set; accidentals do not exist; rests/holds differ from jianpu (`.` not `0`).

#### A.3.7 Arabic and Turkish maqam names and quarter-tones

- **Conflict**: Arabic *rast* is conventionally **C** (Rast C, Dukah D, Sikah E half-flat, Jaharkah F, Nawa G, Husayni A, Awj B half-flat) [E/W-partial]; Turkish AEU *rast* is **G** (Rast G, Dügâh A, Segâh B koma-flat, Çârgâh C, Neva D) [W]. Same word, a fifth apart: never map a bare name; require the tradition from the header (`Arabic`/`Turkish`/`Arel`) or refuse with a diagnostic.
- **Turkish AEU** [W]: 53 commas per octave (koma, 1200/53 = 22.64 cents); accidentals of 1, 4, 5, 8 and 9 commas (sharps/flats): 1 = 22.6c, 4 = 90.6c, 5 = 113.2c, 8 = 181.1c, 9 = 203.8c. Note names are individual per octave (Kaba Çârgâh low, Tiz Çârgâh high). Scores may be written for a transposed *ahenk* (written != sounding) [E].
- **Arabic accidentals**: half-flat (koron) U+1D133, half-sharp (sori) U+1D132; ASCII `d`/`+`/`↓`; MusicXML `quarter-flat/quarter-sharp` or fractional `<alter>` (-0.5); LilyPond `eh`/`ih` [E].
- **Decode**: carry fractional alteration to `pitchHz = 440 * 2^((m + alter - 69)/12)` (or cents from commas); `midiNote` = nearest semitone, tie toward the letter's natural pitch; `stringIndex/fret` from the nearest fretted pitch + a bend/cents offset (see `spec/microtonal-bends.md`). Fretted oud/saz have no frets (fretless): pitch fidelity is the whole point, so microtonal support is the prerequisite; until then warn and round.
- **Instruments** [E, verify]: Arabic oud low->high C2 F2 A2 D3 G3 C4 (courses); Turkish oud E2 A2 B2 E3 A3 D4; bağlama "düzen" named by three solfege names (bağlama düzeni = La-Re-Mi). Add as named tuning rows only after the tuning-table hardening (5.3).

#### A.3.8 Greek and Byzantine

- **Greek (modern)**: fixed-do Ντο Ρε Μι Φα Σολ Λα Σι (accents optional), Latin chord letters. **Homoglyph trap**: Greek capitals look Latin: Α Β Ε Ζ Η Ι Κ Μ Ν Ο Ρ Τ Υ Χ; Greek Η (eta) looks like Latin **H** (feeds the German-H rule, 4.3.1), Β like B. Fold on chord rows (same rule as Cyrillic, 4.3.7).
- **Byzantine chant** [W]: syllables Νη Πα Βου Γα Δι Κε Ζω (Ni Pa Vu Ga Di Ke Zo), diatonic scale by convention Ni=C Pa=D Vu=E Ga=F Di=G Ke=A Zo=B (a transposition choice; some sources differ), with Vu and Zo slightly low versus Western pitch. Neume notation is **relative** (intervals + mode + ison drone) and has no machine-readable pitch without mode context: **out of scope**; the only route is a MusicXML export from a Byzantine editor.
- **Bouzouki** [E, verify]: tetrachordo 4 courses C F A D (Ντο Φα Λα Ρε); trichordo 3 courses D A D (Ρε Λα Ρε); the lower courses are octave-paired. Rows for 5.6 after verification.

#### A.3.9 Hebrew and other right-to-left scripts (and non-ASCII digits)

- **Hebrew** [E]: fixed-do דו רה מי פה סול לה סי; flats/sharps במול / דיאז (`סי במול` = Bb); header words קאפו (capo), כיוון (tuning), בית (verse), פזמון (chorus), גשר (bridge), אינטרו, סולו; chord letters are usually Latin.
- **RTL hazards** (Hebrew, Arabic, Persian, Urdu charts): (1) copied text is in logical order but visually right-aligned: chord-over-lyric **column alignment inverts**, so position-to-syllable mapping is wrong; map by token order, not column, when the lyric run is RTL; (2) Unicode bidi controls (U+200E/U+200F, U+202A-202E, U+2066-2069) and BOM inside tab rows break column alignment: strip them in `TabTextSanitizer`; (3) never run bidi reordering on tab rows (they are LTR digit/dash grids); (4) mixed lines (RTL header + `Am` chords) must keep chord tokens whole.
- **Digit folding** (language-neutral, cheap): fold every Unicode `Nd` digit to ASCII in headers/capo/tuning/time-signature/tempo/chord rows: Arabic-Indic U+0660-0669, Persian U+06F0-06F9, Devanagari U+0966-096F, Bengali, full-width U+FF10-FF19 (4.4 #16 only mentions full-width). Do not fold inside fret bodies unless the whole row is non-ASCII digits (then warn).

#### A.3.10 Russian: additional points beyond 4.3.7

- **Homoglyphs in the tab body**: typing on a Cyrillic layout yields Cyrillic `х` (U+0445) for dead-note `x`, `р` (U+0440) for pull-off `p`, `с` for `c`, `е` for `e` etc. Fold `{х р с е о а у}` to Latin **inside tab rows and legends** (4.3.7 folds chord rows only); otherwise dead notes and pull-offs vanish.
- **H usage**: Russian-language chord sheets frequently use German-style `H7`/`Hm` for B natural alongside Latin names, so the H-dialect (4.3.1) applies to Russian/Ukrainian songbooks; bare `B` in modern web charts is probably B natural [E, weak], hence ambiguity #1 must stay evidence-based.
- **Words** [E]: `Бой` (strum pattern; "бой шестёрка/восьмёрка"), `Перебор` (picking pattern), `Барре`, `Аппликатура` (fingering), `Проигрыш` (interlude); technique words in A.3.13.

#### A.3.11 Welsh, Celtic and tonic sol-fa

- **Tonic sol-fa (Curwen; Welsh *sol-ffa*)** [E]: `d r m f s l t` (doh ray me fah soh lah te) movable-do, `:` separates beats, `|` bar, `.` half-beat, `,` quarter-beat, `-` continuation, blank = rest; octave by `d'` / `s,` or sub/superscript digits; header "Key C" / "Doh is C". Chromatic syllables follow vowel rules (sharps and flats change the vowel) and differ between Curwen and Kodaly: accept only diatonic tokens plus an explicit legend, warn otherwise. Mid-tune key changes use "bridge notes" (superscripts): warn and keep the first key. Welsh hymnals print mostly sol-fa, so this is the realistic Welsh input; word tables: *pennill* verse, *cytgan* chorus [E].
- **Celtic trad**: in practice **ABC** (A.2.1), `K:HP/Hp` for Highland pipes, DADGAD and GDAD/GDAE bouzouki/mandola tunings (5.6/5.8). Tin-whistle "tab" = hole patterns (not string/fret): skip.

#### A.3.12 Historic European tablature (lute, vihuela, baroque guitar)

- **Regional systems** [W]: *French* tab = letters for frets (a = open), **top line = highest course**; *Italian* tab = numbers, **bottom line = highest-sounding course** (top printed line is the lowest course, the reverse of modern tab); *German* tab = staffless symbols encoding course+fret; rhythm flags above the staff in all three; flags are **sticky** until changed [E]. Spanish vihuela tab follows the Italian/number convention [E]. *Alfabeto* (Italian/Spanish baroque guitar) = letters/symbols for chord shapes [E].
- **Impact on the model**: the "highest string first" assumption (and the `tab-row order` invariant, 5.1) fails for Italian-style tab: row 0 is the **lowest** course. Resolve tradition (French vs Italian) before reading, then map to tab-row order explicitly.
- **Tunings** [E]: renaissance lute in G (G2 C3 F3 A3 D4 G4), 6-course; baroque lute in D minor 11-13 courses (more than `kMaxStrings`), vihuela G; courses are doubled (flag, 5.6). **Verdict**: skip unless an early-music mode is planned; TabCode (A.2.8) is the ASCII source format.

#### A.3.13 Language-neutral gaps found along the way

- **Per-string assignment** `6=D 5=A ...`, `(6) = D`, ES `6ª en Re`, IT `6ª in Re`, FR `6e corde en Ré`, JA `⑥弦=D` / `6弦をD`, ZH `六弦降D`: check `parseTuningStatement` handles `N=note` pairs with ordinals (`ª º e ème th`), circled digits (U+2460-2465) and solfege names; common in classical and non-English sources.
- **Roman numerals as fret/position/capo** (European classical: `Capo III`, `Cejilla en V`, `CVII` = barre at VII, `Kapo II`): distinct from Roman-numeral harmony (4.3.4). Parse I-XXIV only in capo/position contexts.
- **Ordinal capo/fret forms**: `2. Bund` (DE), `2ª casa` (PT), `2ème case` (FR: *case* = fret), `traste 2` (ES), `2º tasto` (IT).
- **Chord "words" overlap lyrics** in every Romance language (4.3.2): `la`, `si`, `mi`, `do`, `re`, `sol`; also Portuguese `dó`, French `la`, `si`: chord-row gate only.

#### A.3.14 Foreign-language technique vocabulary (annotation rows, legends, prose directives)

In tab **bodies** the glyphs are universal (`h p b r / \ ~ x PM`; German sources use the same letters, `*`/`<n>` for natural and `[n]`/`A.H.` for artificial harmonics, `PM----`, `v/~` vibrato) [W]. Words matter in (1) legends (normalizer legend collector), (2) annotation rows above the staff, (3) prose such as "palm mute the riff" (`TabDirectiveApplier`). All entries [E] unless marked; verify with native samples.

| Technique | DE | FR | ES | IT | PT |
|-----------|----|----|----|----|----|
| palm mute | Palm Mute, Handballendämpfung, abgedämpft/gedämpft | étouffé (à la paume), palm mute | apagado (con la palma), muteo, palm mute | smorzato (col palmo), palm mute | abafado, abafamento (com a palma), palm mute |
| let ring | ausklingen lassen, klingen lassen | laissez vibrer (l.v.) [W], laisser sonner | dejar sonar, dejar vibrar | lasciar vibrare (l.v.) [W], lasciar suonare | deixar soar, deixar ressoar |
| natural harmonic | Flageolett, Naturflageolett [W] | harmonique (naturelle) | armonico (natural) | armonico (naturale) | harmônico (BR) / harmónico (PT) natural |
| artificial / pinch | künstliches Flageolett, A.H. [W], pinch harmonic | harmonique artificielle / pincée | armonico artificial / pinch | armonico artificiale / pinch | harmonico artificial / pinch |
| bend | Bending [W], Ziehen | tiré [W], bend | bend, estiramiento, estirar | bend/bending, tirata | bend, puxada, esticar |
| slide | Slide, Gleiten/Bundwechsel [W], Glissando | glissé, glissando, slide | slide, deslizamiento, glissando | glissato, glissando, slide, scivolata | slide, deslize, escorregada |
| hammer-on | Hammer-On [W] | liaison ascendante, hammer-on, martelé | ligado ascendente, martillo, hammer-on | legato ascendente [W], hammer-on | ligado de subida/ascendente, martelo |
| pull-off | Pull-Off [W] | liaison descendante, pull-off, **tiré** | ligado descendente, pull-off | legato discendente [W], pull-off | ligado de descida/descendente, pull-off |
| dead / muted note | tote Note, Dead Note | note morte / étouffée / mutée | nota muerta / apagada | nota morta / smorzata | nota morta / abafada |
| whammy bar | Tremolo-Arm, Vibratohebel, Whammy | levier (de vibrato/tremolo) | barra de trémolo [W], palanca | leva (del tremolo) | alavanca, barra de tremolo |
| tremolo picking | Tremolo | tremolo | trémolo | tremolo | trêmulo/tremolo |

| Technique | RU | JA | ZH (simp. / trad.) | KO |
|-----------|----|----|--------------------|----|
| palm mute | глушение ладонью, пальм-мьют | パームミュート, ブリッジミュート | 掌闷 / 掌悶 | 팜뮤트, 브릿지 뮤트 |
| let ring | дать звучать | レットリング, 余韻 | 延音 | 렛링 |
| harmonic | флажолет, гармоника | ハーモニクス (ナチュラル/ピッチ/アーティフィシャル) | 泛音, 人工泛音 | 하모닉스 |
| bend | бенд, подтяжка | チョーキング, ベンド | 推弦 | 벤딩, 초킹 |
| slide | слайд, глиссандо | スライド, グリッサンド | 滑音, 滑弦 | 슬라이드 |
| hammer-on / pull-off | хаммер / пулл-офф | ハンマリング(オン) / プリング(オフ) | 击弦 / 勾弦 (擊弦 / 勾弦) | 해머링(온) / 풀링(오프) |
| vibrato | вибрато | ビブラート | 揉弦 | 비브라토 |
| dead note | заглушенная нота, мьют | デッドノート, ミュート | 闷音 (悶音) | 데드노트, 뮤트 |
| tapping | тэппинг | タッピング, ライトハンド | 点弦, 敲弦 | 태핑 |
| whammy bar | рычаг (тремоло) | アーム, アーミング | 摇把 | 암, 트레몰로 암 |
| bass slap / pop | слэп | スラップ, チョッパー / プル | 拍弦 / - | 슬랩 |

- **Ambiguities** (add to 4.4): `tremolo` is the tremolo-**arm** in DE/FR/ES/IT/PT/JA as often as tremolo picking; `tiré` (FR) means bend **and** pull-off; `ligado/legato/liaison` (ES/IT/FR/PT) means hammer **or** pull **or** tie, decided by pitch direction and same-pitch = tie (`ligadura/legatura` = tie/slur); `slide` also means bottleneck; English words (`bend`, `slide`, `tapping`, `ring`) appear in lyrics: require a technique context (digits nearby, a glyph row, or an imperative on a bar/riff line).
- **Mechanics**: accent/case-fold (NFKD, strip combining marks), whole-word match, treat as additional synonyms in `TabDirectiveApplier` and the annotation-row classifier, never in the glyph tables; ship as data so translators can extend.

#### A.3.15 Added ambiguity register entries (continuing 4.4)

| # | Ambiguity | Example | Decision rule | Diagnostic |
|---|-----------|---------|---------------|------------|
| 18 | `tremolo` picking vs whammy arm | DE/FR/ES/IT/PT "tremolo" | whammy if "arm/hebel/barra/leva" or `w` glyph context, else picking | - |
| 19 | FR `tiré`; ES/IT/PT/FR `ligado/legato/liaison` | "tiré", "ligado" | glyph beside it (`b` vs `p`/`h`) then pitch direction; same pitch = tie | "pull/bend guessed" |
| 20 | `C` between digits: banjo choke vs Japanese choking vs chord | `7C9` | both mean bend: same mapping | - |
| 21 | ABC `~`, `.`, `H`, `T` | `~G3` | inside ABC reader only | - |
| 22 | Sargam letters vs tab glyphs and chords | `S R G M P D N` | line-composition test (A.3.0) | "sargam line, Sa=C assumed" |
| 23 | Arabic rast=C vs Turkish rast=G | `Rast` | require tradition word, else refuse | "tradition unknown" |
| 24 | Gongche tonic by *diao*; 凡/乙 inflection | `上尺工` | tonic from 調 name, else ask/default D | "diao assumed" |
| 25 | Kepatihan degree vs pitch; `.` rest vs hold | `. 3 . 5` | gamelan profile or nearest 12-TET | "approx. tuning" |
| 26 | Korean 다 = C (not D), 가 = A | `다장조` | Korean-letter table | - |
| 27 | Cyrillic/Greek homoglyphs in **tab body** | `5р7`, `х` | fold inside tab rows | count folded |
| 28 | RTL/bidi controls in tab or chord rows | U+200F | strip; map chords by token order | "bidi stripped" |
| 29 | Roman numerals as frets/capo vs harmony | `Capo III` | capo/position context only | - |
| 30 | Italian-style historic tab: row 0 = lowest course | lute number tab | tradition flag before read | "tab tradition" |

#### A.3.16 MIDI normalisation cheat-sheet

| Source token | Rule | MIDI (C4 = 60) |
|--------------|------|----------------|
| Letters C D E F G A B / H | `C0 D2 E4 F5 G7 A9 B11`; `H`=11; `B`=10 only in H-dialect | `12*(oct+1) + pc` |
| Fixed-do (Do..Si, ド.., до.., דו.., Ντο.., 도..) | `Do0 Re2 Mi4 Fa5 Sol7 La9 Si11` | as letters |
| Iroha / 가나다 | ハ=C ニ=D ホ=E ヘ=F ト=G イ=A ロ=B; 다=C 라=D 마=E 바=F 사=G 가=A 나=B | as letters |
| Movable-do (sol-fa, jianpu, sargam, gongche, kepatihan) | `tonic + table[degree] + accidental` | tonic octave default 4 |
| Sargam | `S0 r1 R2 g3 G4 m5 M6 P7 d8 D9 n10 N11` (+Sa) | Sa default C4 |
| Gongche (xiaogong) | `上0 尺2 工4 凡5 六7 五9 乙11` from D | tonic D |
| Quarter-tone / koma | fractional alter in semitones (-0.5 half-flat; Turkish koma = 1200/53 = 22.64 cents each) | `pitchHz`; `midiNote` nearest |
| Capo | never fold into tuning; sounding = tuning + capo + fret | section 0 rule |

### A.4 Proposed additions to the gap tables (IDs continue section 2)

| # | Capability | Now? | Verdict / effort | Anchor |
|---|-----------|------|------------------|--------|
| F15 | ChordPro / OnSong parsing (`define`, capo, key, `sot`) | No | Worth, Low | new `ChordProReader` -> `TabChordChart` |
| F16 | ABC notation | No | Worth, Low-Med | new `AbcReader` + `TabFingering` |
| F17 | `.tef` / `.musx` / `.sib` / `.capx` / `.dorico` detection with export advice | No | Low | `detectKind` message table |
| F18 | Rocksmith XML | No | Maybe, Med | new `RocksmithXmlReader` + shared quantiser |
| F19 | MuseScore `.mscz/.mscx` | No | Maybe, Med | new reader on zip+XML infra |
| F20 | UG saved-page JSON metadata | No | Maybe, Low (verify) | HTML path in `TabDocumentNormalizer` |
| F21 | iReal Pro | No | Skip v1 | - |
| F22 | **GP3-5 bend unit (/25 vs 100-per-tone)** | Defect [!] | Fix with C20, Low | `GuitarProLegacyReader.cpp:1082-1088` |
| T11 | Tremolo picking / pick-scrape types | No | Med (also needed by Rocksmith `tremolo`, GPIF) | `ScoreTechnique::Type` |
| L14 | Brazilian cifra qualities (`7M`, `m7(b5)`, `º`, `4`, `bis`) | Partial (letters) | Low | chord normalizer |
| L15 | Sargam (Hindustani/Carnatic) | No | Med, after core | `PitchSequenceReader` |
| L16 | Gongche | No | Low-Med, after core | same |
| L17 | Kepatihan / Balinese / Sundanese numerals | No | Low, after core | same |
| L18 | Arabic/Turkish names, koma/quarter-tone accidentals | No | High (needs microtones) | `pitchHz`, `spec/microtonal-bends.md` |
| L19 | Greek homoglyph + fixed-do names; Byzantine | No | Low / out of scope | dialect stage |
| L20 | RTL/bidi stripping and Unicode digit folding | No | Low | `TabTextSanitizer` |
| L21 | Cyrillic homoglyphs in tab bodies | No | Low | sanitizer |
| L22 | Tonic sol-fa (Welsh/Curwen) | No | Low-Med, after core | same |
| L23 | Historic tab traditions (French/Italian/German) | No | Skip unless early-music | - |
| L24 | Technique vocabulary tables (11 languages) | No | Low (data) | `TabDirectiveApplier`, legend collector |
| L25 | Per-string tuning assignment `N=note` with ordinals | Check | Low | `parseTuningStatement` |
| L26 | Korean letter names 가나다 | No | Low | dialect stage |
| L27 | Japanese tab vocabulary, `チョッパー`, koto/shamisen | No | Low / skip | glyph dialect |

Suggested roadmap slots (section 6 IDs): F22 joins **R3.4** (GP3-5 audit, with C20); L14/L20/L21/L24-L27 are data and sanitizer items inside **R4.2/R4.3** (chord normalizer, keyword rewrite) once **R4.1** exists; L15-L17/L22 extend **R4.5** `PitchSequenceReader` (replace the R6.8 "sargam" stub with the A.3.0 table); L18 stays behind microtones (**R6.8**); F15/F16 become new long-tail items **R6.9 (ChordPro)** and **R6.10 (ABC)**; F17 is a message table in **R3.2/R3.3** territory; F18-F20 are on-demand long-tail; T11 folds into decision **D2**.

### A.5 Open verification items (do before relying on any [E])

1. GP5 fixture with a known full-tone bend: confirm 100 per tone (A1-1) and position range 0..60.
2. A real Rocksmith arrangement XML: `string` order, `bendValue step` unit, `linkNext`, multi-level files.
3. A MuseScore 3 and 4 tab score: `string` index direction, `StringData` order, spanner names.
4. A saved UG page: JSON field names.
5. Native samples for each tradition (gongche diao table, Hindustani octave marks, kepatihan layout, Turkish AEU accidentals, Cifra Club exports).
6. Technique word tables with native speakers; mark each row in tests as [E] until then.

### A.6 Licensing notes for this addendum

alphaTab (MPL-2.0), TuxGuitar (LGPL), MuseScore (GPLv3), abc2midi/abcm2ps (GPL), LilyPond (GPL): documentation only, clean-room. abcjs (MIT), humlib (BSD), ChordPro (Artistic-2.0 reference implementation, public docs) may be read freely. Rocksmith content and community CDLC are copyrighted by Ubisoft and transcribers; never ship, fetch or bundle them. Do not scrape Songsterr or Ultimate-Guitar; accept user-supplied files only.

### A.7 Sources consulted (this pass)

- alphaTab `GpifParser.ts` (bend factors, slide flags, harmonic types, id pools): raw.githubusercontent.com/CoderLine/alphaTab (develop)
- DGuitar javadoc `GPBendPoint` ("bend height (100 per tone)", position in sixtieths): dguitar.sourceforge.net
- abcjs tablature docs: docs.abcjs.net/visual/tablature
- ChordPro 6 release notes / cheat sheet / Chordii format, OnSong ChordPro docs: chordpro.org, vromans.org, onsongapp.com
- RocksmithToTab and Rocksmith2014.NET READMEs: github.com/fholger/RocksmithToTab, github.com/iminashi/Rocksmith2014.NET
- Rhythm Gaming World pro guitar/bass track requirements; Finale `.musx` (Library of Congress, PRONOM, Steinberg forum); Soundslice help (importing, data API); Flat developer docs; TabCode/MEI (Transforming Musicology, ECOLM, luteconv)
- Gongche notation (Wikipedia; PyPI converter docs); Turkish makam notes (Wikipedia: list of notes, Rast, Cargah); Byzantine notation (OrthodoxWiki, Solmization); Portuguese chord-chart tutorial (cursa.app); German/Spanish/French/Italian tab legend pages (delamar.de, lezioni-chitarra.it, ultimate-guitar legends)
- Nothing could be fetched for TablEdit, Capella `.capx`, iReal Pro, MuseScore `.mscx` internals: stated as gaps, not guessed.
