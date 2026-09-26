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

## Notes

- PA-33's fallback table only changes behaviour for a part that omits
  `mass_g`/`coupling` (a hand-authored Workshop part); every shipped
  `.luthierpart` file already specifies both fields explicitly, so no factory
  guitar's rendered audio changes.
- Full suite not yet re-run to completion this session (see handoff process);
  targeted suites (`PartAcoustics`, `Room`, `Whammy`, `Midi`, `Effects`,
  `Controllers`) pass clean after each change.
