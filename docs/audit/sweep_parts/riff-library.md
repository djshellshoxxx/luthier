## riff-library.md

The riff library does not exist on this checkout: no `Source/Riffs/`, no `Resources/Riffs/`, no `Tools/riffs/*.riffdef` or `generate_factory_riffs.py`, no RIFFS tab, Easy drawer, `R` shortcut or RL tests. The destinations it reuses are present (`MidiPerformance::fromScore`, `MidiProfiles::exportToMemory`, the MIDI OUT drag-out pattern, `MidiImportTargets::importPerformance`, `TabReaderTab`, `TuneSession`, `TunePlayer` hand-over pattern), but there is no `edition::Feature` to append to. No FEAT branch pushed yet; all rows OWNED.

| Req | Summary | Engine location | GUI location | Test | Status |
|---|---|---|---|---|---|
| RL-0 (§0) | Ground rules: original content only, explicit techniques, JSON single source with C++ encoder, reviewable `.riffdef`, MIDI-level audition, no new params — not built | - | - | - | OWNED |
| RL-2.1 (§2.1) | 300 factory items per genre/type table, coverage rules, 60-item Free set — absent | - | n/a | - | OWNED |
| RL-2.2 (§2.2) | `.riffdef` authoring grammar (durations, notes, chords, technique suffixes, strum/bass prefixes, bar sums) — absent | - | n/a | - | OWNED |
| RL-2.3 (§2.3) | `Tools/generate_factory_riffs.py` deterministic generator, legality checks, catalog/free manifest, CI diff; `luthier-render --export-riffs` flag in `Tools/RenderCli.cpp` — absent | - | n/a | - | OWNED |
| RL-3 (§3) | `.luthierriff` schema (meta, riff, notes, strums, bass_tech, source), tech tokens = MIDI profile tokens, limits, user folder, `library.json`, stable ids — absent | - | n/a | - | OWNED |
| RL-4 (§4) | `Riff`/`RiffLibrary` (worker scan, LRU 32, bitset filters, AND text search), `RiffAnalysis` (key, techniques, difficulty formula) — absent | - | n/a | - | OWNED |
| RL-5.1 (§5.1) | `RiffCompiler::compile` pure; beats->`RiffEvent`s, bend/vibrato curves, strum stagger, technique mapping; `NoteOnEvent::palmMuteDepth` + `explicitArticulation` — absent | - | n/a | - | OWNED |
| RL-5.2 (§5.2) | `RiffTransposer`: shift in -6..+6, optional scale map, placement candidates/chain move/drop, tuning/family re-fret, whammy-as-bends notices — absent | - | n/a | - | OWNED |
| RL-5.3 (§5.3) | `RiffPlayer` in `LuthierEngine::processSubBlock` after direct events; TunePlayer-style swap; Auto/Own clock, bar-quantised start, tempo factor, bend pacing, 96-slot cap, stop hygiene, not to live MIDI out, audition level trim — absent | - | n/a | - | OWNED |
| RL-5.4 (§5.4) | Engine interactions (bypass interpreter/voicer, auto-articulation skips explicit, cascade, playing mode) — absent | - | n/a | - | OWNED |
| RL-6 (§6) | Destinations: drag `.mid` (CC67/72/73 added in `addScoreNote`, Drag folder pruning, profile toggle, Ctrl+E), Add to Tune (Melody/Bass track, `extra` JSON, locked notes, one edit), Send to Looper, Learn It (`TabReaderTab::openScore`), piano roll — absent | - | - | - | OWNED |
| RL-7.1 (§7.1) | Col 4 RIFFS tab between TUNE and LIVE; Easy Riff drawer + Riffs button; `R` shortcut; Options FILE LOCATIONS "Riffs folder"; gui-integration 19 rows — absent | - | - | - | OWNED |
| RL-7.2 (§7.2) | RIFFS tab layout (search, chips, filters, virtualised list, `RiffTabView` preview + playhead, transport/key/tempo/level, destination buttons, states, Audition-on-select option) — absent | - | - | - | OWNED |
| RL-7.3 (§7.3) | Easy drawer single-column controls (no Add to Tune) — absent | - | - | - | OWNED |
| RL-7.4 (§7.4) | Save as riff from capture region / last N bars, fields, user riff ops, Import .mid as riff — absent | - | - | - | OWNED |
| RL-7.5 (§7.5) | Empty states and error banners — absent | - | - | - | OWNED |
| RL-8 (§8) | Nothing in presets; per-instance uiState fields; playback not restored; excluded from offline renders; pure compile — absent | - | n/a | - | OWNED |
| RL-9 (§9) | Undo: Add to Tune one `tune-melody-edit` entry; looper layer undo; browsing not undoable — absent | - | n/a | - | OWNED |
| RL-10 (§10) | Accessibility (row names, type-ahead, chips, tab-view text, announcements, keyboard destinations, drawer focus, catalog strings `riff.<id>.name`) — absent | - | - | - | OWNED |
| RL-11 (§11) | Edition split table; append `riffLibraryFull/riffExport/userRiffs` to `edition::Feature`, `freeRiffs=60` limit; Free installer manifest — absent (no `edition::` namespace exists here) | - | - | - | OWNED |
| RL-12 (§12) | Legal: origin/author, name pattern, trademark scan of catalog, `deny.txt`, 80% 6-gram similarity guard — absent | - | n/a | - | OWNED |
| RL-13 (§13) | Budget: player 0.02 units, compile 2 ms, index 150 ms, filter 8 ms, memory 3 MB, lazy boot — absent | - | n/a | - | OWNED |
| RL-14 (§14) | Interactions (Tune clock/"Key: Tune", jam-mode query API, snapshots continue, preset load ends notes, fretted slides in Slide Mode, metronome, Workshop swap recompile) — absent | - | n/a | - | OWNED |
| RL-15 (§15) | Failure modes table — absent | - | n/a | - | OWNED |
| RL-T1 (§16 RL-01..03) | Generator determinism, coverage, schema/fuzz — none | - | - | - | OWNED |
| RL-T2 (§16 RL-04..10) | Compile purity, technique mapping, Luthier round trip, drag file, transpose, scale map, re-fret — none | - | - | - | OWNED |
| RL-T3 (§16 RL-11..18) | Player RT safety, host/own clock, stop hygiene, queue pressure, explicit flag, rhythm coexistence, capture — none | - | - | - | OWNED |
| RL-T4 (§16 RL-19..24) | Add to Tune, Looper, Learn It, Save as riff, search/filter, legal — none | - | - | - | OWNED |
| RL-T5 (§16 RL-25..31) | GUI reach, keyboard/SR, empty/error states, state, edition gate, performance, combination — none | - | - | - | OWNED |

<!-- counts DONE=0 NO-GUI=0 NO-TEST=0 PARTIAL=0 MISSING=0 OWNED=29 -->
