# Dyna preparation and processing boundaries — 2026-10-27

Code/test commit `883143b484fd28adb2e6355234f1c3f8f3d142fa`, starting from
`bb38b8687c5386c0e5dc8f80c5f2ac0922cb49b6`. Same executor, one integrator;
OpenAI read-only reviews only. No Claude/external model API.

## Focused correction and verification

The imported estimator validated rate/block but ignored policy. Unsupported
−1/3/INT_MIN/INT_MAX still returned a usable memory requirement. Original RED:
1test/12checks/4failures, exit1. The two-line correction rejects policies outside
0–2 before returning a reservation. Valid preparation/DSP equations, buffer cap,
quality mapping and all original thresholds remain unchanged.

Final combined chorus/Dyna cohort:

| Configuration | Result | Time |
|---|---|---:|
| Linux Pro Release |21tests /83,696checks, exit0|8.868s|
| Linux Free Release |21tests /83,696checks, exit0|8.520s|
| Linux Pro ASan/LSan/UBSan |21tests /83,696checks, exit0|44.488s|

New coverage includes45rate/policy/prepared-quality latency/memory cells:
15/23/0samples as applicable, allocation-free reports/bounds and observed capacity.
The deterministic processing case renders30simulated seconds at each of five
44.1–192k rates:14,049,000stereo frames per route, with blocked1/4/16/64/512/2048
versus single-sample output bit-identical. Fixed-seed parameter schedules,
nonfinite/denormal/±1e6 inputs and repeated resets remain finite/within±16,
with bounded reduction and zero callback allocation-count change.
All five final runs exercise4/4/7/7/14resets respectively; exact frames/blocks,
final RNGs and results are logged. This is150simulated audio seconds per route,
not30minutes or a timed CPU/host/hardware qualification.

The first successful pilot used a4093block reset interval, so low rates only
received the initial reset. Its source/provenance/results remain preserved;
review prompted the final997block interval and explicit repeated-reset assertion.
PF_Config writes in this processing test do not perform structural reprepare;
all prepared quality/policy cases are covered separately. Existing4Dyna and
chorus/memory/mode tests retain their predicates.

Strict environment:
`ASAN_OPTIONS=detect_leaks=1:alloc_dealloc_mismatch=1:halt_on_error=1`,
`UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`, `LSAN_OPTIONS` unset.
No findings through shutdown; sanitizer stderr only JUCE v8.0.10.
All project TUs are fresh with matching unchanged JUCE module objects reused;
caches remain intact. Commands/source/module/binary hashes, RED/pilot/final
logs, exits and scoped review are retained at
`/workspace/luthier-evidence/dynacomp-boundaries-20261027/`.
Build `python3 <evidence>/build.py pro` (also `free`, `asan`), run
`python3 <evidence>/run.py <edition>`.

## Compact continuation state and ownership

| State | Work | Owner / evidence |
|---|---|---|
| Integrated |DD-3T tail scheduling; CE-2/CE-2W memory; CE-2W mode retarget; retained Dyna prototype |sole integrator; prior handoffs/Library chain|
| Integrated |Dyna policy boundary and focused safety checks |code883143b4; results above|
| Active |RC-5 rhythm original−80dBFS DC criterion |sole integrator, `Source/Tests/RefPedalRc5RhythmTests.cpp`; uncommitted next slice|
| Validating |RC-5 initial emitted-audio evidence |default48k/120BPM test reproduces2DC failures; no correction yet in this checkpoint|
| Ready |Smallest reviewed synthesis-only DC correction, then existing rhythm regressions |read-only reviewer; `Common/Rc5Rhythm.{h,cpp}` owned by integrator|
| Blocked/open |Phase90 CPU<0.4%, unresolved/unapproved24-bit metric; Dyna original numerical/CPU failures |unchanged original evidence/thresholds|
| Blocked/open |RC-5 wrapper; all named registration; remaining fidelity/soak/UI/host/native/hardware acceptance |no named acceptance from component checks|
| Restricted |Source push, CI, merge/release/deployment, new dependencies/environments/security changes |current continuation restrictions; no action attempted|

The same-executor disconnect notification was checked with permitted reads at
02:05UTC: expected HEAD/tree and completed exit files were present, no active
build/test. The original imported-Dyna Pro/Free/sanitizer run had completed;
the policy RED also completed, not interrupted. No uncertain test was repeated,
no environment replaced and no access/security denial bypassed.

Five real named DSP prototypes are integrated; zero are registered. Selected six
and deferred twelve remain intact. Original Dyna CPU/fidelity and Phase90 gates
remain open; no source/native/plugin/release pass is inferred from these checks.
The guarded same-identity private checkpoint preserves all predecessor source,
original failures, attribution and a portable Git bundle requiring
`f1efeded706b9dc57da7e941ca856b17eaad5183`. Check manifest/checksums before
isolated import, preserving local work. The active RC-5 working diff must be
preserved separately until its checkpoint; it is not falsely included in this
committed source. No binaries/caches/private intake archives/vendor copies or
credentials packaged; no denied Actions/UI/process operation retried.
