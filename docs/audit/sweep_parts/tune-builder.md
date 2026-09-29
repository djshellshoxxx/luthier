## tune-builder.md

TUNE-HELP has landed on this checkout: the export dialog (audio with stems, MIDI profiles, notation, project), chord popover and pill editing, the full piano roll, bass and layer rows, Vary and the setlist strip, TOOLS/STYLE/FOLLOW, hum capture, timeline modulation, snapshot sections, looper capture and Ctrl+T, with the `TuneEditing`, `TuneIntegration` and `HumCapture` suites registered. Still open: variations (TB-9), artist/time signature (TB-6), the melody string hint (TB-12), range/density controls (TB-49), horizontal roll scroll (TB-28), a Ctrl+S/Ctrl+E test (TB-22), kit names (TB-15), companion-instance bass routing (TB-60), MP3 (TB-65), standalone input arming (TB-72) and the -60 dBFS round-trip decision (TB-T8).

| Req | Summary | Engine location | GUI location | Test | Status |
|---|---|---|---|---|---|
| TB-1 (§0.1) | Control-only: writes MIDI into the rhythm/note engine | `Tune/TunePlayer.cpp:TunePlayer` -> `LuthierEngine` direct MIDI | n/a | `TuneProcessor::theMelodySoundsWhileTheRhythmEngineStrums` | DONE |
| TB-2 (§0.2, §11) | Save/reload `.luthiertune`, small, forward-compatible | `Tune/TuneFile.cpp` | Adv TUNE header, `TunePanel::saveButton/loadButton` | `TuneBuilder::aHundredRandomTunesRoundTripByteIdentical`, `TuneBuilder::unknownFieldsAreKeptAndWrittenBack` | DONE |
| TB-3 (§0.3) | Every edit undoable; locked notes survive regenerate | `Tune/TuneSession.cpp:TuneSession::edit`, `regenerateMelody` | TUNE AUTO, Ctrl+Z in the tab | `TuneBuilder::regenerateLeavesLockedNotesByteIdentical`, `TunePanel::sessionUndoGroupsSameTargetEditsWithin200ms` | DONE |
| TB-4 (§0.4) | Works inside the plugin, no DAW | `PluginProcessor` owns `TuneSession`/`TunePlayer` | Adv TUNE tab | `TuneProcessor::thePluginStateKeepsTheTuneAndTheClickRoute` | DONE |
| TB-5 (§0.5) | Audio, MIDI and notation export from one score | `Tune/TuneMidi.cpp:buildTuneMidiFile`; `Support/TuneExport.*` | `UI/TuneExportDialog.*` | `TuneIntegration.theExportDialogWritesEachDestinationFromOneScreen` | DONE |
| TB-6 (§1) | Meta: title, artist, tempo, time sig, key, mode, swing, feel — no artist / time-signature control; swing only via the kit (KIT button) | `Tune/TuneModel.h:TuneMeta` | TUNE header title/tempo/key/mode; KIT button sets swing | `TunePanel.theHeaderEditsTitleTempoKeyAndSavesAndLoads` | PARTIAL |
| TB-7 (§1) | Sections carry length, chords, rhythm, kit, melody, bass, layers | `Tune/TuneModel.h:TuneSection` | TUNE section strip | `TuneBuilder::timelinePutsChordsMelodyBassAndSectionsOnTheBeat` | DONE |
| TB-8 (§1) | Setlist of section refs with repeats | `TuneArrangement::setlist` | section right-click Repeat, `TuneSectionStrip::buildMenu` | `TuneBuilder::editsKeepNamesUniqueAndTheSetlistInStep`, `TunePanel::theSectionStripsMenuRenamesDuplicatesRepeatsTagsLinksAndDeletes` | DONE |
| TB-9 (§1) | Variations (A/B arrangements) — `TuneVariation` model struct only; no UI or playback | `Tune/TuneModel.h:TuneVariation` | - | - | MISSING |
| TB-10 (§1.1) | ChordCell fields and shorthand parsing | `Tune/TuneTheory.cpp:parseProgression` | progression field | `TuneBuilder::shorthandParsesToTheExpectedCells` | DONE |
| TB-11 (§1.1) | Underspecified last cell holds to fill | `resolveChordSpans` | n/a | `TuneBuilder::chordSpansRepeatAShortProgressionAndHoldAnUnderspecifiedLastCell` | DONE |
| TB-12 (§1.2) | MelodyTrack `string_hint`, `articulation_default` — model fields only; no control, `TunePlayer` ignores the string hint | `MelodyTrack::stringHint` | - | - | PARTIAL |
| TB-13 (§1.2) | MelodyNote velocity/articulation/technique/locked editable | `MelodyNote` | `TunePianoRoll::buildNoteMenu` | `TuneEditing.thePianoRollSelectsNudgesCopiesAndEditsNotes` | DONE |
| TB-14 (§1.2) | Absolute and relative pitch (`root+N`, `chord_tone_N`) | `resolveMelodyPitch` | n/a | `TuneBuilder::relativePitchesFollowTheChordTheyLandOn` | DONE |
| TB-15 (§2.1) | Genre kit dropdown with suggested tempo, feel, palette — works, but the shipped kit names differ from the spec list (Folk Strum, Country Boom-Chick, …) | `Rhythm/GenreKit.*`; `Tune/TuneExamples.*:TuneKits` | TUNE rhythm `kitBox`; PALETTE/KIT buttons | `TuneIntegration.aGenreKitBringsItsTempoFeelAndPalette` | PARTIAL |
| TB-16 (§2.2) | Progression text field (bars, sections, `*2`) | `parseProgression`, `applyProgressionText` | TUNE `progressionEditor` | `TunePanel::theProgressionFieldWritesTheSectionAndShowsErrorsWhereTheyAre`, `TuneBuilder::typedShorthandReplacesTheActiveSectionOrTheNamedOnes` | DONE |
| TB-17 (§2.3, §4.1) | Melody Auto | `Tune/TuneMelody.cpp:generateAutoMelody` | TUNE `autoButton` | `TuneBuilder::autoMelodyIsByteIdenticalForASeedAcrossAThousandRuns` | DONE |
| TB-18 (§2.3, §4.2) | Melody Draw, snapped to key | `TunePianoRoll::addNote` | TUNE `drawToggle` + roll | `TunePanel::thePianoRollDrawsSnappedLockedNotesAndDeletesThem` | DONE |
| TB-19 (§2.3, §4.3) | Record from MIDI in, quantise on release, velocity kept | `TuneSession::finishRecording` | TUNE `recordToggle`, `quantiseBox` | `TunePanel::recordingQuantisesATakeIntoTheSection`, `TuneBuilder::recordQuantiseKeepsVelocityAndRefitsHeldNotesToTheNewChord` | DONE |
| TB-20 (§2.3, §4.4) | Improvise reseeds each pass; Freeze writes it | `generateImprovisedPass`, `freezeImprovisedPass` | TUNE `improviseToggle`, `freezeButton` | `TuneBuilder::improviseVariesEachPassAndFreezeWritesItDown`, `TunePlayer::anImprovisedTuneGetsAPrebuiltTimelineEachPass` | DONE |
| TB-21 (§2.5) | Play loops; edits take effect on the bar (at once when paused) | `TunePlayer::setTimeline` | TUNE `playButton` | `TunePlayer::anEditWhilePlayingWaitsForTheBarLineAndAPausedOneDoesNot` | DONE |
| TB-22 (§2.6) | Ctrl+S saves, Ctrl+E exports — `TunePanel::keyPressed` "save"/"export" bindings; no test | `TunePanel::keyPressed` | TUNE tab focus | - | NO-TEST |
| TB-23 (§2.6, §9) | Export dialog is one screen | `UI/TuneExportDialog.*` | EXPORT button | `TuneIntegration.theExportDialogWritesEachDestinationFromOneScreen` | DONE |
| TB-24 (§3) | TUNE tab between RHYTHM and LIVE | `UI/AdvancedPanel.cpp` tabs table | Adv Col 4 TUNE | `Editor::everyWorkspaceTabSelectsAndPaints` | DONE |
| TB-25 (§3.1) | Header: name, Save, Export, Tempo, Key | `TunePanel` header | TUNE `titleEditor/saveButton/exportButton/tempoSlider/keyBox` | `TunePanel::theHeaderEditsTitleTempoKeyAndSavesAndLoads` | DONE |
| TB-26 (§3.1, §3.3) | Section strip, click to open | `TuneSectionStrip` | TUNE section strip | `TunePanel::theSectionStripsMenuRenamesDuplicatesRepeatsTagsLinksAndDeletes` | DONE |
| TB-27 (§3.1, §3.5) | Rhythm strip: kit, feel, strum, on | `TuneSession` section rhythm | TUNE `kitBox/feelSlider/strumSlider/rhythmOn` | `TunePanel::theRhythmStripSetsTheSectionsKitFeelStrumAndOn`, `TunePanel::aRhythmChangeReachesTheRhythmEngine` | DONE |
| TB-28 (§3.1) | Piano roll 1-4 bars visible, scrollable — no horizontal scroll for sections longer than 4 bars; no test | `TunePianoRoll` | TUNE roll | - | PARTIAL |
| TB-29 (§3.1, §3.6) | Transport: <<, play/pause, >>, loop, count-in, metronome | `TunePlayer` | TUNE transport buttons | `TunePanel::theTransportAndSpaceDriveThePlayer`, `TunePlayer::skippingMovesBySectionsAndWrapsWithLoop`, `TunePlayer::aCountInWaitsABarAndClicksIt` | DONE |
| TB-30 (§3.2) | Pills coloured by diatonic function, neutral non-diatonic | `TuneChordPills::colourForDegree` | TUNE pills | `TuneBuilder::diatonicPaletteAndFunctionsFollowTheKey` | DONE |
| TB-31 (§3.2) | Click a cell: popover with 7 fields | `UI/TuneChordEditor.*` | pill click | `TuneEditing.theChordPopoverEditsEveryFieldOfTheCell` | DONE |
| TB-32 (§3.2) | Drag right edge = duration; drag to reorder | `UI/TuneChordPillsEditing.cpp` | pills | `TuneEditing.dragsResizeAndReorderChordPills` | DONE |
| TB-33 (§3.2) | Typing the field updates pills live | `TunePanel::progressionTextChanged` | TUNE field | `TunePanel::theProgressionFieldWritesTheSectionAndShowsErrorsWhereTheyAre` | DONE |
| TB-34 (§3.2) | Pill right-click: insert, duplicate, delete, copy/paste, substitution | `suggestSubstitutions`; `TuneChordPills::buildMenu` | pill menu | `TuneEditing.theChordPillMenuInsertsDuplicatesDeletesCopiesPastesAndSubstitutes` | DONE |
| TB-35 (§3.3) | Section rename, duplicate, delete, repeat, role tag | `TuneSectionStrip::buildMenu` | section right-click | `TunePanel::theSectionStripsMenuRenamesDuplicatesRepeatsTagsLinksAndDeletes` | DONE |
| TB-36 (§3.3) | Drag sections to reorder | `UI/TuneSectionStripEditing.cpp` | section drag | `TuneEditing.sectionsReorderVaryAndArrangeInTheSetlist` | DONE |
| TB-37 (§3.3) | "Vary" makes a sibling section | `Tune/TuneVary.*` | section menu | `TuneEditing.sectionsReorderVaryAndArrangeInTheSetlist` | DONE |
| TB-38 (§3.3) | Setlist edited by dragging tabs into a timeline strip | `UI/TuneSetlistStrip.*` | strip above tabs | `TuneEditing.sectionsReorderVaryAndArrangeInTheSetlist` | DONE |
| TB-39 (§3.4) | Roll: bar/beat lines, scale-row shading | `TunePianoRoll::paint` | TUNE roll | `TunePanel::rendersWithTheLookAndFeel` | DONE |
| TB-40 (§3.4) | Snap to key; `C` toggles chromatic | `TunePianoRoll::setChromatic` | TUNE roll, key C | `TunePanel.thePianoRollDrawsSnappedLockedNotesAndDeletesThem` | DONE |
| TB-41 (§3.4) | Note right-click: velocity, articulation, technique, unlock, delete | `TunePianoRoll::buildNoteMenu` | roll | `TuneEditing.thePianoRollSelectsNudgesCopiesAndEditsNotes` | DONE |
| TB-42 (§3.4) | Drag-box multi-select, shift-click adds | `TunePianoRoll::selectInBox` | roll | `TuneEditing.thePianoRollSelectsNudgesCopiesAndEditsNotes` | DONE |
| TB-43 (§3.4) | Cut/copy/paste; arrow nudge, Shift larger | `TunePianoRoll::nudgeSelected` | roll | `TuneEditing.thePianoRollSelectsNudgesCopiesAndEditsNotes` | DONE |
| TB-44 (§3.4) | Generators act on the current section, keep locked notes | `generateMelody(section)` | TUNE AUTO | `TuneBuilder::regenerateLeavesLockedNotesByteIdentical` | DONE |
| TB-45 (§3.5) | Rhythm per section; "Link rhythm to X" | `TuneSection::rhythmLinkedTo` | section right-click | `TunePanel::theSectionStripsMenuRenamesDuplicatesRepeatsTagsLinksAndDeletes`, `TunePanel::theSessionAppliesSectionRhythmToTheRhythmEngine` | DONE |
| TB-46 (§3.6) | Host transport wins when it plays | `TunePlayer` followingHost | n/a | `TunePlayer::theHostWinsWhenItPlaysAndTheClockRunsWhenItDoesNot` | DONE |
| TB-47 (§3.6) | Space play/pause, Shift+Space from section start | `TunePanel::keyPressed` | TUNE tab focus | `TunePanel::theTransportAndSpaceDriveThePlayer` | DONE |
| TB-48 (§4.1) | Auto rules: chord-tone start, step/leap mix, cadence rests, kit profile, range | `generateAutoMelody` | TUNE AUTO | `TuneBuilder::autoMelodyStaysInRangeRestsAtCadencesAndStartsOnAChordTone` | DONE |
| TB-49 (§4.1) | `melody_range` / `melody_density` "wider on request" — no control; density follows the kit only | `MelodyTrack::rangeLow/High, density` | - | - | NO-GUI |
| TB-50 (§4.1) | "Regenerate" increments the seed | `regenerateMelody` | TUNE AUTO again | `TunePanel.thePianoRollDrawsSnappedLockedNotesAndDeletesThem` | DONE |
| TB-51 (§4.3) | Quantise grids 1/4, 1/8, 1/8T, 1/16, 1/16T | `QuantiseGrid` | TUNE `quantiseBox` | `TuneBuilder::recordQuantiseKeepsVelocityAndRefitsHeldNotesToTheNewChord` | DONE |
| TB-52 (§4.3) | "Follow chord changes" toggle | `MelodyTrack::followChords` | FOLLOW on melody row | `TuneEditing.theChordToolsStyleAndFollowWorkFromTheTab` | DONE |
| TB-53 (§4.5) | Style transfer per section | `applyMelodyStyle` (`TuneBuilder::styleTransferChangesPhrasingButNeverPitchOrCount`) | STYLE box | `TuneEditing.theChordToolsStyleAndFollowWorkFromTheTab` | DONE |
| TB-54 (§5) | Diatonic palette appends chords | `makeDiatonicChord` | `UI/TuneToolsMenu.cpp` | `TuneEditing.theChordToolsStyleAndFollowWorkFromTheTab` | DONE |
| TB-55 (§5) | Suggest next chord (three moves) | `suggestNextChords` (`TuneBuilder::suggestNextChordOffersThreeDistinctCommonMoves`) | TOOLS | `TuneBuilder.suggestNextChordOffersThreeDistinctCommonMoves`, `TuneEditing.theChordToolsStyleAndFollowWorkFromTheTab` | DONE |
| TB-56 (§5) | Reharmonize, one shot with undo | `reharmonizeSection` (`TuneBuilder::reharmonizeSubstitutesAndKeepsTheLength`) | TOOLS | `TuneBuilder.reharmonizeSubstitutesAndKeepsTheLength`, `TuneEditing.theChordToolsStyleAndFollowWorkFromTheTab` | DONE |
| TB-57 (§5) | Transpose; melodies follow | `transposeTune` | TUNE `keyBox` | `TuneBuilder::transposeMovesChordsKeyAndAbsoluteMelodyAndIsReversible` | DONE |
| TB-58 (§5) | Modal shift + "Follow mode" | `shiftMode` | TUNE `modeBox`; TOOLS | `TuneEditing.theChordToolsStyleAndFollowWorkFromTheTab` | DONE |
| TB-59 (§6) | Bass modes off/root/root-fifth/walking/genre/manual | `generateBassLine` (`TuneBuilder::bassLinesFollowTheChordsAndWalkIntoTheNextRoot`) | `UI/TuneLayersStrip.*` BASS row | `TuneEditing.theBassAndLayerRowsEditTheSection` | DONE |
| TB-60 (§6) | Bass via bass engine, companion instance, or separate MIDI — engine and MIDI-out paths work; no companion-instance routing | `TunePlayer::setBassToEngine`, MIDI out | n/a | `TunePlayer.theBassGoesToTheEngineOnlyForABass`, `TuneProcessor.midiOutCarriesTheTuneWhenAskedTo` | PARTIAL |
| TB-61 (§7) | Layers pad/arpeggio/countermelody/percussion with on/off, volume, pan | `TuneLayer`, `TuneMidi` | LAYERS rows | `TuneEditing.theBassAndLayerRowsEditTheSection` | DONE |
| TB-62 (§8) | Playback through the realism engine | engine direct MIDI | n/a | `TuneProcessor::theMelodySoundsWhileTheRhythmEngineStrums` | DONE |
| TB-63 (§8) | Section state boundary resets envelopes/rhythm phase | `stateBoundary` | section menu | `TuneIntegration.aStateBoundarySectionResetsAtItsStart` | DONE |
| TB-64 (§8) | Loop plays the setlist end to end | `TunePlayer` loop | TUNE LOOP | `TunePlayer::loopingWrapsOnTheSampleWithTheEndBeforeTheStart` | DONE |
| TB-65 (§9.1) | Audio: WAV/FLAC/MP3, 16/24/32f, rate, stems aux 1-8, tail, Renders folder — no MP3 (no encoder; DECISIONS.md entry not written) | `TuneExport::renderAudio/exportAudio` | export dialog | `TuneIntegration.audioExportWritesTheMixAndEveryAuxStem` | PARTIAL |
| TB-66 (§9.2) | MIDI: Luthier/Generic profile, track splits, realism vs plain | `writeTuneMidiFile`; `TuneExport::exportMidi` | TUNE EXPORT | `TuneBuilder.theMidiFileHasAMetaTrackInstrumentTracksAndBeatAccurateTicks`; `TuneIntegration.aLuthierProfileMidiExportReimportsToTheSameAudio` | DONE |
| TB-67 (§9.3) | Notation MusicXML/GP/ASCII with sections and chord symbols | `buildTuneScore`; `TuneExport::exportNotation` | export dialog | `TuneBuilder.thePerformanceScoreKeepsSectionsChordSymbolsAndFrettedNotes`; `TuneIntegration.notationAndProjectExportKeepSectionsChordsAndTheBundle` | DONE |
| TB-68 (§9.4) | Project export bundling preset and guitar | `TuneExport::exportProject` | export dialog | `TuneIntegration.notationAndProjectExportKeepSectionsChordsAndTheBundle` | DONE |
| TB-69 (§10) | Template library (ten, DECISIONS C-16) | `Tune/TuneTemplates.cpp`, `Resources/Tunes/Templates` | TUNE `newButton` menu | `TuneBuilder::theTenTemplatesLoadInOrderAndAreValid` | DONE |
| TB-70 (§11) | JSON schema with signature header | `TuneFile` | n/a | `TuneBuilder::templateFilesAreInCanonicalFormAndBlankMatchesTheBuiltIn`, `TuneBuilder::loadErrorsAreNamedAndLeaveTheTuneAlone` | DONE |
| TB-71 (§12) | Standalone: last tune loads (or blank) | plugin state carries the tune | n/a | `TunePanel.theSessionRoundTripsThroughPluginState`; `TuneIntegration.aRelaunchLoadsTheLastTuneAndPlaysIt` | DONE |
| TB-72 (§12) | Standalone: MIDI in armed to the selected input; audio in armed — no standalone input arming (hum capture reads the sidechain only) | - | - | - | MISSING |
| TB-73 (§13) | Sung/hummed capture, 0.6 confidence, snap, Sing button | `DSP/Common/PitchTracker.*`, `Tune/TuneHumCapture.*` | SING in `TunePanel` | `HumCapture.singingIntoTheTuneTabWritesTheSectionsMelody` | DONE |
| TB-74 (§14) | Mod routes automate feel / tempo drift over the timeline | params `tune_feel_mod`, `tune_tempo_drift` | MOD matrix | `TuneIntegration.theTunesTimelineParametersDriftTempoAndFeel` | DONE |
| TB-75 (§14) | Snapshots capture the section; footswitch switches sections | `PluginProcessorTune.cpp:captureTuneSnapshotState` | LIVE snapshots | `TuneIntegration.aSnapshotRecallsTheTunesSection` | DONE |
| TB-76 (§14) | Looper captures a whole tune render | `Looper::importLayer` | TO LOOPER | `TuneIntegration.theLooperCapturesAWholeTuneRender` | DONE |
| TB-77 (gui 17) | Ctrl+T new tune in the shortcut registry | `AccessibilitySettings` `newTune` | editor `openNewTune` | `TuneIntegration.ctrlTIsInTheShortcutRegistryAndOpensTheTuneTab` | DONE |
| TB-T1 (§15) | Parser: 100 strings parse; malformed give named errors | `parseProgression` | n/a | `TuneBuilder::shorthandParsesToTheExpectedCells`, `TuneBuilder::aHundredGeneratedProgressionsRoundTripThroughShorthand`, `TuneBuilder::malformedShorthandIsRefusedWithANamedError` | DONE |
| TB-T2 (§15) | Auto melody byte-identical over 1000 runs | `generateAutoMelody` | n/a | `TuneBuilder::autoMelodyIsByteIdenticalForASeedAcrossAThousandRuns` | DONE |
| TB-T3 (§15) | Locked notes byte-identical after regenerate | `regenerateMelody` | n/a | `TuneBuilder::regenerateLeavesLockedNotesByteIdentical` | DONE |
| TB-T4 (§15) | 1000 random section reorders keep length and positions | `TuneModel` reorder | n/a | `TuneBuilder::aThousandSectionReordersKeepTheLengthAndEveryNotesPosition` | DONE |
| TB-T5 (§15) | 32-bar loop for 60 s without drift | `TunePlayer` | n/a | `TuneBuilder::aLoopedTuneRunsSixtySecondsWithoutDrift` | DONE |
| TB-T6 (§15) | 100 random files round-trip byte-identical | `TuneFile` | n/a | `TuneBuilder::aHundredRandomTunesRoundTripByteIdentical` | DONE |
| TB-T7 (§15) | Offline render nulls live within -80 dBFS | `TuneExport::renderAudio` | n/a | `TuneIntegration.theOfflineRenderNullsAgainstTheLiveOne` | DONE |
| TB-T8 (§15) | Luthier-profile MIDI re-import renders the same audio — held to -60 dBFS, not byte-identical; decision not recorded in DECISIONS.md | `TuneExport` + `MidiProfiles` | n/a | `TuneIntegration.aLuthierProfileMidiExportReimportsToTheSameAudio` | PARTIAL |
| TB-T9 (§15) | Hum fixture: 95 % pitch, rhythm on grid | `TuneHumCapture` | n/a | `HumCapture.aHummedMelodyIsTranscribedToTheSemitoneAndTheGrid` | DONE |
| TB-T10 (§15) | Standalone relaunch loads and plays the last tune | plugin state | n/a | `TuneIntegration.aRelaunchLoadsTheLastTuneAndPlaysIt` | DONE |

<!-- counts -->
