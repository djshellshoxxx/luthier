## noise-floor.md

On this checkout only the pre-existing single-coil hum exists (`PickupEngine::processStrings`, `noise_amp_buzz` still named "Amp Buzz", mains fixed at 60 Hz, no test); there is no `NoiseFloor` module, none of the 12 new parameters, styles, NOISE FLOOR group, meter or Aux 8 option. The realism-c branch (last commit 2026-09-24 15:52, coverage complete) implements the module with all injection points, parameters, styles, the CHARACTER NOISE FLOOR group with position pad and meter, the ROUTING Aux 8 mirror, the Options mains-region preference (Init presets only) and `NoiseFloor.*` tests, and adds the performance-budget and §19 rows. Owner gaps: the Advanced panel knob is still labelled "Amp Buzz" (spec wants a relabelled mirror), NF-15's idle bound was relaxed from 0.005 to 0.02 units, and the -58 dB hum calibration is recorded in the spec text rather than DECISIONS.md.

| Req | Summary | Engine location | GUI location | Test | Status |
|---|---|---|---|---|---|
| NF-1 (§1, 2.1) | Existing hum kept (ID, range, default 0.12, constant 0.0022, pre-circuit) | `DSP/Pickup/PickupEngine.cpp:processStrings` | ADVANCED amp column "Amp Buzz" knob, `AdvancedPanel::ampBuzz` | (branch) `NoiseFloor.humCalibration` (no test here) | OWNED |
| NF-2 (§1, 5) | Display name "Single-coil Hum"; Advanced knob relabelled mirror — branch renames the param but AdvancedPanel label still "Amp Buzz" | (branch) `Parameters.cpp` renamed | `AdvancedPanel.cpp` `addKnob(ampBuzz, "Amp Buzz")` unchanged on branch | - | PARTIAL |
| NF-3 (§0.1, 2, 4) | Sources injected where they enter (pickup, circuit-in, DI, amp-in); acoustic gets hiss + cable only | (branch) `DSP/Noise/NoiseFloor.*` taps in `LuthierEngine::processSubBlock` | n/a | (branch) `NoiseFloor.aHumbuckerCancelsHumButNotAGroundLoop`, `theVolumeKnobActsOnHumOnly` | OWNED |
| NF-4 (§0.2) | Own module, not the NoiseEngine pool | (branch) `NoiseFloor` class | n/a | (branch) code | OWNED |
| NF-5 (§0.3) | Deterministic from character seed; reset reseeds | (branch) `NoiseFloor::reset/setSeed` | n/a | (branch) `rendersAreDeterministic` | OWNED |
| NF-6 (§0.5) | isIdle skips the module | (branch) `NoiseFloor::isIdle` | n/a | (branch) `idleIsFree`, `defaultsAreBitIdentical` | OWNED |
| NF-7 (§2) | Shared mains phase accumulator, phase-locked with hum | (branch) `NoiseFloor` mains phase | n/a | (branch) `aHumbuckerCancelsHumButNotAGroundLoop` | OWNED |
| NF-8 (§2.1) | Position gain g_angle × g_dist, bit-identical at 0°/1 m | (branch) `NoiseFloor::positionGain`, `PickupEngine::setHumPositionGain` | (branch) NOISE FLOOR position pad | (branch) `positionScalesTheHum` | OWNED |
| NF-9 (§2.1) | `noise_mains_hz` drives setMainsFrequency (today hard-coded 60) | (branch) `ParameterBridge::applyToEngine` | (branch) NOISE FLOOR 50/60 switch | (branch) `regionSetsTheHumFrequency` | OWNED |
| NF-10 (§2.1) | Hum calibration -40±6 dB; out-of-window goes in DECISIONS.md, constant unchanged (measured -58 dB, recorded in spec text only) | (branch) | n/a | (branch) `humCalibration`, `calibrationIsLogged` | OWNED |
| NF-11 (§2.2) | Fluorescent buzz (2×mains impulses, 3.5 kHz BP) | (branch) `NoiseFloor::beginBlock` | (branch) Guitar row | (branch) `fluorescentSpectrum` | OWNED |
| NF-12 (§2.3) | Passive Johnson hiss from live R, before circuit | (branch) `johnsonVoltsRms`, `hissResistance` | (branch) Guitar row | (branch) `passiveHissIsPhysical` | OWNED |
| NF-13 (§2.4) | Cable movement rolls/events, 4 voices, q per CableQuality, after circuit | (branch) `onNoteOn`, `startCableEvent` | (branch) Guitar row | (branch) `cableMovementRollsAndScales` | OWNED |
| NF-14 (§2.5) | Ground loop wavetable at amp input | (branch) `buildGroundLoopTable` | (branch) Rig row | (branch) `ampInputSourcesHitTheirTargets` | OWNED |
| NF-15 (§2.6) | Radio pickup (needs cable) | (branch) `nextRadio` | (branch) Guitar row | (branch) `ampInputSourcesHitTheirTargets` | OWNED |
| NF-16 (§2.7) | Amp hiss with pinking at amp input | (branch) `nextAmpHiss` | (branch) Rig row | (branch) `ampGainRaisesHiss` | OWNED |
| NF-17 (§2.8) | Tube microphonics bounded, G_m < 0.95, 4×12 × 0.2, DC blocker/NaN guard | (branch) `pushAmpOutput`, `beginBlock` | (branch) Rig row | (branch) `microphonicsIsBounded` | OWNED |
| NF-18 (§3) | 12 new params + noise_amp_buzz PhysicalRange row; circuit/amp families | (branch) `Parameters.*`, `PhysicalRange.cpp` | (branch) NOISE FLOOR group | (branch) `RealismUi.everyRealismCParameterHasAControlOnTheCharacterTab`, Ranges suite | OWNED |
| NF-19 (§3) | Styles table, "(modified)" | (branch) `Presets/RealismStyles.h`, `RealismStyleActions.*` | (branch) style dropdown | (branch) `styleOffIsInert`, `RealismUi.theStyleBoxesApplyAndReadModified` | OWNED |
| NF-20 (§3) | Options "Default mains region" (Auto/50/60) seeds new and Init presets (owner: Init only) | (branch) `PresetManager::resetToDefaults` | (branch) Options AUDIO page | (branch) code only | OWNED |
| NF-21 (§4.6) | Aux 8 opt-in identification stem incl. hum × g_pos | (branch) engine noise buffer | (branch) NOISE FLOOR checkbox | (branch) `aux8IsOptIn` | OWNED |
| NF-22 (§5) | CHARACTER NOISE FLOOR group: style, 50/60, Guitar/Rig rows, position pad, meter (10 Hz, stale 2 s) | (branch) n/a | (branch) `UI/RealismGroups.*:NoiseFloorGroup` in `CharacterPanel` | (branch) `RealismUi.theNoiseMeterReadsAndGoesStale` | OWNED |
| NF-23 (§5) | Aux 8 checkbox mirrored on ROUTING | (branch) n/a | (branch) `RoutingPanel` Aux 8 strip | (branch) `RealismUi.theAux8SwitchIsMirroredOnRouting` | OWNED |
| NF-24 (§5) | gui-integration §19 row | n/a | (branch) spec/gui-integration.md row added | n/a | OWNED |
| NF-25 (§6) | Older presets load at defaults, unchanged | (branch) APVTS | n/a | (branch) `defaultsAreBitIdentical` | OWNED |
| NF-26 (§7) | Budget 0.15 units + perf-budget row; no alloc; region change without rebuild (owner: one-cycle table, no swap needed) | (branch) | n/a | (branch) `noAllocationOnTheAudioPath` | OWNED |
| NF-27 (§8 NF-01) | Defaults bit-identical | (branch) | n/a | (branch) `defaultsAreBitIdentical` | OWNED |
| NF-28 (§8 NF-02) | Hum calibration | (branch) | n/a | (branch) `humCalibration` | OWNED |
| NF-29 (§8 NF-03) | Region | (branch) | n/a | (branch) `regionSetsTheHumFrequency` | OWNED |
| NF-30 (§8 NF-04) | Humbucker | (branch) | n/a | (branch) `aHumbuckerCancelsHumButNotAGroundLoop` | OWNED |
| NF-31 (§8 NF-05) | Volume knob | (branch) | n/a | (branch) `theVolumeKnobActsOnHumOnly` | OWNED |
| NF-32 (§8 NF-06) | Position | (branch) | n/a | (branch) `positionScalesTheHum` | OWNED |
| NF-33 (§8 NF-07) | Amp hiss | (branch) | n/a | (branch) `ampGainRaisesHiss` | OWNED |
| NF-34 (§8 NF-08) | Microphonics bounded | (branch) | n/a | (branch) `microphonicsIsBounded` | OWNED |
| NF-35 (§8 NF-09) | Determinism | (branch) | n/a | (branch) `rendersAreDeterministic` | OWNED |
| NF-36 (§8 NF-10) | Cable | (branch) | n/a | (branch) `cableMovementRollsAndScales` | OWNED |
| NF-37 (§8 NF-11) | Passive hiss physical | (branch) | n/a | (branch) `passiveHissIsPhysical` | OWNED |
| NF-38 (§8 NF-12) | Fluorescent spectrum | (branch) | n/a | (branch) `fluorescentSpectrum` | OWNED |
| NF-39 (§8 NF-13) | Aux 8 opt-in | (branch) | n/a | (branch) `aux8IsOptIn` | OWNED |
| NF-40 (§8 NF-14) | Style Off inert | (branch) | n/a | (branch) `styleOffIsInert` | OWNED |
| NF-41 (§8 NF-15) | Idle free < 0.005 units — branch asserts < 0.02 | (branch) | n/a | (branch) `idleIsFree` (relaxed) | OWNED |
| NF-42 (§8 NF-16) | No allocation incl. region/style change | (branch) | n/a | (branch) `noAllocationOnTheAudioPath` | OWNED |
| NF-43 (§8 NF-17) | Sample-rate independence 44.1/96 k | (branch) | n/a | (branch) `regionSetsTheHumFrequency`, `positionScalesTheHum`, `ampGainRaisesHiss` | OWNED |
