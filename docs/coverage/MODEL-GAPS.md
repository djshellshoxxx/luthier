# MODEL-GAPS coverage

Workstream MODEL-GAPS (branch `claude/luthier-model-gaps`): TODO 2k, 2d, 6e,
8 remainder (`bass-techniques.md`), 9, 10 and 11 remainders. One row per
actionable requirement; tests are `Suite::test` in `LuthierTests`.

Parameters added (3, appended in the `MODEL-GAPS` block, count test
`450 + 3`): `finger_alternation_variation`, `rest_stroke`, `aux1_pre_circuit`.

## Coverage

| ID | Spec + section | Implementation | Verification | Status |
|---|---|---|---|---|
| MG-2k-01 | part-acoustics 2.1 "feedback coupling feeds the feedback path gain" | `FeedbackLoop::setBodyCoupling` / `bodyCouplingFor`; `LuthierEngine::applyWorkshopGuitar`, `applyPartSwapLive`, `setGuitarType` | `ModelGaps::chamberingFeedsTheFeedbackCoupling` | verified |
| MG-2k-02 | notation-export 4 / 6.1: capture gets chord symbols (Poly) | `LuthierEngine::captureBlockState`, `ChordSymbol::writeName`, `RhythmEngine::detectHeldChord` | `ModelGapsUi::theCaptureHearsTechniquesAndChordsFromTheEngine` | verified |
| MG-2k-03 | notation-export 6.1: BASS_TECH and SLIDE_BAR events into the capture | `LuthierEngine::captureBassTechnique`, `captureBlockState` | `BassTechniques::aStrikeIsCapturedAsBassTech` | verified (slide bar: implemented, covered by the capture's own `Capture::bassAndSlideEventsBecomeLuthierEvents` for the conversion) |
| MG-2k-04 | TODO 9: the capture's technique hook from triggerNote / applyNoteOff | `LuthierEngine::triggerNote`, `applyNoteOff`, `setPerformanceCapture`; processor wires it | `ModelGapsUi::theCaptureHearsTechniquesAndChordsFromTheEngine`, existing `NotationTab` / `Capture` suites | verified |
| MG-2k-05 | notation-export 7.1: no allocation check compiles in | `LUTHIER_ALLOCATION_COUNTER=1` in CMake test defs; `allocationsOnThisThread()` in CircuitTests.cpp | `Capture::capturingTenThousandNotesDoesNotAllocate`, `TunePlayer` no-allocation checks now active | verified |
| MG-2k-06 | gui-engine-dataflow 22: FeedbackLed drains at 30 Hz | `FeedbackLed::kRefreshHz` | `ModelGapsUi::theFeedbackLedDrainsAtThirtyHertz` | verified |
| MG-2k-07 | file-formats 2: migration backup `Backup/<date>/<name>-v<schema>` | `PresetManager::needsMigration`, `backupMigratedOriginal`, `loadPreset` | `ModelGapsUi::aMigratedPresetKeepsItsOriginal` | verified |
| MG-2k-08 | notation-export 0.1: export on a worker thread | `NotationTakeExport::writeAsync` (NOTATION tab, header File menu) | `ModelGapsUi::notationExportRunsOnAWorkerThread` | verified |
| MG-2k-09 | ambiguity-resolutions 3: doubler pitch / HP / LP defaults | existing `DoublerPedal` | `ModelGapsUi::theDoublerDefaultsAreTheClassicAdt` | verified |
| MG-2k-10 | VP-7-04 at panel level: Standby / bypass on the panels | existing faces | `ModelGapsUi::standbyAndBypassReachTheFacesOnThePanels` | verified |
| MG-2d-01 | ambiguity-resolutions 4.3/4.7: bass pattern on the rhythm engine | `RhythmEngine::setBassPattern` (saved in its state), RHYTHM tab "Bass voicing" (`BassGridGroup`) | `BassTechniques::theStepGridRoundTripsAndEmptyLeavesThePattern`, existing `RubricVoicer` bass tests | verified |
| MG-2d-02 | retire `selectNotesForStyle` | removed from `RhythmEngine` | build; `Rhythm*` suites | verified |
| MG-2d-03 | ambiguity-resolutions 6: crossing velocity from the pattern | already in `RhythmEngine::resolveCrossingSps` | existing `StrumDynamics::crossingSourcesResolveInOrder` and the 22.7 ms test | verified (pre-existing) |
| MG-2d-04 | ambiguity-resolutions 8: feedback / freeze / E-Bow are mod destinations | `ModMatrix` (fixed: offsets no longer clamped to +-4 units) | `ModelGapsUi::theSustainControlsAreModulationDestinations` | verified |
| MG-2d-05 | ambiguity-resolutions 8 / routing-io 2: Aux 1 pre/post-circuit toggle | `aux1_pre_circuit`, `LuthierEngine::setAuxDiPreCircuit`, ROUTING tab toggle | `ModelGapsUi::auxOneTapsBeforeOrAfterTheCircuit` | verified |
| MG-6e-01 | DECISIONS C-09 / ui-wiring 6.2: off-thread build, block-boundary swap | `LuthierEngine::swapPartsAtBlockBoundary`, `applyPartSwapLive`; processor `applyGuitar` | `PartSwap::*` (3 tests) | verified |
| MG-BT-01 | bass-techniques 0.1: inert on a guitar | family checks in `SlapEngine`, fingerstyle, rest stroke, palm profile | `BassTechniques::inertOnAGuitar` | verified |
| MG-BT-02 | 0.1: SLAP group hidden on a guitar; fixed empty-state text | `SlapGroup` on the CHARACTER tab | `BassTechniques::theSlapGroupIsShownOnlyOnABass` | verified |
| MG-BT-03 | 2.1 slap uses the buzz generator | existing `SlapEngine` | existing `Slap::theClackIsTheFretBuzzGenerator`, `SlapWiring::theClackComesFromTheBuzzGenerator` | verified |
| MG-BT-04 | 12 slap is not a loud pluck (centroid 1.5x, rise 3x) | existing `SlapEngine::shapeExcitation` | `BassTechniques::aSlapIsNotALoudPluck` | verified (rise measured on the excitation, see Decisions) |
| MG-BT-05 | 12 pop brighter and shorter than slap by 20% | existing | `BassTechniques::aPopIsBrighterAndShorterThanASlap` | verified |
| MG-BT-06 | 4 double thump at its spacing and ratio | existing | `Slap::theUpStrokeComesAtItsGapAndItsRatio`, `SlapWiring::theDoubleThumpComesBackAtItsGap` | verified (pre-existing) |
| MG-BT-07 | 5 ghosts pitchless; auto-ghost below threshold only | existing | `SlapWiring::aGhostIsAThumpWithNoPitch`, `BassTechniques::autoGhostingFiresBelowTheThresholdOnly` | verified |
| MG-BT-08 | 6 finger alternation (`finger_alternation_variation`) | `BassFingerstyle`, engine re-schedules the middle finger | `BassTechniques::fingerAlternationVaries` | verified |
| MG-BT-09 | 6 rest stroke damps the next-lower string >= 12 dB in 10 ms | `BassFingerstyle::restStringFor`, `StringEngine::touch` | `BassTechniques::theRestStrokeDampsTheNextLowerString` | verified |
| MG-BT-10 | 6 bass pluck position nearer the bridge | `BassFamilyDefaults` (0.12) | `BassTechniques::bassDefaultsApplyOnLoad` | verified |
| MG-BT-11 | 7 bass palm-mute profile: shorter, more fundamental | `StringEngine::Damping::PalmMuteBass` | `BassTechniques::aBassPalmMuteIsShorterAndDarker` | verified |
| MG-BT-12 | 7 pick bass heavier (1.14 mm) | `BassFamilyDefaults` | `BassTechniques::bassDefaultsApplyOnLoad` | verified |
| MG-BT-13 | 8 bass defaults everywhere (strum 100 sps / 0.01, squeak 0.35, Factory low, 864 mm, 1.14 mm, compressor 2:1), defaults not constraints | `BassFamilyDefaults::retarget` (+ existing strum retarget, neck parts) | `BassTechniques::bassDefaultsApplyOnLoad`, `BassTechniques::aUserSettingSurvivesTheFamilyChange` | verified |
| MG-BT-14 | 9 bass step grid on the RHYTHM tab (thumb, pop, ghost, fingerstyle, dead) | `BassStepGrid`, `RhythmEngine::processBassGrid`, `NoteOnEvent::bassTechnique`, `BassGridGroup` | `BassTechniques::theStepGridPlaysItsTechniquesOnABass`, `...RoundTripsAndEmptyLeavesThePattern`, `...aGridTechniqueIsAStrikeOnABassOnly` | verified |
| MG-BT-15 | 9 genre kits gain bass kits using the grid | `GenreKit::bassGrid`, four factory bass kits | `BassTechniques::theBassKitsInstallTheirGrids`, `GenreKits::*` | verified |
| MG-BT-16 | 11 +14 params (12 pre-existing, 2 new here) | `Parameters` MODEL-GAPS block | `Parameters::everyParameterHasAUniqueIdAndSaneDefault` (count 450 + 3) | verified |
| MG-BT-17 | 12 headroom: full slap on every string < 0 dBFS, limiter off | existing gain structure | `BassTechniques::aFullSlapOnEveryStringHasHeadroom` | verified |
| MG-BT-18 | 12 BASS_TECH round-trips; Generic keeps velocity distinction | existing `MidiProfiles` | existing `MidiExport::everyEventClassRoundTripsWithEveryField`, `Capture::bassAndSlideEventsBecomeLuthierEvents`, `BassTechniques::aStrikeIsCapturedAsBassTech` | verified |
| MG-9-01 | notation-export 3: the current bar as fretboard tablature dots | `FretboardComponent::refreshTabDots`, NOTATION tab "ON FRETBOARD" | `ModelGapsUi::theCurrentBarIsDrawnOnTheFretboardAsTabDots` | verified |
| MG-9-02 | notation-export 4: Mono-mode offline chord extraction | `PerformanceCapture::toScore` (`extractChordsWhenMissing`) | `ModelGaps::aMonoTakeGetsItsChordsOffline` | verified |
| MG-9-03 | midi-export 4.1 marked region (NOTATION) | `PerformanceCapture::markIn/markOut`, `CaptureScoreOptions::sampleRange`, `CaptureRanges` | `CaptureRanges::theMarkedRegionIsWhatWasPlayedBetweenTheMarks` | verified |
| MG-10-01 | midi-export 2.1/6: CHARACTER seed / environment events live | `LuthierAudioProcessor::sendCharacterChanges` | `ModelGapsUi::characterSeedAndEnvironmentGoOutAsTheyChange` | verified |
| MG-10-02 | midi-export 5: File -> Import -> MIDI | header File menu "Import MIDI...", `LuthierAudioProcessorEditor::importMidiFile` | `MidiImport::aDropOnTheWindowImports` (same path) | verified |
| MG-10-03 | midi-export 5: drag a .mid onto the window | editor `FileDragAndDropTarget` | `MidiImport::aDropOnTheWindowImports` | verified |
| MG-10-04 | midi-export 5: auto-detect profile; Generic imports with defaults | `MidiProfiles::importFromFile` via `MidiImportTargets` | `MidiImport::bothProfilesGoIntoTheSession` | verified |
| MG-10-05 | midi-export 5: targets session / tune builder / looper | `MidiImportTargets::importPerformance`, `SessionRecorder::importMidi`, `Looper::loadLayerAudio` | `MidiImport::bothProfilesGoIntoTheSession`, `...theTuneBuilderGetsANewTune`, `...theLooperGetsARenderedLayer` | verified |
| MG-10-06 | midi-export 4.1: marked region and current section (MIDI OUT) | `CaptureRanges::midiCaptureRange`, `MidiOutPanel::chosenRange`, MARK IN / OUT | `CaptureRanges::*` | verified |
| MG-10-07 | midi-export 4.2: drag from the session recorder's Save (Luthier / Alt Generic) | `SessionTab::SaveButton` | `PracticeGaps::theSaveButtonDragsTheSavedTakeOut` | verified (the OS drag itself is not automated) |
| MG-11-01 | practice-tools 8/11.2: record audio / MIDI switches | `SessionRecorder::setRecordAudio/Midi`, `SessionRecorderSetup::applyTo` | `PracticeGaps::theSessionRecorderRecordsWhatItIsToldTo` | verified |
| MG-11-02 | practice-tools 8: recorder takes MIDI without allocating | `SessionRecorder::captureMidi` FIFO; processor now feeds it | `PracticeGaps::theSessionRecorderTakesMidiWithoutAllocating` | verified |
| MG-11-03 | practice-tools 11.2: auto-save on stop | `SessionRecorder::stop`, drawer SESSION toggle | `PracticeGaps::stoppingTheRecorderSavesWhenAutoSaveIsOn` | verified |
| MG-11-04 | practice-tools 11.2: looper default length | `Looper::setDefaultLengthSamples`, `PracticeDefaults::applyTo` | `PracticeGaps::theLooperClosesItsFirstLoopAtTheDefaultLength` | verified |
| MG-11-05 | practice-tools 11.2: trainers' note range and question count | `ScaleTrainer` / `EarTrainer::setNoteRange`, `setQuestionCount` | `PracticeGaps::theTrainersKeepToTheirRangeAndSessionLength` | verified |
| MG-11-06 | TODO 11: a test through the tab reader's recent list | `TabReaderTab::openTab` | `PracticeGaps::openingATabInTheReaderListsItAsRecent` | verified |

## Decisions

- Chambering to feedback: solid body is the loop's reference (factor 1, so every compiled guitar and every solid parts guitar sounds as before); other rows scale k_couple by sqrt(table / 0.1) - hollow ~2.8x - because the loop gain is an amplitude and saturates at its ceiling; acoustic "n/a" is left at 1 rather than switching the loop off.
- The capture is fed by the engine itself (processor no longer calls `captureStringActivity`), which keeps frets continuous and adds technique; `captureStringActivity` stays for callers without an engine.
- Poly-mode chord track: the detector's held-notes reading each block, written on change and only when known; Mono mode (and any take without a chord track) extracts chords offline per beat.
- Slide bar into the capture: sent when it moves more than 0.05 fret or its pressure class (lift / light below 0.4 / full) changes.
- Notation export on a worker: the take is drained and copied on the message thread, converted and written on a single-thread pool, reported back on the message thread. The test waits on the worker rather than pumping the whole message queue, which would dispatch other tests' leftovers.
- Migration backup: copied (not moved) once per file and schema, beside the file's own folder like the existing save backups; the original stays until the user saves it in the new schema.
- Mod matrix: offsets were run through the audio guard `sanitise` (+-4), so any destination with a range over a few units (percent, ms, Hz) could not be modulated past 4 units; they are now only checked for being finite and clamped to the parameter's range by `apply`. Found by the new sustain-destination test.
- Aux 1 toggle is a parameter (saved with the preset, automatable) and defaults to post-circuit, routing-io 2's DI.
- Block-boundary part swap applies only when string count, tuning, family, frets, bridge, pickup count / selector, rig defaults and the body IR are unchanged and a parts guitar is loaded; everything else (and any swap without a running audio thread) keeps the park. The message thread waits (bounded 250 ms) for the boundary so callers see the same state as before; the audio thread only takes and hands back the prepared object, the message thread frees it. Sounding strings keep their pitch (no snap).
- Fingerstyle applies to a bass played with fingers (or a non-pick striker) on a plain pluck; the middle finger lands up to 6 ms x variation later, 35% darker, 5% further from the bridge and 10% softer at variation 1, deterministic so performances repeat. A note-off that arrives before its delayed note-on is moved to just after it.
- Rest stroke: `StringEngine::touch` damps what is already in the loop over 1.5 ms for one trip round it (a finger laid on the string), because loop damping alone cannot stop a 55 Hz string inside one 18 ms period; skipped for a string plucked within 30 ms (a double stop).
- Test thresholds: the slap's 3x faster rise is measured on the excitation force pulse (2.1.1 states it as 0.3 ms vs 2 ms of excitation) - on the output both rises are the first quarter-period of the note; finger alternation's "measurably" / "do not" is judged against three standard errors of the per-note centroid scatter, since the excitation's own attack jitter (engine 5.5) varies every note.
- Bass defaults follow the strum retarget rule: on a family change, a value still at the old family's default moves to the new family's, a user value stays; preset loads do not retarget. Compressor: an empty first pre-amp slot gets a 2:1 compressor on a bass; a guitar removes it only while it is still the family's untouched 2:1. Pluck position default for bass: 0.12.
- Factory bass presets change sound (rest stroke on, alternation 0.25, bass palm profile): the spec states these as the bass defaults. Guitar presets are unchanged (all of it is bass-only).
- The bass step grid is structural (rhythm engine state, saved with the preset), not parameters, like the strum grid; an empty grid leaves the strum pattern in charge, a guitar kit clears it, a bass kit installs its grid. Bass kits name all-string strum patterns so they still sound on a guitar.
- SLAP group placed on the CHARACTER tab (bass-techniques 9 / gui-integration 4.4) with the fingerstyle pair; the Techniques tab's SLAP sub-tab (gui-techniques-updates) is another workstream's.
- MIDI import: the tune target builds one section long enough for the notes and records them through the TUNE tab's Record path (sixteenth grid); the looper target renders the notes with a fresh engine on the loaded guitar type (the looper holds audio) and needs the looper stopped; the session target appends the MIDI after what the recorder holds.
- Session recorder MIDI goes through a fixed 8192-event FIFO (the recorder was never given MIDI before); SysEx is not kept.
- Save-button drag hands out a Luthier-profile file of the capture over the recorded span (Generic with Alt), then the WAV; the raw MIDI when nothing was captured.
- Marked region: one pair of marks on the performance capture, on the processor's sample clock, serves both tabs; an unmarked region exports nothing rather than everything. Current section: the TUNE tab's selected section where it first plays, tune beat 0 = host quarter note 0.
- CHARACTER events: stated when the EVENTS source comes on, then on change, at the block's first sample; environment in file units (cold 10 C, room 21.5 C, warm 32 C; dry 25%, normal 45%, humid 70%). Not added to the MIDI OUT file export, which exports the raw MIDI capture.
- Trainers: a note outside the range is not an answer; ear-trainer questions are moved by octaves into the range; 0 questions is open-ended.
- Looper default length closes the first recording on the sample; 0 leaves it to the player.
