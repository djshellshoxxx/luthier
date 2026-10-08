## sustain-and-decay.md

REALISM-C has landed on this checkout: attack transient, longitudinal ping, two-stage decay, tension pitch, physical release, the parameters, styles, the Advanced DECAY row and the CHARACTER SUSTAIN SHAPE group, with 15 `SustainDecay.*` tests and the §19 row. Still open: `sustain_scale` (the Advanced "Sustain" knob) is attached but never applied (SUS-2), and nothing tests the E-Bow/feedback clock restart (SUS-6).

| Req | Summary | Engine location | GUI location | Test | Status |
|---|---|---|---|---|---|
| SUS-1 (§0.2-0.3, 1) | Legacy single-T60 model kept as the slow stage; neutral values skip code (bit-identical); 32-sample tick | `DSP/String/StringEngine.cpp:updateLoopCoefficients` (legacy); `updateShapeTick` | n/a | `SustainDecay.legacyIsBitIdentical` | DONE |
| SUS-2 (§1) | `sustain_scale` and per-note sustain scale keep working and scale the slow stage - per-note works; `ParamIDs::sustainScale` is read only by the range table, the CHARACTER T60 readout (`RealismGroupsC.cpp`) and the knob, never applied to the engine in `ParameterBridge` | per-note `StringEngine::setSustainScale` works; `sustain_scale` unread in `ParameterBridge` | ADVANCED Col 1 "Sustain" knob, `AdvancedPanel::sustain` | - | PARTIAL |
| SUS-3 (§2.1) | Brightness overshoot b(t); hammer/pull restart at 0.5 strength | loop cutoff × b(t) in `updateLoopCoefficients` | n/a | `SustainDecay.brightnessOvershoot` | DONE |
| SUS-4 (§2.2) | Longitudinal ping at f_L and 2f_L, fret-scaled, pre-body (owner: τ 15 ms) | `onShapeExcite`, `updateShapeConstants` | n/a | `SustainDecay.longitudinalPing` | DONE |
| SUS-5 (§3) | Two-stage decay m(t), knee at 10log10(1-a) | `updateShapeTick` | n/a | `SustainDecay.twoStageKnee`, `SustainDecay.kneeDepth` | DONE |
| SUS-6 (§3) | E-Bow and feedback reset the excite clock — `LuthierEngine` E-Bow edge restarts the clock; no test | `LuthierEngine` E-Bow / feedback edges | n/a | - | NO-TEST |
| SUS-7 (§4) | Tension-modulation pitch κ·level², clamp +25/+50 c; getCurrentFrequency reports it | `updateShapeTick`, `getCurrentFrequency` | n/a | `SustainDecay.tensionMagnitude`, `SustainDecay.tensionScalesWithLevelSquared`, `SustainDecay.boundedAtTheLimits` | DONE |
| SUS-8 (§5.1) | Release damping ramp over T_r (T_r = 0 = today) | `StringEngine::release(letRing, fret)` | n/a | `SustainDecay.releaseRamp` | DONE |
| SUS-9 (§5.2) | Release sag, fretted only | `release` | n/a | `SustainDecay.releaseSag` | DONE |
| SUS-10 (§5.3) | Release ring to open string, LightTouch 0.6 | `release` | n/a | `SustainDecay.releaseRing` | DONE |
| SUS-11 (§5) | letRing / E-Bow skip section 5; noise_release thump unchanged | `release` | n/a | `SustainDecay.lettingRingSkipsTheRelease` | DONE |
| SUS-12 (§6) | Range family (spec `string`, built as `strings`); sustain_scale joins (adv 0.05-4); DECISIONS entry | `PhysicalRange.*` | padlock | `Ranges.everyPhysicalRangeIsValid`, `Ranges.stockMatchesTheDeclaredRange` | DONE |
| SUS-13 (§6) | 8 params sustain_attack_transient/_time, fast_share/_ratio, tension_mod, release_time/_sag/_ring | `Parameters.*` REALISM-C block | SUSTAIN SHAPE group | `RealismUi.everyRealismCParameterHasAControlOnTheCharacterTab` | DONE |
| SUS-14 (§6.1) | sustain_style macro writes 8 values, "(modified)", one undo; ship default Legacy | `RealismStyleActions.*` | DECAY row + SUSTAIN SHAPE style box | `SustainDecay.styles`, `RealismUi.theStyleBoxesApplyAndReadModified` | DONE |
| SUS-15 (§7) | setSustainShape at block rate; per-string state; Physical coreDiameterMm/tensionNewtons | `StringEngine.h`, `refreshStringPhysics` | n/a | `SustainDecay.twoStageKnee`, `SustainDecay.sampleRateIndependence` | DONE |
| SUS-16 (§8) | Col 1 STRINGS DECAY row: style, Sustain knob, decay sketch | n/a | `DecayRow` in `AdvancedPanel` | `RealismUi.sustainShapeReadoutAndDecaySketch` | DONE |
| SUS-17 (§8) | CHARACTER SUSTAIN SHAPE group + 30 Hz per-string pitch readout, stale grey | n/a | `SustainShapeGroup`, `PitchOffsetReadout` | `RealismUi.sustainShapeReadoutAndDecaySketch` | DONE |
| SUS-18 (§8) | gui-integration §19 row | n/a | row added | n/a | DONE |
| SUS-19 (§9) | APVTS params; runtime state not saved, reset zeroes it | `StringEngine::reset` | n/a | `SustainDecay.legacyIsBitIdentical` | DONE |
| SUS-20 (§10) | Budget +0.3 units, no alloc, perf-budget row | - | n/a | `SustainDecay.costAndSafety` | DONE |
| SUS-21 (§11 SUS-01) | Legacy bit-identical | - | n/a | `SustainDecay.legacyIsBitIdentical` | DONE |
| SUS-22 (§11 SUS-02) | Two-stage knee | - | n/a | `SustainDecay.twoStageKnee` | DONE |
| SUS-23 (§11 SUS-03) | Knee depth | - | n/a | `SustainDecay.kneeDepth` | DONE |
| SUS-24 (§11 SUS-04) | Tension magnitude | - | n/a | `SustainDecay.tensionMagnitude` | DONE |
| SUS-25 (§11 SUS-05) | Tension ∝ level² | - | n/a | `SustainDecay.tensionScalesWithLevelSquared` | DONE |
| SUS-26 (§11 SUS-06) | Brightness overshoot (owner: 2-8 kHz band) | - | n/a | `SustainDecay.brightnessOvershoot` | DONE |
| SUS-27 (§11 SUS-07) | Longitudinal ping | - | n/a | `SustainDecay.longitudinalPing` | DONE |
| SUS-28 (§11 SUS-08) | Release ramp | - | n/a | `SustainDecay.releaseRamp` | DONE |
| SUS-29 (§11 SUS-09) | Release sag | - | n/a | `SustainDecay.releaseSag` | DONE |
| SUS-30 (§11 SUS-10) | Release ring | - | n/a | `SustainDecay.releaseRing` | DONE |
| SUS-31 (§11 SUS-11) | Bounded at limits | - | n/a | `SustainDecay.boundedAtTheLimits` | DONE |
| SUS-32 (§11 SUS-12) | Styles | - | n/a | `SustainDecay.styles` | DONE |
| SUS-33 (§11 SUS-13) | Letting ring skips release | - | n/a | `SustainDecay.lettingRingSkipsTheRelease` | DONE |
| SUS-34 (§11 SUS-14) | Cost and safety | - | n/a | `SustainDecay.costAndSafety` | DONE |
| SUS-35 (§11 SUS-15) | Sample-rate independence | - | n/a | `SustainDecay.sampleRateIndependence` | DONE |
