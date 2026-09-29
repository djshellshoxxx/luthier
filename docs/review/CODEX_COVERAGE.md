# Q2: engine, DSP and technique test coverage

Baseline: `1a0805a679ac1677898bfd37d75fb9a52961058a` on
`claude/luthier-cloud-session-5lzlix`. Review date: 2026-09-26.
Branch: `codex/luthier-qa-coverage`.

## Status and method

Static coverage map, not measured line/branch coverage. Enumerated the pinned
Source tree and read all 120 test .cpp files under Source/Tests. A lexical scan
found 1,369 LUTHIER_TEST occurrences; this is not a runner-confirmed test count.
Matched module names and API calls, then inspected relevant test bodies, source
contracts, engine integration and technique specs. A missing symbol is evidence
of absent *direct* coverage, not proof that integration tests never execute it.

Read the QA board, HANDOFF, EXTERNAL_AGENT_BRIEF, CLAUDE_CODE_BRIEF, INDEX,
gui-integration, ui-wiring, DECISIONS, COORDINATOR_PLAN, engine,
engine-technique-layer and technique-cascade; this is a scoped QA review, not a
complete certification against every specification or current feature-fleet work.
The assigned report path takes precedence over the generic brief's
docs/coverage/<WORKSTREAM>.md convention.

Added seven deterministic tests in Source/Tests/CodexQA_Coverage.cpp. They are
**uncompiled and unexecuted**. No Linux-green claim. Coordinator reports WSL
access denied and no usable Docker/cmake Linux route; HANDOFF records the Actions
spending-limit block. No Windows/macOS build or test was attempted.

## Feature-to-test map at the pinned baseline

Paths in the existing-test column are under Source/Tests. "Covered" below means
an explicit assertion exists, not that every input, branch or specification is covered.

| Engine / DSP / technique area | Existing test evidence | Remaining coverage boundary |
| --- | --- | --- |
| Common DSP, delay interpolation, oversampling | EngineTests: Common and DelayLine suites; LatencyTests; ReviewRegressionTests | Integer circular indexing, readTap observer isolation and reset across interpolation modes were not directly tested; added here. readKernel analytic accuracy remains open. |
| String pitch, decay, excitation, harmonics | EngineTests; SustainDecayTests; HarmonicRealismTests; FingerstyleAttackTests; StringAgingTests | Broad physical assertions exist; systematic cross-product of rates, contacts, interpolation modes and state transitions is not established by this review. |
| Coupling, body modes, environment, interaction | BodyCouplingTests; EnvironmentTests; StringInteractionTests; PartAcousticsTests; EngineTests | Existing named physical measurements; not an exhaustive multi-technique cascade oracle. |
| Pickup and guitar circuit | EngineTests Pickup suite; CircuitTests; NoiseFloorTests; PartAcousticsTests | Resonance, position nulls and circuit behavior covered; combined lifecycle transitions need targeted cases, not another smoke test. |
| Amp, cabinet, room, effects | EngineTests; EffectsQaTests; IrReloadTests; DoublerTests; ReviewRegressionTests | Stability/response/latency regressions exist. SecretEffect has a stability-at-maximum test, not an exhaustive parameter-response specification. |
| Whammy | EngineTests: TransTrem intervals, vintage detuning, fixed bridge; ReviewRegressionTests hardtail ranges; TuningStabilityTests; AnimatedStringsTests | No test calls setTransposeLock or setPerStringEnabled. Added per-string offsets, transpose composition, down-only behavior and removal of inactive strings. |
| Feedback, E-Bow, freeze | FeedbackTests; EBowTests; SustainTests; ModelGapsTests | Explicit feature suites exist; do not classify these as untested. |
| Noise, scrape, slap, slide | NoiseTests; BuzzTests; NoiseFloorTests; ScrapeTests; SlapTests; SlideTests | Scrape/slap preemption and several wiring cases exist; they do not cover every six-technique same-/cross-string combination. |
| Technique inference and MIDI triggers | ModelTests Technique suite; ControllerTests; HarmonicRealismTests; SlapTests TechniqueTriggers suite | Reset of pending requests/CC edges, disarm release, and byte/offset-preserving filtering added here. Queue overflow and rapid source reassignment remain open. |
| Tap, rhythmic mute, microtonal bends and cascade | ModelTests covers ordinary tap/inference; BassTechniqueTests covers bass grid; SlapTests/ScrapeTests cover selected combinations | No TapEngine, MuteEngine, CascadeResolver or PreBend identifier in scanned test .cpp files; no microtonal mention. No corresponding module filenames in pinned Source tree. This is a spec/implementation readiness gap as well as a test gap, not evidence that ordinary palm mute, bends or taps are absent. |
| Rhythm, voicing, strum, tuning, controllers | RhythmTests; RhythmSchedulerTests; RubricVoicerTests; StrumGestureTests; ModelTests; ControllerTests; TuningStabilityTests | Strong dedicated suites; exhaustive ordered technique-pair coverage is not supplied by generic rhythm tests. |
| Jam drums and bass | JamDspTests JM24-JM34; JamTests; JamProcessorTests | DrumPieces is indirectly exercised through kit fixtures. KitRoom had no direct symbol reference; isolated reset and additive stereo-output contract added here. No acoustic RT60 certification added. |
| Master, loudness, true peak, quality modes | NormalizationTests; NormalizationGoldenTests; EngineTests Master; QualityModeTests; QualityModeRenderTests | Existing golden, level and mode-switch coverage is substantial; this review does not replace the existing release/performance gates. |
| Routing, engine/processor, presets/state | IntegrationTests; PluginBusTests; RoutingTests; StateModelTests; HostStateTests; PresetQaTests; RobustnessTests; CombinationTests | Existing integration coverage acknowledged. Randomized state/factory-preset QA belongs to Q3; audio-thread allocation review belongs to Q1. |

## Ranked gaps and next actions

1. **P1 — full six-technique compatibility matrix and timing oracle.**
   spec/technique-cascade.md sections 7 and 10 require every ordered same-string
   pair, cross-string independence, 10 ms preemption and 30-second reference
   renders. Source/Model/Playing/TechniqueTriggers.h currently enumerates scrape
   and slap; the pinned tree has no TapEngine/MuteEngine/CascadeResolver filenames.
   Existing SlapWiring.slapAndScrapeTakeTheStringFromEachOther and scrape tests
   cover useful subsets. Do not invent production APIs in QA tests. Feature
   owner should establish implemented scope, then add table-driven fixtures for
   every ordered pair and an independent per-string pitch/excitation oracle.
   Deferred: cannot close through permitted new tests alone.

2. **P1 — shared delay-line exactness and observer state.**
   Source/DSP/String/FractionalDelayLine.h:48,96,123. Existing two direct delay
   tests check sine level and modulation smoothness, which can miss indexing,
   stale reset and tap side effects. Added exact integer-delay checks across
   wrap at both safe read limits, readTap/allpass observational equivalence,
   and silence after reset in all modes. Remaining: readKernel fractional
   reference interpolation and mode-change continuity. Tests await Linux.

3. **P2 — technique event lifecycle and non-technique MIDI preservation.**
   Source/Model/Playing/TechniqueTriggers.cpp:7,32,111. Existing SlapTests cover
   idle, keyswitches, zone ownership and CC edges. Added pending-button/held-CC
   reset, fresh edge after reset, disarm clearing, and exact bytes/offsets for
   unrelated notes/CC/pitch wheel while keyswitch note-on and velocity-zero
   release are consumed. Remaining: bounded-buffer overflow, simultaneous
   sources and reconfiguration while held. In particular inspect event delivery
   versus held-state convergence when kMaxEvents is exceeded; this is a test
   recommendation, not a reproduced bug finding.

4. **P2 — whammy per-string and transpose controls.**
   Source/DSP/Whammy/WhammyEngine.cpp:105,110,125. Existing interval tests do not
   invoke these paths. Added analytic cent expectations at 44.1/48/96 kHz,
   inactive-string clearing, and down-only restriction on per-string motion.
   Remaining: spring-return transient spectra and block-partition timing.

5. **P2 — isolated Jam room lifecycle.**
   Source/DSP/Jam/KitRoom.h:26 and KitRoom.cpp reset. Existing Jam kit integration
   does not isolate the room's additive output contract. Added non-vacuous stereo
   impulse response, exact reset replay, preservation of caller output, and
   silence after reset at 44.1/48/96 kHz. Remaining: RT60/decay response and
   sample-rate-change preparation measured acoustically.

6. **P3 — pitch tracker transitions and independent accuracy cases.**
   HumCaptureTests includes pitch/noise, melody transcription and Tune UI flow.
   Add silence-after-voicing, rapid octave transitions, low/high supported range,
   confidence gating and non-default-rate oracles before claiming full tracking
   coverage. Lower priority here than shared waveguide and gesture state.

## Added tests and intent

All use suite filter `CodexQA_Coverage`; no production, specification,
existing test, CMake or parameter files changed.

| Test | Oracle / regression caught |
| --- | --- |
| integerDelaySurvivesCircularWrap | Known integer sequence versus exact delayed sample at multiple rates, over three buffer turns. |
| harmonicTapDoesNotAdvanceAllpassState | Main allpass output must match untouched control sample-for-sample despite additional taps and delay jumps. |
| delayResetClearsEveryInterpolationMode | Filled delay and interpolation state must produce exact silence after reset. |
| whammyPerStringControlAndTransposeAreIndependent | Analytic cent offsets for independent strings plus fixed transpose; disabled strings clear and upward motion is suppressed in down-only mode. |
| triggerResetDropsPendingButtonAndHeldController | Pending request and held state clear; retained configuration receives a new high-CC edge. |
| keyswitchFilterPreservesUnrelatedMidiBytesAndOffsets | Byte-for-byte and sample-position equality of nonconsumed messages; velocity-zero keyswitch releases. |
| kitRoomResetReplaysImpulseAndAddsToOutput | Nonzero stereo response, exact replay, additive output and reset silence. |

No confirmed new production bug or executed expected-fail case is claimed.
No thresholds in existing tests were changed or weakened. If Linux exposes a
real defect, preserve the failure evidence here and hand it to the owner; do not
relax the oracle to obtain green output.

## Verification and coordinator handoff

Completed: pinned-source API review; existing test inventory and direct-symbol
search; CMake inspection; static whitelist and LF checks; GitHub readback/diff.
CMakeLists.txt automatically includes Source/Tests/*.cpp via GLOB with
CONFIGURE_DEPENDS and the file uses the existing LUTHIER_TEST registration macro.
No build-system edits are necessary.

scripts/fixeol.sh was inspected: it restores CRLF only when a changed file's
HEAD content already used CRLF. Both additions are new LF files, so that
restoration has no applicable input. The script itself was **not executed**;
the connector commit contains only new LF files and leaves all existing blobs
unchanged.

Not completed: C++ compilation, Linux test execution, sanitizer execution,
performance measurements, coverage instrumentation or full regression run.
No CI pass is inferred from PR creation. Run on a working Linux checkout:

```sh
cmake --build build --target LuthierTests --parallel
xvfb-run -a build/LuthierTests_artefacts/Release/LuthierTests CodexQA_Coverage
xvfb-run -a build/LuthierTests_artefacts/Release/LuthierTests DelayLine Whammy TechniqueTriggers JamDsp
```

Then run the coordinator's required full regression gate. Keep the PR draft
until Linux build and test evidence exists. This lane never merges or modifies
the integration branch.
