## string-interaction.md

On this checkout only the pre-existing bridge `CouplingMatrix` exists; none of the six interactions (air path, palm spread, adjacent finger damping, release stagger, pickup crosstalk, muted-string thump), their seven parameters or the STRING INTERACTION group is present. The realism-b branch (active today, coverage complete) implements all six with `StringInteraction.SI01..SI14`, the CHARACTER group (`StringInteractionGroup`) and fretboard palm-band shading. Owner gaps: the palm-width mirror in TECHNIQUES > MUTE is deferred, gui-integration §19 rows are not added, the fretting-hand style is proxied from `rh_style == Classical` until muting-rhythm's `mute_fretting_style` merges (techniques branch), SI-13's budget was relaxed to 0.2 units, and SI-12 is covered only by the existing routing suite, not a dedicated -80 dBFS assertion.

| Req | Summary | Engine location | GUI location | Test | Status |
|---|---|---|---|---|---|
| SI-1 (§0.4) | Every amount at 0 skips its path, bit-identical | (branch) per-feature guards | n/a | (branch) `StringInteraction.SI02_SI03_airOffIsBitIdenticalAndDelayed`, `SI11_mutedStringThump` | OWNED |
| SI-2 (§1) | Air path rank-1 term, 0.29 ms delay, HP 120 Hz, a_cat per family, through receive filter + kEnergyCap | (branch) `CouplingMatrix` air path; `LuthierEngine::refreshAirCoupling` | n/a | (branch) `SI01_airMagnitudes`, `SI02_SI03` | OWNED |
| SI-3 (§2) | Palm spread weights w(d) while P > 0.05; spreadMuted flag; never overrides Choked/Silenced/Chuck; restores on lift | (branch) `LuthierEngine::updatePalmSpread`, `palmWeight` | n/a | (branch) `SI04_SI05_palmSpreadCoversAndLifts` | OWNED |
| SI-4 (§3) | Adjacent finger damping (underside 1.0, tip 0.5) as Chuck; bitmask; lift on source note-off; chords don't self-mute | (branch) `applyAdjacentMute`, `liftMutesFrom` | n/a | (branch) `SI06_SI07_neighbourMute` | OWNED |
| SI-5 (§3) | Fretting style scales it (rock 1.0 / classical 0.1) from `MuteSettings::frettingStyle` — owner proxies via rh_style until techniques' mute_fretting_style merges | (branch) proxy on `rh_style` | (branch) text-only in STRING INTERACTION | (branch) `SI06_SI07_neighbourMute` | PARTIAL |
| SI-6 (§4) | Release stagger: 3 ms groups, bias/rank, seeded, never earlier, overflow fires now | (branch) `LuthierEngine::stageNoteOffs`, `ScheduledEvent::staggered` | n/a | (branch) `SI08_SI09_releaseStagger` | OWNED |
| SI-7 (§5) | Pickup crosstalk Gaussian aperture, bend lateral offset; g_s = 1 unbent | (branch) `PickupEngine::setStringLateralOffsets`, `updateCrosstalk` | n/a | (branch) `SI10_crosstalk` | OWNED |
| SI-8 (§6) | Muted-string thump in live and pattern strums; deadStrike skips activity/MIDI note/MIDI out | (branch) `Rhythm/MutedThump.h`, `MidiInterpreter::flushChordGroup`, `RhythmEngine::scheduleStrum` | n/a | (branch) `SI11_mutedStringThump` | OWNED |
| SI-9 (§7) | 7 params: coupling_air_amount, palm_mute_spread, adjacent_mute_amount, release_stagger_ms/_bias, pickup_aperture_scale, muted_thump_level; family rows | (branch) `Parameters.*`, `PhysicalRange.cpp` | (branch) STRING INTERACTION group | (branch) `SI12_SI14_rangesAndPresets` | OWNED |
| SI-10 (§8) | No new MIDI; CC 67 drives spread | (branch) `TechniqueEngine::getPalmMuteAmount` | n/a | (branch) `SI04_SI05` | OWNED |
| SI-11 (§9) | CHARACTER STRING INTERACTION group | (branch) n/a | (branch) `UI/StringInteractionGroup.*` in `CharacterPanel` | (branch) `RealismBUi.theCharacterTabCarriesTheThreeGroups` | OWNED |
| SI-12 (§9) | Mute-zone shading shows palm width and weights | (branch) n/a | (branch) fretboard palm band (`FretboardRealismB`) | (branch) `RealismBUi.theFretboardDrawsTheTouch` | OWNED |
| SI-13 (§9) | Palm width mirrored in TECHNIQUES > MUTE — both realism-b and techniques omit it | none | none | - | MISSING |
| SI-14 (§9) | gui-integration §19 rows (primary CHARACTER > STRING INTERACTION) — not added | n/a | n/a (doc) | - | MISSING |
| SI-15 (§9) | Runtime flags cleared by reset, preset load and panic | (branch) `resetRealismB` | n/a | (branch) code review only | OWNED |
| SI-16 (§10) | Budget 0.1 units (owner 0.2), no allocation | (branch) | n/a | (branch) `SI13_realtime` | OWNED |
| SI-17 (§11 SI-01) | Air magnitudes | (branch) | n/a | (branch) `SI01_airMagnitudes` | OWNED |
| SI-18 (§11 SI-02/03) | Air off bit-identical; air delay | (branch) | n/a | (branch) `SI02_SI03_airOffIsBitIdenticalAndDelayed` | OWNED |
| SI-19 (§11 SI-04/05) | Palm coverage; lift restores | (branch) | n/a | (branch) `SI04_SI05_palmSpreadCoversAndLifts` | OWNED |
| SI-20 (§11 SI-06/07) | Neighbour mute rock / classical / chords | (branch) | n/a | (branch) `SI06_SI07_neighbourMute` | OWNED |
| SI-21 (§11 SI-08/09) | Stagger spread and order | (branch) | n/a | (branch) `SI08_SI09_releaseStagger` | OWNED |
| SI-22 (§11 SI-10) | Crosstalk honesty | (branch) | n/a | (branch) `SI10_crosstalk` | OWNED |
| SI-23 (§11 SI-11) | Thump | (branch) | n/a | (branch) `SI11_mutedStringThump` | OWNED |
| SI-24 (§11 SI-12) | Per-string sum -80 dBFS with every feature at default — owner relies on existing RoutingTests, no assertion at the new defaults | `Routing` per-string test (pre-existing) | n/a | `Routing` suite (not at SI defaults) | PARTIAL |
| SI-25 (§11 SI-13) | Realtime, 0.1 units (relaxed to 0.2) | (branch) | n/a | (branch) `SI13_realtime` | OWNED |
| SI-26 (§11 SI-14) | Ranges and preset round trip | (branch) | n/a | (branch) `SI12_SI14_rangesAndPresets` | OWNED |
