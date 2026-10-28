# Six-pedal completion — coordinator source map and evidence table (2026-10-27)

Coordinator: Claude session on branch `claude/luthier-pedalboard-integration`.
This file is the single live map for this continuation; it does not replace the
acceptance ledger (`docs/coverage/PEDALBOARD-ACCEPTANCE.md`) or any original gate.

## Baseline reconciliation

| Item | Finding |
|---|---|
| Uploaded checkpoint | `luthier-lookahead-traced-1e7aaf1c-20261027.zip`; MANIFEST head `1e7aaf1c939da793ca4d3d3f37a6c627a42aa539` over base `f1efeded`; bundle verified with `git bundle verify`; nested predecessors b54fc7c4, b85d98b2, 1f7bfeed, da984fa2 all ancestors of 1e7aaf1c. Evidence copied unmodified to the coordinator workspace. |
| Remote | 76 checkpoint commits were unpublished; pushed unmodified as `claude/luthier-pedalboard-checkpoint-1e7aaf1c`. `codex/luthier-pedalboard-completion` (PR80, b5bec374) and `codex/luthier-checkpoint-20261026-qa2` (f1efeded) are ancestors. Worker-lane branches (`codex/luthier-pedalboard-{dsp,timefx,state,...}`) carry older lane history that was selectively integrated; nothing newer than 1e7aaf1c exists remotely. |
| Private Library checkpoint v29 (1f7bfeed) | ancestor of 1e7aaf1c; not needed. Failed native packet writes were not retried. |
| Master | `0f62c3d` (unchanged; no direct pushes). |
| Scope | Phase 90, CE-2, DD-3T, CE-2W, Dyna Comp, RC-5. Twelve deferred models retained unmodified. |

## Baseline measurement on this executor (1e7aaf1c, Linux x64, clang 18.1.3, Release, LTO off)

Machine: Intel Xeon @2.1 GHz, 4 cores, no SMT, 15 GB. Different from the previous
executor; earlier CPU numbers are not comparable and not reused as qualification.

Focused six-model suites (`LuthierTests RefPedalPhase90 RefPedalCe2 RefPedalCe2w RefPedalDd3t
RefPedalDyna RefPedalBossRc5 RefPedalRc5 RefPedalChorusMemory`, name-fragment match ran 101 tests):
**1 of 101 tests failed (5 of 172,187 checks), 53.8 s** — `RefPedalCe2.bbdOnOriginalThdGate`
(BBD-on THD 0.0003–0.0097% vs 0.5–3% at all five rates). Log kept outside Git.

Gates that are *not* committed tests (recorded only as probes) remain open until
encoded and passing: Phase90 CPU < 0.4%, Phase90 24-bit click criterion, DD-3T CPU < 0.5%,
CE-2 CPU < 0.4% and raw bypass jump, CE-2W CE-1 THD 1–3%, Dyna THD/attack/CPU,
RC-5 wrapper (skeleton), paged-looper grind readiness (exit 1, 81 observations).

## Ownership (one writer per module)

| Owner | Branch | Writable files |
|---|---|---|
| coordinator | `claude/luthier-pedalboard-integration` | Register*.cpp, PedalBase/IPedal/PedalDesc/Registry, BoardEngine, Parameters, PluginProcessor, CMake, CI/release scripts, docs/coverage, docs/handoff |
| phase90 | `claude/luthier-pb-phase90` | Models/mxr_phase90_script.*, Phase90 tests/probes |
| dd3t | `claude/luthier-pb-dd3t` | Models/boss_dd3t.*, DD-3T tests/probes |
| chorus | `claude/luthier-pb-chorus` | Models/boss_ce2*.*, Common/BbdCore.*, CE-2/CE-2W tests/probes |
| dyna | `claude/luthier-pb-dyna` | Models/mxr_dynacomp.*, Dyna tests/probes |
| rc5 | `claude/luthier-pb-rc5` | Models/boss_rc5.*, rc5_rhythm.*, rc5_storage.*, Common/Rc5*.h/.cpp, RC-5 tests |
| looper | `claude/luthier-pb-looper-core` | Common/{PagedLooperCore,LooperCore,LooperStorage,LooperStateCodec}.*, looper tests, grind probe |

Model routing: only one model is available to this session's agents (no economical/
stronger tier selection was possible); this is reported rather than simulated.

## Release order (owner)
working pedalboard → light stress/QA → Windows standalone → macOS Intel + Apple Silicon →
VST3 → CLAP → Linux. Release workflow and names: `docs/RELEASING.md`.

## CI readability change (coordinator)
`pr-verification.yml` gains a `digest` job that posts the uploaded native logs' failures
as a PR comment (`scripts/ci_failure_digest.py`); `ci_build.ps1` now tees the Windows
build output into `logs/build.log`. Neither changes a test, threshold or validator.

## Evidence table (updated as slices land)

| Gate | Source SHA | Command | Result |
|---|---|---|---|
| Six-model focused suites, Linux Pro | 1e7aaf1c | see above | 100/101 pass; CE-2 BBD THD RED |
| Windows MSVC C2653/C2065 (Phase90 test fixture) | 97d3b013 | build.yml windows pro+free, run 40617409778 | pending at time of writing |
| CE-2 BBD-on THD (spec §9) + new −3 dBFS BBD-on test; full RefPedal 514 tests/12,368,320 checks pass | slice-2 commit (PR #83) | `LuthierTests RefPedalPhase90 RefPedalCe2 RefPedalCe2w RefPedalDd3t RefPedalDyna RefPedalBossRc5 RefPedalRc5 RefPedalChorusMemory RefPedalBbdCore` | **105/105 pass, 172,227 checks, 64.9 s, Linux Pro clang 18** |

2026-10-28 continuation and lane reconciliation: [PEDALBOARD-2026-10-28-CONTINUATION.md](PEDALBOARD-2026-10-28-CONTINUATION.md).
