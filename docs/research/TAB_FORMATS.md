# ASCII guitar/bass tab dialects — field catalogue (universal tab player)

Scope: every plain-text tab convention a user is likely to paste into the TAB reader, from the OLGA /
`alt.guitar.tab` era (1991–) through Harmony Central, Ultimate-Guitar (UG), Songsterr "Pro"
text exports, Guitar Pro ASCII export, Japanese sites (J-Total, ufret), to Markdown/HTML-damaged
copies. Written from first-hand knowledge of those sources (the sandbox has no web egress, so no
external page was fetched for this pass); every example fixture in `Source/Tests/Fixtures/TabCorpus`
and `Fixtures/AsciiDialectCorpus/Corpus.h` is original, short, and copyright-free.
"Before" = branch `claude/luthier-tab-player` at bcbdb07; "After" = this round. Handler column names
the file/function.

## 1. Staff lines and string labels

| Dialect | Example | Before | After | Handler |
| --- | --- | --- | --- | --- |
| Classic `e B G D A E` labels, `\|` after label | `e\|---0---\|` | yes | yes | `AsciiTabReader::classify` |
| Label with colon / space / none | `E:---`, `E ---`, `\|---`, `---` | yes | yes | `classify` |
| Sharp/flat labels (`D#`, `Eb`, `Bb`) | `Eb\|---` | yes | yes | `classify`, `tokeniseNames` |
| Octave labels (`E2`, `e4`) | `E2\|---` | yes | yes | `tokeniseNames` |
| German `H` for B (`e H G D A E`), `B` = Bb in German context | `H\|---` | no | yes | `pitchClassOfLetter`, `AsciiTabReader::parseTuningNames` |
| Numbered strings `1\|`…`6\|` | `1\|---` | yes | yes | `TabTextSanitizer` (`stringLabelsRewritten`) |
| Low-to-high systems (bass players, some Usenet) | `E\|` on top | yes | yes | `TabTextSanitizer` (`systemsReversed`) |
| Unicode box drawing / en dash / full-width `｜` | `e│──0──│` | yes | yes | `TabTextSanitizer` |
| Spaces instead of dashes | `e   0   3` | yes (fill ≥ ¼ spaces) | yes | `classify` |
| 4/5/6-string bass, 7/8-string, 12-line | `B\|` 7th line | yes | yes | string count = largest system |
| Ukulele (gCEA), mandolin (GDAE ×2), banjo (gDGBD) | `Tuning: ukulele` | uke named | uke/mandolin/banjo/baritone named | `matchTuning` |
| Two voices / `Gtr I` `Gtr II` stacked | 12 consecutive lines | yes (12-line system) | yes, warned | `read` (system of 12) |
| Separate parts labelled `Gtr I:` | `[Gtr II]` | normalizer splits parts | same | `TabDocumentNormalizer` (`multiPartBlocks`) |
| HTML `<pre>`, `&amp;`, Markdown fences, `[tab]…[/tab]` | | yes | yes | `TabDocumentNormalizer`, `TabTextSanitizer` |
| Wrapped lines (mailer 72/80-col wrap) | half a system continues below | yes (`wrappedRowsJoined`) | yes | `TabDocumentNormalizer` |
| Drum rows (`HH\|x-x-`) | | refused with message | same | `TabTextSanitizer` (`drumLinesSkipped`) |

## 2. Headers: tuning, capo, key, tempo, metre

| Dialect | Example | Before | After | Handler |
| --- | --- | --- | --- | --- |
| Named tunings | `Tuning: Drop D`, `DADGAD`, `Open G`, `Eb Standard`, `C# standard` | yes | yes | `matchTuning` table |
| `half step down`, `1/2 step down`, `tuned down 1/2` | | yes | yes | `matchTuning` |
| `whole/full step down`, `1 step down`, `1.5 steps down`, `2 steps down`, `down a half step` | | partial (half/whole only) | yes (n steps, fractions, "a half", "one and a half") | `stepsDownIn` (AsciiTabReader.cpp) |
| Named tuning + offset (`Drop D, half step down` = Drop Db) | | no (Drop D only) | yes, offset applied to the named list | `readHeader` |
| Note lists either order, with octaves | `E A D G B e`, `eBGDAE`, `E2 A2 D3 G3 B3 E4` | yes | yes | `parseTuningNames` (span heuristic) |
| Note list with German `H` | `E A D G H E` | no | yes | `parseTuningNames` |
| `Tuning: Standard (E A D G B e)` parenthesised | | yes (named part) | yes | `readHeader` |
| Non-English keywords (`Stimmung`, `Afinación/Afinação`, `Accordage`, `チューニング`) | | no | yes | `readHeader` keyword list |
| Baritone (`B E A D F# B`), bass 4/5/6, 7/8 | | 4/5 bass, 7/8 named | + baritone, 6-string bass | `matchTuning` |
| `Capo: 2`, `Capo on 3rd fret`, `no capo` | | yes | yes | `readHeader` |
| Roman numeral capo (`Capo III`, `Capo: IV`) | | no | yes | `romanNumeralIn` |
| `Key: Am`, `Key of G`, `Key - F#m`, `Tonart`, `Tonalidad` | | no | yes → `Meta::key` (header wins) | `readHeader`, `TabKeyDetector` |
| Key with no header | | no | Krumhansl–Schmuckler profile over notes; chord symbols vote | `TabKeyDetector::detect` |
| `Tempo: 120`, `120 bpm`, `BPM 96`, `♩ = 80`, `q=120` | | yes | yes | `readHeader` |
| `Tempo: 1/4 = 120`, `♩=120 (4/4)` | | 1/4 misread as metre | fixed: fraction before `=` is a note value | `readHeader` |
| `Time: 3/4`, `6/8`, bare `4/4` line | | yes | yes | `readTimeSignature` |

## 3. Bars, repeats, endings, timing lines

| Dialect | Example | Before | After | Handler |
| --- | --- | --- | --- | --- |
| Bar lines `\|`, double `\|\|`, repeats `\|:` `:\|`, `x4`, `(x3)`, `play 4 times` | | yes | yes | `parseSystem`, `repeatCountIn` |
| 1st/2nd endings `\|1.` / `\|2.` | | no (bars kept once) | no — out of reach; played once, warned | — |
| Dash spacing = time, 2-digit widening | | yes (grid normalised per bar) | yes | `parseSystem` (`widened`/`slots`) |
| Rhythm letters `w h q e s` + `.` dotted + `3` triplet | `q e e q.` | yes | yes | `parseRhythmAnchors` |
| Count lines `1 e & a 2 e & a`, `1 & 2 &`, `1 2 3 4` | | yes | yes | `parseRhythmAnchors` |
| GP ASCII export beat ruler `1---2---3---4---` | | ignored (ruler) | same (grid already 4/beat) | `classify` |
| Songsterr/UG-Pro ASCII (`e\|-...-\|` with `PM` rows and `h/p` inline) | | yes | yes | `AsciiTabReader` |
| Japanese convention (`｜`, `チューニング: E A D G B E`) | | partial (`｜` only) | + `チューニング` keyword; circled/full-width digits still unread | `TabTextSanitizer`, `looksLikeTuningStatement` |

## 4. Chords, strums, fingerpicking

| Dialect | Example | Before | After | Handler |
| --- | --- | --- | --- | --- |
| Chord names over the staff (incl. slash, add/sus/dim/aug, 7/9/11/13, `maj7`, `m7b5`, `N.C.`) | `Am     G` above `e\|` | skipped, counted | → `ScoreMeasure::chordSymbols` at the beat under the name | `readChordLine` (AsciiTabReader) |
| Chord charts (UG `[ch]`, ChordPro `[Am]`, plain lines) | | yes (one strummed bar each) | yes | `TabChordChart` |
| Nashville numbers (`1 4 5 1`, `6m`) with a key | | no | yes when a key is known | `TabChordChart::extractChords (keyRoot)` |
| Inline chord diagrams `Am: x02210`, `Am x-0-2-2-1-0`, `{define: Am … frets x 0 2 2 1 0}`, `(x32010)` after name | | no | diagram wins over the name | `TabChordChart::extractDiagrams` |
| Voicings in a non-standard tuning / capo | | standard shapes only | searched voicing on the actual tuning | `TabChordChart::voicingFor` |
| Strumming pattern line `D DU UDU`, `d u d u`, `↓ ↑`, `v ^`, `-` rest, `x` mute, `>` accent | | no | → `pickStrokeDown/Up`, dead-note mutes, accents on the notes under them; chart bars follow the pattern in eighths | `readStrumLine`, `TabChordChart::read` |
| Fingerpicking pattern `p i m a`, `T 1 2 3` (chord charts) | | no | chart bars arpeggiate in that order (thumb = bass string, i m a = G B e) | `TabChordChart::extractPickingPattern` |
| `let ring`, `L.R.`, arpeggio bracket | | yes (span) | yes | `parseSystem` annotations |
| Accent row `>` above the staff | | no | → `accent` on the notes under each mark (dynamics `p mf f` still ignored) | `isAccentLine` / `readStrumLine` |
| Rhythm-only "chuck"/`x` columns (all strings `x`) | | dead notes | dead notes on a strum = muted strum | `RiffCompiler` (MutedPick) |

## 5. Technique glyphs (inline on the string line)

`h p ^ / \ s S b r pb ~ v t T x X o ( ) * < > [ ] tr w PM LR = _ > . nh ah ph th`, bend amounts
(`b1/2 bfull b2 7b9r7 7pb9`), slide-ins/outs, ghost and grace notes — all read before this round
(`parseSystem`, tab-import-export 7.3) and unchanged; routed through `TabTechniqueCompiler` /
`RiffCompiler` to the engine's HammerOn/PullOff/Slide/Bend-curve/Vibrato/Harmonic/PalmMute/Tap/Slap/Pop
techniques. Unknown glyphs are skipped and counted (`ignoredGlyphs`).

## 6. OCR / formatting damage (all before; unchanged)

HTML entities and `<br>`, Markdown fences and `**`, smart dashes, NBSP, BOM, mixed EOLs, tabs, wrapped
systems, collapsed rows, missing labels on continuation systems, 16 k-char lines, random bytes — see
`TabRobustnessTests`, `TabDocumentRecoveryTests`, `AsciiDialectCorpusTests`.

## 7. Not supported (deliberately)

1st/2nd endings (played once); D.S./Coda navigation; per-note durations in a tab with no timing line
(spacing only); polyphonic voices on one staff; GP6 `.gpx` and PowerTab (friendly error).
