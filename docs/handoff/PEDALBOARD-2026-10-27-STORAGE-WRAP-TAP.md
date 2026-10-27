# Fresh-cache wrap correction / RC5 tap estimator — 2026-10-27

Combined code **2d313c64c0390d7c0e0a9641afbe3c26a53c0233**. Storage source
4d6f0cc7c7c558c112c2f36cb93d898bfc50b394 integrated first, then tap helper/tests/
contract. Base43b2b9a3c112f669d7fd5fc2969f3e16a74405e2. Same executor, sole root
Git integrator; disjoint stronger storage worker and workhorse tap worker wrote
only assigned files and private evidence, no Git writes. Storage received fresh
read-only concurrency review and root review; tap received root source/test review.

## Confirmed forward wrap defect

Original modulo mapping aliases same-version pages8/0 in9page loop/readPages8.
Actual restored storage and restored PagedLooperCore at head128 need[8,0,1,2].
The recorder is destroyed, avoiding retained write buffers masking the bug.
A controlled actual AssetStore gates only worker reads until prefetch is queued;
owner manifest reads remain unblocked. Original RED first2tests/309checks/2fails;
expanded original-source RED3tests/461checks/3fails. Original binaries/source/
commands/hash manifests and both results remain. New-only GREEN3tests/2,998checks;
success paths naturally execute more PCM assertions after failed readiness gates.

Production diff only LooperStorage.cpp: eight-byte per-page owner readSlot[2]
lookup hints. cached/read remain constant-time with unchanged epoch/index/version/
merge-token/publication-token checks. Miss assignment is bounded within the same
version-parity lane, protecting every page in the entire circular read-ahead
window. Old/new compaction revisions remain in separate lanes. No PCM buffers,
cachecounts,readAhead,public API,readiness predicate or budget cap are changed.
FIFO worker jobs retain copied metadata and tokenCAS; failed queue pushes leave
retryable tag0. Stale hints after eviction/epoch reset validate out. sizeof(Entry)
charges all extra metadata before allocation. Tested192k90min/depth1 aggregate
55,328,256bytes is below the unchanged64,000,000byte board cap.

| Separate original storage/paged/core/state/resume closure | Result | Time |
|---|---|---:|
| Pro Release |52tests /37,495checks, exit0|2.159s|
| Free Release |52tests /37,495checks, exit0|2.144s|
| Pro strict ASan/LSan/UBSan |52tests /37,495checks, exit0|4.352s|

All original compaction/stale-revision/source-lifetime/cancellation/queues/capture/
state/resume assertions retained. New full8slot window and three complete9page
loops check exact distinguishable PCM, in-flight eviction and zero allocations.
Sanitizers do not establish complete race freedom; longer stress/performance
remain separate. No standalone/product/plugin/native/fullsuite pass inferred.

## RC5 four-interval internal tap tempo

New Rc5TapTempo implements existing§4.4, distinct from board ClockSync's weighted
three-interval policy. Arithmetic mean of last four accepted adjacent intervals,
200..1500ms inclusive; timeout>=2seconds resets history/current anchor. Every
monotonic nonnegative tap advances the adjacent anchor, including invalidinterval;
negative/backward clocks reset without emitting tempo. Valid prepare resets;
invalid prepare preserves prior valid state. Integerceil/floor bounds and guarded
nonnegative subtraction avoid boundary/INT64 overflow. Fixed4slot storage, no
allocation/locks/I/O; optional fullprecision40..300BPM does not choose clockauthority.

Separate tap cohort: Pro/Free/strict ProASanLSanUBSan each10tests/361checks, exit0.
Elapsed runtime not independently logged; no invented duration. Tests cover120BPM
x8 at five rates, exact nonuniformlast4mean, interval/timeout boundaries, anchor
semantics, invalidprepare/reset/clocks includingINT64_MAX and callback allocations.
Actual unchanged Rc5Rhythm matches a manually derived fullprecision tempo sequence
bit-exactly across37/257 partitions; phase never resets at tempo writes and an
analytical phase oracle passes. Rounding to UI0.1BPM yields a distinct reference,
so internal precision is actually exercised. No149case unchangedDCmatrix rerun.
No wrapper/host-clock/master-slave integration or registration is claimed.

Both use detect_leaks=1,alloc_dealloc_mismatch=1,halt_on_error=1;
UBSANhalt_on_error=1,print_stacktrace=1; LSANunset. No diagnostics through shutdown;
ASan stderr onlyJUCE8.0.10. Exact source/dependency/module/object/binary hashes,
commands/logs/exits/patches/RED in evidence. No combined62test-product claim:
these are separate focused binaries. No third-party notices/history were removed.

Evidence and local reproduction:
`/workspace/luthier-evidence/storage-wrap-20261027/` build.py/run.py<pro|free|asan>,
baseline mode retained; unique cache-j2. Tap evidence copied as text/source/logs
into tap-tempo-evidence; original /workspace/luthier-evidence/rc5-tap-tempo-20261027/
build.py<edition>,then<edition>/tests, uniquecache-j1. Peak build concurrency3,
within verified4core quota; no extra VM or new software.

## Current states / next prerequisite

| State | Work | Ownership / next step |
|---|---|---|
| Integrated |Forward restored-cache wrap fix |4d6f0cc7, original3failure evidence retained|
| Integrated |RC5 internal tap estimator |2d313c64, no clock-authority/wrapper claims|
| Ready |Partial-final-page minimum read-ahead coverage |nominalP*A/rate is insufficient; stronger geometric oracle beforepolicychange|
| Broker reserved |Phase90CPU/Fable;CE2BBD-onTHDtest/Sonnet |exact native packets in previoushandoff; no local duplicateimplementations|
| Open |RC5reverse/half/99memories/fullgrammar/livehandoff/click gates |selectedmodelstillunregistered; no destructivepolicy silentlyselected|
| Open |OriginalmodelDSP/CPU/RT/full/native/manual/releasegates |all thresholds/failed evidence preserved|
| Restricted |Push/CI/merge/deploy/release/newsoftware/security/credentials |not attempted; deniedoperationsnotretried|

Read-only primary-manual review succeeded: [official RC5 reference,p9](https://static.roland.com/assets/media/pdf/RC-5_reference_eng01_W.pdf#page=9)
confirms reverse prevents later overdub entry. Enabling reverse during existing
OD and mid-play phase anchoring remain unspecified; no runtimechoice made.
Half-speed stays an explicitly labelled project extension.

The old nominal2s geometry assertion remains unchanged in this checkpoint.
For192k90min/P32768/A12, a partial final page makes worst coverage380,929frames
(1.984s), despite nominal393,216frames(2.048s). General partial-page cases can
be shorter. Correct cache liveness does not close this separate policy gap.
Shared64Mbudget is unchanged; future wrapper must use a proven coverage policy.

Existing native feature report stays pinned4ceb1b5d (55–65%overall/65–75%code
qualified effort ranges); later code is not silently folded into that estimate.
Owner-authorized Claude source sharing remains only via existing broker; no direct
Anthropic calls/keys/bindings/infrastructure here. Six selected/deferred twelve,
first-party/upstream attribution and all predecessor source/recovery bundles remain.
Private checkpoint excludes binaries/caches/private intake packs/credentials.
Recover isolated without resetting local work; no release assets overwritten.
