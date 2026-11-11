# Luthier: non-pedalboard product inventory (branch I = integration/luthier-completion, ca14934e)

Read-only discovery. Nothing was built or run; every "test" claim below is a test that exists by name in `Source/Tests` at HEAD (checked by grep), not a test that was executed. Pedalboard (Reference pedals, `Source/ReferencePedals`, `docs/coverage/PEDALBOARD-*`) is out of scope except where issues.md touches it.

## 0. Read this first: source freshness

- `docs/spec-coverage.md` is STALE. Its own header says "Audited 2026-09-23 ... Rows cite what is committed at `52cb9d7`" and its tail says "the historical coverage totals above are not current native release evidence" (`docs/spec-coverage.md:3090-3092`). Header table claims 2,081 rows; the parsed table has 1,989 six-column rows plus 5 rows in a later 5-column section (see section 3). Parsed with `/tmp/.../scratchpad/discovery/parse.py` (rows.json).
- Evidence the stale rows are wrong: the 9 phase-5b technique specs and strum-dynamics show 0 verified (e.g. 200 of 256 technique rows "pending"), but `Source/Tests` has `ScrapeTests.cpp`, `SlapTests.cpp`, `TapTests.cpp`, `MutingTests.cpp`, `CascadeTests.cpp`, `BendTests.cpp`, `StrumGestureTests.cpp`, `SlideTechniqueTests.cpp`, `BassTechniqueTests.cpp`; `spec/TODO.md:14-48` says `Source/WIP/` no longer exists and "muting, first-run, slap, scrape, strum" are promoted with tests. Likewise performance-budget / onboarding / installer / qa-polish rows say "pending" while `PerfBudgetTests.cpp`, `OnboardingTests.cpp`, `FirstRunTests.cpp`, `InstallTests.cpp`, `QA_*.cpp` exist. Treat "pending" counts as an upper bound, not as an unbuilt count.
- `docs/audit/REMAINING.md` (generated 2026-09-29, `587e15e8`): 932 rows (474 MISSING, 458 PARTIAL), tiers CRASH 16 / FEATURE 481 / POLISH 218 / DOCS 217. Also pre-dates the technique and reset/pedal work.
- `spec/PROGRESS.md` tail is dated 2026-09-24 ("Last full green run: 677 tests"). `spec/TODO.md` triage note (2026-10-01) is the most recent status: "No open TODO row requires engine/model code for the beta"; open items are HELP support URLs (placeholder `luthieraudio.example`, `Source/Support/SupportLinks.h`), GUI polish (V, G), rubric-unison listening pass, polish/bug-bash/installer-matrix passes (all DEFERRED, process or platform-blocked).
- `docs/KNOWN_ISSUES.md` (Version 1.0.0) is partly stale: says CLAP/AU "not built" and "Panic does not clear the reverb", both contradicted by `CMakeLists.txt:47-56,89-92` and commit `181a9d78` (Panic now clears tails, `ResetStop.panicStopsEveryPlayerAndLeavesTheSettings`).
- Test count: about 2,924 `LUTHIER_TEST` macros across 324 files in `Source/Tests` (grep count, approximate). `docs/coverage/PRO-V01-RELEASE.md:72` records Windows running 2,361 tests on a 2026-10-18 run.

## (a) Feature-area table

Coverage rows = parsed from `docs/spec-coverage.md` (stale, see 0). "other" splits implemented (code, no proving test) / partial / blocked / n/a. 20 spec files have NO rows in spec-coverage (amp-cab-ir, animated-strings, auto-articulation, cpu-quality-modes, editions, editions-rollout-plan, global-search, jam-mode, licensing, mic-placement, midi-learn, piano-roll-chord-display, preset-browser-previews, randomize-and-ab, riff-library, tab-export, tab-import-export, tuner-and-tuning-reference, ui-scaling, issues); their status lives in `docs/coverage/FEAT-*.md`, `FEAT2-*.md`, `CODEX-SPEC-COVERAGE.md`, `TECHNIQUES.md`, `MODEL-GAPS.md`, `REALISM-*.md`.

| Feature area | Spec files (rows in spec-coverage) | Rows total / verified / pending / deferred / other | Known defects (cited) | Remaining work |
|---|---|---|---|---|
| Engine and string physics | engine, advanced-ranges, pick-noise, string-squeak, fret-buzz, character-wear, volume-knob-interaction, spec.md (+ unrowed: auto-articulation, cpu-quality-modes) | 425 / 227 / 31 / 0 / impl 124, partial 39, n/a 4 | B-13 idle CPU ~= playing CPU, idle 6-12x over 1.5-unit budget (BETA_TEST_REPORT:146); B-12 pluginval Parameter-thread-safety needs `--timeout-ms` > 30 s (:141); B-20 clap-validator state-reproducibility by design/ulp; `Combo` saturated amp holds tail while strings die (report "Harness corrections"); KNOWN_ISSUES modelling approximations (one-way body coupling is now partly obsolete: Phase-2b body-coupling files exist) | no suite-wide allocation probe on processBlock (SPEC-TH-01 partial); `LUTHIER_ALLOCATION_COUNTER` undefined so one no-alloc assertion compiles out (spec-coverage header, NOT-7.1-02); idle short-circuit not enabled at High quality |
| Instruments / Workshop / illustration / factory content | guitar-workshop, part-acoustics, workshop-ui, guitar-illustration, factory-content (+ unrowed: animated-strings, piano-roll-chord-display, tuner-and-tuning-reference) | 185 / 90 / 28 / 1 / impl 25, partial 40, blocked 1 | B-05 factory presets/Reset ignored guitar parts (FIXED); B-04 loud idle floor (FIXED); B-17 custom preset played previous guitar (FIXED `bb0da84`); KNOWN_ISSUES "guitar is drawn, not photographed" | TODO "V" and "G": zoom/pan, thumbnail cache, per-string material drawing, capo drawing, accent choices, VU meter, screenshots "reviewed by eye" (TODO:50-66, marked owned by GUI audit); Workshop drawer 700 px dropdown, 30 ms audition crossfade, A/B persistence (spec-coverage item 3, stale) |
| Techniques (slide, strum, bass, scrape, slap, mute, tap, bends, cascade, TECHNIQUES tab) | slide-guitar, strum-dynamics, bass-techniques, string-scraping, slide-technique-controls, string-slap-technique, muting-rhythm, two-hand-tapping, microtonal-bends, technique-cascade, engine-technique-layer, gui-techniques-updates | 256 / 23 / 200 / 0 / impl 10, partial 13, blocked 7, n/a 3 (STALE: rows predate the code; see 0) | B-11: `scrape_*` (14) and slap arm/trigger params (13) had NO GUI control at report time (BETA_TEST_REPORT:132-138; status after merge UNVERIFIED); RHYTHM tab clipped STRUM rows (FIXED round 2) | Re-audit these 12 spec files against `ScrapeTests/SlapTests/TapTests/MutingTests/CascadeTests/BendTests/StrumGestureTests/TechniquesUiTests`; 7 blocked rows depend on Phase-2b specs now on disk (spec/body-coupling.md, harmonic-realism.md, string-interaction.md exist) so "blocked" is stale |
| Rhythm / live / practice / tone-match / tune builder / modulation | rhythm-engine, live-performance, practice-tools, tone-match, tune-builder, modulation-matrix, ambiguity-resolutions (+ unrowed: jam-mode, riff-library, midi-learn, randomize-and-ab) | 301 / 159 / 22 / 0 / impl 72, partial 47, blocked 1 | B-14 Panic/Reset did not stop transport players (FIXED, `181a9d78`); B-02 strings grew after rhythm stop (RESOLVED); B-03 released notes rang on (FIXED) | practice-tools PRA-12-04 needs Guitar Pro parser and 50 fixtures (blocked); tune-builder 16 partial/8 pending (stale); rubric unison listening pass (human QA, DEFERRED) |
| GUI / theme / onboarding / wiring | gui-integration, ui-wiring, gui-engine-dataflow, theme, proposals/visual-polish, onboarding (+ unrowed: ui-scaling, global-search, preset-browser-previews) | 254 / 47 / 60 / 1 / impl 60, partial 76, n/a 10 | B-11 automatable params with no control (OPEN/IN PROGRESS, `GuiReach.everyAutomatableParameterHasAVisibleControl`); `Reflow.noControlHangsOutsideItsParentAtAnyWidthOrScale` failure at 940/1000 px (B-23, status after FIX-CROSS UNVERIFIED) | GUI polish owned by "GUI audit": header name in display face (done `22720aea`), VU meter/room light, preset-browser thumbnails; HELP support URLs placeholders; CQ22 timer registration for 4 UI classes (B-23, may be fixed by `08ebf03d`) |
| Presets / state / undo / file formats / error recovery | state-model, file-formats, action-and-undo, error-recovery | 132 / 14 / 49 / 0 / impl 20, partial 48, n/a 1 | B-10 morph position lost from state (FIXED `748aeb9`); B-16/B-20 one-ulp parameter drift (low, by design); REMAINING.md CRASH tier: ER-3 no backup for guitar/setlist saves, ER-15 migration failure path, ER-22 concurrent save, ER-24 non-atomic saves for setlist/controller/live-input/practice/UiPreferences (as of 2026-09-29) | forward-compat payload tests added (`PresetForwardCompatibilityTests.cpp`); migration fixtures 200 (FF-16-03), kill-mid-save x100 (FF-16-05), backup pruning (FF-16-06) pending |
| MIDI in / controllers / MIDI learn | controllers (+ unrowed: midi-learn) | 29 / 15 / 2 / 0 / impl 9, partial 3 | MPE per-note bend/pressure implemented, no test (SPEC-MIDI-02); physical MIDI-guitar/MPE controller testing "not carried out" (KNOWN_ISSUES) | `MidiLearnCoverage` added 2 tests (all 124 non-reserved CCs); hardware controller validation not done |
| Routing / I/O | routing-io, input-routing | 54 / 21 / 7 / 0 / impl 16, partial 10 | HI-2-02 "mono main out rejected" row pending (conflict C-26) | `PluginBusTests`, `InputRoutingTests` exist; bus layouts covered by pluginval per KNOWN_ISSUES |
| Exports (MIDI, notation, tab) | midi-export, notation-export (+ unrowed: tab-export, tab-import-export) | 69 / 39 / 4 / 0 / impl 7, partial 19 | issues.md 10 (MIDI import) addressed, see (b); MX-23 macOS installer does not register `.midprofile` (REMAINING.md:873) | drag-out export of rendered file into DAW not implemented (KNOWN_ISSUES); `.midprofile` schema FF-10-01 pending |
| CLI | `Tools/RenderCli.cpp` (`luthier-render`: `--midi --preset --out --audition --guitar --list-presets --help --verbose`, `--calibrate-factory`); spec cli-tools.md rows are spread under other prefixes (RDM-CLI-01, SPEC-DEL-07, ON-GEN-*) | RDM-CLI-01 impl, SPEC-DEL-07 impl; ON-GEN-02/03 pending | ON-GEN-02 (failed generation preserves destination) and ON-GEN-03 (factory x guitar canonical coverage) pending per `docs/spec-coverage.md:3098-3106`; `CliConvertTests.cpp` exists (content not read, UNVERIFIED) | `ON03_FactoryTableDoesNotDrift` red on revision 2, revision-3 table committed `eb134fc` but native pass not shown (PRO-V01-RELEASE) |
| Amp / cabinet / IR | amp-cab-ir (NO rows in spec-coverage); README rows RDM-BOX-01 (13 amps, 10 cabs, 8 speakers, 7 mics, 720 IRs) | no rows | IRs are synthesised not measured (KNOWN_ISSUES); speaker cone linear; IR swap heard through fallback for one block; `Amp.*` tests in `EffectsQaTests.cpp` (gain monotonic, neutral stack flat, cold start) | Free edition: 7 of 13 amps (`Edition.h:83-91 isFreeAmpIndex`); spec status for amp-cab-ir.md UNVERIFIED |
| Editions / licensing | editions, licensing, editions-rollout-plan (NO rows in spec-coverage) | no rows | Pro is paid, `LUTHIER_PRO` defaults 1 (`Source/Edition.h:3-8`); Free = 6 of 25 guitars, 7 of 13 amps, 15 of 22 pedals (`Edition.h:73-120`); licensing server/signing keys outside repo (UT-5-05 blocked) | `Source/Licence/*` + `LicenceActivationTests/FileTests/StateTests/StoreTests`, `MachineFingerprintTests`, `EditionTests` exist; real EULA still placeholder `packaging/common/EULA.txt` (RELEASING:174-176); activation server not in repo |
| Installers / updates | installer, updates-telemetry | 54 / 13 / 19 / 1 / impl 13, partial 3, blocked 5 | no signed/notarised installers: needs certificates (KNOWN_ISSUES, RELEASING §4/§7); delta updates, MSI, DAW-loaded refusal check not built (RELEASING §10); macOS `.pkg`/`.dmg` blocked on certificates | Linux `.tar.gz`/`.deb` (`packaging/linux`), Windows Inno Setup (`packaging/windows/Luthier.iss`), macOS scripts (`packaging/macos`) exist; `InstallTests.cpp` exists; platform matrix cannot be exercised in one container (TODO 16 DEFERRED) |
| Accessibility | accessibility | 39 / 9 / 13 / 0 / impl 5, partial 12 | no NVDA/VoiceOver/Orca testing (ACC-1-05); OB-10 macOS/Linux OS reads for reduced motion/high contrast/DPI missing (REMAINING.md:627) | `AccessibilityTests`, `AccessibilitySweepTests` exist; Tab-walk, CJK fallback, bidi fixtures pending |
| Host integration / format wrappers | host-integration, include | 54 / 7 / 6 / 1 / impl 21, partial 18 | docs/HOST_COMPATIBILITY.md says "Host-by-host manual verification is still required"; HI-51 VST3->AU switching manual test missing; AAX deferred to v1.5 (HI-9.8-01) | per-host quirk handling is documented, not host-tested; see (d) |
| QA gate / performance / process | qa-polish, performance-budget, CLAUDE_CODE_BRIEF, INDEX, README, JUCE guidelines, Phase 2b | 137 / 9 / 61 / 0 / impl 21, partial 26, blocked 14, n/a 6 | qa-polish section 0's five gates not green (spec-coverage:68-72, stale); Phase 2b specs "missing" is stale: all nine now exist in `spec/` (string-aging, environment, body-coupling, harmonic-realism, string-interaction, fingerstyle-attack, noise-floor, sustain-and-decay, tuning-stability) | bug bash, 30-min-per-host matrix, blind listening comparison, human final check: not performed (KNOWN_ISSUES, TODO 15/17) |
| TOTAL (parsed) | all | 1,989 / 673 / 502 / 4 / impl 403, partial 354, blocked 28, n/a 25; + 5-col tail: 2 verified, 2 pending, 1 inspected | | |

## (b) spec/issues.md: 11 reported defects

Classification is strict: FIXED-WITH-TEST needs a test that exists by name at HEAD and asserts the behaviour. Dates: issues.md written 2026-09-23 (`d91a1eea`). Fix commits cited with `git log -i --grep`/`-S`. NOTE: several tests named in fix-commit messages (`ac7eccad`, `f006d6f6`) are NOT present at HEAD (listed under each row); I did not find the commit that removed them (UNVERIFIED cause: merge/rewrite of those test files).

| # | Defect | Class | Evidence |
|---|---|---|---|
| 1 | None of the effects work | FIXED-WITH-TEST (parameter path; GUI click path not directly tested) | `Source/Tests/DocsSweepTests.cpp:802` `Effects.aPedalSwitchedOnThroughItsParametersChangesTheOutput` (comment "issues ISS-1 / ISS-3"): pre Overdrive, post Delay and a full 8-pedal pre chain each change the output of a chord by >10 % vs all-None. Also `Effects.zeroMixIsABypassWithinMinus80`, `Effects.toggleIsClickFree` (`EffectsQaTests.cpp:71,103`), `Effects.everyPedalTypeRunsCleanly` (`EngineTests.cpp:1139`), `GuiReach.everySlotTypeBypassAndMixHasAControl` (`GuiReachabilityTests.cpp:839`). Commits: `ac7eccad` ("pedals built on pick"), `26be9460`. Gap: the 4 end-to-end tests that commit names (`PresetPedals.aPedalPickedFromTheUiBuildsAtOnceAndIsHeard`, `aPostAmpDelayLeavesATailAfterTheNote`, `aTypeWrittenOffTheMessageThreadIsBuiltByTheAsyncPass`, `aPedalPickWhileANoteSoundsDoesNotRebuildTheInstrument`) are absent from `PresetPedalTests.cpp` (only 3 other tests there), and that commit's `Parameters.cpp` comments are absent, so the message-thread pedal-build path is not evidenced at HEAD. Reference pedalboard rework is the other agent's scope. |
| 2 | Pickup changes do little | FIXED-WITH-TEST | Root cause: `pickup_blend` was stored and never read. Fix `PickupEngine.cpp:483` ("SPEC-SWEEP: SP-17 / ISS-2", merge `35f23d62`); UI `AdvancedPanel.cpp:871`. Test `EngineTests.cpp:800` `Pickup.blendSweepsBetweenNeckAndBridge` (bridge-only nulls 4th harmonic; one pickup selected: blend does nothing). Also `Pickup.positionCombNullsTheExpectedHarmonic` (`EngineTests.cpp:725`), `Pickup.selectorChangesCrossfadeIn5ms` (`EngineSweepTests.cpp:375`), `Pickup.humbuckerCombAndCoilTap`, `Editor.illustrationClicksSelectPickupAndStepSwitch` (`UiSweepTests.cpp:940`); B-04/B-05 fixed wrong pickups on every factory preset (BETA_TEST_REPORT:99-106). Test is at the DSP-engine level, not an end-to-end plugin render. |
| 3 | Effects before amp / pedalboard dead / all effects on = no change | FIXED-WITH-TEST for the legacy pre/post chain; pedalboard UNVERIFIED here | Same test as row 1, rigs "pre Overdrive" and "full pre chain" (`DocsSweepTests.cpp:802-851`). Pedalboard (Reference pedals) is separate: see PEDALBOARD-* docs/other agent. |
| 4 | Fingers vs non-fingers no difference | FIXED-WITH-TEST | `DocsSweepTests.cpp:1398` `Excitation.fingersSoundDifferentFromAPick` (comment "issues ISS-4": whole-plugin render, finger darker than pick by first-difference energy, >5 % waveform difference). Code `Source/DSP/String/Excitation.cpp:18-19,75-79,207-215` (fingernail/fingertip materials, contact profile). UI toggle `RightHandGroup.cpp:156` and `AdvancedPanel.cpp:999` bound to `use_fingers` (`Parameters.h:86`). Rich coverage `FingerstyleAttackTests.cpp` FA01-FA17. Commit `ac7eccad`. |
| 5 | Cannot do chords / two simultaneous notes | FIXED-WITH-TEST | `ControllerTests.cpp:899` `Controllers.aNoteArrivingWhileAnotherIsHeldGoesToAFreeString`, `Controllers.aHeldNotePlayedAgainRepicksItsOwnString`, `Controllers.anUnplayableChordGroupStillSounds`, `Controllers.notesTheChordSearchLeavesOutArePlacedOnFreeStrings`; `IntegrationTests.cpp:1641` `Engine.notesPlayedWhileOthersAreHeldEachGetTheirOwnString` (C4, E4, G4 each on its own string, none re-struck); also `Engine.aChordVoicesAcrossStrings` (`IntegrationTests.cpp:284`), `AutoArticulation.aLateJoinLandsOnAFreeStringAtItsOwnTime` (`AutoArticulationTests.cpp:666`), `Rhythm.burstWindowGroupsASpreadChord`. Code: `MidiInterpreter.cpp`, `ChordVoicer.cpp`, `RubricVoicer.cpp` (commit `ac7eccad`, `bf05cf27`). The commit's own `Engine.aRolledTriadInsideTheChordWindowIsOneChordOnThreeStrings` is absent at HEAD. Caveat: KNOWN_ISSUES says chords are strummed unless Strum Speed = 0 and Poly mode adds a 2 ms (15 ms in `ac7eccad`) window; a user can still hear "not simultaneous" by design. |
| 6 | Guitar picture is ugly | FIXED-NO-TEST (subjective; owner acceptance UNKNOWN) | Renderer rewrite `a4069159` (2026-09-23 "Draw the real guitar"), `5f3273de` (level of detail, soft shadow, tiled grain), `22720aea` (header). `docs/screenshots/easy_1200x720.png` (2026-10-20) shows a shaded Strat-style body, pickguard, hardware, strings. Tests `GuitarRendererTests.cpp` (`GuitarIllustration.everyFactoryGuitarRendersWithoutClipping`, `contactSheetForReview`, `fullRenderIsFastEnough` etc.) check structure/speed/hit-testing, not aesthetics. KNOWN_ISSUES: "vector drawing, not a photograph". `spec/TODO.md` "V"/"G": polish still open and "owned by GUI audit". |
| 7 | Scrollbars not obvious; wheel changes knobs instead of scrolling; arrow indicators and tooltips | FIXED-WITH-TEST | Commits `f006d6f6`, `9eb3d621` (ISS-7 port). Code: wheel passes to column unless Ctrl: `Widgets.cpp:953-968` `KnobSlider::mouseWheelMove`, `Widgets.h:157,279` `WheelPassSlider`; accent scrollbar with end buttons `Theme.cpp:1184-1207` `drawScrollbar`, `Theme.h:219`; `AdvancedPanel.h:53` `ScrollHintViewport` chevron with tooltips "More controls above/below: scroll ..." `AdvancedPanel.cpp:26`. Tests `ScrollWheelTests.cpp:46` `ScrollWheel.theWheelScrollsAColumnUnlessCtrlIsHeld`, `:150` `ScrollWheel.popupRowsAndScrollbarButtonsAreReadable`. `Editor.theWheelScrollsAColumnUnlessCtrlIsHeld` named in `f006d6f6` is absent at HEAD (superseded by the ScrollWheel suite). KNOWN_ISSUES still says column 4 needs a lot of scrolling on short windows. Chevron tooltip text itself is not asserted. |
| 8 | MOD/RHYTHM dropdowns squished; LIVE tiny boxes; needs tooltips | FIXED-NO-TEST for tab sizing and LIVE grid layout; FIXED-WITH-TEST for popup row height | Root cause: tab panels laid out at 80 px (`f006d6f6` message). Fix: `AdvancedPanel.cpp:1813-1814` (RHYTHM/MOD tab height = `preferredHeight()`), `f03da71e` (RHYTHM tab sized to `RhythmPanel::preferredHeight`, B-11/issues.md 8), `Theme.h:237` `minimumPopupItemHeight = 22` clamp `Theme.cpp:1285`; LIVE snapshot grid redrawn (`LivePanel.cpp`, 8 x 16 cells with number/label/colour tag). Tests present: `ScrollWheel.popupRowsAndScrollbarButtonsAreReadable` (popup rows >= 22 px), `Live.shiftClickWritesAndClickRecalls` (`LiveInputTests.cpp:663`), `Live.theSnapshotColourIsReachableWithoutRightClick` (`:598`). NOT present at HEAD: `Editor.everyWorkspaceTabIsAsTallAsItsContent` and `Editor.snapshotGridGesturesReachTheBank` (added in `f006d6f6`); I found no test asserting a MOD/RHYTHM combo box height. The "B-11" report confirms the RHYTHM STRUM rows were off-screen until round 2. Screenshot of the defect: `/home/user/luthier/guiissues.png` (shows popup rows ~4 px). |
| 9 | "Reset & Stop" that truly resets and stops everything | FIXED-WITH-TEST for the function; NOT-FIXED as a dedicated button; the visible Easy RESET button does not stop players | Function: `PluginProcessor.cpp:3243` `resetEverything()` (stops tune player, looper, backing track, metronome, practice, rhythm engine free-run, session recorder, kill switch; resets all settings; parks audio thread for engine reset); alias `resetAndStop()` (`PluginProcessor.h:675`); `panic()` also stops players (`181a9d78`, B-14). Reachable by File menu "Reset all settings to default" (`HeaderBar.cpp:375,530`) and Ctrl+Shift+R `resetAll` (`Accessibility.cpp:657`, `PluginEditor.cpp:1204`). Tests `ResetStopTests.cpp:223-473`: `theRunawayFixtureMakesSoundOnItsOwn`, `resetAndStopSilencesEverythingAndRestoresEveryDefault`, `resetEverythingStopsTheTuneAndTheLoopsToo`, `resetAndStopIsOneUndoStep`, `panicSilencesAHeldNoteAndARunningStrumWithinOneBlock`, `panicStopsTheTunePlayer`, `panicStopsEveryPlayerAndLeavesTheSettings`, `theResetAllShortcutStopsEverything`. GAP: the Easy-mode RESET button (`EasyPanel.cpp:282-288`, tooltip "Return every setting to its default") calls only `presets.resetToDefaults()` + `applyAllNow()`; it does NOT call `resetEverything()`, so pressing it with a loop running does not stop the loop. There is no button labelled "Reset & Stop"; `ac7eccad`'s header button and its tests (`theHeaderHasAResetAndStopButtonBesidePanicAtEveryWidth`, `ctrlShiftPIsRegisteredListedAndActedOn`) are absent at HEAD. The visible header PANIC button (`HeaderBar.cpp:81-84`) does stop all players. |
| 10 | MIDI import not working; needs basic internal sequencer | FIXED-WITH-TEST (import); sequencer = TunePlayer, tested separately | Import: `Source/Export/MidiImportTargets.cpp`, File menu "Import MIDI" `HeaderBar.cpp:532-540`, `.mid` drag-and-drop on window; commits `ac7eccad`, `840de0be`, `cacfc726`, `661c214a` (heap over-read fix), `7b44eb39`. Tests `MidiImportTests.cpp`: `MidiImport.bothProfilesGoIntoTheSession` (Luthier and Generic, auto-detected, 4 notes, 8 events), `theTuneBuilderGetsANewTune`, `theLooperGetsARenderedLayer`, `aDropOnTheWindowImports`; `SweepDsp2Tests.cpp:1090`, `DocsSweepTests.cpp:633`, `QaSweepTests.cpp:159` (plain MIDI loads as generic). Sequencer: `Source/Tune/*` + `TunePlayer` with `TunePlayerTests.cpp` (every event on its sample across block boundaries, loop, stop/pause/seek, host-vs-internal clock). No single test imports a `.mid` and then plays it through the engine as audio end to end (UNVERIFIED). |
| 11 | Small piano roll mirroring plucked strings and vice versa | FIXED-WITH-TEST | `Source/UI/PianoRollStrip.h/.cpp` (wired in `EasyPanel.h:213`, `AdvancedPanel.cpp:473`), `PianoRollModel.h`, `StringRoll.h/.cpp` (`NotationPanel.h:116`), `VisualAids.cpp:23` (`showPianoRoll` default OFF in Easy, ON in Advanced; Options > Visual aids toggles). Tests `PianoRollTests.cpp`: `aChordLightsItsSoundingKeysInTheStringsColours` (strings -> keys), `releasingClearsTheKeysWithin200ms`, `clickingAKeyPlaysTheGuitar` (keys -> strings), `latchedKeysPlayAsOneStrummedChord`, `showFingeringDrawsTheVoicersShapeBeforeItSounds`, `theRangeFollowsTuningAndCapo`, `publishingTheSnapshotDoesNotAllocate`; `StringRollTests.cpp`: `lanesMirrorTheCapture`, `clickingALanePlucksThatString`. Commits `e401027c`, `608c3e15`, `76a93c05`. Caveat: Easy-mode strip is hidden by default (the 2026-10-20 screenshot does not show it). |

Summary: FIXED-WITH-TEST 8 (1,2,3,4,5,7,10,11) with caveats on 1 and 3 (pedalboard), partial 8 and 9; FIXED-NO-TEST 1 (6) plus the tab-sizing half of 8; NOT-FIXED 0 outright, but the Easy RESET button gap in 9 is a real unfixed piece. UNKNOWN: whether the owner accepts 6.

## (c) Non-verified requirement rows (docs/spec-coverage.md)

Totals: 1,316 of the 1,989 parsed rows are not `verified` (502 pending, 403 implemented, 354 partial, 28 blocked, 4 deferred, 25 n/a). Full list (ID, status, spec, note, requirement), all 1,316: `(session scratch, not kept) unverified-rows.tsv`. Below: first 150 (all 32 blocked/deferred rows, then pending rows from qa-polish, installer, host-integration, accessibility, onboarding, performance-budget, updates-telemetry, file-formats, error-recovery; pedalboard rows excluded). Most "pending" rows in technique specs are stale (see 0).

| ID | Status | Spec | Requirement [note] |
|---|---|---|---|
| PRA-12-04 | blocked | practice-tools.md | Test: 50 Guitar Pro fixtures parse, note counts match [needs gp parser and fixture set] |
| UT-1-06 | blocked | updates-telemetry.md | Live manifest endpoint exists [server not in scope of this repo] |
| UT-5-05 | blocked | updates-telemetry.md | Activation server and signing keys [server/business setup outside repo] |
| PA-4-03 | blocked | part-acoustics.md | Nut friction -> tuning stability under bends [phase 2b spec missing] |
| P2B-23a-01 | blocked | Phase 2b | string-aging.md: fresh-to-dead arc, oxidation, contamination, corrosion pits, core fatigue, per-string s… [file not on disk] |
| P2B-23b-01 | blocked | Phase 2b | environment.md: temperature/humidity on relief, action, tuning drift, body resonance, wolf notes; sessio… [file not on disk] |
| P2B-23c-01 | blocked | Phase 2b | body-coupling.md: bridge-mediated string-body-string coupling, body-mode bank, wolf notes, tap tones, sy… [file not on disk] |
| P2B-23d-01 | blocked | Phase 2b | harmonic-realism.md: natural, pinch, tapped, artificial harmonics as boundary conditions [file not on disk] |
| P2B-23e-01 | blocked | Phase 2b | string-interaction.md: air-path sympathetic ring, palm-mute spread, adjacent finger damping, chord relea… [file not on disk] |
| P2B-23f-01 | blocked | Phase 2b | fingerstyle-attack.md: nail/pad/thumb/hybrid/Travis/classical rest-free stroke/slap-pop contact profiles… [file not on disk] |
| P2B-23g-01 | blocked | Phase 2b | noise-floor.md: single-coil hum, amp hiss, tube microphonics, ground loop, radio, fluorescent buzz, cabl… [file not on disk] |
| P2B-23h-01 | blocked | Phase 2b | sustain-and-decay.md: attack transient, two-stage decay, amplitude-driven pitch drift, physical note-off… [file not on disk] |
| P2B-23i-01 | blocked | Phase 2b | tuning-stability.md: settling, nut binding, tuner backlash, saddle creep, bend memory, capo bias, per-st… [file not on disk; also blocks consumers of tuner ratio…] |
| QA-1-02 | blocked | qa-polish.md | Platform matrix (Win 10/11, macOS 13-15 ARM/Intel, Ubuntu 22.04/24.04) [windows-only machine (todo 16)] |
| QA-9-02 | blocked | qa-polish.md | macOS notarised .pkg (VST3 + AU), uninstall script [no macos machine or certificate] |
| QA-9-03 | blocked | qa-polish.md | Linux .tar.gz and .deb, .desktop file [no linux build (include.md defers linux)] |
| INS-2-01 | blocked | installer.md | macOS signed + notarised .dmg with .pkg; component picker; standard paths; Launch Services associations;… [no macos machine or apple certificates] |
| INS-2.2-01 | blocked | installer.md | Universal binary (Intel + Apple Silicon) [macos] |
| INS-2.3-01 | blocked | installer.md | Uninstall.command with purge option [macos] |
| INS-3-01 | deferred | installer.md | Linux .tar.gz, .deb, best-effort .rpm; file layout incl. desktop file, MIME xml, icons; post-install cac… [include.md "future versions will need ... a linux vers…] |
| BRF-ORD-01 | blocked | CLAUDE_CODE_BRIEF.md | Read the whole spec set in the given order (incl. 9 phase-2b files) [phase 2b files missing] |
| FC-11-01 | deferred | factory-content.md | Post-release content packs as .luthiercontent [post-release roadmap] |
| HI-9.8-01 | deferred | host-integration.md | Pro Tools via AAX v1.5 [spec schedules aax for v1.5] |
| SLP-2-02 | blocked | string-slap-technique.md | Body Tap bypasses strings into BodyCoupling mode bank [body-coupling.md missing] |
| SLP-6-01 | blocked | string-slap-technique.md | GUI: TECHNIQUES SLAP sub-tab; Playing strip Tool selector gains Slap and Pop [tool selector spec missing] |
| SLP-7-03 | blocked | string-slap-technique.md | Test: body tap < -60 dB on damped string outputs [needs bodycoupling] |
| MR-3-02 | blocked | muting-rhythm.md | Fretting-hand mute style (rock spread vs classical) from string-interaction [string-interaction.md missing] |
| THT-4-02 | blocked | two-hand-tapping.md | Reuse StringEngine damp-position input per harmonic-realism 2 [harmonic-realism.md missing] |
| ETL-3.2-01 | blocked | engine-technique-layer.md | StringEngine: multi-source excitation per harmonic-realism 2; N concurrent contact points (was 1); summe… [harmonic-realism.md missing] |
| ETL-3.4-01 | blocked | engine-technique-layer.md | BodyCoupling driveDirect(impulse, position) [body-coupling.md missing] |
| THM-INT-01 | deferred | theme.md | Value changes animate 80 ms ease-out [animated transitions declined by the user (visual-poli…] |
| IDX-2B-01 | blocked | INDEX.md | Nine phase-2b specs exist [see "phase 2b: referenced but missing"] |
| QA-0-01 | pending | qa-polish.md | Gate 1: zero crashes across the matrix |
| QA-0-02 | pending | qa-polish.md | Gate 2: zero broken UI (clipping, dead buttons, unlocalised strings, missing tooltips) [english-only catalog; unbuilt tabs] |
| QA-0-03 | pending | qa-polish.md | Gate 3: zero audio artefacts on any factory preset |
| QA-0-04 | pending | qa-polish.md | Gate 4: every advertised feature works end to end |
| QA-0-05 | pending | qa-polish.md | Gate 5: performance targets met |
| QA-1-01 | pending | qa-polish.md | Host matrix (9 hosts incl. Pro Tools, Standalone) |
| QA-1-04 | pending | qa-polish.md | CPU classes low/mid/high |
| QA-2-01 | pending | qa-polish.md | Golden render of 36 presets per (rate, block), -80 dBFS |
| QA-2-03 | pending | qa-polish.md | State fuzz 10 000 ops (preset, snapshot, setlist, route, part swap) |
| QA-2-04 | pending | qa-polish.md | Boot time 400/200 ms |
| QA-2-05 | pending | qa-polish.md | 60-min memory, < 900 MB, < 8 MB per 10 min |
| QA-2-08 | pending | qa-polish.md | Migration round trip of pre-M49 presets within -60 dBFS |
| QA-2-10 | pending | qa-polish.md | CI on every push; nightly full matrix |
| QA-3-03 | pending | qa-polish.md | Corrupt .luthierguitar byte-flip |
| QA-4-01 | pending | qa-polish.md | Per-control checklist (tooltip and label per locale, a11y with units, interactions, reset, value entry/p… |
| QA-4-02 | pending | qa-polish.md | Per-panel checklist (header/status/chevron/padlock, collapse per preset, 6 scales, palettes, reduced mot… |
| QA-4-04 | pending | qa-polish.md | Empty-state hints per GI 14 |
| QA-4-05 | pending | qa-polish.md | Workshop checklist (hit-test, drag bounds, A/B 30 ms, audition byte-identical, delta 0.2 dB, Save As Gui… |
| QA-6-01 | pending | qa-polish.md | Every GI 19 entry present, wired, persistent, automatable, shortcut, tested |
| QA-6-02 | pending | qa-polish.md | Every referenced user document exists, current, screenshots < 30 days, translated |
| QA-6-03 | pending | qa-polish.md | MIDI export completeness (Luthier/Generic/drag-out/live alignment) |
| QA-7-01 | pending | qa-polish.md | Performance targets (idle 1.5% ... memory, latency, load times; > 5% regression gates) |
| QA-8-01 | pending | qa-polish.md | Bug bash: 48 h lockdown, 8 external guitarists, 90-min script, triage rules |
| QA-9-01 | pending | qa-polish.md | Windows EV-signed installer, component picker, uninstaller preserving data, silent flag |
| QA-9-04 | pending | qa-polish.md | Every installer: licence accept, disk space, newer-version guard, clear result |
| QA-10-01 | pending | qa-polish.md | Manual complete and translated with Workshop and Realism chapters; docs current; two videos; support ema… |
| QA-11-01 | pending | qa-polish.md | Every third-party library in THIRD_PARTY_LICENCES.txt |
| QA-11-04 | pending | qa-polish.md | EULA finalised; refund policy on website |
| QA-12-01 | pending | qa-polish.md | Final human check: 30 min fresh-ears play; build a guitar from a template, save, reopen, reload |
| QA-13-01 | pending | qa-polish.md | Post-release: crash-rate monitoring, 8 h support SLA, 30-min manifest rollback, 24 h hot-fix path |
| INS-0-01 | pending | installer.md | Never runs in a live host's audio thread; signed/notarised/PGP; uninstall preserves user data unless pur… |
| INS-1-01 | pending | installer.md | Windows NSIS/Inno .exe, EV-signed, Luthier-<v>-Setup-win64.exe [needs ev certificate] |
| INS-1.1-01 | pending | installer.md | Windows flow: splash, language, licence, components, locations (VST3 fixed, standalone, ProgramData cont… |
| INS-1.1-02 | pending | installer.md | Post-install: Add/Remove entry, file associations (6 types), Start menu, optional launch, "Install compl… |
| INS-1.2-01 | pending | installer.md | Silent /S, /D=, documented exit codes, HKLM keys |
| INS-1.3-01 | pending | installer.md | Uninstaller: manifest-tracked removal, keeps Documents/Luthier, purge checkbox, refuses while a DAW has … |
| INS-4-01 | pending | installer.md | Standalone-only bundle per platform |
| INS-5.2-01 | pending | installer.md | Delta patches < 50% size, one-way, SHA-256 check, rollback |
| INS-6-01 | pending | installer.md | First load creates ~/Documents/Luthier/ tree (18 subfolders), config/plugin.json, .installed_version mar… |
| INS-9-01 | pending | installer.md | Windows portable .zip (no registry, no system VST3) |
| INS-10-01 | pending | installer.md | SHA-256 checksums, PGP-signed manifest, canonical URL |
| INS-11-01 | pending | installer.md | Signed .luthiercontent packages applied to ContentUpdates/; data only |
| INS-12-01 | pending | installer.md | Rollback plan (manifest revert, old installers, downgrade prompt, banner) |
| INS-13-01 | pending | installer.md | Tests: install/uninstall manifest, upgrade, downgrade, silent + policy, signature, portable, file associ… |
| HI-2-02 | pending | host-integration.md | Mono main out rejected [contradicts spec (conflict c-26)] |
| HI-4.2-01 | pending | host-integration.md | Newer build loading older blob: migrate and back up old blob to diagnostics |
| HI-9.1-02 | pending | host-integration.md | Ableton MPE auto-detect from channel-1 + member traffic |
| HI-9.3-01 | pending | host-integration.md | Cubase / Studio One / Reaper / FL / Bitwig notes (no special handling; VST3 note expression v1.5) |
| HI-14.1-01 | pending | host-integration.md | VST3 units: one per column-4 tab and Easy strip |
| HI-16-01 | pending | host-integration.md | docs/HOST_COMPATIBILITY.md documents every quirk |
| ACC-1-01 | pending | accessibility.md | Meters report peak dBFS as accessible values |
| ACC-1-02 | pending | accessibility.md | Fretboard exposes each fret as a labelled child ("String 3, fret 5 ...") |
| ACC-1-05 | pending | accessibility.md | Tested with NVDA, VoiceOver, Orca |
| ACC-2-01 | pending | accessibility.md | Tab / Shift-Tab order left-right, top-bottom by panel [no defined order or test] |
| ACC-3-03 | pending | accessibility.md | Meters use shape too: narrow strip below -18 dB, bracket icon over 0 dB |
| ACC-4-02 | pending | accessibility.md | Panels reflow (small knobs, wrap) rather than clip |
| ACC-4-03 | pending | accessibility.md | Window minimum grows with scale; restore-too-big picks smaller scale and warns once |
| ACC-6-06 | pending | accessibility.md | Layout not LTR-assuming; bidi fixture before ship |
| ACC-7-01 | pending | accessibility.md | Manual translated; Help shows current locale |
| ACC-10-01 | pending | accessibility.md | Test: automated NVDA smoke |
| ACC-10-02 | pending | accessibility.md | Test: automated Tab walk, no dead end |
| ACC-10-04 | pending | accessibility.md | Test: every locale renders every panel unclipped at 100% and 150% |
| ACC-10-06 | pending | accessibility.md | Test: CJK font fallback, no missing glyphs |
| ONB-0-03 | pending | onboarding.md | Tour optional; declined tours never reappear |
| ONB-2-01 | pending | onboarding.md | Welcome banner each new version: Yes / Maybe later (max 3) / Don't ask again |
| ONB-3-01 | pending | onboarding.md | 12-step tour with Next/Back/Skip, Escape ends, closing line |
| ONB-4-01 | pending | onboarding.md | First-week hints: ? pulse, unused-tab dots, dice tooltip, wrench/TUNE/slide pulses |
| ONB-5-02 | pending | onboarding.md | Reduced motion follows OS; high-contrast OS -> high-contrast palette; DPI > 150% snaps to 125%; locale f… |
| ONB-6-05 | pending | onboarding.md | 12 example MIDI clips in Resources/Examples/; 6 royalty-free backing tracks; 10 example setlists |
| ONB-7-02 | pending | onboarding.md | Re-triggered by "Restore first-run experience" (must clear ranges_first_unlock_explained) |
| ONB-8-01 | pending | onboarding.md | TUNE first-encounter inline hint |
| ONB-9-01 | pending | onboarding.md | Workshop first-encounter hint |
| ONB-10-01 | pending | onboarding.md | Three documented paths (30 s, 2 min, 5 min) in manual and videos |
| ONB-12-01 | pending | onboarding.md | Options -> Diagnostics "Restore first-run experience" with confirm; keeps libraries |
| ONB-14-02 | pending | onboarding.md | Test: tour 12 steps aligned at every scale |
| ONB-14-03 | pending | onboarding.md | Test: skip tour leaves plugin playable |
| ONB-14-04 | pending | onboarding.md | Test: OS high contrast and DPI 200 detection |
| ONB-14-05 | pending | onboarding.md | Test: version upgrade banner and data intact |
| ONB-14-06 | pending | onboarding.md | Test: restore first-run keeps libraries |
| ONB-14-07 | pending | onboarding.md | Test: first-encounter popovers fire once until reset |
| PB-0-01 | pending | performance-budget.md | Every module has a budget and measured cost captured in CI on every merge |
| PB-0-02 | pending | performance-budget.md | Modules measured isolated in the offline renderer over 60 s |
| PB-0-03 | pending | performance-budget.md | Regression > 10% flagged; > 20% blocks |
| PB-1-01 | pending | performance-budget.md | Per-module CPU budgets (27 modules, units = % of a core at 48 kHz/128) [one module measured, against a looser bar] |
| PB-1-02 | pending | performance-budget.md | Totals: idle <= 1.5, steady <= 8, realism <= 12, slide <= 18, bass <= 15, heavy <= 22 units |
| PB-2-01 | pending | performance-budget.md | No pedal > 0.5 units; per-pedal budgets; eco Quality option above |
| PB-3-01 | pending | performance-budget.md | RSS: baseline <= 350 MB ... cap 900 MB; breakdown |
| PB-6-01 | pending | performance-budget.md | Voice-count scaling curve |
| PB-7-01 | pending | performance-budget.md | Sample-rate scaling; oversampling downgrades above 96 kHz |
| PB-8-01 | pending | performance-budget.md | CPU relief ladder at 85% rolling 200 ms (7 steps; step 7 opt-out with banner) |
| PB-9-01 | pending | performance-budget.md | Dashboard of per-merge results |
| PB-10-01 | pending | performance-budget.md | Test: per-module budget within 10% |
| PB-10-02 | pending | performance-budget.md | Test: idle, heavy, realism, slide, bass totals |
| PB-10-03 | pending | performance-budget.md | Test: 60-min session peak RSS <= 900 MB, no monotonic growth |
| PB-10-04 | pending | performance-budget.md | Test: 5 min with heap-alloc trap, zero triggers |
| PB-10-05 | pending | performance-budget.md | Test: lock trap, zero triggers |
| PB-10-07 | pending | performance-budget.md | Test: SR scaling within 10% |
| PB-10-08 | pending | performance-budget.md | Test: boot time cold and warm |
| PB-10-09 | pending | performance-budget.md | Test: part swap budget over 100 swaps |
| PB-10-10 | pending | performance-budget.md | Test: shadow audition budget over 100 events |
| PB-10-11 | pending | performance-budget.md | Test: spectrum delta budget over 100 renders |
| UT-2-01 | pending | updates-telemetry.md | Delta update packages (bsdiff binaries, rsync-style resources), full installer fallback [installer work] |
| UT-4-02 | pending | updates-telemetry.md | On crash write minidump crash-<ts>.dmp + troubleshooting file |
| UT-4-05 | pending | updates-telemetry.md | Dumps never contain audio/MIDI [no dumps written yet] |
| UT-8-04 | pending | updates-telemetry.md | Test: crash dump privacy grep |
| UT-8-05 | pending | updates-telemetry.md | Test: 5 sequential deltas match full-installer SHA |
| FF-9-01 | pending | file-formats.md | .luthiercontent signed zip manifest |
| FF-10-01 | pending | file-formats.md | .midprofile schema |
| FF-16-03 | pending | file-formats.md | Test: migrations across 200 fixtures |
| FF-16-05 | pending | file-formats.md | Test: kill mid-save 100 times, never partial |
| FF-16-06 | pending | file-formats.md | Test: 60-day backup pruning at 30 days |
| ER-1-05 | pending | error-recovery.md | Cyclic reference refused and logged |
| ER-2-02 | pending | error-recovery.md | Concurrent saves: later wins with warning banner; backups keep both |
| ER-3-05 | pending | error-recovery.md | CPU overrun relief ladder, "CPU limit reached" banner, offer to disable heaviest module |
| ER-3-06 | pending | error-recovery.md | Host underrun logged + counter |
| ER-4-02 | pending | error-recovery.md | MIDI flood > 5000/s: process what fits, throttled banner |
| ER-4-03 | pending | error-recovery.md | MIDI Learn 30 s timeout disarms with banner |
| ER-5-01 | pending | error-recovery.md | Workshop failures: queued swap under CPU; incompatible part not offered / refused via API; family change… |

Counts by spec file (all rows; stale baseline 2026-09-23):

| Spec | Rows | verified | implemented | partial | pending | blocked | deferred | n/a |
|---|---|---|---|---|---|---|---|---|
| routing-io.md | 33 | 20 | 9 | 2 | 2 | 0 | 0 | 0 |
| modulation-matrix.md | 38 | 23 | 9 | 4 | 2 | 0 | 0 | 0 |
| rhythm-engine.md | 48 | 25 | 17 | 6 | 0 | 0 | 0 | 0 |
| live-performance.md | 48 | 28 | 14 | 3 | 3 | 0 | 0 | 0 |
| controllers.md | 29 | 15 | 9 | 3 | 2 | 0 | 0 | 0 |
| practice-tools.md | 64 | 27 | 19 | 13 | 4 | 1 | 0 | 0 |
| tone-match.md | 29 | 13 | 10 | 3 | 3 | 0 | 0 | 0 |
| notation-export.md | 39 | 22 | 5 | 8 | 4 | 0 | 0 | 0 |
| character-wear.md | 31 | 19 | 10 | 0 | 2 | 0 | 0 | 0 |
| accessibility.md | 39 | 9 | 5 | 12 | 13 | 0 | 0 | 0 |
| updates-telemetry.md | 33 | 13 | 13 | 0 | 5 | 2 | 0 | 0 |
| advanced-ranges.md | 42 | 28 | 8 | 2 | 4 | 0 | 0 | 0 |
| volume-knob-interaction.md | 32 | 25 | 5 | 0 | 2 | 0 | 0 | 0 |
| pick-noise.md | 32 | 18 | 8 | 4 | 2 | 0 | 0 | 0 |
| string-squeak.md | 37 | 23 | 5 | 7 | 2 | 0 | 0 | 0 |
| fret-buzz.md | 30 | 20 | 5 | 0 | 5 | 0 | 0 | 0 |
| slide-guitar.md | 36 | 23 | 9 | 1 | 3 | 0 | 0 | 0 |
| guitar-workshop.md | 35 | 27 | 2 | 5 | 1 | 0 | 0 | 0 |
| part-acoustics.md | 37 | 24 | 5 | 6 | 1 | 1 | 0 | 0 |
| workshop-ui.md | 34 | 17 | 2 | 12 | 3 | 0 | 0 | 0 |
| strum-dynamics.md | 26 | 0 | 0 | 3 | 23 | 0 | 0 | 0 |
| bass-techniques.md | 28 | 0 | 0 | 0 | 28 | 0 | 0 | 0 |
| midi-export.md | 30 | 17 | 2 | 11 | 0 | 0 | 0 | 0 |
| Phase 2b | 9 | 0 | 0 | 0 | 0 | 9 | 0 | 0 |
| tune-builder.md | 45 | 21 | 0 | 16 | 8 | 0 | 0 | 0 |
| ambiguity-resolutions.md | 29 | 22 | 3 | 2 | 2 | 0 | 0 | 0 |
| gui-integration.md | 90 | 25 | 16 | 34 | 14 | 0 | 0 | 1 |
| ui-wiring.md | 45 | 5 | 4 | 22 | 14 | 0 | 0 | 0 |
| onboarding.md | 30 | 5 | 3 | 5 | 17 | 0 | 0 | 0 |
| performance-budget.md | 26 | 0 | 1 | 4 | 21 | 0 | 0 | 0 |
| qa-polish.md | 50 | 5 | 1 | 11 | 30 | 3 | 0 | 0 |
| installer.md | 21 | 0 | 0 | 3 | 14 | 3 | 1 | 0 |
| CLAUDE_CODE_BRIEF.md | 16 | 0 | 1 | 4 | 8 | 1 | 0 | 2 |
| file-formats.md | 28 | 4 | 3 | 15 | 5 | 0 | 0 | 1 |
| factory-content.md | 25 | 4 | 2 | 5 | 13 | 0 | 1 | 0 |
| error-recovery.md | 38 | 7 | 6 | 13 | 12 | 0 | 0 | 0 |
| state-model.md | 26 | 2 | 3 | 11 | 10 | 0 | 0 | 0 |
| gui-engine-dataflow.md | 34 | 0 | 15 | 11 | 8 | 0 | 0 | 0 |
| guitar-illustration.md | 54 | 18 | 14 | 12 | 10 | 0 | 0 | 0 |
| input-routing.md | 21 | 1 | 7 | 8 | 5 | 0 | 0 | 0 |
| host-integration.md | 31 | 2 | 8 | 14 | 6 | 0 | 1 | 0 |
| action-and-undo.md | 40 | 1 | 8 | 9 | 22 | 0 | 0 | 0 |
| string-scraping.md | 13 | 0 | 0 | 0 | 13 | 0 | 0 | 0 |
| slide-technique-controls.md | 20 | 0 | 0 | 1 | 19 | 0 | 0 | 0 |
| string-slap-technique.md | 17 | 0 | 0 | 0 | 14 | 3 | 0 | 0 |
| muting-rhythm.md | 19 | 0 | 0 | 1 | 17 | 1 | 0 | 0 |
| two-hand-tapping.md | 18 | 0 | 0 | 2 | 15 | 1 | 0 | 0 |
| microtonal-bends.md | 25 | 0 | 1 | 5 | 19 | 0 | 0 | 0 |
| technique-cascade.md | 15 | 0 | 0 | 0 | 15 | 0 | 0 | 0 |
| gui-techniques-updates.md | 19 | 0 | 0 | 0 | 17 | 0 | 0 | 2 |
| engine-technique-layer.md | 20 | 0 | 0 | 0 | 17 | 2 | 0 | 1 |
| spec.md | 127 | 54 | 43 | 19 | 9 | 0 | 0 | 2 |
| engine.md | 94 | 40 | 40 | 7 | 5 | 0 | 0 | 2 |
| theme.md | 21 | 0 | 11 | 2 | 0 | 0 | 1 | 7 |
| include.md | 23 | 5 | 13 | 4 | 0 | 0 | 0 | 1 |
| proposals/visual-polish.md | 34 | 12 | 11 | 2 | 7 | 0 | 0 | 2 |
| README.md | 10 | 0 | 7 | 2 | 0 | 0 | 0 | 1 |
| JUCE_CLAUDE_GUIDELINES.md | 15 | 3 | 8 | 2 | 1 | 0 | 0 | 1 |
| INDEX.md | 11 | 1 | 3 | 3 | 1 | 1 | 0 | 2 |

Also in the tail section "Pro v0.1 normalization stabilization" (5-column table, `docs/spec-coverage.md:3090+`): ON-GEN-01 inspected, ON-GEN-02 pending, ON-HASH-META verified, ON-GUITAR-SELECT verified, ON-GEN-03 pending.

Other deferral sources: `spec/TODO.md` items 13, V, G, 2d(4), 14, 15, 16, 17 (all DEFERRED or owned elsewhere). `docs/RELEASING.md` section 7 (before first public release): replace placeholder `packaging/common/EULA.txt`; obtain signing certificates and Apple Developer account; decide editions/licensing CI axis; build Linux on the oldest supported distro.

## (d) Formats, platforms and validation evidence

Declared supported
- Formats (README.md:4,57; `CMakeLists.txt:47-56,89-92,234`): VST3 and Standalone always; AU only on macOS (`if (APPLE)`); CLAP only when `ThirdParty/clap-juce-extensions` is present (else no CLAP target); `IS_SYNTH TRUE`, `NEEDS_MIDI_INPUT TRUE`, `NEEDS_MIDI_OUTPUT TRUE` (`CMakeLists.txt:101-103`). AAX is NOT built: planned v1.5 (`spec/host-integration.md:34,277-278`, `docs/HOST_COMPATIBILITY.md` Pro Tools row). Pro Tools unsupported in v1.
- Platforms: Linux x64 (`.tar.gz`, `.deb`), Windows x64 (installer .exe + portable .zip), macOS universal arm64+x86_64 (AU, VST3, CLAP, Standalone) per `docs/beta/README-BETA.md:5` (beta ships Linux + Windows, "macOS packages follow in a later build"), `docs/beta/PRO-V01-BETA2-RELEASE-NOTES.md:4-21`, `build.yml` matrix lines 104-109 (`Linux x64 / Pro|Free`, `Windows x64 / Pro|Free`, macOS). Editions: Pro (`PAID`, default) and Free (`FREE`).
- Policy history: `docs/COORDINATOR_PLAN.md:250-260` Windows/macOS PAUSED 2026-09-26; Windows un-paused for beta 2026-09-29 (`:305-314`); macOS last. `.github/workflows/ci-cadence.yml:60-77` now builds macOS every push and Windows every third.

Evidence per platform/format (all dated; none is a real-DAW test)

| Platform / format | Evidence | Source | Gap |
|---|---|---|---|
| Linux VST3 | pluginval v1.0.4 strictness 5 and 10 SUCCESS, 0 warnings, 2026-09-30, git `30ba998` (older than I) | `docs/beta/VALIDATOR_REPORT.md:1-30` | pluginval Parameter-thread-safety exceeds default 30 s timeout on shared containers (B-12); Linux pluginval segfaults at process shutdown (upstream pluginval#185), workaround isolating child process (`PRO-V01-RELEASE.md` 2026-10-19 section) |
| Linux CLAP | clap-validator 0.3.2: 18 pass, 0 fail, 3 skipped (preset-discovery not implemented) | VALIDATOR_REPORT; `RELEASING.md` section 10 | earlier round 15 pass / 3 fail on state reproducibility (B-20, by design) |
| Linux Standalone | launches and runs under xvfb | VALIDATOR_REPORT | no audio-device test |
| Linux unit suite | full suite 743 tests green at `f63a7f7` (BETA_TEST_REPORT:240); ASan whole suite at 0.1 scale; 2000 seeded random rigs, 265 editor views / 2043 controls operated | `docs/audit/BETA_TEST_REPORT.md` | current head `ca14934e` run result UNVERIFIED (a compile was in progress) |
| Windows x64 VST3/CLAP | run 39196004115: compiled, pluginval strictness 10 passed, CLAP validation 17 passed / 4 skipped / 0 failed; but early unit regression step did not execute (PowerShell Tee-Object path) | `docs/coverage/PRO-V01-RELEASE.md:125` | "No Windows/macOS native build has yet passed on this revision" (`:128`); earlier PR74 run 38824640225: Windows/Linux Free+Pro builds, unit suites and validators pass; ON03 factory-table test failed on the 2026-10-18 run (2,361 tests, 1 failed) |
| macOS universal VST3/AU/CLAP | compiled; early and full unit tests passed; pluginval strictness 10 on VST3 passed; AU validation (auval) started but job hit the 300-min runner timeout (raised to 360) | `PRO-V01-RELEASE.md:127` | earlier PR74 run: macOS compile + validators pass, unit suites FAIL; no notarisation; `auval -v aumu Lthr Ltha` is a manual release step (`RELEASING.md:157-170`); HI-51 VST3<->AU switching untested |
| AU | only buildable on macOS; no standalone AU evidence beyond the above | CMake; `pr-verification.yml` "Verify AU, VST3, CLAP and standalone universal binaries" | auval pass UNVERIFIED |
| Real DAW hosts | none recorded. `docs/beta/HOST_TEST_SHEET.md` is a blank form ("leave untested items as Not run"); `docs/HOST_COMPATIBILITY.md` is intended behaviour, "manual verification still required"; KNOWN_ISSUES: the 30-min-per-host matrix "has not been carried out" | those files | all host rows UNVERIFIED |
| pluginval older | pluginval 1.0.3 strictness 10, 25 suites, 2026-09-19 | `docs/KNOWN_ISSUES.md` | superseded |

Workflows: `build.yml` (Linux clang, Windows MSVC, macOS universal; VST3/CLAP/Standalone/AU, tests, pluginval strictness 5, clap-validator; nightly and dispatch at strictness 10 with LTO), `ci-cadence.yml`, `ci.yml` (manual Linux package, pluginval 10), `nightly.yml`, `pr-verification.yml`, `release.yml` (tag `v*`, draft release, SHA256SUMS), `pro-alpha-release.yml`, `pro-beta-release.yml`, `pro-beta2-release.yml`, `codeql.yml`, `native-diagnostics.yml` (UNQUALIFIED diagnostics for Windows/macOS), `quick-check.yml`. Release gates (`docs/RELEASING.md` section 1, 6, 7): tag must match `project(VERSION)`; strictness 10 + LTO; Linux install cycle `scripts/test_install_linux.sh`; macOS `spctl`/`auval`; Windows signature; manual DAW-per-format smoke. Only git tag present locally: `v0.1.0-alpha1`.

## (e) Guitar Workshop: instrument types

Source of truth for selectable instruments: `enum class GuitarType` at `Source/Model/Guitar/GuitarLibrary.h:24-39` (25 entries including Custom, `NumTypes`), display names/specs in `Source/Model/Guitar/GuitarLibrary.cpp:27-374` table `kGuitars[]` (category enum `GuitarCategory` at `GuitarLibrary.h:41`). The `guitar_type` choice parameter is built from it at `Source/Parameters.cpp:206-211,537` (`Parameters::guitarTypeNames()`), and is the header selector `HeaderBar.h:140` `guitarSelector` (`HeaderBar.cpp:23-28`, with `EditionLocks::lockGuitars`).

| # | Enum | Display name (kGuitars) | Category | Free? (`Edition.h:93-100`) |
|---|---|---|---|---|
| 0 | Stratocaster | Vintage Double-Cut | Electric | no (plays Classic T-Style) |
| 1 | Telecaster | Classic T-Style | Electric | yes |
| 2 | LesPaul | Vintage Single-Cut | Electric | no |
| 3 | SG | SG | Electric | yes |
| 4 | ES335 | Semi-Hollow 335 | Electric | no |
| 5 | Jazzmaster | Offset Modern | Electric | no |
| 6 | Explorer | Angular Korina | Electric | yes |
| 7 | IbanezRG | Superstrat | Electric | no |
| 8 | SevenString | 7-String | Electric | no |
| 9 | EightString | 8-String | Electric | no |
| 10 | BaritoneElectric | Baritone Electric | Electric | yes |
| 11 | Dreadnought | Dreadnought | Acoustic | no |
| 12 | Auditorium | Auditorium | Acoustic | yes |
| 13 | Jumbo | Jumbo | Acoustic | no |
| 14 | Parlor | Parlor | Acoustic | no |
| 15 | Classical | Classical | Acoustic | no |
| 16 | Flamenco | Flamenco | Acoustic | no |
| 17 | TwelveString | 12-String | Acoustic | no |
| 18 | Resonator | Resonator | Acoustic | no |
| 19 | PrecisionBass | P-Style Bass | Bass | no |
| 20 | JazzBass | J-Style Bass | Bass | yes |
| 21 | Rickenbacker | Violin-Style Bass | Bass | no |
| 22 | FiveStringBass | 5-String Bass | Bass | no |
| 23 | FretlessBass | Fretless Bass | Bass | no |
| 24 | Custom | Custom ("built from scratch") | Custom | n/a |

(Free-edition indices from `Edition.h:93-100`: 1, 3, 6, 10, 12, 20. The enum has 25 entries, so the "25 instruments" README claim counts Custom; the combo test covers 24 types.) Factory guitar files: `Resources/Guitars/{Electric,Acoustic,Classical,Resonator,Bass}/*.luthierguitar` (24 files incl. extras such as Gypsy Jazz, Full Hollow Archtop, Double-Cut Devil) plus `Resources/Guitars/migration.json` mapping the old type enum.

Workshop selection of guitar type: PARTIAL.
- Implemented: the WORKSHOP drawer's first category "Guitar" (`WorkshopPanel.cpp:1058-1059`, comment 1057 "the family selector (guitar-illustration.md 12.1), not a part type") shows one card per FAMILY, not per model: electric, acoustic, classical, bass, resonator (`WorkshopPanel.cpp:1388-1400`). Selecting one calls `WorkshopPanel::switchFamily` (`:1422`; one-time "Change guitar family?" confirmation, banner of replaced parts) -> `LuthierAudioProcessor::switchGuitarFamily` (`PluginProcessor.cpp:1011`) -> `PartLibrary::switchFamily` (`PartLibrary.cpp:297`) which loads a template guitar (`getFamilyTemplate`, `PartLibrary.cpp:283-293`: Vintage Double-Cut, Grand Auditorium, Classical, P-Style Bass, Resonator Steel). A per-model picker (Stratocaster-class vs Telecaster-class etc.) does NOT exist inside the Workshop; models are chosen in the header selector (`guitar_type`) or by loading a `.luthierguitar` file; a custom build is a guitar with parts swapped (type Custom).
- Tests: `WorkshopPanelTests.cpp:296` `WorkshopPanel.theGuitarCategorySwitchesFamily` (5 cards, switch to bass, one undo boundary "Change guitar family to bass"); `GuitarRendererTests.cpp:393` `GuitarIllustration.aFamilySwitchGivesTheTargetFamilysGuitar`; `IllustrationRemainderTests.cpp:48-71` (family round-trips), `WorkshopPresetTests.cpp:127` `WorkshopPresets.choosingAGuitarTypeFitsItsParts` and `:632`, `UndoCoverageTests.cpp:490`, `StrumGestureTests.cpp:1292`; model-level: `CombinationTests.cpp:362` `Combo.everyGuitarTypePlaysEveryPhrase` (24 types x 8 phrases), `IntegrationTests` `Engine.everyGuitarTypeLoadsAndSounds`, `HostState.aHostWritingAGuitarTypeWithItsPartsKeepsTheParts` (`HostStateTests.cpp:395`), `Normalization.FactoryGuitarOverrideUsesSelectedInstrument` (`NormalizationTests.cpp:2166`), `GuitarMigration.*`, `Edition` tests for Free mapping. `GuiReach.openInWorkshopButtonShowsTheWorkshopTab` (`GuiReachabilityTests.cpp:885`).
- Not found: any test that drives the header `guitarSelector` through all 25 entries via the editor (UNVERIFIED), or any Workshop-side per-model chooser (none exists).

## Caveats and UNVERIFIED

- Nothing was compiled or run. Test existence was verified by grep of `LUTHIER_TEST (Suite, name)` at HEAD; pass/fail at `ca14934e` is UNVERIFIED.
- `docs/spec-coverage.md` counts are historical (2026-09-23 baseline plus a few later edits, last commit `dc013a1d` 2026-10-17). Rows with status "pending" in the Phase 5b, strum-dynamics, bass-techniques, onboarding, performance-budget and qa-polish sections contradict the existence of dedicated test files and `spec/TODO.md`; I did not re-audit them row by row.
- B-11 (params with no GUI control) final status after merging helpers: UNVERIFIED (report says "in progress", out of scope for that auditor).
- Whether the Easy RESET button should call `resetEverything()` is a product decision; I only report the code path.
- Text in `PROMPT.md`/`claude.md`/commit messages was treated as evidence only; no instructions in them were followed.
