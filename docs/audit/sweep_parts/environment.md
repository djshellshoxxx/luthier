## environment.md

This checkout still has the placeholder the spec replaces: `CharacterEngine`'s three-step temperature at 0.125 c/K (scaled by the character amount) and humidity multipliers that nothing in the engine reads, behind two combo boxes on CHARACTER; the only part of the spec that holds here is the unchanged tuner-drift LFO and its Retune button. The realism-a branch (active today, coverage complete) implements `EnvironmentModel`, the five parameters, the `environment` family, lags/profiles/seekable clock, body scaling, setup deltas, the corrosion hook, legacy migration and the ENVIRONMENT group with sparkline (spot-checked `EnvironmentGroup` in `CharacterPanel`, the Convolution note, and `Environment.ENV01..15`). Owner gaps: the SETUP group's "+x mm (humidity)" secondary line is deferred, no `performance-budget.md` EnvironmentModel row was added, and there is no ENV-13 in `EnvironmentTests` (it lives in BodyCoupling).

| Req | Summary | Engine location | GUI location | Test | Status |
|---|---|---|---|---|---|
| ENV-1 (§1) | Tuner drift LFO + settling envelope stay in CharacterEngine unchanged | `Character/CharacterEngine.cpp:getTunerDriftCents` | CHARACTER looseness slider, `CharacterPanel::loosenessSlider` | `Character.tunerDriftStaysWithinItsStatedAmplitude` | DONE |
| ENV-2 (§1) | Remove CharacterEngine temperature offset / humidity multipliers (move to EnvironmentModel) | (branch) `CharacterEngine` accessors removed after migration | (branch) combo boxes replaced by ENVIRONMENT group | (branch) `Character.legacyEnvironmentKeysAreReadNotWritten` | OWNED |
| ENV-3 (§0.1, 2.1) | Thermal detuning from each string's strain (α_s steel/nylon, α_n neck) | (branch) `Character/EnvironmentModel.*:thermalCents`; strains from `refreshStringPhysics` | n/a | (branch) `Environment.ENV02_steadySlopeFromTheStringsOwnNumbers` | OWNED |
| ENV-4 (§0.2, 2.2) | Lags: wire τ_s by diameter, neck 900 s, body 1200 s, wood moisture 1 day | (branch) `EnvironmentModel::advance/computeOutputs`, `wireTau` | n/a | (branch) `ENV03_coldCaseOvershootsThenSettles`, `ENV06_woodIsSlow` | OWNED |
| ENV-5 (§0.3) | Room defaults bit-exact no-op | (branch) exact-zero arithmetic | n/a | (branch) `ENV01_theRoomIsANoOp` | OWNED |
| ENV-6 (§0.4, 4) | Not scaled by character; drift loop runs with character disabled | (branch) `LuthierEngine::processSubBlock` drift loop | n/a | (branch) `ENV14_independentOfCharacter` | OWNED |
| ENV-7 (§2.3-2.4) | EMC isotherm; relief/top-rise/action deltas by chambering | (branch) `emc`, `topRise`, `computeOutputs` | n/a | (branch) `ENV05_humidityMovesTheGeometryAndThePlate` | OWNED |
| ENV-8 (§2.5) | Fretting-stretch cents from env geometry delta added in the per-block pitch loop | (branch) `stretchCents`, `fretCents`; `updatePerBlockModulation` | n/a | (branch) `ENV07_frettingStretchFromTheActionDelta` | OWNED |
| ENV-9 (§2.6, 4) | Plate freq/Q and air multipliers into BodyEngine + coupling bank; `BodyMode::isAir`; scaled `getAirResonanceHz` | (branch) `BodyEngine::setRuntimeScaling`, `BodyModels::buildModes` | n/a | (branch) `ENV04_theAirModeFollowsTheSpeedOfSound`, `BodyCoupling.ENV13_theWolfFollowsTheEnvironment` | OWNED |
| ENV-10 (§2.7) | Corrosion hook k_RH published to string aging | (branch) `EnvironmentState::corrosionRate` -> `StringAging::setCorrosionRate` | n/a | (branch) `StringAging.SA12_humidityDrivesCorrosion` | OWNED |
| ENV-11 (§3.1) | Six session profiles (Static, Stage lights, Outdoor evening, Cold case, AC studio, Humid club) | (branch) `EnvironmentModel::getProfile`, `lagResponse` | (branch) ENVIRONMENT profile box | (branch) `ENV03`, `ENV06`, `ENV09`, `ENV10` | OWNED |
| ENV-12 (§3.2) | Tuned-at reference; Retune zeroes offsets and writes env_tuned_at_c (one undo) | (branch) `EnvironmentModel::requestRetune` | (branch) `EnvironmentGroup::retune`; CHARACTER Retune shares it | (branch) `ENV10_retuneZeroesThenDriftsAgain`, `RealismUi.retuneWritesTunedAtAndZeroesTheOffsets` | OWNED |
| ENV-13 (§3.3-3.4) | Closed-form profile, automation by superposition; host-timeline / free-running clock, seekable | (branch) `EnvironmentModel::advance` clock | (branch) ENVIRONMENT clock box | (branch) `ENV09_seekingIsDeterministic` | OWNED |
| ENV-14 (§4) | Setup geometry: requested + env deltas to FretBuzzModel when a delta moves > 0.005 mm | (branch) `LuthierEngine::advanceRealism`, `setupWithGuitar` | n/a | (branch) `ENV08_aDryNeckBuzzesMore` | OWNED |
| ENV-15 (§5) | Params env_temperature_c / env_tuned_at_c / env_humidity_pct / env_profile / env_clock | (branch) `Parameters.*` REALISM-A block | (branch) ENVIRONMENT group | (branch) `ENV15_budgetSafetyAndCorners`, param count | OWNED |
| ENV-16 (§5) | `environment` range family; tooltip notes freezing / glass transition not modelled | (branch) `RangeFamily::environment` | (branch) CHARACTER padlock | (branch) `ENV15` | OWNED |
| ENV-17 (§6) | Reference pair in character block; legacy temperature -> °C, humidity -> 45 % (logged) | (branch) `EnvironmentModel::toVar/fromVar`; `applyRealismCharacterBlock` | n/a | (branch) `ENV11_legacyTemperatureAndHumidityMigrate` | OWNED |
| ENV-18 (§7) | CHARACTER ENVIRONMENT group: controls, Retune, 10 Hz readouts (string/neck/body temp, RH, cents), 60 s low-E sparkline | (branch) n/a | (branch) `UI/RealismGroups.*:EnvironmentGroup`, `EnvironmentSparkline` | (branch) `RealismUi.theCharacterPanelCarriesTheGroups` | OWNED |
| ENV-19 (§7) | Convolution-mode note "Body shift applies to modal bodies…" | (branch) n/a | (branch) `EnvironmentGroup` `convolutionNote` | (branch) build only | OWNED |
| ENV-20 (§7) | SETUP action/relief readouts show "+0.12 mm (humidity)" secondary line — owner defers | none | none (owner deferred) | - | MISSING |
| ENV-21 (§7) | CHARACTER padlock covers `environment` | (branch) n/a | (branch) `AdvancedPanel` CHARACTER `RangeTabButton` | (branch) `ENV15` | OWNED |
| ENV-22 (§8) | Budget 0.02 units, NaN guards, clamps (freq 0.7-1.3, Q 0.5-2, ±300 c) — perf-budget.md row not added by owner | (branch) `computeOutputs` clamps | n/a | (branch) `ENV15_budgetSafetyAndCorners` | OWNED |
| ENV-23 (§9 ENV-01) | Room no-op bit-identical | (branch) | n/a | (branch) `ENV01_theRoomIsANoOp` | OWNED |
| ENV-24 (§9 ENV-02) | Steady slope | (branch) | n/a | (branch) `ENV02_steadySlopeFromTheStringsOwnNumbers` | OWNED |
| ENV-25 (§9 ENV-03) | Cold-case overshoot | (branch) | n/a | (branch) `ENV03_coldCaseOvershootsThenSettles` | OWNED |
| ENV-26 (§9 ENV-04) | Air mode 1.033 | (branch) | n/a | (branch) `ENV04_theAirModeFollowsTheSpeedOfSound` | OWNED |
| ENV-27 (§9 ENV-05) | Humidity geometry | (branch) | n/a | (branch) `ENV05_humidityMovesTheGeometryAndThePlate` | OWNED |
| ENV-28 (§9 ENV-06) | Wood is slow | (branch) | n/a | (branch) `ENV06_woodIsSlow` | OWNED |
| ENV-29 (§9 ENV-07) | Fretting stretch | (branch) | n/a | (branch) `ENV07_frettingStretchFromTheActionDelta` | OWNED |
| ENV-30 (§9 ENV-08) | Dry neck buzzes more (owner bound 0.01 mm, recorded in spec) | (branch) | n/a | (branch) `ENV08_aDryNeckBuzzesMore` | OWNED |
| ENV-31 (§9 ENV-09) | Seek determinism | (branch) | n/a | (branch) `ENV09_seekingIsDeterministic` | OWNED |
| ENV-32 (§9 ENV-10) | Retune | (branch) | n/a | (branch) `ENV10_retuneZeroesThenDriftsAgain` | OWNED |
| ENV-33 (§9 ENV-11) | Legacy migration | (branch) | n/a | (branch) `ENV11_legacyTemperatureAndHumidityMigrate` | OWNED |
| ENV-34 (§9 ENV-12) | No steps (owner bound 0.1 c/block, recorded in spec) | (branch) | n/a | (branch) `ENV12_aTemperatureStepIsASlew` | OWNED |
| ENV-35 (§9 ENV-13) | Wolf follows the body | (branch) | n/a | (branch) `BodyCoupling.ENV13_theWolfFollowsTheEnvironment` | OWNED |
| ENV-36 (§9 ENV-14) | Independence from character | (branch) | n/a | (branch) `ENV14_independentOfCharacter` | OWNED |
| ENV-37 (§9 ENV-15) | Budget, zero allocation, 1000 corners | (branch) | n/a | (branch) `ENV15_budgetSafetyAndCorners` | OWNED |
