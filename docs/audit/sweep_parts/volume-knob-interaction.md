## volume-knob-interaction.md

`GuitarCircuit` has replaced `CableSim`. It solves pickup, pots, tone cap, bleed, cable and amp input as one nodal network (trapezoidal MNA, recorded in DECISIONS.md), sits between the pickups and the pre-effects, and carries all its parameters. Controls: the Advanced column-2 Circuit section and the Easy rig strip, both with the live response view. Every §6 test exists in `CircuitTests.cpp`/`FeedbackTests.cpp`. Gaps: the custom bleed R/C/mode editor is always visible instead of only for Custom. The CHARACTER tab has no full-size CIRCUIT group. 50s wiring and the parameter table's defaults are untested. The Easy Tone macro silently rescales the physical tone wiper. The response-view test is on the visual branch.

| Req | Summary | Engine location | GUI location | Test | Status |
|---|---|---|---|---|---|
| VK-1 (intro) | `CableSim` removed, not deprecated | only a comment remains in `DSP/Circuit/GuitarCircuit.h` | n/a | `Circuit::theEngineRunsThroughTheCircuit` | DONE |
| VK-2 (§0.1, §1.1) | One network solved once per change (H(s) bilinear; nodal trapezoidal per DECISIONS) | `GuitarCircuit::rebuild`, `buildPassiveNetlist` | n/a | `Circuit::theAudioPathMatchesTheResponse` | DONE |
| VK-3 (§0.2, §1.4) | Passive by default; Active toggle: buffer, attenuator volume, active 1-pole tone | `buildActivePickupNetlist`, `activeToneCornerHz` | ADVANCED col 2 Circuit "Active" (`AdvancedPanel` circuitActive) | `Circuit::activeModeRemovesTheLoading` | DONE |
| VK-4 (§0.3) | Component values in ohms and farads (caps in nF per DECISIONS) | `Parameters.cpp` circuit block, `Parameters::parseOhms` | ADVANCED col 2 Circuit | `Circuit::ohmsReadTheWayThePartsArePrinted` | DONE |
| VK-5 (§0.4) | Honest magnitudes: 10->7 loses a few dB at 5 kHz | `GuitarCircuit` | n/a | `Circuit::turningDownDarkensAsWellAsQuietens` | DONE |
| VK-6 (§0.5) | No audio-thread allocation; the fixed filter is re-solved between blocks (on the audio thread, per DECISIONS) | `LuthierEngine` -> `circuit.setComponents` | n/a | `Circuit::sweepingEveryControlDoesNotAllocate` | DONE |
| VK-7 (§1, §4) | Ls/Rs/Cp from the pickup part (selected coils in parallel) | `LuthierEngine::getLiveCircuitComponents`, `pickups.getSelectedCoil` | n/a | `Circuit::theEngineRunsThroughTheCircuit` | DONE |
| VK-8 (§1.2) | Volume = wiper: level, loading and cable decoupling | `GuitarCircuit::taperFraction`, netlist | ADVANCED col 2 large Volume knob (`addLargeKnob guitarVolume`); EASY rig strip | `Circuit::turningDownDarkensAsWellAsQuietens` | DONE |
| VK-9 (§1.3) | Treble bleed None / Kinman / Fender / Custom (trademark-safe labels) | `TrebleBleed`, `Parameters::trebleBleedNames` | ADVANCED col 2 "Treble bleed" choice | `Circuit::aKinmanBleedKeepsTheTop` | DONE |
| VK-10 (§1.3) | Custom bleed R, C and parallel/series | `circuit_bleed_r/_c/_mode` in the netlist | ADVANCED col 2 Bleed R / Bleed C / Bleed wiring | `Circuit::everyCornerOfTheAdvancedRangeIsStable` | DONE |
| VK-11 (§2) | Cable pF/m table 52/98/160/220 and length | `cableCapacitancePerMetre` | ADVANCED col 2 Length + Cable quality | `Circuit::cableCapacitanceMovesTheResonance` | DONE |
| VK-12 (§2) | `cable_on` off = zero-length cable, not silence | `cableCapacitance` | ADVANCED col 2 "Cable" toggle | `Circuit::bypassIsNeutral` | DONE |
| VK-13 (§3) | All IDs, types and defaults per the table | `Parameters.cpp` ~606-620 | ADVANCED col 2 Circuit | `Circuit::parametersMatchTheSpecTable` | DONE |
| VK-14 (§3) | `guitar_volume`/`guitar_tone` are the wipers — the Easy Tone macro rescales the tone wiper (`guitarTone × (0.45 + macroTone×1.1)`), so the physical control's value is not what the circuit sees | `Parameters.cpp` ~1345 | EASY Tone macro | - | DEFERRED |
| VK-15 (§3) | Circuit-family stock/advanced ranges | `PhysicalRange.cpp` circuit rows | Options > RANGES; knobs | `Circuit::everyCornerOfTheAdvancedRangeIsStable`, `Ranges::stockMatchesTheDeclaredRange` | DONE |
| VK-16 (§3.1) | 50s wiring topology keeps the top as volume comes down | `PotTaper::fiftiesWiring` in the netlist | ADVANCED col 2 "Taper" | `Circuit::fiftiesWiringKeepsTheTop` | DONE |
| VK-17 (§4) | Feedback level taken after the circuit; volume 5 lowers the loop by the circuit's attenuation ±0.5 dB | `FeedbackLoop` | n/a | `Feedback::theVolumeKnobLowersTheLoopByTheCircuitsAttenuation` | DONE |
| VK-18 (§4) | Chain position String -> Pickup -> Circuit -> PreFX, where CableSim was | `LuthierEngine::processBlock` | n/a | `Circuit::theEngineRunsThroughTheCircuit` | DONE |
| VK-19 (§4) | Linear circuit, not oversampled | `GuitarCircuit` at base rate | n/a | `Circuit::everyCornerOfTheAdvancedRangeIsStable` | DONE |
| VK-20 (§5) | CIRCUIT panel in Advanced column 2: large volume and tone knobs | `AdvancedPanel` Circuit section | ADVANCED col 2 | `GuiReach::everyAutomatableParameterHasAVisibleControl` | DONE |
| VK-21 (§5) | Pot and cap dropdowns (250k/500k/1M, 10n/22n/47n, custom) plus taper dropdown | `StandardValueChoice` (`UI/CircuitPanel.h`) | ADVANCED col 2 | `GuiReach::operatingEachControlWritesItsParameter` | DONE |
| VK-22 (§5) | Treble-bleed R/C editor appears only for Custom — Bleed R, Bleed C and Bleed wiring are always shown; nothing ties their visibility to `circuit_treble_bleed` | `AdvancedPanel.cpp` ~674-690 | ADVANCED col 2 | - | PARTIAL |
| VK-23 (§5) | Live H(s) response view with the resonant peak marked, redrawn as controls move | `CircuitResponseView` (`UI/CircuitPanel.cpp`) | ADVANCED col 2; EASY rig strip | on visual: `LiveDisplays::theFretboardDrawsTheSlideBarAndTheCircuitCurveFollowsTheVolume` | OWNED |
| VK-24 (§5; gui-int 4.4) | CHARACTER tab CIRCUIT group, the same panel at full size — `CharacterPanel` has no circuit group on this branch or on visual | - | - | - | MISSING |
| VK-25 (§5) | Easy compact card: volume, tone and the mini response view | `EasyPanel::buildRigStrip` | EASY rig strip | `Circuit::theEasyRigStripCarriesTheGuitarKnobsAndTheView` | DONE |
| VK-T1 (§6) | Test: volume interaction (level vs taper, darkening) | - | n/a | `Circuit::turningDownDarkensAsWellAsQuietens` | DONE |
| VK-T2 (§6) | Test: treble bleed restores top | - | n/a | `Circuit::aKinmanBleedKeepsTheTop` | DONE |
| VK-T3 (§6) | Test: active mode removes loading | - | n/a | `Circuit::activeModeRemovesTheLoading` | DONE |
| VK-T4 (§6) | Test: pot value moves the resonance | - | n/a | `Circuit::potValueMovesTheResonance` | DONE |
| VK-T5 (§6) | Test: cable capacitance moves the resonance | - | n/a | `Circuit::cableCapacitanceMovesTheResonance` | DONE |
| VK-T6 (§6) | Test: tone sweeps down, never above unity | - | n/a | `Circuit::theToneControlSweepsDownAndNeverBoosts` | DONE |
| VK-T7 (§6) | Test: bypass is neutral (against the bare coil, per DECISIONS) | - | n/a | `Circuit::bypassIsNeutral` | DONE |
| VK-T8 (§6) | Test: stability over the advanced ranges at 44.1-192 kHz | - | n/a | `Circuit::everyCornerOfTheAdvancedRangeIsStable` | DONE |
| VK-T9 (§6) | Test: no allocation while sweeping | - | n/a | `Circuit::sweepingEveryControlDoesNotAllocate` | DONE |
| VK-T10 (§6) | Test: feedback coupling | - | n/a | `Feedback::theVolumeKnobLowersTheLoopByTheCircuitsAttenuation` | DONE |

<!-- counts DONE=31 NO-GUI=0 NO-TEST=0 PARTIAL=1 MISSING=1 OWNED=1 DEFERRED=1 -->
