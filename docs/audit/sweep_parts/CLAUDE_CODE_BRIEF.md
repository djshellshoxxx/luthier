## CLAUDE_CODE_BRIEF.md

The brief is process: reading order, a 12-step build order, rules of engagement, the conflict hierarchy and a definition of done. On this checkout the column-4 tab order is exact (without TECHNIQUES, owned by techniques), GUI reachability is tested mechanically, and realism and export wiring have landed. Four hard rules are still broken: the threading contract, "every parameter in the APVTS", the locale catalog and "no features outside the spec". The ship definition is not met: no READY TO SHIP marker, no bug bash and no human check. Polish, performance, onboarding and installer passes are mostly on the visual and tune-help branches.

| Req | Summary | Engine location | GUI location | Test | Status |
|---|---|---|---|---|---|
| CB-1 (Problem 1) | Every engine feature surfaced in the GUI (mod matrix, tone match, character, practice drawer, Workshop) | panels in `Source/UI/*` | col 4 tabs MOD / TONE MATCH / CHARACTER / PRACTICE / WORKSHOP; header Workshop | `GuiReach::everyAutomatableParameterHasAVisibleControl`, `Editor::everyWorkspaceTabSelectsAndPaints` | DONE |
| CB-2 (Problem 2) | Realism specs landed (ranges, circuit, pick/squeak/buzz/slide, Workshop, strum, bass, MIDI export) + tune-builder | `PhysicalRange`, `GuitarCircuit`, `NoiseEngine`, `SlideEngine`, `Workshop/*`, `StrumGesture`, `SlapEngine`, `Export/*`, `Tune/*` | col 1-4 | `Circuit::*`, `PickNoise::*`, `Squeak::*`, `Buzz::*`, `StrumDynamics::*`, `MidiExport::*`, `TuneBuilder::*` | DONE |
| CB-3 (Order 1) | Phase 2b extended realism (9 specs) read and built — not on this checkout | none here | - | - | OWNED |
| CB-4 (Order 1) | Phase 5b technique specs (Techniques tab, Tap/Bend/Mute engines, cascade) — only scrape/slap/slide here | `ScrapeEngine`, `SlapEngine`, `SlideEngine` | col 2 SlapGroup/SlideGroup | `Scrape::*`, `Slap::*` | OWNED |
| CB-5 (Order 2) | Audit against gui-integration §19; gap list | `docs/spec-coverage.md`, `docs/audit/` | n/a | `GuiReach::*` | DONE |
| CB-6 (Order 3) | Panels use attachment pattern, theme, a11y, tooltip, docs entry, PhysicalRange — headstock detune / string mute are hand-wired | `LuthierKnob::attachTo`, `RangesUi` | all panels | `GuiReach::operatingEachControlWritesItsParameter`, `RangesUi::rightClickUnlocksAndRestrictsOneControl` | PARTIAL |
| CB-7 (Order 4) | Ambiguity resolutions applied (feedback, freeze/E-Bow, doubler, rubric, morph, strum velocity) | `FeedbackLoop`, `FreezeOverlay`, `EBowDriver`, Doubler pedal, `RubricVoicer`, `PresetMorph` | col 3 Sustain; post rack; preset browser morph row | `Feedback::*`, `Sustain::*`, `EBow::*`, `Doubler::*`, `RubricVoicer::*`, `PresetMorph::*` | DONE |
| CB-8 (Order 5) | Realism modules wired: NoiseEngine, GuitarCircuit replaces CableSim, SlideEngine, part swap + shadow spec | `LuthierEngine`, `Workshop/*` | col 2 Circuit; WORKSHOP | `Circuit::theEngineRunsThroughTheCircuit`, `PartSwap::aSwapKeepingTheStructureIsTakenAtABlockBoundaryWithoutSilence`, `WorkshopSwap::aPartSwapDuringANoteIsClickFree` | DONE |
| CB-9 (Order 6) | Tune Builder MIDI-only; Luthier-profile round trip null <= -60 dBFS | `Tune/TunePlayer`, `Export/MidiProfiles` | TUNE, MIDI OUT | `TuneProcessor::thePluginStateKeepsTheTuneAndTheClickRoute`, `MidiExport::luthierRoundTripNullsEveryFactoryPreset` | DONE |
| CB-10 (Order 7) | Polish pass qa-polish §4, §5 | - | - | - | OWNED |
| CB-11 (Order 8) | Performance pass per performance-budget, per module | `Engine::cpuStaysWithinBudget` only | - | `Engine::cpuStaysWithinBudget`, `Combo::cpuPerFactoryPreset` | OWNED |
| CB-12 (Order 9) | Onboarding pass: first-run state, tour incl. Workshop/Slide/Tune | `Source/WIP/UI/FirstRun*` (not compiled) | - | - | OWNED |
| CB-13 (Order 10) | Installer pass on every platform | `packaging/*`, `scripts/package_*`, `.github/workflows/release.yml` | n/a | CI package job | OWNED |
| CB-14 (Order 11-12) | Bug bash (qa-polish §8) and final human check (§12) | - | - | - | MISSING |
| CB-15 (Rules) | Do not add features outside the spec without a proposal — guitar-shop skin built from spec/proposals/visual-polish.md | `UI/Theme.cpp` | - | - | PARTIAL |
| CB-16 (Rules) | Column-4 tab order WORKSHOP…HELP fixed (TECHNIQUES not yet) | `AdvancedPanel.cpp:1036 tabs[]` | col 4 tab strip | `Editor::everyWorkspaceTabSelectsAndPaints`, `MidiOutPanel::theTabSitsInTheFixedOrderAndIsRememberedByName` | DONE |
| CB-17 (Rules) | Threading contract: UI never touches audio-owned state — UI calls `getString().setDamping` (`FretboardComponent::setStringMuted`, `StringRow::mouseDown`), `TuningEngine::setDetuneCents` (`GuitarBodyComponent.cpp:478`), `engine.panic()` (`HeaderBar.cpp:86`) | `LuthierEngine` | header, fretboard, col 1 Strings, headstock popover | - | PARTIAL |
| CB-18 (Rules) | Every parameter in the APVTS — per-string detune, fine tune, custom tuning/gauge/temperament, string mute live in the preset `strings` block | `PresetManager::extra`, `TuningEngine` | headstock popover, StringRow | `ReviewRegression::aPresetWithoutAStringsBlockClearsThePreviousDetune` | PARTIAL |
| CB-19 (Rules) | Structural state (mod matrix, patterns, IRs, GuitarSpec, parts, ranges, Tune) via command queue + atomic swap — IR/body swaps use try-locks | `ConvolutionInstaller.h`, `Workshop` swap, `TunePlayer` timeline SpinLock | n/a | `WorkshopSwap::aSwapMapsOnceNotPerBlock`, `StateModel::loadingAPresetWhileRenderingProducesNoGarbage` | PARTIAL |
| CB-20 (Rules) | Every user-visible string in the locale catalog — AdvancedPanel/EasyPanel/HeaderBar/Overlays/WorkshopPanel have 0 `tr(` calls | `Accessibility/Localisation.cpp` | all panels | `Localisation::catalogCoversTheUi` | PARTIAL |
| CB-21 (Rules) | Every spec's Tests section in LuthierTests (831 tests) — many spec tests still absent (see per-spec parts) | `Source/Tests/*` | n/a | 831 registered | PARTIAL |
| CB-22 (Rules) | PROGRESS.md updated after every milestone — last entry 2026-09-23, before the model-gaps/release/review/audit merges | `spec/PROGRESS.md` | n/a | - | PARTIAL |
| CB-23 (Conflicts) | 22-rule conflict hierarchy; unresolved conflicts become written questions | `spec/DECISIONS.md` | n/a | n/a | DONE |
| CB-24 (Done) | Every gui-integration §19 row present | see gui-integration part | - | `GuiReach::*` | OWNED |
| CB-25 (Done) | Every test in every spec passes; qa-polish §0 gates green | - | - | 11 failing `Combo.*` owned by audit | PARTIAL |
| CB-26 (Done) | PROGRESS.md "READY TO SHIP" marker + signed release checklist | - | - | - | MISSING |

Notes: CB-3 on realism-a/b/c (`StringAging.h`, `EnvironmentModel.h`, `BodyCouplingBank.h`, `Harmonics.h`, `RightHand.h`, `NoiseFloor.h`, `StabilityModel.h`). CB-4 on techniques (`DSP/Techniques/TapEngine.h`, `BendEngine.h`, `MuteEngine.h`, `CascadeResolver.h`). CB-10, CB-11 and CB-13 on visual ("QA: ..." commits, "Performance: boot/load/swap timings...", "Packaging and CI: CPack..."). CB-12 on tune-help (onboarding). CB-24: TECHNIQUES tab on techniques, remaining rows on visual.

<!-- counts DONE=8 NO-GUI=0 NO-TEST=0 PARTIAL=9 MISSING=2 OWNED=7 -->
