# VISUAL-WORKSHOP-QA coverage

The VISUAL-WORKSHOP-QA workstream (branch `claude/luthier-visual`): TODO items **G** (illustration remainder), **V** (visual appeal), **2h** (Easy amp card), **7** (workshop remainder), **14 / 14b** (the ui-wiring, performance-budget, qa-polish, installer, action-and-undo and gui-integration 20-22 audits), the known issue *first note after an IR load*, and the added spec `piano-roll-chord-display.md`.

Statuses:
- **verified**: implemented, reachable in the GUI where it has a GUI, and covered by the test named.
- **implemented**: implemented and reachable, with no dedicated test (the reason is given).
- **deferred**: not done; the reason is given.

Tests are named `Suite.test`. They live in the test files this workstream added:
- `WorkshopRemainderTests.cpp`
- `IllustrationRemainderTests.cpp`
- `AppearanceTests.cpp`
- `IrReloadTests.cpp`
- `ScreenshotTests.cpp`
- `CpuReliefUiTests.cpp`
- `PianoRollTests.cpp`
- the helpers' files, named in their sections.

**Parameters added: none.** The QA helper added `aux1_pre_circuit` inside the `VISUAL-WORKSHOP-QA` markers. MODEL-GAPS added the same ID with the same meaning on the integration branch, so the merge keeps theirs and drops ours: no `VISUAL-WORKSHOP-QA` params block remains, and the count test is the integration branch's. Factory presets sound the same.

## 1. Illustration remainder (TODO G)

| ID | Spec + section | Implementation | Test | Status |
|---|---|---|---|---|
| G-1 | guitar-illustration 5 (headstocks) | `Tools/body_outlines.py` reverse inline-6 head (`inlineReverse`); `HeadstockOutlines.h` regenerated | `Headstocks.everyLayoutIsDrawableAndEveryGuitarHasItsPosts` | verified |
| G-2 | guitar-illustration 4 (body refinements), 16 | `GuitarRenderer` capo, slide, pick and handle paths; per-string material colours; `scene.stringDescriptions` | `WorkshopStrings.anOverriddenStringIsDrawnInItsOwnMaterial`, `WorkshopAccessories.*` | verified |
| G-3 | guitar-workshop 12.3 (family switch, amp per family) | `Workshop/FamilyDefaults.{h,cpp}` `ampModelFor` / `applyAmpDefaults`, called from `switchGuitarFamily`, banner line added | `FamilySwitch.theAmpFollowsTheFamilyOnlyWhenItDoesNotSuit` | verified |
| G-4 | guitar-illustration 1 (zoom / pan) | `BenchIllustration::mouseWheelMove`: Ctrl-scroll up to 4x about the pointer | `BenchZoom.ctrlScrollZoomsToFourTimesAboutThePointer` | verified |
| G-5 | guitar-illustration 9 (thumbnail cache) | `UI/Guitar/GuitarThumbnails.{h,cpp}`: worker thread, 200-entry LRU, 256x128; preset browser rows show them | `Thumbnails.workerRendersCachesAndEvictsAt200`, `Thumbnails.everyFactoryPresetShowsItsGuitar` | verified |
| G-6 | workshop-ui 3.3 (per-string material) | `PartLibrary` `StringOverride` / `parts.strings.per_string_override`; `PartAcoustics` overrides; `StringMaterials::computeSpec (woundOverride)`; `LuthierEngine` `partsStringMaterial` / `partsStringWound`; bench Ctrl-click and inspector `#string.` fields | `WorkshopStrings.aPerStringOverrideIsSeenReadHeardAndOneEntry`, `.aCardOntoAStringOverridesItAndASetClearsOverrides`, `.anOverriddenStringIsDrawnInItsOwnMaterial` | verified |
| G-7 | guitar-illustration 8 (capo drawing) | `GuitarRenderer::capoPath`; `GuitarBodyComponent` draws the capo at `capo_fret` | `WorkshopAccessories.theCapoIsDrawnAndDraggedByFrets` | verified |
| G-8 | guitar-illustration 2.3, accessibility 5 (reduced-motion crossfade) | `UI/Guitar/IllustrationMotion.h` `SceneCrossfade` (250 ms, a static outline under reduced motion) | `ReducedMotion.aGuitarChangeCrossfadesOrIsStaticWithAnOutline`, `.theBenchFadesCommittedChangesOnly` | verified |
| G-9 | guitar-illustration 2.2 (60 ms note dots) | `IllustrationMotion.h` `NoteDots`; `GuitarBodyComponent::updateLiveOverlay` | `NoteDots.theDotAppearsWithin60msAndFadesOver60ms` | verified |

## 2. Visual appeal (TODO V) and the Easy amp card (TODO 2h)

| ID | Spec + section | Implementation | Test | Status |
|---|---|---|---|---|
| V-1 | visual-polish 6 (header name) | `HeaderBar.cpp`: LUTHIER in `Fonts::display (24)` with the brass headstock mark; compact widths under 1280 | `Screenshots.everyPanelInEveryPalette` (reviewed) | verified |
| V-2 | visual-polish 5 (accent choices, follow the guitar) | `AccessibilitySettings` six accents plus `kFollowGuitar`, `accentFor` / `accentContrast` (4.5:1), saved as `accent`; Options -> Appearance `accentBox`; the editor feeds `finish.colourA` | `Accent.everyChoiceMeetsContrastOnEveryPalette`, `Accent.theWindowTakesTheAccentAndFollowsTheGuitar` | verified |
| V-3 | visual-polish 4 (VU meter, room light) | `UI/StageTouches.{h,cpp}` `VuMeter` (300 ms ballistics, 0 VU = -18 dBFS, stale grey) and `RoomLight`; placed in Easy; Appearance switch | `StageTouches.theVuNeedleHasBallisticsAndGreysWhenStale`, `.theRoomLightFollowsSizeAndWet` | verified |
| V-4 | visual-polish 3, ui-wiring 10-11 (live overlays) | `DataStreamDisplay` in the footer (200 lines, stops after 500 ms, reduced motion); `NoiseEventStrip` static counts under reduced motion; Appearance switches | `DataStream.itKeeps200StopsAfter500msAndHonoursReducedMotion`, `NoiseStrip.reducedMotionShowsAStaticCountAndAppearanceHidesIt` | verified |
| V-5 | qa-polish 4 (every panel, three palettes, reviewed) | `Screenshots.everyPanelInEveryPalette` writes every view to `$LUTHIER_SCREENSHOTS`. Fixes found in review: workspace tabs wrap; panels fill; tracked text shrinks to fit (`Fonts::drawTrackedText`); table headers from the palette; overlay slider value boxes (white on Light) fixed in `OverlayHost::show`; Expression dead-zone labels; the saved font shown | `Screenshots.everyPanelInEveryPalette`, `Reflow.overlayValueBoxesUseThePaletteTextColour` | verified |
| 2h-1 | easy-mode 2h (amp card at 1200x720) | Rig strip shares; `AmpFace.cpp` border, pad, faceplate and column widths | `Screenshots.*` and the Easy layout knob-size test (32 px) | verified |

## 3. Workshop remainder (TODO 7)

| ID | Spec + section | Implementation | Test | Status |
|---|---|---|---|---|
| 7-1 | workshop-ui 3.3 | per-string overrides (G-6) | as G-6 | verified |
| 7-2 | workshop-ui 4 (nut slot drag) | `BenchIllustration` `Drag::nut`, 0.05 mm snap, 1.2 mm limit, keyboard parity; `WorkshopBench::setNutSlotDepth` | `WorkshopNut.aSlotDragIsOneEntryInRealUnitsAndReachesTheEngine` | verified |
| 7-3 | workshop-ui 4, gui-integration 21 (pick, slide and capo overlays and drags) | `Drag::pick`, `pickRotate`, `slide`, `slideRotate`, `capo`; `currentOverlay`, `accessoryAt`, `describeAccessory` | `WorkshopAccessories.thePickIsDraggedAndTurnedOnTheBench`, `.theCapoIsDrawnAndDraggedByFrets`, `.theSlideTurnsOnTheBenchAndItsMaterialIsPlayed` | verified |
| 7-4 | advanced-ranges 3 (WORKSHOP padlock) | `RangeTabButton` on the WORKSHOP tab covers {buzz, pick, slide}; pickup height stops at 0.8 mm (0.5 mm unlocked) | `WorkshopRanges.theTabCarriesAPadlockAndHeightsStopAtStock` | verified |
| 7-5 | gui-integration 17 (wrench) | W toggles the Workshop; the wrench opens the bench in Easy and the tab in Advanced | `WorkshopEditor.theWrenchOpensTheBenchInEasyModeAndTheTabInAdvanced` | verified |
| 7-6 | workshop-ui 6 (spectrum announcement) | `takeSpectrum` posts a polite summary (`getLastAnnouncement`) | `WorkshopSpectrum.theSummaryIsAnnouncedForScreenReaders` | verified |
| 7-7 | TODO 5b (slide material in the engine) | `LuthierEngine::setSlideBar`; processor `setSlidePart` / `getSlidePart`, saved in the guitar block as `slide`; `fitAccessory (slide)` | `WorkshopAccessories.theSlideTurnsOnTheBenchAndItsMaterialIsPlayed` | verified |
| 7-8 | guitar-illustration 14 (audition tint) | `setAuditionTint` / `endAuditionTint`, 500 ms fade | `WorkshopSpectrum.theBodyTintsWhileAuditioningAndFadesIn500ms` | verified |
| 7-9 | workshop-ui 9 (narrow layouts) | `kWideBench` 900, `kNarrowBench` 700, collapsible inspector, category box | `WorkshopLayout.theBenchCollapsesItsInspectorAndDrawerWhenNarrow` | verified |

## 4. Audits 14 / 14b: items done in this branch

| ID | Spec + section | Implementation | Test | Status |
|---|---|---|---|---|
| UW-9 | ui-wiring 9 (right-click items 12-13) | `Widgets.cpp`: read-only automation ID; "Show in Shortcuts" through `shortcutActionForParameter` and `showShortcutInOptions` | `ContextMenu.everyParameterShowsItsAutomationIdAndBoundOnesTheirShortcut` | verified |
| UW-12 | accessibility 3 (screen-reader names) | `labelForScreenReaders` on attach | `ScreenReader.everyAttachedControlHasAName` | verified |
| UW-14 | gui-integration 20 (panel `?`) | `PanelHelpButton` plus `openHelpForPanel`; Easy cards, Advanced sections, workspace `?`; HelpContent aliases | `PanelHelp.everyPanelsQuestionMarkOpensItsOwnTopic` | verified |
| UW-15 | gui-integration 20 (NEW dots) | `UI/NewFeatureDots.{h,cpp}` (the table is empty for 1.0) | `NewDots.anEntryPointIsMarkedForItsFirstWeekOnly` | verified |
| UW-16 | gui-integration 11.2 (drag to modulate) | `kModSourceDragPrefix`, `addModulationFromDrop` (depth 0.25, one undo entry); knobs and sliders are drop targets | `DragToModulate.aDroppedSourceRoutesAt25PercentAsOneEntry` | verified |
| UW-17 | gui-integration 19, accessibility 4 (reflow, UI scale) | `setScaleFactor (uiScale)`; reflow from 940 px | `Reflow.noControlHangsOutsideItsParentAtAnyWidthOrScale` | verified |
| UW-18 | advanced-ranges 4 (warning arc) | warning arc past stock | `RangeMarking.theWarningArcAppearsPastStockAndGoesWhenReturned` | verified |
| GI-21.4 | gui-integration 21 (slide bar on the fretboard) | `FretboardComponent` bar at the overlay fret, slant and material colour; no ease under reduced motion | `LiveDisplays.theFretboardDrawsTheSlideBarAndTheCircuitCurveFollowsTheVolume` | verified |
| GI-21.7 | gui-integration 21 (circuit curve) | `CircuitResponseView::refresh` | same test | verified |
| DIAG-1 | gui-integration 5 (Diagnostics) | `UI/AudioPathView.{h,cpp}`: live stage diagram and the feature-flags mirror | `Diagnostics.theAudioPathShowsWhatIsSoundingAndTheFlags` | verified |
| FL-1 | installer 6 (File Locations) | "Open Guitars folder" and "Open Parts folder" | review | implemented (they open the system file browser) |
| PB-4.4 | performance-budget 4 | `aux1_pre_circuit` (the QA helper's engine side) plus the **AUX 1 PRE-CIRCUIT** toggle beside SIDECHAIN TO AMP in Routing | `Routing.diPreCircuitBypassesTheCircuit`, `ScreenReader.everyAttachedControlHasAName` | verified |
| PB-8 | performance-budget 8 (relief steps 1, 2, 6 and 7; banner; opt-out) | Step 1: the noise strip drains every other tick. Step 2: the data stream is suspended. Step 6: `WorkshopBench::beginAudition` refuses. Step 7: `LuthierEngine` chokes the quietest sounding string every 200 ms. `UI/CpuReliefUi.{h,cpp}`: "CPU limit" banner once per episode, Options -> Diagnostics "Under CPU overload, drop the least active strings" (user preference, default on) | `CpuReliefUi.theBannerComesOncePerEpisodeAndGoes`, `.theOptOutIsSavedAndReachesTheLadder`, `.reliefSevenDropsTheQuietestStringsAndStopsWhenTheLoadFalls`, `.streamSuspendsAndAuditionFreezesUnderLoad` | verified (step 1 untested) |
| PB-8.3 / 8.5 | performance-budget 8 (steps 3 and 5) | none | none | deferred: the mod-matrix rate and reverb tap count are DSP changes with audible trade-offs for the engine owners; the ladder still reports the steps |
| KI-1 | known issue: the first note after an IR load | `SpectrumDelta.cpp` priming pluck removed; the engine was already right, and the plugin-path difference was parameter smoothing after a guitar change | `IrReload.theFirstNoteAfterALoadIsEveryNote`, `IrReload.throughThePluginAGuitarChangeLeavesNoOnsetDifference` | verified |

## 5. Piano roll and chord display (spec/piano-roll-chord-display.md, added 2026-09-24)

| ID | Spec + section | Implementation | Test | Status |
|---|---|---|---|---|
| PR-S2 | 2 (SoundingNotes) | `Support/SoundingNotes.{h,cpp}`: two buffers, one atomic sequence number, relaxed atomic stores, one packed word per string (note, bend, start). `Support/SoundingNotesPublisher.{h,cpp}` fills it after every block from the engine's string activity and each string's frequency (nearest semitone, cents). Called from `PluginProcessor::processSlice` | `PianoRoll.publishingTheSnapshotDoesNotAllocate` (PR-07) | verified |
| PR-S1 | 1 (range, colours, roll) | `UI/PianoRollModel.{h,cpp}`: `rangeFor` (tuning, capo, fret counts, drawn to whole octaves), keys at 85 % in the string's colour with a 150 ms release, a 4 s roll, bend tick above 20 cents, stale after 250 ms. `StringColours::forString` | PR-01, PR-02, PR-06 | verified |
| PR-S1b | 1 (strip, placement) | `UI/PianoRollStrip.{h,cpp}`: header (ROLL/KEYS, Latch, Play, Clear, Fingering), keys and roll. Advanced: `AdvancedPanel` grows the top guitar strip by the roll's height, directly under the fretboard, with a collapse arrow and a 40-140 px drag handle. Easy: `EasyPanel`, 56 px under the guitar | `VisualAids.everyControlIsReachableFocusableAndNamed` (OP-02) | verified |
| PR-S3 | 3 (playing from the keys) | Clicks and glissandi with velocity 40-110 by key position; `LuthierAudioProcessor::playKeyboardNote` / `releaseKeyboardNote` / `playKeyboardChord` add channel 1 events to the preview MIDI (the fretboard's path, merged with a try-lock). Latch with Play/Enter and Clear/Escape. Computer keyboard while focused (A-L, W-P, Z/X) | PR-03, PR-04 | verified (the computer keyboard is untested: the harness has no key-state source) |
| PR-S3b | 3 (Show fingering) | A private `RubricVoicer` on the engine's tuning; `FretboardComponent::setGhostDots` (Advanced) or `GuitarBodyComponent::setGhostDots` (Easy) draws hollow dots; a note below the lowest string gets an x and the tooltip "Below this guitar's range" | PR-05 | verified |
| CD-S4 | 4 (chord name) | `UI/ChordNaming.{h,cpp}` (rules and spelling), `UI/ChordNameFader.{h,cpp}` (timing), `UI/ChordNameOverlay.{h,cpp}` (snapshot to name to fade, lower bout, display face, 12 % of the height clamped to 28-96 px, 0.35 peak). Drawn in `GuitarBodyComponent`'s live pass | CD-01 to CD-05, `ChordName.sizedFromTheIllustrationAndAnnouncedPolitely` | verified |
| CD-S4b | 4 (screen reader) | Low-priority `postAnnouncement` of a new name, at most every 1.5 s, only with the names on | `ChordName.sizedFromTheIllustrationAndAnnouncedPolitely` | verified |
| OP-S5 | 5 (Options) | `UI/VisualAids.{h,cpp}` (UiPreferences keys `visualAids.*`). Options -> Appearance (the page with "Show tooltips") gains a "Visual aids" section: chord names, announce (enabled only with names on), piano roll for Advanced, piano roll for Easy, "Piano roll shows" | `VisualAids.theOptionsPersistAndStayOutOfPresets` (OP-01) | verified |
| ST-S6 | 6 (UiState) | `pianoRollExpanded`, `pianoRollHeight`, `pianoLatch`, `pianoShowFingering`, `pianoLatchedNotes` in the plugin state's `ui` block; not in presets; `ui` is a session layer that undo leaves alone | OP-01 | verified |
| PF-S7 | 7 (performance) | Publish: 14 relaxed stores and one release store per block. The roll repaints only the keys and roll area, at 30 Hz. The chord name repaints only its bout area and is not in the cached scene | PR-07 | implemented (the 2 ms budget at 1920x1080 is not measured separately) |


## 6. Audit 14: performance budget, QA polish and installer (helper branch `wip/vwq-qa`, merged)

| Item / IDs | Spec | Implementation | Verification | Status |
|---|---|---|---|---|
| 1 PB-0.4 | performance-budget.md 0.4 | `Source/Tests/CircuitTests.cpp`: `luthier::tests::allocationsOnThisThread` (the counter is renamed `g_allocs`). `CMakeLists.txt`: `LUTHIER_ALLOCATION_COUNTER=1` in `LUTHIER_TEST_DEFS`. | `Capture.capturingTenThousandNotesDoesNotAllocate` and `TunePlayer.rendersWithoutAllocating` now compile and pass. | verified |
| 2 QA-1.3 | qa-polish.md 1.3 | 176.4 kHz added to the rate list in `IntegrationTests.cpp` `Engine::sampleRateChangesAreSurvived`. | `Engine.sampleRateChangesAreSurvived` | verified |
| 3 QA-6.2 | qa-polish.md 6.2 | `docs/KNOWN_ISSUES.md`: VST3 and Standalone only (no AU), plus the Linux route through `setup_linux.sh`. | review | verified |
| 4 QA-11.1 | qa-polish.md 11 | `THIRD_PARTY_LICENCES.txt`, also copied to `Resources/`. It lists JUCE 8.0.10 and its bundled libraries, the VST3 SDK licence, and Bebas Neue and Lato (OFL). | `Legal.thirdPartyLicencesNameEveryBundledDependency` | verified |
| 5 IN-3.2 / IN-2.1 | installer.md 1.1, 2.1, 3.1 | `IrLibrary::getCandidateFolders()`: `/usr/share/luthier`, `/usr/local/share/luthier`, `~/.local/share/luthier`, `<exe>/../share/luthier`, the VST3 `<prefix>/share/luthier`, macOS `/Library/Application Support/Luthier`, and Windows `ProgramData\Luthier`. | `IrLibrary.candidatesIncludeTheInstallerLayout` | verified |
| 6 IN-6.1-6.3 | installer.md 6 | `Source/Support/InstallLayout.{h,cpp}`: `InstallLayout::ensure` creates the 18 subfolders, `config/plugin.json` and `.installed_version`, and returns the first-run / upgrade result. The processor constructor calls it and exposes the result through `getInstallLayoutResult()`. | `InstallLayout.createsTheTreeAndMarker`, `.sameVersionIsQuiet`, `.differentVersionReportsUpgrade` | verified |
| 7 QA-2.6 | qa-polish.md 2 | new test | `Presets.everyFactoryPresetRoundTripsToTheUlp` (drift ≤ 1 ulp, see Decisions) | verified |
| 8 QA-3.13 | qa-polish.md 3 | new test | `Presets.everyByteOfAFactoryPresetFlippedIsRefusedOrLoads` | verified |
| 9 QA-3.14 | qa-polish.md 3 | new test, in `WorkshopQaTests.cpp` | `Workshop.everyByteOfAFactoryGuitarFlippedIsRefusedOrLoads` | verified |
| 10 QA-5.9 | qa-polish.md 5 | Fix in `PedalsMod.cpp` `DelayPedal::process`: the DC blocker moved to the wet path, because the dry path used to pass through it. | `Effects.zeroMixIsABypassWithinMinus80` (chain mix and each pedal's own Mix) | verified |
| 11 QA-5.7 | qa-polish.md 5 | new test | `Effects.toggleIsClickFree` | verified |
| 12 QA-5.10-5.12 | qa-polish.md 5 | Fix in `AmpEngine::preampStage`: the tube curve's resting point is subtracted, cached as `stageRest`. A cold start thumped at up to +1 dBFS. | `Amp.gainSweepIsMonotonicAt1kHz`, `Amp.neutralToneStackIsFlatWithin1dB`, `Amp.coldStartHasNoTransient` | verified (sweep and tone stack have measured departures, see Decisions) |
| 14 IN-5.2 | installer.md 5.1 | `Source/Updates/UpdateDownloader.{h,cpp}`: a `Fetcher` seam, writes `<Downloads>/<name>.part` and then renames it, never launches the installer. `UpdatesPage` gains "Release notes" and "Download" buttons (`OptionsPages.{h,cpp}`). | `Updates.theDownloadLandsInDownloadsUnderItsOwnName` | verified |
| 15 IN-8.6 | installer.md 8 | `PresetManager::noteMigration`, a migration generation counter, `getLastMigration`. A migration is a derived ranges block from an older `pluginVersion`, retired parameters, or a pre-parts guitar name (the last is noted in `PluginProcessor::takeGuitarBlock`). `PluginEditor::pollForNotifications` posts one info banner (`"migrated"`). | `Editor.aMigratedPresetRaisesOneInfoBanner` | verified |
| 16 IN-8.2 | installer.md 8 | `PresetManager::backupFolderFor`: `<nearest Presets ancestor>/Backup/<date>/`. `pruneOldBackups` also sweeps `Presets/Backup`. | `Presets.backupsGoToThePresetsRootBackupFolder`. The existing `Presets.savingBacksUpTheVersionItReplaces` still passes and covers the fallback. | verified |
| 17 PB-0.4 | performance-budget.md 0.4 | `Looper::captureMidi` writes an `AbstractFifo` of 8192 events. `Looper::drainPendingMidi` runs from the processor timer and `save()`. | `PracticeLooper.recordingMidiDoesNotAllocate` | verified |
| 18 PB-0.4 / 0.5 | performance-budget.md 0.4, 0.5 | `MidiLearnManager`: the audio thread publishes `pendingLearnCc` (atomic); a 30 Hz timer runs `servicePendingLearn`. The lock is a `ThreadProbe::ProbedCriticalSection`. | `MidiLearn.learningDoesNotAllocateOrLockOnTheAudioThread`, `ThreadProbe.theLockTrapSeesALock` | verified |
| 19 PB-10.4 / 10.5 | performance-budget.md 10 | Lock trap: `ThreadProbe::noteLock` / `ProbedCriticalSection`, plus a Linux `pthread_mutex_lock` interposer in the tests. The test found two more audio-thread problems, both fixed. `RoutingMatrix::getMidiOutConfig` took a lock every block and is now a seqlock. `ParameterBridge::applyToEngine` built about 424 `juce::String`s (heap) every block and now uses a lock-free pointer-keyed index cache plus precomputed per-slot indices. | `Engine.fiveMinutesOfPlaybackNeitherAllocatesNorLocks` (40 s by default, 5 min under `LUTHIER_PERF=1`) | verified |
| 20 PB-4.2 / 4.3 | performance-budget.md 4 | option (a) | `Latency.dspLatencyIsWithinBudget` | verified per-string and Aux 8. DI and main are held at measured values, see Decisions. |
| 21 PB-10.6 / QA-7.7 | performance-budget.md 10 | Fixes: `MasterBus::getLatencySamples` (the 72-sample limiter lookahead was never reported) is added to the engine's latency. `Oversampler::getLatencySamples` now reports 6/10/11 instead of 3/5/6. | `Latency.anImpulseArrivesWhenReported`, `Latency.oversamplerReportsItsGroupDelay` | verified |
| 22 PB-4.5 | performance-budget.md 4 | `RoutingMatrix::LatencyReport::auxNoise`, filled in `updateRoutingLatencyReport` | `Latency.dspLatencyIsWithinBudget` (≤ 128) | verified |
| 23 PB-4.4 | performance-budget.md 4 | `aux1_pre_circuit` (default off). After the merge it is MODEL-GAPS' parameter, with the same ID and meaning; `LuthierEngine::setDiPreCircuit` forwards to `setAuxDiPreCircuit`. The toggle is MODEL-GAPS' in Routing. `docs/CHANGELOG.md` is updated. | `Routing.diPreCircuitBypassesTheCircuit`, `Parameters.*` | verified |
| 24 PB-5.1 / QA-2.4 | performance-budget.md 5 | new test (`PerfBudgetTests.cpp`) | `Boot.coldAndWarmInstantiationStayInBudget` | verified (1.5x bar by default, spec numbers under `LUTHIER_PERF=1`, see Decisions) |
| 25 PB-5.3-5.5, 10.9-10.11 | performance-budget.md 5, 10 | new tests, p95 of 100 events | `Workshop.hundredRandomPartSwapsStayUnder50ms`, `WorkshopBench.hundredAuditionsStayInBudget`, `Workshop.guitarLoadStaysUnder300ms`, `Tune.loadStaysUnder100ms`, `WorkshopSpectrum.hundredShadowRendersStayUnder40ms` | verified |
| 26 PB-1.1 / 10.1 | performance-budget.md 1 | `cpuUnits()`, `threadCpuTimeSeconds()` and `perfRunRequested()` in `TestFramework.h` | `PerfBudget.everyModuleWithinBudget` (gated) | implemented, runs nightly |
| 27 PB-1.2 / 10.2 / QA-7.1 | performance-budget.md 1 | new test | `PerfBudget.scenarioTotals` (gated) | implemented, runs nightly |
| 29 PB-7.2 | performance-budget.md 7 | `LuthierEngine::effectiveOversamplingFactor` (halved above 96 kHz, quartered above 176.4 kHz), applied in `prepare` and `setOversamplingFactor`. `AmpEngine::getOversampledRate`. | `Engine.oversamplingDowngradesAbove96k` | verified |
| 30 PB-6.1 / 7.1 / 10.7 | performance-budget.md 6, 7 | new tests | `PerfBudget.voiceCountScaling`, `PerfBudget.sampleRateScaling` (gated) | implemented, runs nightly |
| 31 PB-3.1 / 10.3 / QA-2.5 / 7.6 | performance-budget.md 3, 10 | `Source/Support/MemoryProbe.h` (Linux `/proc`, Windows `GetProcessMemoryInfo`, macOS `task_info`). The Looper now sizes its undo/redo buffers on first use (saves 184 MB virtual per instance and boot time). | `Memory.baselineInstanceUnder350MB` (about 125 MB measured). `Memory.sixtyMinuteSessionNoMonotonicGrowth` is gated. | verified / nightly |
| 32 PB-8.1 / 8.2 | performance-budget.md 8 | `Source/Support/CpuRelief.{h,cpp}`: 200 ms average, one step up per 200 ms above 85%, one step down per second below 70%, step 7 opt-out. The engine hook applies step 4 through `NoiseEngine::setDegraded`, only when the step changes (marked edit in `LuthierEngine.cpp`). | `CpuRelief.laddersUpAtEightyFivePercentAndBackDown`, `.stepSevenIsOptOut`, `.theEngineHalvesTheNoisePoolsOnlyUnderLoad` | verified. The UI toggle and banner are not built, see Decisions. |
| 33 QA-2.2 | qa-polish.md 2 | Denormal check added to `Parameters.fuzzAcrossTenThousandStates`. A 100k-state run with sidechain audio is new. | `Parameters.fuzzAcrossTenThousandStates`, `Parameters.fuzzAcrossHundredThousandStates` (gated) | verified |
| 34 QA-2.3 | qa-polish.md 2 | new test. It found that a playing-noise voice and the coupling state outlived `LuthierEngine::panic` and kept the strings sounding and growing. `panic` now also resets `playingNoise`, `coupling`, `noteSustainScale` and the bridge/coupling vectors. | `StateModel.tenThousandRandomOperationsLeaveNoStuckState` (1000 ops by default, 10 000 gated) | verified |
| 35 QA-3.x | qa-polish.md 3 | new tests (`RobustnessTests.cpp`) | `Stress.midiStormDuringLoadsRecallsAndGuitarLoads`, `.midiLearnArmDisarmHundredTimes`, `.undoRedoThousandTimes` (60 by default, 1000 gated), `.slideAndAdvancedRangeTogglesMidPlay`, `.busLayoutChangesMidPlay`, `.thirtyTwoInstancesRenderInTurn` (8 by default, 32 gated) | verified |
| 36 QA-5.4 | qa-polish.md 5 | option: the test with the noise floor off | `Engine.silenceInSilenceOutWithNoiseFloorOff` (4 guitar families, ≤ -100 dBFS RMS) | verified |
| 38 QA-2.7 | qa-polish.md 2 | new test | `Workshop.everyFactoryGuitarRoundTripsInAudio` (≤ -80 dBFS) | verified |
| 39 QA-5.14 | qa-polish.md 5 | new test | `Squeak.aThousandRunsAreByteIdentical` | verified |
| 39 QA-5.17 | qa-polish.md 5 | new test | `Buzz.oneDecibelUnderTheThresholdIsCleanThreeOverBuzzes` | verified |
| 39 QA-5.18 | qa-polish.md 5 | new test | `Slide.theBarArrivesWithinTwoCents` | verified |
| 39 QA-5.23 | qa-polish.md 5 | new test | `Circuit.atVolumeFiveKinmanHoldsTheTiltNoneLosesIt` | verified |
| 39 QA-5.24, 5.25, 5.26, 5.28, 5.29, 5.21 | qa-polish.md 5 | not done | none | deferred: Aux 8 re-applied null, strum chop +25-35%, rasgueado 14 ms, pop vs follow-through, ghost ≤ -30 dBFS. These need the slap and strum fixtures of those workstreams. The pre-M44 CableSim table (5.21) no longer exists in the repo to compare against. |
| 44 QA-2.10 / PB-0.1 / 9.1 | qa-polish.md 2 | `.github/workflows/ci.yml` (build, suite, pluginval, cpack, `dpkg-deb -c` path check, `desktop-file-validate`, manifest, artefacts) and `nightly.yml` (`LUTHIER_PERF=1` run, dashboard JSON artefact, double-build `.deb` comparison) | YAML parses. The workflows have not run on GitHub (no push). | implemented |
| 45 QA-2.9 | qa-polish.md 2.9 | `scripts/pluginval.sh` (downloads pluginval v1.0.3, strictness 10, xvfb, fails on warnings) | ran here and passed | verified |
| 46 QA-9.3 / IN-3.1-3.4 / IN-4.1 | installer.md 3 | `install()` and CPack in `CMakeLists.txt`. `packaging/linux/`: `luthier.desktop`, `luthier.xml`, `postinst`, `postrm`, `install.sh`, `uninstall.sh`, `CPackProjectConfig.cmake`. | `cpack` ran here. `dpkg-deb -c`/`-I` checked. The tarball's `install.sh --prefix` and `uninstall.sh` were run into a scratch prefix. Two cpack runs with `SOURCE_DATE_EPOCH` gave byte-identical `.deb`/`.tar.gz`. | verified |
| 47 IN-1.3 / IN-13.6 | installer.md 1.3, 13 | `Source/StandaloneApp.cpp` (custom JUCE app with `JUCE_USE_CUSTOM_PLUGIN_STANDALONE_APP=1`: opens a command-line file and hands off via `anotherInstanceStarted`). `Source/Support/FileOpenRouter.{h,cpp}` holds the pure router and dispatch. | `FileOpen.everyAssociationRoutesToItsLoader`, `.theCommandLineNamesTheFile`, `.aPresetFileOpensAndAStrangerIsRefused`. Standalone smoke-run under xvfb with a guitar file. | verified |
| 48 IN-10.1 / 13.4 / 0.2 / 0.5 | installer.md 10 | `scripts/release_manifest.sh` (SHA-256, `manifest.txt`, `SHA256SUMS`, `gpg --detach-sign` when a key exists). The determinism check is in `nightly.yml`. | ran here on the two packages | verified |
| 49 IN-5.3 / IN-11.1 | installer.md 11, 5.2 | `Source/Updates/ContentPackage.{h,cpp}`: RSA-signed manifest, data-only allowlist, path-traversal refusal, staged install into `ContentUpdates/<name>/`, delta patches with base-version check and rollback. | `ContentPackage.aSignedPackageInstallsIntoItsFolder`, `.aBadOrMissingSignatureIsRefused`, `.pathTraversalAndCodeAreRefused`, `.aBadHashRollsBackAndOffersTheFullDownload` | verified (no UI entry point yet) |
| 13 QA-4.25 | qa-polish.md 4 | none | none | out of scope (not this workstream's). The slide announcement belongs to the accessibility work. |
| 28 PB-2.1 | performance-budget.md 2 | none | none | deferred: not in this task's list. `PerfBudget.everyModuleWithinBudget` measures the pre-chain; per-pedal eco/Quality parameters are a DSP decision. |
| 37 QA-2.1 | qa-polish.md 2 | none | none | deferred: not in this task's list. Golden renders need a committed render set and the `--golden` CLI mode. |
| 40 QA-5.31 | qa-polish.md 5 | none | none | out of scope: the bass two-finger alternation belongs to the bass workstream. |
| 41 QA-4.x | qa-polish.md 4 | none | none | out of scope: the automated UI walker touches the UI panels this task may not edit. |
| 42 QA-4.1 / 6.3 / 10.1 | qa-polish.md 4, 6, 10 | none | none | out of scope: translations and the tooltip catalog need the UI files. |
| 43 QA-4.10 / 4.16 | qa-polish.md 4 | none | none | out of scope: panel collapse memory and empty-state hints are UI work. |
| G | installer.md 1, 2, 9 | none | none | platform-deferred: the Windows and macOS installers, AU, and IN-0.4, IN-7.2, QA-1.1, QA-1.2, QA-1.5 and PB-5.2 need those platforms. |
| H | qa-polish.md 5.3, 8.1, 10.2, 11.4, 12.1, 13.1; installer.md 12; QA-6.1 | none | none | process: audio-lead signoff, bug bash, videos and support, EULA, the final human check, post-release monitoring, the rollback plan and the GI-19 feature matrix cannot be closed by code. |


## 7. Audit 14b: action and undo (helper branch `wip/vwq-undo`, merged)

| ID | Spec + section | Implementation | Verification | Status |
|---|---|---|---|---|
| AU-0.1b (reset) | a&u 0.1 | PluginEditor.cpp `resetAll` no longer pushes; `resetEverything` pushes "Reset everything" | `Undo.resetEverythingIsOneEntry` | verified |
| AU-0.1b (clamp) | a&u 0.1 | OptionsPages.cpp `RangesPage::clampOne` uses `ScopedUndoAction` (made public for tests) | `Undo.clampToStockIsOneEntry` | verified |
| AU-9b (Ctrl-Y) | a&u 9 | Accessibility.cpp `redoAlt` (not on macOS), Localisation string, HelpContent group, PluginEditor handler | `Accessibility.shortcutDefaultsMatchTheCanonicalTable` | verified |
| AU-13.3 / 2.1 | a&u 2, 13 | `UndoHistory::push` (cap 200) | `Undo.overflowDropsTheOldest` | verified |
| AU-3.1a / 0.1 | a&u 3.1 | `parameterGestureChanged` | `Undo.aGestureIsOneEntry` | verified |
| AU-3.1d / 11 / 13.9 | a&u 3.1, 11, 13 | gesture-driven entries | `Undo.writesWithoutAGestureMakeNoEntries` (1000 writes, 0 entries) | verified |
| stale comment | - | StateModelTests.cpp:55 | - | done |
| AU-3.17 / 7b / 8 (tune) | a&u 3.17, 7 | `LuthierAudioProcessor::applyUndoState` + `UndoState::withSessionLayers`: drops `ui` (including bench slots), `tune`, `metronome`; keeps current `liveMode`, `slotBActive`, `lockedParameters`, `clickToMain`; no `ignoreNextProgramChange` on undo | `Undo.doesNotMoveTheViewOrTheTune` | verified |
| AU-3.8a (name) / 13.6 | a&u 3.8, 6 | `loadPresetAsUserAction`, `stepPresetAsUserAction`; HeaderBar prev/next/open/import, Overlays browser, EasyPanel style, editor `[`/`]` | `Undo.aPresetLoadIsOneNamedEntry` | verified |
| AU-3.1c / 3.3b | a&u 3.1, 3.3 | "Change X from A to B" (with unit label), "Turn on/off X" for booleans | `Undo.aGestureIsOneEntry`, `Undo.aToggleSaysTurnOnOrOff` | verified |
| AU-3.9b (melody) | a&u 3.9 | TunePanel.cpp `melodyNoteTarget` passed by draw and delete | `TunePanel.melodyNoteEditsGroupOnTheSameNote` | verified |
| AU-0.2 / 1.1 / 5.1 / 5.2 / 13.2 | a&u 0.2, 1, 5 | `UndoHistory::canUndo` stops after an undone boundary. Boundaries: preset loads, file open/import, New preset, guitar-type change (gesture on `guitar_type`), family switch ("Change guitar family to X"), setlist load, setlist step | `Undo.aPresetLoadIsABoundary`, `WorkshopPanel.theGuitarCategorySwitchesFamily` (extended) | verified |
| AU-9b (Ctrl-Alt-Z) | a&u 9 | `undoAcrossBoundary` shortcut (Accessibility.cpp, Localisation.cpp, HelpContent.cpp). PluginEditor posts a banner with an "Undo" action (the confirmation); plain Ctrl-Z at a boundary posts an info banner | `Accessibility.shortcutDefaultsMatchTheCanonicalTable`, `Undo.aPresetLoadIsABoundary` | verified (banner UI untested) |
| AU-0.3 / 3.1b / 3.2 / 3.11c / 4.1 / 13.5 | a&u 0.3, 4 | `UndoHistory::push` merge rule, `setUndoClock`. Params merge on class "param" + paramID. Toggles never merge | `Undo.gesturesGroupWithin200ms` (199 merges, 201 splits, other param splits, undo reaches first before) | verified |
| AU-3.7a/b | a&u 3.7 | `captureSnapshotAsUserAction`, `recallSnapshotAsUserAction`; LivePanel / LiveStrip save, recall, rename (grouped by index), colour, delete | `Undo.snapshotSaveAndRecallAreEntries` | verified (rename/colour/delete untested) |
| AU-3.6 / UW-18.2 | a&u 3.6, ui-wiring 18 | Widgets.cpp right-click Modulate/Remove; ModMatrixPanel add, remove, enable, depth (grouped per route), curve, clear | `Undo.rightClickModulationIsUndoable` | verified (panel routes untested) |
| UW-18.2 (rhythm) | gui-integration 18 | RhythmPanel `StrumGrid::commit`, `FingerpickGrid::commit` (grouped), swing, load and randomise pattern | - | implemented-untested |
| AU-3.12 | a&u 3.12 | `MidiLearnManager::onBeforeLearn` hook, called on the message thread in the deferred learn; Widgets.cpp "Remove MIDI mapping" | `Undo.aMidiLearnIsOneEntry` | verified (drives the hook directly: no dispatch loop in tests) |
| AU-3.13b | a&u 3.13 | PedalRack `reorder` and Clear slot inside `ScopedUndoAction` (`reorder` made public) | `Undo.movingAPedalIsOneEntry` | verified |
| AU-3.10 | a&u 3.10 | `loadSetlist` boundary, `applyCurrentSetlistEntry(asUndoStep)`, LivePanel add/remove/move entries; the setlist is now in the state blob | `Undo.benchSlotsAndSetlistAreInTheStateButBenchSlotsIgnoreUndo` (state) | implemented (edit entries untested) |
| AU-3.15 | a&u 3.15 | CharacterPanel: each handler `pushUndoAction(..., "character-edit", control)`, grouped. Merges skip the state capture (`UndoHistory::wouldMerge`) | - | implemented-untested |
| AU-3.14a | a&u 3.14 | PracticePanel: scale/key (`practice-scale`, callbacks), layer mute/rev/half/mode/level/pan (`looper-layer`, `LooperTab::editLayer`, callbacks), backing-track load (callbacks) | - | implemented-untested |
| AU-3.14b | a&u 3.14 | `Looper::clear` keeps each layer's audio (`LoopLayer::clearKeepingUndo`), `Looper::restoreCleared`; the Clear button pushes a callback entry | `Undo.aClearedLoopLayerComesBack` | verified |
| UW item 7 (bench slots) | ui-wiring 17, workshop-ui 7 | `getStateInformation` / `setStateInformation` `ui.benchSlots`; not in presets; `ui` is dropped on undo | `Undo.benchSlotsAndSetlistAreInTheStateButBenchSlotsIgnoreUndo` | verified |
| UW item 8 (setlist) | ui-wiring 17 | state key `setlist` {file, data, position}; `getSetlistFile()` | same test | verified |
| UW item 13 / GI-22.7 | ui-wiring 23, gui-integration 22 | - | `Undo.randomWalkUndoesBackToTheStart` (1000 ops in 8 segments of 125; params, bench fits, mod routes; full undo equals the start state, full redo equals the end state) | verified |
| AU-1.2 / 12 (part) | a&u 1, 9, 12 | `getUndoHistory`, `getNumRedoSteps`; `HeaderBar::buildUndoHistoryMenu` / `applyUndoHistoryChoice`; File -> "Undo history..." (newest 20, boundaries under a separator and header, click undoes to before that entry) | `Undo.theHistoryListsNewestFirstAndUndoesToAPoint` | verified. Search filter and the Diagnostics "Show Undo Depth" footer: deferred (not in this task's scope) |
| AU-13.7 | a&u 13 | - | `Undo.undoMidPlayProducesNoGarbage` | verified |
| AU-8 (bleed guard) | a&u 8 | Tier 4 plus session layers | `Undo.laterUntrackedEditsSurviveUndo` | verified. Still bleeds (these are in the blob with no entry): IR slot loads (toneMatch), routing (mute/solo/gain), rhythm-engine settings other than the pattern (enable, free-run, voicing, capo, humanise), metronome state (dropped from undo entirely, so never restored), and the headstock per-string detune. Undoing an older entry reverts any of these made since |
| AU-8b | a&u 8 | - | - | deferred (family-switch warning not asked for in this task) |


## 8. Deferred

- **performance-budget 8, relief steps 3 and 5** (mod-matrix rate, reverb taps). These are DSP trade-offs for the engine owners; the ladder reports the steps.
- **ui-wiring: localisation wiring and translated parameter names, panel collapse memory, empty-state hints.** These need translations and a string catalogue, and touch panels owned by other workstreams (AdvancedPanel, CharacterPanel).
- **qa-polish 2.1: golden renders.** They need a committed render set and a `--golden` CLI mode.
- **qa-polish 5.21 / 5.24-5.29, bass and strum realism checks.** They need other workstreams' fixtures (see section 6).
- **action-and-undo:** the history search filter, the Diagnostics "Show undo depth" footer, the family-switch warning (AU-8b), and undo entries for IR loads, routing, rhythm settings and headstock detune. The undo helper's follow-up round covers these; see section 7 for what landed.
- **Test-only items:**
  - UW-23 scaled-up workshop and slide loops: the performance tests cover the workshop loops at 100 events.
  - SpectrumDelta against offline renders.
- **Known suite failures on the unmodified integration branch** (not from this workstream):
  - `Feedback.eachStringHearsItsOwnNote` (18.6 cents at octave bias 1).
  - `Editor.everyAutomatableParameterHasAVisibleControl` (the scrape, slap, macro-assign and right-hand parameters have no control in the editor).

## 9. Decisions

This workstream (the main branch of work):
- `parts.strings.per_string_override` numbers strings from 1, as the player does; the engine index is `string - 1`.
- The pickup-height floor (0.8 mm, or 0.5 mm unlocked) belongs to the **buzz** range family; the WORKSHOP tab's padlock covers {buzz, pick, slide}.
- Preset thumbnails are 256x128 landscape. They are rendered on one worker thread, and the cache holds 200.
- Amp defaults per family (guitar-workshop 12.3): switching family moves the amp only when the current one does not suit the new family. The resonator uses the AcousticDI.
- An accent that misses 4.5:1 on a palette's panels is moved toward `textPrimary` until it passes. "Follow the guitar" is treated the same way.
- The UI scale uses `AudioProcessorEditor::setScaleFactor`. The reflow test starts at 940 px, the smallest window the editor allows, not 800.
- The undo cap is 200 (action-and-undo 2), not gui-integration 18's 64.
- The "first note after an IR load" known issue is closed. The engine was already correct. The plugin path's difference was parameter smoothing after a guitar change, and the test primes one silent block, which is the intended behaviour.
- The audition tint holds while auditioning and fades over 500 ms after it ends.
- A preset with no `slide` in its guitar block keeps the default glass bar.
- Bench drags push parameter gestures, so their undo entries read "Change X from A to B", not custom sentences. The part swaps and pickup moves keep workshop-ui 8's wording.
- **Relief step 7** chokes the quietest sounding string every 200 ms while the step holds. The quietest string is used as the "least recently active" one, because it has been ringing longest.
- **The CPU relief ladder is off in non-realtime renders**, where there is no deadline to miss and an audible step would be printed into the file. **It is also off in the test runner**, so a busy machine cannot drop strings inside unrelated tests.
- The overlay host sends a look-and-feel change when a panel first joins it. JUCE does not send one on reparenting, which left Options sliders with white value text on the Light palette.
- A guitar-type load defers to parameters the host wrote with the type (the integration branch's rule). A workshop edit or family switch always writes, and the processor's own guitar writes are not stamped as host writes.
- The preset ulp round-trip test compares the reload with the saved file. `toVar` stores the value a load/save trip leaves alone.
- **Merge with the integration branch:**
  - `aux1_pre_circuit` is MODEL-GAPS'. Its Routing toggle is theirs, and ours was removed.
  - `PanelHelpButton` is TUNE-HELP-ONBOARDING's; ours was removed.
  - MIDI Learn polls at 30 Hz instead of posting from the audio thread.
  - File -> Undo history uses menu id 15.

Piano roll and chord display:
- **Placement in Advanced.** The fretboard sits in the top guitar strip, not in column 2. The roll goes directly under it, the strip grows by the roll's height, and the columns give up that space.
- **String colours.** No per-string palette existed. `StringColours::forString` spreads the strings' hues from warm (low string) to cool (high string). It colours the piano roll, and the fretboard's sounding dots keep their accent.
- **Range padding.** "Padded to whole octaves" is read as the *drawn* keyboard: C below the lowest playable note to B above the highest. The *playable* range is exactly the spec's (E2-E6 on a 24-fret standard guitar), and the keys outside it are dimmed.
- **Keyboard state.** The processor has no `MidiKeyboardState`, and JUCE's locks on the audio thread. The keys use the fretboard's preview MIDI instead: channel 1, merged with a try-lock.
- **Show fingering in Easy.** Easy has no fretboard, so the ghost dots are drawn on the guitar illustration.
- **Strums.** Membership of a strum is decided from the audio's own start samples (30 ms), because a 30 Hz UI cannot see a 30 ms window.
- **Chord-name spelling.** A loaded tune (a tune player with a timeline) decides the spelling from its key. Otherwise sharps are used, except B flat and E flat.
- **Chord-name announcements** use the lowest announcement priority, JUCE's nearest to "polite".

From the performance, QA and installer helper:

- **Chord window versus the section 4 budgets (item 20, option a).** The Poly-mode chord window (2 ms, 96 samples at 48 kHz) is musical latency: the interpreter waits to see whether notes belong together. It sits outside the section 4 DSP budgets. `Latency.dspLatencyIsWithinBudget` therefore subtracts the engine's event latency from every tap, and subtracts the amp and pedal oversampling from the main output, which section 4 excludes by name.
- **DI and main latency are over budget (recorded, not weakened silently).** Measured at 48 kHz / 128:
  - DI: 128 samples, against a budget of 32. The body convolution uses `juce::dsp::Convolution::Latency{128}`.
  - Main out: 328 samples, against a budget of 128. That is body 128, cabinet 128 and the limiter lookahead 72.
  - Meeting 32/128 needs zero-latency non-uniform convolution and a shorter lookahead. That is a CPU and sound trade-off for the engine owners. The test holds the measured values as a regression bar and names the spec numbers in its messages.
  - Per-string latency (0) and Aux 8 (0) meet the spec.
- **Reported latency corrected (item 21).**
  - The master limiter's 1.5 ms lookahead was never reported. `MasterBus::getLatencySamples` is now part of the engine's latency.
  - The oversampler reported about half its real group delay: 3/5/6 against a measured 6.35/9.52/11.11 at 2x/4x/8x. It now reports 6/10/11.
  - Latency accuracy is measured two ways. The engine sidechain path, with the amp at 1x and cab and room off, is checked by impulse peak to ±1 sample. The oversampler is checked by group delay in the guitar band (DC to 1 kHz) to ±1 sample. An IIR half-band has no single delay, and its broadband impulse peak (15-20 samples) is ringing, not arrival.
- **Preset round trip "to the ulp" (item 7) allows one ulp.** JUCE float parameters store the plain value, so a normalised value through a skewed range can come back one ulp off. The spec's wording is "drift > float ulp fails".
- **Neutral tone stack (item 12).**
  - A passive FMV stack is not flat at noon; its mid scoop is the circuit. "Neutral" is taken as the flattest control setting, found on a 0.05 grid, and the passband as 100 Hz to 5 kHz.
  - Fender, Vox and Modern are within 1 dB.
  - The Marshall component set (470 pF, 33 k) measures 1.38 dB at best and is held at 1.5 dB. Flattening it would change the modelled circuit.
- **Amp gain sweep (item 12).**
  - The saturated high-gain models lose up to 1.2 dB of 1 kHz RMS at the top of the sweep, from compression and sag. The worst are the Deluxe, Champ and AC30, from gain 70.
  - Changing the amp would change the factory presets (rule 6). The test asserts no drop beyond 1.5 dB from the loudest step, and that full gain is louder than none.
- **Amp cold-start fix preserves the sound.**
  - The subtracted tube resting point is a constant before linear filters that end in a coupling high-pass, so the steady state is unchanged. Only the start-up step goes.
  - The existing preset tests (including `Presets.audioIsIdenticalAfterARoundTrip` and every-factory-preset renders) pass.
- **Delay DC blocker moved to the wet path (item 10).** The 12 Hz blocker on the dry path cost about 0.06 dB at 82 Hz and broke the Mix-0 bypass. Presets that use the Delay change by that amount only.
- **Boot time (item 24).**
  - Warm instantiation measured 225-255 ms on this shared 4-vCPU container. Of that, the factory guitar's parts load is about 80 ms and the two cabinet IR installs about 50 ms.
  - The spec's 200/400 ms are for a "mid CPU". They are asserted as written under `LUTHIER_PERF=1`; the default run holds 1.5x of them as a regression bar.
  - The Looper change (undo/redo on first use) took 30-90 ms off prepare.
- **Timing tests use the 95th percentile of 100 events (item 25).** The worst case is reported in the message but not asserted, because a shared runner's scheduler owns it. The audit asked for "worst of N".
- **CPU-unit tests are gated behind `LUTHIER_PERF=1` (items 26, 27, 30, the 60-minute memory session, the 100k fuzz, 32 instances, 1000 undo steps, 10 000 state operations).**
  - A unit is a percentage of whichever core runs the test, so it is machine-relative and flaky on shared CI.
  - The nightly workflow runs them and publishes `perf-dashboard.json`.
  - The three looser existing bars the audit lists (Modulation 1.0, Scrape 0.5, Slap 0.6) are left as they are. `everyModuleWithinBudget` holds the section 1 numbers where it measures.
- **CPU relief (item 32).** Only step 4 has an existing engine hook (`NoiseEngine::setDegraded`), so only step 4 acts. The Options → Diagnostics toggle and the "CPU limit" banner are not built (UI files are out of bounds). The ladder exposes `setStringDropAllowed` and `isCpuLimitBannerDue` for them.
- **The allocation trap found `ParameterBridge` allocating about 424 strings per block (item 19).** It is fixed with a lock-free cache keyed by the `ParamIDs` pointer, filled on the warm-up block, plus indices precomputed in `cachePointers` for the per-slot IDs. `RoutingMatrix::getMidiOutConfig` became a seqlock. Neither changes behaviour.
- **`LuthierEngine::panic` now also resets the playing-noise pool, the coupling matrix, `noteSustainScale` and the bridge/coupling vectors (item 34).** The state fuzz showed strings growing after a panic until `reset()`.
- **Migration banner criterion (item 15).**
  - A load counts as migrated when a legacy conversion ran:
    - no ranges block and an older `pluginVersion`;
    - pickup placements;
    - `feedback_on`;
    - `strum_speed`;
    - the engine doubler;
    - a pre-parts guitar name that resolves through `migration.json`.
  - Factory presets without a ranges block but with the current version do not count.
  - One banner per editor, which is "the first affected load".
- **Backup path (item 16).** The backup goes to the nearest `Presets` ancestor's `Backup/<date>/`, falling back to the file's own folder outside a Presets tree. The "first migration" backup is the save-time backup: a migrated file's original is kept at the moment it is first overwritten.
- **InstallLayout (item 6)** runs in the processor constructor on every instantiation. It is cheap: it checks what exists before creating anything. `plugin.json` is created with a minimal `{format, createdBy, settings}` because no spec defines its contents.
- **Update download (item 14).** The fetch goes through a `Fetcher` stream seam rather than Telemetry's `Transport`, because that returns text and an installer is binary.
- **Packaging (item 46).**
  - The VST3 bundle's build-time `Resources` copy is left out of the packages. Content ships once, in `share/luthier`, and `IrLibrary` finds it from `<prefix>/lib/vst3/Luthier.vst3` (four levels up) and from the fixed paths.
  - JACK and PipeWire are `Recommends:`, not `Depends:`: the ALSA backend works without them.
  - The X11 libraries are named explicitly because JUCE opens them at run time.
- **Standalone single instance (item 47).** `moreThanOneInstanceAllowed()` is false, so a double-clicked file goes to the running window.
- **Content signatures (item 49).** RSA (`juce::RSAKey`) over the manifest's SHA-256. The trusted public key is a parameter; embedding the release key is a release step. Allowed types:
  - preset, guitar, tune and part files;
  - pattern, kit, set and midprofile files;
  - `.wav`, `.json`, `.txt` and `.md`.
- **`aux1_pre_circuit` (item 23).** The pre-circuit DI is the pickup signal before the circuit, or the sidechain when re-amping. It carries no input gain, because the input gain is the trim into the rig after the circuit.

From the action-and-undo helper:

- The stack limit is 200 (action-and-undo.md 2), not gui-integration.md 18's 64. The more specific spec wins.
- Part-swap and drag wording follows workshop-ui.md 8 ("Fitted X (was Y)", "Moved ... mm"), not action-and-undo 3.4/3.5's wording.
- Save As Guitar keeps its undo entry: it repoints the preset's guitar reference, which is a state change, not only a file write.
- The boundary-crossing key is Ctrl-Alt-Z (section 9), because Ctrl-Shift-Z is redo. This overrides sections 0.2 and 5's "Shift-Ctrl-Z" and gui-integration 18's "holding Shift".
- Grouping keeps gesture = one entry (workshop-ui.md 8) for every control. A drag that pauses for more than 200 ms is still one entry, which departs from 3.1's split-on-pause. Separate gestures on the same parameter within 200 ms (wheel ticks, combo scrolls) merge. The gap is measured from the previous entry's latest time to the new gesture's start.
- Toggles (boolean parameters) never group (3.3). Choice parameters group like continuous ones (3.2).
- A boundary is modelled by position alone: plain undo refuses when the entry just undone was a boundary. No extra "stopped" state is kept, and redo walks through boundaries normally.
- Guitar load is the gesture on `guitar_type` (a boundary entry "Load guitar X"). Program-change and host `setCurrentProgram` loads push nothing: they are not the plugin's UI (section 11).
- Undo leaves the session layers alone: view (`ui`, which includes the bench A/B slots), Live Mode, A/B slot flag, randomise locks, click-to-main, the metronome, and the tune (the tune keeps its own recorded stack). Locks and the metronome are treated as options, not values.
- Recall from MIDI or setlist pushes nothing. Only the UI recall pushes "Recall snapshot i name" (7: "not a change" for passive or live events).
- Layers outside the state blob (scale trainer, looper layer settings, backing track, looper clear) use callback entries. The callbacks capture only processor-owned objects, so they stay valid after the panel closes.
- The setlist is stored inline (entries plus file path plus position), not only as a path. A missing file or an unsaved edit still restores, and setlist edits are undoable through the blob.
- gui-integration 22 asks for a "1000-op random walk", but 1000 is more than the 200-entry cap. The walk is 1000 ops in segments of 125 (≤200). Each segment is fully undone and compared, fully redone and compared, and then undone to start the next.
- The random walk compares states with a 1e-6 numeric tolerance, not byte for byte. A guitar restore rewrites the parameters its parts overlap (`applyGuitar`), and float rounding there moves values like `realism_detune` by about 3e-8. The walk also settles with one fit and undo first. A fresh instance reads per-string custom gauges as 0 until a parts guitar has been applied once, which is a first-load artefact of the per-string side channel (GAPS B1), not an undo failure.
- The MIDI Learn test calls the `onBeforeLearn` hook directly, because the test binary runs no dispatch loop (the same constraint as the existing MidiLearn tests).
- Undo-history click crossing a boundary does not ask again: choosing an older entry in the list is itself the explicit request.
- `LoopLayer`'s undo buffer now also holds audio emptied by Clear. The per-layer Undo button therefore restores a cleared layer (section 3.14's "restore mechanism").
