All rows are OWNED by the output-normalization FEAT session; no Phase-2 fixes. Pre-existing code the owner must integrate with:

- NOTE `Source/DSP/Master/MasterBus.h:19 MasterBus` — per-sample `gainSmooth`, sample-peak limiter at -0.3 dBFS (`setLimiterEnabled` :28, `limiterEnv` :66), short-term `getLufs` :40 (keep unchanged; the new `Bs1770Meter` is separate).
- NOTE ON-02 needs golden hashes of today's output generated *before* any normalization code lands (`Tests/Golden/NormalizationOffHashes.json`); REALISM/TECHNIQUES merges will change the audio, so the goldens must be regenerated from the merge base the feature lands on.
- NOTE `Source/Live/Snapshots.h:211 SnapshotBank::applyBlend`, `Source/Presets/PresetMorph.*` — wrap their writes in the new `PerformanceWriteScope`; `Source/Parameters.h:560 lastWrite` stamps have no source field yet.
- NOTE `Source/PluginProcessor.h:389 recallSlot`, `:423-424 undo/redo`, `:453 captureStateBlock`, `:456 createOfflineInstance` — restore-scope refactor and worker-safe capture.
- NOTE `Source/UI/OptionsPages.h` `AudioPage` (`LuthierChoice oversampling`) — host of the new group; `Source/UI/Notifications.h` banners; `HeaderBar` meters region; `EasyPanel` `LevelMeter` column.
- NOTE Depends on preset-browser-previews (shared `PreviewRenderService` calibration lane, `Bs1770Meter`, `truePeakDbtp` sidecar) and on editions (`Edition.h`, absent here); spec allows a standalone offline instance until the service exists.
