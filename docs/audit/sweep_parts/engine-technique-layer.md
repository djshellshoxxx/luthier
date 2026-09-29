## engine-technique-layer.md

This checkout has two of the five modules (`DSP/Noise/ScrapeEngine`, `DSP/Slap/SlapEngine`) with prepare/reset/processBlock, idle flag paths and tests, plus `Model/Playing/TechniqueTriggers` with keyswitches 12-18 and ~39 scrape/slap params; those rows are DONE. TapEngine, MuteEngine, CascadeResolver, `TechniqueLayer`, stage order, pre-bend/tap/slide keyswitches, undo classes and migration resets are on the techniques branch (has work; spot-checked `DSP/Techniques/*`, `LuthierEngineTechniques.cpp`, `TechniqueTriggers.h` KS 19-21, 64 appended params and the four `TechniqueLayer.*` tests). Owner gaps (in TECHNIQUES.md as deferred, or omitted): BodyCoupling `driveDirect` for body tap, ModMatrix `PreBendEvent` + per-string range factor, APVTS parameter groups `parameters/techniques/<t>/*`, the low-CPU-class banner, formal command/result types, MPE Z routing check, and the "100 pre-delta presets byte-identical" test is weakened.

| Req | Summary | Engine location | GUI location | Test | Status |
|---|---|---|---|---|---|
| ETL-1 (§1) | ScrapeEngine and SlapEngine modules (prepareToPlay/reset/processBlock) | `DSP/Noise/ScrapeEngine`, `DSP/Slap/SlapEngine` | n/a | `Scrape.resetRepeatsExactly`, `Slap.whatANoteBecomes` | DONE |
| ETL-2 (§0.2-0.3) | Existing modules: zero-cost idle flag path, no audio allocations, reset | `ScrapeEngine::processBlock`, `SlapEngine::processBlock` | n/a | `Scrape.idleCostsNothing`, `Slap.idleAndActiveStayInBudget` | DONE |
| ETL-3 (§1, §0.2-0.3) | New TapEngine, MuteEngine, CascadeResolver with idle paths and reset | (branch) `DSP/Techniques/TapEngine`, `MuteEngine`, `CascadeResolver`, `TechniqueLayer.h` | n/a | (branch) `Tap.*`, `Muting.*`, `Cascade.*`, `Tap.cpuStaysInBudget` | OWNED |
| ETL-4 (§2) | Pipeline insertion 2b CascadeResolver, 2c technique modules, before RhythmEngine/StringEngine | (branch) `LuthierEngine::techniqueBeginBlock`, `LuthierEngineTechniques.cpp` | n/a | (branch) `TechniqueLayer.theModulesRunInTheDocumentedOrder` | OWNED |
| ETL-5 (§1, §2, §3.4) | Body Tap routes to BodyCoupling directly via `driveDirect(impulse, position)` — missing on every branch | - (SlapEngine internal knock resonators) | n/a | - | OWNED |
| ETL-6 (§3.1) | TechniqueEngine gesture types scrape/slap/tap/body-tap; emit() | `TechniqueTriggers:TechniqueId{scrape,slap}`; (branch) tap/pre-bend/slide ids | n/a | `TechniqueTriggers.keyswitchesAreTakenAndBecomeEvents`; (branch) `Tap.theTriggersTakeTheirNotes` | OWNED |
| ETL-7 (§3.2) | StringEngine: N concurrent contact points (taps as capos in series) | (branch) `TapEngine::soundingFret`, `techniqueFret` | n/a | (branch) `Tap.twoTapsOnAStringAreCaposInSeries` | OWNED |
| ETL-8 (§3.3) | RhythmEngine step `mute_type`, emitted with note-on | (branch) `RhythmEngine::emitNote`, `RhythmPattern::getMuteStep` | n/a | (branch) `Muting.aPatternsMuteRowReachesItsNotes` | OWNED |
| ETL-9 (§3.5) | ModMatrix `PreBendEvent` source + per-string microtonal range factor — deferred on owner branch | - | n/a | - | OWNED |
| ETL-10 (§3.6) | Keyswitches for scrape / slap types from an unused range, no conflict | `TechniqueTriggers.h:TechniqueKeyswitch` 12-18 | n/a | `TechniqueTriggers.keyswitchesAreTakenAndBecomeEvents`, `TechniqueTriggers.nothingArmedMeansNothingTaken` | DONE |
| ETL-11 (§3.6) | Keyswitches for tap (19), pre-bend (20), slide gesture (21) | (branch) `TechniqueKeyswitch::tap/preBend/slideGesture` | n/a | (branch) `Tap.theTriggersTakeTheirNotes`, `Bend.aPreBendStartsFlatAndReleases`, `SlideControls.aScriptedGestureArrivesOnTime` | OWNED |
| ETL-12 (§3.6) | MPE Y/Z routed to slide position / pressure and bends | (branch) `SlideControlSettings`, `StringBendSource::mpeY`, `VibratoSource::mpeZ` | (branch) SLIDE / BEND pages | (branch) `SlideControls.everyControlRoundTrips` (no MPE Z render test) | OWNED |
| ETL-13 (§4) | Commands Arm / SetParam / TriggerGesture / LoadMuteGrid; `TechniqueFiredResult` for overlays | (branch) param atomics, `TapEngine::requestGesture` lock-free queue, fire counters | (branch) `TechniqueOverlay` | (branch) `TechniqueLayer.commandsDoNotAllocate` | OWNED |
| ETL-14 (§5) | Six arm booleans + ~60 technique params | `scrape_armed` etc. (39 here); (branch) +64 in TECHNIQUES block | (branch) TECHNIQUES pills/pages | (branch) `Integration` param count, `*.everyControlRoundTrips` | OWNED |
| ETL-15 (§5) | APVTS grouped under `parameters/techniques/<technique>/*` — flat layout everywhere | - | n/a | - | OWNED |
| ETL-16 (§6) | Pre-delta presets load disarmed/default | (branch) `onTechniquesBlockLoaded` | n/a | (branch) `TechniqueLayer.preDeltaPresetsLoadDisarmedAndIdle` | OWNED |
| ETL-17 (§6) | Old bass-slap presets still reach SlapEngine | `SlapEngine` (same params) | n/a | `SlapPresets.everySlapFieldRoundTrips`; (branch) `TechniqueLayer.bassSlapPresetsStillReachTheSlapEngine` | OWNED |
| ETL-18 (§7) | Technique arm/controls + grid/scale/curve inside PRESET; gestures session-only | (branch) `TechniqueLayer::toVar/fromVar`, `PresetManager::captureTechniquesBlock` | n/a | (branch) `Muting.theMuteGridsRoundTrip`, `Bend.aLoadedScaleIsTheGrid` | OWNED |
| ETL-19 (§7) | Snapshot bank captures arm state | params captured by snapshots | LIVE tab snapshot bank | `StateModel.recallingASnapshotStaysInsideThePreset` (generic; no technique-arm assertion) | OWNED |
| ETL-20 (§8) | Undo classes technique-arm (no group), technique-param (200 ms), mute-grid-paint (200 ms); gestures not undoable | (branch) `TechniqueUndo` | n/a | (branch) `TechniquesUi.theUndoClassesGroupAsSpecified` | OWNED |
| ETL-21 (§9) | Idle < 0.1 %, per-module 0.5-1 %, all six ~2.5 %; low-CPU class defaults off + CPU-warning banner — banner deferred | (branch) flag-test idle paths | - | (branch) `Tap.cpuStaysInBudget`; `Scrape.idleCostsNothing` | OWNED |
| ETL-T1 (§10) | Test: pipeline order with counters | (branch) | n/a | (branch) `TechniqueLayer.theModulesRunInTheDocumentedOrder` | OWNED |
| ETL-T2 (§10) | Test: additive-only regression (whole suite) | (branch) | n/a | (branch) full suite | OWNED |
| ETL-T3 (§10) | Test: 100 pre-delta presets byte-identical playback — weakened to one idle test | (branch) | n/a | (branch) `TechniqueLayer.preDeltaPresetsLoadDisarmedAndIdle` | OWNED |
| ETL-T4 (§10) | Test: 1000 commands/s without audio allocation | (branch) | n/a | (branch) `TechniqueLayer.commandsDoNotAllocate` | OWNED |
| ETL-T5 (§10) | Test: old bass-slap preset maps to SlapEngine | (branch) | n/a | (branch) `TechniqueLayer.bassSlapPresetsStillReachTheSlapEngine` | OWNED |

<!-- counts DONE=3 NO-GUI=0 NO-TEST=0 PARTIAL=0 MISSING=0 OWNED=23 -->
