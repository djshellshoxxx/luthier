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
