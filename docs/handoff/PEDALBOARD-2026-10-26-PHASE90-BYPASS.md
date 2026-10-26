# Phase90 bypass contract audit — 2026-10-26

Starting local HEAD `4d50cab038828e91424d90dfd52fc8f531fc3044`, branch
`codex/luthier-pedalboard-completion`, same `/workspace/luthier` executor.
Production is unchanged from `f1efeded706b9dc57da7e941ca856b17eaad5183`.
The Pro/Free convolution fixture fix `77ade0bcd4d04f8dd3eb0bf083ec85ae120ce959`
is preserved. One coordinator, zero active workers; no new team or environment.
No AGENTS.md was found at the repository/workspace root or in the affected
source/spec directories. No runtime or original test source is edited here.

## Result and engineering review correction

Fresh-context independent review at `0bef72a64b365eab375fde27f3044b8e819c5b1a`
confirmed the double fade but corrected the initial categorical law-conflict
claim. The original private evidence checkpoint version3 retains that earlier
analysis and every measurement. This correction changes no runtime, gate,
threshold or raw result. No extra probe or test was run for the review.

**Named Phase90 bypass acceptance remains RED. Do not register the model.**
There are two distinct issues, neither cleared by existing passing model tests:

1. The real model applies a linear engage ramp; the real node applies another
   linear ramp. With zero trims and latency, their composition is
   `dry + a*a*(wet-dry)`, rather than one fade. This is reproduced on the current
   source, not inferred solely from the older failed probe.
2. The original fade clauses have a compatible interpretation. Phase90 §3
   requires **5 ms equal-power audio mixing**; `BOARD_CORE.md` §5.2 says the
   **`mix_enable` control ramps linearly over5ms**, without explicitly requiring
   a linear audio equation. A linear position driving cosine/sine gains can
   satisfy both. Phase90 §9 separately says
   “peak delta < 1 LSB 24b for sine.” Both raw adjacent-sample delta and the
   isolated gain-change contribution of an ideal equal-power fade exceed that
   bound on the test sine. The text does not define a discontinuity estimator,
   permitted input amplitude/frequency, or slope subtraction.

`IMPLEMENTATION_PLAN.md` §1 takes precedence for actual conflicts; no entry
establishes a contrary Phase90 audio law or defines that literal delta. A-PED
§5's separate1.5× natural-step limit is retained and cannot replace the named
24-bit criterion. No requirement or threshold has been lowered.

Engineering can proceed toward the already specified equal-power law with one
fade owner, while preserving true dry bypass and frozen LFO. This is not
categorically an owner-only sound choice. A proposed clarification of exactly
what the24-bit delta measures still needs recorded review; it is not silently
approved here. Preserve original wording and measured diagnostics alongside it.
Simply leaving the model permanently enabled would remove the frozen-clock
behavior; changing the node's global law would affect other pedals. Neither is
a justified correction. Changing the specified equal-power audio law, fade
duration or required bypass/clock behavior would be a separate product change.

Review recommendation, still unimplemented: the board owns the audible
envelope; Phase90 supplies its active processed path, while `setEnabled` still
controls clock freezing/reset. Use a narrow model-specific audio-curve policy,
preserving other models and Buffered/DSP tail contracts. Scope direct-model
versus composed-slot assertions before changing any existing unit test. The
current probe proves free-running freeze only: `setTime` and transport handlers
can write phase while disabled, so sync/transport freeze needs its own test.

## Independent current-source experiment

New audit source:
`docs/coverage/probes/phase90-bypass-contract.cpp`.
Build helper:
`docs/coverage/probes/build-phase90-bypass-contract.py`.
These are explicit acceptance diagnostics, outside the automatically globbed
unit runner; their RED exit must remain visible. They do not remove, skip or
replace any original test. No dummy model, mock mixer, registry import or
production substitute is used.

The executable freshly compiles the actual `MxrPhase90Script`, `NodeTable.cpp`
and `PedalPool.cpp`, and drives the actual `routing::nodePedal` with a real
`PedalRuntime`, `PedalNodeState`, `Topology` and `ExecCtx`. It directly mirrors
the engine's enable-to-model dispatch. This is a node/model test, **not a full
BoardEngine/catalogue/host integration test**.

The independent wet reference is another fully enabled instance of the same
actual DSP. Sweep depth and mismatch are zero so a frozen/advancing LFO cannot
misalign its filter history. All four nonlinear stages and independent stereo
histories remain. Before every transition the reference agrees exactly with
the expected actual active/dry paths: maximum pre-event error **0**. No impulse
or synthetic fixed-gain replacement stands in for the phaser.

Matrix: 44.1/48/88.2/96/192 kHz × host sizes 1/4/16/64/512/2048 × enable/disable:
**60 cases**. Host buffers are split into the real node's ≤64-sample execution
size and at the event. Signals: left 1 kHz sine, peak0.1, phase0.37rad; right
731 Hz cosine, peak0.15, phase0.13rad. Warmup250ms; event then5ms fade plus64
settled samples. Trims unity, latency0, hiss off, default Script variant.
This is a bounded transition test, not a long soak, Block-variant certification,
callback allocator/lock instrumentation or native host test.

The equal-power oracle is independently `dry*cos(a*pi/2)+wet*sin(a*pi/2)`, with
exact endpoint branches. Linear and squared-linear predictions are separately
reported diagnostics, never substitutions for that oracle. Float evaluation
tolerance is1e-14, not a changed audible threshold. Signed PCM24 uses the
conservative larger one-LSB spacing2^-23 =1.1920928955078125e-7; using2^-24
would fail more strongly.

Ranked cause checks:

1. Two fade owners predict the squared-linear node curve. Maximum mismatch
   **3.5388358909926865e-15**, while the single-linear mismatch reaches
   **0.024199572793760532**.
2. Misaligned reference state/LFO predicts pre-event or settled differences.
   Pre-event error, settled dry null and settled active error are all **0**.
3. Buffer-boundary timing/gain errors predict partition-dependent measurements.
   All recorded curve/signal metrics are identical across the six host sizes;
   actual one-sample runtime endpoint observations are within one sample of5ms.

## Results preserved separately

| Check | Outcome |
| --- | --- |
| New original equal-power oracle | **RED60/60**, direct-model maximum error0.05143641535296209; node maximum0.04710071136835112 |
| Single-linear prediction (diagnostic, not a separate required audio law) | **RED60/60**; squared-linear agreement confirms compounded fades |
| Raw output peak delta compared with1LSB24 (diagnostic interpretation) | **RED60/60**, measured0.0035774793528568225–0.01560920884586392 |
| Dry-sine control peak delta | 0.00358820153218546–0.015614971588625667, also greater than1LSB without a fade |
| Independent ideal equal-power gain-step contribution (diagnostic) | **RED60/60**,0.00021786300661905122–0.0009994847057285122 (about1,828–8,384LSB) |
| Finite output, exact settled dry, settled active equality, frozen disabled LFO, pre-event oracle | All60 cases pass; settled null exactly0, stronger than −140dB |
| Existing Pro model tests | **7tests/53,039checks/3.5s/exit0** |
| Existing Free model tests | **7tests/53,039checks/3.6s/exit0** |
| Strict ASan/LSan/UBSan actual-node audit | Completes all60 cases in6.49s; identical measurements and intentional RED exit1, no sanitizer diagnostic through shutdown |

Current normal probe exits **1**. Its equal-power waveform mismatch is genuine
evidence of the implementation defect. The diagnostic bundle also requires both
equal-power and single-linear predictions to pass; therefore its overall exit
cannot be reused as the eventual clarified regression gate. Preserve it intact.
Raw adjacent-sample delta contains ordinary sine slope, and the ideal gain-step
metric contains the intended fade movement. Neither isolates an unintended
switching discontinuity. At48kHz a0.1-peak1kHz sine naturally moves by up to
0.01308062585 per sample. Even identical constant dry/wet paths of0.1 in an
ideal240-sample equal-power fade have first gain increment0.00065235196.
Failing these diagnostic interpretations does not prove the intended no-click
requirement impossible. The earlier categorical conclusion was overstated.

The independent reviewer proposed, **without approval or gate substitution**,
retaining the1LSB24 bound but explicitly defining unwanted switching error
against an independently aligned prescribed transition: `r[n]=y[n]-y_ref[n]`,
`max|r[n]-r[n-1]| < 2^-23`. Keep the separate full-waveform law, duration, A-PED
and exact-endpoint checks: residual delta alone can miss slow error drift and
depends on correct reference histories. The source spec remains unchanged;
this estimator is not newly authorized as acceptance by this documentation fix.

Existing runner hashes: Pro
`20c931a359e6c2c4cec10873037a7f76fd2421e52deccea5c4395771044fa231`, Free
`84764c3adaf185b1b04c078fcc7401699d308637b8ace40c6a3fe9ff79c51b2d`.
Pro log SHA256 `a60e0f0ec0b8f495da18bb55001adeeda355bfc7fb7acbb0def8c9e907b67716`;
Free log `a8c43695531bc7c7a7b46f56002be72b4e36fb59b488fe681b840126536cc29a`.
New final normal audit JSONL SHA256
`59f4a8d9731423f7ebe6818b01bf2adc0bb54ddfa0ba7bb30a8ba8d8a4a84ddd`.

Strict probe binary SHA256
`cd59571a7155be2efc5e8cf07146817cc87c56fd9dfbd6707e8e66720fe2a63f`.
It uses the existing Debug Pro sanitizer configuration, with
`ASAN_OPTIONS=detect_leaks=1:alloc_dealloc_mismatch=1:abort_on_error=1:halt_on_error=1`,
`UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`, `LSAN_OPTIONS` unset.
Its JSONL is byte-identical to the normal audit. Stderr is solely
`JUCE v8.0.10` plus LF (13bytes), SHA256
`caa5dfd38189bd20d3e5d513a789308de87e516a3b9d4e77050e1faa8005df0b`.
The initial collection wrapper returned2 because it expected empty stderr;
the program returned1 for its contract predicates. Raw outputs and both statuses
are retained; the benign banner was inspected without a rerun or suppression.
This is bounded instrumented transition coverage, not a passing acceptance audit,
whole-suite sanitizer qualification, Free sanitizer build or a release gate pass.

## Reproduction and recovery

```sh
source /workspace/luthier-toolchain/env.sh
python docs/coverage/probes/build-phase90-bypass-contract.py build <evidence-dir>
<evidence-dir>/phase90-bypass-contract > <evidence-dir>/contract-red.jsonl
# Expected current-source exit1: preserve it rather than convert it to a pass.

python docs/coverage/probes/build-phase90-bypass-contract.py build-asan <asan-dir>
env -u LSAN_OPTIONS -u LUTHIER_PERF \
  ASAN_OPTIONS=detect_leaks=1:alloc_dealloc_mismatch=1:abort_on_error=1:halt_on_error=1 \
  UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
  <asan-dir>/phase90-bypass-contract > <asan-dir>/contract-red.jsonl
# Same contract RED; inspect stderr separately for sanitizer diagnostics.
```

The helper uses only an already configured build and installed compiler. When
the compile database is absent, it reads existing Ninja rules. It never calls
CMake/configure, downloads or installs. Sanitizer builds keep all safety flags;
ASan globals/UBSan RTTI need real existing routing/AssetStore/cryptography
objects even when their functions are not called. Missing compile database,
PATH and initial link dependencies were ordinary local harness build errors;
logs are retained. No permission/access denial was retried or routed around.

Evidence directory: `/workspace/luthier-evidence/phase90-bypass-20261026/`.
`build-provenance.json` stores exact commands/source/object/binary hashes;
`RESULTS.json` stores parsed predicates without altering the raw RED. Original
Oct25 evidence remains at `phase90-bypass-contract/`, rootSHA
`2fb21cabf26d40a441bb67a8e4be2dd2cb480930`. The new probe adapts that in-project
experiment, retaining its origin; no external implementation was copied and no
license/copyright notice was removed. Historical linear-only diagnostics do not
certify the separate equal-power or24-bit criteria.

Keep approved six Phase90/CE-2/DD-3T/CE-2W/Dyna Comp/**RC-5**; preserve all other
twelve models/specs/tests for updates. No Phase90 registration, source merge,
push, CI, new software, paid compute, credential/security change, release,
environment replacement, denied Actions read, extra denied UI stress or denied
process-control retry occurred. The prior full-suite RED, subsequent focused
fixture passes and all native/manual gaps remain distinct. A further whole-suite
run adds no decision evidence for this unchanged ambiguous model contract.
