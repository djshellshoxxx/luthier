## string-interaction.md

REALISM-B has landed on this checkout: all six interactions (air path, palm spread, adjacent finger damping, release stagger, pickup crosstalk, muted-string thump), their seven parameters, the CHARACTER STRING INTERACTION group and fretboard palm-band shading, with `StringInteraction.SI01..SI14` registered. Still open: the fretting style is proxied from `rh_style` until techniques' `mute_fretting_style` exists (SI-5), the TECHNIQUES > MUTE palm-width mirror (SI-13), the §19 rows (SI-14), a reset/load/panic flag test (SI-15) and the per-string sum check at the new defaults (SI-24).

| Req | Summary | Engine location | GUI location | Test | Status |
|---|---|---|---|---|---|
| SI-1 (§0.4) | Every amount at 0 skips its path, bit-identical | per-feature guards | n/a | `StringInteraction.SI02_SI03_airOffIsBitIdenticalAndDelayed`, `StringInteraction.SI11_mutedStringThump` | DONE |
| SI-2 (§1) | Air path rank-1 term, 0.29 ms delay, HP 120 Hz, a_cat per family, through receive filter + kEnergyCap | `CouplingMatrix` air path; `LuthierEngine::refreshAirCoupling` | n/a | `StringInteraction.SI01_airMagnitudes`, `StringInteraction.SI02_SI03_airOffIsBitIdenticalAndDelayed` | DONE |
| SI-3 (§2) | Palm spread weights w(d) while P > 0.05; spreadMuted flag; never overrides Choked/Silenced/Chuck; restores on lift | `LuthierEngine::updatePalmSpread`, `palmWeight` | n/a | `StringInteraction.SI04_SI05_palmSpreadCoversAndLifts` | DONE |
| SI-4 (§3) | Adjacent finger damping (underside 1.0, tip 0.5) as Chuck; bitmask; lift on source note-off; chords don't self-mute | `applyAdjacentMute`, `liftMutesFrom` | n/a | `StringInteraction.SI06_SI07_neighbourMute` | DONE |
| SI-5 (§3) | Fretting style scales it (rock 1.0 / classical 0.1) from `MuteSettings::frettingStyle` — owner proxies via rh_style until techniques' mute_fretting_style merges | (branch) proxy on `rh_style` | (branch) text-only in STRING INTERACTION | (branch) `SI06_SI07_neighbourMute` | PARTIAL |
| SI-6 (§4) | Release stagger: 3 ms groups, bias/rank, seeded, never earlier, overflow fires now | `LuthierEngine::stageNoteOffs`, `ScheduledEvent::staggered` | n/a | `StringInteraction.SI08_SI09_releaseStagger` | DONE |
| SI-7 (§5) | Pickup crosstalk Gaussian aperture, bend lateral offset; g_s = 1 unbent | `PickupEngine::setStringLateralOffsets`, `updateCrosstalk` | n/a | `StringInteraction.SI10_crosstalk` | DONE |
| SI-8 (§6) | Muted-string thump in live and pattern strums; deadStrike skips activity/MIDI note/MIDI out | `Rhythm/MutedThump.h`, `MidiInterpreter::flushChordGroup`, `RhythmEngine::scheduleStrum` | n/a | `StringInteraction.SI11_mutedStringThump` | DONE |
| SI-9 (§7) | 7 params: coupling_air_amount, palm_mute_spread, adjacent_mute_amount, release_stagger_ms/_bias, pickup_aperture_scale, muted_thump_level; family rows | `Parameters.*`, `PhysicalRange.cpp` | STRING INTERACTION group | `StringInteraction.SI12_SI14_rangesAndPresets` | DONE |
| SI-10 (§8) | No new MIDI; CC 67 drives spread | `TechniqueEngine::getPalmMuteAmount` | n/a | `StringInteraction.SI04_SI05_palmSpreadCoversAndLifts` | DONE |
| SI-11 (§9) | CHARACTER STRING INTERACTION group | n/a | `UI/StringInteractionGroup.*` in `CharacterPanel` | `RealismBUi.theCharacterTabCarriesTheThreeGroups` | DONE |
| SI-12 (§9) | Mute-zone shading shows palm width and weights | n/a | fretboard palm band (`FretboardRealismB`) | `RealismBUi.theFretboardDrawsTheTouch` | DONE |
| SI-13 (§9) | Palm width mirrored in TECHNIQUES > MUTE — both realism-b and techniques omit it | none | none | - | MISSING |
| SI-14 (§9) | gui-integration §19 rows (primary CHARACTER > STRING INTERACTION) — not added | n/a | n/a (doc) | - | MISSING |
| SI-15 (§9) | Runtime flags cleared by reset, preset load and panic — `resetRealismB` clears the flags; no test | `resetRealismB` | n/a | - | NO-TEST |
| SI-16 (§10) | Budget 0.1 units (owner 0.2), no allocation | - | n/a | `StringInteraction.SI13_realtime` | DONE |
| SI-17 (§11 SI-01) | Air magnitudes | - | n/a | `StringInteraction.SI01_airMagnitudes` | DONE |
| SI-18 (§11 SI-02/03) | Air off bit-identical; air delay | - | n/a | `StringInteraction.SI02_SI03_airOffIsBitIdenticalAndDelayed` | DONE |
| SI-19 (§11 SI-04/05) | Palm coverage; lift restores | - | n/a | `StringInteraction.SI04_SI05_palmSpreadCoversAndLifts` | DONE |
| SI-20 (§11 SI-06/07) | Neighbour mute rock / classical / chords | - | n/a | `StringInteraction.SI06_SI07_neighbourMute` | DONE |
| SI-21 (§11 SI-08/09) | Stagger spread and order | - | n/a | `StringInteraction.SI08_SI09_releaseStagger` | DONE |
| SI-22 (§11 SI-10) | Crosstalk honesty | - | n/a | `StringInteraction.SI10_crosstalk` | DONE |
| SI-23 (§11 SI-11) | Thump | - | n/a | `StringInteraction.SI11_mutedStringThump` | DONE |
| SI-24 (§11 SI-12) | Per-string sum -80 dBFS with every feature at default — owner relies on existing RoutingTests, no assertion at the new defaults | `Routing` per-string test (pre-existing) | n/a | `Routing` suite (not at SI defaults) | PARTIAL |
| SI-25 (§11 SI-13) | Realtime, 0.1 units (relaxed to 0.2) | - | n/a | `StringInteraction.SI13_realtime` | DONE |
| SI-26 (§11 SI-14) | Ranges and preset round trip | - | n/a | `StringInteraction.SI12_SI14_rangesAndPresets` | DONE |
