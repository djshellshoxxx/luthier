## tuning-stability.md

REALISM-C has landed on this checkout: `Model/Playing/StabilityModel.*` with all six mechanisms, Retune string/all/auto, session state, the 8 parameters, the CHARACTER TUNING STABILITY group with offset strip, Easy headstock badges and Workshop inspector figures, with 16 `TuningStability.*` tests and the §19 row. Still open: Retune as a MIDI Learn action target (TS-19) and a dedicated whammy return-error test (TS-12).

| Req | Summary | Engine location | GUI location | Test | Status |
|---|---|---|---|---|---|
| TS-1 (§1) | Existing drift/LFO/looseness unchanged; Retune button calls `CharacterEngine::retune()` | `Character/CharacterEngine.cpp:retune/getTunerDriftCents` | CHARACTER `CharacterPanel::retuneButton`, `loosenessSlider` | `Character.retuneResetsTheDrift`, `Character.tunerDriftStaysWithinItsStatedAmplitude` | DONE |
| TS-2 (§1) | New `stabilityCents` field in the StringTuning sum | `TuningEngine::setStabilityCents` | n/a | `TuningStability.offIsInert` | DONE |
| TS-3 (§1, 5) | Part fields tuners.ratio/stability/locking, nut.friction, capo.pressure consumed via `TuningHardware` | `PartAcoustics::mapSpec`, `setCapoPart`, `StabilityModel::setHardware` | n/a | `TuningStability.*`, `RealismUi.theWorkshopInspectorShowsTheDerivedFigures` | DONE |
| TS-4 (§0.4) | Deterministic; reset zeroes event offsets | `StabilityModel::reset` | n/a | `TuningStability.resetAndDeterminism` | DONE |
| TS-5 (§2) | Per-string sum clamped ±50 stock / ±200 advanced | `StabilityModel::advance` | n/a | `TuningStability.costAndSafety` | DONE |
| TS-6 (§2.1) | String settling σ, W, material factor, commit on retune | `StabilityModel` | n/a | `TuningStability.settling` | DONE |
| TS-7 (§2.2) | Nut binding stuck += 0.03μB; locking nut μ = 0; cap ±6 | `advance` | n/a | `TuningStability.nutBinding` | DONE |
| TS-8 (§2.2) | Seeded ping releases the bind over 20 ms | `onPluck` | n/a | `TuningStability.thePingReleasesTheBind` | DONE |
| TS-9 (§2.3) | Tuner backlash armed on downward approach, fires on excursion | `applyTuningChange`, `startBacklash` | n/a | `TuningStability.backlashNeedsADownwardApproach` | DONE |
| TS-10 (§2.4.1) | Floating-bridge equilibrium k_f | `applyTuningChange`, `retuneString` | n/a | `TuningStability.floatingEquilibrium` | DONE |
| TS-11 (§2.4.2) | Creep τ_c 90 s, k_c by bridge | `advance` | n/a | `TuningStability.creepTimeConstant` | DONE |
| TS-12 (§2.4.3) | Whammy return error, seeded sign, cap ±8 — no dedicated test | `advance` whammy tracking | n/a | `TuningStability.resetAndDeterminism`, `TuningStability.costAndSafety` (indirect) | PARTIAL |
| TS-13 (§2.5) | Bend memory, cap -10 | `advance` | n/a | `TuningStability.bendMemory` | DONE |
| TS-14 (§2.6) | Capo bias and capoComp on retune | `capoBiasCents`, `retuneString` | capo figure under offset strip | `TuningStability.capoBias` | DONE |
| TS-15 (§3) | Retune string n (clearDrift, CharacterEngine::retuneString) / Retune all low->high | `StabilityModel::requestRetune`, `LuthierEngine::runStability`, `CharacterEngine::retuneString` | offset strip click, Retune all button | `TuningStability.retuneScope`, `RealismUi.theRetuneAllButtonClearsEveryOffset` | DONE |
| TS-16 (§3) | Glide 250 ms ringing / snap silent; ≤ 0.5 c per block | `advance` | n/a | `TuningStability.smoothGlides` | DONE |
| TS-17 (§3) | Auto-retune Off/Idle (10 s silent)/Stop/Idle+Stop | `advance` | auto-retune dropdown | `TuningStability.autoRetuneOnIdle`, `TuningStability.autoRetuneOnTransportStop` | DONE |
| TS-18 (§3) | Atomic command mask consumed at block start; not undoable | `requestRetune(mask)` | n/a | `RealismUi.theOffsetStripRetunesTheStringItIsClickedOn` | DONE |
| TS-19 (§3) | Retune is a MIDI Learn action target (rising edge CC >= 64) — owner defers | none (hook `requestRetuneAll()` only) | none | - | MISSING |
| TS-20 (§4) | 8 params stability_amount + six scales + auto_retune; `strings` family | `Parameters.*`, `PhysicalRange.cpp` | TUNING STABILITY group | Ranges suite, `RealismUi.everyRealismCParameterHasAControlOnTheCharacterTab` | DONE |
| TS-21 (§5) | onTuningChanged from tuning preset / 12-string / detune / fine-tune; preset loads suppress events | `LuthierEngine::setTuningPreset`, `TuningPopover` | n/a | `TuningStability.backlashNeedsADownwardApproach`, `TuningStability.floatingEquilibrium` | DONE |
| TS-22 (§5) | Amount 0: advance returns, stabilityCents exactly 0 | - | n/a | `TuningStability.offIsInert` | DONE |
| TS-23 (§5) | σ commits only with transport stopped | - | n/a | `TuningStability.resetAndDeterminism` | DONE |
| TS-24 (§6) | CHARACTER TUNING STABILITY group replaces tuner-drift section (looseness, Retune all, amount, scales, auto-retune) | n/a | `TuningStabilityGroup` in `CharacterPanel` | `RealismUi.everyRealismCParameterHasAControlOnTheCharacterTab` | DONE |
| TS-25 (§6) | Offset strip ±20 c, colour by cause + dot glyph, tooltip, click retunes, 10 Hz, stale grey | n/a | `OffsetStrip` | `RealismUi.theOffsetStripRetunesTheStringItIsClickedOn` | DONE |
| TS-26 (§6) | Easy headstock popover "+3 c" + Retune | n/a | `StabilityBadge`, `TuningPopover` | `RealismUi.theHeadstockPopoverShowsOffsetsAndRetunes` | DONE |
| TS-27 (§6) | Workshop tuners/nut/capo inspectors show derived figures (capo figure on CHARACTER instead) | `describeTuningFigures` | `WorkshopPanel::refreshInspector` | `RealismUi.theWorkshopInspectorShowsTheDerivedFigures` | DONE |
| TS-28 (§6) | gui-integration §19 row | n/a | row added | n/a | DONE |
| TS-29 (§7) | Session extras "stability" {sigma, capo_comp}; preset load recomputes σ, clears capoComp; no stability block in presets | `StabilityModel::toVar/fromVar`, processor state, `PresetManager::fromVar` | n/a | `TuningStability.serialization` | DONE |
| TS-30 (§8) | Budget 0.02 units + perf-budget row; no alloc | - | n/a | `TuningStability.costAndSafety` | DONE |
| TS-31 (§9 TS-01) | Off is inert | - | n/a | `TuningStability.offIsInert` | DONE |
| TS-32 (§9 TS-02) | Nut binding | - | n/a | `TuningStability.nutBinding` | DONE |
| TS-33 (§9 TS-03) | Ping | - | n/a | `TuningStability.thePingReleasesTheBind` | DONE |
| TS-34 (§9 TS-04) | Backlash needs downward approach | - | n/a | `TuningStability.backlashNeedsADownwardApproach` | DONE |
| TS-35 (§9 TS-05) | Floating equilibrium | - | n/a | `TuningStability.floatingEquilibrium` | DONE |
| TS-36 (§9 TS-06) | Creep time constant | - | n/a | `TuningStability.creepTimeConstant` | DONE |
| TS-37 (§9 TS-07) | Bend memory | - | n/a | `TuningStability.bendMemory` | DONE |
| TS-38 (§9 TS-08) | Settling | - | n/a | `TuningStability.settling` | DONE |
| TS-39 (§9 TS-09) | Capo bias | - | n/a | `TuningStability.capoBias` | DONE |
| TS-40 (§9 TS-10) | Retune scope | - | n/a | `TuningStability.retuneScope` | DONE |
| TS-41 (§9 TS-11) | Auto-retune on idle | - | n/a | `TuningStability.autoRetuneOnIdle` | DONE |
| TS-42 (§9 TS-12) | Reset and determinism | - | n/a | `TuningStability.resetAndDeterminism` | DONE |
| TS-43 (§9 TS-13) | Smooth glides | - | n/a | `TuningStability.smoothGlides` | DONE |
| TS-44 (§9 TS-14) | Sample-rate independence | - | n/a | `TuningStability.creepTimeConstant`, `TuningStability.autoRetuneOnIdle` (44.1/96 loops) | DONE |
| TS-45 (§9 TS-15) | Serialization | - | n/a | `TuningStability.serialization` | DONE |
| TS-46 (§9 TS-16) | Cost and safety | - | n/a | `TuningStability.costAndSafety` | DONE |
