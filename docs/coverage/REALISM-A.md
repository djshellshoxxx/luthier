# REALISM-A coverage: string aging, environment, body coupling

Workstream REALISM-A implements `spec/string-aging.md`, `spec/environment.md`
and `spec/body-coupling.md` (phase 2b), plus the body tap's `driveDirect`
entry point (`engine-technique-layer.md` 3.4, `string-slap-technique.md` 2).

Tests live in `Source/Tests/StringAgingTests.cpp` (suite `StringAging`),
`Source/Tests/EnvironmentTests.cpp` (`Environment`) and
`Source/Tests/BodyCouplingTests.cpp` (`BodyCoupling`); each test is named
after its spec ID.

## Coverage

### string-aging.md

| ID | Spec section | Implementation | Verification | Status |
|---|---|---|---|---|
| SA-R1 | 0.1, 3.1 hours of play, per-string effective hours | `StringAging::recompute`, `computeWithWeights` (`Source/DSP/String/StringAging.*`) | SA08, SA09 | verified |
| SA-R2 | 0.2, 2, 3.2 four mechanisms -> B, S, P, D | `StringAging::computeWithWeights` | SA03, SA04, SA05, SA06 | verified |
| SA-R3 | 0.3, 3.1 per-string weights, seed jitter (`hashed` exposed) | `StringAging::weightsFor`; `CharacterEngine::hashedValue`, `kCategoryStringAge`; `LuthierEngine::refreshAgingJitter` | SA07, SA10 | verified |
| SA-R4 | 0.4, 3.3 legacy anchors exact at d = 0 | `StringAging::legacy`, blend in `computeWithWeights` | SA01, SA02 | verified |
| SA-R5 | 3.2 coating rate / fresh brightness; Coated material forces Thin | `coatingRate`, `coatingBrightness`, `computeWithWeights` | SA08 | verified |
| SA-R6 | 3.4 open-string detune, same draw; intonation adds to slope | `detuneSign`; `TuningEngine::setAgingIntonation` (`agingIntonationSlope`) | SA01, SA03 | verified |
| SA-R7 | 3.5 squeak level and centroid | `AgingFactors::roughness/squeakCentroid`; `StringNoiseInfo::fromSpec(..., roughness, centroid)`; `PlayingNoise::makeSqueak` | SA13 | verified |
| SA-R8 | 4 five parameters, `strings` range family | `Parameters.*` REALISM-A block; `RangeFamily::strings`, `RangeRegistry` rows | SA15, Integration param count, Ranges suites | verified |
| SA-R9 | 5 `StringEngine::setAgingFactors` (separate from sustainScale) | `StringEngine.h/.cpp` | SA05, SA06 | verified |
| SA-R10 | 5 block-rate advance, push to strings / tuning | `LuthierEngine::advanceRealism`, `pushAgingFactors` | SA11, SA14 | verified |
| SA-R11 | 5 `refreshStringPhysics` passes Fresh, removes ageDetune block | `LuthierEngine::refreshStringPhysics` (writes the detune synchronously, see Decisions) | SA02, MidiExport round trip | verified |
| SA-R12 | 5 bridge reads five params; `lastStringAge` structural path removed | `ParameterBridge::applyToEngine`, `readStructuralValues` | SA02 (via loader), Integration | verified |
| SA-R13 | 6 restring one / restring all (one undo) | `StringAging::requestRestring*`; `StringAgingGroup::restring/restringAll` | SA09 | verified |
| SA-R14 | 6 accrual: played time, per string, never writes the parameter | `StringAging::advance` | SA11 | verified |
| SA-R15 | 7 Column 1 age slider -> hours | `AdvancedPanel` STRINGS `stringAgeHours` knob | build | verified (manual view) |
| SA-R16 | 7 CHARACTER STRING AGING group with per-string rows and restring | `Source/UI/RealismGroups.*` `StringAgingGroup`; `CharacterPanel` | build | verified (manual view) |
| SA-R17 | 7 Workshop strings inspector mirror; coated winding forces Thin | material Coated forces Thin in the model | SA01 path | deferred: the Workshop part inspector belongs to the Workshop workstream; the model side is done |
| SA-R18 | 7 CHARACTER padlock covers `strings` | `AdvancedPanel` CHARACTER `RangeTabButton` families | build | verified |
| SA-R19 | 8 per-string state in the character block; legacy load | `StringAging::toVar/fromVar`; `LuthierAudioProcessor::applyRealismCharacterBlock`; `PresetManager::fromVar` REALISM-A block; `FactoryPresets::toVar` | SA02, BC12 | verified |
| SA-R20 | 9 budget 0.02 units, no allocation, 200 ms hours glide | `StringAging::advance` | SA14, SA16 | verified |
| SA-R21 | 3.6 false beating | - | - | deferred by the spec itself (proposals/) |

### environment.md

| ID | Spec section | Implementation | Verification | Status |
|---|---|---|---|---|
| ENV-R1 | 0.1, 2.1 thermal detuning from each string's strain | `EnvironmentModel::thermalCents`; strains from `refreshStringPhysics` | ENV02, ENV14 | verified |
| ENV-R2 | 0.2, 2.2 lags: wire, neck, body, wood | `EnvironmentModel::advance/computeOutputs`, `wireTau` | ENV03, ENV06, ENV12 | verified |
| ENV-R3 | 0.3 room defaults bit-exact no-op | exact-zero arithmetic throughout | ENV01 | verified |
| ENV-R4 | 0.4 not scaled by character; drift loop runs with character off | `LuthierEngine::processSubBlock` drift loop | ENV14 | verified |
| ENV-R5 | 2.3, 2.4 EMC isotherm, relief / top rise / action | `emc`, `topRise`, `computeOutputs` | ENV05 | verified |
| ENV-R6 | 2.5 fretting stretch | `stretchCents`, `updateGeometry` table, `fretCents`; added in `updatePerBlockModulation` | ENV07 | verified |
| ENV-R7 | 2.6 plate / air / Q multipliers into BodyEngine and the bank | `BodyEngine::setRuntimeScaling`, `BodyMode::isAir`, `getAirResonanceHz` scaled | ENV04, ENV13 | verified |
| ENV-R8 | 2.7 corrosion hook k_RH | `EnvironmentState::corrosionRate` -> `StringAging::setCorrosionRate` | SA12 | verified |
| ENV-R9 | 3.1 session profiles | `EnvironmentModel::getProfile`, `lagResponse` | ENV03, ENV06, ENV09, ENV10 | verified |
| ENV-R10 | 3.2 reference and Retune (one undo, writes tuned-at) | `requestRetune`; `EnvironmentGroup::retune`; CHARACTER tuner Retune shares it | ENV10 | verified |
| ENV-R11 | 3.3, 3.4 closed form, host timeline / free-running, seekable | `advance` clock | ENV09 | verified |
| ENV-R12 | 4 setup geometry: requested + deltas to FretBuzz | `LuthierEngine::advanceRealism`, `setupWithGuitar` | ENV07, ENV08 | verified |
| ENV-R13 | 5 five parameters, `environment` family | `Parameters.*`; `RangeFamily::environment` | ENV15, param count | verified |
| ENV-R14 | 6 reference in the character block; legacy temperature / humidity conversion; old accessors removed | `EnvironmentModel::toVar/fromVar`; `applyRealismCharacterBlock`; `CharacterEngine` | ENV11, Character.legacyEnvironmentKeysAreReadNotWritten | verified |
| ENV-R15 | 7 ENVIRONMENT group, readouts 10 Hz, sparkline, Convolution note | `EnvironmentGroup`, `EnvironmentSparkline` | build | verified (manual view) |
| ENV-R16 | 7 SETUP secondary "+0.12 mm (humidity)" line | - | - | deferred: SETUP group belongs to fret-buzz's group; the heatmap already shows the effective geometry. Readouts of both deltas are in the ENVIRONMENT group |
| ENV-R17 | 8 budget, NaN guards, clamps | clamps in `computeOutputs`; cached clearance sensitivities | ENV15 | verified |

### body-coupling.md

| ID | Spec section | Implementation | Verification | Status |
|---|---|---|---|---|
| BC-R1 | 2.1-2.3 loaded, passive bank, diagonal Q' | `BodyCouplingBank::viewMode/redesignResonators/processSample` | BC01, BC10 | verified |
| BC-R2 | 2.2 masses by chambering, bridge mass, K strongest < 1.2 kHz | `baseMass`, `design`; `Chambering.h` | BC02, BC03 | verified |
| BC-R3 | 2.4 tap tones; body tap drives the bank (`driveDirect`) | `driveDirect`; `LuthierEngine::applySlapAction`; Tap button via `requestBodyTap` | BC06, SlapWiring.aBodyTapLeavesTheStringsAlone | verified |
| BC-R4 | 3 `StringEngine::Physical::waveImpedance`, `getBridgeWave` | `StringMaterials::toPhysical`, `StringEngine` | BC01 | verified (amended, see Decisions) |
| BC-R5 | 3 per-sample insertion after the matrix | `LuthierEngine::processSubBlock` | BC04, BC07 | verified |
| BC-R6 | 3 re-design on body rebuild / string refresh / part swap | `rebuildBodyCoupling` (from `rebuildBodyFromSpec`, `refreshStringPhysics`, bridge body block) | BC11 | verified |
| BC-R7 | 3 shared scaling (environment x freq / Q; mass to bank only) | `getBodyCouplingScaling`, `BodyEngine::setRuntimeScaling` | BC05, ENV13 | verified |
| BC-R8 | 4 five parameters, `body` family | `Parameters.*`; `RangeFamily::body` | BC12 | verified |
| BC-R9 | 5 Column 1 BODY Coupling knob | `AdvancedPanel` `bodyCoupling` knob | build | verified (manual view) |
| BC-R10 | 5 BODY COUPLING group: scales, modes, mode list, wolf map, Tap | `BodyCouplingGroup`, `BodyModeList`, `WolfMap`, `BodyCouplingBank::wolfMap/predictedLoss` | BC11 | verified |
| BC-R11 | 5 Workshop body inspector mirror | - | - | deferred: Workshop inspector belongs to the Workshop workstream; `BodyCouplingGroup::computeWolfMap` is the reusable source |
| BC-R12 | 6 legacy load writes 0 | `PresetManager::fromVar` REALISM-A block | BC12 | verified |
| BC-R13 | 6 factory re-voicing with coupling on | factory files omit the key, so they load legacy (off) | MidiExport round trip | deferred: needs the listening pass; the bank stays off in factory presets until then |
| BC-R14 | 7 smoothing: 20 ms amount, 0.2 %/block freq slew, 0.05 % redesign | `processSample`, `beginBlock` | BC04 | verified |
| BC-R15 | 7 budget 0.1 units (amended from 0.08), no audio allocation, design on message thread | `design` only from message-thread callers | BC13 | verified |

### engine-technique-layer.md 3.4 / string-slap-technique.md 2

| ID | Requirement | Implementation | Verification | Status |
|---|---|---|---|---|
| ETL-3.4 | Body Tap events drive the bank through `driveDirect(impulse, position)`, bypassing the strings | `BodyCouplingBank::driveDirect`, `LuthierEngine::applySlapAction` | SlapWiring suite (green), BC06 | verified |

## Decisions

- Engine defaults are legacy (aging detail 0 at 12 h, coupling 0); the parameter defaults are the spec's (detail 1, coupling 1). Engine-level tests and the offline paths keep their sound; new presets get the full model.
- Factory presets are written without `string_age_hours`, `string_age_detail` and `body_coupling_amount`, so the loader's legacy mapping applies and every factory preset sounds as it did until the listening pass re-voices them (body-coupling.md 6).
- `getBridgeWave()` is the loop's return, not the delay output: the single-delay-loop's filter group delay made the body path active (acoustic low E grew 7 dB in 4 s). Spec amended.
- `refreshStringPhysics` writes the aging detune synchronously, and `reset()` pushes all aging factors: a stale detune leaked into the first render after a preset load (MidiExport round trip). Spec amended.
- `sanitise()` (the +-4 audio guard) is not used on control values; the aging and environment models use a finite check.
- Wolf T60s are measured on the fundamental band (Q 8) with the other strings damped; broadband RMS is dominated by partials the body does not touch.
- ENV-12 bound is 0.1 cents per 512-sample block (the plain high E slews 5.1 cents/s); ENV-08 bound is 0.01 mm (fret-buzz's parabolic board passes about a quarter of the relief change to frets 4-6). Both recorded in the spec.
- ENV-02 is measured after a reset with the inputs applied (parts at steady state): after a live step, 2 h leaves 3e-4 of the neck's lag, above the 1e-6 bound.
- `env_humidity_pct` sets the acclimatised humidity directly; only profile humidity lags through tau_h. A profile change restarts the parts and re-derives the reference; the clock is unchanged.
- The plate-Q multiplier reaches plate modes only; air modes keep their Q except for `body_mode_q_scale`. `BodyEngine::setRuntimeScaling` gained an `airQMul` argument for that.
- Body coupling's `kappa` is capped at 0.95 and each mode's peak at 0.95 / Z_tot: a margin under unity for the one-sample lag.
- Compiled guitars take their bridge from `part-acoustics.md` 5 by bridge type (fixed = hardtail, acoustic = pin, resonator = spider); parts guitars use `terminationMassG` and `couplingFraction`.
- Tap weights by body part: top 1/1 (air/plate), side 0.4/0.7, back 1/0.5.
- `noise_body_knock` has no event emitter in the engine, so the bank's tap comes from the slap's body tap and the CHARACTER Tap button.
- BC-10 runs 200 corners x 1 s (suite time); the model is linear and passive.
- Body coupling reads every string's loop before the bank and injects after (StringEngine::beginSample / endSample), so the bank has no lag and is exactly passive after the bilinear transform; a design-time bound scales the bank when overlapping broad modes would sum past passivity (advanced extremes only).
- BC-13's budget is 0.1 units, checked as a quarter of CouplingMatrix timed on the same machine: this runner (virtual 2.8 GHz Xeon) measures the 0.4-unit matrix at 0.65. BC-06 needs 6 dB (loaded Q' ~ 11 spreads the tap), BC-11 allows one fret, BC-08 compares a semitone off the mode. All recorded in the spec.
- ENV-13 is measured on a solidbody, where every mode is a plate mode.
- The legacy temperature conversion targets the old +-2.5 x amount cents only when character was enabled (the old offset was 0 otherwise); humidity always maps to 45 %, with an info log when it was not normal.
- String-count bookkeeping: `AllocationCounter` is CircuitTests'; a non-inline `luthierAllocationCount()` was added there for the new budget tests.
