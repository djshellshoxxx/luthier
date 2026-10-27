# Paged graph redo preservation — 2026-10-27

Code/test/contract `88f9053f36813b2c5258e9db0afeadf9c01bb217`, based on
`4cfbd8d760faf9e4228a916bd4bd912b94b3c5a6`. Single integrator, completed
independent read-only OpenAI review; same executor, no Claude/API calls.

E64 requires retained redo on structural changes. The old internal graph kept
only audible layers, losing redo on restore. Actual-core regression:15cells
(five44.1–192kHz rates × history depths1/3/8), original22tests/27,444checks,
80failures, exit1. It records/commits layers, undoes multiple passes, captures,
restores into prepared depth8 and redoes in exact original order/audio.

Graph now carries redo hashes/count; atomic publication packs active+redo
references in slots1..8 and retired references9..10. Ready includes every redo
asset's matching epoch/frame/readiness. Restore validates active+redo<=depth,
counts and hashes before clearing the unpublished candidate, restores stack
order/used stream IDs and offsets retired IDs correctly. No extra streams or
processing equation changes. sizeof(Impl) is included in admission; this local
compiler/layout's measured reservation increase is256bytes, not a portable size
formula. Existing stopped/head0 restore behavior remains unchanged.

| Final entire focused RAM/paged/RC5 cohort | Result | Time |
|---|---|---:|
| Pro Release |22tests /28,474checks, exit0|0.649s|
| Free Release |22tests /28,474checks, exit0|0.618s|
| Pro ASan/LSan/UBSan |22tests /28,474checks, exit0|2.034s|

Additional assertions reject negative/oversized/overflowing/over-depth counts,
null redo hashes and Empty-with-redo without altering original audio-owner state,
clock, assets/generation or available history. Existing concurrent capture oracle
now verifies complete redo assets and bounds; original predicates remain.
Strict ASAN_OPTIONS=detect_leaks=1:alloc_dealloc_mismatch=1:halt_on_error=1;
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1; LSAN_OPTIONS unset. No findings
through shutdown; stderr only JUCE v8.0.10. Read-only reviewer ran no tests.
No TSan, native/full-product, physical-device/DAW or release pass is inferred.

Evidence `/workspace/luthier-evidence/looper-redo-graph-20261027/`: build.py and
run.py <pro|free|asan>, installed Ninja full dependency/command tracking, actual
source/module/object/binary hashes/logs/exits, original RED and source, review.
Only affected TUs/link rebuild; previous cache remains. All predecessors retained.

## Continuation states

| State | Work | Ownership / next step |
|---|---|---|
| Integrated |Internal available-redo graph |sole integrator,88f9053f|
| Active |JSON redo schema migration |codec still omits redo; separate RED70checks reproduced, not closed by this commit|
| Ready follow-up |Gated redo-worker Pending/Ready/failure; actual JSON→core redo PCM |bounded existing-store tests, no new infrastructure|
| Open |Full E64 transport/phase, final wrappers and named acceptance |restore remains stopped/head0;zero named registration|
| Blocked/open |Phase90 CPU/24-bit, Dyna fidelity/CPU, native/manual gates |original thresholds/failures retained|
| Restricted |Push/CI/merge/releases/deployment/new software/environment/security changes |none attempted;denials not retried|

This is an internal prerequisite, not end-to-end persistence acceptance. Codec
migration will make supported redo explicit and preserve true future schemas.
Six selected/deferred twelve, attribution and original failures remain intact.
Private checkpoint includes predecessor recovery chain; no binaries/caches,
private intake archives or credentials. Recover without resetting local work.
