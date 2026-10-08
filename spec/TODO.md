# TODO

Working list for the autonomous run (`PROMPT.md`). Order follows
`INDEX.md` "Build order for the engine work", then `CLAUDE_CODE_BRIEF.md`
steps 6-12. `GAPS.md` has the detail behind each row; `DECISIONS.md` has
every judgement call made along the way.

Build: `cmake --build build --config Release --target LuthierTests -- -v:m -p:CL_MPCount=1`,
one target at a time, foreground. Tests: `build/LuthierTests_artefacts/Release/LuthierTests.exe [filter...]`.
On Linux: `scripts/setup_linux.sh` then
`ninja -C build LuthierTests`, and
`xvfb-run -a build/LuthierTests_artefacts/Release/LuthierTests [filter...]`.

## Triage note (2026-10-01)

The engine/model workstream branches (tune-help, techniques,
feat2-notation/tab, midi/practice gaps, model-gaps, realism A/B/C,
visual-workshop-qa) have all been merged into `codex/luthier-beta`, but
many checkboxes below were never ticked. A full re-verification of every
open row against the current `Source/` tree (with `docs/coverage/*.md` as
the cross-check) was done on 2026-10-01; the result is recorded here.
`Source/WIP/` no longer exists - every historical WIP module (muting,
first-run, slap, scrape, strum) is promoted into the real tree with tests.

What is genuinely still open is small and almost entirely non-code:

- **13 (HELP)** - the support URLs / email are still RFC-2606
  `luthieraudio.example` placeholders. The mechanism is done (one
  configurable `Source/Support/SupportLinks.h`, overridable at configure
  time via `LUTHIER_HOMEPAGE_URL` / `LUTHIER_SOURCE_URL` /
  `LUTHIER_SUPPORT_EMAIL`; `SupportLinks::areConfigured()` is false until
  set, and the About text says so). Setting the real values is a release
  config step that needs the real domain/email - **DEFERRED** to the
  release owner, not a code change.
- **2d(4)** - a listening pass on the rubric's unison voicings. The
  behaviour is intentional (`DECISIONS.md` "Rubric unisons"); this is a
  human QA/listening task, nothing to implement - **DEFERRED**.
- **V (Visual appeal) / remaining G (illustration polish)** - GUI/drawing
  polish (zoom/pan, thumbnail worker cache, accent choices, VU meter,
  screenshots-by-eye). **Owned by the GUI audit** - not touched here.
- **15 / 17** - final subjective polish, performance and bug-bash passes -
  **DEFERRED** (process, not a discrete code item).
- **16** - installer platform matrix - **DEFERRED** (cannot be run from a
  single-platform CI container; see `DECISIONS.md`).

No open TODO row requires engine/model code for the beta: the realism,
technique, capture, notation, MIDI, practice and tune work the rows
describe is implemented and tested.

## Remaining / deferred

- [ ] **13. HELP tab** - DEFERRED (release config). Content, per-panel `?`
      icons (gui-integration 20) and the `showHelp` topics are done
      (`PanelHelpButton`, `AdvancedPanel::showHelp`, `HelpTab` tests); the
      stale `docs/TROUBLESHOOTING.md` / `USER_MANUAL.md` strings are fixed.
      Open: set real support URLs/email in `Source/Support/SupportLinks.h`
      (and `docs/KNOWN_ISSUES.md`). Needs the real domain - owner's call.
- [ ] **V. Visual appeal** - OWNED BY GUI AUDIT. The guitar-shop theme,
      amp/pedal faces, palettes-reach-the-UI and `Theme`/`FacesIntegration`
      tests are in. Remaining (header plugin name in the display face,
      Options -> Appearance accent choices + follow-the-guitar, VU meter /
      room light, live overlay polish, preset-browser thumbnails,
      screenshots reviewed by eye) is GUI-drawing polish - not in scope
      here.
- [ ] **G. Realistic guitar illustration** - CORE DONE, GUI polish owned
      elsewhere. `GuitarRenderer` / `GuitarBodyComponent` /
      `HeadstockOutlines` / `GuitarThumbnails` built and green
      (`GuitarRendererTests`, `IllustrationRemainderTests`,
      `AnimatedStringsTests`). Remaining (zoom/pan, thumbnail worker-thread
      cache, per-string material override drawing, capo drawing,
      reduced-motion crossfade, 60 ms note-dot timing) is GUI-drawing work
      owned by the GUI audit.
- [ ] **2d(4). Rubric unison listening pass** - DEFERRED (human QA). The
      voicer's unison weights are the spec's (`DECISIONS.md` "Rubric
      unisons"); whether strummed unisons sound right is a listening pass,
      not code. The rest of 2d is done (see Done).
- [ ] **14. Spec audit sweep** - features built & tested; formal read-through
      only. `ui-wiring.md`, `onboarding.md`, `performance-budget.md`,
      `qa-polish.md`, `installer.md` and gui-integration 20-22 are now
      backed by code and tests (`PerfBudgetTests`, `GuiReachabilityTests`,
      `InstallTests`, `QA_*`, OB-* onboarding tests, `?`-icon tests).
      `GAPS.md` still lists them "Not audited yet" but self-declares (its
      own footer) as written against a build it did not run. No specific
      known code gap remains; this is a verification exercise.
- [ ] **15. Polish / performance / onboarding pass** - DEFERRED (process).
      Onboarding is done (OB-* tests); perf is budgeted (`PerfBudgetTests`);
      the remaining subjective polish/perf pass is a final-pass task.
- [ ] **16. Installer pass** - DEFERRED (platform-blocked). `InstallTests`
      exist; the Windows/macOS/Linux packaging matrix cannot be exercised
      from this container. See `DECISIONS.md`.
- [ ] **17. Bug bash and final check** - DEFERRED (process).
- [x] **18. Plugin targets build clean.** `Luthier_VST3` and
      `Luthier_Standalone` build clean from the Linux Ninja tree (verified
      2026-10-01). `Luthier_CLAP` is wired when `clap-juce-extensions` is
      present. AU is added on macOS.

## Done (verified against the current tree, 2026-10-01)

- [x] Scrape and strum integration built and green; SlapEngine +
      TechniqueTriggers integrated. (Was "In progress".) `Source/WIP` is
      gone - muting (`Source/Rhythm/Muting`, `Source/DSP/Techniques/MuteEngine`,
      `Source/UI/MuteGroup`; `MutingTests`) and first-run / first-encounter
      (`Source/UI/FirstRun`, `FirstEncounterHint`; `FirstRunTests`) are all
      promoted with tests.

- [x] **13c. Phase 2b realism specs** - all nine on disk AND implemented
      with tests (the old "BLOCKED: not on disk" note is wrong):
      string-aging (`DSP/String/StringAging`; `StringAgingTests`),
      environment (`Character/EnvironmentModel`; `EnvironmentTests`),
      body-coupling (`DSP/Coupling/BodyCouplingBank`; `BodyCouplingTests`),
      harmonic-realism (`LuthierEngineRealismB`; `HarmonicRealismTests`),
      string-interaction (`LuthierEngineRealismB` + `UI/StringInteractionGroup`;
      `StringInteractionTests`), fingerstyle-attack (`LuthierEngineRealismB`;
      `FingerstyleAttackTests`), noise-floor (`DSP/Noise/NoiseFloor`;
      `NoiseFloorTests`), sustain-and-decay (`LuthierEngineRealismB`;
      `SustainDecayTests`), tuning-stability (`TuningStabilityTests`).

- [x] **C. docs/spec-coverage.md** - built and maintained (one row per
      actionable requirement, ~3000 rows, refreshed through recent commits).

- [x] **6e. Off-thread part swap + block-boundary swap.** `WorkshopSwap`
      suite (`WorkshopPresetTests.cpp`): click-free swap, note queued while
      parked is kept, map-once-not-per-block, no file I/O on the audio
      thread.

- [x] **5b. slide-technique-controls.md** - `SlideEngine` implements
      position source (abs/rel, modwheel/bend/MPE Y/expression/CC/drag),
      slant & pressure sources, contact string mask, speed limit,
      auto-vibrato on hold, scripted `SlideGesture`; Techniques tab Slide
      sub-tab (`TechniquePages` SlidePage); `SlideTechniqueTests`.

- [x] **2k. Coverage-refresh gaps (2026-09-23)** - all done:
      - part-acoustics 2.1: chambering feeds the feedback coupling
        (`FeedbackLoop::bodyCouplingFor`/`setBodyCoupling`;
        `ModelGaps::chamberingFeedsTheFeedbackCoupling`).
      - notation-export 4/6.1: the engine calls the capture's
        chordSymbol / bassTechnique / slideBar (`LuthierEngine::captureBlockState`;
        `ModelGapsUi::theCaptureHearsTechniquesAndChordsFromTheEngine`).
      - notation-export 7.1: `LUTHIER_ALLOCATION_COUNTER` is defined for
        the test target (`CMakeLists.txt`); the no-alloc checks are live
        (`Capture::capturingTenThousandNotesDoesNotAllocate`). (The old
        note that it "is never defined" was wrong.)
      - gui-engine-dataflow 22: `FeedbackLed::kRefreshHz == 30`
        (`ModelGapsUi::theFeedbackLedDrainsAtThirtyHertz`).
      - file-formats 2: a migrated load backs up the original to
        `Presets/Backup/<date>/` (`PresetManager::backupMigratedOriginal`;
        `ModelGapsUi::aMigratedPresetKeepsItsOriginal`, `PresetQaTests`).
      - notation-export 0.1: export runs on a worker thread
        (`NotationTakeExport::writeAsync`;
        `ModelGapsUi::notationExportRunsOnAWorkerThread`).
      - Doubler pitch / HP / LP defaults and VP-7-04 at panel level
        (`ModelGapsUi::theDoublerDefaultsAreTheClassicAdt`,
        `::standbyAndBypassReachTheFacesOnThePanels`).

- [x] **2h. Easy rig strip polish** - amp card has room at 1200x720
      (`EasyPanel`/`AmpFacePanel`; `EasyLayout::ampKnobsHaveRoomAtCompactWindowSize`).

- [x] **2d. ambiguity-resolutions.md gaps** (except the 2d(4) listening
      pass, above):
      - RubricVoicer in at runtime; bass-pattern on the rhythm engine
        (`RhythmEngine::setBassPattern`, `RubricVoicer::setBassPattern`);
        `RhythmEngine::selectNotesForStyle` retired.
      - Crossing velocity from the pattern (`RhythmEngine::resolveCrossingSps`;
        `StrumGestureTests`).
      - Feedback / freeze / E-Bow as mod destinations
        (`ModelGapsUi::theSustainControlsAreModulationDestinations`); Aux 1
        pre/post-circuit toggle (`aux1_pre_circuit`,
        `LuthierEngine::setAuxDiPreCircuit`;
        `ModelGapsUi::auxOneTapsBeforeOrAfterTheCircuit`).

- [x] **7. Workshop bench** - remainder done: per-string string overrides
      (`StringOverride`, `WorkshopBench::setStringOverride`,
      `partsStringMaterial/Wound`; `WorkshopStrings` tests), nut slot drag
      (`setNutSlotDepth`; `WorkshopNut`), pick/slide/capo overlays & drags
      (`WorkshopAccessories`), WORKSHOP tab padlock (`WorkshopRanges`),
      wrench-in-Easy editor test (`WorkshopEditor`), spectrum summary to
      screen readers (`WorkshopSpectrum`), slide material in the engine
      (`LuthierEngine::setSlideBar`, processor `setSlidePart`).

- [x] **8. StrumGesture + BassTechniques** - `Source/Rhythm/StrumGesture`
      (RHYTHM STRUM; `StrumGestureTests`); SLAP group + bass step grid
      (`Source/Rhythm/BassStepGrid`, `RhythmEngine::processBassGrid`,
      `BassGridGroup`; `SlapTests`, `BassTechniqueTests`).

- [x] **9. PerformanceCapture + NOTATION tab** - remainder done: technique
      hook from `triggerNote` (`LuthierEngine` noteOn passes technique /
      harmonic partial; palmMute/accent/pickStroke marks), marked-region
      range (`CaptureRanges`; `CaptureRangeTests`), fretboard tablature dots
      (`NotationPanel` toggle -> `FretboardComponent`), Mono offline chord
      extraction (`PerformanceCapture` via `ChordDetector`).

- [x] **10. MIDI export + MIDI OUT tab** - remainder done: CHARACTER seed /
      environment events (`sendCharacterChanges` SysEx), import UI (header
      "Import MIDI...", file drop, `MidiImportTargets`; `MidiImportTests`),
      marked-region / current-section ranges (`CaptureRanges`), drag from the
      session recorder Save button (`PracticePanel` external drag).

- [x] **11. PracticeRoutines + PRACTICE tab** - remainder done:
      SessionRecorder honours record audio/MIDI and auto-save
      (`Looper`; `PracticeGapsTests`), looper default length, trainers'
      note range / question count (`PracticeRoutineSetup`), tab-reader
      recent-list test (`PracticeGapsTests`).

- [x] **12. Tune Builder + TUNE tab** - remainder done: editing GUI
      (`TuneChordEditor`, `TuneChordPillsEditing`, `TuneSectionStripEditing`,
      `TunePianoRoll`, `TuneLayersStrip`, `TuneExportDialog`;
      `TuneEditingTests`); kit suggested tempo, separate tune undo stack,
      state-boundary test, hum capture (`TuneHumCapture`; `HumCaptureTests`),
      tests 15-07..15-10, mod routes, Ctrl+T in the registry
      (`TuneIntegrationTests`).

- [x] **13b. Phase 5b technique specs** - all implemented with tests:
      string-scraping (`DSP/Noise/ScrapeEngine`; `ScrapeTests`),
      string-slap (`DSP/Slap/SlapEngine`; `SlapTests`, `BassTechniqueTests`),
      muting-rhythm (`DSP/Techniques/MuteEngine` + `Rhythm/Muting` + `UI/MuteGroup`;
      `MutingTests`), two-hand-tapping (`DSP/Techniques/TapEngine`;
      `TapTests`), microtonal-bends (`DSP/Techniques/MicrotonalScale`;
      `BendTests`), technique-cascade (`CascadeTests`),
      gui-techniques-updates (`UI/Techniques/TechniquePages`,
      `TechniqueMirrors`; `TechniquesUiTests`), engine-technique-layer
      (`DSP/Techniques/TechniqueLayer`; `CascadeTests` TechniqueLayer suite).

- [x] **14b. action-and-undo.md** - entry classes, grouping and state
      boundaries verified (`UndoCoverageTests`); the off-by-one is fixed
      (c0b05eb).

- [x] **14c. Onboarding restore clears `ranges_first_unlock_explained`** -
      `FirstRun`/`DiagnosticsPage` restore clears the one-time flag and
      keeps libraries (`FirstRunTests::restoreClearsTheSettings...`).

- [x] 2g. Trademark sweep: guitar types, amps, speakers, mics, bridge types,
      bleed and slide names, factory presets and their descriptions, genre
      kits, tooltips, 14 factory parts and 2 guitars renamed to reference-style
      names; old part and guitar names still resolve; `Tools/trademark_scan.py`
      and the `Trademarks` tests keep them out.

- [x] 6. Workshop parts: part model + library, 148 factory parts and 27
      guitars, mapSpec, processor loader, preset `guitar.reference` /
      `override`, Save As Guitar / Save As Part, Ctrl+G and Ctrl+Shift+E,
      click-free swap, and the partial capo (TuningEngine string mask, capo in
      the preset guitar block). 415 tests green.

- [x] 5. Slide Mode: SlideEngine, header toggle and `S` shortcut, CHARACTER
      SLIDE group with the low-action warning, fretboard bar overlay.

- [x] 4. Setup geometry, sensed fret buzz, sitar mode, SETUP group with the
      live heatmap and setup styles. (Character fret wear moving buzz,
      fret-buzz.md 8, waits on a per-fret wear height from CharacterEngine.)

- [x] 3a. Finger squeak audible/discoverable (owner 2026-09-26): lifted-hand
      chord changes squeak, level normalised to 20-30 dB under the note at the
      output, acoustic direct-air path, per-event scatter, probability 1 = always
      (string-squeak.md 2.2/3.1/5.1; StringSqueakRealismTests).
- [x] 3. NoiseEngine pool; pick click/chirp/scrape; finger squeak; CHARACTER
      tab STRING NOISE and PICK groups with style presets, noise-event strip
      and the tab padlock. (Aux 8 is 2f; scrape trigger is 3f.)

- [x] 2. `GuitarCircuit` replacing `CableSim`; 11 circuit params in the
      `circuit` family; CIRCUIT panel with live response view and standard-value
      dropdowns; coil resonance moved out of `PickupEngine`.
- [x] 2b. Advanced amp ranges audible (AmpEngine accepts the advanced span).

- [x] 1. Advanced ranges UI: Options RANGES page, warning arc and `*`,
      header padlock, right-click unlock/restrict, locked-edge notice,
      first-unlock explainer, randomise-in-stock, control resync on range swap.

- [x] `PhysicalRange`, `RangeRegistry`, `RangeState` (commit 2ef230b).
- [x] `RangeState` wired into preset save and load, ranges block applied
      before parameter values; harness fixed; round-trip test added.

## Not touched here (owned by other workstreams)

- Host-clock validation in `PluginProcessor` (owned elsewhere).
- B-* beta-test findings / RT-safety docs (owned elsewhere).
- GUI parameter wiring / orphan params (GUI audit).
- Normalization goldens / ON27 (owned elsewhere).
