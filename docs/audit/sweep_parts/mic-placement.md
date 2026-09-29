## mic-placement.md

Continuous mic placement does not exist on this checkout: no `MicPlacementModel/Stage`, `CabinetVoices.h`, `TptSvf`, `AcousticMicModel`, `AcousticLandmarks`, migration or mirror, none of the 25 new parameters, and no `MicPlacementView`/`Editor`, Easy pad or MP tests. What exists is the legacy discrete placement the spec keeps for compatibility: `mic_position`/`mic_distance`(`_2`) choices, reachable as combos in Advanced Column 3 CAB and loaded as grid IRs through `ParameterBridge::applyStructural`, plus the hooks the spec names (`CabinetEngine::getIrLoadCount`, `writtenSinceGuitarType`/`lastWrite`, `BodyEngine::getAirResonanceHz`, `ExpSmoother`). No FEAT branch pushed yet; new work is OWNED.

| Req | Summary | Engine location | GUI location | Test | Status |
|---|---|---|---|---|---|
| MP-4a (§4) | Legacy `mic_position`, `mic_distance`, `mic_position_2`, `mic_distance_2` remain in the list at current indices/ranges with visible controls (future Quick combos) | `Parameters.h:227-232` | ADVANCED Col 3 CAB, `AdvancedPanel.cpp:813-821` (`micPosition`, `micDistance`, `micPosition2`, `micDistance2`) | `GuiReach::everyAutomatableParameterHasAVisibleControl`, `GuiReach::operatingEachControlWritesItsParameter` | DONE |
| MP-0 (§0) | Ground rules: one anchor IR per (cab, speaker, mic), continuous parametric stage, identity at anchor, named physics, click-free TPT SVFs at 32-sample control rate, one shared model, no samples — not built (placement still swaps IRs via `applyStructural`) | - | - | - | OWNED |
| MP-2.1 (§2.1) | Coordinates: `mic_speaker`, cone-landmark `u`, per-size radii, cabinet speaker/height table, snap rings, distance/angle/rear — not built | - | - | - | OWNED |
| MP-2.2 (§2.2) | `MicPlacementModel::evaluate` terms (beaming weight, presence/axis knots, HF corner knots, angle/polar/polarity, proximity knots, dust cap, surround, HF shelves, rear, level, level match, floor bounce by room ρ, per-speaker variation); `kSpeakers/kMics` moved to `DSP/Amp/CabinetVoices.h` + `kMicPolar`/`kCabGeometry` — not built (tables still anonymous in `CabinetEngine.cpp:23,45`) | - | n/a | - | OWNED |
| MP-2.3 (§2.3) | Two-mic time of arrival: Physical/Aligned, rear re-inversion, `mic_phase_align` kept, Lagrange line with slew limit — not built | - | - | - | OWNED |
| MP-3 (§3) | Acoustic: `ac_mic_along/across` landmarks via `computeAcousticLandmarks`, `AcousticMicModel` radiator weights + composite table, mic voice/shared terms, calibration K, `ac_mic_mix` after `circuit.process` (skipped at 0), mic 2 equal-power, AcousticDI bypass — not built | - | - | - | OWNED |
| MP-4b (§4) | Legacy->continuous mapping tables, continuous->legacy mirror written only into serialised output, `MicPlacementMigration::apply` at preset load / setState / snapshot recall / morph endpoint, legacy host-write mapping with ±250 ms rule, legacy params no longer structural — not built | - | n/a | - | OWNED |
| MP-5 (§5) | `MicPlacementStage` in `CabinetEngine::MicPath` (7 SVFs, signed smoothed gain, floor + ToF delays, `TptSvf` in `DspCommon.h`), processing order, smoothing, new CabinetEngine API, anchor-only `findCabIr`, bridge push, acoustic branch lerp, `RoomEngine::setCloseMicDistance` bleed with d_c table, Aux 3/4/5 routing — not built | - | n/a | - | OWNED |
| MP-6.1 (§6.1) | Advanced Col 3 CAB `MicPlacementView` (face, speaker thumbnail, dist/angle/rear, Quick combos, mini-plot, ToF/level-match); acoustic "MICROPHONES" variant with Pickup<->Mic and Mic 2 — absent | - | - | - | OWNED |
| MP-6.2 (§6.2) | `MicPlacementEditor` over Cols 3-4 (cabinet/cone/mic rendering, snap, wheel, side view, right-click menu, reset, response plot with notch readout, status chips) — absent | - | - | - | OWNED |
| MP-6.3 (§6.3) | Easy Cabinet card "Mic: bright<->warm" pad, mic 2 ghost, acoustic knob, double-click overlay — absent | - | - | - | OWNED |
| MP-6.4 (§6.4) | Empty states/errors table — absent | - | - | - | OWNED |
| MP-6.5 (§6.5) | Options APPEARANCE "Snap mics to landmarks", "Show mic response plot"; `UiState::micGrilleVisible/micFocusedHandle` — absent | - | - | - | OWNED |
| MP-7 (§7) | 25 params appended last in order with ranges/defaults, `RangeFamily::mic`, preset `ranges.families` "mic", mod-matrix destinations, automatable in Free — absent | - | - | - | OWNED |
| MP-8 (§8) | State round-trip, undo classes (3.5 multi-target drag, 3.1, 3.2 Quick combo, 3.3, Reset), accessibility (handle groups with 4 sliders, value text, key table, reduced motion, Easy pad) — not built | - | - | - | OWNED |
| MP-9 (§9) | Interactions: user IR bypasses stage + hides handle, morph, mod/learn/automation, randomize limits, factory acoustic presets `ac_mic_mix`≈0.5, Free anchor IR subset — not built | - | - | - | OWNED |
| MP-10 (§10) | Failure modes table — not built | - | n/a | - | OWNED |
| MP-11 (§11) | Budget: CabinetEngine 0.5 units, AcousticMicModel 0.12 (0 at mix 0), no alloc, plot 0.5 ms — not built | - | n/a | - | OWNED |
| MP-12 (§12) | Both editions in full, no `ProFeatureGuard` entry — not built | - | n/a | - | OWNED |
| MP-T1 (§13 MP-01..06) | Layout, identity, legacy fidelity, migration map/trigger, mirror — none | - | - | - | OWNED |
| MP-T2 (§13 MP-07..19) | DSP physics tests (radius, angle, proximity, beaming, click-free, RT safety, IR load count, ToF, slew, rear, level match, room bleed, speaker variation) — none | - | - | - | OWNED |
| MP-T3 (§13 MP-20..25) | Acoustic off bit-identical, acoustic positions, landmarks, determinism, CPU, plot=engine — none | - | - | - | OWNED |
| MP-T4 (§13 MP-26..34) | GUI tests (CAB view drag/undo, keyboard, a11y, snap, Easy pad, editor, switches, user IR, reflow) — none | - | - | - | OWNED |
| MP-T5 (§13 MP-35..37) | Combination (morph, round trip, Free), LFO + rhythm, trademark/localisation — none | - | - | - | OWNED |

<!-- counts DONE=1 NO-GUI=0 NO-TEST=0 PARTIAL=0 MISSING=0 OWNED=23 -->
