# Beta known issues (v1.0.0-beta1)

Logged, not fixed, from the final pre-beta audits (QA, feature/wiring audit,
optimisation scout). Critical findings from the same passes were fixed before
the tag; everything here is minor or needs an owner decision.

## Needs an owner decision

- Support, homepage and source links are still `.example` placeholders
  (`Source/Support/SupportLinks.h`, shown by `HelpTab`). Pass the real URLs
  through the `LUTHIER_*_URL` CMake defines before a public release. The
  support email is deliberately deferred (owner, 2026-10-07).
- `tune_feel_mod` and `tune_tempo_drift` have no dedicated control; they are
  reachable only as MOD-matrix destinations (`ParameterVisibility.h`). Decide
  whether they get a knob or stay matrix-only, then document it.
- Plugin latency is 1196 samples at 48 kHz (body + cabinet convolution
  partitions of 128, oversampling filters, 1.5 ms limiter lookahead, 2 ms chord
  window in Poly). A 64-sample partition would cut ~256 samples at roughly
  twice the convolution CPU; product decision.

## Minor (cosmetic, docs, environment)

- "Wah Funk Rhythm" renders 20 dB quieter than any other factory preset
  (RMS -64 to -73 dBFS over the audition phrases; peak -26 dBFS). Not silent,
  not clipping; voicing of the envelope-filter pedal (`FactoryPresets.cpp`,
  EnvF last parameter 0, Comp mix 1). Re-voice when the preset bank is next
  reviewed.
- `LuthierRender --convert` skips `.html` and `.md` tab sources as
  "unsupported format" although the in-plugin importer reads both. CLI-only.
- `Accessibility.h:122` and `Rhythm/GenreKit.h:14` trigger `-Wcomment`
  ("/*" inside a block comment) in every translation unit that includes them.
- `rest_stroke` is exposed through two C++ symbols (`restStroke`,
  `bassRestStroke`) for one parameter ID; by design, cosmetic.
- Moisture tooltip in `NoiseGroups.cpp` lacks a final period.
- `docs/audit/REMAINING.md` IN-19: the Windows installer offers a post-install
  launch; the macOS dmg flow was not re-checked.
- GAPS row "Animated strings follow CPU relief" is closed: `QualityBadge`
  feeds the governor's live level and relief into
  `AnimationPolicy::setSource`, and `StringAnimator` follows the resulting
  strings style.
- CQ12's CPU-ordering check (High > Medium >= Low) is noisy on loaded shared
  machines; its loudness checks are deterministic and pass.

## Load-time and size notes (no change made)

- `prepareToPlay` is 235-250 ms warm, dominated by the looper's eight 30 s
  stereo layers being calloc'd (page faults inside glibc, not in the clear) and
  by one `ConvolutionMessageQueue` thread per `juce::dsp::Convolution`. True
  lazy allocation of looper layers (size on first arm, message thread) and one
  shared message queue for all convolutions are the remaining wins.
- Editor construction is 75-90 ms warm: first `PresetManager::refresh()` JSON
  parses all 70 factory presets (22 %), `AdvancedPanel` workspace build (13 %),
  `HelpTab` text shaping (4 %). Deferring non-default workspace tabs and the
  help text to first show would roughly halve it.
- Release binaries: VST3 18.4 MB, Standalone 18.6 MB stripped, with
  `-ffunction-sections -fdata-sections -fvisibility=hidden --gc-sections` and
  LTO on. `.text` is 14.3 MB, mostly UI. `-Os` for `Source/UI` only is the
  next candidate; needs a measured before/after. Resources (47 MB of IRs and
  backing FLACs) ship beside the binary, not inside it.
- `scripts/ci_build.sh` defaults `LUTHIER_LTO=OFF`; release builds pass
  `LUTHIER_LTO=ON` through `release.yml`, push/PR builds stay without LTO on
  purpose (compile cache).
