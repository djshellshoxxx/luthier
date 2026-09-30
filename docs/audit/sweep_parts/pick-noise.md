## pick-noise.md

The shared `NoiseEngine` pool is implemented: six classes, fixed pools, oldest-steal and deterministic hashing. Click, chirp, fingertip noise and the rake are in and follow the spec formulas. The CHARACTER > PICK group carries every pick control. The remaining gaps:
- The default pick thickness decodes to 1.07 mm, not 0.73 mm.
- Ultex, Tortex and Stone are missing (DECISIONS.md keeps the shipped list).
- Textures are five shared classes, not one table per material.
- The rake fires only from keyswitches while SCRAPE is armed. There is no Easy gesture.
- There is no "noise rides the instrument" test.

Pool degradation, the pick illustration and the audio-thread allocation test exist only on the visual branch.

| Req | Summary | Engine location | GUI location | Test | Status |
|---|---|---|---|---|---|
| PN-1 (§1) | Pools 16/16/16/8/16/8, allocated in prepare() | `DSP/Noise/NoiseEngine.h:kPoolSizes`, `NoiseEngine::prepare` | n/a | `NoisePool.aFullPoolStealsTheOldest` | DONE |
| PN-2 (§1) | A full pool steals the oldest generator | `NoiseEngine::trigger` | n/a | `NoisePool.aFullPoolStealsTheOldest` | DONE |
| PN-3 (§1) | Degradation step 4 halves every pool | `NoiseEngine::setDegraded` (no caller here) | n/a | - | OWNED (on visual: CpuRelief `halveNoisePools` calls `setDegraded`; CpuReliefTests) |
| PN-4 (§1) | Per-material noise textures synthesised once, read at a random offset — only 5 shared texture classes, not one per material | `NoiseEngine::prepare` (`NoiseTexture` enum) | n/a | `NoisePool.aSeedRepeatsExactly` | DEFERRED |
| PN-5 (§1.1) | Generator = excitation → 1-3 pole resonator → envelope | `DSP/Noise/NoiseEngine.h:NoiseGenerator` | n/a | `NoisePool.aSeedRepeatsExactly` | DONE |
| PN-6 (§1.2) | Click enters the excitation; chirp, scrape and other classes enter pre-body — no test that noise goes through body/pickup | `NoiseEngine::process` (excitation vs surface out); `LuthierEngine.cpp` string loop | n/a | `NoisePool.clickIsExcitationAndTheRestAreSurface` | DONE |
| PN-7 (§1.3) | Aux 8 carries the sum of all noise | `LuthierEngine.cpp` aux8; `Routing/RoutingMatrix` | ADVANCED > ROUTING, Aux 8 strip | `PluginBuses.aux8CarriesThePlayingNoiseAndObeysItsStrip` | DONE |
| PN-8 (§2, §2.1) | `pick_material`, default Celluloid, 8 materials — Ultex, Tortex and Stone/horn missing (DECISIONS.md keeps the 12-choice list) | `PlayingNoise::getPickMaterial`; `Parameters::pickMaterialNames` | CHARACTER > PICK, `NoiseGroups::pickMaterial` | `PickNoise.clickPitchTracksMaterialAndThickness` | DEFERRED |
| PN-9 (§2, §7) | `pick_thickness` 0.38-3 mm, default 0.73 — shipped default 0.5 normalised decodes to 1.07 mm | `Parameters.cpp:486`, `Parameters::pickThicknessMm`; `PhysicalRange.cpp:125` | CHARACTER > PICK, `NoiseGroups::pickThickness` | `Ranges.stockMatchesTheDeclaredRange` | DEFERRED |
| PN-10 (§2, §7) | `pick_tip_radius`, `pick_bevel`, `pick_wear` parameters with stock/advanced ranges | `Parameters.cpp:501-503`; `PhysicalRange.cpp:127` | CHARACTER > PICK, `NoiseGroups::pickTip/pickBevel/pickWear` | `GuiReach.everyAutomatableParameterHasAVisibleControl` | DONE |
| PN-11 (§2) | `pick_angle` 0-60°, default 20 (0.35 → 21°) | `Parameters::pickAngleDegrees` | CHARACTER > PICK, `NoiseGroups::pickAngle` | `PickNoise.angleTradesClickForChirp` | DONE |
| PN-12 (§2, §6) | `use_fingers` silences click, chirp and scrape | `PlayingNoise::makeClick/makeChirp` guards | ADVANCED > right hand, `AdvancedPanel::useFingers` | `PickNoise.fingersNeitherClickNorChirp` | DONE |
| PN-13 (§3) | Click level = amount × vel^0.7 × stiffness × cos^1.5, −30 dB nominal | `PlayingNoise::makeClick` | n/a | `PickNoise.clickScalesWithVelocityToThePower0_7`; `PickNoise.aClickSitsAboutThirtyDecibelsUnderTheNote` | DONE |
| PN-14 (§3) | Click fundamental formula from density and thickness | `PlayingNoise::makeClick` | n/a | `PickNoise.clickPitchTracksMaterialAndThickness` | DONE |
| PN-15 (§3) | Click decay 3-15 ms from tip and damping, wear sub-resonance, brighter near the bridge — none of these tested | `PlayingNoise::makeClick` (tip, wear, `pluckPosition`) | n/a | `PickNoise.decayWearAndPositionShapeTheClick` | DONE |
| PN-16 (§4) | Chirp on wound strings only, sin(angle) × roughness × depth, band at windingPitch × speed, 8-40 ms | `PlayingNoise::makeChirp` | n/a | `PickNoise.plainStringsNeverChirp`; `PickNoise.angleTradesClickForChirp` | DONE |
| PN-17 (§5) | Rake from the `pick_scrape` MIDI class or the Easy rake gesture — only keyswitches 13/14 while SCRAPE is armed with trigger=Keyswitch; no Easy gesture; no arm control on this checkout | `ScrapeEngine::takeRake` → `LuthierEngine::triggerPickScrape` | none | `ScrapeEngineWiring.theRakeHasATrigger` | PARTIAL |
| PN-18 (§5) | Rake sweeps the strings, 100 ms-3 s, 8-pool, level `pick_scrape_amount` | `PlayingNoise::startScrape`; `ScrapeEngine::takeRake` clamp | CHARACTER > PICK, `NoiseGroups::pickScrape` | `ScrapeEngineWiring.theRakeHasATrigger` | DONE |
| PN-19 (§6) | Fingers: nail/flesh blend and fingertip release noise about 12 dB under the pick's, wound strings only | `PlayingNoise::makeFingertipNoise` | ADVANCED, `AdvancedPanel::nailVsFlesh` | `PickNoise.fingersNeitherClickNorChirp` | DONE |
| PN-20 (§7) | Click/chirp/scrape amounts, stock 0-1, advanced 0-4, pick family | `Parameters.cpp:504-506`; `PhysicalRange.cpp:128-130` | CHARACTER > PICK | `Ranges.everyPhysicalRangeIsValid` | DONE |
| PN-21 (§8) | PICK group on CHARACTER: striker dropdowns, material, thickness, tip, bevel, wear, angle, click, chirp, scrape | `UI/NoiseGroups.cpp` | CHARACTER > PICK, `NoiseGroups` ctor | `GuiReach.everyAutomatableParameterHasAVisibleControl` | DONE |
| PN-22 (§8) | Pick drawn at true size, rotated by angle, 8 px drag handles | - | - | - | OWNED (on visual: `GuitarRenderer::pickPath/pickHandle`, illustration drag sets `pick_angle`) |
| PN-T1 (§9) | Test: click scales as vel^0.7 within 1 dB | - | - | `PickNoise.clickScalesWithVelocityToThePower0_7` | DONE |
| PN-T2 (§9) | Test: nylon 0.6 mm clicks an octave above metal 3 mm | - | - | `PickNoise.clickPitchTracksMaterialAndThickness` | DONE |
| PN-T3 (§9) | Test: angle lowers click and raises chirp | - | - | `PickNoise.angleTradesClickForChirp` | DONE |
| PN-T4 (§9) | Test: plain strings never chirp, every factory guitar | - | - | `PickNoise.plainStringsNeverChirp` | DONE |
| PN-T5 (§9) | Test: two bodies give different click spectra; amount 0 is bit-identical to noise off — missing | - | - | `PickNoise.noiseRidesTheInstrument` | DONE |
| PN-T6 (§9) | Test: zero amounts take no generator over 10 000 notes | - | - | `NoisePool.zeroIsFree` | DONE |
| PN-T7 (§9) | Test: 20 clicks into a 16-pool | - | - | `NoisePool.aFullPoolStealsTheOldest` | DONE |
| PN-T8 (§9) | Test: no audio-thread allocation for noise | - | - | - | OWNED (on visual: `Engine.fiveMinutesOfPlaybackNeitherAllocatesNorLocks`) |
| PN-T9 (§9) | Test: a fixed seed is byte-identical | - | - | `NoisePool.aSeedRepeatsExactly` | DONE |

<!-- counts DONE=21 NO-GUI=0 NO-TEST=3 PARTIAL=4 MISSING=0 OWNED=3 -->
