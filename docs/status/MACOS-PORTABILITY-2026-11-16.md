# macOS release-blocker repairs — 2026-11-16

This continues the recorder, chorus and MIDI checkpoints. It does not claim
native Windows/macOS release qualification or publish installers.

## Observed native failures

GitHub Actions run `43843489246`, source `7560468b`, failed both macOS editions:

- `HostState.aSessionSurvivesThePrepareThatFollowsIt`: exact persisted board
  state changed at the reamp gain property.
- `RefPedalControlRace.saveExtraExceptionReacquiresGuardAndAllowsRetry`:
  a one-shot fixture callback threw again on retry.
- Segmentation fault entering the asset-store drain test, after
  `RefPedalState.concurrentSaveIsCompleteGeneration`.
- Free: both tiny-convolution tests selected the same body IR for their two
  presets, violating the intended reload fixture.
- Pro: the full-suite CQ-23 repaint measurement saw two paints for one static
  change, although the earlier focused execution passed.

## Repairs and local evidence

**Queued asset lifetime.** Queued jobs captured the outer `AssetStore`, then
read its `unique_ptr<Impl>` during `Impl` destruction. libc++ clears that pointer
before invoking the destructor; the worker drains queued jobs while the pointer
is null. Jobs now capture the immutable directory and use a free write helper.
The atomic same-directory write behavior and worker drain contract are retained.

The original implementation reproduced a null-pointer access and ASan crash
using Clang 19/libc++ 19 with real JUCE 8.0.10. The fix passes two tests / 19
checks: repeated queued destruction with 65 required completions per round,
and a throwing callback followed by successful work.

**Exact host capture.** Fused multiply/add changes the rounding of normalized
host gains on ARM/FMA targets. A value intended as 0 dB became a tiny nonzero
gain and changed exact JSON state. Explicit binary32 intermediate products
retain the same rounding as the non-fused parameter conversion. No tolerance,
near-zero clamp or state-test relaxation is used. GCC with `-mfma
-ffp-contract=fast` reproduced nine failing zero-gain checks in the original
header. The fixed header passes two tests / 165 checks, including monotonicity
and exact endpoints across all ordinary extended controls.

**One-shot fixture.** A moved-from `std::function` need not be empty; libc++ can
retain a small callable. The test now uses `std::exchange` to make its callback
explicitly one-shot. All exception/retry assertions remain.

**Distinct IR fixture.** Both top woods are set through the real host parameter
path (alder then mahogany). The former alternate-only override depended on
which nearest IR the first preset selected. Assertions still require actual
different shipped files, increased load counters, finite/nonzero processing,
tiny callback sizes and allocator churn.

**Repaint measurement boundary.** Flush pending native paints after warm-up,
before taking state and paint counters. This keeps pre-measurement invalidation
out of the measurement interval. No paint budget is raised. This is a boundary
correction; the intermittent native CQ-23 failure still requires qualification.

The committed `scripts/portability_checks` harness passes four tests / 184
checks with Clang 19, libc++ 19, FMA enabled, ASan and UBSan, using real JUCE.
LeakSanitizer was disabled only for this traced local environment; the CI job
retains its default. Native processor/GUI tests remain required.

## CI and remaining gates

Quick-check adds a real-JUCE libc++/FMA sanitizer job. Both editions now run
the known state, lifetime, MIDI and tiny-buffer regressions early in native CI,
before the unchanged full suite. Release builds still require all selected
platforms, full tests, validators, LTO and packaging. The full local native
runner is still compiling at this checkpoint; exact-source native results and
remaining latency/IR audit work are not inferred from the isolated checks.


The real-JUCE libc++/FMA portability CI job and recorder ThreadSanitizer job
both passed on commit `ec38f6ac` (quick-check run `43894765167`). The later local
headless processor selection also passed all 59 tests / 19,314 checks across
state, control, asset lifetime, MIDI and backend release paths. These Linux
results do not establish native macOS GUI or package acceptance.

## Exact-source follow-up

PR verification job `133630891947` on
`53632189ed05a56b2bba35012f883b2b522a074a` exposed two additional fixture and
tooling issues before release qualification:

- `RefPedalControlRace.publisherReclaimerAndAudioHaveDistinctThreads` could let
  the control loop finish before macOS scheduled the audio worker. The fixture
  now uses a three-worker readiness barrier after each worker performs its first
  real audio, reclaim or snapshot operation. Allocation, lock, finite-sample and
  retired-plan assertions are unchanged.
- CLAP Validator 0.3.2 hit its known temporary-state-file failure and obsolete
  reproducibility checks. All platform scripts now use 0.4.1 build
  `0.4.1-127-g152b982`, whose current validation set includes explicit binary
  state reproducibility and additional audio, parameter, fuzz and state checks.
  CLAP validation is not skipped or relaxed.

The changed race fixture and all partitioned UI sources pass local C++17 syntax
compilation; the release guards and shell syntax also pass. Native macOS
execution, AU/VST3/CLAP validation and package qualification remain required.
