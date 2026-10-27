# Redo worker publication coverage — 2026-10-27

Test source `6e5ac69ead71ff08c5fdc5711e861b33d2914e43` plus reviewed oracle
correction `f36eb2d05f759f3e9b6f56437fc479741ae58a64`, based on
`03dd7ca0afad2f6225e87fdbd7ae6c600d4cc262`. Single integrator and completed
read-only OpenAI review; same executor, no Claude/model API calls.

Coverage-only: production already passes. No new defect or runtime fix claimed.
Four cells use actual AssetStore plus the existing gated worker fixture:
44.1/192kHz × successful/failed writes. After base Ready, the new overdub write
blocks; Undo makes that incomplete asset redo-only. Capture remains Pending,
with no completed hashes/counts and no caller allocation. Releasing the worker
resolves Ready or Failed without another audio callback; Ready includes the redo
asset, Failed includes I/O fault and no claimed completed assets. Both terminal
states roundtrip through the codec with matching redo/status.

Review caught that initial polling could miss an intervening Ready in failure
cases. The follow-up checks every successfully observed polling snapshot for
forbidden Ready or partial nonReady assets. Original initial/terminal predicates
remain. First29tests/28,947checks results are preserved separately; final:

| Entire focused real-core/codec/RC5 cohort | Result | Time |
|---|---|---:|
| Pro Release |29tests /28,951checks, exit0|0.640s|
| Free Release |29tests /28,951checks, exit0|0.635s|
| Pro ASan/LSan/UBSan |29tests /28,951checks, exit0|2.103s|

Strict ASAN_OPTIONS=detect_leaks=1:alloc_dealloc_mismatch=1:halt_on_error=1;
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1; LSAN_OPTIONS unset. No findings
through shutdown; stderr only JUCE v8.0.10. No TSan/full-product/native/device/DAW
or release pass inferred. These are authorized real storage regressions, distinct
from the separately denied extra UI callback stress; that task remains unrun.

Evidence `/workspace/luthier-evidence/looper-redo-worker-20261027/`, build.py and
run.py <pro|free|asan>, retained dependency-tracked Ninja cache. Only changed test
TU plus link rebuilds; exact source/module/object/binary hashes/logs/exits and
first/final results retained. Review itself ran no tests/builds.

| State | Work | Owner / next step |
|---|---|---|
| Integrated |Redo worker publication coverage |sole integrator,f36eb2d0|
| Ready |Runtime-only captured transport/head/gain/clock restore opt-in |default preset restore staysStopped; metadata separate from schema2|
| Open |Live final-boundary structural handoff and final wrappers |helper snapshot alone cannot catch old core advancing during preparation|
| Open |RC5 timed grammar/99memories; selected-model fidelity/CPU/native/manual |zero named registrations; original thresholds/failures retained|
| Restricted |Push/CI/merge/release/deployment/new software/environment/security changes |none attempted;denials not retried|

Architecture separates persisted loop memory from session transport. E64 still
requires structural replacement transport/phase continuity; an explicit resume
policy can address the helper prerequisite without inventing preset auto-play.
Six selected/deferred twelve and attribution remain. Private checkpoint retains
all predecessors/base bridge/worker histories; no binaries/caches/private intake
archives/credentials. Recover without resetting existing work.
