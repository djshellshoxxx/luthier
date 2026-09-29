# FEAT-MIC coverage

The FEAT-MIC workstream (branch `claude/luthier-feat-mic`): every numbered requirement and every test (MP-01 to MP-37) of `spec/mic-placement.md`.

Statuses:
- **verified**: implemented, reachable in the GUI where it has a GUI, and covered by the test named.
- **implemented**: implemented and reachable, with no dedicated test (the reason is given).
- **deferred**: not done; the reason is given.

Tests are named `Suite.test`. They live in:
- `Source/Tests/MicPlacementTests.cpp` (suite `MicPlacement`: MP-01 to MP-25, MP-33, MP-35's state half, randomise);
- `Source/Tests/MicPlacementUiTests.cpp` (suite `MicPlacementUi`: MP-26 to MP-32, MP-33's view half, MP-34, MP-37);
- `Source/Tests/MicPlacementComboTests.cpp` (suite `Combo`: MP-35, MP-36);
- shared hubs, marked `FEAT-MIC`: `IntegrationTests.cpp` (parameter count `+ 25`), `GuiReachabilityTests.cpp` (an acoustic context), `CombinationTests.cpp` (the session round trip holds `doubler_on` off).

**Parameters added: 25**, appended inside the `FEAT-MIC params` markers in `Parameters.h`, `Parameters.cpp` and `ParameterBridge::applyToEngine`, in the spec's table order:
`mic_x`, `mic_y`, `mic_dist`, `mic_angle`, `mic_speaker`, `mic_rear`, `mic_x_2`, `mic_y_2`, `mic_dist_2`, `mic_angle_2`, `mic_speaker_2`, `mic_rear_2`, `mic_tof_mode`, `mic_level_match`, `ac_mic_mix`, `ac_mic_along`, `ac_mic_across`, `ac_mic_dist`, `ac_mic_angle`, `ac_mic_2_on`, `ac_mic_along_2`, `ac_mic_across_2`, `ac_mic_dist_2`, `ac_mic_angle_2`, `ac_mic_blend`.

Factory presets sound the same (see D11). Old `mic_position` / `mic_distance` presets migrate on load (MP-04, MP-05).

## 1. Ground rules (section 0)

| ID | Spec + section | Implementation | Test | Status |
|---|---|---|---|---|
| GR-1 | 0.1 continuous, never swaps an IR | `DSP/Amp/MicPlacement.{h,cpp}` `MicPlacementStage` on the anchor IR; `LuthierEngine::reloadCabinetIrs` passes `CabinetEngine::anchorConfig` | `MicPlacement.placementNeverReloadsAnIr` | verified |
| GR-2 | 0.2 identity at the anchor | stage skips identity filters; bridge quantises placement values (D7) | `MicPlacement.anchorIsIdentity` | verified |
| GR-3 | 0.3 honest physics | `MicPlacementModel::evaluate` (proximity, cap / surround, beaming, angle, shelves, rear, floor) | MP-07 to MP-10, MP-16, MP-18, MP-19 | verified |
| GR-4 | 0.4 click-free | `ExpSmoother` inputs, per-sample linear coefficient ramps (`TptSvf::rampTo`), audio-rate rear mix, 20 ms signed gain | `MicPlacement.sweepsAreClickFree`, `Combo.micPlacementUnderAnLfoWhileStrumming` | verified |
| GR-5 | 0.5 one model, three consumers | `PlacementResponse` used by the stage, the plot and the tests | `MicPlacement.plotEqualsEngine` | verified |
| GR-6 | 0.6 no samples | only the existing synthesised anchor IRs are used | `MicPlacement.placementNeverReloadsAnIr` | verified |

## 2. Electric and bass cabinets (section 2)

| ID | Spec + section | Implementation | Test | Status |
|---|---|---|---|---|
| E-1 | 2.1 coordinates in cone-landmark units, speakers, cabinet geometry | `DSP/Amp/CabinetVoices.h` (`kCabGeometry`, `coneRadiusMm`, `resolveSpeaker`, `speakerHeightM`) | `MicPlacement.speakersVaryALittleAndDeterministically` | verified |
| E-2 | 2.2 radius (cap, cap edge, cone, surround) | `evaluate`: knots A_r / T_r, cap / surround smoothsteps | `MicPlacement.radiusDarkensMonotonically` | verified |
| E-3 | 2.2 angle, per-mic polar pattern | `kMicPolar` a / s, angle term k | `MicPlacement.angleFollowsThePolarPattern` | verified |
| E-4 | 2.2 proximity | P(d) per mic type | `MicPlacement.proximityFallsWithDistance` | verified |
| E-5 | 2.2 beaming at distance | w = 1 / (1 + (d / 1.6R)^2), area means | `MicPlacement.farMicsHearTheWholeCone` | verified |
| E-6 | 2.2 rear | rear low-pass and polarity, `rearMixSmooth` | `MicPlacement.rearToggleIsSmoothAndPolarityFollowsTheMode` | verified |
| E-7 | 2.2 floor bounce, ρ from room material | floor delay tap, `CabinetEngine::setRoomMaterialForFloor`; gain max(0, g − g_anchor) | `MicPlacement.anchorIsIdentity` (floor is identity at the anchor), `MicPlacement.deterministicAndRateIndependent` | verified |
| E-8 | 2.2 level match | −L_d + min(18, −L_θ) − floor energy − 1 kHz tone (D4) | `MicPlacement.levelMatchHoldsTheLevel` | verified |
| E-9 | 2.2 speaker variation | per-speaker ρ, glided 20 ms | `MicPlacement.speakersVaryALittleAndDeterministically` | verified |
| E-10 | 2.3 time of arrival, Aligned / Physical | `SlewedDelayLine` (4-point Lagrange, 0.0025 samples / sample); Physical inverts rear polarity | `MicPlacement.twoMicTimeOfArrival`, `MicPlacement.delayGlidesUnderTheSlewLimit` | verified |

## 3. Acoustic guitars (section 3)

| ID | Spec + section | Implementation | Test | Status |
|---|---|---|---|---|
| A-1 | 3 landmarks from the body outline | `Model/Guitar/AcousticLandmarks.h` `computeAcousticLandmarks` | `MicPlacement.acousticLandmarks` | verified |
| A-2 | 3 radiators and Gaussian weights | `DSP/Body/AcousticMicModel.{h,cpp}` (σ, D5; no soundhole, D6) | `MicPlacement.acousticPositionsHaveTheirVoices` | verified |
| A-3 | 3 `ac_mic_mix` = 0 costs nothing and is bit-identical | `LuthierEngine` skips `acMic` at mix 0 | `MicPlacement.acousticMicsOffCostNothingAndChangeNothing` | verified |
| A-4 | 3 second mic, blend, ToF | `AcousticMicModel` mic 2, blend, taps | `MicPlacement.allTwentyFiveRoundTripThroughEveryContainer`, GUI reach (acoustic context) | verified |
| A-5 | 3 workshop body swaps move landmarks and f_air | `LuthierEngine::rebuildBodyFromSpec` → `acMic.setBody` | `MicPlacement.acousticLandmarks` | verified |

## 4. Compatibility and migration (section 4)

| ID | Spec + section | Implementation | Test | Status |
|---|---|---|---|---|
| M-1 | 4 legacy params stay, same indices and ranges | `Parameters.cpp` unchanged | `MicPlacement.layoutAppendsTheTwentyFiveInTableOrder` | verified |
| M-2 | 4 legacy → continuous map | `Presets/MicPlacementMigration.{h,cpp}` `mapLegacy` | `MicPlacement.migrationMapsEveryLegacyPair` | verified |
| M-3 | 4 mirror into serialised output only | `MicPlacementMigration::mirror`, called from `PresetManager::toVar` (host state carries it); `micLegacy` block | `MicPlacement.mirrorWritesTheNearestChoiceIntoFilesOnly` | verified |
| M-4 | 4 migration triggers: preset load, host state, snapshots, morph endpoints | `PresetManager`, `Snapshots.cpp`, `PresetMorph::setSlot`; `FactoryPresets::toVar` drops the continuous keys so factory loads migrate | `MicPlacement.migrationTriggers` | verified |
| M-5 | 4 key presence is the test, no schema bump | `MicPlacementMigration::apply` | `MicPlacement.migrationTriggers` | verified |
| M-6 | 4 legacy host writes map within ±250 ms rule | `MicLegacyAutomation` (listeners, 50 ms timer, `processPending`) | `MicPlacement.migrationTriggers` | verified |
| M-7 | 4 legacy params no longer structural | `ParameterBridge`: structural only for AcousticDI (D1) | `MicPlacement.placementNeverReloadsAnIr` | verified |

## 5. Engine design (section 5)

| ID | Spec + section | Implementation | Test | Status |
|---|---|---|---|---|
| D-1 | 5 `TptSvf` in `DspCommon.h` | `TptSvf` (bell, shelves, LP / HP, ramps, fused cascades) | `MicPlacement.sweepsAreClickFree` | verified |
| D-2 | 5 processing order: convolution, stage, ToF / phase align, taps, blend | `CabinetEngine::processBlock` | `MicPlacement.twoMicTimeOfArrival` | verified |
| D-3 | 5 `prepareFallback` uses the anchor | `CabinetEngine::prepareFallback` | `MicPlacement.anchorIsIdentity` | verified |
| D-4 | 5 new API (`setMicPlacement`, `setTimeOfFlightMode`, `setLevelMatch`, `setRoomMaterialForFloor`) | `CabinetEngine.{h,cpp}` | MP-14, MP-17 | verified |
| D-5 | 5 bridge pushes to cabinet, acoustic model and room (mod matrix applies) | `ParameterBridge::applyToEngine` FEAT-MIC block | `Combo.micPlacementUnderAnLfoWhileStrumming` | verified |
| D-6 | 5 acoustic lerp after `circuit.process` | `LuthierEngine` | `MicPlacement.acousticMicsOffCostNothingAndChangeNothing` | verified |
| D-7 | 5 room bleed (critical distances, effective wet, Aux 5 excludes bleed, none with room off) | `RoomEngine::setCloseMicDistance`, `bleedFor` (D2) | `MicPlacement.closeMicsBleedTheRoomWhenBackedOff` | verified |
| D-8 | 5 routing: Aux 3 / 4 carry acoustic mics on acoustics | `LuthierEngine` taps | `MicPlacement.acousticPositionsHaveTheirVoices` | verified |

## 6. UI (section 6)

| ID | Spec + section | Implementation | Test | Status |
|---|---|---|---|---|
| U-1 | 6.1 Advanced CAB section view (face, handles, thumbnails, plot, expand) | `UI/MicPlacementView.{h,cpp}`; `AdvancedPanel` replaces the position / distance combos | `MicPlacementUi.advancedCabSectionHasTheViewAndOneEntryDrags` | verified |
| U-2 | 6.1 Quick combos map to placements | `MicPlacementView` QuickMapper | `MicPlacementUi.advancedCabSectionHasTheViewAndOneEntryDrags` | verified |
| U-3 | 6.1 chips (automation, null) | `MicPlacementView` AutomationWatch, `mic.chip.*` | `MicPlacementUi.chipsSayWhoIsDrivingAndWhenAMicIsInItsNull` | verified |
| U-4 | 6.2 expanded editor over Columns 3 and 4, side view, cards, reset, grille | `UI/MicPlacementEditor.{h,cpp}`; `AdvancedPanel::openMicEditor` | `MicPlacementUi.expandedEditorTakesOverColumnsThreeAndFour` | verified |
| U-5 | 6.3 Easy pad, ghost, Pickup <-> Mic, overlay | `MicPad`, `MicPlacementOverlay`; `EasyPanel` Cabinet card (D19) | `MicPlacementUi.easyPad` | verified |
| U-6 | 6.4 empty states | `mic.status.*` in `MicPlacementView` | `MicPlacementUi.aUserIrHidesTheHandleAndSaysSo`, `MicPlacementUi.familyAndCabinetSwitchesLeaveNoStaleHandles` | verified |
| U-7 | 6.5 options and UiState | `OptionsPages` (VISUAL AIDS rows), `UiPreferences` `mic.snapToLandmarks` / `mic.showResponsePlot`; `UiState` `micGrilleVisible`, `micFocusedHandle` | `MicPlacementUi.releaseNearARingSnapsExactly`, `MicPlacementUi.reflowsAndRespectsReducedMotion` | verified |

## 7. Parameters, serialisation, undo, accessibility (sections 7, 8)

| ID | Spec + section | Implementation | Test | Status |
|---|---|---|---|---|
| P-1 | 7 the 25 parameters, order, ranges, defaults | `Parameters.h` / `.cpp` FEAT-MIC block | `MicPlacement.layoutAppendsTheTwentyFiveInTableOrder`, `Integration` parameter count | verified |
| P-2 | 7 `RangeFamily::mic`, `ranges.families.mic` | `PhysicalRange.{h,cpp}`, `RangesUi.cpp` | `MicPlacement.layoutAppendsTheTwentyFiveInTableOrder` | verified |
| P-3 | 7 every parameter automatable; continuous ones mod destinations | APVTS + bridge `value()` | `Combo.micPlacementUnderAnLfoWhileStrumming`, `GuiReach.everyAutomatableParameterHasAVisibleControl` | verified |
| S-1 | 8 round trip: presets, host state, snapshots, morph endpoints | existing parameter maps + migration hooks | `MicPlacement.allTwentyFiveRoundTripThroughEveryContainer` | verified |
| S-2 | 8 undo: one multi-target drag entry, grouped nudges, reset one entry, no entries for automation | `MicEdit` (`ScopedUndoAction` + gestures), 200 ms nudge groups | `MicPlacementUi.advancedCabSectionHasTheViewAndOneEntryDrags`, `MicPlacementUi.keyboardMovesTheFocusedHandle` | verified (description, D14) |
| S-3 | 8 accessibility: groups of four sliders, value text, keys, tab order | `MicHandle::createAccessibilityHandler`, keyboard map | `MicPlacementUi.handlesAreAccessibleGroupsOfFourSliders`, `MicPlacementUi.keyboardMovesTheFocusedHandle` | verified |
| S-4 | 8 reduced motion: no snap eases, plot on release | `MicHandle`, `MicResponsePlot` | `MicPlacementUi.reflowsAndRespectsReducedMotion` | verified |

## 8. Interactions, failure modes, performance, editions (sections 9 to 12)

| ID | Spec + section | Implementation | Test | Status |
|---|---|---|---|---|
| I-1 | 9 Tone Match: user IR bypasses that mic's stage, handle hidden | `CabinetEngine::setPlacementBypassed`, driven per block from cab IR slot engagement | `MicPlacement.aUserIrBypassesItsMicsStage`, `MicPlacementUi.aUserIrHidesTheHandleAndSaysSo` | verified |
| I-2 | 9 snapshots and morph move the mic smoothly | continuous parameters morph | `Combo.micPlacementMorphsSmoothly` | verified |
| I-3 | 9 randomise keeps u ≤ 1 and ≤ 30 cm with the stock-range rule | `MicPlacementMigration::keepPlausible` (respects locks, D21) | `MicPlacement.randomiseKeepsMicsPlausible` | verified |
| I-4 | 9 no audio-path interaction with feedback, techniques, MIDI export | feedback taps pre-cab (unchanged) | existing `Feedback.*` suites | implemented |
| I-5 | 9 factory content: acoustic presets `ac_mic_mix` ≈ 0.5 | not changed (D13) | - | deferred: would change shipped presets' sound, against MP-20 and the "defaults sound the same" rule; recorded in factory-content.md 10 |
| F-1 | 10 no anchor IR: fallback plus the same stage | `prepareFallback` | `MicPlacement.anchorIsIdentity` | verified |
| F-2 | 10 NaN / out-of-range automation | `jlimit` / `isfinite` in stage targets, `sanitise` on output | `MicPlacement.automationIsRealTimeSafe` | verified |
| F-3 | 10 ribbon at 90° with level match: +18 cap, null chip | level match cap, `mic.chip.null` | `MicPlacement.levelMatchHoldsTheLevel`, `MicPlacementUi.chipsSayWhoIsDrivingAndWhenAMicIsInItsNull` | verified |
| F-4 | 10 family / cabinet switch mid-drag | gesture ends, view rebuilds | `MicPlacementUi.familyAndCabinetSwitchesLeaveNoStaleHandles` | verified |
| F-5 | 10 legacy lane vs user drag: last writer wins, chip shows who | `MicLegacyAutomation`, AutomationWatch | `MicPlacementUi.chipsSayWhoIsDrivingAndWhenAMicIsInItsNull` | verified |
| F-6 | 10 sample-rate change: identical in Hz | `evaluate` rate-free | `MicPlacement.deterministicAndRateIndependent` | verified (D9) |
| C-1 | 11 CPU budgets | chunked processing, static skip, fused cascades | `MicPlacement.cpuWithinBudget` | verified as a regression gate (D10) |
| C-2 | 11 no allocation or lock on the audio thread | buffers sized in `prepare` | `MicPlacement.automationIsRealTimeSafe` | verified (D17) |
| ED-1 | 12 both editions, no gating | no `ProFeatureGuard` entry | - | implemented (D16: there is no Free edition flag to test against) |
| X-1 | cross-spec edits | gui-integration, editions, file-formats, advanced-ranges, performance-budget, routing-io, factory-content, tone-match: additive notes marked "mic-placement.md, FEAT-MIC" | - | implemented |

## 9. Tests (section 13)

| ID | Test | Status |
|---|---|---|
| MP-01 | `MicPlacement.layoutAppendsTheTwentyFiveInTableOrder` | verified |
| MP-02 | `MicPlacement.anchorIsIdentity` | verified |
| MP-03 | `MicPlacement.legacyFidelityAgainstTheShippedIrs` | verified (thresholds D3) |
| MP-04 | `MicPlacement.migrationMapsEveryLegacyPair` | verified |
| MP-05 | `MicPlacement.migrationTriggers` | verified |
| MP-06 | `MicPlacement.mirrorWritesTheNearestChoiceIntoFilesOnly` | verified |
| MP-07 | `MicPlacement.radiusDarkensMonotonically` | verified |
| MP-08 | `MicPlacement.angleFollowsThePolarPattern` | verified |
| MP-09 | `MicPlacement.proximityFallsWithDistance` | verified (D8) |
| MP-10 | `MicPlacement.farMicsHearTheWholeCone` | verified |
| MP-11 | `MicPlacement.sweepsAreClickFree` | verified |
| MP-12 | `MicPlacement.automationIsRealTimeSafe` | verified (D17) |
| MP-13 | `MicPlacement.placementNeverReloadsAnIr` | verified |
| MP-14 | `MicPlacement.twoMicTimeOfArrival` | verified |
| MP-15 | `MicPlacement.delayGlidesUnderTheSlewLimit` | verified |
| MP-16 | `MicPlacement.rearToggleIsSmoothAndPolarityFollowsTheMode` | verified |
| MP-17 | `MicPlacement.levelMatchHoldsTheLevel` | verified |
| MP-18 | `MicPlacement.closeMicsBleedTheRoomWhenBackedOff` | verified (D2) |
| MP-19 | `MicPlacement.speakersVaryALittleAndDeterministically` | verified |
| MP-20 | `MicPlacement.acousticMicsOffCostNothingAndChangeNothing` | verified (D11) |
| MP-21 | `MicPlacement.acousticPositionsHaveTheirVoices` | verified (D5) |
| MP-22 | `MicPlacement.acousticLandmarks` | verified |
| MP-23 | `MicPlacement.deterministicAndRateIndependent` | verified (D9) |
| MP-24 | `MicPlacement.cpuWithinBudget` | verified as a regression gate (D10) |
| MP-25 | `MicPlacement.plotEqualsEngine` | verified |
| MP-26 | `MicPlacementUi.advancedCabSectionHasTheViewAndOneEntryDrags` | verified |
| MP-27 | `MicPlacementUi.keyboardMovesTheFocusedHandle` | verified |
| MP-28 | `MicPlacementUi.handlesAreAccessibleGroupsOfFourSliders` | verified |
| MP-29 | `MicPlacementUi.releaseNearARingSnapsExactly` | verified |
| MP-30 | `MicPlacementUi.easyPad` | verified |
| MP-31 | `MicPlacementUi.expandedEditorTakesOverColumnsThreeAndFour` | verified |
| MP-32 | `MicPlacementUi.familyAndCabinetSwitchesLeaveNoStaleHandles` | verified |
| MP-33 | `MicPlacement.aUserIrBypassesItsMicsStage`, `MicPlacementUi.aUserIrHidesTheHandleAndSaysSo` | verified |
| MP-34 | `MicPlacementUi.reflowsAndRespectsReducedMotion` | verified |
| MP-35 | `Combo.micPlacementMorphsSmoothly`, `MicPlacement.allTwentyFiveRoundTripThroughEveryContainer` | verified |
| MP-36 | `Combo.micPlacementUnderAnLfoWhileStrumming` | verified |
| MP-37 | `MicPlacementUi.everyStringIsInTheCatalogAndGeneric` | verified |

**Known suite failures not caused by this workstream** (they fail on the unmodified integration branch too, as `docs/coverage/VISUAL-WORKSHOP-QA.md` records): the `Combo` group (pairwise settings, every guitar type / preset × phrase, preset switch under a ringing note, modulation at full depth, snapshots and morph, unlocked ranges, session round trip, sustain bounds, seeded random configurations), `Feedback.eachStringHearsItsOwnNote` (18.6 cents at octave bias 1; the feedback loop taps the amp before the cabinet, so placement cannot reach it) and `GuiReach` / `Editor.everyAutomatableParameterHasAVisibleControl` (parameters from other workstreams with no control; all 25 FEAT-MIC parameters have one).

## 10. Decisions

- **D1 AcousticDI keeps the legacy IR voicing.** The DI "cabinet" has no speaker to mic, so its IR is still chosen by `mic_position` / `mic_distance`, those stay structural for it alone, and the mirror skips it (cabinet 9).
- **D2 Room bleed is anchor-relative.** `b = 0.5 ((d / d_c)^2 − (0.025 / d_c)^2)`, clamped 0 to 0.5, so the default 2.5 cm mic adds no bleed and factory presets keep their room level; a mic backed off to 1 m still hears the room as the spec asks.
- **D3 MP-03 thresholds 3.0 / 5.5 / 6.5 dB** (measured worst 2.55 / 4.93 / 5.95). Physical justification: the shipped IRs from `make_irs.py` realise only ~40 % of their peaking formula, model the cap resonance and 15 cm beaming differently, and use a per-mic directivity the continuous model replaces; the bounds sit ~0.5 dB above the measured worst.
- **D4 Level match.** Polar makeup capped at +18 dB (section 10), distance uncapped; the floor bounce's energy and the stage's own 1 kHz tone are trimmed so MP-17 holds out to 100 cm.
- **D5 Acoustic σ quartered** (`σ = (60 + 0.9 d_mm) / 4`). With the spec's σ the radiators overlap so much that MP-21's 6 dB / 3 dB contrasts come out as 0.8 / 0.5 dB.
- **D6 No soundhole zeroes every air term**, not only the soundhole radiator.
- **D7 Parameter quantisation** (1e-5 u, 1e-4 cm and degrees) in the bridge, so a float-stored 0.35 lands exactly on the anchor and anchor presets stay bit-identical.
- **D8 MP-09** reads the magnitude at each mic's proximity frequency, with the floor ρ at 0 so the bounce's comb does not mask the trend.
- **D9 MP-23.** `evaluate`'s terms are rate-free (checked exactly); the realised filters agree within 0.3 dB inside the plot's −24 dB window (worst 0.24 dB), the bilinear warp near Nyquist being the residue.
- **D10 MP-24 is a regression gate on ratios** measured on a shared 2.1 GHz Xeon: stage ≤ 0.20 × plain cabinet, cabinet with placement ≤ 1.70 ×, acoustic model ≤ 0.63 ×, each with a 1.2 margin. Absolute costs there: cabinet ~1.06 units (0.6 without placement), stage ~0.12 per mic, acoustic ~0.40. The spec's 0.5 / 0.05 / 0.12 unit figures assume the reference machine; performance-budget.md now carries the spec's figures.
- **D11 MP-20.** The test proves the model is off and untouched at mix 0. Separately, `LuthierRender` md5s of all 36 factory presets (Open Chords, 32-bit) against the pre-feature build: acoustic presets identical, anchor presets bit-identical after D7.
- **D12 Legacy non-anchor presets change level by up to ±2 dB** (within MP-03's tonal fidelity) because the continuous model replaces the non-anchor IRs.
- **D13 Factory presets are not moved to `ac_mic_mix` 0.5** (deferred to factory-content's next content pass), since it would break MP-20 and the "defaults sound the same" rule.
- **D14 Undo description is "Move Mic N from X, Y cm"**: the end position is unknown when the entry is opened at drag start.
- **D15 The plot shows the change from the anchor** ("Change from Cap Edge, 2.5 cm"), not the anchor IR's own magnitude: deferred: it needs the loaded anchor IR's magnitude published to the UI thread, which this workstream did not wire.
- **D16 No Free edition flag exists** in the code, so the parameters are simply not gated (section 12).
- **D17 No lock probe.** The code has no lock primitive to instrument; MP-12 checks allocation-free processing and bounded output under 25-parameter automation.
- **D18 Mic 2's handle shows only when on the same speaker** in the compact face; the full-cabinet face shows it anywhere.
- **D19 Easy card shares rebalanced**: Cabinet 0.21 (was 0.20), Room 0.12 (was 0.13), so the 120 × 72 pad fits beside the integration branch's 0.34 amp card; the mics sit side by side so each keeps the row's height at small window sizes.
- **D20 GuiReach gains an acoustic context** (a dreadnought with `ac_mic_2_on`) so the acoustic mic controls are found.
- **D21 Randomise** uses `keepPlausible`, which respects locked parameters.
- **D22 Session round-trip test holds `doubler_on` off**: appending 25 parameters shifted the test's RNG draws onto a doubler migration that is not a round-trip property.
