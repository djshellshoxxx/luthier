## part-acoustics.md

`mapSpec` (Model/Workshop/PartAcoustics.cpp) is the one deterministic mapping from parts to engine numbers. It runs once per swap, and every part field it reads can be edited in the WORKSHOP inspector. Wood, joint, bridge/tailpiece/pickguard mass, frets, nut, magnets, covers, pole pieces, position, wiring components and strings all reach the engine. Chambering now reaches the feedback loop (MODEL-GAPS merged). Some computed values are never consumed: `airResonanceHz/Q`, `bodyGainDb`, `finishDampingDb`, and the part's `winding_pitch_per_mm` (PlayingNoise recomputes it). Body modes use BodyModels' own wood table, whose values differ from §1. These fields are not mapped: neck wood/profile, fretboard radius, bridge.piezo, coil_turns, switching, strings.core and tension_kg. Loss-domain damping composition is missing too. nut.friction is on realism-c.

| Req | Summary | Engine location | GUI location | Test | Status |
|---|---|---|---|---|---|
| PA-1 (§0.1) | One `mapSpec(GuitarSpec)->DerivedAcoustics`, once per swap; the audio path never reads raw part fields | `Model/Workshop/PartAcoustics.cpp:mapSpec`, `LuthierEngine::applyWorkshopGuitar` | n/a | `WorkshopSwap::aSwapMapsOnceNotPerBlock` | DONE |
| PA-2 (§0.2) | Physical units in, DSP units out | `mapSpec`, `stringTensionNewtons` | n/a | `PartAcoustics::scaleLengthSetsTension` | DONE |
| PA-3 (§0.3) | Monotonic and continuous | `mapSpec` | n/a | `PartAcoustics::theMappingIsMonotonic` | DONE |
| PA-4 (§0.4) | Honest magnitudes: a small part swap gives a small real delta — no test bounds the size of a swap's spectral change | `mapSpec` multipliers | n/a | - | NO-TEST |
| PA-5 (§0.5) | Every constant named and sourced — fitted constants have no source: magnet 0.09/-0.9, coupling ×1.4, chamber feedback 0.1-0.8, shape-area 1.3×0.78, fretboard tanδ ×20 | `PartAcoustics.cpp`, `LuthierEngine.cpp:applyWorkshopGuitar` | n/a | - | PARTIAL |
| PA-6 (§1) | 17-wood table (ρ, E, tanδ) | `lookUpWood` | WORKSHOP inspector (`WorkshopPanel` fieldEditor -> `WorkshopBench::editField`) | `PartAcoustics::theWoodTableIsTheSpecs` | DONE |
| PA-7 (§1.1) | Body/top density and E set mode frequency, `f ∝ sqrt(E/ρ)` — body modes come from `BodyModels` kWoods, which have different ρ/E; the §1 table only trims by a density override | `mapSpec` (`engineWood`, `resonanceTrim`), `Model/Guitar/BodyModels.cpp` | WORKSHOP inspector | `PartAcoustics::theMappingIsMonotonic` (density only) | PARTIAL |
| PA-8 (§1.1) | tanδ sets mode Q, `Q≈1/(2tanδ)` — Q comes from BodyModels' loss factors (e.g. alder 0.013, not 8.5e-3); §1 tanδ is used only for fretboard brightness | `BodyModels.cpp` (Q from loss factor) | n/a | - | PARTIAL |
| PA-9 (§1.1, §3) | Neck and fretboard wood/density feed neck mass, body coupling and dead-spot frequency — not mapped | - | - | - | MISSING |
| PA-10 (§2) | body.wood and a `density_kg_m3` override | `mapSpec` body block | WORKSHOP inspector | `PartAcoustics::everyMappedFieldMovesSomething` | DONE |
| PA-11 (§2) | body.thickness_mm: mode frequency and mass | `scaleDepth`, `shapeFor` | WORKSHOP inspector | `PartAcoustics::theMappingIsMonotonic`, `PartAcoustics::everyMappedFieldMovesSomething` | DONE |
| PA-12 (§2) | body.area_cm2: mode frequency ∝ 1/area; air volume | `scaleWidth` | WORKSHOP inspector | `PartAcoustics::everyMappedFieldMovesSomething` | DONE |
| PA-13 (§2.1) | Chambering sets mode count (3/5/7/9/12), by BodyShape — no test checks the shape or mode count per value | `shapeFor` | WORKSHOP inspector | `PartAcoustics::chamberingPicksItsShapeAndSustain` | DONE |
| PA-14 (§2.1) | Air resonance per row (range, Q, 1/sqrt(V)) reaches the body: `BodyConfig::airHzOverride/airQOverride` from `mapSpec` build the Helmholtz mode | `mapSpec` chamber block -> `BodyModels::buildModes`/`computeAirResonance` | n/a | `PartAcoustics::chamberingReachesTheBodyEngine`, `chamberingPutsTheAirModeInItsRange` | DONE |
| PA-15 (§2.1) | Chambering mode gain +3/+6/+10 dB against solid on the body modes (electric chamberings; acoustic is the whole mic signal, not a colour) | `BodyConfig::modeGainDb` applied after normalisation | n/a | `PartAcoustics::chamberingReachesTheBodyEngine` (+6 dB semi-hollow) | DONE |
| PA-16 (§2.1) | Chambering sustain -5/-12/-20/-25% | `sustainScale × chamber.sustain` -> `partsSustain` | n/a | `PartAcoustics::chamberingPicksItsShapeAndSustain` | DONE |
| PA-17 (§2.1) | Chambering feedback coupling into the feedback loop | `d.feedbackGain` -> `FeedbackLoop::setBodyCoupling` | n/a | `ModelGaps::chamberingFeedsTheFeedbackCoupling` | DONE |
| PA-18 (§2) | Bracing: acoustic mode splitting | `engineBracing` -> `BodyConfig::bracing` | WORKSHOP inspector | `PartAcoustics::bracingSplitsTheAcousticModes` | DONE |
| PA-19 (§3) | Scale length -> tension `T=(2Lf)²μ` | `stringTensionNewtons`, `StringMaterials::computeSpec` | WORKSHOP inspector | `PartAcoustics::scaleLengthSetsTension` | DONE |
| PA-20 (§3) | neck.profile: mass distribution only — not mapped (the renderer only draws it) | - | - | - | MISSING |
| PA-21 (§3) | Joint coupling: bolt 0.55 / set 0.80 / through 0.95 | `jointCoupling` | WORKSHOP inspector | `PartAcoustics::couplingsMultiply` | DONE |
| PA-22 (§3) | fretboard.wood: termination damping; harder is brighter | `mapSpec` (tanδ -> `fretBrightness`) | WORKSHOP part swap | `PartAcoustics::fretboardFretsAndNutSetTheirBrightness` | DONE |
| PA-23 (§3) | fretboard.radius_mm: buzz clearance geometry — not mapped (DECISIONS lists it as not yet consumed) | - | - | - | MISSING |
| PA-24 (§4) | frets.material brightness NS .70 / SS .90 / EVO .80 / brass .60, plus buzz spectrum | `fretMaterialBrightness` -> `setTerminationBrightness` | WORKSHOP part swap | `PartAcoustics::fretboardFretsAndNutSetTheirBrightness`, `PartAcoustics::aReferenceGuitarSoundsLikeTheEngineDefault` (NS only) | DONE |
| PA-25 (§4) | frets.height_mm: buzz clearance | `d.setup.fretHeight` | WORKSHOP inspector | `PartAcoustics::everyMappedFieldMovesSomething` | DONE |
| PA-26 (§4) | frets.width_mm: wider is duller | `mapSpec` fretBrightness | WORKSHOP inspector | `PartAcoustics::everyMappedFieldMovesSomething` | DONE |
| PA-27 (§4) | frets.count: playable range | `d.spec.maxFrets` | WORKSHOP inspector | `PartAcoustics::fretCountAndNutSlotsReachTheSetup` | DONE |
| PA-28 (§4) | nut.material brightness, open strings only | `nutMaterialBrightness`; `LuthierEngine.cpp` (fret<=0 -> nut factor) | WORKSHOP part swap | `PartAcoustics::fretboardFretsAndNutSetTheirBrightness`, `PartAcoustics::aReferenceGuitarSoundsLikeTheEngineDefault` (bone only) | DONE |
| PA-29 (§4) | nut.slot_depths_mm: open-string clearance | `d.setup.nutDepth` | WORKSHOP setup strip | `PartAcoustics::fretCountAndNutSlotsReachTheSetup` | DONE |
| PA-30 (§4) | nut.friction: tuning stability under bends | on realism-c: `d.nutFriction` (tuning-stability) | - | - | OWNED |
| PA-31 (§5) | bridge.mass_g: termination impedance and sustain | `terminationMassG` -> `sustainScale` | WORKSHOP inspector | `PartAcoustics::theMappingIsMonotonic` | DONE |
| PA-32 (§5) | bridge.coupling | `couplingFraction` -> `spec.couplingAmount` | WORKSHOP inspector | `PartAcoustics::couplingsMultiply` | DONE |
| PA-33 (§5) | bridge.type preset mass/coupling table — the part files match the table, but `mapSpec` falls back to 100 g / 0.55 whatever the type, and no test checks the files | `Resources/Parts/Bridges/*.luthierpart` | WORKSHOP part swap | - | NO-TEST |
| PA-34 (§5) | has_tremolo / tremolo_type -> WhammyEngine type | `mapSpec` (`d.spec.bridge`) | WORKSHOP part swap | `PartAcoustics::bridgeAndTailpieceMapTheirFields` | DONE |
| PA-35 (§5) | bridge.piezo adds a saddle piezo source, no pickup slot used | `mapSpec` sets `hasPiezo` from `bridge.piezo` | WORKSHOP bridge part | `PartAcoustics::aPiezoBridgeAddsAPiezoSource` | DONE |
| PA-36 (§5) | tailpiece.mass_g adds to termination mass | `terminationMassG` | WORKSHOP inspector | `PartAcoustics::bridgeAndTailpieceMapTheirFields` | DONE |
| PA-37 (§5) | tailpiece.break_angle_deg: steeper is brighter | `mapSpec` (breakAngle -> fretBrightness) | WORKSHOP inspector | `PartAcoustics::fretboardFretsAndNutSetTheirBrightness` | DONE |
| PA-38 (§6) | Pickup L/R/C set the resonant peak with the load | `PickupDerived::spec` | WORKSHOP inspector | `PartAcoustics::everyMappedFieldMovesSomething`, `PartAcoustics::theMappingIsMonotonic` | DONE |
| PA-39 (§6, §6.2) | Magnet table: pull, damping, sustain and flat pull | `lookUpMagnet`, `LuthierEngine::applyWorkshopGuitar` (magnetSustain/Detune) | WORKSHOP inspector | `PartAcoustics::magnetPullShortensSustainAndPullsFlat` | DONE |
| PA-40 (§6) | coil_turns: output ∝ turns (dB against a per-family reference winding), inductance ∝ turns² unless stated | `mapSpec` pickup block | WORKSHOP pickup part | `PartAcoustics::coilTurnsSetTheOutput` | DONE |
| PA-41 (§6) | pole_piece_material eddy losses | `poleBrightness` -> `PickupEngine` | WORKSHOP inspector | - | NO-TEST |
| PA-42 (§6) | Nickel cover -0.8 dB at 4 kHz | `coverLossDbAt4k` -> `PickupEngine` | WORKSHOP inspector | `PartAcoustics::aCoverCostsTopEnd` | DONE |
| PA-43 (§6.1) | Position sets the comb | `spec.position` | WORKSHOP pickup drag | `PartAcoustics::pickupPositionSetsTheComb` | DONE |
| PA-44 (§6) | Height: output level and magnetic damping | `heightMm` -> `PickupEngine` heightGain, magnet proximity | WORKSHOP wheel/drag | `PartAcoustics::magnetPullShortensSustainAndPullsFlat` | DONE |
| PA-45 (§7) | Wiring pots, cap, taper, bleed, active -> circuit | `mapSpec` wiring block -> `CircuitComponents` | WORKSHOP inspector | `PartAcoustics::everyMappedFieldMovesSomething` (pots/cap) | DONE |
| PA-46 (§7) | `switching` topology (3-way/5-way/series/coil tap) — the part field is ignored; the selector parameter works independently | - | - | - | MISSING |
| PA-47 (§8) | gauges_in -> μ and tension | `StringMaterials::computeSpec` | WORKSHOP inspector | `PartAcoustics::scaleLengthSetsTension`, `StringPhysics::thickerStringsAreHeavierAndTighter` | DONE |
| PA-48 (§8) | winding and winding_material -> squeak/brightness material | `materialFor` | WORKSHOP part swap | `PartAcoustics::stringsAndFinishMapTheirFields` | DONE |
| PA-49 (§8) | core round/hex: warmth and stiffness — not mapped | - | - | - | MISSING |
| PA-50 (§8) | winding_pitch_per_mm feeds squeak and chirp — mapped into `d.windingPitchPerMm`, but the engine recomputes 1/wrap in `PlayingNoise` and ignores the part | `mapSpec`; `DSP/Noise/PlayingNoise.cpp` | n/a | - | PARTIAL |
| PA-51 (§8) | tension_kg[] override — not mapped | - | - | - | MISSING |
| PA-52 (§8) | Wound μ from core and winding geometry — uses a per-material `woundMassFactor`, not core diameter | `stringLinearDensity`, `StringMaterials` | n/a | - | PARTIAL |
| PA-53 (§8) | Inharmonicity `B ∝ d⁴E/(TL²)` | `StringMaterials::computeSpec` | n/a | `StringPhysics::woundStringsAreLessStiffThanTheirDiameterSuggests` | DONE |
| PA-54 (§9) | pickguard.mass_g: top damping on acoustics/thinlines — only added to termination mass | `terminationMassG` | WORKSHOP inspector | `PartAcoustics::everyMappedFieldMovesSomething` | PARTIAL |
| PA-55 (§9) | hardware_color has no audio effect | (not read by `mapSpec`) | n/a | `PartAcoustics::hardwareColourIsSilent` | DONE |
| PA-56 (§9) | finish.gloss: -0.5 dB and Q x0.92 on acoustic top modes (heard in Modal/Hybrid body mode; the default acoustic IR is a recording) | `BodyConfig::topDampingDb` in `buildModes` | n/a | `PartAcoustics::aThickFinishDampsTheTop` | DONE |
| PA-57 (§9) | finish.aging -> body break-in | `d.body.age` | n/a (guitar-file field) | `PartAcoustics::stringsAndFinishMapTheirFields` | DONE |
| PA-58 (§10) | Masses add (bridge + tailpiece + pickguard) | `terminationMassG` | n/a | `PartAcoustics::bridgeAndTailpieceMapTheirFields` | DONE |
| PA-59 (§10) | Couplings multiply | `couplingFraction` | n/a | `PartAcoustics::couplingsMultiply` | DONE |
| PA-60 (§10) | Dampings add in the loss domain, `1/Q = Σ1/Q_i` — not implemented; sustain factors multiply | - | - | - | MISSING |
| PA-T1 (§11) | Test: every field ±10% moves the rendered spectrum — the test compares the DerivedAcoustics struct only and skips unmapped fields | - | n/a | `PartAcoustics::everyMappedFieldMovesSomething` | PARTIAL |
| PA-T2 (§11) | Test: monotonicity for density, mass, thickness, inductance and position | - | n/a | `PartAcoustics::theMappingIsMonotonic` | DONE |
| PA-T3 (§11) | Test: 628 vs 648 mm tension within 1% | - | n/a | `PartAcoustics::scaleLengthSetsTension` | DONE |
| PA-T4 (§11) | Test: magnet pull shortens sustain ≥15% and pulls flat | - | n/a | `PartAcoustics::magnetPullShortensSustainAndPullsFlat` | DONE |
| PA-T5 (§11) | Test: first comb null within 5% | - | n/a | `PartAcoustics::pickupPositionSetsTheComb` | DONE |
| PA-T6 (§11) | Test: cover -0.8±0.2 dB | - | n/a | `PartAcoustics::aCoverCostsTopEnd` | DONE |
| PA-T7 (§11) | Test: chambering air mode in range, and the body engine builds it | - | n/a | `PartAcoustics::chamberingReachesTheBodyEngine` | DONE |
| PA-T8 (§11) | Test: hardware colour silent | - | n/a | `PartAcoustics::hardwareColourIsSilent` | DONE |
| PA-T9 (§11) | Test: composition, two halves make a quarter | - | n/a | `PartAcoustics::couplingsMultiply` | DONE |
| PA-T10 (§11) | Test: mapping runs once per swap | - | n/a | `WorkshopSwap::aSwapMapsOnceNotPerBlock` | DONE |
| PA-T11 (§11) | Test: determinism | - | n/a | `PartAcoustics::theMappingIsDeterministic` | DONE |

<!-- counts DONE=53 NO-GUI=0 NO-TEST=3 PARTIAL=7 MISSING=7 OWNED=1 -->
