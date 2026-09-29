## environment.md

REALISM-A has landed on this checkout: `Character/EnvironmentModel.*` replaces `CharacterEngine`'s three-step temperature and unused humidity multipliers, with the five parameters, the `environment` family, lags, profiles and the seekable clock, body scaling, setup deltas, the corrosion hook, legacy migration and the CHARACTER ENVIRONMENT group with its sparkline, and `Environment.ENV01..ENV15` are registered. Still open: the SETUP humidity readout line (ENV-20), a test for the convolution-mode note (ENV-19) and the `spec/performance-budget.md` row (ENV-22).

| Req | Summary | Engine location | GUI location | Test | Status |
|---|---|---|---|---|---|
| ENV-1 (§1) | Tuner drift LFO + settling envelope stay in CharacterEngine unchanged | `Character/CharacterEngine.cpp:getTunerDriftCents` | CHARACTER looseness slider, `CharacterPanel::loosenessSlider` | `Character.tunerDriftStaysWithinItsStatedAmplitude` | DONE |
| ENV-2 (§1) | Remove CharacterEngine temperature offset / humidity multipliers (move to EnvironmentModel) | `CharacterEngine` accessors removed after migration | combo boxes replaced by ENVIRONMENT group | `Character.legacyEnvironmentKeysAreReadNotWritten` | DONE |
| ENV-3 (§0.1, 2.1) | Thermal detuning from each string's strain (α_s steel/nylon, α_n neck) | `Character/EnvironmentModel.*:thermalCents`; strains from `refreshStringPhysics` | n/a | `Environment.ENV02_steadySlopeFromTheStringsOwnNumbers` | DONE |
| ENV-4 (§0.2, 2.2) | Lags: wire τ_s by diameter, neck 900 s, body 1200 s, wood moisture 1 day | `EnvironmentModel::advance/computeOutputs`, `wireTau` | n/a | `Environment.ENV03_coldCaseOvershootsThenSettles`, `Environment.ENV06_woodIsSlow` | DONE |
| ENV-5 (§0.3) | Room defaults bit-exact no-op | exact-zero arithmetic | n/a | `Environment.ENV01_theRoomIsANoOp` | DONE |
| ENV-6 (§0.4, 4) | Not scaled by character; drift loop runs with character disabled | `LuthierEngine::processSubBlock` drift loop | n/a | `Environment.ENV14_independentOfCharacter` | DONE |
| ENV-7 (§2.3-2.4) | EMC isotherm; relief/top-rise/action deltas by chambering | `emc`, `topRise`, `computeOutputs` | n/a | `Environment.ENV05_humidityMovesTheGeometryAndThePlate` | DONE |
| ENV-8 (§2.5) | Fretting-stretch cents from env geometry delta added in the per-block pitch loop | `stretchCents`, `fretCents`; `updatePerBlockModulation` | n/a | `Environment.ENV07_frettingStretchFromTheActionDelta` | DONE |
| ENV-9 (§2.6, 4) | Plate freq/Q and air multipliers into BodyEngine + coupling bank; `BodyMode::isAir`; scaled `getAirResonanceHz` | `BodyEngine::setRuntimeScaling`, `BodyModels::buildModes` | n/a | `Environment.ENV04_theAirModeFollowsTheSpeedOfSound`, `BodyCoupling.ENV13_theWolfFollowsTheEnvironment` | DONE |
| ENV-10 (§2.7) | Corrosion hook k_RH published to string aging | `EnvironmentState::corrosionRate` -> `StringAging::setCorrosionRate` | n/a | `StringAging.SA12_humidityDrivesCorrosion` | DONE |
| ENV-11 (§3.1) | Six session profiles (Static, Stage lights, Outdoor evening, Cold case, AC studio, Humid club) | `EnvironmentModel::getProfile`, `lagResponse` | ENVIRONMENT profile box | `Environment.ENV03_coldCaseOvershootsThenSettles`, `Environment.ENV06_woodIsSlow`, `Environment.ENV09_seekingIsDeterministic`, `Environment.ENV10_retuneZeroesThenDriftsAgain` | DONE |
| ENV-12 (§3.2) | Tuned-at reference; Retune zeroes offsets and writes env_tuned_at_c (one undo) | `EnvironmentModel::requestRetune` | `EnvironmentGroup::retune`; CHARACTER Retune shares it | `Environment.ENV10_retuneZeroesThenDriftsAgain`, `RealismUi.retuneWritesTunedAtAndZeroesTheOffsets` | DONE |
| ENV-13 (§3.3-3.4) | Closed-form profile, automation by superposition; host-timeline / free-running clock, seekable | `EnvironmentModel::advance` clock | ENVIRONMENT clock box | `Environment.ENV09_seekingIsDeterministic` | DONE |
| ENV-14 (§4) | Setup geometry: requested + env deltas to FretBuzzModel when a delta moves > 0.005 mm | `LuthierEngine::advanceRealism`, `setupWithGuitar` | n/a | `Environment.ENV08_aDryNeckBuzzesMore` | DONE |
| ENV-15 (§5) | Params env_temperature_c / env_tuned_at_c / env_humidity_pct / env_profile / env_clock | `Parameters.*` REALISM-A block | ENVIRONMENT group | `Environment.ENV15_budgetSafetyAndCorners`, param count | DONE |
| ENV-16 (§5) | `environment` range family; tooltip notes freezing / glass transition not modelled | `RangeFamily::environment` | CHARACTER padlock | `Environment.ENV15_budgetSafetyAndCorners` | DONE |
| ENV-17 (§6) | Reference pair in character block; legacy temperature -> °C, humidity -> 45 % (logged) | `EnvironmentModel::toVar/fromVar`; `applyRealismCharacterBlock` | n/a | `Environment.ENV11_legacyTemperatureAndHumidityMigrate` | DONE |
| ENV-18 (§7) | CHARACTER ENVIRONMENT group: controls, Retune, 10 Hz readouts (string/neck/body temp, RH, cents), 60 s low-E sparkline | n/a | `UI/RealismGroups.*:EnvironmentGroup`, `EnvironmentSparkline` | `RealismUi.theCharacterPanelCarriesTheGroups` | DONE |
| ENV-19 (§7) | Convolution-mode note "Body shift applies to modal bodies…" — label shown in convolution mode only (`EnvironmentGroup` refresh); no test | n/a | `EnvironmentGroup` `convolutionNote` | - | NO-TEST |
| ENV-20 (§7) | SETUP action/relief readouts show "+0.12 mm (humidity)" secondary line — owner defers | none | none (owner deferred) | - | MISSING |
| ENV-21 (§7) | CHARACTER padlock covers `environment` | n/a | `AdvancedPanel` CHARACTER `RangeTabButton` | `Environment.ENV15_budgetSafetyAndCorners` | DONE |
| ENV-22 (§8) | Budget 0.02 units, NaN guards, clamps (freq 0.7-1.3, Q 0.5-2, ±300 c) — perf-budget.md row not added by owner | `computeOutputs` clamps | n/a | `Environment.ENV15_budgetSafetyAndCorners` | PARTIAL |
| ENV-23 (§9 ENV-01) | Room no-op bit-identical | - | n/a | `Environment.ENV01_theRoomIsANoOp` | DONE |
| ENV-24 (§9 ENV-02) | Steady slope | - | n/a | `Environment.ENV02_steadySlopeFromTheStringsOwnNumbers` | DONE |
| ENV-25 (§9 ENV-03) | Cold-case overshoot | - | n/a | `Environment.ENV03_coldCaseOvershootsThenSettles` | DONE |
| ENV-26 (§9 ENV-04) | Air mode 1.033 | - | n/a | `Environment.ENV04_theAirModeFollowsTheSpeedOfSound` | DONE |
| ENV-27 (§9 ENV-05) | Humidity geometry | - | n/a | `Environment.ENV05_humidityMovesTheGeometryAndThePlate` | DONE |
| ENV-28 (§9 ENV-06) | Wood is slow | - | n/a | `Environment.ENV06_woodIsSlow` | DONE |
| ENV-29 (§9 ENV-07) | Fretting stretch | - | n/a | `Environment.ENV07_frettingStretchFromTheActionDelta` | DONE |
| ENV-30 (§9 ENV-08) | Dry neck buzzes more (owner bound 0.01 mm, recorded in spec) | - | n/a | `Environment.ENV08_aDryNeckBuzzesMore` | DONE |
| ENV-31 (§9 ENV-09) | Seek determinism | - | n/a | `Environment.ENV09_seekingIsDeterministic` | DONE |
| ENV-32 (§9 ENV-10) | Retune | - | n/a | `Environment.ENV10_retuneZeroesThenDriftsAgain` | DONE |
| ENV-33 (§9 ENV-11) | Legacy migration | - | n/a | `Environment.ENV11_legacyTemperatureAndHumidityMigrate` | DONE |
| ENV-34 (§9 ENV-12) | No steps (owner bound 0.1 c/block, recorded in spec) | - | n/a | `Environment.ENV12_aTemperatureStepIsASlew` | DONE |
| ENV-35 (§9 ENV-13) | Wolf follows the body | - | n/a | `BodyCoupling.ENV13_theWolfFollowsTheEnvironment` | DONE |
| ENV-36 (§9 ENV-14) | Independence from character | - | n/a | `Environment.ENV14_independentOfCharacter` | DONE |
| ENV-37 (§9 ENV-15) | Budget, zero allocation, 1000 corners | - | n/a | `Environment.ENV15_budgetSafetyAndCorners` | DONE |
