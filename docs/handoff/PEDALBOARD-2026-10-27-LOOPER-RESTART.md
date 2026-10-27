# Paged looper rapid restart — 2026-10-27

Code/test `526c1f1d2ac8d429c4eed88056502deeb4ca02db`, based on
`cc2081d6d3e1b1f1a090954b60a57e1b466669f0`; same executor, single integrator,
completed independent OpenAI read-only review. No Claude/API use.

RAM Play from Stopped immediately rewinds. Paged Play previously waited for the
Stop fade to reach zero before rewinding, so a rapid new Play resumed the old
cursor. The correction rewinds only Stopped→Playing. Recording/overdub closure,
other transport actions, gain/fades, timeline, history, storage and state formats
remain unchanged. No registration or named-model acceptance follows.

The original-source regression fails90/360checks in1test, exit1. It compares the
actual RAM and paged cores over45cases: five rates44.1–192kHz, blocks1/64/2048,
and stopped intervals0samples/1sample/2ms. Maximum original output mismatch is
0.0205078125. A nonconstant32frame loop fits both prepared cache pages, starts
from head11, then tests immediate restart head0 and both channels over2048frames
(error<1e-15), finite output and no callback allocation. Seams are explicitly zero
on both cores. This tests restart position, not cold-I/O readiness or click gates.

| Focused entire real-core cohort | Result | Time |
|---|---|---:|
| Linux Pro Release |19tests /25,685checks, exit0|0.400s|
| Linux Free Release |19tests /25,685checks, exit0|0.457s|
| Linux Pro ASan/LSan/UBSan |19tests /25,685checks, exit0|1.672s|

Strict ASAN_OPTIONS=detect_leaks=1:alloc_dealloc_mismatch=1:halt_on_error=1;
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1; LSAN_OPTIONS unset. No findings
through shutdown; sanitizer stderr only JUCE v8.0.10. No physical device/DAW,
TSan, whole product, native format or release qualification is implied.

Evidence: `/workspace/luthier-evidence/looper-rapid-restart-20261027/`.
`python3 <evidence>/build.py pro` then `python3 <evidence>/run.py pro`; also free
and asan. Installed Ninja tracks full compiler dependency files and command
hashes. RED initialized the closure; GREEN Pro rebuilt only PagedLooperCore.cpp
and linked (2actions), reusing unchanged objects. No broad suite repeated.
Provenance records actual Ninja output separately from planned commands, source,
module/object/binary hashes, elapsed time and exits. Original source/test/RED,
review and final strict results are retained. No claim of measured wall-time
speedup beyond those command counts.

## Compact continuation state

| State | Work | Owner / next step |
|---|---|---|
| Integrated |Rapid paged Stop→Play rewind |sole integrator,526c1f1d|
| Integrated |History eligibility and prior selected-model corrections |source-pinned private predecessor chain;zero named registrations|
| Ready |RC-5 immediate main-press state cycle |sole integrator; original down-edge clauses, actual RAM/paged test first|
| Open |Full RC-5 gesture grammar, wrapper,99memories, host/storage/native |no release-deferred GestureDecoder substitution|
| Needs later contract decision |Held second press after double-Stop |consumed versus later destructive Clear unspecified; do not silently choose|
| Blocked/open |Phase90 original CPU/24-bit; Dyna fidelity/CPU; other model and platform/manual gates |unchanged requirements/failure evidence|
| Restricted |Source push,CI,merge,release/deployment,new dependencies/environments/security changes |none attempted; denied operations not retried|

The six-model initial scope and deferred twelve remain intact. Preserve all
upstream attribution and original requirements. Private checkpoint carries this
increment plus all predecessors, the verified published-base bridge and worker
histories. Recover into isolated branches without resetting local work. No
binaries/caches/private research packs/credentials are included.
