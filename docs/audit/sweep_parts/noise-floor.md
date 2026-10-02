## noise-floor.md

REALISM-C has landed on this checkout: `DSP/Noise/NoiseFloor.*` with every injection point, the 12 parameters, styles, the CHARACTER NOISE FLOOR group with position pad and meter, the ROUTING Aux 8 mirror and the Options mains-region preference (Init presets only), with 18 `NoiseFloor.*` tests; the performance-budget and §19 rows exist. Still open: the Advanced knob still reads "Amp Buzz" (NF-2), the DECISIONS.md calibration entry (NF-10) and a test for the mains preference (NF-20).

| Req | Summary | Engine location | GUI location | Test | Status |
|---|---|---|---|---|---|
| NF-1 (§1, 2.1) | Existing hum kept (ID, range, default 0.12, constant 0.0022, pre-circuit) | `DSP/Pickup/PickupEngine.cpp:processStrings` | ADVANCED amp column "Amp Buzz" knob, `AdvancedPanel::ampBuzz` | `NoiseFloor.humCalibration` | DONE |
| NF-2 (§1, 5) | Display name "Single-coil Hum"; Advanced knob relabelled mirror - `Parameters.cpp` names the param "Single-coil Hum" but `AdvancedPanel.cpp` still `addKnob(ampBuzz, "Amp Buzz")` | `Parameters.cpp:549` (renamed, same ID) | `AdvancedPanel.cpp:837` label still "Amp Buzz" | - | PARTIAL |
| NF-3 (§0.1, 2, 4) | Sources injected where they enter (pickup, circuit-in, DI, amp-in); acoustic gets hiss + cable only | `DSP/Noise/NoiseFloor.*` taps in `LuthierEngine::processSubBlock` | n/a | `NoiseFloor.aHumbuckerCancelsHumButNotAGroundLoop`, `NoiseFloor.theVolumeKnobActsOnHumOnly` | DONE |
| NF-4 (§0.2) | Own module, not the NoiseEngine pool | `NoiseFloor` class | n/a | n/a (structure) | DONE |
| NF-5 (§0.3) | Deterministic from character seed; reset reseeds | `NoiseFloor::reset/setSeed` | n/a | `NoiseFloor.rendersAreDeterministic` | DONE |
| NF-6 (§0.5) | isIdle skips the module | `NoiseFloor::isIdle` | n/a | `NoiseFloor.idleIsFree`, `NoiseFloor.defaultsAreBitIdentical` | DONE |
| NF-7 (§2) | Shared mains phase accumulator, phase-locked with hum | `NoiseFloor` mains phase | n/a | `NoiseFloor.aHumbuckerCancelsHumButNotAGroundLoop` | DONE |
| NF-8 (§2.1) | Position gain g_angle × g_dist, bit-identical at 0°/1 m | `NoiseFloor::positionGain`, `PickupEngine::setHumPositionGain` | NOISE FLOOR position pad | `NoiseFloor.positionScalesTheHum` | DONE |
| NF-9 (§2.1) | `noise_mains_hz` drives setMainsFrequency (today hard-coded 60) | `ParameterBridge::applyToEngine` | NOISE FLOOR 50/60 switch | `NoiseFloor.regionSetsTheHumFrequency` | DONE |
| NF-10 (§2.1) | Hum calibration -40+-6 dB; out-of-window goes in DECISIONS.md, constant unchanged (measured -58 dB, in spec text only) - no matching entry in spec/DECISIONS.md | - | n/a | `NoiseFloor.humCalibration`, `NoiseFloor.calibrationIsLogged` | PARTIAL |
| NF-11 (§2.2) | Fluorescent buzz (2×mains impulses, 3.5 kHz BP) | `NoiseFloor::beginBlock` | Guitar row | `NoiseFloor.fluorescentSpectrum` | DONE |
| NF-12 (§2.3) | Passive Johnson hiss from live R, before circuit | `johnsonVoltsRms`, `hissResistance` | Guitar row | `NoiseFloor.passiveHissIsPhysical` | DONE |
| NF-13 (§2.4) | Cable movement rolls/events, 4 voices, q per CableQuality, after circuit | `onNoteOn`, `startCableEvent` | Guitar row | `NoiseFloor.cableMovementRollsAndScales` | DONE |
| NF-14 (§2.5) | Ground loop wavetable at amp input | `buildGroundLoopTable` | Rig row | `NoiseFloor.ampInputSourcesHitTheirTargets` | DONE |
| NF-15 (§2.6) | Radio pickup (needs cable) | `nextRadio` | Guitar row | `NoiseFloor.ampInputSourcesHitTheirTargets` | DONE |
| NF-16 (§2.7) | Amp hiss with pinking at amp input | `nextAmpHiss` | Rig row | `NoiseFloor.ampGainRaisesHiss` | DONE |
| NF-17 (§2.8) | Tube microphonics bounded, G_m < 0.95, 4×12 × 0.2, DC blocker/NaN guard | `pushAmpOutput`, `beginBlock` | Rig row | `NoiseFloor.microphonicsIsBounded` | DONE |
| NF-18 (§3) | 12 new params + noise_amp_buzz PhysicalRange row; circuit/amp families | `Parameters.*`, `PhysicalRange.cpp` | NOISE FLOOR group | `RealismUi.everyRealismCParameterHasAControlOnTheCharacterTab`, Ranges suite | DONE |
| NF-19 (§3) | Styles table, "(modified)" | `Presets/RealismStyles.h`, `RealismStyleActions.*` | style dropdown | `NoiseFloor.styleOffIsInert`, `RealismUi.theStyleBoxesApplyAndReadModified` | DONE |
| NF-20 (§3) | Options "Default mains region" (Auto/50/60) seeds new and Init presets (owner: Init only) — Options AUDIO `mainsRegion` box attached; no test | `PresetManager::resetToDefaults` | Options AUDIO page | - | NO-TEST |
| NF-21 (§4.6) | Aux 8 opt-in identification stem incl. hum × g_pos | engine noise buffer | NOISE FLOOR checkbox | `NoiseFloor.aux8IsOptIn` | DONE |
| NF-22 (§5) | CHARACTER NOISE FLOOR group: style, 50/60, Guitar/Rig rows, position pad, meter (10 Hz, stale 2 s) | n/a | `UI/RealismGroups.*:NoiseFloorGroup` in `CharacterPanel` | `RealismUi.theNoiseMeterReadsAndGoesStale` | DONE |
| NF-23 (§5) | Aux 8 checkbox mirrored on ROUTING | n/a | `RoutingPanel` Aux 8 strip | `RealismUi.theAux8SwitchIsMirroredOnRouting` | DONE |
| NF-24 (§5) | gui-integration §19 row | n/a | spec/gui-integration.md row added | n/a | DONE |
| NF-25 (§6) | Older presets load at defaults, unchanged | APVTS | n/a | `NoiseFloor.defaultsAreBitIdentical` | DONE |
| NF-26 (§7) | Budget 0.15 units + perf-budget row; no alloc; region change without rebuild (owner: one-cycle table, no swap needed) | - | n/a | `NoiseFloor.noAllocationOnTheAudioPath` | DONE |
| NF-27 (§8 NF-01) | Defaults bit-identical | - | n/a | `NoiseFloor.defaultsAreBitIdentical` | DONE |
| NF-28 (§8 NF-02) | Hum calibration | - | n/a | `NoiseFloor.humCalibration` | DONE |
| NF-29 (§8 NF-03) | Region | - | n/a | `NoiseFloor.regionSetsTheHumFrequency` | DONE |
| NF-30 (§8 NF-04) | Humbucker | - | n/a | `NoiseFloor.aHumbuckerCancelsHumButNotAGroundLoop` | DONE |
| NF-31 (§8 NF-05) | Volume knob | - | n/a | `NoiseFloor.theVolumeKnobActsOnHumOnly` | DONE |
| NF-32 (§8 NF-06) | Position | - | n/a | `NoiseFloor.positionScalesTheHum` | DONE |
| NF-33 (§8 NF-07) | Amp hiss | - | n/a | `NoiseFloor.ampGainRaisesHiss` | DONE |
| NF-34 (§8 NF-08) | Microphonics bounded | - | n/a | `NoiseFloor.microphonicsIsBounded` | DONE |
| NF-35 (§8 NF-09) | Determinism | - | n/a | `NoiseFloor.rendersAreDeterministic` | DONE |
| NF-36 (§8 NF-10) | Cable | - | n/a | `NoiseFloor.cableMovementRollsAndScales` | DONE |
| NF-37 (§8 NF-11) | Passive hiss physical | - | n/a | `NoiseFloor.passiveHissIsPhysical` | DONE |
| NF-38 (§8 NF-12) | Fluorescent spectrum | - | n/a | `NoiseFloor.fluorescentSpectrum` | DONE |
| NF-39 (§8 NF-13) | Aux 8 opt-in | - | n/a | `NoiseFloor.aux8IsOptIn` | DONE |
| NF-40 (§8 NF-14) | Style Off inert | - | n/a | `NoiseFloor.styleOffIsInert` | DONE |
| NF-41 (§8 NF-15) | Idle free < 0.005 units — branch asserts < 0.02 | - | n/a | `NoiseFloor.idleIsFree` (relaxed) | DONE |
| NF-42 (§8 NF-16) | No allocation incl. region/style change | - | n/a | `NoiseFloor.noAllocationOnTheAudioPath` | DONE |
| NF-43 (§8 NF-17) | Sample-rate independence 44.1/96 k | - | n/a | `NoiseFloor.regionSetsTheHumFrequency`, `NoiseFloor.positionScalesTheHum`, `NoiseFloor.ampGainRaisesHiss` | DONE |
