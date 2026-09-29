## output-normalization.md

Output normalization is not implemented on this checkout: no `LoudnessNormalizer`, `TruePeakDetector`, `Bs1770Meter`, `LoudnessRoles`, `NormalizationCalibrator/Phrase`, `NormalizationBadge`, no `normalization` session key, no Options AUDIO group and no ON tests or golden hashes. The existing `MasterBus` (master gain, -0.3 dBFS sample-peak safety limiter, DC blocker, short-term LUFS estimate), `SnapshotBank::applyBlend`, `PresetMorph`, `createOfflineInstance`, `captureStateBlock` and the `undo/redo/recallSlot` restore paths are what it builds on. No FEAT branch pushed yet; all rows OWNED.

| Req | Summary | Engine location | GUI location | Test | Status |
|---|---|---|---|---|---|
| ON-0 (§0) | Ground rules: off by default and bit-identical (stage skipped), calibrated static gain not auto-gain, honest warning, -1 dBTP safety, RT rules, offline determinism, main output only — not built | - | - | - | OWNED |
| ON-2.1 (§2.1) | Switch (default off), target -14/-16/-18/-20/-23 (default -18), gain `clamp(target-measured,-12,+24)` ahead of `master_gain` — not built | - | - | - | OWNED |
| ON-2.2 (§2.2) | 300 ms linear-dB glide per 32-sample timeline-aligned segment; cached-load gain rides the 30 ms load crossfade — not built | - | n/a | - | OWNED |
| ON-2.3 (§2.3) | Recalibration triggers: discrete config events immediate, Config-role params 250 ms timeline debounce, nothing else, hold while measuring, stale-serial discard — not built | - | n/a | - | OWNED |
| ON-3.1 (§3.1) | `LoudnessRoles` table Config/Performance/Mix for every APVTS id (unlisted = test failure) — not built | - | n/a | - | OWNED |
| ON-3.2 (§3.2) | Snapshots/morph as performance via `ParameterBridge::PerformanceWriteScope` + `lastWrite` source tag; preset-morph gain lerp between endpoint calibrations — not built (`Live/Snapshots.h:211 applyBlend` has no scope) | - | n/a | - | OWNED |
| ON-3.3 (§3.3) | Config hash (quantised Config JSON, GuitarSpec, mod matrix, rack types, IR stat, edition, `kCalibrationRevision`) — not built | - | n/a | - | OWNED |
| ON-4.1 (§4.1) | `MasterBus::normalizer` (packed atomic result, `applyLoadGain`, `isActive` block-level branch), 4x true-peak detector in limiter with -1 dBTP ceiling glide forced on, bridge role vector + `ConfigChangeTracker` + `markConfigDirty` — not built | - | n/a | - | OWNED |
| ON-4.2 (§4.2) | `NormalizationCalibrator` worker thread, `captureSoundState()` worker-safe, publish result + status — not built | - | n/a | - | OWNED |
| ON-4.3 (§4.3) | Reference render on offline instance (borrowed calibration lane or own), `setCalibrationRenderMode`, setup, `NormalizationPhrase` guitar/bass, `Bs1770Meter` shared with previews, 1.5 s cost, 48 kHz — not built | - | n/a | - | OWNED |
| ON-4.4 (§4.4) | Caches: in-memory LRU 512, factory table `Resources/NormalizationFactory.json` via `luthier-render --calibrate-factory`, disk cache; prefetch on preset/tune/setlist load — not built | - | n/a | - | OWNED |
| ON-4.5 (§4.5) | Analytic estimate fallback — not built | - | n/a | - | OWNED |
| ON-4.6 (§4.6) | Offline determinism: non-realtime audio-thread wait for result (10 s cap), synchronous verify in `prepareToPlay`, `AudioExporter` sets non-realtime — not built | - | n/a | - | OWNED |
| ON-5.1 (§5.1) | Options AUDIO "OUTPUT NORMALIZATION" group under Oversampling (switch, target, readout, caption); header `NormalizationBadge` left of output meter + Easy meter column, click opens Options AUDIO — absent (`UI/OptionsPages.h` AudioPage has only `oversampling` etc.) | - | - | - | OWNED |
| ON-5.2 (§5.2) | Readout/badge state texts, 10 Hz refresh, stale muted — absent | - | - | - | OWNED |
| ON-5.3 (§5.3) | `normalization.on` info banner with [Options]/[Don't show again], suppress pref; caption always while on — absent | - | - | - | OWNED |
| ON-5.4 (§5.4) | ROUTING caption, Workshop bench note, Diagnostics path/State Inspector/debug overlay lines, catalog keys — absent | - | - | - | OWNED |
| ON-6 (§6) | Session key `normalization` (authoritative, snap on restore, background verify), `UiPreferences` defaults, legacy sessions load off, not preset data, `restoreState(soundOnly)` for undo/redo/A-B, `updateHostDisplay` on change — not built | - | n/a | - | OWNED |
| ON-7 (§7) | No parameters added — nothing to do yet | - | n/a | - | OWNED |
| ON-8 (§8) | Not undoable — n/a until built | - | n/a | - | OWNED |
| ON-9 (§9) | Accessibility (switch name + warning description, disabled target, rate-limited announcements, focusable badge, "Toggle output normalization" unbound command) — not built | - | - | - | OWNED |
| ON-10 (§10) | Interactions table (aux/per-string untouched, Aux 7 normalized, meters post-gain, looper/recorder record normalized, backing/click/jam after master, preview offset, exporter, workshop/tone-match off, telemetry boolean) — not built | - | - | - | OWNED |
| ON-11 (§11) | Both editions identical; per-edition factory tables — not built | - | n/a | - | OWNED |
| ON-12 (§12) | Failure modes table — not built | - | n/a | - | OWNED |
| ON-13 (§13) | Budget: MasterBus 0.22 units active, unchanged inactive; worker 1.5 s; memory; boot — not built | - | n/a | - | OWNED |
| ON-14 (§14) | New/changed file list — not built | - | n/a | - | OWNED |
| ON-T1 (§15 ON-01..10) | Default off, off bit-identical goldens (must be generated before code lands), ±1 LU on 936 combos, spread, dynamics, input independence, clamping, unmeasurable, true peak, limiter transparency — none | - | - | - | OWNED |
| ON-T2 (§15 ON-11..17) | Glides, debounce, performance writes, preset morph, offline determinism, RT safety — none | - | - | - | OWNED |
| ON-T3 (§15 ON-18..30) | Session round trip, legacy/pref, not preset data, undo/A-B, aux untouched, meter, previews, no recursion, roles/hash, cache, cached load, failure path, sample rates — none | - | - | - | OWNED |
| ON-T4 (§15 ON-31..36) | Combination, Free edition, performance, GUI options/badge, accessibility — none | - | - | - | OWNED |

<!-- counts DONE=0 NO-GUI=0 NO-TEST=0 PARTIAL=0 MISSING=0 OWNED=30 -->
