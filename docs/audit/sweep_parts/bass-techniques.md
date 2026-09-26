## bass-techniques.md

With model-gaps merged, bass techniques are largely complete here: `SlapEngine` does slap (fast rise, fret collision through the fret-buzz generator), pop, double thump and ghosts with auto-ghosting; fingerstyle alternation and rest stroke exist; `BassFamilyDefaults` applies the section-8 table on a guitar load; the bass-only SLAP group (CHARACTER) and bass step grid (RHYTHM) are attached, and bass kits install grids; `BassTechniques.*` covers nearly every test in §12. Gaps: the fixed empty-state message is a constant that is never displayed; slap/pop position ranges are 5-400 mm and not in the `buzz` range family; double-thump spacing is `slap_rebound_gap`, not 1/(2 x strum_crossing_sps) (string-slap-technique.md later made it a user gap); imported/live BASS_TECH events do not drive the engine, the capture omits strength/fret-contact, and the Generic velocity mapping is untested.

| Req | Summary | Engine location | GUI location | Test | Status |
|---|---|---|---|---|---|
| BT-1 (§0.1) | Inert on a non-bass guitar; SLAP group hidden | `SlapEngine::setInstrument(bass)`; `RhythmEngine::setBassFamily` | CHARACTER > SLAP hidden (`SlapGroup::refresh`) | `BassTechniques.inertOnAGuitar`, `BassTechniques.theSlapGroupIsShownOnlyOnABass` | DONE |
| BT-2 (§0.1) | Fixed empty-state text "Bass techniques are inactive. Load a bass to use them." shown on a guitar — defined as `SlapGroup::kInactiveMessage` but never displayed (group just hides) | n/a | none (constant unused by any component) | `BassTechniques.theSlapGroupIsShownOnlyOnABass` (string only) | PARTIAL |
| BT-3 (§1) | Family from the spec (category Bass), not string count; 4/5/6 strings | `LuthierEngine` `spec.category == GuitarCategory::Bass` | n/a | `TunePlayer.theBassGoesToTheEngineOnlyForABass`, `BassTechniques.aFullSlapOnEveryStringHasHeadroom` (5-string) | DONE |
| BT-4 (§0.2, 2.1) | Slap collision: 0.3 ms rise, driven into frets, clack from the fret-buzz generator, inharmonic attack settling | `SlapEngine::shapeExcitation`, `makeContactBuzz`; `LuthierEngine::playSlapStrike` | n/a | `BassTechniques.aSlapIsNotALoudPluck`, `Slap.theClackIsTheFretBuzzGenerator`, `SlapWiring.theClackComesFromTheBuzzGenerator` | DONE |
| BT-5 (§2.2, 11) | slap_strength / thumb_hardness / fret_contact with defaults; fret_contact 0 = thumb thump, no clack | `Parameters.cpp` 762-765; `SlapEngine` | CHARACTER > SLAP (`SlapGroup`) | `Slap.theClackIsTheFretBuzzGenerator`, `SlapPresets.everySlapFieldRoundTrips` | DONE |
| BT-6 (§2.2, 3, 11) | slap_position_mm 20-200 and pop_position_mm 10-150, both in the `buzz` range family — params are 5-400 mm, no PhysicalRange rows | `PhysicalRange.cpp` buzz rows (stock = declared 5-400) | SLAP `slapPosition`, `popPosition` | `Ranges.stockMatchesTheDeclaredRange` | DONE |
| BT-7 (§3) | Pop: release snap-back, brighter and shorter than slap; pop_strength 0.75, pos 40 | `SlapEngine` pop | SLAP `popStrength`, `popPosition` | `BassTechniques.aPopIsBrighterAndShorterThanASlap` | DONE |
| BT-8 (§4) | Double thump: up-stroke x0.65, brighter; enabled + ratio params | `SlapEngine.cpp` ~491 (rebound, `velocityForLevelRatio`) | SLAP `doubleThump`, `upRatio` | `Slap.theUpStrokeComesAtItsGapAndItsRatio` | DONE |
| BT-9 (§4) | Return stroke at 1/(2 x strum_crossing_sps) — uses `slap_rebound_gap` (60 ms) instead | `SlapEngine.cpp` `reboundGapMs` | (rebound gap has no control here; techniques SLAP page) | `SlapWiring.theDoubleThumpComesBackAtItsGap` | PARTIAL |
| BT-10 (§0.3, 5) | Ghost: heavy damping before the strike (shared with the chuck), ghost_level/damping | `SlapEngine::applyGhostDamping` | SLAP `ghostLevel`, `ghostDamping` | `SlapWiring.aGhostIsAThumpWithNoPitch` | DONE |
| BT-11 (§5) | Auto-ghost below threshold (32), on by default for bass | `SlapEngine::classify` | SLAP `ghostAuto`, `ghostThreshold` | `BassTechniques.autoGhostingFiresBelowTheThresholdOnly` | DONE |
| BT-12 (§5) | Ghost triggered explicitly by the BASS_TECH MIDI event class — only the step grid sets `bassTechnique`; no MIDI/import path decodes BASS_TECH into the engine | `SlapEngine::classify` (`e.bassTechnique`) | RHYTHM bass grid only | `BassTechniques.aGridTechniqueIsAStrikeOnABassOnly` | PARTIAL |
| BT-13 (§6) | Finger alternation variation (timing and tone), default 0.25 | `DSP/Slap/BassFingerstyle.h` | SLAP `alternation` | `BassTechniques.fingerAlternationVaries` | DONE |
| BT-14 (§6) | Rest stroke damps the next-lower string (>= 12 dB in 10 ms) | `BassFingerstyle` rest stroke | SLAP `restStroke` | `BassTechniques.theRestStrokeDampsTheNextLowerString` | DONE |
| BT-15 (§6, 8) | Bass pluck position default nearer the bridge (0.12) | `BassFamilyDefaults` pluckPosition | Advanced pluck position | `BassTechniques.bassDefaultsApplyOnLoad` | DONE |
| BT-16 (§7) | Pick bass 1.14 mm default | `BassFamilyDefaults` pickThicknessMm | CHARACTER > PICK `pickThickness` | `BassTechniques.bassDefaultsApplyOnLoad` | DONE |
| BT-17 (§7) | Bass palm-mute profile: shorter decay, more fundamental | `StringEngine` `Damping::PalmMuteBass` | n/a | `BassTechniques.aBassPalmMuteIsShorterAndDarker` | DONE |
| BT-18 (§8) | Strum 100 sps, miss 0.01 on bass | `retargetStrumDefaults` | RHYTHM STRUM | `StrumDynamics.bassDefaultsApply`, `BassTechniques.bassDefaultsApplyOnLoad` | DONE |
| BT-19 (§8) | Squeak pressure 0.35, setup Factory low, 864 mm scale, compressor on 2:1 | `Model/Guitar/BassDefaults.cpp:retarget` | respective groups | `BassTechniques.bassDefaultsApplyOnLoad` | DONE |
| BT-20 (§8) | Defaults, not constraints: a user value survives the family change | `BassFamilyDefaults::retarget` follow() | n/a | `BassTechniques.aUserSettingSurvivesTheFamilyChange` | DONE |
| BT-21 (§9) | SLAP group on CHARACTER (bass only) with all 14 listed controls | `UI/SlapGroup.*` | CHARACTER > SLAP (`CharacterPanel::slapGroup`) | `BassTechniques.theSlapGroupIsShownOnlyOnABass` | DONE |
| BT-22 (§9) | Bass step grid on RHYTHM: thumb/pop/ghost/finger/dead per step (bass only) | `BassStepGrid`; `RhythmEngine` bass grid | RHYTHM > `BassGridGroup` | `BassTechniques.theStepGridPlaysItsTechniquesOnABass`, `BassTechniques.theStepGridRoundTripsAndEmptyLeavesThePattern` | DONE |
| BT-23 (§9) | Genre kits gain bass kits using the grid | `GenreKitLibrary` bass kits | RHYTHM kit selector | `BassTechniques.theBassKitsInstallTheirGrids` | DONE |
| BT-24 (§10) | BASS_TECH Luthier profile: technique, strength, position, fret-contact per event, round-trips exactly — capture writes tech/str/pos only (force left at default, no fret-contact field); re-imported events are not replayed | `LuthierEngine::captureBassTechnique`; `PerformanceCapture::bassTechnique`; `LuthierMidiEvents` bassTech | MIDI OUT panel | `BassTechniques.aStrikeIsCapturedAsBassTech`, `MidiExport.everyEventClassRoundTripsWithEveryField` | PARTIAL |
| BT-25 (§10) | Generic: slap/pop high velocity, ghost low velocity — not implemented as a mapping, untested | none (notes keep their played velocity) | MIDI OUT | - | MISSING |
| BT-26 (§11) | 14 params with ranges/defaults; gesture params not in a range family | `Parameters.cpp` 762-790 | SLAP group | `GuiReach.everyAutomatableParameterHasAVisibleControl` | DONE |
| BT-T1 (§12) | Test: inert on a guitar | | n/a | `BassTechniques.inertOnAGuitar` | DONE |
| BT-T2 (§12) | Test: slap uses FretBuzz at contact 0.8, not at 0 | | n/a | `Slap.theClackIsTheFretBuzzGenerator` | DONE |
| BT-T3 (§12) | Test: slap not a loud pluck (centroid 1.5x, rise 3x) | | n/a | `BassTechniques.aSlapIsNotALoudPluck` | DONE |
| BT-T4 (§12) | Test: pop brighter and shorter by 20 % | | n/a | `BassTechniques.aPopIsBrighterAndShorterThanASlap` | DONE |
| BT-T5 (§12) | Test: double thump two events at spacing, ratio within 0.5 dB | | n/a | `Slap.theUpStrokeComesAtItsGapAndItsRatio` | DONE |
| BT-T6 (§12) | Test: ghosts pitchless | | n/a | `SlapWiring.aGhostIsAThumpWithNoPitch` | DONE |
| BT-T7 (§12) | Test: auto-ghost threshold | | n/a | `BassTechniques.autoGhostingFiresBelowTheThresholdOnly` | DONE |
| BT-T8 (§12) | Test: rest stroke >= 12 dB in 10 ms | | n/a | `BassTechniques.theRestStrokeDampsTheNextLowerString` | DONE |
| BT-T9 (§12) | Test: finger alternation varies / not at 0 | | n/a | `BassTechniques.fingerAlternationVaries` | DONE |
| BT-T10 (§12) | Test: bass defaults on load | | n/a | `BassTechniques.bassDefaultsApplyOnLoad` | DONE |
| BT-T11 (§12) | Test: headroom, full slap every string < 0 dBFS limiter off | | n/a | `BassTechniques.aFullSlapOnEveryStringHasHeadroom` | DONE |
| BT-T12 (§12) | Test: BASS_TECH round-trips; Generic velocities keep slap/ghost distinction — Luthier part only via capture + format tests; Generic untested | | n/a | `BassTechniques.aStrikeIsCapturedAsBassTech` | PARTIAL |

<!-- counts DONE=31 NO-GUI=0 NO-TEST=0 PARTIAL=6 MISSING=1 OWNED=0 -->
