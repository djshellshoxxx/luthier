## body-coupling.md

REALISM-A has landed on this checkout: `DSP/Coupling/BodyCouplingBank.*`, the wave-impedance bridge return, tap injection, the five parameters, the `body` family, the Advanced Coupling knob and the BODY COUPLING group with mode list, wolf map and Tap, with `BodyCoupling.BC01..BC13` registered (amended numbers recorded in the spec). Still open: the Workshop body-inspector mirror (BC-17), the factory re-voicing pass (BC-20; presets load at 0) and the `spec/performance-budget.md` row (BC-22).

| Req | Summary | Engine location | GUI location | Test | Status |
|---|---|---|---|---|---|
| BC-1 (§0.1, 2.1) | Emergent wolfs from a bridge admittance bank built from BodyModels modes (no wolf table) | `DSP/Coupling/BodyCouplingBank.*:design/viewMode` | n/a | `BodyCoupling.BC02_aWolfOnAnAcoustic` | DONE |
| BC-2 (§0.2, 2.3) | Passive loaded diagonal form Q'_k, Y'_k; 0.35 cap reported to validator | `BodyCouplingBank::redesignResonators/processSample` | n/a | `BodyCoupling.BC01_theBankIsPassive`, `BodyCoupling.BC10_stabilityAcrossTheAdvancedCorners` | DONE |
| BC-3 (§0.3, 3) | One body, two views: bank and BodyEngine get the same scaling | `getBodyCouplingScaling`, `BodyEngine::setRuntimeScaling` | n/a | `BodyCoupling.BC05_theWolfMovesWithTheBody` | DONE |
| BC-4 (§0.4) | CouplingMatrix unchanged (saddle path kept) | `DSP/Coupling/CouplingMatrix.cpp` (untouched) | n/a | `BodyCoupling.BC04_offIsBitIdentical` | DONE |
| BC-5 (§2.2) | Mode masses by chambering + bridge/tailpiece mass; K strongest < 1.2 kHz | `baseMass`, `design`; `Chambering.h` | n/a | `BodyCoupling.BC02_aWolfOnAnAcoustic`, `BodyCoupling.BC03_aSolidbodyIsMild` | DONE |
| BC-6 (§2.4) | Tap tones: 1 ms half-sine force into the bank from body knock / slap body tap | `BodyCouplingBank::driveDirect`; `LuthierEngine::applySlapAction` | BODY COUPLING Tap button | `BodyCoupling.BC06_BC09_aTapRingsTheStringsNearAMode`, `SlapWiring.aBodyTapLeavesTheStringsAlone` | DONE |
| BC-7 (§3) | `Physical::waveImpedance`, `StringEngine::getBridgeWave` (owner amended: loop return) | `StringMaterials::toPhysical`, `StringEngine` | n/a | `BodyCoupling.BC01_theBankIsPassive` | DONE |
| BC-8 (§3) | Per-sample insertion after the matrix | `LuthierEngine::processSubBlock` (StringEngine::beginSample/endSample) | n/a | `BodyCoupling.BC04_offIsBitIdentical`, `BodyCoupling.BC07_sympatheticRingThroughTheBody` | DONE |
| BC-9 (§3) | Re-design on body rebuild / string refresh / part swap; message-thread design, try-lock stage | `rebuildBodyCoupling` | n/a | `BodyCoupling.BC11_theWolfMapIsHonest`, `BodyCoupling.BC13_budgetAndSafety` | DONE |
| BC-10 (§3) | Scaling: env × freq scale × Q scale to both; mass scale to bank only | `getBodyCouplingScaling` | n/a | `BodyCoupling.BC05_theWolfMovesWithTheBody`, `BodyCoupling.ENV13_theWolfFollowsTheEnvironment` | DONE |
| BC-11 (§4) | Params body_coupling_amount / body_mode_mass_scale / _q_scale / _freq_scale / body_coupling_modes | `Parameters.*` | BODY COUPLING group | `BodyCoupling.BC12_legacyLoadIsOff` | DONE |
| BC-12 (§4) | `body` range family | `RangeFamily::body` | CHARACTER padlock | `BodyCoupling.BC12_legacyLoadIsOff` | DONE |
| BC-13 (§5) | Advanced Col 1 BODY Coupling knob next to air readout | n/a | `AdvancedPanel` `bodyCoupling` knob "Coupling" | `GuiReach.everyAutomatableParameterHasAVisibleControl` | DONE |
| BC-14 (§5) | CHARACTER BODY COUPLING group after SETUP: scales, modes, mode list | n/a | `UI/RealismGroups.*:BodyCouplingGroup`, `BodyModeList` | `RealismUi.theCharacterPanelCarriesTheGroups` | DONE |
| BC-15 (§5) | Wolf map strings × frets 0-19, warning colour > 30 % + dot glyph | `BodyCouplingBank::wolfMap/predictedLoss` | `WolfMap` | `BodyCoupling.BC11_theWolfMapIsHonest` | DONE |
| BC-16 (§5) | Tap button fires one body tap | `requestBodyTap` | BODY COUPLING Tap | `BodyCoupling.BC06_BC09_aTapRingsTheStringsNearAMode` | DONE |
| BC-17 (§5) | Workshop body inspector mirrors mode list and wolf map — owner defers | none | none (owner deferred) | - | MISSING |
| BC-18 (§5) | CHARACTER padlock covers `body` | n/a | `RangeTabButton` families | `BodyCoupling.BC12_legacyLoadIsOff` | DONE |
| BC-19 (§6) | Bank derived not stored; legacy load writes 0 | `PresetManager::fromVar` REALISM-A block | n/a | `BodyCoupling.BC12_legacyLoadIsOff` | DONE |
| BC-20 (§6) | Factory presets re-voiced with coupling on (listening pass) — owner defers; factory files omit the key so load at 0 | none | n/a | - | MISSING |
| BC-21 (§7) | Smoothing 20 ms amount, 0.2 %/block freq slew, 0.05 % redesign threshold | `processSample`, `beginBlock` | n/a | `BodyCoupling.BC04_offIsBitIdentical` | DONE |
| BC-22 (§7) | Budget (spec 0.08, owner amended 0.1 units), no audio alloc — perf-budget.md row not added | `design` message-thread only | n/a | `BodyCoupling.BC13_budgetAndSafety` | PARTIAL |
| BC-23 (§8 BC-01) | Passivity | - | n/a | `BodyCoupling.BC01_theBankIsPassive` | DONE |
| BC-24 (§8 BC-02) | Wolf on an acoustic | - | n/a | `BodyCoupling.BC02_aWolfOnAnAcoustic` | DONE |
| BC-25 (§8 BC-03) | Solidbody is mild | - | n/a | `BodyCoupling.BC03_aSolidbodyIsMild` | DONE |
| BC-26 (§8 BC-04) | Off is identical | - | n/a | `BodyCoupling.BC04_offIsBitIdentical` | DONE |
| BC-27 (§8 BC-05) | Wolf moves with the body | - | n/a | `BodyCoupling.BC05_theWolfMovesWithTheBody` | DONE |
| BC-28 (§8 BC-06) | Tap tone | - | n/a | `BodyCoupling.BC06_BC09_aTapRingsTheStringsNearAMode` | DONE |
| BC-29 (§8 BC-07) | Sympathetic ring through the body | - | n/a | `BodyCoupling.BC07_sympatheticRingThroughTheBody` | DONE |
| BC-30 (§8 BC-08) | Heavy bridge couples less | - | n/a | `BodyCoupling.BC08_aHeavyBridgeCouplesLess` | DONE |
| BC-31 (§8 BC-09) | Muted strings do not answer | - | n/a | `BodyCoupling.BC06_BC09_aTapRingsTheStringsNearAMode` | DONE |
| BC-32 (§8 BC-10) | Stability sweep (owner: 200 corners × 1 s) | - | n/a | `BodyCoupling.BC10_stabilityAcrossTheAdvancedCorners` | DONE |
| BC-33 (§8 BC-11) | Wolf map is honest | - | n/a | `BodyCoupling.BC11_theWolfMapIsHonest` | DONE |
| BC-34 (§8 BC-12) | Legacy load | - | n/a | `BodyCoupling.BC12_legacyLoadIsOff` | DONE |
| BC-35 (§8 BC-13) | Budget and safety | - | n/a | `BodyCoupling.BC13_budgetAndSafety` | DONE |
