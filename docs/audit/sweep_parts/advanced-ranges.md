## advanced-ranges.md

Most of the mechanism is implemented and tested: `PhysicalRange`/`RangeRegistry`/`RangeState` with invariants, stock = the declared range, the preset `ranges` block with per-family legacy derivation, widening and clamping through `changeRanges` with undo, the Options -> RANGES page, right-click unlock/restrict, value-driven marking, the header padlock, the inline locked notice, the first-unlock explainer and randomise-respects-stock. Registry rows now cover every amp, circuit, pick, squeak, buzz and slide parameter with distinct stock/advanced pairs; same-pair rows are simply omitted, which §3.3 permits. Gaps: the `modulation` family's setter clamps are missing (`ModSources::setRateHz` hard-clamps 0.01-40 Hz). Snapshots store normalised values, so a recall after locking re-maps instead of clamping. The telemetry boolean is missing. The WORKSHOP tab padlock, 200 ms undo grouping and the Diagnostics mirror are on the visual branch.

| Req | Summary | Engine location | GUI location | Test | Status |
|---|---|---|---|---|---|
| AR-1 (§0.1) | Stock is the default for new, factory and first-run presets | `RangeState` defaults, `PresetManager` load | n/a | `Ranges::theRangesBlockRoundTripsAndDerivesWhenAbsent` | DONE |
| AR-2 (§0.2, §6.1) | Advanced is marked, never hidden: warning arc past stock plus `*` readout, driven by the value | `RangesUi::tagSlider/markReadout`, LookAndFeel stock props | every physical knob (`Widgets.cpp`) | `Ranges::markingFollowsTheValueNotTheMode`, `RangesUi::controlsFollowASwappedRangeAndMarkTheValue` | DONE |
| AR-3 (§0.3, §1.2, §1.3) | Widening keeps plain values; narrowing clamps and is undoable and announced | `RangeState::applyTo`, `LuthierAudioProcessor::changeRanges`, `RangesUi::apply` | Options > RANGES | `Ranges::wideningPreservesEveryPlainValue`, `Ranges::narrowingClampsAndReportsTheCount` | DONE |
| AR-4 (§0.4) | Range mode travels in the preset; display prefs are per user (`UiPreferences`) | `PresetManager` "ranges" block; `RangesUi` prefs | Options > RANGES | `Presets::anAdvancedValueSurvivesTheRoundTrip` | DONE |
| AR-5 (§0.5, §3.5) | Only physical params get a range; sparse registry; an empty family reads stock | `RangeRegistry` (`PhysicalRange.cpp`) | n/a | `Ranges::everyPhysicalRangeIsValid` | DONE |
| AR-6 (§0.6) | No parameter count change — no test pins the parameter count or order | `PhysicalRange` swaps only `NormalisableRange` | n/a | - | NO-TEST |
| AR-7 (§1) | PhysicalRange struct and invariants asserted | `PhysicalRange::isValid` | n/a | `Ranges::everyPhysicalRangeIsValid` | DONE |
| AR-8 (§1.0) | Stock equals the shipped declared range | `RangeRegistry::noteDeclaration`, `findDeclarationMismatches` | n/a | `Ranges::stockMatchesTheDeclaredRange` | DONE |
| AR-9 (§1.1) | Normalisation is against the live range | `makeRange`, `applyTo` | n/a | `Ranges::normalisationFollowsTheLiveRange` | DONE |
| AR-10 (§1.1) | Mode change is a structural change through the command queue — `changeRanges` is a direct message-thread call (pushUndo, applyTo, `bridge.applyAllNow`) and does not go through the ui-wiring command queue | `PluginProcessor.cpp:changeRanges` | Options > RANGES | - | PARTIAL |
| AR-11 (§1.1) | qa-polish automation matrix gains one case per family — no automation-under-range-switch test | - | n/a | - | MISSING |
| AR-12 (§1.4, §5) | Modulation and MIDI Learn sweep the live range (normalised) | `ParameterBridge::value`, `MidiLearnManager` | n/a | - | NO-TEST |
| AR-13 (§2) | Seven family keys | `RangeFamily`, `getRangeFamilyName` | Options > RANGES | `Ranges::theRangesBlockRoundTripsAndDerivesWhenAbsent` | DONE |
| AR-14 (§2, §4) | Per-control unlock via right-click; "Restrict to stock" offered only for listed controls | `showParameterContextMenu` (`Widgets.cpp`) | right-click any physical knob | `RangesUi::rightClickUnlocksAndRestrictsOneControl` | DONE |
| AR-15 (§2.1, §3.4) | `modulation` family: LFO/env/seq/follower setters clamp to the 3.4 stock pair unless advanced; locking clamps and counts; unlocked values survive state/preset | `Modulation/ModRanges.h`, `ModSources.h` setters, `ModMatrix::setModulationRangeAdvanced`, `LuthierAudioProcessor::setRanges` | MOD tab padlock (`RangeTabButton`); source-card sliders follow the live pair | `Ranges::modulationSettersClampUnlessAdvanced`, `ModMatrixUi::theSourceCardsWriteTheirNewControls` | DONE |
| AR-16 (§3.1) | amp rows (gain, bass/mid/treble, presence, master) | `PhysicalRange.cpp` table | main face amp knobs | `Ranges::everyPhysicalRangeIsValid` | DONE |
| AR-17 (§3.2) | circuit rows (volume/tone pot, tone cap, cable, input Z); treble bleed non-physical | `PhysicalRange.cpp` table | Circuit panel | `Ranges::stockMatchesTheDeclaredRange` | DONE |
| AR-18 (§3.3) | squeak/pick/buzz/slide rows from their specs; same-pair rows need no entry | `PhysicalRange.cpp` table | CHARACTER / WORKSHOP | `Ranges::everyPhysicalRangeIsValid`, `Ranges::stockMatchesTheDeclaredRange` | DONE |
| AR-19 (§4) | `ranges` block schema: families + per_control_unlocks; redundant unlocks dropped; malformed = absent | `RangeState::toVar/fromVar`, `setUnlockedIndividually` | n/a | `Ranges::theRangesBlockRoundTripsAndDerivesWhenAbsent` | DONE |
| AR-20 (§4.1) | Legacy per-family derivation from plain values after load | `RangeState::deriveFromCurrentValues`, `PresetManager.cpp` ~694 | n/a | `Ranges::theRangesBlockRoundTripsAndDerivesWhenAbsent` | DONE |
| AR-21 (§5) | Snapshots never carry mode; recall after locking clamps — `Snapshots.cpp` stores and recalls `getValue()` (normalised), so a recall against the stock range re-maps rather than clamps the plain value | `Live/Snapshots.cpp` (~182, ~368) | LIVE | - | PARTIAL |
| AR-22 (§5) | A/B slots each carry their own ranges block | `captureStateBlock` -> `getStateInformation` | header A/B | - | NO-TEST |
| AR-23 (§5, §6.2) | Randomise respects stock (pref default on); off uses the live range | `PresetManager::randomise`, `RangesUi::randomiseRespectsStock` | Options > RANGES toggle | `RangesUi::randomiseStaysInStockUnlessToldOtherwise` | DONE |
| AR-24 (§5) | Reset to default writes defaultValue and never changes range mode — `resetToDefaults` writes the normalised `getDefaultValue()`, never checked against an advanced live range | `PresetManager::resetToDefaults` | header File menu | - | NO-TEST |
| AR-25 (§6.1) | Tab padlock (accent unlocked / muted locked) on tabs holding physical params — CHARACTER only here; WORKSHOP padlock on visual (`AdvancedPanel` RangeTabButton for WORKSHOP); MOD waits for AR-15 | `RangesUi::RangeTabButton` | CHARACTER tab | - | OWNED |
| AR-26 (§6.1, gui-int 2) | Header padlock while anything is unlocked | `RangesUi` PadlockButton | header | `RangesUi::theHeaderPadlockShowsOnlyWhenSomethingIsUnlocked` | DONE |
| AR-27 (§6.2.1) | Master per-preset toggle with the clamp count before commit | `RangesPage::masterToggled/setAllFamilies` | Options > RANGES | `RangesUi::theRangesPageListsLocksAndClamps` | DONE |
| AR-28 (§6.2.2) | "Always show marked values as warning colour" pref, default on | `RangesUi` kWarningColourKey | Options > RANGES | - | NO-TEST |
| AR-29 (§6.2.4) | Out-of-stock summary with per-row clamp and empty text | `RangesPage::clampOne`, `RangeState::findValuesOutsideStock` | Options > RANGES | `RangesUi::theRangesPageListsLocksAndClamps` | DONE |
| AR-30 (§6.3) | Locked: inline notice with the fixed text; a drag stops at the stock edge | `showLockedRangeNoticeIfAtEdge`, `RangesUi::kLockedNoticeText` | any physical knob | - | NO-TEST |
| AR-31 (§6.4) | First-unlock explainer, once, on the first transition | `RangesUi::showExplainerIfFirstTime`, `RangesUi::apply` | CallOutBox at the anchor | - | NO-TEST |
| AR-32 (§7) | Undo: a lock restores the clamped values; an unlock narrows back | `pushUndoState` full-state entry in `changeRanges` | Ctrl+Z | `RangesUi::rightClickUnlocksAndRestrictsOneControl`, `RangesUi::theRangesPageListsLocksAndClamps` | DONE |
| AR-33 (§7) | Entry class `ranges-toggle`, grouped by target within 200 ms, not a boundary | on visual: "Undo tier 3: 200 ms grouping", `PluginProcessor.h` entry class/target | - | - | OWNED |
| AR-34 (§8) | Telemetry usage boolean `advanced_ranges_used` — `Telemetry` has no usage feature booleans, on any branch | `Updates/Telemetry.cpp` | Options > PRIVACY | - | MISSING |
| AR-35 (§8) | Options -> Diagnostics mirrors the boolean | on visual: `AudioPathView::describeFlags` ("Advanced ranges: ...") on DiagnosticsPage | Options > DIAGNOSTICS | - | OWNED |
| AR-36 (§9) | Range mode read once on change, nothing per block | `RangeState::applyTo` (message thread) | n/a | - | NO-TEST |
| AR-T1 (§10) | Test: invariants sweep | - | n/a | `Ranges::everyPhysicalRangeIsValid` | DONE |
| AR-T2 (§10) | Test: widening is silent (plain values within 1e-9 **and a bit-identical rendered block**) — plain values checked, no rendered-block comparison | - | n/a | `Ranges::wideningPreservesEveryPlainValue` | PARTIAL |
| AR-T3 (§10) | Test: narrowing clamps and reports a count of 1 | - | n/a | `Ranges::narrowingClampsAndReportsTheCount` | DONE |
| AR-T4 (§10) | Test: undo restores a clamp | - | n/a | `RangesUi::rightClickUnlocksAndRestrictsOneControl`, `RangesUi::theRangesPageListsLocksAndClamps` | DONE |
| AR-T5 (§10) | Test: normalisation follows the live range | - | n/a | `Ranges::normalisationFollowsTheLiveRange` | DONE |
| AR-T6 (§10) | Test: legacy load derives per family | - | n/a | `Ranges::theRangesBlockRoundTripsAndDerivesWhenAbsent` | DONE |
| AR-T7 (§10) | Test: legacy load of an ordinary preset stays stock | - | n/a | `Ranges::theRangesBlockRoundTripsAndDerivesWhenAbsent` | DONE |
| AR-T8 (§10) | Test: marking follows the value | - | n/a | `Ranges::markingFollowsTheValueNotTheMode` | DONE |
| AR-T9 (§10) | Test: randomise respects stock over 1000 passes | - | n/a | `RangesUi::randomiseStaysInStockUnlessToldOtherwise` | DONE |
| AR-T10 (§10) | Test: snapshots do not carry mode (capture advanced, lock, recall, values clamped) — no test (and AR-21 would fail it) | - | n/a | - | MISSING |

<!-- counts DONE=29 NO-GUI=0 NO-TEST=8 PARTIAL=3 MISSING=3 OWNED=3 -->
