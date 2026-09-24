## practice-tools.md

The drawer (eight tabs, strip readouts, practice level, tap, panic) and the PRACTICE setup tab (progress, routines, defaults, library, session setup) are built, and the §12.1 setup-surface tests all exist. The runtime tools have real holes: backing-track pitch and tempo shift store values that the renderer never reads; MP3 is not enabled although the file chooser offers `*.mp3`; the scale-trainer quiz is never fed played MIDI (`ScaleTrainer::answer(int midiNote)` has no caller); the metronome ignores host and tap tempo; the tab reader reads only ASCII/MusicXML into a static text view (no GP5/GP/PTB, cursor, count-in, fretboard highlight or scoring); looper and backing-track pan/low-cut/high-cut have no controls; the loop MIDI re-render is never performed. Three of the five §12 runtime tests are missing.

| Req | Summary | Engine location | GUI location | Test | Status |
|---|---|---|---|---|---|
| PT-1 (§0.1) | Isolated panel; zero CPU when closed — untested | `PluginProcessor.cpp:1313` `practicePanelOpen` gate | Practice drawer | - | NO-TEST |
| PT-2 (§0.2) | Click to monitor by default, optional main | `PluginProcessor::processSlice` click route | drawer METRO "click to main" | `TuneProcessor::theMetronomeTabSendsTheClickToTheMainOut` | DONE |
| PT-3 (§0.3, §2) | Loop re-renders stored MIDI through a new tone — MIDI captured and saved, re-render never done | `Looper::captureMidi`, `Looper::save` (MIDI events) | - | - | PARTIAL |
| PT-4 (§0.4, §3) | Backing tracks stream from disk, 4-s ring | `BackingTrackPlayer` (BufferingAudioSource, `kRingBufferSeconds`) | drawer TRACK | - | NO-TEST |
| PT-5 (§1) | Time signatures incl. custom | `Metronome::setTimeSignature` | drawer METRO | `PracticeMetronome::settingsRoundTrip` | DONE |
| PT-6 (§1) | Tempo 20-300, follows tap + host tempo — never follows either | `Metronome::setTempo` | METRO `tempoSlider` | - | PARTIAL |
| PT-7 (§1) | Accent map, accent/normal/ghost (+silent) | `Metronome::setBeatAccent` | METRO beat buttons | `PracticeMetronome::accentPatternIsObeyed` | DONE |
| PT-8 (§1) | Subdivisions incl. dotted, each with own gain — untested | `Metronome::setSubdivision/setSubdivisionLevelDb` | METRO | - | NO-TEST |
| PT-9 (§1) | 6 click sounds | `ClickSound` | METRO sound box | `PracticeMetronome::everyClickSoundIsAudibleAndFinite` | DONE |
| PT-10 (§1, §10) | 16 click WAVs in Resources/Practice/Clicks — clicks synthesized; folder absent | `Metronome` synth | - | - | MISSING |
| PT-11 (§1) | Silent bars every N (1-16) | `Metronome::setSilentBarPeriod` | METRO | `PracticeMetronome::silentBarsMuteTheClickWithoutStoppingTheCount` | DONE |
| PT-12 (§1) | Progressive tempo A->B over N bars | `Metronome::startProgressiveTempo` | METRO progressive controls | `PracticeMetronome::progressiveTempoRampsAndStops` | DONE |
| PT-13 (§1) | 4-dot visual — untested | `MetronomeTab::paint` | METRO | - | NO-TEST |
| PT-14 (§2) | Loop 1-240 s, bar-quantized | `Looper` | drawer LOOP | `PracticeLooper::loopLengthQuantisesToBars` | DONE |
| PT-15 (§2) | 8 layers, MIDI + audio | `Looper::kMaxLayers` | LOOP layer strips | `PracticeLooper::recordsClosesAndOverdubs` | DONE |
| PT-16 (§2) | Undo/redo per layer | `LoopLayer` undo | LOOP undo | `PracticeLooper::layerUndoAndRedo` | DONE |
| PT-17 (§2) | Reverse / half-speed per layer | `LoopLayer::setReversed/setHalfSpeed` | LOOP | `PracticeLooper::reverseAndHalfSpeedDoNotAlterTheRecording` | DONE |
| PT-18 (§2) | Overdub / replace / play-once — modes untested | `LayerMode` | LOOP mode box | - | NO-TEST |
| PT-19 (§2) | Per-layer volume, pan | `LoopLayer::setLevelDb/setPan` | LOOP level/pan | `PracticeLooper::mutedLayersAreSilent` | DONE |
| PT-20 (§2) | Per-layer low-cut / high-cut — engine only | `LoopLayer::setLowCutHz/setHighCutHz` | - | - | NO-GUI |
| PT-21 (§2) | Export mixdown / stems — untested | `Looper::exportMixdown/exportStems` | LOOP Export mix / Export stems | - | NO-TEST |
| PT-22 (§2) | `.luthierloop` save/load with MIDI, audio, settings — untested | `Looper::save/load` | LOOP Save/Load | - | NO-TEST |
| PT-23 (§2) | Audio snapshots in temp folder, moved on save — held in memory, written as WAVs beside the loop JSON | `Looper::save` | n/a | - | PARTIAL |
| PT-24 (§3) | WAV/AIFF/FLAC/MP3 — no MP3 (`registerBasicFormats`, no JUCE_USE_MP3AUDIOFORMAT/dr_mp3) though chooser offers *.mp3 | `BackingTrack.cpp:33` | TRACK Load (`PracticePanel.cpp:670`) | - | PARTIAL |
| PT-25 (§3) | Volume, mono — untested | `setLevelDb/setMonoSum` | TRACK | - | NO-TEST |
| PT-26 (§3) | Pan, low-cut, high-cut — engine only | `BackingTrackPlayer::setPan/setLowCutHz/setHighCutHz` | - | - | NO-GUI |
| PT-27 (§3) | Loop points with zero-crossing snap — untested | `setLoopSeconds`, `snapToZeroCrossing` | TRACK set start/end | - | NO-TEST |
| PT-28 (§3) | Pitch shift +-12 st, tempo-independent — stored, renderer never reads `pitchSemis` | `BackingTrackPlayer::setPitchShiftSemitones` | TRACK `pitchSlider` (no effect) | - | MISSING |
| PT-29 (§3) | Tempo 25-200 %, pitch-independent — stored, renderer never reads `tempoRatio` | `BackingTrackPlayer::setTempoRatio` | TRACK `tempoSlider` (no effect) | - | MISSING |
| PT-30 (§3) | Section markers with names, jump hotkeys — markers unnamed in UI, no hotkeys | `addMarker/jumpToMarker` | TRACK Mark + `markerBox` | - | PARTIAL |
| PT-31 (§3) | Auto-detect tempo on load — untested | `BackingTrackPlayer::estimateTempo` | TRACK `tempoLabel` | - | NO-TEST |
| PT-32 (§3) | Gapless playlist — `setPlaylist` exists, no UI, not gapless | `BackingTrackPlayer::setPlaylist` | - | - | PARTIAL |
| PT-33 (§4) | Scale Explore with interval/degree/note overlays — no overlay selector | `ScaleTrainer` | drawer SCALE | `PracticeTrainers::scaleTrainerKnowsItsScales` | PARTIAL |
| PT-34 (§4) | Quiz: detects played note via MIDI, scores — `answer(midiNote)` never fed MIDI | `ScaleTrainer::answer` | SCALE mode Quiz | `PracticeTrainers::scaleQuizScoresAnswers` (direct call) | PARTIAL |
| PT-35 (§4) | Interval trainer and chord-tone trainer — modes exist, no played-note input or choice list | `ScaleTrainer::Mode` | SCALE `modeBox` | - | PARTIAL |
| PT-36 (§4) | All diatonic modes, harmonic/melodic minor, pentatonic, blues | `ScaleType` | SCALE `scaleBox` | `PracticeTrainers::scaleTrainerKnowsItsScales` | DONE |
| PT-37 (§4) | Custom scale via interval list — engine only | `ScaleTrainer::setCustomIntervals` | - | - | NO-GUI |
| PT-38 (§5) | Ear training: intervals, 11 chord qualities, adaptive, stats.json — 14 progressions (spec: 5 named + 10 = 15) | `EarTrainer`, `Trainers.cpp:kProgressions` | drawer EAR | `PracticeTrainers::earTrainerPosesAnswerableQuestions`, `PracticeTrainers::earTrainerDifficultyAdapts`, `PracticeTrainers::earTrainerStatsRoundTrip` | PARTIAL |
| PT-39 (§5) | Uses Luthier's own guitar sound — untested | EarTab audition through engine | EAR | - | NO-TEST |
| PT-40 (§6) | Tab formats GP5/GP/ASCII/MusicXML/PTB — ASCII and MusicXML only | `Notation/NotationImporter` | drawer TAB Open | `Notation::importerIsHonestAboutWhatItReads` | PARTIAL |
| PT-41 (§6) | Scrolling tab view, cursor, tempo, section loop, count-in — static TextEditor | `TabReaderTab::tabView` | TAB | - | MISSING |
| PT-42 (§6) | Highlight current fret on the fretboard | - | - | - | MISSING |
| PT-43 (§6) | Speed trainer (+N% per pass until misses) — in routines only, not in TAB | `SpeedTrainer` (PracticeRoutineTempo) | PRACTICE routines | `PracticeRoutine::theSpeedTrainerClimbsUntilAPassMissesNotes` | PARTIAL |
| PT-44 (§6) | Play-along note detection and scoring | - | - | - | MISSING |
| PT-45 (§7) | Chord progression looper: symbols, voiced, strummed, looped; tempo/feel/kit — only parsing tested | `ProgressionLooper` | drawer PROG | `PracticeTrainers::progressionParsing` | NO-TEST |
| PT-46 (§8) | Session recorder ring (default 60 min), audio + MIDI | `SessionRecorder` | drawer SESSION | `PracticeSession::ringBufferNeverGrows`, `PracticeGaps::theSessionRecorderRecordsWhatItIsToldTo` | DONE |
| PT-47 (§8) | Save last take WAV + MIDI, timestamp name | `SessionRecorder::saveLastTake` | SESSION Save | `PracticeGaps::theSessionRecorderRecordsWhatItIsToldTo`, `PracticeGaps::theSaveButtonDragsTheSavedTakeOut` | DONE |
| PT-48 (§8) | Sessions/tmp, 24 h cleanup — cleanup untested | `SessionRecorder::cleanUpOldTempFiles` (PluginProcessor.cpp:130) | n/a | - | NO-TEST |
| PT-49 (§8) | Off by default, enabled in Options | `OptionsPages` recorder toggle | Options | `PracticeSession::disabledByDefault` | DONE |
| PT-50 (§9) | Drawer 32-360 px resizable; collapsed strip bpm / loop LED / track title — untested | `PracticePanel` | drawer | - | NO-TEST |
| PT-51 (§9) | Tabs METRO..SESSION in order | `PracticePanel` tabs | drawer | `PracticeRoutine::toolsAreInTheDrawersTabOrder` | DONE |
| PT-52 (§9) | Drawer master volume, tap (mirrors global), panic — untested | `PracticePanel::practiceLevel/tapButton/panicButton` | drawer strip | - | NO-TEST |
| PT-53 (§11.2) | Progress: 90 days, per tool, accuracy, tempo, streak, CSV, clear | `PracticeStats`; `PracticeSetupPanel` | ADVANCED > PRACTICE | `PracticeRoutine::progressReportsDaysStreaksAccuracyAndTempo`, `PracticeSetupPanel::progressReadsStatsJsonBackAndSaysSoWhenEmpty` | DONE |
| PT-54 (§11.2) | Routines: 3 factory, drive the drawer, saved JSON | `PracticeRoutineLibrary/Runner` | PRACTICE | `PracticeRoutine::theThreeFactoryRoutinesLastTheirStatedLengthAndAllApply`, `PracticeSetupPanel::aRoutineStartsInTheDrawerFromTheTab` | DONE |
| PT-55 (§11.2) | Defaults per tool | `PracticeDefaults` | PRACTICE | `PracticeSetupPanel::defaultsControlsWriteTheModelAndApplyOnReopen`, `PracticeGaps::theLooperClosesItsFirstLoopAtTheDefaultLength` | DONE |
| PT-56 (§11.2) | Library: loops, sessions, backing folder, recent tabs | `PracticeLibrary` | PRACTICE | `PracticeSetupPanel::libraryListsLoadsAndDeletesAndStatesItsEmptyLists`, `PracticeGaps::openingATabInTheReaderListsItAsRecent` | DONE |
| PT-57 (§11.2) | Session setup: ring length, audio/MIDI, auto-save, size warning | `SessionRecorderSetup` | PRACTICE | `PracticeSetupPanel::sessionSetupWritesTheModelAndStatesTheSizeInPlainWords`, `PracticeGaps::stoppingTheRecorderSavesWhenAutoSaveIsOn` | DONE |
| PT-58 (§11.3, §11.4) | No transport on tab; empty states | `PracticeSetupPanel` | PRACTICE | `PracticeSetupPanel::theTabExposesNoTransport`, `PracticeRoutine::emptyStatesReadAsTheSpecWritesThem` | DONE |
| PT-59 (§12) | Test: metronome +-0.5 ms over 60 s | - | n/a | `PracticeMetronome::interClickIntervalIsWithinHalfAMillisecond` | DONE |
| PT-60 (§12) | Test: looper audio vs fresh render null -80 dBFS | - | n/a | - | MISSING |
| PT-61 (§12) | Test: 60-min WAV stream, no memory growth | - | n/a | - | MISSING |
| PT-62 (§12) | Test: 50 GP fixture files parse | - | n/a | - | MISSING |
| PT-63 (§12) | Test: session recorder never allocates on audio thread | - | n/a | `PracticeSession::ringBufferNeverGrows`, `PracticeGaps::theSessionRecorderTakesMidiWithoutAllocating` | DONE |
| PT-64 (§12.1) | Tests: stats accumulate, routine drives drawer, round-trip, defaults apply, clear confirms | - | n/a | `PracticeRoutine::sixtySecondsOfMetronomeAddSixtySecondsAgainstToday`, `PracticeRoutine::aRoutineDrivesTheDrawerEntryByEntryOnTime`, `PracticeRoutine::aFiveEntryRoutineRoundTripsIdentically`, `PracticeRoutine::defaultsSurviveAReopenAndStartTheMetronomeAtThem`, `PracticeSetupPanel::clearHistoryEmptiesStatsButKeepsLoopsAndSessions` | DONE |

<!-- counts DONE=26 NO-GUI=3 NO-TEST=15 PARTIAL=13 MISSING=9 OWNED=0 -->
