## muting-rhythm.md

Not on this checkout: `Source/WIP/Rhythm/Muting.*`, `WIP/UI/MuteGroup.*` and `WIP/Tests/MutingTests.cpp` are uncompiled drafts; the only live muting is the older PalmMute/MutedPick keyswitch articulation and strum-dynamics `chuck_amount`/`chuck_damping`. The techniques branch has the work (head 1b8bf87, 2026-09-24): it moves Muting out of WIP, adds `DSP/Techniques/MuteEngine`, `StringEngine` `Damping::Muted`, `mute_type` per pattern step, the live grid, MUTE sub-tab, Easy 4-way button, RHYTHM Mute Row, six grid presets and 19 `Muting.*` tests (spot-checked `Muting.h` MuteStep per-step pressure/position overrides, the six presets in `Muting.cpp`, test names). Owner gap: MIDI export of mute types (§8) is deferred.

| Req | Summary | Engine location | GUI location | Test | Status |
|---|---|---|---|---|---|
| MR-1 (§0.2, §1) | Seven mute types (open, palm light 150 ms / heavy 50 ms / extreme 20 ms, ghost, chuka, fret mute); muted strike is a thump + damped ring | (branch) `Rhythm/Muting.*:MuteType, Muting::dampingFor`; `StringEngine::setMutedDamping` | (branch) TECHNIQUES > MUTE, `MutePage` | (branch) `Muting.typesRoundTripThroughTheirIds`, `Muting.eachTypeDampsAsDescribed` | OWNED |
| MR-2 (§1) | Per-type position/pressure defaults, overridable per step | (branch) `MuteStep::pressure/positionMm`, `Muting::palmFactor` | (branch) MUTE page | (branch) `Muting.eachTypeDampsAsDescribed` | OWNED |
| MR-3 (§0.3-0.4, §2) | `mute_type` per pattern step; JSON extension; missing = open | (branch) `RhythmPattern::getMuteStep/setMuteStep`, `toVar/fromVar` | n/a | (branch) `Muting.theMuteGridsRoundTrip` | OWNED |
| MR-4 (§2, §4) | Live 16-step Mute Grid (runtime pattern) synced to host tempo, in Advanced Col 4 | (branch) `MuteEngine` live grid, `MuteEngine::apply` | (branch) TECHNIQUES > MUTE grid (Col 4 tab) | (branch) `Muting.paintingTheLiveGridAppliesWithinABar` | OWNED |
| MR-5 (§3) | Master mute mode overrides all pattern types | (branch) `Muting::resolve` | (branch) MUTE page + Easy Mute button | (branch) `Muting.theMasterModeOverridesEverything`, `resolutionOrderIsMasterThenStepThenChuka` | OWNED |
| MR-6 (§3) | Palm position (35 mm) / pressure (0.5) | (branch) `mute_palm_position/pressure`, `Muting::palmFactor` | (branch) MUTE page | (branch) `Muting.eachTypeDampsAsDescribed`, `Muting.parametersReachTheEngine` | OWNED |
| MR-7 (§3) | Fretting-hand mute style rock spread / classical | (branch) `MuteEngine::deadensOtherStrings` | (branch) MUTE page | (branch) `Muting.rockSpreadDeadensTheStringsAMutedStrumMisses` | OWNED |
| MR-8 (§3) | Chuka source: strums with dynamics < 0.3 | (branch) `Muting::resolve` (`kChukaDynamic`) | (branch) MUTE page | (branch) `Muting.aSoftStrumIsAChuka` | OWNED |
| MR-9 (§3) | Random mute humanise 0-1 (default 0) | (branch) `Muting::humanise` | (branch) MUTE page | (branch) `Muting.humaniseShiftsAboutHalfTheEligibleSteps` | OWNED |
| MR-10 (§3) | Ghost note velocity 0.4 | (branch) `MuteEngine::apply` | (branch) MUTE page | (branch) `Muting.aGhostNoteHasNoPitchedContent` | OWNED |
| MR-11 (§4) | RhythmEngine passes mute_type with each note-on | (branch) `RhythmEngine::emitNote` (`pendingMute`), `NoteOnEvent::muteType` | n/a | (branch) `Muting.aPatternsMuteRowReachesItsNotes` | OWNED |
| MR-12 (§4) | StringEngine applies initial damping + post-strike release | (branch) `LuthierEngine::techniqueStrike`, `techniqueBeginBlock` | n/a | (branch) `Muting.palmMuteHeavyOnLowEDecaysIn40To60Ms`, `Muting.aFretMuteRingsThenStops` | OWNED |
| MR-13 (§5) | Mute stacks with slap/scrape/slide/tap/bend | (branch) `CascadeResolver` | n/a | (branch) `Muting.aMuteIsStampedOnAnyTechnique`, `Cascade.theMatrixIsTheSpecs` | OWNED |
| MR-14 (§6) | Six presets (Metal Chug 16ths ... Classical Staccato) | (branch) `Muting.cpp` grid presets; `Presets/TechniquePresets.cpp` | (branch) MUTE preset list, browser | (branch) `TechniquesUi.thePresetChipFilters`, `Muting.theMuteControlsDriveTheModel` | OWNED |
| MR-15 (§7) | Techniques > Muting sub-tab: 16-step editor + all controls | (branch) - | (branch) `UI/MuteGroup.*`, `MutePage` | (branch) `TechniquesUi.everySubTabRendersItsControls` | OWNED |
| MR-16 (§7) | Easy Playing strip 4-way Mute button (Off/Light/Heavy/Extreme) | (branch) `mute_master_mode` | (branch) `TechniquePillRow:EasyMuteButton` | (branch) `Muting.theEasyMuteButtonCyclesFourWays` | OWNED |
| MR-17 (§7) | RHYTHM tab Mute row | (branch) - | (branch) `RhythmPanel::muteRow` (`MuteGridEditor`) | (branch) `Muting.theMuteControlsDriveTheModel` | OWNED |
| MR-18 (§8) | MIDI export: Luthier SysEx NOTE `mute_type`; generic text meta — deferred on owner branch | - | n/a | - | OWNED |
| MR-T1 (§9) | Test: palm heavy low E T60 40-60 ms | (branch) | n/a | (branch) `Muting.palmMuteHeavyOnLowEDecaysIn40To60Ms` | OWNED |
| MR-T2 (§9) | Test: ghost < -30 dB pitched | (branch) | n/a | (branch) `Muting.aGhostNoteHasNoPitchedContent` | OWNED |
| MR-T3 (§9) | Test: chuka at dyn 0.2 no sustained pitch | (branch) | n/a | (branch) `Muting.aSoftStrumIsAChuka` | OWNED |
| MR-T4 (§9) | Test: grid paint applies within a bar | (branch) | n/a | (branch) `Muting.paintingTheLiveGridAppliesWithinABar` | OWNED |
| MR-T5 (§9) | Test: humanise 0.5 ~50% over 100 loops | (branch) | n/a | (branch) `Muting.humaniseShiftsAboutHalfTheEligibleSteps` | OWNED |
| MR-T6 (§9) | Test: preset save/restore round-trips grid | (branch) | n/a | (branch) `Muting.theMuteGridsRoundTrip` | OWNED |
| MR-T7 (§9) | Test: patterns without mute_type play identically | (branch) | n/a | (branch) `Muting.existingPatternsPlayIdentically` | OWNED |
| MR-T8 (§9) | Test: slap carries its mute; grid override propagates | (branch) | n/a | (branch) `Muting.aSlappedNoteCarriesItsMute` | OWNED |

<!-- counts DONE=0 NO-GUI=0 NO-TEST=0 PARTIAL=0 MISSING=0 OWNED=26 -->
