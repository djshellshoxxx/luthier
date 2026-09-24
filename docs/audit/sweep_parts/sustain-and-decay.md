## sustain-and-decay.md

On this checkout the string has one legacy T60, a fixed pitch and an instant release; none of the attack transient, longitudinal ping, two-stage decay, tension pitch, physical release, nine parameters, styles, DECAY row or SUSTAIN SHAPE group exists. Worse, `sustain_scale` (the Advanced "Sustain" knob, `AdvancedPanel::sustain`) is declared and attached but never read by `ParameterBridge`, so the knob does nothing — and realism-c explicitly leaves it unwired (wiring it would re-voice a factory preset). The realism-c branch (active today, coverage complete) implements everything else with `SustainDecay.*` (15 tests), `DecayRow`/`SustainShapeGroup`, the `strings` family (named `strings`, not the spec's `string`, per DECISIONS "Phase 2b range families"), and adds the §19 and performance-budget rows.

| Req | Summary | Engine location | GUI location | Test | Status |
|---|---|---|---|---|---|
| SUS-1 (§0.2-0.3, 1) | Legacy single-T60 model kept as the slow stage; neutral values skip code (bit-identical); 32-sample tick | `DSP/String/StringEngine.cpp:updateLoopCoefficients` (legacy); (branch) `updateShapeTick` | n/a | (branch) `SustainDecay.legacyIsBitIdentical` | OWNED |
| SUS-2 (§1) | `sustain_scale` and per-note sustain scale keep working and scale the slow stage — param attached to the Sustain knob but never applied; owner defers | per-note `StringEngine::setSustainScale` works; `sustain_scale` unread in `ParameterBridge` | ADVANCED Col 1 "Sustain" knob, `AdvancedPanel::sustain` | - | PARTIAL |
| SUS-3 (§2.1) | Brightness overshoot b(t); hammer/pull restart at 0.5 strength | (branch) loop cutoff × b(t) in `updateLoopCoefficients` | n/a | (branch) `brightnessOvershoot` | OWNED |
| SUS-4 (§2.2) | Longitudinal ping at f_L and 2f_L, fret-scaled, pre-body (owner: τ 15 ms) | (branch) `onShapeExcite`, `updateShapeConstants` | n/a | (branch) `longitudinalPing` | OWNED |
| SUS-5 (§3) | Two-stage decay m(t), knee at 10log10(1-a) | (branch) `updateShapeTick` | n/a | (branch) `twoStageKnee`, `kneeDepth` | OWNED |
| SUS-6 (§3) | E-Bow and feedback reset the excite clock | (branch) `LuthierEngine` E-Bow / feedback edges | n/a | (branch) code only | OWNED |
| SUS-7 (§4) | Tension-modulation pitch κ·level², clamp +25/+50 c; getCurrentFrequency reports it | (branch) `updateShapeTick`, `getCurrentFrequency` | n/a | (branch) `tensionMagnitude`, `tensionScalesWithLevelSquared`, `boundedAtTheLimits` | OWNED |
| SUS-8 (§5.1) | Release damping ramp over T_r (T_r = 0 = today) | (branch) `StringEngine::release(letRing, fret)` | n/a | (branch) `releaseRamp` | OWNED |
| SUS-9 (§5.2) | Release sag, fretted only | (branch) `release` | n/a | (branch) `releaseSag` | OWNED |
| SUS-10 (§5.3) | Release ring to open string, LightTouch 0.6 | (branch) `release` | n/a | (branch) `releaseRing` | OWNED |
| SUS-11 (§5) | letRing / E-Bow skip section 5; noise_release thump unchanged | (branch) `release` | n/a | (branch) `lettingRingSkipsTheRelease` | OWNED |
| SUS-12 (§6) | Range family (spec `string`, built as `strings`); sustain_scale joins (adv 0.05-4); DECISIONS entry | (branch) `PhysicalRange.*` | (branch) padlock | (branch) Ranges suite | OWNED |
| SUS-13 (§6) | 8 params sustain_attack_transient/_time, fast_share/_ratio, tension_mod, release_time/_sag/_ring | (branch) `Parameters.*` REALISM-C block | (branch) SUSTAIN SHAPE group | (branch) `RealismUi.everyRealismCParameterHasAControlOnTheCharacterTab` | OWNED |
| SUS-14 (§6.1) | sustain_style macro writes 8 values, "(modified)", one undo; ship default Legacy | (branch) `RealismStyleActions.*` | (branch) DECAY row + SUSTAIN SHAPE style box | (branch) `styles`, `RealismUi.theStyleBoxesApplyAndReadModified` | OWNED |
| SUS-15 (§7) | setSustainShape at block rate; per-string state; Physical coreDiameterMm/tensionNewtons | (branch) `StringEngine.h`, `refreshStringPhysics` | n/a | (branch) suite above | OWNED |
| SUS-16 (§8) | Col 1 STRINGS DECAY row: style, Sustain knob, decay sketch | (branch) n/a | (branch) `DecayRow` in `AdvancedPanel` | (branch) `RealismUi.sustainShapeReadoutAndDecaySketch` | OWNED |
| SUS-17 (§8) | CHARACTER SUSTAIN SHAPE group + 30 Hz per-string pitch readout, stale grey | (branch) n/a | (branch) `SustainShapeGroup`, `PitchOffsetReadout` | (branch) `RealismUi.sustainShapeReadoutAndDecaySketch` | OWNED |
| SUS-18 (§8) | gui-integration §19 row | n/a | (branch) row added | n/a | OWNED |
| SUS-19 (§9) | APVTS params; runtime state not saved, reset zeroes it | (branch) `StringEngine::reset` | n/a | (branch) `legacyIsBitIdentical` | OWNED |
| SUS-20 (§10) | Budget +0.3 units, no alloc, perf-budget row | (branch) | n/a | (branch) `costAndSafety` | OWNED |
| SUS-21 (§11 SUS-01) | Legacy bit-identical | (branch) | n/a | (branch) `legacyIsBitIdentical` | OWNED |
| SUS-22 (§11 SUS-02) | Two-stage knee | (branch) | n/a | (branch) `twoStageKnee` | OWNED |
| SUS-23 (§11 SUS-03) | Knee depth | (branch) | n/a | (branch) `kneeDepth` | OWNED |
| SUS-24 (§11 SUS-04) | Tension magnitude | (branch) | n/a | (branch) `tensionMagnitude` | OWNED |
| SUS-25 (§11 SUS-05) | Tension ∝ level² | (branch) | n/a | (branch) `tensionScalesWithLevelSquared` | OWNED |
| SUS-26 (§11 SUS-06) | Brightness overshoot (owner: 2-8 kHz band) | (branch) | n/a | (branch) `brightnessOvershoot` | OWNED |
| SUS-27 (§11 SUS-07) | Longitudinal ping | (branch) | n/a | (branch) `longitudinalPing` | OWNED |
| SUS-28 (§11 SUS-08) | Release ramp | (branch) | n/a | (branch) `releaseRamp` | OWNED |
| SUS-29 (§11 SUS-09) | Release sag | (branch) | n/a | (branch) `releaseSag` | OWNED |
| SUS-30 (§11 SUS-10) | Release ring | (branch) | n/a | (branch) `releaseRing` | OWNED |
| SUS-31 (§11 SUS-11) | Bounded at limits | (branch) | n/a | (branch) `boundedAtTheLimits` | OWNED |
| SUS-32 (§11 SUS-12) | Styles | (branch) | n/a | (branch) `styles` | OWNED |
| SUS-33 (§11 SUS-13) | Letting ring skips release | (branch) | n/a | (branch) `lettingRingSkipsTheRelease` | OWNED |
| SUS-34 (§11 SUS-14) | Cost and safety | (branch) | n/a | (branch) `costAndSafety` | OWNED |
| SUS-35 (§11 SUS-15) | Sample-rate independence | (branch) | n/a | (branch) `sampleRateIndependence` | OWNED |
