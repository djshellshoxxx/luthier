## strum-dynamics.md

Strum dynamics are essentially complete: `Rhythm/StrumGesture` plans one gesture (crossing sps with the four-source priority, acceleration ease, direction, tilt, evenness, accent, weighted seeded misses, strikers, chuck), the STRUM group on RHYTHM attaches every control (evenness as rhythm-engine state per DECISIONS), strikers are mirrored in CHARACTER > PICK, and the Easy Feel knob maps as specified; `StrumDynamics.*` (25 tests) covers every §8 item. Remaining gaps: the up-stroke differs from the down-stroke only in force (no steeper/brighter down, no extra chirp/less click up), the MIDI chuck key range is deferred by DECISIONS, and the palm-mute-vs-chuck distinction has no test.

| Req | Summary | Engine location | GUI location | Test | Status |
|---|---|---|---|---|---|
| SD-1 (§0.1, 1) | One gesture in strings/sec; inter-string delay 1/sps; per-string events derived | `Rhythm/StrumGesture.*:plan`, `effectiveCrossingSps` | RHYTHM > STRUM `crossing` | `StrumDynamics.crossingTimingIsExact`, `StrumDynamics.rhythmEngineStrumsAreSpacedExactly` | DONE |
| SD-2 (§1.1) | Source priority MPE > live spread > pattern crossing_sps > global default | `RhythmEngine::getCrossingSource`; `MidiInterpreter` | STRUM `sourceLabel` (+ USE KNOB) | `StrumDynamics.crossingSourcesResolveInOrder`, `StrumDynamics.liveSpreadWins`, `StrumDynamics.mpePassesThrough`, `StrumDynamics.rhythmEngineSuppliesVelocityFromThePattern` | DONE |
| SD-3 (§2) | Acceleration ease blend (strum_acceleration 0.35) changes spacing not duration | `StrumGesture::ease` (inverted smoothstep, DECISIONS) | STRUM `acceleration` | `StrumDynamics.accelerationChangesSpacingNotDuration` | DONE |
| SD-4 (§2.1) | Down starts low, up starts high; up faster by strum_up_velocity_ratio (1.25) | `StrumGesture::plan`, `effectiveCrossingSps` | STRUM `upRatio` | `StrumDynamics.upIsFasterThanDownByTheRatio` | DONE |
| SD-5 (§0.3, 2.1) | Down steeper/harder/brighter; up shallower, softer, more chirp, less click — only force differs (`kUpStrokeForce` 0.85), no angle/brightness/noise change | `StrumGesture.cpp` ~281 | n/a | `StrumDynamics.upStrokesAreSofter` | PARTIAL |
| SD-6 (§3) | Tilt ramp (-1..+1, +0.15 down / mirrored up) | `StrumGesture` tiltFactor | STRUM `tilt` | `StrumDynamics.tiltAndAccentShapeTheForce` | DONE |
| SD-7 (§3) | Evenness ±40 % at 0, equal at 1, deterministic per seed and strum; stays rhythm-engine state (DECISIONS) | `StrumGesture` evennessFactor; `RhythmEngine::setStrumEvenness` | STRUM `evenness` (unattached slider to engine state) | `StrumDynamics.evennessBoundsTheVariation` | DONE |
| SD-8 (§3) | Accent +1.5 dB on the first two strings in the direction | `StrumGesture` accent | n/a | `StrumDynamics.tiltAndAccentShapeTheForce` | DONE |
| SD-9 (§3.1) | Misses (0.04), leading string 3x, seeded; humanise missPercent retained | `StrumGesture::plan`; `RhythmHumanise::missPercent` | STRUM `misses`; RHYTHM humanise | `StrumDynamics.missesAreWeightedAndDeterministic` | DONE |
| SD-10 (§4) | Guitar/bass default columns applied by family | `StrumSettings::guitarDefaults/bassDefaults/retargetDefaults` | n/a | `StrumDynamics.bassDefaultsApply` | DONE |
| SD-11 (§5) | Six strikers with crossing factors (thumb x0.6 ... brush x0.4) | `getStrikerCrossingFactor` | STRUM strikers; CHARACTER > PICK mirror | `StrumDynamics.strikersCrossAtTheirOwnSpeed` | DONE |
| SD-12 (§5) | Striker selects the pick-noise generator (thumb: no click) | `getStrikerMaterial`; `LuthierEngine` pick noise | n/a | `StrumDynamics.strikerSelectsTheNoise` | DONE |
| SD-13 (§5) | Separate strum_striker_down / _up | `Parameters.cpp` 756-757 | STRUM `strikerDown/Up`; `NoiseGroups` PICK mirror | `StrumDynamics.parametersReachTheEngine` | DONE |
| SD-14 (§6.1) | Chuck: all strings damped hard (chuck_damping 0.92) before the crossing; pitch gone, body kept | `StringEngine` `Damping::Chuck`; `LuthierEngine::triggerNote` | STRUM `chuckDamping` | `StrumDynamics.chuckKillsPitch` | DONE |
| SD-15 (§6.1) | chuck_amount blends toward a full mute | `StrumGesture` chuck | STRUM `chuckAmount` | `StrumDynamics.chuckStepsCarryTheChuck` | DONE |
| SD-16 (§6.1) | Chuck from the pattern's chuck step type | `RhythmEngine` chuck step | RHYTHM pattern editor | `StrumDynamics.chuckStepsCarryTheChuck`, `StrumDynamics.patternCrossingAndChuckRoundTrip` | DONE |
| SD-17 (§6.1) | Chuck from a MIDI note in the chuck key range — deferred by DECISIONS (no range specified) | none | none | - | MISSING |
| SD-18 (§6.2) | Palm-muted strum keeps pitch, chuck loses it — both available, no test of the distinction | `TechniqueEngine` palm mute; `Damping::Chuck` | n/a | - | NO-TEST |
| SD-19 (§6.3) | STRUM group on RHYTHM: crossing, acceleration, tilt, evenness, misses, two strikers, chuck amount/damping | `UI/StrumGroup.*` | RHYTHM > STRUM (`RhythmPanel`) | `StrumDynamics.theStrumGroupDrivesTheModel`, `GuiReach.everyAutomatableParameterHasAVisibleControl` | DONE |
| SD-20 (§6.3) | Easy Feel knob: crossing 60->400 sps, evenness 0.45->0.95, 0.5 = defaults | `StrumFeel` | Easy rhythm strip `rhythmFeelSlider` | `StrumDynamics.feelMapsAsSpecified`, `StrumDynamics.theEasyFeelKnobScalesTheStrum` | DONE |
| SD-21 (§7) | Params: crossing 20-800, acceleration, up ratio 0.5-2, tilt, misses, strikers, chuck x2 (+9); no PhysicalRange family | `Parameters.cpp` 751-759 | STRUM group | `StrumDynamics.parametersReachTheEngine` | DONE |
| SD-22 (§7) | Legacy strum_speed superseded, presets migrate | `Parameters.cpp` 575; preset migration | hidden (GuiReach `intentionallyHidden`) | `StrumDynamics.olderPresetsKeepTheirStrumSpeed` | DONE |
| SD-T1 (§8) | Test: crossing timing 5 ms ± 1 sample | | n/a | `StrumDynamics.crossingTimingIsExact` | DONE |
| SD-T2 (§8) | Test: acceleration spacing not duration | | n/a | `StrumDynamics.accelerationChangesSpacingNotDuration` | DONE |
| SD-T3 (§8) | Test: up faster by the ratio | | n/a | `StrumDynamics.upIsFasterThanDownByTheRatio` | DONE |
| SD-T4 (§8) | Test: rhythm engine supplies velocity | | n/a | `StrumDynamics.rhythmEngineSuppliesVelocityFromThePattern` | DONE |
| SD-T5 (§8) | Test: live spread wins | | n/a | `StrumDynamics.liveSpreadWins` | DONE |
| SD-T6 (§8) | Test: MPE passes through | | n/a | `StrumDynamics.mpePassesThrough` | DONE |
| SD-T7 (§8) | Test: misses weighted and deterministic | | n/a | `StrumDynamics.missesAreWeightedAndDeterministic` | DONE |
| SD-T8 (§8) | Test: evenness bounds variation | | n/a | `StrumDynamics.evennessBoundsTheVariation` | DONE |
| SD-T9 (§8) | Test: striker selects the noise | | n/a | `StrumDynamics.strikerSelectsTheNoise` | DONE |
| SD-T10 (§8) | Test: chuck kills pitch, keeps body | | n/a | `StrumDynamics.chuckKillsPitch` | DONE |
| SD-T11 (§8) | Test: Feel knob maps | | n/a | `StrumDynamics.feelMapsAsSpecified` | DONE |
| SD-T12 (§8) | Test: bass defaults apply | | n/a | `StrumDynamics.bassDefaultsApply` | DONE |

<!-- counts DONE=31 NO-GUI=0 NO-TEST=1 PARTIAL=1 MISSING=1 OWNED=0 -->
