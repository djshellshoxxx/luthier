# RC-5 prerequisite: real-core history eligibility — 2026-10-27

Code/test commit `ff23891f742d6cd284036776b3ea7edcdc06c577`, same executor,
single integrator and completed OpenAI read-only review. No Claude/model API.

Both RAM and paged cores now expose audio-owner `canUndo()` / `canRedo()`.
They mirror existing apply guards: Playing plus accepted undo layers or redo
layers respectively; unprepared returns false. These are current action
eligibility, not retained history while recording/overdubbing/stopped. Calls
perform only bounded reads: no allocation, mutation, worker service or I/O.
This adds no UI-thread safety guarantee and changes no processing, class layout,
state format, parameter identity or registration.

The RC-5 decoder needs to cancel a provisional overdub before querying history.
Both cores already preserve displaced redo until a pass is accepted; the new
queries expose the resulting eligibility without guessing from undoDepth().
The original test-first compile fails on missing APIs (exit1); that is an API
gap, not an audio failure. The new real-core case then passes1,096checks.

| Configuration | Entire RAM/paged cohort | Time |
|---|---|---:|
| Linux Pro Release |18tests /25,325checks, exit0|0.314s|
| Linux Free Release |18tests /25,325checks, exit0|0.307s|
| Linux Pro ASan/LSan/UBSan |18tests /25,325checks, exit0|0.676s|

The new case uses both real cores and actual paged AssetStore workers. It tests
record/committed overdub/undo/provisional overdub/cancel/redo, exact restored audio,
new commit invalidating redo, Stop/Play/Clear/unprepared eligibility, and128repeated
queries with unchanged state/length/depth/timeline and allocation counts. Reset is
checked after Clear, not independently on populated history; reset code is
unchanged. Existing capacity, queue, capture, cancellation, compaction and bounded
worker/storage tests retain their predicates and pass. Disk readiness waits are
off audio; queries themselves never wait. Generated test files stay in existing
UUID-scoped temporary fixtures. No physical device/DAW/TSan qualification implied.

Evidence: `/workspace/luthier-evidence/looper-history-queries-20261027/`.
Build `python3 <evidence>/build.py pro` (also `free`, `asan`); run
`python3 <evidence>/run.py <edition>`. Only the focused real project closure is
compiled; matching unchanged JUCE modules/caches are reused. Strict environment:
`ASAN_OPTIONS=detect_leaks=1:alloc_dealloc_mismatch=1:halt_on_error=1`,
`UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`, `LSAN_OPTIONS` unset.
No findings through shutdown; sanitizer stderr only JUCE v8.0.10. Missing-API
compiler output/source, first case, final commands/source/module/binary hashes,
logs/exits and scoped review remain. Review executed no tests/builds.

## Current states

| State | Work | Ownership / next step |
|---|---|---|
| Integrated |RAM/paged Undo/Redo availability |sole integrator,ff23891f|
| Integrated |Previous selected-model corrections and source recovery |private predecessor chain;zero named registration|
| Rejected/preserved |Phase90 paired-stage CPU candidate |all10trials still fail<0.4%;no runtime/compiler change retained|
| Ready |Rapid paged Stop→Play restart boundary |real-core test before correction; RAM rewinds immediately, paged only after stop fade|
| Open |RC-5 main-pedal decoder/wrapper/99memory/host/storage/native |existing generic GestureDecoder has different release/deferred grammar|
| Needs contract decision later |Holding the second press which double-Stops: consumed versus later destructive Clear |frozen text does not choose; no destructive behavior silently added|
| Blocked/open |Phase90 CPU/24-bit, Dyna original numerical/CPU, remaining fidelity/soak/platform/manual gates |unchanged thresholds and evidence|
| Restricted |Push/CI/merge/release/deployment/new dependencies/environments/security changes |none attempted;denied operations not retried|

Fresh Stopped press→immediate Play and stopped-origin hold→Clear are both explicit;
a future decoder must preserve press origin. The ambiguous double-Stop-held case
is distinct and does not block ordinary restart correctness. Query eligibility
is only a prerequisite, not completed RC-5 grammar. Selected six/deferred twelve
and attribution stay intact. No full-product/native/plugin/release pass inferred.
The private checkpoint includes source-pinned handoffs, full predecessor chain,
verified base bridge/retained worker refs and checksums; recover into isolated
branches preserving local work. No binaries/caches/private intake archives,
vendor copies or credentials are packaged.
