# Luthier feature incompleteness report — 2026-10-26

Assessment source: local HEAD `f5658a4d7837dc10eb45cfc2a98acb7d3d4eeaa3`, branch
`codex/luthier-pedalboard-completion`. Test extension: `d5b64ab650665d94825eaaf460f0024558825f38`.
Production source is unchanged from `f1efeded706b9dc57da7e941ca856b17eaad5183`.
Latest test-only fixture fix: `77ade0bcd4d04f8dd3eb0bf083ec85ae120ce959`.
Free/Pro Release and strict Pro ASan/LSan/UBSan each pass2/11,260 focused checks;
the historical full-suite result and broader open gates remain distinct below.
This report adds no feature or runtime authorization. One coordinator, zero active
workers; the one Linux Free full suite has completed with two failed tests. No push, CI run,
merge, release, installation or paid infrastructure was started for this report.

Subsequent owner-approved initial scope: **Phase90, CE-2, DD-3T, CE-2W, Dyna Comp
and RC-5**, with at least one advanced dedicated looper. The18-model inventory
below remains the retained roadmap; the other12 are future updates. Distortion+
was provisional and is now deferred. See the existing
[scope/retention note](PEDALBOARD-SIX-MODEL-SCENARIO-2026-10-26.md). Original quality,
CPU/numerical/safety limits and common-board/native/manual gates are unchanged.
RC-5's named wrapper/99-memory/clock/advanced-playback integration remains a
large requirement; existing core/storage/rhythm helpers are not its completion.

Luthier is software artwork and a functioning virtual instrument/effects application.
That description does not change its functional promises, licensing or acceptance
requirements. It is not yet qualified as a feature-complete pedalboard release.

2026-10-27 update: [current six-model completion ranges and affected-row corrections](LUTHIER-CURRENT-COMPLETION-2026-10-27.md) supersede the historical counts/next-slice status below; original evidence is preserved.

## How to read the remaining work

The board infrastructure is substantial working code. The largest implementation
gap is the required named pedal catalogue and its DSP/looper integration. A clean
Linux Pro suite and passing Linux plugin validators do not close that gap: an
unregistered model cannot be exercised as a shipping pedal by catalogue traversal.

Amounts below describe bounded engineering work, not percentages or delivery dates.
**Small** means one isolated change/check; **moderate** means several related
changes or a finite qualification matrix; **large** means multiple missing behaviors
or unresolved DSP/real-time contracts. Difficulty measures technical uncertainty,
independently of amount. Confidence is high for inspected source/terminal logs,
medium for outstanding gates documented in existing maps without a fresh dedicated
measurement, and low where current evidence is unavailable.

The private frozen-plan ZIP could not be materialized: the supported helper returned
`library file transfer failed: download failed` (exit 1), without an underlying HTTP
reason. Its bytes were not recovered. This report uses the parent's source-pinned
finish instructions, the normative reference-pedalboard implementation plan and
current repository maps. The older October 9 general project plan is explicitly
historical; its optional/post-beta work is listed separately, not silently added to
the urgent pedalboard release gate. This is a comprehensive assessment of those
identified groups, not a claim to have re-audited every Luthier feature specification.

## Current evidence, with its limits

| Evidence | Actual result and limit |
| --- | --- |
| Linux Pro full suite, production/test source f1efeded | **2,806 tests / 14,935,613 checks / exit 0**, 5,870.9 s. Clean HOME. Includes state, factory presets, routing, existing native UI lifecycle and standard robustness tests. |
| Historical Pro full-suite failure | **2,805 tests / 14,933,248 checks**, one PB02 Wah loudness failure: 0.52 LU against the unchanged 0.5 LU gate. Preserved separately; exact older source was not captured. Corrected f1ef run measures 0.458321 LU. |
| Strict focused ASan + LSan + UBSan | **2 tests / 11,029 checks / exit 0**, 407.5 s. Tiny convolution capacities 1/2/3/4/5/7/8 at 8/44.1/192 kHz; original long test retained. Leak and allocation-family checks enabled. Selected tests, not the whole sanitizer suite or proof of the original corrupting instruction. |
| Linux Free native build | Fresh Release build: tests, renderer, standalone, VST3 and CLAP; **1,650 build steps / exit 0**. Genuine Linux x86-64 binaries, production source f1ef, test code d5b64ab6. |
| Linux Free VST3 | pluginval strictness 5 passes; explicit **30 rate/buffer pairs**, 44.1/48/88.2/96/192 kHz × 1/16/37/64/481/2048, including automation. External Steinberg validator unavailable, not passed. |
| Linux Free CLAP | **21 tests: 18 pass / 0 fail / 3 unsupported preset-factory skips / 0 warnings**. No validator exclusions added. |
| Linux Free standalone | Remained open for the intended 10-second smoke interval (timeout exit 124). No ALSA sequencer device; this is survival evidence, not working audio-device/listening acceptance. |
| Linux Free full suite, historical d5 source | **FAILED:2/2790 tests,21/14,943,980 checks,5375.2s,exit1**. Both tiny-convolution tests fail body-IR reload counters. PB02 passed776 checks. The separate robustness report has0findings; tracked report restoration is byte-identical. Follow-up fix is measured separately; no full-suite rerun is claimed. |
| Follow-up fixture correction77ade0bc | The two presets resolve to the same Free body IR; cache reuse was correct. The alternate fixture now explicitly selects supported mahogany through normal host structural updates. Free and Pro each pass2/11,260checks(28.5/33.0s); strict Pro ASan/LSan/UBSan passes2/11,260/320.0s, no diagnostics. Original presets/render counts and predicates retained, stronger real-IR/active-path checks added. Full suites on the changed test source were not rerun; no Free ASan claim. |
| Native Windows/macOS | Results below are source-specific older diagnostics. Neither is current f1ef product qualification. |

Evidence roots: `/workspace/luthier-evidence/engine-retirement-source8/` and
`/workspace/luthier-evidence/tiny-boundary-free-20261026/`. Exact commands, binary
hashes, original failures, setup failures and run scopes are in
[the QA checkpoint](PEDALBOARD-2026-10-26-QA-CHECKPOINT.md),
[the qualification review](PEDALBOARD-2026-10-26-QUALIFICATION-REVIEW.md) and
[the tiny-buffer/Free handoff](PEDALBOARD-2026-10-26-TINY-BOUNDARY-FREE.md).

## Pedalboard feature-by-feature assessment

### 1. Board engine, routing, bypass and safety — moderate remaining; hard concurrency work

**Working/verified:** immutable compile/publish/retire topology; stable UUID instances;
transactional edits/undo; PRE/LOOP/POST placement; series/parallel/AB, send/return,
sidechain/reamp and bounded feedback; topology rejection; watchdog/fault behavior;
dry latency alignment/crossfades; runtime-aware snapshots; control intent; CPU
admission/governor. Current Pro tests cover these implementations. The existing
30-second concurrent edit/audio grind passed with 3,000 edits, 296 rejected edits,
122,393 blocks, zero measured callback allocations/locks and zero invalid output.

**Biggest gap:** whole-processor real-time/concurrency qualification. The board's
measured callback hooks are useful, but existing body/cabinet paths still contain
try-lock calls. A no-blocking claim is different from the user's strict no-lock
requirement; zero hook counts do not prove every legacy lock primitive absent.
The integrated retirement seam has no current whole-target TSan pass. Earlier
isolated tap-state TSan evidence does not qualify all publishers/readers.

**Next slice/dependencies:** audit the actual process call graph against the no-lock
contract, then instrument the existing bounded retirement tests with installed
tools if authorized. No new feature is needed to begin that analysis. The required
dedicated 30-minute current-board wall-clock soak is not established by the
97-minute full suite or by an older model-only simulated soak. Confidence: high
for current code/tests; medium for the full unclosed concurrency/performance scope.

Evidence: `Source/ReferencePedals/{BoardEngine,TopologyCompiler,PedalPool}.cpp`,
`Source/Tests/RefPedal{Routing,Retirement,Soak}Tests.cpp`,
`Source/DSP/Body/BodyEngine.cpp:227,511`,
`Source/DSP/Amp/CabinetEngine.cpp:565,590`,
`docs/coverage/PEDALBOARD-ENGINE.md`. Early pending paragraphs in that append-only
ledger are superseded where later checkpoints implement the same item.

### 2. Generic effects and shared DSP — moderate qualification; moderate-to-hard DSP work

**Working/verified:** ten registered processors: boost, overdrive, fuzz, compressor,
chorus, phaser, tremolo, wah, delay and reverb. Shared smoothing, filters, bounded
buffers, oversampling/latency helpers, finite guards and denormal handling exist.
Current Pro suite includes generic DSP/partition/extreme-control regressions.
Tail-capable generic delay/reverb have actual residual-tail integration.

**Remaining:** complete each original acoustic/aliasing/CPU acceptance grid rather
than equate finite output with tonal accuracy. Examples retained in the maps are
wet-chain calibration, all mode-switch transient limits, all reverb type/freeze
conditions, and representative isolated CPU/denormal timing. Subsequent density,
tail and CPU-metadata tests exist; old first-slice omissions are not automatically
current failures. No fresh calibrated all-model performance pass was performed
for this report. CPU descriptor values are admission estimates, not measurements.

**Next slice:** close one exact unresolved generic acceptance row using its
existing oracle/probe, then record unchanged limits and measured results. Confidence:
high for ten working registrations; medium for the remaining acoustic checklist.
Evidence: `Generic/RegisterGeneric{A,B,C}.cpp`, `Common/`, `RefPedalGeneric*Tests.cpp`,
`RefPedalTimeFxCpuMetadataTests.cpp`, `docs/coverage/PEDALBOARD-TIMEFX.md`.

### 3. Required named models WP11–WP16 — the largest implementation gap

The integrated production registration inventory is **13 total: ten generic plus
three explicitly provisional compatibility prototypes**. **Zero of the eighteen
required named models is registered.** Four named modulation/delay translation
units contain runtime DSP; fourteen named wrappers are generated skeletons in
this combined tree. These counts are from current headers/translation units and
uncommented registration calls, not old PR labels. Prototypes `prototype.ds1`,
`prototype.ts808` and `prototype.phase90` do not satisfy the corresponding named
model specifications. Procedural face artwork does not satisfy DSP acceptance.

Preserved DSP branch `40ed1621b52a6b9110e15d87747924cf59b96e3d` contains substantial
named-drive/Muff candidates and circuit primitives absent from this integrated
tree. Therefore skeleton here does **not** mean nobody implemented candidate code
elsewhere. That code remains to be reconciled, reviewed, corrected and qualified;
no fresh branch-wide DSP rerun was done for this report, so old failures there are
historical measurements, not asserted current failures.

| Required group | Current integrated implementation | Remaining amount, difficulty, biggest gap | Next smallest slice / dependency |
| --- | --- | --- | --- |
| **WP11: DS-1, TS808, SD-1 (3)** | All three named wrappers are skeletons; `registerDriveA()` empty. Separate DSP candidates exist. | **Large; hard.** Circuit-specific tonal/alias/oversampling/CPU behavior and registration acceptance. Generic/prototype drives cannot substitute. High confidence in integrated absence; candidate acceptance unknown. | Compare one preserved named candidate against its exact spec and unchanged oracle; import only a reviewed passing dependency chain. |
| **WP12: Distortion+, RAT, Dyna Comp (3)** | All three wrappers skeletons; `registerDriveB()` empty. Separate DSP candidates exist. | **Large; hard.** Three distinct clipping/filter/dynamics behaviors; gain/response/transient and CPU qualification. High integrated-source confidence; candidate remaining effort uncertain. | Select one model, reproduce its existing oracle results, fix the smallest demonstrated defect before registration. |
| **WP13: NYC, Deluxe, Green Russian, Wicker Muffs (4)** | Four wrappers/shared Muff header skeletons; `registerMuffs()` empty. Preserved branch has implementations. | **Large; hard.** Multi-stage nonlinear/alias behavior plus variant controls, gate/mids/expression. Four model-specific gates remain; no current accepted common engine is imported. | Qualify the shared candidate engine and one variant before importing the other variants. Preserve unique branch changes and original thresholds. |
| **WP14: CE-2, CE-2W, Phase90, DD-3T (4)** | Four real DSP implementations, tests and telemetry; `registerModDelay()` empty. All-rate behavioral tests pass within Pro suite. | **Moderate-to-large; hard.** Remaining tonal/CPU/spec conflicts and end-to-end registered integration. Exact outstanding numerical decisions are detailed below. | Reproduce one original failing/conflicting criterion on an immutable current snapshot; resolve implementation/spec conflict with independent review before registration. |
| **WP15: Ditto, Nano360, RC-1 (3) + LooperCore** | Named wrappers skeletons and `registerLoopers()` empty. `LooperCore`, `PagedLooperCore`, storage/compaction and state-codec implementations/tests exist. | **Large; hard.** Product wrappers, distinct gestures, seamless boundaries, model readback and actual board asset/error lifecycle. Shared storage is substantial completed work, not a completed named looper. | Implement one specified wrapper against the existing paged core; prove record/play/overdub/undo/cancel/state/failure behavior through the board. |
| **WP16: RC-5 (1)** | Named wrapper and `registerRc5()` skeletons. Separate original rhythm helper has **57 patterns × 2 variations, 7 synthesized kits**, deterministic/bar-change tests. | **Large; hard.** Wire looper + rhythm + **99 memories**, PC/CC profile, slave/master clock output, storage failures and LCD/ring readback. Helpers alone do not provide the pedal. | Complete WP15 integration first, then one RC-5 memory record/save/reload path; preserve original synthesized-pattern attribution. |

Correction after the later E58 checkpoint was inspected: the **current Phase90
uses the factored two-anchor prewarp**, not the earlier literal equal-AP1 model.
The older endpoint error (−7.63/−6.58% against 3%) remains preserved as historical
RED/oracle evidence, not a current unresolved notch failure. Actual-model probes
at five rates pass the original notch/THD limits; local CPU trials are
0.3324–0.3905%, below the unchanged 0.4% budget on that machine. The E58 snapshot
also has 53,039 sanitizer checks and a five-rate 30-simulated-minute/model soak;
no new execution of those probes occurred for this report. Fixed Block feedback
is 0.35, with separate 0.6 stability-probe evidence; neither is silently exposed
as a new product control. The named spec's literal bypass transition delta
<1 LSB24-bit remains unresolved: the intentional fade increment exceeds it,
although the unchanged general A-PED transition criterion passes.

CE-2W CE1 THD and CE-2/DD-3T CPU gates remain unclosed in the evidence ledger;
those measurements were not freshly rerun here. Reviewed CE-2 headroom changes
retain passing original low/high-level THD limits; the old literal-tanh failure
is not still unfixed. The older four-model 30-simulated-minute/rate soak predates
later changes and does not establish current end-to-end acceptance.

Evidence: `Models/Register{DriveA,DriveB,Muffs,ModDelay,Loopers,Rc5}.cpp` (all
entry bodies empty), the eighteen model header/source pairs, `Common/Looper*`,
`Common/PagedLooperCore*`, `Common/Rc5Rhythm*`, `Models/rc5_patterns.inc`,
`docs/coverage/PEDALBOARD-TIMEFX.md:376`, and the accompanying machine-readable
`integrated-model-inventory.json`. No failed candidates were enabled to improve counts.

### 4. Serialization, presets, snapshots and assets — small-to-moderate integration qualification; hard edge cases

**Working/verified:** bounded strict JSON; atomic rejection; unknown-field and
forward-schema retention; portable precision-preserving floating conversion;
eight board snapshots; runtime-aware history; SHA-256 asset storage; missing/corrupt
asset errors. Real processor tests preserve **all 373 appended host intent values**,
including vacant/unused slots and rejected structural requests. Restored action
values do not replay a destructive kill edge. Current Linux CLAP state tests pass.

**Biggest gaps:** actual named-looper audio asset lifecycle is unwired with the
wrappers; current Mac post-prepare exact JSON identity remains unqualified. The
historical original Mac JSON pair was unavailable and is not claimed recovered.
Linux passing exact restoration does not prove the Mac result. Model-specific
saveExtra must use coherent published state; tested helpers do not certify future
wrappers automatically.

**Next slice/dependencies:** end-to-end named-looper save while recording/compacting,
failure and reload once a wrapper exists; run the unchanged exact-state assertion
on a qualified native Mac build when permitted. Confidence high for integrated
Linux behavior; low for the unknown current Mac outcome.
Evidence: `BoardState.cpp`, `BoardSnapshots.cpp`, `Services/AssetStore.cpp`,
`Common/LooperStateCodec.cpp`, `RefPedalStateTests.cpp`, `RefPedalHostTests.cpp:235`.

### 5. Host automation, MIDI, clock and expression — moderate-to-large remaining; hard timing/host integration

**Working/verified:** PRE/LOOP/POST processor hooks; append-only parameter identity;
stable storage slots/UUID routing; bounded event/output queues; exact-offset MIDI
dispatch; automation capture and host/model/MIDI intent resolution; foot gestures,
basic learn and expression helpers. Existing regressions exercise 1,000 host writes/s.
Direct-host MIDI fixtures explicitly reserve registered buffers while stopped;
unknown empty unregistered direct hosts deliberately have no guaranteed output
capacity. Wrapper reservation contracts remain intact.

**Remaining:** full specified phase-PLL/jitter/output-clock behavior; most-active
learn arbitration and duplicate/import conflict flows; complete expression
calibration/macro consumers; NRPN/bank/scene-PC recall+ramping and model-specific
panic semantics. Per-target continuous smoothing was subsequently implemented in
the engine, so the old host map's smoothing-pending sentence is superseded. Helper
tests and basic router support do not establish every end-user workflow. Current
source shows first-qualifying learn behavior; no broad exemption was accepted.

**Next slice:** one precise specified missing MIDI workflow with existing bounded
queue contracts and exact-offset tests. Cross-platform host capacity/lifecycle
qualification follows; native/DAW tests need appropriate hosts, not new credentials.
Confidence high for implemented paths, medium for the outstanding full MIDI checklist.
Evidence: `Midi/{MidiRouter,ClockSync,BoardMidiLearn,ExprEngine,GestureEngine}.cpp`,
`Midi/HostAutomationCapture.h`, `RefPedalHostTests.cpp`,
`docs/coverage/PEDALBOARD-HOST-MIDI.md:47` and later supplements.

### 6. Board UI, faces, search and interaction — moderate remaining; moderate-to-hard native UX work

**Working/verified:** one owned board root, native nonmodal popout/dock, browser,
actual descriptor controls/inspectors, keyboard operations, undo/reorder and host
gestures; seven surfaces/seven mounting types; original physical profiles for all
18 named and ten generic faces; bounded shared image cache; MIDI settings and error
readback. The real GuiReach flow populates actual slots and opens dock/settings/
inspectors; it does not skip all `ref_*` IDs. Current Pro full suite includes the
existing **10,000-operation native processor/editor lifecycle** test. That is the
standard authorized test, distinct from the separately denied extra callback stress.

**Biggest gaps:** full complex-routing cable visualization and usable named-looper
LCD/ring/readback, since backend models are missing. Other finite gaps: seven-style
two-scale golden/DPI seams/LOD finish; per-monitor/maximized geometry; denied-peer
fallback in real hosts; touch/a11y/screen-reader parity; complete learn badges/
conflict workflow; isolated full-board 40-cable repaint/performance gate. A passing
headless control walk is not visual/listening/manual acceptance.

**Next slice:** complete one actual topology illustration against engine truth;
defer named-looper displays until genuine telemetry exists. Native peer/DPI and
screen-reader checks require native host/manual access. Confidence high for current
controls/lifecycle, medium for the remaining visual/accessibility checklist.
Evidence: `Source/UI/ReferencePedals/`, `RefPedalUiLifecycleTests.cpp:62`,
`GuiReachabilityTests.cpp`, `docs/coverage/PEDALBOARD-UI.md:116` and later corrections.

### 7. Robustness, memory integrity and performance — moderate qualification; hard failures if found

**Working/verified:** current full Pro suite, deterministic restoration, hostile
topology/state tests, existing finite-output sweeps, standard 30-second concurrent
grind and 10k native UI lifecycle. Strict targeted sanitizers now cover 21 tiny
sample-rate/capacity cells without suppressing leak/allocation mismatch checks.
Historical PB02 failure and original tiny-buffer corruption evidence remain saved;
the 0.5 LU gate and long tiny-buffer assertion counts were not weakened.

**Remaining:** fresh full-suite qualification at the eventual accepted source;
full instrumented/TSan scope; current dedicated long board/MIDI soak;
isolated CPU/denormal budgets; investigate GCC14 optimized-build
`-Wstringop-overflow` warning at `Source/DSP/Jam/DrumPieces.cpp:663` (`CymbalPiece::render`).
No runtime corruption was established for that warning, and it is not dismissed
by the focused convolution sanitizer result. No complete proof of the original
tiny-buffer corrupting instruction is claimed.

**Follow-up complete:** runtime A/B confirms preset0(Strat) and1(Tele) both play
Tele and use the same IR in Free. Test-only77ade0bc retains both original preset
loads/render counts, selects mahogany after the full preset apply through the
normal host structural flush, and proves real distinct active IRs/counters in
all21cells. Production code is unchanged. Next bounded feature slice is the
actual Phase90 node/model fade composition against the original equal-power
and24-bit-delta criteria; no such test or implementation has started.
Confidence high for logged passes; medium/unknown for unexecuted qualification.

## Native formats and release readiness

Compilation, validator acceptance, full unit acceptance and real-device/DAW
acceptance are separate columns; a product can succeed in one and remain open in
another. Current native delivery priority remains Windows standalone, macOS Intel
and ARM, VST3/CLAP, then Linux. Linux serves as the available local test harness.

| Deliverable | Verified evidence | Missing / next slice; amount and difficulty |
| --- | --- | --- |
| **Windows x64 standalone, VST3, CLAP** | Older source `6d9b5943217d6e0872f65aea6514ebf40fc789e1`, run 40198638051: build, focused GUI/host tests, VST3 strictness5 and CLAP 17 pass/0 fail/4 skips. Preserved diagnostic artifact 12602394197. | **Moderate qualification; moderate difficulty** unless native defects reproduce. Current f1ef retirement/DSP tree and both editions need exact-source build/full relevant gates; standalone device/DAW acceptance absent. Older binary is not a current release candidate. No new CI authorized now. |
| **macOS Intel + Apple Silicon standalone, VST3, CLAP** | Older 6d9 universal compilation/link passed, but malformed `lipo -verify_arch` command failed architecture check before focused tests/validators. Artifact 12602221877 retained as failed diagnostic. Textual command fix and strict fixtures passed later. | **Moderate-to-large qualification; hard uncertainty.** Corrected run 40292011494 at 8d6600f8 has unknown outcome after a denied status read; neither architecture's current runtime pass is established. Need exact-source architecture verification plus both runtime/validator slices and original state/crash diagnostics. Do not retry the denied read or assume the command fix proves binary acceptance. |
| **Linux Pro standalone/VST3/CLAP** | Current production source f1ef products rebuilt in earlier continuation, validators and full Pro suite pass; source-pinned handoff has exact hashes. | **Moderate final qualification.** Required named models/RT/CPU gates still open; device/DAW acceptance absent. No release upload/installer qualification. |
| **Linux Free standalone/VST3/CLAP** | Fresh f1ef production/d5 test build; pluginval all30 pairs and CLAP18/0/3 pass; ten-second standalone smoke. Full suite failed2/2790; test-only fixture correction77ade passes both affected cases with11,260checks in each edition. | **Fresh final-source full qualification remains**, plus common model/performance/manual gates. Build warning preserved. No successful audio-device session. |
| **Packaged Releases assets** | Existing packaging/provenance/checksum scripts and older retained diagnostic bundles; private QA archives available. | **Moderate release work after source qualifies.** New immutable version/tag, genuine per-OS/per-edition format checks, checksums, source commit, install/known-issue/license notices and secret scan. No current release was created. Failed Linux bundles and old native diagnostics cannot be promoted as qualified. |
| **Signing/notarization and real hosts** | No current signed/notarized/manual acceptance claimed. | **Owner/external gates** where certificates, legal agreements, hardware or native hosts are required. Unsigned/unnotarized prerelease delivery may be disclosed if project policy permits; signing is not silently assumed mandatory or available. Do not create identities/credentials or pay for infrastructure. |

Native provenance: [resume handoff](PEDALBOARD-2026-10-26-RESUME.md:34).
The denied request was `GET /repos/djshellshoxxx/luthier/actions/runs/40292011494?exclude_pull_requests=true`,
reported `Forbidden`. No diagnostic headers identified its cause. Actions-read
permission is a documented requirement, not a proven explanation of this specific
403. Its result is **unknown**, not still building, passed or failed. Local
qualification is independent of that denied observation.

## Other project features retained in older plans

These are included so the report does not confuse the pedalboard release with
every historical product expansion. Their frozen-plan membership is unknown until
the exact plan bytes are accessible; no optional feature work was started.

| Feature group | Current evidence / remaining amount / biggest gap | Difficulty, dependency and next slice |
| --- | --- | --- |
| **Core instrument, techniques, practice, rhythm, tune/notation and existing content** | Broad implemented modules and current full Pro tests; Free full suite pending. No fresh per-spec exhaustive audit. Known one-way body/string coupling, pickup damping and synthetic-IR approximations are disclosed design limits, not automatically defects. Remaining work demonstrated here is qualification/manual listening, **moderate**, not a claimed rewrite. | **Moderate qualification**, higher if a specific acoustic failure appears. Next: finish current edition qualification and manual rubric/voicing/listening cases. See `spec/TODO.md` and `docs/KNOWN_ISSUES.md`; stale CLAP-absent statements there are superseded by current actual builds. |
| **Whammy position control (old S12)** | **Confirmed wiring gap, small-to-moderate.** `whammy_position` has UI/APVTS writers, but current engine uses only `midi.getWhammyPosition()` at `LuthierEngine.cpp:2400`; no parameter-to-engine consumer found. Existing editor test verifies knob attachment, not audible effect. | **Moderate**: define knob/MIDI precedence and reset/state behavior before wiring; add an audible regression. Not silently fixed by a passing general suite. Frozen urgent-plan membership unknown. |
| **Pro/Free limits, gating and commercial activation (old S7)** | Edition identities/catalogue substitutions and licensing classes exist. Current `Edition.h::has()` still returns only `isPro`; product call-site search finds only MIDI export checks. Proposed `EditionCapacity.h` absent; broader capacity/feature-consumer and production activation readiness unqualified. **Large** if the whole old commercial rollout is required. | **Hard integration**, plus owner configuration/keys/service terms if commercial activation is required. Next: map actual limit consumers and one feature boundary, retaining cross-edition state. Not a prerequisite invented here for an honestly labelled development build. |
| **Tab-import expansion (old S14)** | Existing import/export code and tests are in the suite. The older plan asks broader dialect/instrument resolution, GPIF hardening/track picker, re-entrant strings, GPX and language normalization. Each new format/behavior's current completion is **unknown**, not freshly source-audited. **Large historical expansion**, not an asserted current blocker. | **Hard**, fixture/corpus/provenance dependencies. Next if frozen scope requires it: inventory one specified importer with licensable authored fixtures; do not claim old research as implementation. |
| **Visual polish, support/help and subjective acceptance** | Core UI/help mechanisms exist. Older TODOs retain multi-monitor/DPI/illustration polish, subjective screenshots/listening and actual support endpoints. Current exact remaining non-board count **unknown**; likely several bounded checks rather than missing whole screens. | **Small-to-moderate per item**. Human visual/listening review and real support details may be owner gates. Next: one identified current failure, not blanket adoption of every old unchecked box. |
| **Standalone CLI easter egg, seven premium/v2 additions (old S5/S8)** | Older plan labels CLI egg unstarted and premium list undefined/post-beta. No current source re-audit of those additions; **unknown/not urgent frozen scope confirmed**. | No defensible estimate until required behavior is confirmed. Preserve/defer rather than expand this finish task or call the pedalboard done because optional work was excluded. |
| **Golden/preset/IR data regeneration (old S9)** | Current Pro full-suite PB02/normalization checks pass; prior generation/source manifests retained. Old 'all stale' wording is not current proof. Final regeneration after further audible DSP changes is a **moderate finite qualification step** where required. | **Moderate**, deterministic tooling/content provenance. Next: compare source-specific manifests/goldens after the final accepted audio change; do not regenerate solely to hide an oracle mismatch. |

## Constraints, attribution and immediate continuation

The single Free run finished at20:47:11UTC, PID165131/session92865, with the
failed outcome above. Log SHA256:
`35adaa26f97cd15e375d6904cd9a7c7a3559ba4d702a9b5f87a339881f38e0cd`.
Generated report and restored original SHA256 both:
`e8993815226dd95e871671ac0effdb7d2e482d2abe1cc80efb2a02d4441d78d6`.
The original acceptance JSON retains `terminal:null` because its parser missed
the failed-summary format; the independent result JSON records exact totals,
with original output preserved and acceptance still false. Nineteen zero-check
cases and a compiler-guarded Clang18 golden skip are not new passed gates.
The0.5LU predicate, assertions and original failure evidence are intact.
Focused diagnosis and the test-only fix77ade subsequently passed both editions
and strict Pro sanitizers, without another full suite. Locally feasible
qualification is distinct from owner-only access:
missing named implementations, a not-yet-run bounded check and a denied endpoint
are three different conditions. No blanket pause waiting for credits is inferred.

The separate automatically denied additional UI callback stress remains unexecuted;
it must not be retried or repackaged. Current instructions also prohibit pushes,
CI launches, merges/deployments/releases, paid infrastructure, new downloads,
credential/security changes and denied Library-input retries. Those restrictions
do not imply these actions already failed in this report. Source integration would
require fresh ownership/ref checks and the applicable authorization, not a blind
merge of diverged worker branches.

Attribution is preserved: first-party copyright/license remain Sheldon Davidson /
Circuit Drift Labs under `LICENSE` and `COPYRIGHT-TRADEMARK.md`; `THIRD_PARTY_LICENCES.txt`
must accompany distributable artifacts. The adapted tiny-buffer fixture retains
history including Claude's Q3 harness commit `74f0dcda2a1e2027f446383376669f0821c97248`,
later Sheldon Davidson commits `63178fcbebcbb4bfd5ca97012ef168baa2d5324e` and
`34ac45e9ee8a2298042aa44a8cfd83fad794ac50`, and this Codex continuation's d5b64ab6
test extension (existing repository author identity preserved). No author history
was rewritten. JUCE 8.0.10 and the pinned clap-juce-extensions source retain their
upstream notices. Evidence-only archives previously carried summaries/logs but
not separate license copies; the report supplement includes those notices. No new
license entitlement or hardware equivalence is asserted.

The machine-readable inventory and source hashes accompany this report. It is an
as-of assessment; pending execution remains pending until its actual terminal
evidence is added. There is no evidence-based overall completion percentage or
promise that native release qualification will take a particular number of hours.
