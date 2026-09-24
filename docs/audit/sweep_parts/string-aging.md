## string-aging.md

Nothing of this spec is on this checkout beyond the legacy three-step `string_age` choice (`StringMaterials.cpp:kAgeEffects`, applied to all strings) and PlayingNoise's 1.0/1.15/1.4 age roughness; there is no `StringAging`, none of the five parameters, no `strings` range family, no restring/accrual and no STRING AGING group. The realism-a branch (last commit 2026-09-24 16:02, coverage doc complete, 809-test suite green per its Results) implements every row; spot-checked `StringAging.*`, the Advanced "Age (h)" knob, `StringAgingGroup` in `CharacterPanel` and all 16 `StringAging.SA*` tests. Owner gaps: the Workshop strings-inspector mirror is deferred, no `performance-budget.md` StringAging row was added, and false beating is excluded by the spec itself.

| Req | Summary | Engine location | GUI location | Test | Status |
|---|---|---|---|---|---|
| SA-1 (§0.1, 3.1) | Age in hours of play; per-string effective hours H_i = max(0, hours-base)+accrued | (branch) `DSP/String/StringAging.*:StringAging::recompute/computeWithWeights` | (branch) CHARACTER > STRING AGING per-string rows | (branch) `StringAging.SA08_coatingIsARate`, `SA09_restringOneMakesOnlyThatStringNew` | OWNED |
| SA-2 (§0.2, 2, 3.2) | Four mechanisms fold into B, S, P, D (+ tuning) only; physical curves B/S/D/P/C | (branch) `StringAging::computeWithWeights` | n/a | (branch) `SA03_physicalCurvesMeetTheAnchors`, `SA04_everyCurveIsMonotonic` | OWNED |
| SA-3 (§0.3, 3.1) | Per-string weights wc/wk by wound/plain, seed jitter j_i via public `CharacterEngine::hashed(kCategoryStringAge,i)` | (branch) `StringAging::weightsFor`; `CharacterEngine::hashedValue`; `LuthierEngine::refreshAgingJitter` | n/a | (branch) `SA07_woundStringsDullFaster`, `SA10_theSeedIsDeterministic` | OWNED |
| SA-4 (§0.4, 3.3) | d=0 reproduces kAgeEffects Fresh/BrokenIn/Old exactly; lerp(legacy, physical, d) | (branch) `StringAging::legacy` + blend | n/a | (branch) `SA01_legacyAnchorsAreExact` | OWNED |
| SA-5 (§3.1-3.2) | Coating rate r_c (1/0.33/0.25), fresh brightness B_0; Coated material forces >= Thin | (branch) `coatingRate`, `coatingBrightness` | (branch) STRING AGING coating box | (branch) `SA08_coatingIsARate` | OWNED |
| SA-6 (§3.1) | Corrosion uses k_RH from environment (1 when absent) | (branch) `StringAging::setCorrosionRate` <- `EnvironmentState::corrosionRate` | n/a | (branch) `SA12_humidityDrivesCorrosion` | OWNED |
| SA-7 (§3.4) | Open-string detune D_i·r_i with the same RtRandom{0xA6E0000+i} draw; intonation adds d·D·0.5/12 c/fret to the slope | (branch) `StringAging::detuneSign`; `TuningEngine::setAgingIntonation` | n/a | (branch) `SA01`, `SA03` | OWNED |
| SA-8 (§3.5) | Squeak level ageRoughness and centroid 1-0.30·C; NoiseEngine squeak band-pass × centroid | (branch) `AgingFactors::roughness/squeakCentroid`; `StringNoiseInfo::fromSpec`; `PlayingNoise::makeSqueak` | n/a | (branch) `SA13_squeakReconciliation` | OWNED |
| SA-9 (§1, 5) | `string_age` inert after load; refreshStringPhysics passes Fresh and drops the ageDetune block; bridge's lastStringAge path removed | (branch) `LuthierEngine::refreshStringPhysics`, `ParameterBridge::applyToEngine` | n/a | (branch) `SA02_aLegacyPresetNullsAgainstThePreSpecPath` | OWNED |
| SA-10 (§4) | Params string_age_hours / string_corrosivity / string_age_detail / string_coating / string_age_accrual (ranges, defaults) | (branch) `Parameters.*` REALISM-A block | (branch) STRING AGING group | (branch) `SA15_theStringsRangeFamily`, `Integration` param count | OWNED |
| SA-11 (§4) | New `strings` range family appended after modulation; RangeRegistry rows | (branch) `RangeFamily::strings`, `PhysicalRange.cpp` rows | (branch) CHARACTER padlock | (branch) `SA15_theStringsRangeFamily` | OWNED |
| SA-12 (§5) | `StringEngine::setAgingFactors` separate from sustainScale, into updateLoopCoefficients/updateDispersion | (branch) `StringEngine::setAgingFactors` | n/a | (branch) `SA05_oldStringsLoseAThirdOfTheirSustain`, `SA06_oldStringsAreDuller` | OWNED |
| SA-13 (§5) | Block-rate advance next to character.advance; recompute/push on input change | (branch) `LuthierEngine::advanceRealism`, `pushAgingFactors` | n/a | (branch) `SA11_accrualIsPlayedTime`, `SA14_automatingHoursDoesNotClick` | OWNED |
| SA-14 (§6) | Restring one string (command) | (branch) `StringAging::requestRestring` | (branch) STRING AGING per-row Restring, `StringAgingGroup::restring` | (branch) `SA09_restringOneMakesOnlyThatStringNew` | OWNED |
| SA-15 (§6) | Restring all: hours=0 + clear state, one undo entry | (branch) `requestRestringAll` | (branch) `StringAgingGroup::restringAll` | (branch) `RealismUi.restringAllZeroesTheHoursAndTheState` | OWNED |
| SA-16 (§6) | Accrual Off/Real/x10/x100 on played time (level > 1e-3), never writes the param, survives reset | (branch) `StringAging::advance` | (branch) STRING AGING "Age while playing" | (branch) `SA11_accrualIsPlayedTime` | OWNED |
| SA-17 (§7) | Advanced Col 1 STRINGS age slider bound to hours with Fresh/Broken in/Old ticks | (branch) n/a | (branch) `AdvancedPanel` `stringAgeHours` knob "Age (h)" | (branch) build/manual only | OWNED |
| SA-18 (§7) | CHARACTER STRING AGING group after STRING NOISE; per-string H_i, B bar, Restring; 4 Hz readouts, stale grey | (branch) n/a | (branch) `UI/RealismGroups.*:StringAgingGroup` in `CharacterPanel` | (branch) `RealismUi.theCharacterPanelCarriesTheGroups` | OWNED |
| SA-19 (§7) | Workshop strings inspector mirrors per-string rows; coated winding sets coating >= Thin — owner defers the inspector mirror (model side done) | (branch) material Coated forces Thin | none (owner deferred) | - | MISSING |
| SA-20 (§7) | CHARACTER padlock covers `strings` | (branch) n/a | (branch) `AdvancedPanel` CHARACTER `RangeTabButton` families | (branch) `SA15` | OWNED |
| SA-21 (§8) | Per-string state in preset `character.aging` block; legacy load maps string_age -> hours, detail 0, coating None | (branch) `StringAging::toVar/fromVar`; `PresetManager::fromVar`; `applyRealismCharacterBlock` | n/a | (branch) `SA02`, `BodyCoupling.BC12_legacyLoadIsOff` | OWNED |
| SA-22 (§9) | Budget 0.02 units, no alloc, relaxed atomics, 200 ms hours one-pole — perf-budget.md StringAging row not added by owner | (branch) `StringAging::advance` | n/a | (branch) `SA14`, `SA16_budgetAndSafety` | OWNED |
| SA-23 (§10 SA-01) | Legacy anchors within 1e-9 | (branch) | n/a | (branch) `SA01_legacyAnchorsAreExact` | OWNED |
| SA-24 (§10 SA-02) | Legacy null vs pre-spec path | (branch) | n/a | (branch) `SA02_aLegacyPresetNullsAgainstThePreSpecPath` | OWNED |
| SA-25 (§10 SA-03) | Curves meet anchors within 1 % | (branch) | n/a | (branch) `SA03_physicalCurvesMeetTheAnchors` | OWNED |
| SA-26 (§10 SA-04) | Monotonic 0-2000 h | (branch) | n/a | (branch) `SA04_everyCurveIsMonotonic` | OWNED |
| SA-27 (§10 SA-05) | Low E T60 28-40 % shorter at 120 h | (branch) | n/a | (branch) `SA05_oldStringsLoseAThirdOfTheirSustain` | OWNED |
| SA-28 (§10 SA-06) | High-E centroid >= 15 % lower | (branch) | n/a | (branch) `SA06_oldStringsAreDuller` | OWNED |
| SA-29 (§10 SA-07) | Wound ages faster | (branch) | n/a | (branch) `SA07_woundStringsDullFaster` | OWNED |
| SA-30 (§10 SA-08) | Coating is a rate | (branch) | n/a | (branch) `SA08_coatingIsARate` | OWNED |
| SA-31 (§10 SA-09) | Restring one | (branch) | n/a | (branch) `SA09_restringOneMakesOnlyThatStringNew` | OWNED |
| SA-32 (§10 SA-10) | Determinism by seed | (branch) | n/a | (branch) `SA10_theSeedIsDeterministic` | OWNED |
| SA-33 (§10 SA-11) | Accrual | (branch) | n/a | (branch) `SA11_accrualIsPlayedTime` | OWNED |
| SA-34 (§10 SA-12) | Humidity hook | (branch) | n/a | (branch) `SA12_humidityDrivesCorrosion` | OWNED |
| SA-35 (§10 SA-13) | Squeak reconciliation | (branch) | n/a | (branch) `SA13_squeakReconciliation` | OWNED |
| SA-36 (§10 SA-14) | No clicks on 0->200 h | (branch) | n/a | (branch) `SA14_automatingHoursDoesNotClick` | OWNED |
| SA-37 (§10 SA-15) | Ranges invariants, legacy stock | (branch) | n/a | (branch) `SA15_theStringsRangeFamily` | OWNED |
| SA-38 (§10 SA-16) | Budget and zero allocations | (branch) | n/a | (branch) `SA16_budgetAndSafety` | OWNED |
