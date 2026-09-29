## technique-cascade.md

This checkout has only ad-hoc pairwise cascade among scrape, slap and slide (`ScrapeEngine::preempt`, `SlapEngine::preempt`, slide string blocking in `LuthierEngine`), of which the 10 ms graceful preemption is DONE (`Scrape.aPreemptedScrapeFadesOutInTenMilliseconds`); there is no CascadeResolver, matrix, conflict UI or combined preset. The techniques branch has the work: `DSP/Techniques/CascadeResolver` (class table, matrix, priority rules, conflict messages), `TechniqueLayer::stages` order, six combined presets, red-slash pills and 10 `Cascade.*` tests including a 10 000-gesture fuzz (spot-checked). Owner gaps: the combined-preset "saved reference" is a second render rather than a stored reference and covers a short phrase, not 30 s; the preemption test measures an amplitude fade, not the spectral-discontinuity criterion; no test of the ~2.5 % all-six / 1 % body-tap CPU budget; body tap does not drive BodyCoupling directly (stage 7).

| Req | Summary | Engine location | GUI location | Test | Status |
|---|---|---|---|---|---|
| TC-1 (§0.1, §1) | Compatibility class table (pitch/damping/excitation/effect) | (branch) `DSP/Techniques/CascadeResolver::relation` | n/a | (branch) `Cascade.theMatrixIsTheSpecs` | OWNED |
| TC-2 (§0.2-0.3, §2) | Same-string matrix: compatible / queue / alternate / conflict, ~200 ms window | (branch) `CascadeResolver::relation/request` | n/a | (branch) `Cascade.theMatrixIsTheSpecs`, `Cascade.everyPairResolvesAsDocumented` | OWNED |
| TC-3 (§0.4) | Cross-string combinations always compatible | (branch) engine | n/a | (branch) `Cascade.techniquesOnDifferentStringsAreIndependent` | OWNED |
| TC-4 (§3.1-3.2, 3.4-3.6) | Priority: user beats auto; most recent wins; slide holds over scrape/tap; mute and bend always added | (branch) `CascadeResolver::request` | n/a | (branch) `Cascade.thePriorityRulesDecide`, `Tap.theSlideBarHoldsItsStrings` | OWNED |
| TC-5 (§3.3) | Graceful preemption: preempted gesture releases over a 10 ms crossfade | `DSP/Noise/ScrapeEngine::preempt`, `DSP/Slap/SlapEngine::preempt`; `LuthierEngine` call sites | n/a | `Scrape.aPreemptedScrapeFadesOutInTenMilliseconds`, `SlapWiring.slapAndScrapeTakeTheStringFromEachOther` | DONE |
| TC-6 (§4) | Cascade schedule: TechniqueEngine -> resolver -> excitation -> damping -> pitch -> StringEngine -> BodyCoupling; idle stages free | (branch) `LuthierEngine::techniqueBeginBlock`, `TechniqueLayer::stages` | n/a | (branch) `TechniqueLayer.theModulesRunInTheDocumentedOrder` | OWNED |
| TC-7 (§4 stage 7) | Body tap bypasses directly to BodyCoupling — not on owner branch (no `driveDirect`) | - | n/a | - | OWNED |
| TC-8 (§5) | Six combined presets (Metal Lead Combo, Funk Slap Groove, Slide Blues, Percussive Tap, Scrape Intro, Full Cascade Demo) | (branch) `Presets/TechniquePresets.cpp` | (branch) preset browser Techniques chip | (branch) `Cascade.theCombinedPresetsArmTheirTechniques` | OWNED |
| TC-9 (§0.5, §6) | Conflict indicator: red slash on pill + tooltip "Conflicts with Slide on strings 3-6…" | (branch) `CascadeResolver::conflictMessage` | (branch) `TechniquePill`, `TechniqueTable::conflictFor` | (branch) `Cascade.theConflictMessageNamesStrings`, `Cascade.aConflictShowsOnThePillWithinAFrame` | OWNED |
| TC-10 (§8) | Cascade decisions push no undo; arming a technique does | (branch) `TechniqueUndo` | n/a | (branch) `TechniquesUi.theUndoClassesGroupAsSpecified` | OWNED |
| TC-11 (§9) | Resolver O(strings); all six active ~2.5 % CPU; body tap ≤ 1 % — no CPU test on branch beyond tap | (branch) resolver | n/a | (branch) `Tap.cpuStaysInBudget` (tap only) | OWNED |
| TC-T1 (§7, §10) | Test: every matrix cell has a fixture (same string) | (branch) | n/a | (branch) `Cascade.everyPairResolvesAsDocumented` | OWNED |
| TC-T2 (§10) | Test: preemption ends over 10 ms, spectral discontinuity < 0.5 dB between frames — only amplitude fade measured | | n/a | `Scrape.aPreemptedScrapeFadesOutInTenMilliseconds`; (branch) `Cascade.aTapPreemptsAScrapeOnItsString` | OWNED |
| TC-T3 (§10) | Test: cross-string slap E / slide B / tap G / bend A | (branch) | n/a | (branch) `Cascade.techniquesOnDifferentStringsAreIndependent` | OWNED |
| TC-T4 (§7, §10) | Test: combined presets match a saved reference within 0.5 dB over 30 s — branch compares two renders | (branch) | n/a | (branch) `Cascade.theCombinedPresetsArmTheirTechniques` | OWNED |
| TC-T5 (§7) | Test: fuzz 10 000 gestures, no crash/orphans, CPU in budget | (branch) | n/a | (branch) `Cascade.aFuzzOfGesturesLeavesNothingBehind` | OWNED |
| TC-T6 (§10) | Test: conflict slash within one UI frame | (branch) | (branch) pills | (branch) `Cascade.aConflictShowsOnThePillWithinAFrame` | OWNED |

<!-- counts DONE=1 NO-GUI=0 NO-TEST=0 PARTIAL=0 MISSING=0 OWNED=16 -->
