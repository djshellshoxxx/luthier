# RC-5 immediate press-cycle prerequisite — 2026-10-27

Code/test/contract `e2c4a98d556d2592a2cbc3b0fd9e3bde5301b9c6`, base
`63eac83e3e690c13fc2b3270b45c723759ace606`. Same executor; sole integrator,
completed independent OpenAI read-only review, no Claude/external model API.

Rc5PressCycle implements only §2.1/2.2's immediate down-edge state cycle:
Empty→Record, Recording→Play, Playing→Overdub, Overdubbing→Play,
Stopped→Play. It returns an optional LooperAction from the actual current core
state. Caller applies at its sample boundary. Duplicate held downs return no
action; up/reset change only one bookkeeping flag. No queue, clock, allocation,
I/O, media mutation or transport ownership is hidden in the helper.

The generic GestureDecoder emits on release/later, so is not substituted. A
future full controller must intercept actions for double/hold gestures, preserve
press origin and cancel provisional overdub as required. This helper alone is
not a complete decoder and is deliberately unconnected to the skeleton RC-5
wrapper/registration. Stop-hold Clear and the held-double-Stop policy are not
implemented or silently decided. No numerical/CPU gate changed.

Missing-header compile RED is exit1, preserved before implementation; it proves
an absent API, not an audio defect. Final real RAM/paged cohort:

| Edition | Result | Time |
|---|---|---:|
| Pro Release |21tests /27,069checks, exit0|0.487s|
| Free Release |21tests /27,069checks, exit0|0.459s|
| Pro ASan/LSan/UBSan |21tests /27,069checks, exit0|1.657s|

Two new tests contribute1,384checks. They exercise both real cores at five rates
44.1–192kHz and blocks1/16/512, all five states, duplicate/unmatched edges,
zero-frame close, exact committed overdub PCM, stopped restart, external
transport changes while held, reset non-mutation and unprepared safety. No
allocation change in guarded down/up regions; actual storage-worker readiness
waits happen off audio. Existing nineteen core tests retain all predicates.
Strict ASAN_OPTIONS=detect_leaks=1:alloc_dealloc_mismatch=1:halt_on_error=1;
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1; LSAN_OPTIONS unset. No findings
through shutdown; stderr only JUCE v8.0.10. Review executed no tests/builds.

Evidence `/workspace/luthier-evidence/rc5-press-cycle-20261027/`:
`python3 <evidence>/build.py <pro|free|asan>` then run.py with same edition.
Installed Ninja reuses the previous core cache; only the added RC-5 test TU and
link rebuild (2actions/edition). Logs identify the shared cache binary path and
exact hashes; older checkpoint binary hashes remain distinct. No whole-product,
TSan, hardware/DAW, native-format or release pass is inferred.

## Compact continuation states

| State | Work | Ownership / next step |
|---|---|---|
| Integrated |Immediate RC-5 press-cycle primitive and tests |sole integrator,e2c4a98d|
| Integrated |Paged rapid restart and earlier selected-model fixes |private predecessor chain;zero named registrations|
| Reviewing/ready |Paged capture/codec available-redo preservation |E64 requires preserved redo; reproduce real-core round trip before change|
| Open |Full RC-5 timed grammar/wrapper/99memories/host/storage/native |primitive is only one prerequisite|
| Needs later contract decision |Held second press after double-Stop |no destructive policy chosen|
| Blocked/open |Phase90 original CPU/24-bit; Dyna fidelity/CPU; native/manual/model gates |thresholds and original failures retained|
| Restricted |Source push,CI,merge,releases/deployment,new software/environment/security changes |none attempted;denials not retried|

Selected six/deferred twelve and attribution preserved. Private checkpoint
includes all predecessors, verified base bridge/retained worker histories and
checksums; recover into isolated branches without resetting local work. No
binaries/caches/private intake archives/credentials uploaded.
