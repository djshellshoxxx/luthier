## string-slap-technique.md

The generalised `SlapEngine` is complete here: four slap types, five trigger sources (velocity zone, keyswitches 15-18, CC, MPE zone, button request), force, per-type contact positions and string masks, ghost mode, rebound gap, snap-back and body-part knocks, with the clack from the fret-buzz generator and body taps bypassing the strings; `Slap.*` / `SlapWiring.*` / `SlapPresets.*` cover every §7 test. Body taps drive the body-coupling bank (`applySlapAction`) and the Slap/Pop tools are in the Easy Tool selector, but only the bass subset (CHARACTER > SLAP) has controls: the generic `slap_*` parameters have no GUI and there is no TECHNIQUES > SLAP sub-tab (NO-GUI / MISSING); tap/mute/bend cascade rows are PARTIAL. Not owned by anyone: the five §4 presets are test-only (`SlapSettings::fromPreset`), and the "thumb rebounds by default" wording conflicts with bass-techniques' `double_thump_enabled` default off.

| Req | Summary | Engine location | GUI location | Test | Status |
|---|---|---|---|---|---|
| SS-1 (§0.2) | Slap is a strike plus a slap-buzz event against the frets | `DSP/Slap/SlapEngine::makeContactBuzz` -> fret-buzz generator; `LuthierEngine::playSlapStrike` | n/a | `Slap.theClackIsTheFretBuzzGenerator`, `SlapWiring.theClackComesFromTheBuzzGenerator` | DONE |
| SS-2 (§0.3) | Position and force independent (position shapes tone, force loudness/buzz) | `SlapEngine::classify` (contactMm, force) | n/a | `Slap.theContactPointIsMeasuredFromTheLastFret`, `Slap.whatANoteBecomes` | DONE |
| SS-3 (§0.4) | Works on any guitar with wound strings, not bass-only | `SlapEngine` (no family gate on armed slaps) | n/a | `SlapWiring.thePlainHighEIsAudibleButClacksLess` | DONE |
| SS-4 (§1) | Slap type Thumb/Pop/Palm/Body Tap | `slap_type`, `SlapType` | none (no Techniques tab; no generic slap control in Source/UI) | `Slap.whatANoteBecomes` | NO-GUI |
| SS-5 (§1) | Trigger source velocity zone/keyswitch/CC/MPE zone/strip button; no on-screen slap button | `slap_trigger`, `slap_velocity_zone`, `slap_trigger_cc`; `TechniqueTriggers` | none (no Techniques tab; no generic slap control in Source/UI) | `Slap.theButtonAndTheKeyswitchesQueueTheirStrikes`, `SlapWiring.aThumbSlapIsTheSameHoweverItIsFired` | NO-GUI |
| SS-6 (§1) | Contact position defaults 60 thumb / 40 pop / 100 palm | `slap_position_mm`, `pop_position_mm`, `slap_palm_position_mm` | CHARACTER > SLAP (bass thumb/pop positions); palm position has no control | `Slap.theContactPointIsMeasuredFromTheLastFret` | NO-GUI |
| SS-7 (§1) | Contact force 0-1 default 0.6 | `slap_force` | none (no Techniques tab; no generic slap control in Source/UI) | `Slap.whatANoteBecomes` | NO-GUI |
| SS-8 (§1) | String mask: bass thumb E/A, palm all | `slap_string_mask`, `SlapEngine` effective mask | none (no Techniques tab; no generic slap control in Source/UI) | `Slap.theFactorySlapsAreWhatSectionFourSays` | NO-GUI |
| SS-9 (§1) | Ghost mode via modifier keyswitch (16) / dedicated CC (87) | `slap_ghost_mode`, `slap_ghost_cc` | none (no Techniques tab; no generic slap control in Source/UI) | `SlapWiring.aGhostIsAThumpWithNoPitch` | NO-GUI |
| SS-10 (§1) | Rebound on/off with user gap (default 60 ms); gap has no control | `double_thump_enabled`, `slap_rebound_gap` | CHARACTER > SLAP (toggle only) | `SlapWiring.theDoubleThumpComesBackAtItsGap` | NO-GUI |
| SS-11 (§1) | "Double thump on thumb rebounds by default" — `double_thump_enabled` defaults off (bass-techniques 11 says false) | `Parameters.cpp` 778: `double_thump_enabled` default false (matches bass-techniques 4; spec wording says rebound by default) | n/a | - | PARTIAL |
| SS-12 (§1) | Snap-back (bass only) | `slap_snap_back` | none (no Techniques tab; no generic slap control in Source/UI) | `Slap.theFactorySlapsAreWhatSectionFourSays` | NO-GUI |
| SS-13 (§1, 2) | Body tap resonance weighting modes, driven into the body-coupling mode bank | `SlapEngine::startBodyTap`/`knockFor`, `slap_body_part`; `LuthierEngine::applySlapAction` also calls `bodyCoupling.driveDirect` | none (no Techniques tab; no generic slap control in Source/UI) | `Slap.theBodyPartWeightsTheKnock`, `BodyCoupling.BC06_BC09_aTapRingsTheStringsNearAMode` | NO-GUI |
| SS-14 (§2) | SlapEngine trigger / processBlock / reset; consumes TechniqueEngine gestures | `SlapEngine::processBlock`, `reset`; `TechniqueTriggers` | n/a | `Slap.idleAndActiveStayInBudget`, `Slap.theButtonAndTheKeyswitchesQueueTheirStrikes` | DONE |
| SS-15 (§2) | Body tap bypasses StringEngine | `LuthierEngine::applySlapAction` bodyTap | n/a | `SlapWiring.aBodyTapLeavesTheStringsAlone` | DONE |
| SS-16 (§2) | Insertion after TechniqueEngine, alongside ScrapeEngine, before StringEngine | `LuthierEngine` block loop (slap.processBlock before strings) | n/a | `SlapWiring.aThumbSlapIsTheSameHoweverItIsFired` | DONE |
| SS-17 (§3) | Bass slap/pop share SlapEngine; bass controls stay | `SlapEngine` reads `slap_strength` etc. | CHARACTER > SLAP | `SlapWiring.aThumbSlapIsTheSameHoweverItIsFired` | DONE |
| SS-18 (§3) | Migration: existing bass presets keep working — on techniques: `TechniqueLayer.bassSlapPresetsStillReachTheSlapEngine` (slap_armed introduced there) | `slap_armed` param (`Parameters.cpp` 784); bass slap params reach `SlapEngine` via `setSlapSettings` | n/a | `SlapWiring.aThumbSlapIsTheSameHoweverItIsFired`, `SlapPresets.everySlapFieldRoundTrips` | DONE |
| SS-19 (§4) | Five presets (Bass Standard, Bass Aggressive, Funk Guitar Palm Slap, Acoustic Body Tap, Percussive Fingerstyle) loadable — `SlapSettings::fromPreset` called only by tests on every branch | `SlapSettings::fromPreset` (called only by tests; no factory presets or browser entries) | none | `Slap.theFactorySlapsAreWhatSectionFourSays` | PARTIAL |
| SS-20 (§5) | Cascade: compatible with palm mute and bend | `LuthierEngineRealismB.cpp` 642 (thumb palm-mute damping on slap strikes); MuteEngine is only in Source/WIP (not compiled), no BendEngine | n/a | - | PARTIAL |
| SS-21 (§5) | Cascade: tap alternation | `LuthierEngine::triggerNote` (Technique::Tap calls `slap.preempt`), `SlapEngine::classify` refuses Tap; no CascadeResolver | n/a | `Slap.anotherTechniqueOnTheStringDropsTheUpStroke` (up-stroke drop only) | PARTIAL |
| SS-22 (§5) | Not compatible with slide (bar under string) or scraping the same string — scrape conflict tested; the slide gate (`classify(e, underBar)`) has no test | `SlapEngine::classify(e, slide.isUnderBar)`; `scrape.preempt`/`slap.preempt` | n/a | `SlapWiring.slapAndScrapeTakeTheStringFromEachOther` (scrape only) | NO-TEST |
| SS-23 (§6) | GUI: TECHNIQUES > Slap sub-tab | n/a | none (no Techniques tab in Source/UI) | - | MISSING |
| SS-24 (§6) | Playing strip Tool selector gains Slap / Pop — "Slap" style segment (thumb slap / finger pop via `RhTool::slap/pop`) | `RhTool::slap/pop` | Easy `RightHandToolSelector` | `FingerstyleAttack.FA13_slapAndPopTools` | DONE |
| SS-T1 (§7) | Test: thumb slap 60 mm force 0.6 within 1 dB of the bass reference (all trigger paths agree) | | n/a | `SlapWiring.aThumbSlapIsTheSameHoweverItIsFired` | DONE |
| SS-T2 (§7) | Test: palm slap broadband, < -25 dB pitched | | n/a | `SlapWiring.aPalmSlapIsBroadbandAndPitchless` | DONE |
| SS-T3 (§7) | Test: body tap < -60 dB on string outputs | | n/a | `SlapWiring.aBodyTapLeavesTheStringsAlone` | DONE |
| SS-T4 (§7) | Test: ghost mode thump has no clear pitch | | n/a | `SlapWiring.aGhostIsAThumpWithNoPitch` | DONE |
| SS-T5 (§7) | Test: rebound gap ± 3 ms | | n/a | `SlapWiring.theDoubleThumpComesBackAtItsGap` | DONE |
| SS-T6 (§7) | Test: plain string audible with reduced buzz | | n/a | `SlapWiring.thePlainHighEIsAudibleButClacksLess` | DONE |
| SS-T7 (§7) | Test: CPU idle < 0.05 %, active < 0.6 % | | n/a | `Slap.idleAndActiveStayInBudget` | DONE |
| SS-T8 (§7) | Test: preset save/restore round-trips every added field | | n/a | `SlapPresets.everySlapFieldRoundTrips` | DONE |

<!-- counts DONE=17 NO-GUI=9 NO-TEST=1 PARTIAL=4 MISSING=1 OWNED=0 -->
