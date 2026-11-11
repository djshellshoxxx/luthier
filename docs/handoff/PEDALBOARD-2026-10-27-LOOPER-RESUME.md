# Runtime-only captured-boundary looper resume — 2026-10-27

Code/test/contract **4ceb1b5d339692a1c76eaa6053368d6b9b34fac9**, based on
`70e1abc721b80cdb4b748c634aba7d9b57998df3`. Same executor, sole integrator,
completed independent OpenAI read-only review. No Claude/model API.

E64 requires transport/phase during structural replacement; Architecture.md26
separates persisted loop memory from session transport. Graph now atomically
publishes an available runtime state/head/gain/sample tuple with the asset graph.
`restore(graph, RestorePolicy::ResumeCaptured)` is explicit opt-in. DefaultStopped
keeps prior behavior; schema2 deliberately omits runtime metadata, and decoded
memory cannot acquire resume intent. Recording/Overdubbing remain unavailable.
Invalid policy/state/head/clock/finitegain/Empty metadata reject before clear.

Gain preserves both5ms ramp directions; sample clock preserves timed commands.
Read-ahead primes at captured head through the same bounded prefetch extracted
from existing service. Activation still requires playbackReady. Added runtime
atomics are compile-time guaranteed lock-free for double/int64; actualsizeofImpl
remains charged in admission. Signed clock saturates at INT64_MAX rather than
introducing overflow from restored boundary timestamps; normal arithmetic stays
unchanged. No processing equation/parameter identity/registration is changed.

RED is missing-API compile exit1, preserved before implementation, not an audio
failure. Final focused real-core/codec/RC5 cohort:

| Edition | Result | Time |
|---|---|---:|
| Pro Release |32tests /31,122checks, exit0|0.951s|
| Free Release |32tests /31,122checks, exit0|0.941s|
| Pro ASan/LSan/UBSan |32tests /31,122checks, exit0|2.719s|

Three new cases add2,171checks. Fifteen five-rate/fade-mode cells compare known
nonconstant PCM and exact uninterrupted stereo output at head137, including actual
prepared-depth2→3 change, both ramps, timed Stop inside sub-blocks, and no callback
allocation. Restored cache primes head137 without another process call. I/O waits
are off audio and explicitly isolate phase correctness from cold-I/O scheduling.
Other cases cover invalid Ready/Empty/unavailable metadata without mutation,
normal memory restoreStopped, JSON absent resume authority, empty clock restore,
INT64_MAX boundary, and concurrent snapshots across5,000single-sample callbacks.
The reader verifies the exact head=(sample−192)%192 relationship, gain/state/assets
and availability; this is bounded tuple coverage, not a whole-target TSan pass.

Strict ASAN_OPTIONS=detect_leaks=1:alloc_dealloc_mismatch=1:halt_on_error=1;
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1; LSAN_OPTIONS unset. No findings
through shutdown; stderr only JUCE v8.0.10. Review ran no tests/builds; its suggested
Empty/concurrent checks were subsequently added with final results above. First
31test results remain separately preserved. Existing core/codec predicates stay.
No full product/native/DAW/device/release pass is inferred; the denied extra UI
callback stress remains unexecuted and is unrelated to these core tests.

Evidence `/workspace/luthier-evidence/looper-resume-20261027/`, build.py and run.py
<pro|free|asan>, exact source/module/object/binary hashes, logs/exits, original
missing-API compile and independent review. Installed Ninja tracks dependencies;
final coverage change rebuilt only the test TU and link. No unchanged full suite
was repeated. All predecessor evidence/recovery bundles remain nested.

## Compact continuation states

| State | Work | Owner / next step |
|---|---|---|
| Integrated |Explicit captured-boundary resume helper |sole integrator,4ceb1b5d|
| Reported |Fixed-source six-model completion inventory |55–65%overall effort estimate;65–75%implementation;not measured acceptance|
| Ready/reviewing |RC5 sample-clock double-press Stop/provisional-cancel boundaries |same integrator; frozen down-edge grammar, no destructive held-double policy silently selected|
| Open |Live final-boundary structural handoff |old processor advances during off-audio prepare; helper alone does not solve it|
| Open |Full RC5wrapper/99memory/advanced playback/host; model/native/manual gates |zero selected named registrations; no tolerance weakening|
| Restricted |Source push/CI/merge/release/deployment/new software/environment/security changes |none attempted;denials not retried|

Detailed updated report: LUTHIER-CURRENT-COMPLETION-2026-10-27.md, source4ceb1b5d.
Private native-text Library libfile_685b1640f8448191824388e0b1ad0565 version0,
SHA2566a5022d6777ac83819ba98b3d1b0a851338b68fba675710f17a7b9a86845f1c7.
Six selected/deferred twelve and attribution retained. Recover bundles into
isolated branches preserving local work; no binaries/caches/private intake packs
or credentials uploaded. Actual active count at report boundary: one coordinator,
zero editing workers; completed review. No input/approval/upload wait was pending.
