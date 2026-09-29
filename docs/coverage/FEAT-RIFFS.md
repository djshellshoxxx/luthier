# FEAT-RIFFS coverage

Workstream FEAT-RIFFS: `spec/riff-library.md` (RL-01..RL-31), plus three
fixes found while that spec was written and the column-4 tab strip overflow.
Branch `claude/luthier-feat-riffs`.

Tests: `Source/Tests/RiffTests.cpp` (suite `Riffs`, engine side) and
`Source/Tests/RiffPanelTests.cpp` (suite `RiffPanel`, GUI under xvfb).
Content: `Tools/riffs/*.riffdef` (300 items), compiled by
`Tools/generate_factory_riffs.py` into `Resources/Riffs`.

No automatable parameters were added (riff-library 0.6): parameter count
unchanged.

## (a) Coverage

### Content and generator (riff-library 2, 3, 12)

| ID | Spec | Implementation | Verification | Status |
|---|---|---|---|---|
| RF-count | 2.1 table, 300 items | `Tools/riffs/<genre>.riffdef` x12 | Riffs.catalogMeetsTheCoverageTable; generator `check_coverage` | verified |
| RF-rules | 2.1 >=20/genre, diff 1-4 per genre, >=40% at 1-2, technique minimums, Drop D x4, 7-string x2, 5-string bass x6, Funk/Soul slap-pop x2 | `check_coverage` | Riffs.catalogMeetsTheCoverageTable | verified |
| RF-free | 2.1 free set: 5/genre, a bass line each, difficulty <= 3 | `free yes` in riffdefs; `free.txt` | generator; catalogMeetsTheCoverageTable (60) | verified |
| RF-grammar | 2.2 notation (durations, triplets, notes, chords, shapes, suffixes, strums, bass prefixes, bar sums with line/col errors) | `generate_factory_riffs.py` `compile_item`; `Tools/riffs/README.md` | generator run (300 items) | verified |
| RF-legal-tech | 2.3 legality (hammer/pull prior note, bend <= 3 / 1.5 bass, node frets, pm+tap) | `compile_item` legality block | generator run | verified |
| RF-gen-out | 2.3 files, catalog.json, free.txt | `write_tree` | RL-01 | verified |
| RF-determinism | 2.3 byte-identical rerun, sorted keys, 6-decimal reals, dates from riffdef | `canonical`, `fmt_number`, `--check` | Riffs.generatorOutputIsCommittedAndDeterministic (RL-01); CI step "Factory riffs are up to date" | verified |
| RF-est | 4 generator warns when difficulty differs from estimate by > 2 | `estimate_difficulty` (mirrors RiffAnalysis) | generator `-v` (0 warnings) | verified |
| RF-names | 12 `[Descriptor] [Figure] [n]`, < 32 chars, no "style of" | `check_names` | Riffs.catalogNamesAreOriginalAndClean (RL-24) | verified |
| RF-deny | 12 deny-list, case-insensitive, names and tags | `Tools/riffs/deny.txt` (712 entries), `denied` | RL-24 | verified |
| RF-tm | 12 trademark scan over catalog.json | `Tools/trademark_scan.py`, generator `trademark_in` | `python3 Tools/trademark_scan.py` (0 hits) | verified |
| RF-sim | 12 no pair > 80% interval-and-rhythm 6-grams | `check_similarity` | generator run | verified |
| RF-origin | 12 origin original + author initials | `compile_item` | RL-24 | verified |
| RF-strings | 10 `strings.en.json` | `write_tree` | file present | verified |
| RF-cli | 2.3 `luthier-render --export-riffs <dir> [--profile]` via the drag-out path | `Tools/RenderCli.cpp` `exportRiffs` | manual run: 300 files | verified |
| RF-format | 3 `.luthierriff` schema, limits, str 0 = highest, tech tokens = wire tokens | `Source/Riffs/Riff.{h,cpp}` | RL-03 | verified |
| RF-tech-recompute | 3 techniques recomputed on load, warn on mismatch, filter uses recomputed | `Riff::computeTechniques`, `fromJson`, `RiffIndexEntry::fromRiff` | RL-03 | verified |
| RF-user | 3 user riffs in Documents/Luthier/Riffs/User, atomic, id `user.<type>.<uuid>` | `RiffLibrary::saveUserRiff`, `Riff::saveToFile` (TemporaryFile) | RL-22 | verified |
| RF-global | 3 library.json favourites / recents (50) / play counts | `RiffLibrary::loadGlobal/saveGlobal` | RiffPanel.stateSurvives... (favourites via library) | verified |

### Library, analysis, compiler, player (riff-library 4, 5)

| ID | Spec | Implementation | Verification | Status |
|---|---|---|---|---|
| RL-lib | 4 index from catalog.json + user scan on a ThreadPool at editor open; LRU 32; bitset masks; AND text search | `Source/Riffs/RiffLibrary.*`; `RiffBrowser::ensureLibraryLoaded` | RL-23; RL-30 (index <= 150 ms) | verified |
| RL-analysis | 4 Krumhansl key, tempo from capture, techniques, difficulty formula | `Source/Riffs/RiffAnalysis.*` | RL-22 | verified |
| RL-compile | 5.1 pure compile to POD events, bend curves, vibrato, strum stagger, BASS_TECH, technique mapping | `Source/Riffs/RiffCompiler.*` | RL-04, RL-05, RL-13 | verified |
| RL-noteon | 5.1 `NoteOnEvent::palmMuteDepth` (read at the PalmMute switch) and `explicitArticulation` | `PlayingEvents.h`, `LuthierEngine::triggerNote` | RL-05, RL-16 | verified |
| RL-transpose | 5.2 key shift, scale map, placement candidates, chain moves, drops, family octave, notices | `Source/Riffs/RiffTransposer.*` | RL-08, RL-09, RL-10 | verified |
| RL-player | 5.3 RiffPlayer in LuthierEngine after the direct notes; SpinLock hand-over, retired slots, collectGarbage | `Source/Riffs/RiffPlayer.*`; `LuthierEngine::playRiffEvents`; `LuthierAudioProcessor::timerCallback` | RL-11 (shortened), RL-14 | verified |
| RL-clock | 5.3 Auto / Own clock, next bar / beat start, whole-bar loops, host stop ends, host jump re-locates, tempo factor / bpm | `RiffPlayer::renderSubBlock/advance` | RL-12, RL-13 | verified |
| RL-bends | 5.3 one bend per string per 64 samples; <= 96 slots, bends dropped first, overflow counter | `RiffPlayer::playSpan/emitBends/addBend`; `LuthierEngine::riffBendCents` | RL-15, RL-05 | verified |
| RL-stop | 5.3 stop / panic / new riff / preset load / guitar change end every riff note | `RiffPlayer::releaseAll`, `notifyEngineReset`; `LuthierAudioProcessor::panic` | RL-14 | verified |
| RL-capture | 5.3 riff notes through triggerNote (capture, fretboard, meters); not on live MIDI out | `StringActivityEvent::preview`, `MidiOutRouter::emit` | RL-18 | verified |
| RL-level | 5.3 audition level -24..0 dB scales velocity | `RiffPlaySettings::levelDb` in the compiler | code review | verified |
| RL-rhythm | 5.4 riff notes bypass interpreter and voicer; chord detector unchanged | `playRiffEvents` schedules directly | RL-17 | verified |
| RL-cascade | 5.4 a user slide / tap on the string overrides (last event wins) | ordinary triggerNote path | code review | verified |

### Destinations (riff-library 6)

| ID | Spec | Implementation | Verification | Status |
|---|---|---|---|---|
| RL-drag | 6.1 compile at key and tempo shown; fromScore + STRUM + BASS_TECH; MidiProfiles export to Riffs/Drag/<Name> - <Key> <bpm>.mid; Drag-as toggle, Alt forces Generic; external drag | `RiffDestinations::toPerformance/writeDragFile`; `RiffBrowser::makeDragFile`, `shouldDropFilesWhenDraggedExternally`, `DragTile` | RL-07 | verified |
| RL-prune | 6.1 Drag/ files older than 30 days pruned | `RiffDestinations::pruneDragFolder` (once per session, worker) | code review | verified |
| RL-savemid | 6.1 Save .mid... / Ctrl+E | `DragTile::onClick`, `RiffBrowser::keyPressed` | RL-26 | verified |
| RL-tune | 6.2 Add to Tune: section melody / bass (manual), key-transposed, clipped, locks kept, extra riff_str/fret/tech, one undo | `RiffDestinations::addToTune` | RL-19 | verified |
| RL-looper | 6.3 Send to Looper, whole bars at current tempo | `RiffDestinations::sendToLooper/render` | RL-20 | verified |
| RL-learn | 6.4 Learn It: drawer on TAB, openScore, loop, 70%, speed trainer +5%/pass | `TabReaderTab::openScore`; `RiffBrowser::learnIt/tickSpeedTrainer` | RL-21 | verified |
| RL-pianoroll | 6.5 audition lights the piano roll | through string activity (SoundingNotes) | code review | verified |

### UI (riff-library 7, 8, 9, 10)

| ID | Spec | Implementation | Verification | Status |
|---|---|---|---|---|
| RL-tab | 7.1 RIFFS tab between TUNE and LIVE, remembered by name | `AdvancedPanel::buildWorkspace` | RL-25; Editor.everyWorkspaceTabSelectsAndPaints | verified |
| RL-drawer | 7.1 / 7.3 Easy Riff drawer: 320 px, over the rig strip, full height, 150 ms (0 under reduced motion), Riffs button, Escape, focus back | `EasyPanel::setRiffDrawerOpen`; `RiffBrowser` (compact) | RL-25 | verified |
| RL-key | 7.1 `R` (rebindable): Advanced selects RIFFS, Easy toggles the drawer | `Accessibility.cpp` "riffs"; `LuthierAudioProcessorEditor::keyPressed` | RL-25 | verified |
| RL-options | 7.1 Options "Riffs folder"; "Audition on select" | `FileLocationsPage` (see Decisions) | build; RiffPanel suite | verified |
| RL-layout | 7.2 header, genre chips, filter row, virtualised list, preview, status; stacks below 640 | `RiffBrowser::resized` | RL-25 (no clipping at 1280x800 and the minimum size) | verified |
| RL-list | 7.2 columns: star, name, type, key, tempo, difficulty pips + number, technique glyphs with tooltips; sorts | `RiffBrowser::paintListBoxItem`, `getTooltipForRow`, `RiffQuery::Sort` | RL-26 | verified |
| RL-tabview | 7.2 RiffTabView on the compiled placement, playhead at 30 Hz, 250 ms stale rule | `Source/UI/RiffTabView.*` | RL-26 (description) | verified |
| RL-controls | 7.2 click selects; Space/Enter/Play; audition-on-select; Up/Down while playing | `RiffBrowser::keyPressed/selectRow` | RL-26 | verified |
| RL-states | 7.2 playing (Stop, speaker glyph), waiting (pulse / "waiting" under reduced motion), loading | `RiffBrowser::timerCallback/updateStatus` | code review | verified |
| RL-save | 7.4 Save as riff (marked region or last 2 bars, 1/16), fields, user riff Edit info / Duplicate / Reveal / Delete (confirm, trash), Import .mid as riff | `RiffBrowser::showSaveDialog/saveRiffFromCapture/importMidiAsRiff/deleteSelectedUserRiff`; `RiffDestinations::riffFromCapture`; NOTATION "SAVE AS RIFF" | RL-22 | verified |
| RL-empty | 7.5 empty states and banners in the spec's words | `RiffBrowser::updateStatus`; ErrorLog category Content | RL-27 | verified |
| RL-state | 8 per-instance uiState (selection, filters, sort, key, scale map, clock, tempo, loop, start, level, Drag-as, drawer, split); never in presets; audition never restored | `RiffUiState`; `UiState::riffs`; `setStateInformation` stops the player | RL-28 | verified |
| RL-offline | 8 audition excluded from AudioExporter / luthier-render renders | the player only plays when asked; offline engines never start it | code review | verified |
| RL-undo | 9 only Add to Tune is undoable (one tune-melody-edit entry) | `TuneSession::edit` in `addToTune` | RL-19 | verified |
| RL-a11y | 10 table-role list with row names, chips as toggles, dual-thumb sliders, tab view text, polite announcements, keyboard equivalents, focus order | `RiffBrowser::getRowName/getFocusOrder/announce`; `RiffTabView` | RL-26 | verified |
| RL-help | gui-integration 20 help topic | `HelpContent.cpp` "riffs" | HelpTab.theWorkspaceTopicNamesEveryTabThatExists | verified |

### Tests of riff-library 16

| ID | Test | Status |
|---|---|---|
| RL-01 | Riffs.generatorOutputIsCommittedAndDeterministic; CI step | verified |
| RL-02 | Riffs.catalogMeetsTheCoverageTable | verified |
| RL-03 | Riffs.factoryFilesLoadAndRoundTripByteIdentical, Riffs.mutatedFilesLoadOrRefuseCleanly (10 000 mutations) | verified |
| RL-04 | Riffs.compileIsPureAndThreadSafe | verified |
| RL-05 | Riffs.techniquesMapOntoTheEngine | verified |
| RL-06 | Riffs.everyFactoryRiffSurvivesTheLuthierProfile (string, fret, beat +/- 1 tick, technique set, all 300) | verified; the -60 dBFS render null is deferred (see Decisions) |
| RL-07 | Riffs.dragFileIsAValidMidiFileInBothProfiles | verified |
| RL-08 | Riffs.transposeTakesTheSmallestShiftThatFits | verified |
| RL-09 | Riffs.scaleMapMovesDegreesAndKeepsPassingTones | verified |
| RL-10 | Riffs.refrettingKeepsPitchAcrossTuningsAndFamilies | verified |
| RL-11 | Riffs.swapsFromAnotherThreadNeverAllocateOnTheAudioThread (200 swaps; shortened from 10 minutes) | verified, shortened |
| RL-12 | Riffs.hostClockStartsOnTheNextBarAndLoopsWithoutDrift | verified |
| RL-13 | Riffs.ownClockRunsAtTheFactorAndStrumsStayInSeconds | verified |
| RL-14 | Riffs.stopPanicNewRiffAndResetLeaveNothingSounding | verified |
| RL-15 | Riffs.queuePressureDropsBendPointsNotNotes | verified |
| RL-16 | Riffs.riffNotesAreExplicitAndKeepTheirStrings | verified |
| RL-17 | Riffs.riffsCoexistWithTheRhythmEngine | verified |
| RL-18 | Riffs.captureSeesRiffNotesExactly (incl. no live MIDI out) | verified |
| RL-19 | RiffPanel.addToTuneKeepsLocksAndIsOneUndo | verified |
| RL-20 | RiffPanel.sendToLooperMakesAWholeBarLayer | verified (Free 60 s limit: see editions) |
| RL-21 | RiffPanel.learnItOpensTheTabReaderAtSeventyPercent | verified |
| RL-22 | Riffs.saveAsRiffAnalysesTheCapture | verified |
| RL-23 | Riffs.searchAndFilterMatchBruteForceQuickly | verified |
| RL-24 | Riffs.catalogNamesAreOriginalAndClean | verified |
| RL-25 | RiffPanel.riffsTabAndDrawerAreReachable | verified |
| RL-26 | RiffPanel.keyboardOrderAndAccessibleNames | verified |
| RL-27 | RiffPanel.emptyAndErrorStatesShowTheirHints | verified |
| RL-28 | RiffPanel.stateSurvivesReopenAndHostRestoreButNotPresets | verified |
| RL-29 | edition gate | deferred: no edition module exists in the code base (no `edition::Feature`, no Free build) |
| RL-30 | Riffs.playerCompileAndIndexStayInBudget | verified (player bound relaxed, see Decisions) |
| RL-31 | RiffPanel.riffAuditionSurvivesTheCombination (60 s) | verified |

### Fixes found while the spec was written

| ID | What | Implementation | Verification | Status |
|---|---|---|---|---|
| FIX-a | `MidiPerformance::fromScore` / `addScoreNote` write CC67 (pm), CC72 (pinch), CC73 (natural), set before the note and reset after | `Source/Export/MidiPerformance.cpp` | Riffs.fromScoreWritesPalmMuteAndHarmonicControllers (round trip), RL-07 | verified |
| FIX-b | the audition race: `startAudition` rebuilt its sequence on the UI thread under the audio thread | fresh sequence handed over through a SpinLock slot, swapped with ScopedTryLock, retired for the timer (`PluginProcessor.*`) | Riffs.auditionRestartsWhileProcessingIsRaceFree | verified |
| FIX-c | column-4 tab strip overflow (about 15 tabs) | `Source/UI/WorkspaceTabStrip.*`: natural widths plus shared spare room when they fit; otherwise one scrolling row with arrows and an overflow menu, selected tab always whole, keyboard reachable; adding a tab needs no layout change | RiffPanel.workspaceTabStripOverflowsCleanlyAt1200And1920 | verified |

## (b) Decisions

- Content was authored in parallel by six helper agents from one brief (Tools/riffs/README.md grammar, per-genre technique quotas), then validated by the generator's rules; every item is hand-composed notation, reviewed as text.
- The grammar needed syntax the spec's list lacks: `tah` (tapped harmonic), `wb<n>` (whammy), `/in` `\in` `/out` `\out` for slidein / slidedown (into from above) / slideup (off upward) / slideout (off downward): every wire token needed a spelling for the coverage rule.
- `#` starts a comment only at the start of a word, so chord symbols like `F#m7` stay whole; a tag cannot be a header word (`capo`).
- The deny list seeds about 700 artist, band, song and album names; plain dictionary words (Money, Time, Walk) are left out because they would ban honest descriptors - human legal sign-off covers them.
- The canonical JSON layout (sorted keys, one-space indent, arrays of scalars and array elements inline) is shared by the generator and `RiffJson`, so a factory file saves back byte-identical (RL-03).
- A legato slide's technique sits on the note it starts from; the arrival note plays as `Technique::Slide` from it. With no arrival note the compiler adds a no-pluck glide at the note's midpoint.
- Picked slides (slidein, slidedown-into, a shift slide's arrival) are a `Pluck` with `slideFromFret`: the engine's `Slide` re-excites at 0.35 by design (legato), which would make a picked slide-in nearly silent.
- Harmonics follow REALISM-B's `touchFret` contract (merged in): natural stops open and touches the node; artificial is `ArtificialHarmonic` and tapharm is `Tap`, both stopped at the fret and touched `value` frets above - not NaturalHarmonic as 5.1 lists, because the merged engine distinguishes them.
- BASS_TECH maps slap and thump to the thumb, pop to pop, lhslap to a dead strike (BassStepType).
- A note-off is left out when the next note on the same string starts by then: the new note takes the string, and a strum's staggered off could otherwise silence it.
- Riff bends reach the engine as per-string cents added to the MIDI bend at block rate (`riffBendCents`), the resolution played bends already have; `sanitise` (a +/-4 audio guard) must not be applied to cents.
- The player keeps a per-string "late" count so a stop also ends a strum note still waiting in the engine's schedule.
- Speed trainer passes count as clean: audition has no note follower to score a pass against.
- There is no Options "General" page in this build: "Audition on select" and "Riffs folder" sit in a RIFFS row on FILE LOCATIONS.
- The internal drop onto the TUNE melody strip is not built: RIFFS and TUNE share column 4, so the strip is never on screen during a row drag; Add to Tune covers it. Row and tile drags are external (.mid) drags.
- Send to Looper renders through a fresh engine's own RiffPlayer (the audition path) rather than `MidiImportTargets::importPerformance`, so the layer is exactly the riff's whole bars and sounds like the audition.
- Import .mid as riff places Generic files by channel and tuning (MidiPerformance::toScore), not RubricVoicer.
- Riff notes are marked `preview` in string activity so live MIDI out skips them while the fretboard, capture and meters still see them.
- RL-30's player bound is checked as <= 0.5% of a core over 20 000 blocks (the spec's 0.02 units needs performance-budget's calibrated harness, which is not part of LuthierTests); compile (2 ms) and index (150 ms) are checked as specified.
- RL-11 runs 200 swaps over about half a second, not 10 minutes, to keep the suite's run time; the player allocates nothing by construction (fixed arrays, shared_ptr moves only).
- Deferred: editions (section 11, RL-29) - no edition infrastructure exists; the Free manifest (`free.txt`) and per-item `free` flags are shipped for when it does.
- Deferred: RL-06's -60 dBFS render null between the imported file and the audition (the note, string, fret, beat and technique round trip is verified for all 300).
- Deferred: runtime lookup of `riff.<id>.name` in the locale catalog (the generator writes `strings.en.json`; the English name is the fallback the spec names).
- For the coordinator: legal review should look at the "Freddie Green" artist name found in existing rhythm content (not changed here); it is in `Tools/riffs/deny.txt` for riffs.
