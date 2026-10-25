# Pedalboard loopers coverage (WP15/WP16)

Transferred baseline: a3ceccaa (bounded layered core) and 30eeb3f7 (bounded reset
queue drain), imported into the state worker as 47a8513d and 6a49a0e8.

## Unlimited overdub core increment

The core now separates a speculative scratch layer from the configured 1..8
retained undo layers. Committing beyond that count retires the oldest accepted
layer; each audio frame folds retired samples into its base in chronological
order before reuse. A preallocated epoch pool tracks accepted/retired/discarded
samples by reference count. This matters for partial/canceled overdubs: canceled
scratch samples never become base audio, and an aborted overdub at the undo limit
does not remove the previous undo layer.

Every buffer and epoch record is included in `memoryBytes()` and the supplied
byte budget. Zero-byte preparation fails without retained storage. The tradeoff
is more bookkeeping RAM and a shorter RAM recording capacity for the same budget.
Gestures and clear/reset have cost bounded by the fixed layer/queue counts,
independent of loop duration. Per-frame folding scans at most nine storage layers;
there is no audio callback allocation or full-loop copy.

Focused verification: **6 cases, 23,050 checks, 0 failures**, in normal and ASan
runs. Existing timestamp/undo/budget tests remain green. Added 300 partial accepted
or canceled overdubs, 50 cancel-at-capacity/undo cycles, 1,000 zero-frame canceled
gestures, stale-audio reset, zero-byte budget, and an allocation-counter test with
100 overdub/commit/cancel/undo/redo cycles. The allocation counter reports zero.
`-Wall -Wextra -Wpedantic` syntax checking is clean.

Reproduce and inspect evidence:

```sh
bash /workspace/luthier-evidence/loopers/run.sh
bash /workspace/luthier-evidence/loopers/run-asan.sh
```

The ASan harness instruments core and tests; reused JUCE core is not instrumented.
This is core acceptance only. Named wrappers/registration, 11/99 memories,
concurrent audio snapshots, worker spill/persistence, reverse/half speed, rhythm,
MIDI clock master, and full crossfade seam modeling remain subsequent increments.
The existing seam algorithm attenuates loop edges rather than overlapping two
unmodified regions; it must not be represented as a completed full crossfade.
The original PhraseLooper prototype remains intact until integration is verified.

## External PCM pager increment

`Common/LooperStorage` provides append/read paging through IAssetStore. The audio
owner uses fixed pools of write/read pages and a bounded SPSC work queue; it does
not allocate, lock, wait, signal a condition variable, or perform file I/O. The
worker polls its queue, writes little-endian float32 mono/stereo pages, verifies
asset hashes, and publishes an external binary manifest. Only its content hash
needs to enter host state, so long recordings do not consume the 256 KiB JSON cap.
Metadata/cache/worker scratch reservations must fit the configured RAM budget;
PCM plus page/manifest headers must fit the storage budget. Preparation rejects
insufficient budgets and retains the previous instance rather than truncate the
requested duration. The RAM figure excludes OS thread stack reservation and
independent IAssetStore internals; storage reservation describes the active loop,
not global garbage collection of content-addressed historical project assets.

At 48 kHz stereo / 90 minutes / 4096-frame pages:

- Capacity: **259,200,000 frames**, with no duration truncation.
- Reserved working data/scratch: **6,261,616 bytes**, within 32 MiB.
- Maximum active PCM/page/manifest payload: **2,076,890,704 bytes**, within 4 GiB.

Read-ahead supplies playback pages. A cache miss returns zero wet samples and
sets readUnderrun; failed/missing/corrupt assets set ioFailure. Exhausted write
pages reject further recording immediately with writeOverrun and retain the
accepted frame count. A caller can retain dry audio while reacting to these
flags. Wrapper-level automatic transition to PLAY remains integration work.
Manifest output is unchanged on failure. Restore validates a bounded manifest
before mutation and waits on an off-audio queue barrier; a blocked old write
causes a one-second timeout that preserves the previous loop. Old jobs cannot
later overwrite the restored caches. Both asset-path verification and bounded
page/manifest reads stream from files to avoid loading arbitrary-size assets.

Focused pager verification: **9 cases, 876 checks, 0 failures** in normal, ASan,
and TSan runs. The tests use real on-disk AssetStore plus a controllable mock gate
for deterministic slow/failing reads/writes. They cover 90-minute capacity,
exact reservation limits, mono/stereo page/manifest restoration, float32 fidelity,
missing pages, malformed manifests, write overrun, read recovery, atomic restore
timeout, and zero audio-owner allocation. Reused JUCE modules are not sanitizer
instrumented; the pager, tests, and AssetStore are instrumented.

```sh
bash /workspace/luthier-evidence/loopers/run-storage.sh
bash /workspace/luthier-evidence/loopers/run-storage-asan.sh
bash /workspace/luthier-evidence/loopers/run-storage-tsan.sh
```

This is a shared storage primitive, not full looper parity. Remaining integration
includes sparse/random overdub pages, worker compaction of retired accepted
layers, cache version handoff during compaction, actual pedal state machines and
registrations, persistent 11/99-memory ownership, UI meters, and MIDI/rhythm.
Existing RAM-core tests do not substitute for paged-processor duration tests.


## Sparse overdub pages and cache versions

`beginLayer/add/canAdd` now supports fixed-length sparse deltas, arbitrary punch-in
positions, and accumulating input over repeated loop wraps. Unwritten pages are
implicit silence and do not create audio assets. Two cache ways preserve the
previous immutable page while its replacement is read ahead. Page epoch/revision
are published as one atomic value, preventing old worker results from becoming
current after a clear/re-record. A storage overrun closes the recording and
finalizes accepted pages once the worker recovers.

New focused result: **10 cases, 1,987 checks, 0 failures**, normal, ASan and TSan;
the allocation hook covers append, sparse add, reads and prefetch. Seven wraps
starting at frame 23 preserve and restore exact binary-fraction deltas. Updated
90-minute / 48 kHz stereo reservation: **7,031,232 bytes** working RAM and the
same **2,076,890,704-byte** active storage bound. Full-duration preparation still
refuses insufficient budgets rather than reducing capacity. Warning-enabled
syntax checking remains clean.

Off-audio tests explicitly wait for page availability when simulating normal
worker service. Audio methods themselves never wait; deliberate slow-worker
fixtures still demonstrate immediate overrun/underrun reporting. Retired-layer
worker compaction and its output/version handoff remain the next integration
step, followed by named processors.

Atomic manifest capture checkpoint: `capture(Capture&)` uses bounded atomic loads and leaves the output unchanged after eight contended attempts. Epoch/phase/frame rereads reject mixed generations; a single writer sequence protects four atomic hash words. Empty, Recording, Pending and Failed captures have null hashes; Ready alone carries the current generation manifest. Tests gate actual writes to verify Recording→Pending→Ready and new-generation/failure hash redaction, and concurrently capture 100 recording generations. Normal/ASAN/TSAN storage suite: 12 cases / 2345 checks / 0 failures; the final acquire-fence and capture allocation-hook additions were rerun under TSAN. JUCE shared objects remain unsanitized. This is the storage primitive; named model `saveExtra` integration remains outstanding.

Worker retired-layer compaction checkpoint: immutable finalized layers merge into base one page at a time between pending read/write jobs. Per-page revision/token metadata identifies whether a returned cache page already contains the retired layer; old cached pages remain usable with the explicit retired contribution. Worker completion releases the source lifetime hold; audio acknowledges only after removing that contribution from its playback/state graph. Capture remains Pending until acknowledgment. Epoch changes cancel old writes without publishing into a new recording. Failed merges report Failed/null hash rather than claiming an old manifest is latest.

Normal/ASAN/TSAN: 18 cases / 4749 checks / 0 failures. Includes 100 repeated sparse merges, partial/wrapping layers, blocked first/second page handoff without double counting, source destruction during blocked merge I/O, cancellation into a new take, failed-store capture, and allocation-counted merge enqueue. Warning-only syntax compilation is clean. Current single-stream 90min/48k stereo reservation: 259200000 samples/channel, 8109920 bytes RAM and 2076890704 bytes active external storage. Each stream currently owns one worker thread; OS thread stack and independent AssetStore overhead are excluded from this primitive figure and must be included separately by the aggregate transport budget. Unreferenced historical blobs are not globally garbage collected. Shared paged transport and named models remain unimplemented at this checkpoint.

Fixed-stack worker/aggregate-budget support: LooperStorage now uses JUCE Thread with an explicit 256 KiB stack and charges it in its reservation. A release/acquire worker-exit handshake supplements the framework wait before destroying instrumented storage fields. Linux tests query the actual worker stack via pthread_getattr_np and verify exactly 262144 bytes. `reservation(Config,ram,storage)` validates without allocating, and `available(frame)` probes cache readiness without introducing a false underrun. Normal/ASAN: 19 cases / 4759 checks / 0 failures; final worker-exit handshake rerun under TSAN: same 19 / 4759 / 0, no sanitizer warnings. Single-stream 90min/48k stereo reservation including requested stack is now 8372448 bytes; the 64 KiB allowance already covers allocator/guard overhead, while the independent shared AssetStore's worker is outside this per-stream reservation. Linux stack verified; macOS/Windows pass the same explicit size through JUCE but are untested here.

Paged transport checkpoint (`Common/PagedLooperCore`): chronological bounded commands and exact sample recording boundaries; stereo dry path, mono averaged recording option, 5ms transport gain ramps, undo/redo/cancel, oldest-layer worker compaction, explicit pool/queue/store overload and wet-silence underruns. Aggregate reservation charges every `undoDepth+3` stream, metadata/cache/temporary I/O allocations and fixed 256KiB worker stacks before allocation. Full requested capacity is refused on insufficient budget rather than shortened.

Verified configurations (bytes are aggregate per prepared processor; independent Board AssetStore is shared and excluded):

| Configuration | Frames/channel | Streams | RAM bytes | Active storage bytes |
|---|---:|---:|---:|---:|
| Generic 90min / 48k / stereo / depth8 | 259200000 | 11 | 92099320 | 22845797744 |
| Native RC5 90min / 192k / stereo / depth1 | 1036800000 | 4 | 54315488 | 33184181488 |
| Ditto 5min / 192k / mono / depth1 | 57600000 | 4 | 9541472 | 924525264 |
| RC1 12min / 192k / stereo / depth1 | 138240000 | 4 | 20261792 | 4430700160 |

RC5 tested at 44.1/48/96/192k with minimum 64MiB RAM cap: `writePages=4,readPages=14,readAhead=12`, pageFrames8192/8192/16384/32768 respectively. All preserve full90min and ≥2s lookahead (2.048s at48/96/192k). These are transport budgets for all scratch/history streams, not claims that model memory inventory/storage limits are implemented.

Atomic capture publishes stream IDs, expected epochs and immutable references. Any-thread capture checks only atomic data, resolves completed worker I/O without another callback, and rechecks descriptor sequence. A frozen pre-compaction base plus two retired-layer manifests keeps saves mathematically consistent during partial compaction and preserves the undo boundary on restore. In-progress first recording/current overdub emits explicit Recording/Pending without stale hashes. Same-core prepare/restore/destruction cannot overlap capture; the engine's pinned runtime/message-thread lifecycle supplies this barrier. Restore is for stopped unpublished processors; external failure marks Failed rather than claiming a partial processor is Ready.

Normal, ASAN/UBSAN and TSAN: 9 cases / 979 checks / 0 failures, no sanitizer reports. Includes exact timestamps and dry output,20 compacted overdubs, undo/redo/cancel,100 concurrent generation captures with repeated restore, blocked-write auto-close, failed-store dry-through, save during blocked compaction and restore after all I/O completes with no further audio callback, plus allocation-counted callbacks/capture. Latest input validation for malformed Empty/retired hashes is a small follow-up to these runs and will be covered in the next codec/wrapper suite. Evidence scripts/logs: `/workspace/luthier-evidence/loopers/run-paged{,-asan,-tsan}.sh`.

Remaining parity work: named wrappers/registration and sample-counter foot gestures; model extras codec/unknown-field overlay; Nano11/RC599 inventories and boundary memory load; RC5 rhythm/MIDI (rhythm transferred to coordinator); converter colour/latency and rate-conversion of restored assets; full overlap seam crossfade (current option remains documented edge attenuation); graceful snapshot of an unfinished recording/current overdub; global asset garbage collection/project relocation. No whole-engine audio/UI/model acceptance is claimed by these isolated core harnesses.

Provisional press/hold redo correction: RAM and paged transports now retain redo while an overdub is provisional. Cancellation restores that history; accepting a new take discards it. RAM uses the existing spare buffer via constant-time swaps, with no additional allocation. Regressions cover two undo entries,20 provisional partial cancellations followed by redo, and acceptance replacing redo; paged covers the same gesture contract. RAM normal/ASAN:7 cases /23179 checks /0 failures. Paged normal/TSAN:10 cases /1050 checks /0 failures (also covers the malformed Empty/retired input guards added at the previous checkpoint). This correction is required for down-edge tap plus hold-to-redo semantics in upcoming wrappers.

Model extra-state codec checkpoint: `Common/LooperStateCodec` validates/encodes the model-owned `extras.looper` subtree, with hash-only base/undo/retired references, exact int64 generations, resource metadata and explicit Empty/Recording/Pending/Ready/Failed statuses. Unknown nested fields clone unchanged; newer integral schema remains opaque (`Stored.forward`, caller must keep it out of DSP), and encoding preserves the whole future tree. Unfinished states cannot claim an older completed hash. Known asset fields never emit file paths. Decode/encode failures keep previous outputs. Normal and ASAN/UBSAN:4 cases /139 checks /0 failures, including malformed asset/type/range atomic failures, duplicate content-addressed audio hashes, nested precise doubles, and future schema preservation. No named wrapper currently consumes this helper; MIDI/other top-level extras must be preserved by overlaying only the `looper` key.


E64 graph/rebuild checkpoint: atomic capture now includes the active undo assets, redo stack, published transport and exact playhead. Legacy schema1 assets with no redo/phase fields remain readable. Reducing selected undo depth preserves audible older layers as retired worker-compaction inputs and keeps the nearest allowed redo; increasing depth does not eagerly allocate eight streams for default-depth instances. A replacement can reserve the larger restored history explicitly, within the aggregate budget. Once settled, a later save/rebuild can release that temporary capacity. No discarded undo history is claimed recoverable beyond the selected depth.

Restored unpublished processors synchronously prime the actual configured read-ahead window off audio before admission, with a single one-second deadline across the whole pool. Failed/missing/slow assets reject that replacement; callback processing never waits. Published state uses fixed lock-free atomic words and sequence rechecks, including the extra redo references. Per-frame wrapper processing is bracketed by once-per-block service/publication; transport gain is a five-millisecond raised cosine. The existing seam control remains edge attenuation, not the required full overlap crossfade.

Exact aggregate logical reservations after the larger graph publication (not RSS, no allocator bookkeeping): 90min/48k stereo depth8/11 streams92100152 RAM bytes and22845797744 active asset bytes; RC5-like90min/192k stereo depth1/4 streams with2.048s read-ahead54316320 RAM and33184181488 asset bytes; Ditto-like5min/192k mono depth1/4 streams9542304 RAM and924525264 asset bytes; RC1-like12min/192k stereo depth1/4 streams20262624 RAM and4430700160 asset bytes. Each stream includes its256KiB worker stack, caches, metadata and bounded I/O scratch. AssetStore's own worker and historical content-addressed blobs are separate; historical asset GC is still absent. These configurations demonstrate capacity accounting, not named-model acceptance.

Verification: storage19 cases /4761 checks /0 failures, including a deliberately blocked read proving waitAvailable returns false without readiness, then succeeds after unblocking; paged core12 cases /1237 checks /0 failures under normal, ASAN/UBSAN and TSAN, including1→8→1 selected-depth rebuilds while undone, exact audio, nearest redo, immediate restored phase, concurrent capture and callback allocation probes. State codec4 cases /154 checks /0 failures normal and ASAN/UBSAN before its final exact existing-property setter; the setter uses the separately sanitizer-verified board codec pattern and native model sanitizer coverage also executes it. Shared JUCE module objects are uninstrumented. Evidence: /workspace/luthier-evidence/loopers/e64-{storage-full,paged-final-normal,paged-final-asan,paged-final-tsan}.log.

Measured hot-path correction: overdub page prefetch occurs when acquiring the writable page rather than on every sample; wrapping the known in-range playhead uses a branch. Existing multi-page sparse-overdub, compaction and slow-store tests retain exact results. The focused Linux O3 native Ditto4096-frame mono overdub benchmark (106.666667 audio seconds) used0.205639 thread CPU seconds,0.192787%, versus0.317582% before this change with identical checksum and zero faults. This single local hot-loop measurement does not establish cross-platform or full-engine CPU acceptance; descriptor0.3% remains the declared spec budget, not a calibrated measurement.


Shared sample-clock foot gesture helper checkpoint: immediate down,10ms debounce, double gap measured from first release, held threshold crossings and remembered pre-press transport; release at the exact threshold resolves the hold once before the next sample render. A short bounce release is always accepted, so debounce cannot invent a held switch. Model timing values are explicit configuration. Six cases /55 checks /0 failures, including48k/44.1k thresholds, stale/negative input, double expiry, configurable timing and1000 allocation-counted hold sequences. The native Ditto sanitizer tests additionally exercise the helper. Evidence: /workspace/luthier-evidence/loopers/gesture-final-normal.log.

Final graph codec ASAN rerun after the exact existing-property setter:4 cases /154 checks /0 failures, no diagnostics (/workspace/luthier-evidence/loopers/e64-codec-final-asan.log).


Native Ditto factory checkpoint (deliberately NOT added to RegisterLoopers or the shipping catalog):12 reconciled parameters and five lane mappings; physical footswitch transport independent of slot engagement; immediate mono recording with bit-exact stereo dry;5-minute bounded paged capacity; configurable undo depth, speculative-take cancel, undo/redo/clear gestures; virtual beat/bar start/end quantization, transport follow and stop-at-end;10ms loop/dry gain smoothing; real wet-only0..256 converter delay with zero reported dry latency; actual transport/playhead/fault/tail meters. Model saveExtra owns only extras.looper and captures the atomically published asset graph; unsupported newer schema remains opaque with dry-through and an explicit fault. Missing/incompatible/unfinished assets throw off-audio for the engine candidate-admission rejection path. Selected-depth rebuilds use the E64 graph semantics.

The wrapper requires the Board-owned IAssetStore service; it never invents a filesystem path. Its E69 constructor-safe non-media estimate and actual report charge sizeof(TcDitto)+sizeof(the heap-owned Core facade); the entire Core pimpl/media pools, queues, metadata, worker stacks and caches are separately reported by reservedBytes. Project JSON heap is outside the engine prepared-DSP accounting; a256KiB state allowance is additionally subtracted from this model's32MiB pager budget. The descriptor declares300 CPU units at1000 units/percent (0.3%), not a measured calibration.

Native factory harness:8 cases /5166 checks /0 failures under normal, ASAN/UBSAN and TSAN; no sanitizer diagnostics, no callback allocations, clean -Wall -Wextra -Wpedantic syntax compilation. Uses isolated registration of the test-target factory, NOT proof of actual catalog registration or whole-engine acceptance. Evidence: /workspace/luthier-evidence/loopers/{ditto-test,ditto-asan-full,ditto-tsan-full,native-warnings}.log. Includes native1→8→1 rebuild while undone, retained audible/redo assets and vendor data, thresholds at exact release samples, double cancellation, quantized2000-frame bar, real wet delay/instant dry and forward/missing assets.

Remaining required Ditto gates before catalog registration: actual board return-tap service with the specified DC block/gain/runaway handling; full overlap seam crossfade and3ms overdub punch fades (current seam option remains edge attenuation); offline sample-rate conversion of stored assets; graceful snapshot of unfinished recording/current overdub; normative full300s capacity render,1000-cycle analytical overdub,100 random bit-exact undo/redo, complete sample-rate/block matrix, frequency/THD/zipper measurements and30-minute seeded gesture soak. The unavailable return-tap extension explicitly faults and refuses new recording/overdub instead of substituting ordinary input. Existing accepted audio can still play/stop/undo. Nano360/RC1/RC5 wrappers and99/11 memory inventory remain subsequent work; coordinator-owned RC5 rhythm helper is ready separately. No hardware-equivalence or feature-complete claim is made.


Extended acceptance checkpoint (test-only): the native Ditto factory completes1000 accepted1000-frame overdubs with dyadic input and verifies every playback sample against the analytical sum, zero faults, exact retained loop length, then exact audio before/after undo and redo of the final layer. Adds6002 checks. Native suite now9 cases /11168 checks /0 failures normal and ASAN/UBSAN. Core additionally executes100 seeded random undo/redo commands including legal no-ops at history boundaries; each action checks every output sample bit-exactly and published active/redo counts. Adds3608 checks. Core suite now13 cases /4845 checks /0 failures normal and TSAN, no diagnostics. Evidence: /workspace/luthier-evidence/loopers/{ditto-thousand-normal,ditto-thousand-asan,paged-random-normal,paged-random-tsan}.log. This closes the bounded1000-cycle analytical and100-command random-history coverage at the tested1000Hz/1000-frame and1000Hz/32-frame fixtures; full44.1..192k sample-rate/block matrix, real-duration300s render, spectral/zipper gates and30-minute randomized soak remain unproven. Named catalog registration remains blocked by the listed functional requirements.

## Completed-asset sample-rate conversion checkpoint

`Common/LooperAssetConverter` converts immutable completed base/active/redo/retired
asset graphs during stopped off-audio restore. It streams verified little-endian
float32 pages through the supplied store, with at most five cached source pages;
PCM never enters host JSON and no extra worker is created. Sparse absent pages
are silence. The periodic 64-tap Blackman-windowed sinc is normalized per sample,
uses cutoff 0.45×min(1,target/source), and supports rate ratios 1/8..8 (all 25 pairs
among 44.1/48/88.2/96/192k). This is an explicit offline engineering filter, not
hardware converter coloration. Compatible same-rate/page-format assets remain
hash-exact after verification. Converted length/playhead use nearest-sample time
scaling; history order, transport, generation and opaque codec fields survive.

Native Ditto now accepts such cross-rate completed state, including load before
prepare. It converts into unpublished content-addressed assets, restores a new
core, and swaps only after success. Failed conversion or candidate admission
retains the previous running core and stored template. Missing, corrupt,
nonfinite, malformed or unwritable assets reject off audio without path leakage.
The previous fallback rejection for unfinished takes and same-rate incompatible
page/channel formats remains explicit; none is relabeled as completed audio.

Reservation covers both manifest vectors, five source cache pages, an extra
source-page allowance, encoded output page, fixed control state and 64KiB I/O
allowance. Both conversion scratch and simultaneous old/new pools are charged
against the instance 32MiB media cap; old/new external reservations are also
charged against 4GiB. Converter scratch is released before replacement allocation.
For full 300s mono44.1→192k base+undo conversion: 734,164 scratch bytes and 462,262,616
new active asset bytes. Existing default-depth 192k core reservation is 9,542,304 RAM
and 924,525,264 active storage bytes; simultaneous old/new default cores require
19,084,608 RAM and 1,849,050,528 active storage bytes, plus the existing wrapper/state
allowance. High-depth replacements may reject if simultaneous pools cannot fit;
capacity/history are never shortened to force admission. Shared IAssetStore
internals, allocator bookkeeping and historical/orphan content-addressed blobs
remain separately owned; this is logical reservation, not RSS or global GC.
E69 nonmedia owned report remains sizeof(wrapper)+sizeof(heap core facade), with
an additional sizeof(Core) temporary facade allowance for transactional restore.

Original RED: the added completed-loop cross-rate wrapper test failed at accepted
restore (3 checks/1 failure), while the nine original cases remained green.
After implementation, focused converter 5 cases/201,895 checks pass normal and
ASAN/UBSAN. Covers 25 stereo host-rate pairs, duration/phase, independent channel
content, constant history graphs, low-tone fidelity, 30kHz alias rejection for
96→44.1k, exact budget boundaries/no writes on rejected budgets, store failure,
missing/hash-corrupt/structurally corrupt/nonfinite assets, hostile counts and
same-rate hash identity. Native Ditto 11 cases/16,315 checks pass normal,
ASAN/UBSAN and TSAN; includes redo/unknown preservation, before-prepare restore,
atomic failed restore and the existing callback allocation trap. Shared JUCE
objects remain uninstrumented. Warning-enabled syntax checking is clean.

All owned normal regressions were rerun: RAM core 7 / 23,179; storage 19 / 4,761; paged
core 13 / 4,845; state codec 4 / 154; gesture 6 / 55, all with zero failures. Evidence:
`/workspace/luthier-evidence/loopers/rate-final-results.json`, original
`rate-original-red.log`, `rate-conversion-final-{normal,asan}.log`,
`rate-ditto-final-{asan,tsan}.log`, and `rate-reservation.log`. A test-only corrupt
asset cleanup initially re-queried its deliberately invalid content hash and got
an empty path (correct AssetStore behavior); that RED is preserved, and cleanup
now retains the already verified temporary file path. Product checks were not
weakened.

This closes bounded completed-asset rate conversion for the tested formats and
budgets, not named-model registration or full original T1–T14 acceptance. Full
300s conversion/render performance, complete 20Hz–20kHz migration FRF/THD and
near-Nyquist transition-band measurements, 30-minute soak, actual return-tap,
full overlap seam/overdub punch fades and unfinished-take snapshots remain open.
The default native playback path is unchanged by the offline converter. No Mac,
whole-board, hardware comparison or feature-complete claim is made.

## T1 full capacity and corrected T8 migration filter

Independent DSP review found a blocking bandwidth defect in the e6430df5
prototype: the old 64-tap cutoff was 19,845 Hz whenever either rate was 44.1k.
Actual asset RED at the original thresholds:42 checks/8 failures;20k gain was
−7.822655dB for 44.1→192k and−6.405690dB for 192→44.1k, with additional upsampling
image/noise failures. `rate-fullband-original-red.log` and the exact original
source/test/binary under `fullband-original-e643` are preserved. The original
±0.01 dB 20 Hz–20kHz and THD+N<−140dBFS limits were not weakened. The old far 30k
alias fixture alone did not qualify near-Nyquist behavior.

The converter now uses beta 19 Kaiser-windowed periodic sinc, pass edge 20k for
supported audio rates and stop edge at the smaller Nyquist. Odd source support
scales with transition width and source rate, with 25% margin on a 180 dB design
estimate. That estimate is not a measured universal rejection claim. Rational
P/Q phases are precomputed and charged to the reservation; quotient/remainder
sample indexing avoids accumulated long-recording phase error. Decoded PCM
caches scale with kernel/page support and clip every contiguous fragment at the
actual partial page/loop end. No whole recording or extra worker is allocated.
Unsupported large phase tables reject before asset writes instead of exceeding
the budget. The finite contract has been updated to supersede the old filter.

Actual asset T8 sweep: 500 coherent cases (20 frequencies from 20 Hz through20k,
all 25 pairs among44.1/48/88.2/96/192k) at 0.125 peak input. Worst absolute gain
error 2.31e−7dB; highest fitted harmonic/image/noise residual −171.159753dBFS.
Both use the unchanged original limits. Just-over-destination-Nyquist inputs
22,060/22,150/22,500Hz from 192→44.1k produce approximately−180..−182dBFS RMS;
these are reported diagnostics, not an invented new acceptance threshold.
DSP's independently pinned review checks rational phase, cache lifetime,
partial-page bounds, table/cache accounting and Bessel-series accuracy, finding
no blocking implementation defect at cpp SHA4e195d9194afe1dc5fa9962282ef8184d96c1b7d26be73e99d169b4768be7de4.
It also distinguishes the ideal-tone acoustic oracle from the separate scalar
implementation consistency check.

The explicit long fixture (outside the default test registry) exercises real
AssetStore files with unique tone+dither pages. T1 passes at every supported
rate: 13,230,000/14,400,000/26,460,000/28,800,000/57,600,000 frames, respectively.
It verifies Recording and exact length at capacity−1, automatic PLAYING at
300*rate, no extension by the next sample, and actual playback from a restored
full-capacity asset. Callback generation is paced outside the callback on
completed file writes; no audio-path waiting or timing oracle was introduced.

Full 300 s mono44.1→192k base plus undo conversion also passes: 57,600,000 output
frames, retained layer and PLAYING state, 2,365,288 reserved converter scratch
bytes, 462,262,616 converted active payload bytes. The scratch is released before
old/new default pools coexist (19,084,608 RAM bytes plus the existing wrapper/
state allowance;1,849,050,528 active pool storage bytes). Actual final/peak
committed store: 34,590 files, 568,438,600 content bytes and 709,410,816 Linux
allocated bytes; at most 450,048 additional bytes for one atomic temporary blob.
Filesystem contents are checked against the store observer. An injected store
failure after two output pages leaves the previous full graph unchanged. Eight
long-range positions in each base/undo stream match a separate scalar Python
reference bit-exactly as float32 (16 checks), including page crossings and the
last two samples. This is long-phase/indexing consistency, not a substitute for
T8 acoustics. All owned large asset directories were removed after success;
only source, hashes, numeric summaries and logs are retained.

Initial explicit long run completed in 162.096 seconds. The final run strengthens
rollback assertions to include playhead, transport and all history counts and
completed in 144.274 seconds; its full conversion/rollback/validation
phase took 65.8964 seconds. Both full runs produce identical recorded/converted
asset hashes and bit-exact scalar sample checks. They ran on the contended shared
Linux machine, bounded by a 1200-second outer process limit and 180-second per-T1
deadline. These are observed offline elapsed times, not realtime callback CPU
qualification or promised cross-platform conversion times. Final product source
and explicit fixture/binary hashes are pinned in status.json; the earlier run is
preserved under initial-162s-run. All large owned asset directories were removed.

Final focused results: converter 7 cases/203,402 checks normal and ASAN/UBSAN;
native Ditto 11 cases/16,315 checks normal, ASAN/UBSAN and TSAN, all zero failures.
No sanitizer diagnostics; shared JUCE objects remain uninstrumented. The existing
callback allocation probe remains green. Final warning-enabled syntax checks and
Python compilation are clean. These instrumented short fixtures do not claim
that the full 300 s run or a 30-minute soak ran under every sanitizer.

Evidence under `/workspace/luthier-evidence/loopers/`: `kaiser-fullband-summary.json`,
`rate-fullband-kaiser-matrix.log`, `full 300 s-kaiser/{status.json,run.log,
independent-reference.log,repeated-hash-consistency.json}`; independent review at
`/workspace/luthier-evidence/dsp-looper-kaiser-review-4e195/REVIEW.md`.
The committed explicit fixture is `Source/Tests/Fixtures/ReferencePedals/
DittoLongAcceptance.h`, with `verify_ditto_long_samples.py`; it deliberately
requires a fresh caller-owned directory and removes only that directory.

This closes the tested full-capacity and bounded completed-asset migration
spectral/duration gates. Catalog registration remains blocked: real return-tap,
full overlap seam and overdub punch fades, unfinished-take snapshots, complete
native model parameter/spectral/zipper/block matrix and 30-minute randomized
callback/CPU acceptance still require their original gates. No Mac, whole-board,
hardware comparison or feature-complete claim is made.
