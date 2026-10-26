# Six named pedals first — unapproved release-scope scenario

This is an evidence-based proposal in response to the owner's “what if”, not a
ratified scope change. Keep the full eighteen-model roadmap, all candidate source,
tests, failure evidence, parameter/state compatibility and original acceptance
limits. No registration, deletion, build, test, CI or release was started for this
comparison. The existing Linux Free full suite continues independently.

Source: integrated production `f1efeded706b9dc57da7e941ca856b17eaad5183`, local
documentation HEAD at comparison `d4137a638a30d4d6965fb955c4caf1774ab1214a`;
retained DSP branch `40ed1621b52a6b9110e15d87747924cf59b96e3d`. The retained timefx
branch is `e8138ee2fdc7c5cf823dbd3ccf814585ec912f9e`; the four models below are already
in the combined production tree. These are local retained refs, not a new remote
author/branch survey. Raw results are preserved historical probe executions; this
scenario analysis runs no new measurements.

## Recommendation

Provisional first six, in evidence/readiness order: **Phase90 Script/Block,
CE-2, DD-3T, CE-2W, Dyna Comp, Distortion+**. The last two, especially sixth place,
have lower-confidence remaining-effort estimates. **None of these six is currently
release-qualified or registered.** There is no evidence-backed set of six ready
pedals that can simply be switched on.

This means six of the eighteen **named** models. The existing ten generic effects
are not counted as named models or silently removed. The three explicitly
provisional compatibility processors do not satisfy named acceptance; preserve
legacy state even if a later approved product decision changes their presentation.

| Rank / model | Why it makes this shortlist | Original gates and missing integration | Expected remaining effort / confidence |
| --- | --- | --- | --- |
| **1. Phase90** | Actual E58 model is integrated. Five-rate two-anchor notch/THD evidence passes unchanged limits; local O3 stereo process CPU 0.3324–0.3905% versus 0.4%. Preserved 53,039-check sanitizer snapshot and five-rate 30-simulated-minute/model soak. | Literal named bypass delta <1 LSB24-bit is unresolved: intentional fade increment 9.04e-6–4.23e-5 versus 1.1921e-7. General A-PED transition passes, but cannot replace that gate. Need review/closure of intended criterion, full current registration/state/automation and real CPU/memory admission metadata, plus native qualification. | **Moderate; hard acceptance ambiguity**, high confidence in current code and preserved evidence. Do not present old uncorrected notch failure as current. |
| **2. CE-2** | Integrated BBD/clock/mode/foot/expression implementation; reviewed headroom correction passes original low-level <0.3% and high-level 2–6% THD at four rates. Partition, hostile input, stereo and callback-allocation tests exist. | Stable CPU <0.4% not established: retained post-bypass trials span 0.3799–0.4715%. Remaining BBD-mode calibration, full spectral/transition criteria, named bypass interpretation and end-to-end registration need closure. A single fast trial is not acceptance. | **Moderate; DSP/performance work**, medium confidence. More evidence and smaller recorded performance gap than the branch high-gain drives. |
| **3. DD-3T** | Integrated actual delay, quantization, tap, sync and Short Loop, foot-gesture ownership, coherent extras save and wet-tail hook. Detailed timing/reset/partition/feedback/state tests exist. Its Short Loop uses its own buffer; it does **not** require the missing RC-1/RC-5 wrappers or 99-memory storage subsystem. | 0.5% CPU budget remains open: historical post-bypass range 0.5235–0.6217%; later memoization trials did not establish a stable pass. Complete exact requested acoustic/mode-transition/state/bypass gates and registered host/MIDI/UI flow. Keep 333.3 ms subdivision behavior with the documented normative timing rationale; no timing tolerance is relaxed. | **Moderate; moderate-to-hard**, medium confidence. Unclosed performance is substantial, but not an absent product wrapper. |
| **4. CE-2W** | Integrated three continuously advancing mode histories, stereo/wet-polarity/vibrato behavior and tests. Preserved local CPU 0.7951–0.8441% is below its 1% target on that machine. | CE1 wet THD probe 0.8065% versus original 1–3% band remains unclosed; estimated high-frequency CE1 color needs calibration. Rate-law text and physical-buffer versus wire-null wording need reviewed resolution without silently changing behavior or limits. Registered state/automation and native evidence still needed. | **Moderate-to-large; hard tonal/spec uncertainty**, medium confidence. Included after considering these blockers, not merely because its code is integrated. |
| **5. Dyna Comp** | Preserved branch has an actual estimated OTA/detector/program-dependent-release implementation, mix/Mini attack controls and short 15/23-sample oversampling. Finite/block/reset/RT tests and an exact 789,504-value release-cache comparison exist. Less large-oversampler/circuit dependency than high-gain drive candidates. | Corrected natural-sensitivity probes fail original THD: 0.042433% at default versus 0.1–1%, 0.016624% at maximum versus 3–5%. FAST attack/default recovery and 0.5% CPU gate remain open. Historical actual-default CPU 1.083%; later independent-stereo cache trials ~1.189% were contention-affected, not qualification. M102/Script component variation incomplete. Needs scoped branch reconciliation, original acoustic/timing gates, full model integration. | **Large; hard**, medium-low confidence. Smaller apparent architecture/performance gap than several drives, but not a nearly accepted compressor. |
| **6. Distortion+** | Preserved branch has a complete public descriptor and actual specified estimated shunt circuit, variant/filter/state/smoothing/partition/hostile-input tests. Correct original harmonic fixtures pass drive .5/max (25.459% in 10–35%, 39.301% in 25–60% at 48 kHz); only low drive of those three fails (17.981% versus 5–15%). Simpler single-drive choice than importing the whole Muff family. | **Serious remaining risk:** Eco ADAA option explicitly unavailable; original low-drive THD fails, original-stimulus alias grid remains open, long CircuitPedal oversampler/CPU unresolved. Historical O2 stereo CPU 17.6537% versus 0.8%; later contention-affected solver trials do not close it. Solver iteration, latency/bypass and complete registered integration need checks. | **Large; hard; lowest-confidence selection.** It is a provisional investigation slot, not a promised quick sixth finish. Replace it if a matched immutable comparison establishes a better candidate. |

CPU trials above differ in compiler flags, stimulus, quality, snapshot and build
contention; they are **not** a controlled leaderboard and are not frequency-normalized
to the specification's reference CPU. They identify open risk, not guaranteed
optimization factors or predicted delivery dates. Passing structural millions of
checks does not override a failing required tonal/timing/CPU measurement.

## Why not automatically select other candidates?

**Next alternates: TS808, then Green Russian**, with order tentative because no
matched current all-candidate qualification run exists. TS808 has the benefit of
a real circuit and control inventory on the DSP branch, but recorded default
gain +17.724 dB fails unity ±1.5 dB, original low-drive THD fails while maximum
passes, alias/CPU fail, and long-FIR latency exceeds the named allowance. Its
literal low-frequency circuit versus bass-clean requirement also needs review.
It is not demonstrably easier than Distortion+ just because its prototype exists.

Green Russian avoids Newton iteration with a closed-form asinh approximation,
making it an interesting performance alternate. However its nonlinear/interstage
laws contain unadopted estimates; THD 10.7872% misses 15–40%, the relative notch
15–25 dB remains unmet, and the retained 5 kHz alias diagnostic −19.567 dBc misses
−60 dBc. Its 4.8025% stereo CPU cannot be directly compared with a summed-mono
1.5% target. It carries more audible-model uncertainty than a simple registration
task. Do not swap it in without original-oracle evidence.

**NYC/Wicker** have strong independent KCL/tone and level-calibration work, but
two nonlinear stages, original THD failures (57.5365/49.8927% versus 15–35%),
solver-count/CPU gaps, missing enable pre-roll and unclosed original alias tests.
**DS-1** has particularly large original output-THD errors (150.050/211.862%
versus 1–6/25–40%), plus gain/alias/CPU/latency gates. **SD-1** has low-drive
harmonic/asymmetry, bypass response, latency and CPU issues. **RAT** has an original
mid-drive THD failure, expensive multi-voice/circuit work and absent dynamic
oversampling transitions. **Deluxe Muff** has the largest calibration/integration
risk: +41.596 dB reference gain, safety-guard hits, low/high-sustain THD failures,
gate/attack/OS work. None is shown to be closer by the available evidence.

**Ditto/Nano360/RC-1/RC-5** have no implemented named wrappers in this integrated
tree. The paged core, state codec, worker compaction/storage and original 57×2
rhythms are useful completed prerequisites, but do not supply their gestures,
readback, scene/memory behavior or end-to-end failure handling. Avoiding these
four for the first release removes a major dependency chain. DD-3T Short Loop
must not be advertised as a replacement for those missing products.

## Shared work the six-model scenario does not remove

- Preserve the same original quality/CPU/numeric/safety gates for each chosen
  model. No disappearing failure, unavailable control or unmeasured field becomes
  accepted because twelve other models are deferred.
- Integrate branch code serially after semantic/patch review, not by replacing
  the combined tree. The Dyna/Distortion+ dependencies include current helper and
  oversampling/state-layout contracts; preserve authorship, notices and original
  failed measurements. Never import unrelated branch history blindly.
- Add validated registrations only after model acceptance. Named timefx
  descriptors currently carry zero CPU cost while unregistered; do not enable
  them as free CPU slots. Provide correct prepared-memory/admission metadata,
  actual parameter mapping/gestures, original procedural faces/readback, bounded
  extras serialization, bypass/tails and instance lifecycle evidence.
- Board real-time no-lock/concurrency/CPU requirements, complete complex-routing
  UI behavior, generics' own quality gates, state/migration and repeated loads,
  required soak and existing regression thresholds remain.
- Current-source Windows standalone, Intel/ARM macOS, VST3/CLAP and Linux build/
  full-test/validator/package qualification, genuine device/DAW/listening checks,
  provenance/checksums/notices and signing status remain separate. There is no
  new progress on these gates merely from proposing a smaller catalogue.

If the owner later approves six, retain these twelve explicitly for updates:
**DS-1, TS808, SD-1, RAT, NYC Muff, Deluxe Muff, Green Russian, Wicker Muff,
Ditto, Nano360, RC-1 and RC-5**. Keep their specifications, source branches,
tests/oracles, asset/state identifiers and failure logs. This proposal edits no
normative roadmap, reserves no release tag and promises no deletion or date.

## Effect on remaining work

The benefit is real: it removes twelve model acceptances from the initial release,
including the four missing looper wrappers, full RC-5 memories/clock integration
and most difficult multi-stage families. It does **not** remove two-thirds of all
remaining project work: board/UI/host/generic/platform requirements are shared,
and the proposed sixth model still has severe gaps.

The earlier 50–60% full-eighteen estimate was a rough, low-confidence engineering
judgment, not measured acceptance. For this **conditional** six-model functional
scope, “around two-thirds”, broadly **60–75%**, is a plausible planning range,
assuming reusable branch candidates and tractable fidelity/performance fixes.
The upper end is optimistic; it is not justified if the last two models need DSP
redesign. No measured overall percentage or platform-release percentage exists.
The narrower 80–90% estimate applies only to basic board/routing/state/UI plumbing,
not full UI, strict RT, every required model or native/manual release acceptance.

## Exact evidence to consult next

Integrated source at f1efeded: `Source/ReferencePedals/Models/{mxr_phase90_script,
boss_ce2,boss_dd3t,boss_ce2w}.{h,cpp}`; tests under `Source/Tests/RefPedal*` and
`ReferencePedals_*`; actual corrected probes in
`docs/coverage/probes/phase90-e58-{actual,edges,soak}.cpp` and the adjacent
`phase90-e58-{actual,edge,soak}-results.txt` files;
`docs/coverage/PEDALBOARD-TIMEFX.md:376–452` and later DD3T/tail notes.

DSP source at 40ed1621: `Source/ReferencePedals/Models/{mxr_dynacomp,
mxr_distortion_plus,ibanez_ts808,ehx_green_russian}.{h,cpp}` and
`Common/{CircuitPedal,OversamplerAdapter,NewtonClipper}.h`;
`docs/coverage/PEDALBOARD-DSP.md`, especially the corrected Dyna natural-control
section, original harmonic fixtures (line475), release cache (636), and
`docs/coverage/pedalboard-dsp-evidence/{original-named-thd.txt,
named-drives-wp12-measurements.txt,dynacomp-natural-thd.txt,
dynacomp-release-cache-comparison.txt}`. Newer primitive optimizations explicitly
do not close whole-model CPU gates. No stale first-slice claim is substituted
for the current implementation, and no historical result is labelled fresh.
