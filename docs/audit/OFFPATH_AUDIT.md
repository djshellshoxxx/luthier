# Off-path normalization audit (ON02 / ON03)

Lane: engine/controller fix (`claude/luthier-fix-engine`, branched from `codex/luthier-beta`).
Investigator note for the consolidation coordinator.

## Verdict: legitimate/pre-existing drift, NOT a techniques-merge regression. Coordinator must regenerate.

`ON02_OffPathMatchesGoldenHashes` (104 of 122 combos) and
`ON03_FactoryTableDoesNotDrift` (4 combos) fail because the committed golden
data is **stale**, not because the techniques / gaps-host / browser
consolidation changed the default signal path.

### Evidence

1. **The identical failure exists on the pre-techniques backup.** Building
   `origin/backup/pre-techniques-6f5b9cd` (which the golden and
   `NormalizationTestUtil.h` are byte-identical to at HEAD) and running ON02
   against the same committed golden fails on the **exact same 104 combos**
   (`diff` of the two failing sets is empty). If the recent merge had changed
   the off-path audio, the backup would pass. It does not.

2. **Neutering every recent-merge render-path change did not help.** On HEAD I
   independently reverted, one build each, all of:
   - the FEAT-BROWSER preview player mixing into the main bus (`PreviewPlayer::processBlock`),
   - the `ConvolutionInstaller` block-size floor change (16 -> 1),
   - the entire technique parameter-apply block (`ParameterBridge::applyToEngine`),
   - every `LuthierEngine` technique call site (`techniqueBeginBlock`,
     `techniqueStampEvents`, `techniqueStrike`, `techniqueNoteOff`,
     `techniqueBendCents`, `techniqueFret`, slide `controlledBarFret` /
     `getControlVibratoCents`).
   ON02 still failed the identical 104 combos every time. The techniques engines
   are correct pass-throughs when unarmed; they are not the cause.

3. **The golden predates a large body of audio-affecting work.** The golden
   (`Source/Tests/Golden/NormalizationOffHashes.json`, header `generated
   2026-09-26`, `toolchain clang-18-linux-x64`) was last committed at `8ad54d0`.
   `8ad54d0` is an **ancestor of the backup**. Between `8ad54d0` and the backup,
   audio-affecting features merged without the golden being regenerated,
   including FEAT-MIC mic placement (`Source/DSP/Amp/MicPlacement.*`, +800 lines),
   `Source/DSP/Body/AcousticMicModel.*` (new), FEAT-ASSIST auto-articulation
   (`Source/Model/Playing/AutoArticulator.*`, +1000 lines), `CabinetVoices`,
   and feat2-notation pick-stroke techniques (`MidiInterpreter.*`, +100 lines).
   Those legitimately changed the default/off-path voicing; the golden was never
   updated to match. It has been stale since well before this lane's work.

   (It likely went unnoticed because ON02 only runs when the build's toolchain
   fingerprint matches the golden's coarse `clang-18-linux-x64` string; a CI on
   any other clash silently skips it. This container matches, so it runs.)

### ON03 (factory table)

`ON03_FactoryTableDoesNotDrift` fails with **"is not in the factory table"**
(not "drifted") for the sampled combos, which land on high-index presets
(`p43`, `p48`, `p58`) — the newly added TECHNIQUES/browser factory presets that
were **never calibrated** into `Resources/NormalizationFactory.json` (revision 1,
regenerated pre-techniques). The pre-existing presets' calibration keys are also
shifted because a preset now legitimately serialises a `techniques` block
(engine-technique-layer.md 7), which `stripPresetIdentity` does not strip, so it
enters the calibration hash. Both are legitimate: the table must be regenerated
to cover the new content and the new sound-state field.

### Action for the coordinator (do NOT let this lane do it)

Regenerate both, as the final consolidation step, on the CI toolchain:

    scripts/regen_normalization_hashes.sh     # ON-02 golden (off-path)
    scripts/regen_normalization_factory.sh    # ON-03 factory calibration table

This lane deliberately did **not** touch `NormalizationOffHashes.json`,
`NormalizationFactory.json`, or any `Source/Support/Normalization*` code.

### Optional follow-up (not this lane; keeps future off-path hashes stable)

To stop an off-by-default `techniques` block from shifting existing presets'
calibration keys on every future change, the techniques/normalization lane could
make the block hash-neutral at its default (omit it from `PresetManager::toVar`
when `TechniqueLayer` is at defaults; `TechniqueLayer::fromVar` already treats a
missing block as "disarmed/default"). Left for the owning lane.
