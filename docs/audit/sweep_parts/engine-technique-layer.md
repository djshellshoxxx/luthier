## engine-technique-layer.md

The compiled tree has two of the five modules (`DSP/Noise/ScrapeEngine`, `DSP/Slap/SlapEngine`) with idle paths and tests, `Model/Playing/TechniqueTriggers` keyswitches 12-18, ~32 scrape/slap params, `BodyCouplingBank::driveDirect` (used by slap body tap and the Tap button), and snapshot capture of all params. TapEngine, MuteEngine, CascadeResolver, `TechniqueLayer`, keyswitches 19-21, MPE Y/Z routing, technique undo classes, param grouping and the ModMatrix pre-bend source are not built (no `DSP/Techniques/`). Re-verified.

| Req | Summary | Engine location | GUI location | Test | Status |
|---|---|---|---|---|---|
| ETL-1 (§1) | ScrapeEngine and SlapEngine modules (prepareToPlay/reset/processBlock) | `DSP/Noise/ScrapeEngine`, `DSP/Slap/SlapEngine` | n/a | `Scrape.resetRepeatsExactly`, `Slap.whatANoteBecomes` | DONE |
| ETL-2 (§0.2-0.3) | Existing modules: zero-cost idle flag path, no audio allocations, reset | `ScrapeEngine::processBlock`, `SlapEngine::processBlock` | n/a | `Scrape.idleCostsNothing`, `Slap.idleAndActiveStayInBudget` | DONE |
| ETL-3 (§1, §0.2-0.3) | New TapEngine, MuteEngine, CascadeResolver with idle paths and reset | `DSP/Noise/ScrapeEngine`, `DSP/Slap/SlapEngine` only; no `DSP/Techniques/` (TapEngine, MuteEngine, CascadeResolver, TechniqueLayer.h absent) | n/a | - | MISSING |
| ETL-4 (§2) | Pipeline insertion 2b CascadeResolver, 2c technique modules, before RhythmEngine/StringEngine | `LuthierEngine.cpp` runs scrape/slap ad hoc; no 2b CascadeResolver / 2c stage order | n/a | `Scrape.idleCostsNothing`, `Slap.idleAndActiveStayInBudget` (per module only) | PARTIAL |
| ETL-5 (§1, §2, §3.4) | Body Tap routes to BodyCoupling directly via `driveDirect(impulse, position)` | `DSP/Coupling/BodyCouplingBank::driveDirect(impulse, position)` (BodyCouplingBank.cpp:349); `LuthierEngine.cpp:1646` slap bodyTap and `:2027` Tap button call it | n/a | `BodyCoupling.BC06_BC09_aTapRingsTheStringsNearAMode`, `SlapWiring.aBodyTapLeavesTheStringsAlone` | DONE |
| ETL-6 (§3.1) | TechniqueEngine gesture types scrape/slap/tap/body-tap; emit() | `Model/Playing/TechniqueTriggers.h:TechniqueId{scrape,slap}`; no tap/body-tap/pre-bend/slide ids | n/a | `TechniqueTriggers.keyswitchesAreTakenAndBecomeEvents` | PARTIAL |
| ETL-7 (§3.2) | StringEngine: N concurrent contact points (taps as capos in series) | - | n/a | - | MISSING |
| ETL-8 (§3.3) | RhythmEngine step `mute_type`, emitted with note-on | - (no mute_type in `RhythmEngine`/`RhythmPattern`) | n/a | - | MISSING |
| ETL-9 (§3.5) | ModMatrix `PreBendEvent` source + per-string microtonal range factor | - (no `PreBendEvent` source in `Modulation/`) | n/a | - | MISSING |
| ETL-10 (§3.6) | Keyswitches for scrape / slap types from an unused range, no conflict | `TechniqueTriggers.h:TechniqueKeyswitch` 12-18 | n/a | `TechniqueTriggers.keyswitchesAreTakenAndBecomeEvents`, `TechniqueTriggers.nothingArmedMeansNothingTaken` | DONE |
| ETL-11 (§3.6) | Keyswitches for tap (19), pre-bend (20), slide gesture (21) | `TechniqueTriggers.h:TechniqueKeyswitch` stops at palmSlap=18; no tap(19)/preBend(20)/slideGesture(21) | n/a | - | MISSING |
| ETL-12 (§3.6) | MPE Y/Z routed to slide position / pressure and bends | - (no MPE Y/Z routing to slide/bend; only `TriggerSource::mpeZone` channel trigger) | - (no SLIDE/BEND pages) | - | MISSING |
| ETL-13 (§4) | Commands Arm / SetParam / TriggerGesture / LoadMuteGrid; `TechniqueFiredResult` for overlays | `TechniqueTriggers::request` (lock-free) and param atomics for scrape/slap; no TriggerGesture/LoadMuteGrid/TechniqueFiredResult | n/a | - (no command allocation test) | PARTIAL |
| ETL-14 (§5) | Six arm booleans + ~60 technique params | `scrape_armed`, `slap_armed` etc. (32 `scrape_`/`slap_` ids in `Parameters.h`); no tap/mute/bend/cascade params | - (scrape params have no attached control; slap in CHARACTER `SlapGroup`) | `SlapPresets.everySlapFieldRoundTrips` | PARTIAL |
| ETL-15 (§5) | APVTS grouped under `parameters/techniques/<technique>/*` | - (APVTS layout is flat everywhere) | n/a | - | MISSING |
| ETL-16 (§6) | Pre-delta presets load disarmed/default | scrape/slap params default disarmed when absent from an old preset; no `onTechniquesBlockLoaded` | n/a | `SlapPresets.everySlapFieldRoundTrips` | PARTIAL |
| ETL-17 (§6) | Old bass-slap presets still reach SlapEngine | `DSP/Slap/SlapEngine` (same slap_* params) | n/a | `SlapPresets.everySlapFieldRoundTrips` | DONE |
| ETL-18 (§7) | Technique arm/controls + grid/scale/curve inside PRESET; gestures session-only | scrape/slap arm and controls are APVTS params saved in the preset; no grid/scale/curve block (`PresetManager` has no `captureTechniquesBlock`) | n/a | `SlapPresets.everySlapFieldRoundTrips` | PARTIAL |
| ETL-19 (§7) | Snapshot bank captures arm state | params captured by snapshots (`Live/Snapshots.cpp` excludes only `snapshotMorph`) | LIVE tab snapshot bank | `StateModel.recallingASnapshotStaysInsideThePreset` (generic; no technique-arm assertion) | NO-TEST |
| ETL-20 (§8) | Undo classes technique-arm (no group), technique-param (200 ms), mute-grid-paint (200 ms); gestures not undoable | - (no `TechniqueUndo`; no technique-arm / technique-param / mute-grid-paint undo classes) | n/a | - | MISSING |
| ETL-21 (§9) | Idle < 0.1 %, per-module 0.5-1 %, all six ~2.5 %; low-CPU class defaults off + CPU-warning banner | flag idle paths in `ScrapeEngine`/`SlapEngine` | n/a | `Scrape.idleCostsNothing`, `Slap.idleAndActiveStayInBudget` | PARTIAL |
| ETL-T1 (§10) | Test: pipeline order with counters | - | n/a | - | MISSING |
| ETL-T2 (§10) | Test: additive-only regression (whole suite) | full test suite (`Source/Tests`) | n/a | whole suite | PARTIAL |
| ETL-T3 (§10) | Test: 100 pre-delta presets byte-identical playback | - | n/a | - | MISSING |
| ETL-T4 (§10) | Test: 1000 commands/s without audio allocation | - | n/a | - | MISSING |
| ETL-T5 (§10) | Test: old bass-slap preset maps to SlapEngine | `DSP/Slap/SlapEngine` | n/a | `SlapPresets.everySlapFieldRoundTrips` (round trip, no legacy-preset fixture) | PARTIAL |

<!-- counts DONE=5 NO-GUI=0 NO-TEST=1 PARTIAL=9 MISSING=11 DEFERRED=0 -->
