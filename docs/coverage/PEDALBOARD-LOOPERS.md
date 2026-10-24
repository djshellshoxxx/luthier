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
