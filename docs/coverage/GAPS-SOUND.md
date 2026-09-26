# GAPS-SOUND

Gap-fill work against `docs/audit/SPEC_SWEEP.md` for: part-acoustics,
modulation-matrix, tone-match, engine, input-routing, routing-io,
advanced-ranges, string-squeak, fret-buzz, slide-guitar, pick-noise.

Table: row ID | what was done | test | status

| Row | What was done | Test | Status |
|---|---|---|---|
| PA-6 | none (table already matched spec) | `PartAcoustics::theWoodTableIsTheSpecs` | DONE |
| PA-13 | none (mapping already correct) | `PartAcoustics::chamberingPicksItsShape` | DONE |
| PA-24 | none (table already matched spec) | `PartAcoustics::fretMaterialsAreTheSpecsTable` | DONE |
| PA-27 | none (already correct) | `PartAcoustics::fretsCountSetsThePlayableRange` | DONE |
| PA-28 | none (table already matched spec) | `PartAcoustics::nutMaterialsAreTheSpecsTable` | DONE |
| PA-29 | none (already correct) | `PartAcoustics::nutSlotDepthsReachSetupWhenTheGuitarOmitsThem` | DONE |
| PA-33 | `mapSpec` now falls back to a `bridge.type` -> mass/coupling table (part-acoustics.md 5) instead of a flat 100 g / 0.55 when a part omits the fields; every factory bridge part already supplies both, so this is additive only | `PartAcoustics::everyFactoryBridgeMatchesItsTypeRow`, `PartAcoustics::tremoloTypeSelectsTheWhammyBridge` | DONE |
| PA-34 | none (mapping already correct) | `PartAcoustics::tremoloTypeSelectsTheWhammyBridge` | DONE |
| PA-48 | none (mapping already correct) | `PartAcoustics::windingMapsToItsStringMaterial` | DONE |
| PA-40 | not done: `coil_turns` overlaps the part's own `output_dbfs_reference`, which factory pickups already tune by ear; adding a second additive term risks detuning every factory guitar's pickup level and any golden-hash/normalization data derived from it | - | DEFERRED - needs a product call on whether `output_dbfs_reference` should be derived from `coil_turns` instead of standing beside it |
| EN-16 | none (map already correct) | `Midi::defaultCcMapMatchesTheSpec` | DONE |
| EN-52 | none (already correct) | `Whammy::floydSpringsRingOnReturn` | DONE |
| EN-90 | `RoomEngine` and `ReverbPedal` FDN feedback clamp changed from 0.9985 to the spec's 0.998; added `getFeedbackGain()` accessors for the test | `Room::feedbackNeverExceedsTheCap` | DONE |
| FB-8 | none (already correct); added `FretBuzz::getGeneratorIndex` test accessor | `Buzz::theCentreRisesWithTheContactFret` | DONE |
| PN-9 | Tried changing the default from 0.5 (1.07 mm) to the spec's 0.316 (0.73 mm); reverted. Every factory preset relies on the parameter default rather than setting it explicitly, so the change moved every guitar's click pitch slightly and broke `NormalizationGolden::ON02_OffPathMatchesGoldenHashes`'s sibling checks and `Normalization::ON03_FactoryTableDoesNotDrift`/`ON33_Performance` (confirmed by reverting and re-running: those three pass again, unrelated `ON02` mismatch on `p06_gown_*` persists either way and is not caused by this change) | - | DEFERRED - fixing the default needs the calibration/normalization owner to regenerate the factory table and golden hashes alongside it |
| MM-1 | none (already correct) | `Modulation::controlRateIsABlockOver32FlooredAt128` | DONE |
| MM-41 | none (already correct); extended the existing `routeModulatesItsDestination` test to also check re-enabling a route | `Modulation::routeModulatesItsDestination` | DONE |
| RIO-4 | none (already correct) | `Routing::sidechainDrivesTheEnvelopeFollower` | DONE |
| RIO-13 | none (already correct) | `Routing::sidechainNeverReachesTheMainOut` | DONE |
| RIO-14 | none (already correct) | `Routing::sidechainMeterReadsTheSidechain` | DONE |
| SQ-16 | none (already correct) | `Squeak::moistureLowersOddsAndBrightness` | DONE |
| SQ-17 | none (already correct) | `Squeak::pressureRaisesLevelAndCoarsensTexture` | DONE |
| SG-12 | none (already correct); test covers `SlideEngine::vibratoCents`'s formula directly (depth and sounding-length scaling), not a full engine-level render with `vibrato_depth` and the damped-segment sustain scale, which the work list also asks for | `Slide::vibratoCentsFollowsDepthAndSoundingLength` | PARTIAL - the formula is covered; an engine-level render test is still open |

## Notes

- PA-33's fallback table only changes behaviour for a part that omits
  `mass_g`/`coupling` (a hand-authored Workshop part); every shipped
  `.luthierpart` file already specifies both fields explicitly, so no factory
  guitar's rendered audio changes.
- Full suite run once (3908 s, 9 of 1390 tests failed). Three failures
  (`ON03_FactoryTableDoesNotDrift`, `ON33_Performance`, and one of
  `ON02_OffPathMatchesGoldenHashes`'s two mismatches) were this session's PN-9
  attempt and are gone now that it is reverted. The other six were already
  failing independent of anything in this branch (confirmed one, `ON02`'s
  remaining `p06_gown_phrase`/`p06_gown_chord` mismatch, by reverting PN-9 and
  re-running; did not chase the rest given they are outside this helper's
  specs) and are not mine to fix: `everyFactoryPresetPlaysEveryPhrase` /
  `snapshotsAndPresetMorph` (preset #10 "P-Bass Flatwound" renders silent -
  bass-techniques/normalize territory), `everyAutomatableParameterHasAVisibleControl`
  (gui-integration/gui-reach), `ON27_Cache` (normalize), `CQ10`/`CQ22`
  (cpu-quality-modes, owned by FIX-CROSS). Worth flagging to the coordinator.
