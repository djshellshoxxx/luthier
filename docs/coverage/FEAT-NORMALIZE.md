# FEAT-NORMALIZE: output normalization coverage

Spec: `spec/output-normalization.md` (test prefix ON-). Branch
`claude/luthier-feat-normalize`. No APVTS parameters are added (spec 7): the
setting is session state (`normalization` root key) plus `UiPreferences`.

With normalization **off** (the default) the audio path is bit-identical to the
build before this feature: `MasterBus::processBlock` runs its original loop
behind one block-level branch, and the change tracker does nothing per block
(ON-02, both halves).

## Coverage

| ID | Spec section | Implementation | Test | Status |
|---|---|---|---|---|
| Switch, target, gain rule | 2.1 | `Source/Support/OutputNormalization.{h,cpp}`, `LoudnessNormalizer::gainForMeasurement` | `GainRuleClampsAndRounds`, ON-07 | done |
| 300 ms linear-dB glide, 32-sample timeline segments | 2.2 | `Source/DSP/Master/LoudnessNormalizer.{h,cpp}` | `GlideIsTimelineAlignedAndBlockIndependent`, ON-11, ON-16 | done |
| Cached load rides the load | 2.2, 4.4 | `OutputNormalization::notifyConfigurationChanged (prefetch)`, `LoudnessNormalizer::applyLoadGain`, `PresetManager::onPresetLoaded` | ON-28 | done |
| Recalibration triggers, 250 ms debounce | 2.3 | `Source/Support/ConfigChangeTracker.{h,cpp}` | ON-13, ON-06 | done |
| Loudness roles, default Config | 3.1 | `Source/DSP/Master/LoudnessRoles.{h,cpp}` | ON-26 | done |
| Snapshots / morph are performance | 3.2 | `PerformanceWriteScope` in `SnapshotBank::applyBlend`, `PresetMorph::apply` | ON-14 | done |
| Preset-morph gain lerp | 3.2 | `OutputNormalization::updateMorph` | ON-15 | done |
| Configuration hash (SHA-256, canonical JSON, quantised) | 3.3 | `NormalizationCalibrator::hashSoundState / canonicalSoundState` | ON-26, ON-27 | done |
| MasterBus stage, block-level branch, gain ahead of DC/meter/limiter | 4.1 | `Source/DSP/Master/MasterBus.{h,cpp}` | ON-02, ON-11, ON-33 | done |
| True-peak safety, -1 dBTP, forced limiter | 4.1 | `Source/DSP/Master/TruePeakDetector.h`, `MasterBus::processBlockNormalized` | ON-09, ON-10, `TruePeakFindsInterSamplePeaks` | done |
| Worker, one atomic result word, serial check | 4.2 | `Source/Support/NormalizationCalibrator.{h,cpp}` | ON-12, ON-16, ON-25 | done |
| Reference render + phrase | 4.3 | `Source/Support/NormalizationPhrase.{h,cpp}`, `NormalizationCalibrator::renderAndMeasure` | ON-03, ON-30, ON-33 | done |
| BS.1770-4 meter | 4.3 | `Source/DSP/Master/Bs1770Meter.{h,cpp}` | `Bs1770MeterReadsReferenceTones` | done |
| Caches: LRU, factory table, disk | 4.4 | `NormalizationCalibrator` caches, `Resources/NormalizationFactory.json`, `Tools/RenderCli.cpp --calibrate-factory` | ON-27, ON-03 (drift gate) | done |
| Tune / setlist prefetch | 4.4 | `OutputNormalization::prefetchPresets`; `LuthierAudioProcessor::loadSetlist` queues every entry's preset | ON-31 | setlist done; tune deferred |
| Analytic estimate | 4.5 | `NormalizationCalibrator::estimateFor` | ON-29 | done |
| Offline determinism, the one sanctioned wait | 4.6 | `OutputNormalization::processBlockStart`, `AudioExporter` (`setNonRealtime`) | ON-16, ON-17, ON-29 | done |
| Options -> AUDIO group | 5.1 | `Source/UI/NormalizationOptions.{h,cpp}`, `AudioPage` | ON-34 | done |
| Header / Easy badge | 5.1 | `Source/UI/NormalizationBadge.{h,cpp}`, `HeaderBar`, `EasyPanel`, `PluginEditor::openNormalizationOptions` | ON-35 | done |
| Readout states | 5.2 | `OutputNormalization::readoutText / badgeText` | ON-07, ON-08, ON-15, ON-29, ON-34 | done |
| Banner `normalization.on`, [Options] [Don't show again] | 5.3 | `NormalizationUi::postEnabledBanner`, `Notification::secondaryAction` | ON-34, ON-36 | done |
| ROUTING caption, Workshop note, Diagnostics, debug overlay | 5.4 | `NormalizationCaption`, `RoutingPanel`, `WorkshopPanel`, `DiagnosticsPage`, `DebugPanel` | `CaptionsAndDiagnostics` | done |
| Locale keys | 5.4 | `Source/Accessibility/Localisation.cpp` (`options.audio.normalization.*`, `banner.normalization.*`, `badge.normalization.*`) | ON-34, ON-36 | done |
| Session state, snap on restore, verify | 6 | `OutputNormalization::toVar / restoreFromSession`, `LuthierAudioProcessor::getStateInformation` | ON-18 | done |
| UiPreferences defaults; legacy sessions load off | 6 | `NormalizationOptions.cpp` defaults registrar | ON-19 | done |
| Not preset data | 6 | outside `presets.toVar()` | ON-20 | done |
| Undo / redo / A-B keep it (`RestoreScope::soundOnly`) | 6, 8 | `LuthierAudioProcessor::restoreState` | ON-21 | done |
| No parameters | 7 | - | ON-26 (count printed) | done |
| Accessibility | 9 | `NormalizationOptionsGroup`, `NormalizationBadge`, `toggleNormalization` shortcut (unbound) | ON-36 | done |
| Aux / per-string untouched, Aux 7 follows main | 10 | engine structure (master bus is main only) | ON-22 | done |
| Header meter shows normalized level | 10 | unchanged `MasterBus` meter | ON-23 | done |
| Preview player offset | 10 | `OutputNormalization::getPreviewGainOffsetDb` | ON-24 | done (API; player is FEAT-BROWSER's) |
| Combinations | 10 | - | ON-31 | done |
| Telemetry usage boolean | 10 | `Telemetry::record (usage, "normalization_enabled")` on a user toggle | (queued like every usage event) | done |
| Editions | 11 | edition string in every hash and in the factory table | ON-32 | partial (see deferred) |
| Failure modes | 12 | estimate, ErrorLog `CALIBRATION_FAILED`, failure banner | ON-29 | done |
| Performance budget | 13 | SIMD true-peak FIR, exact FIR gating, steady-state fast path | ON-33 (active 0.17 units, inactive = bypassed) | done |
| Golden off-path hashes | 15 ON-02 | `Source/Tests/Golden/NormalizationOffHashes.json`, `scripts/regen_normalization_hashes.sh` | ON-02 | done |
| Dynamics, input independence, clamping, unmeasurable | 15 | - | ON-05, ON-06, ON-07, ON-08 | done |

Tests: `Source/Tests/NormalizationTests.cpp` (engine), `NormalizationGuiTests.cpp`
(xvfb), `NormalizationGoldenTests.cpp` (ON-02 golden). Run
`xvfb-run -a build/LuthierTests_artefacts/Release/LuthierTests Normalization`.
`LUTHIER_SLOW_TESTS=1` runs the full 36 x 25 grids (ON-02, ON-03, ON-09 and the
other sweeps); the default run uses a fixed, seeded sample of each.

## Regenerating the reference data

- `scripts/regen_normalization_hashes.sh` rewrites the ON-02 golden hashes.
  Run it after a merge that legitimately changes audio. Hashes are only
  checked on the toolchain that wrote them (recorded in the file); the
  same-build A/B test holds everywhere.
- `scripts/regen_normalization_factory.sh` rewrites
  `Resources/NormalizationFactory.json` (about 20 minutes). Run it when the
  factory presets or the engine's level change; ON-03's drift gate and ON-27
  fail until it is.

## How a new parameter declares its loudness role

Nothing to do for a sound-shaping parameter: every id `LoudnessRoles` does not
name is **Config**. A playing gesture (a volume knob, a technique strength) goes
in `kPerformanceIds` / `kPerformancePrefixes` in
`Source/DSP/Master/LoudnessRoles.cpp`; a mix or output trim goes in `kMixIds`.
The hash only includes Config values that differ from their default, so a
parameter appended at its default leaves every existing hash (and the factory
table) valid.

## Decisions

1. **Change tracker in its own class** (`ConfigChangeTracker`, owned by the
   processor) rather than inside `ParameterBridge` (4.1): same behaviour, and the
   bridge (a file every workstream edits) is untouched. It listens to every
   parameter itself to tag performance writes.
2. **Role default is Config** (coordinator instruction), not a test failure;
   ON-26 checks conflicts, the spec-named roles and the hash properties instead.
3. **Effect-slot "performance controls"**: wah / volume-pedal position are the
   MIDI expression input in this build, not parameters, so no slot parameter is
   Performance. `LoudnessRoles::isPerformanceSlotParameter` is the hook.
4. **Rate families**: this engine is ~3 dB louder at 96 kHz than at 48 kHz, so
   4.3's rate-invariance assumption does not hold. 44.1/48 kHz share the 48 kHz
   calibration (and the factory table); 88.2/96 kHz and 176.4/192 kHz render at
   96 / 192 kHz and hash separately. ON-30 checks 44.1/48 share a hash and all
   three land on target. (Engine rate dependence reported, not fixed here.)
5. **Fresh render instance per calibration** instead of a reused or borrowed
   one: a reused instance carries noise-generator state, which would make the
   measurement depend on history. `PreviewRenderService` does not exist yet.
6. **Limiter attack on the normalization path**: a sliding minimum plus a box
   average over the lookahead replaces the exponential attack (release, lookahead
   and backstop unchanged). The exponential attack let fast transients through
   by 0.4 dB, so -1 dBTP could not be held (ON-09). The off path keeps today's
   limiter exactly.
7. **Exact true-peak gating**: the FIR runs only while a sample in its window
   exceeds ceiling / (FIR L1 norm); below that no interpolated point can reach
   the ceiling. Keeps the active path at 0.17 units (budget 0.22).
8. **Offline preset loads snap** (cached or not) at the load's block: the
   realtime prefetch shortcut would otherwise make a cold-cache render differ
   from a warm one (ON-16).
9. **Derived per-string detune** (`fineTuneCents`, `realismDetuneCents`, random
   draws the engine derives from `string_age` / realism parameters) is left out
   of the hash; the parameters that cause it are hashed. Otherwise the same
   preset loaded twice hashed differently.
10. **Header meter**: `MasterBus`'s short-term meter averages its two channels
   where BS.1770 sums them, so it reads 3.01 dB under true LUFS for centred
   stereo. The spec keeps that meter unchanged; ON-23 checks it on its own scale.
11. **Aux 7** is now the live-performance monitor mix, built from the post-master
   main when active: ON-22 checks it rises with the main by the normalization
   gain, and every other aux and per-string output is bit-identical.
12. **Disk cache** lives in `~/Documents/Luthier/Cache/normalization`, the folder
   Diagnostics "Reset all settings and clear caches" already deletes; that reset
   also turns the preference off (10).
13. **Measuring text** uses "..." (ASCII) rather than an ellipsis character: the
   compiled-in catalog is ASCII.
14. **Morph**: during a morph between presets, calibrations triggered by the
   morph's own structural preset loads are suppressed and the lerp rules.
15. **ON-16's session** changes the guitar type on its own (not straight
   after a preset load) and pauses 300 ms of wall time before each event:
   `ParameterBridge::writtenSinceGuitarType` keeps or replaces a guitar's
   parameters by a 250 ms wall-clock window, so a preset load immediately
   followed by a type change produced a different sound depending on how fast
   the blocks rendered (engine behaviour, reported here, not changed).
16. **Build fixes** outside the spec, needed after the merge: the headless
   renderer excluded `PluginEditorOnboarding.cpp` and now compiles
   `UiPreferences.cpp` (PresetManager reads it).

## Deferred

- **ON-32 Free edition**: there is no Edition.h and no Free CI configuration in
  this build. The edition string is part of every hash and of the factory table,
  so a Free build gets its own table when editions land.
- **ON-31 tune playback across sections**: `OutputNormalization::prefetchPresets`
  exists and is tested, and a setlist load uses it; calling it from the tune
  load path waits on TuneSession exposing the presets a tune's sections use.
- **Preview player gain** (10): `getPreviewGainOffsetDb` is ready for
  `PreviewPlayer` (FEAT-BROWSER), which does not exist on this branch.
- **Full-grid sweeps** run only with `LUTHIER_SLOW_TESTS=1` (about 35 min for
  ON-02 alone); the default run uses fixed seeded samples.
