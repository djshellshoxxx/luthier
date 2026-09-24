## tuning-stability.md

On this checkout only the existing drift sources exist (`tuning_drift` random walk, `CharacterEngine` tuner-drift LFO driven by the CHARACTER looseness slider, and the Retune button calling `CharacterEngine::retune()`); there is no `stabilityCents`, `StabilityModel`, event-driven mechanism, retune command channel, parameter, offset strip or headstock badge, and the part fields tuners/nut/capo are unconsumed. The realism-c branch (active today, coverage complete) implements all six mechanisms, Retune string/all/auto, session state, 8 parameters, the TUNING STABILITY group with offset strip, Easy headstock badges, Workshop inspector figures and `TuningStability.*` (16 tests), plus §19 and performance-budget rows. Owner gap: Retune as a MIDI Learn action target is deferred; the capo-bias figure sits under the offset strip because the Workshop inspector has no capo slot.

| Req | Summary | Engine location | GUI location | Test | Status |
|---|---|---|---|---|---|
| TS-1 (§1) | Existing drift/LFO/looseness unchanged; Retune button calls `CharacterEngine::retune()` | `Character/CharacterEngine.cpp:retune/getTunerDriftCents` | CHARACTER `CharacterPanel::retuneButton`, `loosenessSlider` | `Character.retuneResetsTheDrift`, `Character.tunerDriftStaysWithinItsStatedAmplitude` | DONE |
| TS-2 (§1) | New `stabilityCents` field in the StringTuning sum | (branch) `TuningEngine::setStabilityCents` | n/a | (branch) `TuningStability.offIsInert` | OWNED |
| TS-3 (§1, 5) | Part fields tuners.ratio/stability/locking, nut.friction, capo.pressure consumed via `TuningHardware` | (branch) `PartAcoustics::mapSpec`, `setCapoPart`, `StabilityModel::setHardware` | n/a | (branch) `TuningStability.*`, `RealismUi.theWorkshopInspectorShowsTheDerivedFigures` | OWNED |
| TS-4 (§0.4) | Deterministic; reset zeroes event offsets | (branch) `StabilityModel::reset` | n/a | (branch) `resetAndDeterminism` | OWNED |
| TS-5 (§2) | Per-string sum clamped ±50 stock / ±200 advanced | (branch) `StabilityModel::advance` | n/a | (branch) `costAndSafety` | OWNED |
| TS-6 (§2.1) | String settling σ, W, material factor, commit on retune | (branch) `StabilityModel` | n/a | (branch) `settling` | OWNED |
| TS-7 (§2.2) | Nut binding stuck += 0.03μB; locking nut μ = 0; cap ±6 | (branch) `advance` | n/a | (branch) `nutBinding` | OWNED |
| TS-8 (§2.2) | Seeded ping releases the bind over 20 ms | (branch) `onPluck` | n/a | (branch) `thePingReleasesTheBind` | OWNED |
| TS-9 (§2.3) | Tuner backlash armed on downward approach, fires on excursion | (branch) `applyTuningChange`, `startBacklash` | n/a | (branch) `backlashNeedsADownwardApproach` | OWNED |
| TS-10 (§2.4.1) | Floating-bridge equilibrium k_f | (branch) `applyTuningChange`, `retuneString` | n/a | (branch) `floatingEquilibrium` | OWNED |
| TS-11 (§2.4.2) | Creep τ_c 90 s, k_c by bridge | (branch) `advance` | n/a | (branch) `creepTimeConstant` | OWNED |
| TS-12 (§2.4.3) | Whammy return error, seeded sign, cap ±8 — no dedicated test | (branch) `advance` whammy tracking | n/a | (branch) `resetAndDeterminism`, `costAndSafety` (indirect) | OWNED |
| TS-13 (§2.5) | Bend memory, cap -10 | (branch) `advance` | n/a | (branch) `bendMemory` | OWNED |
| TS-14 (§2.6) | Capo bias and capoComp on retune | (branch) `capoBiasCents`, `retuneString` | (branch) capo figure under offset strip | (branch) `capoBias` | OWNED |
| TS-15 (§3) | Retune string n (clearDrift, CharacterEngine::retuneString) / Retune all low->high | (branch) `StabilityModel::requestRetune`, `LuthierEngine::runStability`, `CharacterEngine::retuneString` | (branch) offset strip click, Retune all button | (branch) `retuneScope`, `RealismUi.theRetuneAllButtonClearsEveryOffset` | OWNED |
| TS-16 (§3) | Glide 250 ms ringing / snap silent; ≤ 0.5 c per block | (branch) `advance` | n/a | (branch) `smoothGlides` | OWNED |
| TS-17 (§3) | Auto-retune Off/Idle (10 s silent)/Stop/Idle+Stop | (branch) `advance` | (branch) auto-retune dropdown | (branch) `autoRetuneOnIdle`, `autoRetuneOnTransportStop` | OWNED |
| TS-18 (§3) | Atomic command mask consumed at block start; not undoable | (branch) `requestRetune(mask)` | n/a | (branch) `RealismUi.theOffsetStripRetunesTheStringItIsClickedOn` | OWNED |
| TS-19 (§3) | Retune is a MIDI Learn action target (rising edge CC >= 64) — owner defers | none (hook `requestRetuneAll()` only) | none | - | MISSING |
| TS-20 (§4) | 8 params stability_amount + six scales + auto_retune; `strings` family | (branch) `Parameters.*`, `PhysicalRange.cpp` | (branch) TUNING STABILITY group | (branch) Ranges suite, `RealismUi.everyRealismCParameterHasAControlOnTheCharacterTab` | OWNED |
| TS-21 (§5) | onTuningChanged from tuning preset / 12-string / detune / fine-tune; preset loads suppress events | (branch) `LuthierEngine::setTuningPreset`, `TuningPopover` | n/a | (branch) `backlashNeedsADownwardApproach`, `floatingEquilibrium` | OWNED |
| TS-22 (§5) | Amount 0: advance returns, stabilityCents exactly 0 | (branch) | n/a | (branch) `offIsInert` | OWNED |
| TS-23 (§5) | σ commits only with transport stopped | (branch) | n/a | (branch) `resetAndDeterminism` | OWNED |
| TS-24 (§6) | CHARACTER TUNING STABILITY group replaces tuner-drift section (looseness, Retune all, amount, scales, auto-retune) | (branch) n/a | (branch) `TuningStabilityGroup` in `CharacterPanel` | (branch) `RealismUi.everyRealismCParameterHasAControlOnTheCharacterTab` | OWNED |
| TS-25 (§6) | Offset strip ±20 c, colour by cause + dot glyph, tooltip, click retunes, 10 Hz, stale grey | (branch) n/a | (branch) `OffsetStrip` | (branch) `RealismUi.theOffsetStripRetunesTheStringItIsClickedOn` | OWNED |
| TS-26 (§6) | Easy headstock popover "+3 c" + Retune | (branch) n/a | (branch) `StabilityBadge`, `TuningPopover` | (branch) `RealismUi.theHeadstockPopoverShowsOffsetsAndRetunes` | OWNED |
| TS-27 (§6) | Workshop tuners/nut/capo inspectors show derived figures (capo figure on CHARACTER instead) | (branch) `describeTuningFigures` | (branch) `WorkshopPanel::refreshInspector` | (branch) `RealismUi.theWorkshopInspectorShowsTheDerivedFigures` | OWNED |
| TS-28 (§6) | gui-integration §19 row | n/a | (branch) row added | n/a | OWNED |
| TS-29 (§7) | Session extras "stability" {sigma, capo_comp}; preset load recomputes σ, clears capoComp; no stability block in presets | (branch) `StabilityModel::toVar/fromVar`, processor state, `PresetManager::fromVar` | n/a | (branch) `serialization` | OWNED |
| TS-30 (§8) | Budget 0.02 units + perf-budget row; no alloc | (branch) | n/a | (branch) `costAndSafety` | OWNED |
| TS-31 (§9 TS-01) | Off is inert | (branch) | n/a | (branch) `offIsInert` | OWNED |
| TS-32 (§9 TS-02) | Nut binding | (branch) | n/a | (branch) `nutBinding` | OWNED |
| TS-33 (§9 TS-03) | Ping | (branch) | n/a | (branch) `thePingReleasesTheBind` | OWNED |
| TS-34 (§9 TS-04) | Backlash needs downward approach | (branch) | n/a | (branch) `backlashNeedsADownwardApproach` | OWNED |
| TS-35 (§9 TS-05) | Floating equilibrium | (branch) | n/a | (branch) `floatingEquilibrium` | OWNED |
| TS-36 (§9 TS-06) | Creep time constant | (branch) | n/a | (branch) `creepTimeConstant` | OWNED |
| TS-37 (§9 TS-07) | Bend memory | (branch) | n/a | (branch) `bendMemory` | OWNED |
| TS-38 (§9 TS-08) | Settling | (branch) | n/a | (branch) `settling` | OWNED |
| TS-39 (§9 TS-09) | Capo bias | (branch) | n/a | (branch) `capoBias` | OWNED |
| TS-40 (§9 TS-10) | Retune scope | (branch) | n/a | (branch) `retuneScope` | OWNED |
| TS-41 (§9 TS-11) | Auto-retune on idle | (branch) | n/a | (branch) `autoRetuneOnIdle` | OWNED |
| TS-42 (§9 TS-12) | Reset and determinism | (branch) | n/a | (branch) `resetAndDeterminism` | OWNED |
| TS-43 (§9 TS-13) | Smooth glides | (branch) | n/a | (branch) `smoothGlides` | OWNED |
| TS-44 (§9 TS-14) | Sample-rate independence | (branch) | n/a | (branch) `creepTimeConstant`, `autoRetuneOnIdle` (44.1/96 loops) | OWNED |
| TS-45 (§9 TS-15) | Serialization | (branch) | n/a | (branch) `serialization` | OWNED |
| TS-46 (§9 TS-16) | Cost and safety | (branch) | n/a | (branch) `costAndSafety` | OWNED |
