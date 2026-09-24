## string-scraping.md

The scrape engine is complete and well tested here: `ScrapeEngine` turns a gesture into per-winding catch impulses at sample offsets (plain strings emergently silent, pressure-scaled with a slight pitch load, mirrored directions, modwheel sweep, retrigger drop, zero idle cost), with keyswitch/CC/MPE triggers and cascade hooks for mute, slide and preemption. On this checkout none of the `scrape_*` parameters has a visible control and there is no on-screen trigger (only `pick_scrape_amount` in CHARACTER > PICK); the techniques branch adds TECHNIQUES > SCRAPE (`ScrapePage`), a "Scrape" pill with popover and the tap-preempts-scrape cascade, so those rows are OWNED. The five §4 presets exist only as `ScrapeSettings::fromPreset` (test-only) on every branch, and the bend-shifts-winding interaction has no test.

| Req | Summary | Engine location | GUI location | Test | Status |
|---|---|---|---|---|---|
| SC-1 (§0.1, 1) | Per-winding impulses, catches = travel x windings_per_mm, spread through the duration; no convolution | `DSP/Noise/ScrapeEngine.cpp:processBlock` (windingsPerMm) | n/a | `Scrape.catchesComeAtWindingsPerMmTimesSpeed` | DONE |
| SC-2 (§0.2) | Only wound strings scrape (plain < -30 dB vs wound E) | `ScrapeEngine` (no windings on plain) | n/a | `Scrape.aPlainStringIsNearSilent`, `ScrapeEngineWiring.thePlainHighEIsThirtyDecibelsUnderTheWoundLowE` | DONE |
| SC-3 (§0.3) | Fast vs slow scrape differ (zipper vs individual clicks) | `ScrapeEngine` catch rate | n/a | `Scrape.theFactoryScrapesAreWhatSectionFourSays` | DONE |
| SC-4 (§1) | Pressure scales catch amplitude and loads pitch slightly | `ScrapeEngine` pressure / loadCents | n/a | `Scrape.doublingPressureDoublesEachCatch`, `ScrapeEngineWiring.harderScrapingLoadsThePitchSlightly` | DONE |
| SC-5 (§1) | ScrapeGesture struct (string, start, end, duration, pressure, tool, angle) | `ScrapeEngine.h` `ScrapeGesture`/`ScrapeSettings` | n/a | `SlapWiring.slapAndScrapeTakeTheStringFromEachOther` (builds a gesture) | DONE |
| SC-6 (§0.4, 2) | Trigger: keyswitch / CC / MPE zone work — on-screen button missing here; on techniques: SCRAPE page trigger + "Scrape" pill (`TechniquePillRow`) | `scrape_trigger`, `ScrapeEngine::requestTrigger`, keyswitch 12 | none here (TECHNIQUES tab on techniques) | `Scrape.triggersListenOnlyWhereTheyAreTold`, `ScrapeEngineWiring.theKeyswitchScrapesAndNeverPlaysANote` | OWNED |
| SC-7 (§2) | Direction B->N / N->B / Hold+Sweep — no control here; on techniques: `ScrapePage` Direction | `scrape_direction` | (branch) TECHNIQUES > SCRAPE | `Scrape.aReversedScrapeIsTheForwardOneMirrored` | OWNED |
| SC-8 (§2) | Sweep source auto/modwheel/expression/aftertouch/custom CC — no control here; on techniques: Sweep source + Sweep CC | `scrape_sweep_source`, `scrape_sweep_cc` | (branch) TECHNIQUES > SCRAPE | `Scrape.aModwheelSweepCatchesWithinOneBlock` | OWNED |
| SC-9 (§2) | Sweep range start/end mm, default 200-900 — no control here; on techniques: Start/End knobs | `scrape_start_mm`, `scrape_end_mm` | (branch) TECHNIQUES > SCRAPE | `Scrape.theFactoryScrapesAreWhatSectionFourSays` | OWNED |
| SC-10 (§2) | Pressure 0-1 default 0.5 — no control here; on techniques: Pressure knob | `scrape_pressure` | (branch) TECHNIQUES > SCRAPE | `Scrape.doublingPressureDoublesEachCatch` | OWNED |
| SC-11 (§2) | Tool pick/nail/thumb default pick — no control here; on techniques: Tool choice | `scrape_tool` | (branch) TECHNIQUES > SCRAPE | `Scrape.theFactoryScrapesAreWhatSectionFourSays` | OWNED |
| SC-12 (§2) | Angle default 20 deg — no control here; on techniques: Angle knob | `scrape_angle` | (branch) TECHNIQUES > SCRAPE | - | OWNED |
| SC-13 (§2) | String mask, default wound strings only (0 = wound set) — no control here; on techniques: `StringMaskSelector` | `ScrapeEngine::effectiveMask` | (branch) TECHNIQUES > SCRAPE | `Scrape.aPlainStringIsNearSilent` | OWNED |
| SC-14 (§2, 6) | Retrigger threshold 200 ms, silent drop — no control here; on techniques: Retrigger knob | `scrape_retrigger` | (branch) TECHNIQUES > SCRAPE | `Scrape.aRetriggerInsideTheThresholdIsDropped` | OWNED |
| SC-15 (§2) | Duration (gesture/preset field) — no control here; on techniques: Duration knob | `scrape_duration` | (branch) TECHNIQUES > SCRAPE | `Scrape.catchesComeAtWindingsPerMmTimesSpeed` | OWNED |
| SC-16 (§2) | Scrape level trim (pick_scrape_amount), zero is free | `PlayingNoise`/`ScrapeEngine` level | CHARACTER > PICK `NoiseGroups::pickScrape` | `Scrape.theLevelTrimScalesAndZeroIsFree` | DONE |
| SC-17 (§3) | ScrapeEngine trigger/processBlock/reset; zero idle cost (flag check) | `ScrapeEngine` | n/a | `Scrape.idleCostsNothing`, `Scrape.resetRepeatsExactly` | DONE |
| SC-18 (§3) | Pipeline: after TechniqueEngine, before StringEngine; impulses at sample offsets into the excitation | `LuthierEngine` block loop (`ScrapeEngine::getExcitation`) | n/a | `Scrape.catchesComeAtWindingsPerMmTimesSpeed`, `ScrapeEngineWiring.theKeyswitchScrapesAndNeverPlaysANote` | DONE |
| SC-19 (§4) | Five presets (Classic Rock, Metal Zipper, Slow Ratchet, Nail, Modwheel-Sweep) loadable — defined only in `ScrapeSettings::fromPreset`, called by tests; not in factory presets or browser on any branch | `ScrapeSettings::fromPreset` | none | `Scrape.theFactoryScrapesAreWhatSectionFourSays` | PARTIAL |
| SC-20 (§5) | Cascade: scrape through a palm mute is duller/thumpier | `ScrapeEngine::setMuteAmount` | n/a | `Scrape.aMuteMakesItDullerAndThumpier` | DONE |
| SC-21 (§5) | Cascade: slide takes the string (blocked) | `ScrapeEngine::setStringBlocked`; `LuthierEngine` ~1950 | n/a | `Scrape.aPreemptedScrapeFadesOutInTenMilliseconds` | DONE |
| SC-22 (§5) | Cascade: a bend stretches winding spacing — implemented, no test | `ScrapeEngine::setString(..., bendCents)` | n/a | - | NO-TEST |
| SC-23 (§5) | Cascade: tap on the same string damps the scrape — preempt hook here, no TapEngine; on techniques: `Cascade.aTapPreemptsAScrapeOnItsString` | `ScrapeEngine::preempt` | n/a | (branch) `Cascade.aTapPreemptsAScrapeOnItsString` | OWNED |
| SC-24 (§5) | Scrape on scrape queues; slap/scrape conflict latest wins | `ScrapeEngine` queue; `preempt` | n/a | `Scrape.aScrapeOnAScrapeQueues`, `SlapWiring.slapAndScrapeTakeTheStringFromEachOther` | DONE |
| SC-25 (§2 GUI) | Controls exposed in the Techniques tab — no TECHNIQUES tab here; on techniques: `TechniquesPanel` SCRAPE sub-tab | n/a | (branch) TECHNIQUES > SCRAPE | (branch) `TechniquesUi.everySubTabRendersItsControls` | OWNED |
| SC-T1 (§6) | Test: catch rate = windings_per_mm x speed | | n/a | `Scrape.catchesComeAtWindingsPerMmTimesSpeed` | DONE |
| SC-T2 (§6) | Test: plain high E < -30 dB | | n/a | `ScrapeEngineWiring.thePlainHighEIsThirtyDecibelsUnderTheWoundLowE` | DONE |
| SC-T3 (§6) | Test: pressure doubling doubles catch (20 %), more pitch modulation | | n/a | `Scrape.doublingPressureDoublesEachCatch`, `ScrapeEngineWiring.harderScrapingLoadsThePitchSlightly` | DONE |
| SC-T4 (§6) | Test: direction reversal mirrors | | n/a | `Scrape.aReversedScrapeIsTheForwardOneMirrored` | DONE |
| SC-T5 (§6) | Test: modwheel sweep within one block | | n/a | `Scrape.aModwheelSweepCatchesWithinOneBlock` | DONE |
| SC-T6 (§6) | Test: CPU idle < 0.05 %, active < 0.5 % | | n/a | `Scrape.idleCostsNothing`, `Scrape.anActiveScrapeStaysInBudget` | DONE |
| SC-T7 (§6) | Test: retrigger below threshold drops | | n/a | `Scrape.aRetriggerInsideTheThresholdIsDropped` | DONE |

<!-- counts DONE=18 NO-GUI=0 NO-TEST=1 PARTIAL=1 MISSING=0 OWNED=12 -->
