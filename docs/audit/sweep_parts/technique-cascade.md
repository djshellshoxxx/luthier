## technique-cascade.md

The compiled tree has only ad-hoc pairwise cascade among scrape, slap and slide (`ScrapeEngine::preempt`, `SlapEngine::preempt`, slide string blocking in `LuthierEngine`) with the 10 ms graceful preemption DONE (`Scrape.aPreemptedScrapeFadesOutInTenMilliseconds`) and body tap bypass via `BodyCouplingBank::driveDirect` DONE; there is no CascadeResolver, matrix, conflict UI, combined preset or `Cascade.*` test. Re-verified.

| Req | Summary | Engine location | GUI location | Test | Status |
|---|---|---|---|---|---|
| TC-1 (§0.1, §1) | Compatibility class table (pitch/damping/excitation/effect) | - (no `CascadeResolver`; only ad-hoc `ScrapeEngine::preempt`, `SlapEngine::preempt`) | n/a | - | MISSING |
| TC-2 (§0.2-0.3, §2) | Same-string matrix: compatible / queue / alternate / conflict, ~200 ms window | - (no `CascadeResolver`; only ad-hoc `ScrapeEngine::preempt`, `SlapEngine::preempt`) | n/a | - | MISSING |
| TC-3 (§0.4) | Cross-string combinations always compatible | - (no `CascadeResolver`; only ad-hoc `ScrapeEngine::preempt`, `SlapEngine::preempt`) | n/a | - | MISSING |
| TC-4 (§3.1-3.2, 3.4-3.6) | Priority: user beats auto; most recent wins; slide holds over scrape/tap; mute and bend always added | - (no `CascadeResolver`; only ad-hoc `ScrapeEngine::preempt`, `SlapEngine::preempt`) | n/a | - | MISSING |
| TC-5 (§3.3) | Graceful preemption: preempted gesture releases over a 10 ms crossfade | `DSP/Noise/ScrapeEngine::preempt`, `DSP/Slap/SlapEngine::preempt`; `LuthierEngine` call sites | n/a | `Scrape.aPreemptedScrapeFadesOutInTenMilliseconds`, `SlapWiring.slapAndScrapeTakeTheStringFromEachOther` | DONE |
| TC-6 (§4) | Cascade schedule: TechniqueEngine -> resolver -> excitation -> damping -> pitch -> StringEngine -> BodyCoupling; idle stages free | - (no `TechniqueLayer::stages`) | n/a | - | MISSING |
| TC-7 (§4 stage 7) | Body tap bypasses directly to BodyCoupling | `BodyCouplingBank::driveDirect` (BodyCouplingBank.cpp:349); `LuthierEngine.cpp:1646` slap bodyTap | n/a | `BodyCoupling.BC06_BC09_aTapRingsTheStringsNearAMode`, `SlapWiring.aBodyTapLeavesTheStringsAlone` | DONE |
| TC-8 (§5) | Six combined presets (Metal Lead Combo, Funk Slap Groove, Slide Blues, Percussive Tap, Scrape Intro, Full Cascade Demo) | - (no `Presets/TechniquePresets.cpp`) | - | - | MISSING |
| TC-9 (§0.5, §6) | Conflict indicator: red slash on pill + tooltip "Conflicts with Slide on strings 3-6…" | - (no `conflictMessage`) | - (no TechniquePill) | - | MISSING |
| TC-10 (§8) | Cascade decisions push no undo; arming a technique does | - (no `TechniqueUndo`) | n/a | - | MISSING |
| TC-11 (§9) | Resolver O(strings); all six active ~2.5 % CPU; body tap <= 1 % | per-module idle/budget in `ScrapeEngine`/`SlapEngine`; no resolver | n/a | `Slap.idleAndActiveStayInBudget`, `Scrape.idleCostsNothing` | PARTIAL |
| TC-T1 (§7, §10) | Test: every matrix cell has a fixture (same string) | - | n/a | - | MISSING |
| TC-T2 (§10) | Test: preemption ends over 10 ms, spectral discontinuity < 0.5 dB between frames | `DSP/Noise/ScrapeEngine::preempt` | n/a | `Scrape.aPreemptedScrapeFadesOutInTenMilliseconds` (amplitude fade only) | PARTIAL |
| TC-T3 (§10) | Test: cross-string slap E / slide B / tap G / bend A | - | n/a | `SlapWiring.slapAndScrapeTakeTheStringFromEachOther` (pair only) | MISSING |
| TC-T4 (§7, §10) | Test: combined presets match a saved reference within 0.5 dB over 30 s | - | n/a | - | MISSING |
| TC-T5 (§7) | Test: fuzz 10 000 gestures, no crash/orphans, CPU in budget | - | n/a | - | MISSING |
| TC-T6 (§10) | Test: conflict slash within one UI frame | - | - | - | MISSING |

<!-- counts DONE=2 NO-GUI=0 NO-TEST=0 PARTIAL=2 MISSING=13 DEFERRED=0 -->
