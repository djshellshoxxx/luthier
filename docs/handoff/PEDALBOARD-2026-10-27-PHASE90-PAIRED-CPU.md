# Phase90 paired-stage CPU experiment — rejected, 2026-10-27

Production source remains `fca7f440d219e7e37a33c120f159ef65cca48b8f` for this
experiment. The private candidate never replaced repository code or compiler
settings. Same executor, one integrator and a completed OpenAI read-only review;
no Claude/external model API, new dependency or environment.

## Candidate and equivalence evidence

The candidate prepared both channels in originalC0→C1 order, processed four
stages with channel-inner scheduling, then preservedC0→C1 feedback/noise/output
order. It retained source snapshots, target-frequency reuse, each stage's exact
expression association and denormal flush, including same-pointer buffers.
Only the candidate translation unit used `-fno-math-errno`; no fast-math,
finite-only, architecture target or approximate reciprocal flag was enabled.
This flag relaxes errno side effects; changed cross-channel scheduling can also
change FP-trap order. No complete C++ observable-behavior equivalence is claimed.

A renamed actual baseline and actual candidate pass180scenarios across five rates,
blocks1/64/2048, both render entries, three stereo offset modes, distinct/same
buffers, noise/variant/parameter changes, bypass, reset and hostile inputs:
**3,450,240comparisons, zero binary-output/phase errors, max absolute error0**.
The unmodified oracle's printed FRF/THD lines match exactly. Source/header,
compiler commands, binary/module hashes and raw outputs are preserved.

Whole-object disassembly counts show no `sqrtpd` in either object and packed
`divpd` sites increasing1→5. These are not hot-path timing attribution or proof
of a claimed sqrt SIMD speedup. No production compiler change was made.

## Unchanged CPU gate — still RED

The original oracle was reused byte-for-byte:480,000stereo frames at48kHz,
blocks128, five trials each at0°/180°, same environment/toolchain/O3/NDEBUG,
no-fast-math. Model setup/input generation remain outside CLOCK_THREAD_CPUTIME_ID
measurement;10audio seconds makes the printed CPU percentage `seconds*10`.
No benchmark workload, threshold, warmup or machine was substituted.

| Source | Measured process-only CPU range | Original gate |
|---|---:|---|
| Current baseline |0.523460640–0.623894860%|all10trials fail<0.4%|
| Private paired candidate |0.451308340–0.585801760%|all10trials fail<0.4%|

Individual ranges overlap and one candidate180°trial is slower than its paired
baseline. No robust general speedup or CPU acceptance is asserted. The probe
processes exit0 because they report measurements; the separately parsed gate
is explicitly false for both. Every trial/checksum remains in ORACLE-RESULTS.json
and raw logs. No further unchanged probes were repeated. The candidate and its
flags remain private evidence, with **no retained runtime change**.

## Continuation states

| State | Work | Ownership |
|---|---|---|
| Integrated |Prior Phase90 memory/RTA and selected-pedal corrections |sole integrator, previous source-pinned handoffs|
| Rejected/preserved |Paired-stage CPU candidate and errno flag |private only; no source/compiler mutation retained|
| Active |RC-5 real-core Undo/Redo availability queries |same integrator; `Common/LooperCore.h`, `PagedLooperCore.{h,cpp}`, `Tests/RefPedalPagedLooperTests.cpp` working diff|
| Validating |RAM/paged query and provisional-cancel history regressions |first1test/1,096checks pass; broader Pro passed; Free/sanitizer pending at this note|
| Ready |Finish query qualification, then actual RC-5 press/hold controller integration |generic GestureDecoder uses different release/deferred semantics; do not silently reuse it|
| Open |Phase90<0.4%/unresolved24-bit criterion, Dyna original numerical/CPU failures, RC-5 wrapper/99memory/host/native/hardware |original gates preserved|
| Restricted |Push/CI/merge/release/deployment/new dependencies/environments/security changes |none attempted; denied operations not retried|

Evidence: `/workspace/luthier-evidence/phase90-paired-stage-20261027/`.
`python3 <evidence>/build.py` builds the private experiment using matching existing
JUCE modules; `<evidence>/compare` is the output comparison, and the two unmodified
oracle binaries produce the measurement logs. Candidate flags apply only to its
private model object. No sanitizer/full-product/native/host qualification is
claimed for a rejected candidate. Production tests need not be duplicated for
unchanged Phase90 code; independent looper changes receive their own checks.

The private checkpoint preserves the complete earlier chain, original/rejected
sources and measurements, verified source bundles and the published-base bridge/
retained-worker history. Active looper work is separately labeled from committed
source until tested/committed. Selected six/deferred twelve and upstream notices
remain intact;zero named pedals are registered. No binaries/caches/private intake
archives/vendor copies/credentials are packaged or released.
