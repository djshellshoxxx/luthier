# REALISM-C coverage: noise floor, sustain and decay, tuning stability

Workstream REALISM-C implements `spec/noise-floor.md`, `spec/sustain-and-decay.md`
and `spec/tuning-stability.md`. Parameters: **29** (12 + 9 + 8), appended at the
end of the layout inside `REALISM-C` markers. Every default is neutral: NF-01,
SUS-01 and TS-01 prove factory presets render bit-identically.

## (a) Coverage

### noise-floor.md

| ID | Spec | Implementation | Verification | Status |
|---|---|---|---|---|
| NF-R1 | 0.1, 2 injection points | `LuthierEngine::processSubBlock` (pickup / circuit-in / DI / amp-in taps), `DSP/Noise/NoiseFloor.*` | NoiseFloor.aHumbuckerCancelsHumButNotAGroundLoop, theVolumeKnobActsOnHumOnly | verified |
| NF-R2 | 0.2 own module, not the pool | `NoiseFloor` class | code | verified |
| NF-R3 | 0.3 deterministic, reset reseeds | `NoiseFloor::reset`, `setSeed` from `character_seed` | NoiseFloor.rendersAreDeterministic | verified |
| NF-R4 | 0.5 zero is free | `NoiseFloor::isIdle`, engine skips | NoiseFloor.idleIsFree, defaultsAreBitIdentical | verified |
| NF-R5 | 1 hum relabelled "Single-coil Hum", ID kept | `Parameters.cpp` | Parameters suite | verified |
| NF-R6 | 2.1 position gain, legacy when 1 | `NoiseFloor::positionGain`, `PickupEngine::setHumPositionGain` | NoiseFloor.positionScalesTheHum | verified |
| NF-R7 | 2.1 hum calibration logged | NoiseFloorTests | NoiseFloor.humCalibration | verified (-58 dB, see decisions) |
| NF-R8 | 2.1 `noise_mains_hz` drives `setMainsFrequency` | `ParameterBridge::applyToEngine` | NoiseFloor.regionSetsTheHumFrequency | verified |
| NF-R9 | 2.2 fluorescent | `NoiseFloor::beginBlock` | NoiseFloor.fluorescentSpectrum | verified |
| NF-R10 | 2.3 passive hiss, physical | `NoiseFloor::johnsonVoltsRms`, `hissResistance` | NoiseFloor.passiveHissIsPhysical | verified |
| NF-R11 | 2.4 cable rolls, events, level | `NoiseFloor::onNoteOn`, `startCableEvent` | NoiseFloor.cableMovementRollsAndScales | verified |
| NF-R12 | 2.5 ground loop wavetable | `NoiseFloor::buildGroundLoopTable` | NoiseFloor.ampInputSourcesHitTheirTargets, aHumbuckerCancels... | verified |
| NF-R13 | 2.6 radio | `NoiseFloor::nextRadio` | NoiseFloor.ampInputSourcesHitTheirTargets, rendersAreDeterministic | verified |
| NF-R14 | 2.7 amp hiss, pinked, through the amp | `NoiseFloor::nextAmpHiss` | NoiseFloor.ampGainRaisesHiss | verified |
| NF-R15 | 2.8 microphonics bounded, 4x12 x0.2 | `NoiseFloor::pushAmpOutput`, `beginBlock` | NoiseFloor.microphonicsIsBounded | verified |
| NF-R16 | 3 twelve parameters, ranges, families | `Parameters.*`, `PhysicalRange.cpp` | Ranges suite, Integration parameter count | verified |
| NF-R17 | 3 styles, "(modified)" | `Presets/RealismStyles.h`, `RealismStyleActions.*` | NoiseFloor.styleOffIsInert, RealismUi.theStyleBoxesApplyAndReadModified | verified |
| NF-R18 | 3 Options default mains region (Init only) | `AudioPage`, `PresetManager::resetToDefaults` | code; see decisions | verified (Init only) |
| NF-R19 | 4.6 Aux 8 opt-in stem | engine string loop | NoiseFloor.aux8IsOptIn | verified |
| NF-R20 | 5 NOISE FLOOR group: style, 50/60, Guitar and Rig rows, pad, meter, Aux 8 | `UI/RealismGroups.*`, `CharacterPanel` | RealismUi.everyRealismCParameterHasAControl..., theNoiseMeterReadsAndGoesStale | verified |
| NF-R21 | 5 Aux 8 mirror on ROUTING | `RoutingPanel` | RealismUi.theAux8SwitchIsMirroredOnRouting | verified |
| NF-R22 | 6 serialization, old presets at defaults | APVTS | NoiseFloor.defaultsAreBitIdentical | verified |
| NF-R23 | 7 no allocation, budget row | `performance-budget.md` row | NoiseFloor.noAllocationOnTheAudioPath | verified |
| NF-R24 | 8 NF-17 sample-rate independence | tests at 44.1 / 96 kHz | regionSetsTheHumFrequency, positionScalesTheHum, ampGainRaisesHiss | verified |

### sustain-and-decay.md

| ID | Spec | Implementation | Verification | Status |
|---|---|---|---|---|
| SUS-R1 | 0.2-0.3 control-rate tick, legacy is a code path | `StringEngine::processSample`, `updateShapeTick` | SustainDecay.legacyIsBitIdentical | verified |
| SUS-R2 | 2.1 brightness overshoot | loop cutoff x b(t) in `updateLoopCoefficients` | SustainDecay.brightnessOvershoot | verified |
| SUS-R3 | 2.2 longitudinal ping, fret-scaled | `onShapeExcite`, `updateShapeConstants` | SustainDecay.longitudinalPing | verified |
| SUS-R4 | 3 two-stage decay | `updateShapeTick` m(t), T60 / m | SustainDecay.twoStageKnee, kneeDepth | verified |
| SUS-R5 | 3 E-Bow and feedback restart the clock | `LuthierEngine` E-Bow and feedback edges | code | verified |
| SUS-R6 | 4 tension pitch, clamps, reported frequency | `updateShapeTick`, `getCurrentFrequency` | SustainDecay.tensionMagnitude, tensionScalesWithLevelSquared, boundedAtTheLimits | verified |
| SUS-R7 | 5 release ramp, sag, ring | `StringEngine::release (letRing, fret)` | SustainDecay.releaseRamp, releaseSag, releaseRing | verified |
| SUS-R8 | 5 letRing / E-Bow skip | `release` | SustainDecay.lettingRingSkipsTheRelease | verified |
| SUS-R9 | 6 nine parameters, `strings` family, sustain_scale joins it | `Parameters.*`, `PhysicalRange.*` | Ranges suite | verified |
| SUS-R10 | 6.1 styles, one undo entry | `RealismStyleActions.*` | SustainDecay.styles | verified |
| SUS-R11 | 7 per-string state, Physical core/tension | `StringEngine.h`, `refreshStringPhysics` | tests above | verified |
| SUS-R12 | 8 Column 1 DECAY row + sketch | `DecayRow`, `AdvancedPanel` | RealismUi.sustainShapeReadoutAndDecaySketch | verified |
| SUS-R13 | 8 CHARACTER SUSTAIN SHAPE group + 30 Hz readout, stale | `SustainShapeGroup`, `PitchOffsetReadout` | RealismUi.sustainShapeReadoutAndDecaySketch | verified |
| SUS-R14 | 9 serialization, runtime state reset | APVTS, `StringEngine::reset` | legacyIsBitIdentical | verified |
| SUS-R15 | 10 cost, no allocation | | SustainDecay.costAndSafety | verified |
| SUS-R16 | 11 SUS-15 sample-rate independence | | SustainDecay.sampleRateIndependence, longitudinalPing | verified |
| SUS-R17 | 1 `sustain_scale` keeps working | not applied by the bridge before this spec either | - | deferred: wiring it re-voices a factory preset (see decisions) |

### tuning-stability.md

| ID | Spec | Implementation | Verification | Status |
|---|---|---|---|---|
| TS-R1 | 1 `stabilityCents` in the open-string sum | `TuningEngine` | TuningStability.offIsInert | verified |
| TS-R2 | 1 part fields consumed (tuners, nut, capo) | `PartAcoustics::mapSpec`, `setCapoPart` | TuningStability suite, RealismUi.theWorkshopInspector... | verified |
| TS-R3 | 2.1 settling, sigma, commit | `StabilityModel` | TuningStability.settling, resetAndDeterminism | verified |
| TS-R4 | 2.2 nut binding, ping | `advance`, `onPluck` | TuningStability.nutBinding, thePingReleasesTheBind | verified |
| TS-R5 | 2.3 backlash | `applyTuningChange`, `startBacklash` | TuningStability.backlashNeedsADownwardApproach | verified |
| TS-R6 | 2.4.1 floating equilibrium | `applyTuningChange`, `retuneString` | TuningStability.floatingEquilibrium | verified |
| TS-R7 | 2.4.2 creep | `advance` | TuningStability.creepTimeConstant | verified |
| TS-R8 | 2.4.3 return error | `advance` whammy tracking | TuningStability.resetAndDeterminism (performance with dives), costAndSafety | verified |
| TS-R9 | 2.5 bend memory | `advance` | TuningStability.bendMemory | verified |
| TS-R10 | 2.6 capo bias and capoComp | `capoBiasCents`, `retuneString` | TuningStability.capoBias | verified |
| TS-R11 | 3 Retune string / all, glide, clearDrift, retuneString | `StabilityModel`, `LuthierEngine::runStability`, `CharacterEngine::retuneString` | TuningStability.retuneScope, smoothGlides | verified |
| TS-R12 | 3 auto-retune modes | `advance` | TuningStability.autoRetuneOnIdle, autoRetuneOnTransportStop | verified |
| TS-R13 | 3 atomic command mask, not undoable | `requestRetune` | RealismUi.theOffsetStripRetunes..., theRetuneAllButton... | verified |
| TS-R14 | 3 MIDI Learn target for Retune all | - | - | deferred: the MIDI Learn action-target registry is another workstream's; `requestRetuneAll()` is the hook |
| TS-R15 | 4 eight parameters, `strings` family | `Parameters.*`, `PhysicalRange.cpp` | Ranges suite | verified |
| TS-R16 | 5 events from tuning presets and the detune sliders | `LuthierEngine::setTuningPreset`, `TuningPopover` | TuningStability.backlash..., floatingEquilibrium | verified |
| TS-R17 | 6 CHARACTER TUNING STABILITY group, offset strip, click to retune | `TuningStabilityGroup`, `OffsetStrip`, `CharacterPanel` | RealismUi.theOffsetStripRetunes... | verified |
| TS-R18 | 6 Easy headstock popover badge + Retune | `StabilityBadge`, `TuningPopover` | RealismUi.theHeadstockPopoverShowsOffsetsAndRetunes | verified |
| TS-R19 | 6 Workshop inspector figures | `describeTuningFigures`, `WorkshopPanel::refreshInspector` | RealismUi.theWorkshopInspectorShowsTheDerivedFigures | verified (capo figure shown on CHARACTER, see decisions) |
| TS-R20 | 7 session state, preset load resets | `StabilityModel::toVar/fromVar`, `PresetManager::fromVar`, processor state | TuningStability.serialization | verified |
| TS-R21 | 8 cost, no allocation, 0.5 c glide steps | | TuningStability.costAndSafety, smoothGlides | verified |
| TS-R22 | 9 TS-14 sample-rate independence | | creepTimeConstant, autoRetuneOnIdle | verified |

## (b) Decisions

- **Range family is `strings`**, not the specs' `string`: DECISIONS.md "Phase 2b range families" already named it; spec text corrected.
- **The shipped hum constant is kept**: it measures -58 dB re the reference pluck (1.08 units), outside noise-floor.md's -40 ±6; the spec says keep it, and NF-02 pins it.
- **Reference pluck constant** `NoiseFloor::kReferencePluckPeak = 1.08`, measured; every new source is calibrated against it.
- **Passive hiss density** is the physical 4kTR over 0..sr/2, and `RtRandom::nextGaussian` (variance 0.5) is rescaled; NF-11 integrates the warped continuous response because the DI is the bilinear circuit.
- **Radio needs the cable**: with `cable_on` off there is no antenna.
- **Microphonic loop gain** divides out the amp's measured gain at the resonance, so G_m is the loop gain whatever the amp; NF-08 isolates the loop with a stand-in amp because in the engine the body's tail keeps feeding it.
- **Mains preference seeds Init presets only**, not new instances, so presets older than the parameter render the same on every machine.
- **Region change rebuilds nothing**: the ground-loop table is one mains cycle.
- **Two mains accumulators**: the legacy hum's phase only advances while it sounds; the new sources keep their own.
- **Ping resonator is tau = 15 ms**, not Q 30 (which rings 2.4 ms); spec text corrected.
- **Brightness overshoot measured on 2-8 kHz**, not the centroid, which the string's fundamental dominates; SUS-02, SUS-05, SUS-09, SUS-10 thresholds/windows corrected with derivations in the spec.
- **Release ring gain covers one open-string period**, the period the loop is about to have.
- **`sustain_scale` stays unwired**: it was never applied by the bridge; wiring it would re-voice a factory preset.
- **Stock vs advanced clamps follow the values** (a value past stock implies the unlocked family) for the tension clamp and the stability caps.
- **Auto-retune needs amount > 0 and an offset**, so the default Idle mode leaves an existing preset's character drift alone.
- **Retune All button** sends the command and also calls `CharacterEngine::retune()` directly so the readout is right with no audio running.
- **Preset loads suppress tuning events** until the structural apply ends, so a preset's own tuning is not a detune event.
- **Session state as 17-digit text** for an exact round trip.
- **Capo gap from the part's kind** (screw 4 mm, else 6 mm; 5 mm with none); the capo figure lives under the offset strip because the Workshop inspector has no capo slot.
- **Allocation counter** shared through `realismCAllocationCount()` in CircuitTests.cpp, a uniquely named accessor to avoid clashing with other helpers.
- **Range registry counts its own rows**, so appended rows cannot leave `kNumEntries` stale.
- **gui-integration.md 19** gains the three rows; **performance-budget.md 1** gains the three budget rows.
