# Stress-test audit (branch I = ca14934e). Read-only; nothing run.

## 1. Inventory (existing stress/soak on I)
Test files are globbed (CMakeLists.txt:~353 `file(GLOB ... Source/Tests/*.cpp)`), so a new file needs no CMake edit. Registration: `LUTHIER_TEST(Suite, name)` Source/Tests/TestFramework.h:94 (CHECK :101, CHECK_MSG :109). Filter = name fragment: `LuthierTests <fragment>`.

| Test | File:line | Drives | Duration | Varies | Asserts | Run | CI |
|---|---|---|---|---|---|---|---|
| RefPedalSoak.concurrentEditsAndProcessingRemainFiniteAndAllocationFree | Source/Tests/RefPedalSoakTests.cpp:18 | reference::Board only (NOT LuthierAudioProcessor), 1 generic pedal initially, audio thread + edit thread | 30 s default; LUTHIER_SOAK=1 -> 1800 s; LUTHIER_SOAK_SECONDS clamp 30..7200 | block sizes {1,16,37,64,481,2048}; 100 edits/s: SetMix, Add, Remove, Move(reorder), SetEnabled(bypass), SetTrim, undo/redo; SR fixed 48000 | non-finite or |x|>16, allocations (allocationsOnThisThread), blocking locks (ThreadProbe), blocks>1000, edits>=80/s | `LuthierTests RefPedalSoak` | Yes: ci.yml:32 runs full LuthierTests (30 s); not nightly (nightly filter list excludes it) |
| RefPedalReferenceSoak.randomizedSixtySecondsAtEveryRate | RefPedalReferenceSoakTests.cpp:35 | single pedal objects (BossCe2, Ce2w, Dd3t, Phase90) | 60 s audio per rate, offline (fast); LUTHIER_PERF=1 -> 1800 s | SR {44100,48000,96000,192000}; sizes {1,4,16,64,512,2048}; random params/enable/foot | finite, |x|<=16 only | `LuthierTests RefPedalReferenceSoak` | Yes (full run) |
| Stress.* (midiStorm, midiLearn x100, undoRedo x1000, slideAndAdvancedRange, busLayoutChangesMidPlay, thirtyTwoInstancesRenderInTurn) | RobustnessTests.cpp:285-492 | full LuthierAudioProcessor, kBlock=256, 48 kHz | seconds each; 32 instances only with LUTHIER_PERF=1 | MIDI, ranges, bus layouts | finite, non-silent | `LuthierTests Stress` | ci.yml yes; nightly.yml:36 with LUTHIER_PERF=1 |
| StateModel.tenThousandRandomOperationsLeaveNoStuckState | RobustnessTests.cpp:163 | state model | short | random ops | no stuck state | filter `StateModel` | yes |
| Parameters.fuzzAcrossTenThousandStates | IntegrationTests.cpp:1011 | processor params | short (100k under PERF) | random params | finite | `Parameters` | yes |
| Fuzz.presetLoadAtTrulyExtremeBufferSizes | QA_Robustness.cpp:427 | full processor, every factory preset | PERF: SR {8k,48k,192k} x sizes {16..8192}; else 1 case | presets, SR, block; sizes 1-4 EXCLUDED (heap corruption finding, docs/handoff/BUFFER-SIZE-1-4-HEAP-CORRUPTION-2026-10-26.md) | finite/crash | `Fuzz` | yes |
| Memory.* (hour RSS) | PerfBudgetTests.cpp:~334-390 | processor | PERF long session | - | RSS growth <=350 MB, hourly growth | LUTHIER_PERF=1 `Memory` | nightly |
| RefPedalUi.tenThousandSeededOperations... | BoardUiTests.cpp:120 | board UI | - | UI ops | model/view coherence | `RefPedalUi` | yes |
| QA_RtSafetyAlloc / QA_RtSafetyEngine / CodexQA_RtSafety | Source/Tests | engine / processor | short | - | allocations on audio thread | `RtSafety` (UNVERIFIED exact suite names) | yes |

Other grep hits (docs/handoff/*.md, docs/coverage/*.md, Tools/riffs/*.riffdef, scripts/gen_ref_skeleton.sh) are prose/skeleton text, not runners.

No existing test combines: full processor + board pedals + preset switching + all block sizes + all 5 rates + timing + RSS + click detection + JSON. None produces machine-readable output; nightly.yml parses stdout text into a dashboard JSON. No overrun/RTF/p99 measurement anywhere in the soak tests.

## 2. SoundGrind chain
All three add only a Python wrapper over the existing LuthierTests binary: it runs the whole binary (or `--suite` name fragments) N rounds with env SOUNDGRIND_SEED=seed+round, and logs failed rounds to JSONL. Nothing in C++ reads SOUNDGRIND_SEED (grep Source Tools scripts: no hit) -> it is a repeat-runner, not randomized audio stress. It does not vary params, sizes or rates.
- PR #76 origin/codex/soundgrind-stress-audio-gui: +scripts/soundgrind_stress.py (73 lines). Truncates log; zero-test selection counted as pass.
- PR #84 origin/codex/luthier-soundgrind-repair-20261030: +101-line runner (exclusive log, `--list` preflight of filters, requires "ALL PASSED / N tests" summary), +scripts/test_soundgrind_stress.py (83), docs/handoff/SOUNDGRIND-2026-10-30-REPAIR.md, docs/handoff/LUTHIER-CONSOLIDATION-PROMPT-2026-10-30.md. Handoff says: not CI-run, not independently reviewed, Windows/macOS unverified.
- PR #86 origin/codex/luthier-soundgrind-ci-20261104: PR #84 + .github/workflows/soundgrind-runner-tests.yml (python unittest on ubuntu/windows/macos, paths-filtered to the soundgrind files, timeout 10 min) + scripts/test_soundgrind_ci_contract.py.
- Invocation: `python3 scripts/soundgrind_stress.py --runner build/LuthierTests_artefacts/Release/LuthierTests --rounds 30 --seed 75 [--suite X] [--xvfb] --log out.jsonl`.
- Run 41876094837 (workflow "SoundGrind runner regressions", head f6bf946f on PR #86 branch): get_job_logs failed_only returned "No failed jobs found". Jobs: ubuntu success, windows success, macos-latest conclusion=cancelled (started 17:17:13, completed 17:32:42, ~15.5 min, job 126735774174; its log download returns HTTP 404). So there is no error text; the only non-green cell is a cancelled macOS job (runner queue/cancel; cause UNVERIFIED). The workflow's own timeout is 10 min yet it ran ~15 min, suggesting queue wait or manual cancel.
- Merge into I: `git merge-tree --write-tree --name-only origin/claude/determined-ptolemy-mlbclp <branch>` printed only a tree OID for all three (no conflict lines): #76 3a900407, #84 ef5095ef, #86 66f42930 -> all clean (files are new). 
- Recommendation: do NOT merge #76 or #84 separately. #86 is a superset of #84 which supersedes #76; merging #86 alone is clean and low risk (only scripts/, docs/, one path-filtered workflow), but it delivers no processor stress coverage. Treat as optional repeat-runner tooling; it does not satisfy the required 30 s stress test. If merged, its two handoff docs are stale narrative (consolidation prompt) - consider dropping them. Note the workflow's PR trigger lists base branches [master, codex/...]; PRs into integration branches will not trigger it.

## 3. Gap analysis vs required 30 s test
| Requirement | Status on I |
|---|---|
| Full processor, several active pedals | GAP in soak (Board only, 1 pedal). Processor + board API exists (`getReferencePedals().model()`, used by RefPedalHostTests.cpp) |
| Rapid param automation | partial: Fuzz uses setValueNotifyingHost (QA_Robustness.cpp:~108); board SetParam cmd exists (BoardCommand.h:29) but soak never sends it |
| Bypass toggle | Board SetEnabled in RefPedalSoak only |
| Preset switching | only in Fuzz (processor level), not concurrent with audio |
| Pedal reorder | cmd::Move in RefPedalSoak |
| Block sizes {1,16,64,128,256,512,1024,2048,37/481} | soak has {1,16,37,64,481,2048}; missing 128,256,512,1024. Size 1-4 + preset cabinet/body change has a known heap-corruption history (handoff doc) - include 1 but ordering matters |
| Sample rates 44.1/48/88.2/96/192k | Board soak fixed 48k; per-pedal soak lacks 88.2k; processor none |
| Per-block timing max/p99/mean/overruns/RTF | MISSING everywhere |
| Audio-thread heap allocs | have counter (test binary only) |
| RSS growth | helper exists, Linux only |
| Non-finite / range | have |
| Click/discontinuity detector | MISSING (grep: none in soak tests) |
| Crash detection | process-level only (exit code) |
| JSON result + stable exit code | MISSING |

## 4. Recommendation and design
Place it as a NEW test suite file in Source/Tests (not RenderCli). Reasons: (a) the allocation counter (`operator new` override, CircuitTests.cpp:77-110, `luthier::tests::allocationsOnThisThread()` :51, enabled by LUTHIER_ALLOCATION_COUNTER=1 in LUTHIER_TEST_DEFS CMakeLists.txt:~349) exists only in LuthierTests; LuthierRender has `LUTHIER_CONSOLE_DEFS` only and links no test sources; (b) CI already builds and runs LuthierTests; (c) RenderCli's RenderHost (Tools/RenderCli.cpp:43) is a mock-ish host (engine + APVTS, no LuthierAudioProcessor), which violates "full processor, not a mock". Optionally add a thin `--stress-json` driver later; not needed.

### Files
- NEW Source/Tests/ProcessorStressTests.cpp: `LUTHIER_TEST (ProcessorStress, thirtySecondFullProcessorMatrix)` (30 s default, env LUTHIER_STRESS_SECONDS override, clamp 30..1800) plus `ProcessorStress, selfTestDetectsInjectedFaults` (unit test of the click detector/percentile/exit code logic, fast).
- Header-in-file helpers (anonymous namespace): `struct BlockTimer`, `struct ClickDetector`, `struct StressReport`, `writeReportJson()`.
- Output: path from env LUTHIER_STRESS_JSON (default `stress-report.json` in cwd); stdout one-line summary. Exit code: LuthierTests exit code (nonzero on any CHECK fail; process crash = signal exit). Stable wrapper: `LuthierTests ProcessorStress` ; CI step to upload the JSON (add to nightly.yml; and optionally ci.yml filter step).
- Reuse (no duplication): TestRegistry/LUTHIER_TEST (TestFramework.h:94); allocation count `luthier::tests::allocationsOnThisThread()` (declare as RefPedalSoakTests.cpp:14 does); `luthier::ThreadProbe::markAsAudioThread()` and `audioThreadLocks` (Support/ThreadProbe.h); `threadCpuTimeSeconds()` (TestFramework.h:174); `perfRunRequested()` (TestFramework.h:210); `residentMegabytes()` (QualityTestSupport.h:203) and `heapInUseMegabytes()`; processor construction `std::make_unique<LuthierAudioProcessor>()` + `prepareToPlay` + `renderBlocks` pattern (RobustnessTests.cpp:25-60, 455); parameter automation `setValueNotifyingHost` and `getParameterBridge().applyAllNow()` (QA_Robustness.cpp:~108); presets `FactoryPresets::getNumPresets()/getPreset(i)` (QA_Robustness.cpp:427 uses them) or `loadPresetAsUserAction(i)` (PluginProcessor.h:752); board edit API `engine.getReferencePedals().model().apply(cmd::Add/Remove/Move/SetEnabled/SetParam/SetBoardEnabled)`, `board.reclaim()` as in RefPedalSoakTests.cpp:39-95; model ids from `PedalRegistry::registerAll()/all()` (RefPedalSoakTests.cpp:21-27). Timing: juce::Time::getHighResolutionTicks (used AnimatedStringsTests.cpp:918). JSON: juce::JSON::toString(juce::var(DynamicObject)) (used e.g. Source/PhysicalRange.cpp, RefPedalStateTests.cpp); there is no shared stress JSON writer.
- Verify before use: exact accessor to reach the board through the processor on I is `LuthierEngine::getReferencePedals()` (LuthierEngine.h:354); how the processor exposes `engine` to tests (UNVERIFIED - check RefPedalHostTests.cpp).

### Phases (30 s total, deterministic seed 0x51a7, 5 SR x 9 sizes = 45 cells)
Budget: 30 s wall for the real-time-paced edit thread; audio thread runs unpaced (offline speed) for 45 cells ~0.67 s wall each, each cell renders >= 4 s of audio (cell audio duration `max(2 s, 64 blocks)`; at 192 kHz size 1 that is 384k blocks - cap cell at 1.5 s wall, record `blocks_rendered`). Alternative honest design: one pass per SR (5 x 6 s), cycling sizes {1,16,37,64,128,256,481,512,1024,2048} every 8 blocks inside the pass. Prefer this (less setup cost; prepareToPlay per SR only). Each pass: load a preset with >=4 board pedals (Add 4 distinct models; SetBoardEnabled true), edit thread at 100 ops/s mixing SetParam (rapid automation), SetEnabled (bypass), Move (reorder), preset switch every 500 ms (prepareToPlay NOT re-called; use setCurrentProgram/loadPresetAsUserAction on the message-side thread), plus setValueNotifyingHost on 10% of parameters. Input: chord MIDI retriggered every 250 ms plus a -20 dBFS sine on input to keep pedals non-trivially excited.

### JSON schema (version 1)
{ "schema":1, "seed":..., "duration_s":..., "build":{git, config}, "passes":[{ "sample_rate":..., "blocks":..., "block_sizes":{"1":n,...}, "block_ms":{"mean":..,"p50":..,"p99":..,"max":..}, "budget_ms_min":.., "rtf_mean":.., "rtf_p99":.., "overruns":n, "audio_thread_allocs":n, "audio_thread_locks":n, "non_finite":n, "out_of_range":n, "clicks":n, "rss_mb":{"start":..,"end":..,"growth":..}, "edits":{"param":n,"bypass":n,"reorder":n,"preset":n,"rejected":n} }], "pass":bool, "failures":["..."] }

### Pass/fail thresholds and reasoning
- non_finite == 0 and |x| <= 16 (matches existing soak; 16 = +24 dBFS, far above any legal limiter ceiling; any NaN/Inf is a defect).
- audio_thread_allocs == 0 and locks == 0 (existing RefPedalSoak contract). CAVEAT: processBlock currently may allocate (Stress tests do not assert it); measure first on I, then either fix or record a documented allowlist. UNVERIFIED whether the full processor is allocation-free; do not guess.
- overruns == 0 where overrun = block_time > block_duration (n/sample_rate) for blocks >= 64; for blocks < 64 judge on 99th percentile of RTF instead (per-block timer noise and fixed per-call overhead dominate at 1-16 samples). Gate: rtf_p99 < 0.5 on dedicated runner, < 1.0 on shared CI (hosted runners are noisy; ci.yml macOS already needs slack via slowCiHost() TestFramework.h:~222). rtf_mean < 0.25 under LUTHIER_PERF=1 (machine-relative, like PerfBudget). Both thresholds to be calibrated from the first 3 runs: record, then set at ~2x observed p99. Marked clearly UNVERIFIED until measured.
- rss growth <= 50 MB over the 30 s (after a warm-up of the first pass; baseline instance adds up to 350 MB, PerfBudget:353, so only post-warm-up growth is meaningful); skip (report -1) when residentMegabytes()<0 (non-Linux).
- clicks: sample-to-sample |delta| exceeding max(0.5, 8 x running RMS delta) after high-passing, excluding blocks within 2 ms of a preset switch or board reorder (those legitimately change the tone); budget: clicks == 0 in steady-state blocks, <= N allowed inside the excluded windows (report them). Reasoning: a click is a step discontinuity larger than the signal's own slew; 8x RMS-delta is conservative to avoid false positives on a distorted guitar signal. Calibrate on a clean control pass with no edits first (control pass must show 0).
- crash: non-zero process exit / signal; the test runs under the existing runner so a crash fails CI with no JSON; emit the JSON incrementally (write after each pass) so the last pass is visible.
- stable exit code: LuthierTests exit status (0 pass); a second CLI-independent contract is not needed.

### Risks
- Block size 1-4 with a preset changing cabinet/body once crashed (heap corruption, 2026-10-26 handoff). Include size 1 only after prepare; if the matrix trips it, that is a true finding - keep it as a separate pass so the failure is attributable.
- Preset loading allocates and hits disk; must stay off the audio thread (the audio thread only calls processBlock).
