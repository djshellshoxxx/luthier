## string-aging.md

REALISM-A has landed on this checkout: `DSP/String/StringAging.*`, the five parameters, the `strings` range family, restring/accrual, the Advanced "Age (h)" knob and the CHARACTER STRING AGING group, with all 16 `StringAging.SA*` tests registered. Still open: the Workshop strings-inspector mirror (SA-19), tick marks on the Advanced age knob (SA-17) and the `spec/performance-budget.md` StringAging row (SA-22); false beating is excluded by the spec itself.

| Req | Summary | Engine location | GUI location | Test | Status |
|---|---|---|---|---|---|
| SA-1 (§0.1, 3.1) | Age in hours of play; per-string effective hours H_i = max(0, hours-base)+accrued | `DSP/String/StringAging.*:StringAging::recompute/computeWithWeights` | CHARACTER > STRING AGING per-string rows | `StringAging.SA08_coatingIsARate`, `StringAging.SA09_restringOneMakesOnlyThatStringNew` | DONE |
| SA-2 (§0.2, 2, 3.2) | Four mechanisms fold into B, S, P, D (+ tuning) only; physical curves B/S/D/P/C | `StringAging::computeWithWeights` | n/a | `StringAging.SA03_physicalCurvesMeetTheAnchors`, `StringAging.SA04_everyCurveIsMonotonic` | DONE |
| SA-3 (§0.3, 3.1) | Per-string weights wc/wk by wound/plain, seed jitter j_i via public `CharacterEngine::hashed(kCategoryStringAge,i)` | `StringAging::weightsFor`; `CharacterEngine::hashedValue`; `LuthierEngine::refreshAgingJitter` | n/a | `StringAging.SA07_woundStringsDullFaster`, `StringAging.SA10_theSeedIsDeterministic` | DONE |
| SA-4 (§0.4, 3.3) | d=0 reproduces kAgeEffects Fresh/BrokenIn/Old exactly; lerp(legacy, physical, d) | `StringAging::legacy` + blend | n/a | `StringAging.SA01_legacyAnchorsAreExact` | DONE |
| SA-5 (§3.1-3.2) | Coating rate r_c (1/0.33/0.25), fresh brightness B_0; Coated material forces >= Thin | `coatingRate`, `coatingBrightness` | STRING AGING coating box | `StringAging.SA08_coatingIsARate` | DONE |
| SA-6 (§3.1) | Corrosion uses k_RH from environment (1 when absent) | `StringAging::setCorrosionRate` <- `EnvironmentState::corrosionRate` | n/a | `StringAging.SA12_humidityDrivesCorrosion` | DONE |
| SA-7 (§3.4) | Open-string detune D_i·r_i with the same RtRandom{0xA6E0000+i} draw; intonation adds d·D·0.5/12 c/fret to the slope | `StringAging::detuneSign`; `TuningEngine::setAgingIntonation` | n/a | `StringAging.SA01_legacyAnchorsAreExact`, `StringAging.SA03_physicalCurvesMeetTheAnchors` | DONE |
| SA-8 (§3.5) | Squeak level ageRoughness and centroid 1-0.30·C; NoiseEngine squeak band-pass × centroid | `AgingFactors::roughness/squeakCentroid`; `StringNoiseInfo::fromSpec`; `PlayingNoise::makeSqueak` | n/a | `StringAging.SA13_squeakReconciliation` | DONE |
| SA-9 (§1, 5) | `string_age` inert after load; refreshStringPhysics passes Fresh and drops the ageDetune block; bridge's lastStringAge path removed | `LuthierEngine::refreshStringPhysics`, `ParameterBridge::applyToEngine` | n/a | `StringAging.SA02_aLegacyPresetNullsAgainstThePreSpecPath` | DONE |
| SA-10 (§4) | Params string_age_hours / string_corrosivity / string_age_detail / string_coating / string_age_accrual (ranges, defaults) | `Parameters.*` REALISM-A block | STRING AGING group | `StringAging.SA15_theStringsRangeFamily`, `Parameters.everyParameterHasAUniqueIdAndSaneDefault`, `GuiReach.everyAutomatableParameterHasAVisibleControl` | DONE |
| SA-11 (§4) | New `strings` range family appended after modulation; RangeRegistry rows | `RangeFamily::strings`, `PhysicalRange.cpp` rows | CHARACTER padlock | `StringAging.SA15_theStringsRangeFamily` | DONE |
| SA-12 (§5) | `StringEngine::setAgingFactors` separate from sustainScale, into updateLoopCoefficients/updateDispersion | `StringEngine::setAgingFactors` | n/a | `StringAging.SA05_oldStringsLoseAThirdOfTheirSustain`, `StringAging.SA06_oldStringsAreDuller` | DONE |
| SA-13 (§5) | Block-rate advance next to character.advance; recompute/push on input change | `LuthierEngine::advanceRealism`, `pushAgingFactors` | n/a | `StringAging.SA11_accrualIsPlayedTime`, `StringAging.SA14_automatingHoursDoesNotClick` | DONE |
| SA-14 (§6) | Restring one string (command) | `StringAging::requestRestring` | STRING AGING per-row Restring, `StringAgingGroup::restring` | `StringAging.SA09_restringOneMakesOnlyThatStringNew` | DONE |
| SA-15 (§6) | Restring all: hours=0 + clear state, one undo entry | `requestRestringAll` | `StringAgingGroup::restringAll` | `RealismUi.restringAllZeroesTheHoursAndTheState` | DONE |
| SA-16 (§6) | Accrual Off/Real/x10/x100 on played time (level > 1e-3), never writes the param, survives reset | `StringAging::advance` | STRING AGING "Age while playing" | `StringAging.SA11_accrualIsPlayedTime` | DONE |
| SA-17 (§7) | Advanced Col 1 STRINGS age slider bound to hours with Fresh/Broken in/Old ticks - knob attached (`stringAgeHours`, "Age (h)"); Fresh 0 / Broken in 12 / Old 120 anchors only in the tooltip, no tick marks | n/a | `AdvancedPanel` `stringAgeHours` knob "Age (h)" | `GuiReach.everyAutomatableParameterHasAVisibleControl` | PARTIAL |
| SA-18 (§7) | CHARACTER STRING AGING group after STRING NOISE; per-string H_i, B bar, Restring; 4 Hz readouts, stale grey | n/a | `UI/RealismGroups.*:StringAgingGroup` in `CharacterPanel` | `RealismUi.theCharacterPanelCarriesTheGroups` | DONE |
| SA-19 (§7) | Workshop strings inspector mirrors per-string rows; coated winding sets coating >= Thin - model side done; no aging or coating reference in `UI/WorkshopPanel.cpp` | (branch) material Coated forces Thin | none (owner deferred) | - | MISSING |
| SA-20 (§7) | CHARACTER padlock covers `strings` | n/a | `AdvancedPanel` CHARACTER `RangeTabButton` families | `StringAging.SA15_theStringsRangeFamily` | DONE |
| SA-21 (§8) | Per-string state in preset `character.aging` block; legacy load maps string_age -> hours, detail 0, coating None | `StringAging::toVar/fromVar`; `PresetManager::fromVar`; `applyRealismCharacterBlock` | n/a | `StringAging.SA02_aLegacyPresetNullsAgainstThePreSpecPath`, `BodyCoupling.BC12_legacyLoadIsOff` | DONE |
| SA-22 (§9) | Budget 0.02 units, no alloc, relaxed atomics, 200 ms hours one-pole - `StringAging.SA14_automatingHoursDoesNotClick`, `SA16_budgetAndSafety` exist; spec/performance-budget.md has no StringAging row | `StringAging::advance` | n/a | `StringAging.SA14_automatingHoursDoesNotClick`, `StringAging.SA16_budgetAndSafety` | PARTIAL |
| SA-23 (§10 SA-01) | Legacy anchors within 1e-9 | - | n/a | `StringAging.SA01_legacyAnchorsAreExact` | DONE |
| SA-24 (§10 SA-02) | Legacy null vs pre-spec path | - | n/a | `StringAging.SA02_aLegacyPresetNullsAgainstThePreSpecPath` | DONE |
| SA-25 (§10 SA-03) | Curves meet anchors within 1 % | - | n/a | `StringAging.SA03_physicalCurvesMeetTheAnchors` | DONE |
| SA-26 (§10 SA-04) | Monotonic 0-2000 h | - | n/a | `StringAging.SA04_everyCurveIsMonotonic` | DONE |
| SA-27 (§10 SA-05) | Low E T60 28-40 % shorter at 120 h | - | n/a | `StringAging.SA05_oldStringsLoseAThirdOfTheirSustain` | DONE |
| SA-28 (§10 SA-06) | High-E centroid >= 15 % lower | - | n/a | `StringAging.SA06_oldStringsAreDuller` | DONE |
| SA-29 (§10 SA-07) | Wound ages faster | - | n/a | `StringAging.SA07_woundStringsDullFaster` | DONE |
| SA-30 (§10 SA-08) | Coating is a rate | - | n/a | `StringAging.SA08_coatingIsARate` | DONE |
| SA-31 (§10 SA-09) | Restring one | - | n/a | `StringAging.SA09_restringOneMakesOnlyThatStringNew` | DONE |
| SA-32 (§10 SA-10) | Determinism by seed | - | n/a | `StringAging.SA10_theSeedIsDeterministic` | DONE |
| SA-33 (§10 SA-11) | Accrual | - | n/a | `StringAging.SA11_accrualIsPlayedTime` | DONE |
| SA-34 (§10 SA-12) | Humidity hook | - | n/a | `StringAging.SA12_humidityDrivesCorrosion` | DONE |
| SA-35 (§10 SA-13) | Squeak reconciliation | - | n/a | `StringAging.SA13_squeakReconciliation` | DONE |
| SA-36 (§10 SA-14) | No clicks on 0->200 h | - | n/a | `StringAging.SA14_automatingHoursDoesNotClick` | DONE |
| SA-37 (§10 SA-15) | Ranges invariants, legacy stock | - | n/a | `StringAging.SA15_theStringsRangeFamily` | DONE |
| SA-38 (§10 SA-16) | Budget and zero allocations | - | n/a | `StringAging.SA16_budgetAndSafety` | DONE |
