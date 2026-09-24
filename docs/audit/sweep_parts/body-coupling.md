## body-coupling.md

On this checkout the body is feed-forward only; `CouplingMatrix` is the only string back-coupling, and there is no `BodyCouplingBank`, `waveImpedance`/`getBridgeWave`, tap injection, parameters, `body` family, Coupling knob, BODY COUPLING group or wolf map. The realism-a branch (active today, coverage complete) implements the bank, the five parameters, the Advanced Coupling knob (`AdvancedPanel` `bodyCoupling`), the BODY COUPLING group with mode list, wolf map and Tap, and `BodyCoupling.BC01..BC13`, with amended numbers recorded in the spec (budget 0.1 units, bridge wave = loop return). Owner gaps: the Workshop body-inspector mirror and the factory re-voicing pass are deferred (factory presets load coupling 0, so the feature is off out of the box), and no `performance-budget.md` BodyCouplingBank row was added.

| Req | Summary | Engine location | GUI location | Test | Status |
|---|---|---|---|---|---|
| BC-1 (§0.1, 2.1) | Emergent wolfs from a bridge admittance bank built from BodyModels modes (no wolf table) | (branch) `DSP/Coupling/BodyCouplingBank.*:design/viewMode` | n/a | (branch) `BodyCoupling.BC02_aWolfOnAnAcoustic` | OWNED |
| BC-2 (§0.2, 2.3) | Passive loaded diagonal form Q'_k, Y'_k; 0.35 cap reported to validator | (branch) `BodyCouplingBank::redesignResonators/processSample` | n/a | (branch) `BC01_theBankIsPassive`, `BC10_stabilityAcrossTheAdvancedCorners` | OWNED |
| BC-3 (§0.3, 3) | One body, two views: bank and BodyEngine get the same scaling | (branch) `getBodyCouplingScaling`, `BodyEngine::setRuntimeScaling` | n/a | (branch) `BC05_theWolfMovesWithTheBody` | OWNED |
| BC-4 (§0.4) | CouplingMatrix unchanged (saddle path kept) | `DSP/Coupling/CouplingMatrix.cpp` (untouched) | n/a | (branch) `BC04_offIsBitIdentical` | OWNED |
| BC-5 (§2.2) | Mode masses by chambering + bridge/tailpiece mass; K strongest < 1.2 kHz | (branch) `baseMass`, `design`; `Chambering.h` | n/a | (branch) `BC02`, `BC03_aSolidbodyIsMild` | OWNED |
| BC-6 (§2.4) | Tap tones: 1 ms half-sine force into the bank from body knock / slap body tap | (branch) `BodyCouplingBank::driveDirect`; `LuthierEngine::applySlapAction` | (branch) BODY COUPLING Tap button | (branch) `BC06_BC09_aTapRingsTheStringsNearAMode`, `SlapWiring.aBodyTapLeavesTheStringsAlone` | OWNED |
| BC-7 (§3) | `Physical::waveImpedance`, `StringEngine::getBridgeWave` (owner amended: loop return) | (branch) `StringMaterials::toPhysical`, `StringEngine` | n/a | (branch) `BC01` | OWNED |
| BC-8 (§3) | Per-sample insertion after the matrix | (branch) `LuthierEngine::processSubBlock` (StringEngine::beginSample/endSample) | n/a | (branch) `BC04`, `BC07_sympatheticRingThroughTheBody` | OWNED |
| BC-9 (§3) | Re-design on body rebuild / string refresh / part swap; message-thread design, try-lock stage | (branch) `rebuildBodyCoupling` | n/a | (branch) `BC11_theWolfMapIsHonest`, `BC13_budgetAndSafety` | OWNED |
| BC-10 (§3) | Scaling: env × freq scale × Q scale to both; mass scale to bank only | (branch) `getBodyCouplingScaling` | n/a | (branch) `BC05`, `ENV13_theWolfFollowsTheEnvironment` | OWNED |
| BC-11 (§4) | Params body_coupling_amount / body_mode_mass_scale / _q_scale / _freq_scale / body_coupling_modes | (branch) `Parameters.*` | (branch) BODY COUPLING group | (branch) `BC12_legacyLoadIsOff` | OWNED |
| BC-12 (§4) | `body` range family | (branch) `RangeFamily::body` | (branch) CHARACTER padlock | (branch) `BC12` | OWNED |
| BC-13 (§5) | Advanced Col 1 BODY Coupling knob next to air readout | (branch) n/a | (branch) `AdvancedPanel` `bodyCoupling` knob "Coupling" | (branch) build/manual only | OWNED |
| BC-14 (§5) | CHARACTER BODY COUPLING group after SETUP: scales, modes, mode list | (branch) n/a | (branch) `UI/RealismGroups.*:BodyCouplingGroup`, `BodyModeList` | (branch) `RealismUi.theCharacterPanelCarriesTheGroups` | OWNED |
| BC-15 (§5) | Wolf map strings × frets 0-19, warning colour > 30 % + dot glyph | (branch) `BodyCouplingBank::wolfMap/predictedLoss` | (branch) `WolfMap` | (branch) `BC11_theWolfMapIsHonest` | OWNED |
| BC-16 (§5) | Tap button fires one body tap | (branch) `requestBodyTap` | (branch) BODY COUPLING Tap | (branch) `BC06_BC09` | OWNED |
| BC-17 (§5) | Workshop body inspector mirrors mode list and wolf map — owner defers | none | none (owner deferred) | - | MISSING |
| BC-18 (§5) | CHARACTER padlock covers `body` | (branch) n/a | (branch) `RangeTabButton` families | (branch) `BC12` | OWNED |
| BC-19 (§6) | Bank derived not stored; legacy load writes 0 | (branch) `PresetManager::fromVar` REALISM-A block | n/a | (branch) `BC12_legacyLoadIsOff` | OWNED |
| BC-20 (§6) | Factory presets re-voiced with coupling on (listening pass) — owner defers; factory files omit the key so load at 0 | none | n/a | - | MISSING |
| BC-21 (§7) | Smoothing 20 ms amount, 0.2 %/block freq slew, 0.05 % redesign threshold | (branch) `processSample`, `beginBlock` | n/a | (branch) `BC04` | OWNED |
| BC-22 (§7) | Budget (spec 0.08, owner amended 0.1 units), no audio alloc — perf-budget.md row not added | (branch) `design` message-thread only | n/a | (branch) `BC13_budgetAndSafety` | OWNED |
| BC-23 (§8 BC-01) | Passivity | (branch) | n/a | (branch) `BC01_theBankIsPassive` | OWNED |
| BC-24 (§8 BC-02) | Wolf on an acoustic | (branch) | n/a | (branch) `BC02_aWolfOnAnAcoustic` | OWNED |
| BC-25 (§8 BC-03) | Solidbody is mild | (branch) | n/a | (branch) `BC03_aSolidbodyIsMild` | OWNED |
| BC-26 (§8 BC-04) | Off is identical | (branch) | n/a | (branch) `BC04_offIsBitIdentical` | OWNED |
| BC-27 (§8 BC-05) | Wolf moves with the body | (branch) | n/a | (branch) `BC05_theWolfMovesWithTheBody` | OWNED |
| BC-28 (§8 BC-06) | Tap tone | (branch) | n/a | (branch) `BC06_BC09_aTapRingsTheStringsNearAMode` | OWNED |
| BC-29 (§8 BC-07) | Sympathetic ring through the body | (branch) | n/a | (branch) `BC07_sympatheticRingThroughTheBody` | OWNED |
| BC-30 (§8 BC-08) | Heavy bridge couples less | (branch) | n/a | (branch) `BC08_aHeavyBridgeCouplesLess` | OWNED |
| BC-31 (§8 BC-09) | Muted strings do not answer | (branch) | n/a | (branch) `BC06_BC09_aTapRingsTheStringsNearAMode` | OWNED |
| BC-32 (§8 BC-10) | Stability sweep (owner: 200 corners × 1 s) | (branch) | n/a | (branch) `BC10_stabilityAcrossTheAdvancedCorners` | OWNED |
| BC-33 (§8 BC-11) | Wolf map is honest | (branch) | n/a | (branch) `BC11_theWolfMapIsHonest` | OWNED |
| BC-34 (§8 BC-12) | Legacy load | (branch) | n/a | (branch) `BC12_legacyLoadIsOff` | OWNED |
| BC-35 (§8 BC-13) | Budget and safety | (branch) | n/a | (branch) `BC13_budgetAndSafety` | OWNED |
