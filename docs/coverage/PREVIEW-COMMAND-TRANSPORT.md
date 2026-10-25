# Preview command transport: qualified component, pending processor integration

`Support/PreviewCommandQueue.h` is an internal fixed-storage transport. It is
not wired into `LuthierAudioProcessor` and removes no production callback locks.
The existing whole-processor RT gate remains RED. This deliberately small module
is the first dependency of the preview ownership migration in
`HOST-RT-PUBLICATION-REVIEW.md`, not a completed fix or an acceptance substitute.

## Contract and ordering

Control producers may call note, chord, string/fret and panic methods. A named
`nonRealtimeProducerMutex` serializes these producers only; none of these methods
may run on audio. The one audio consumer calls `drain`. All commands are 32-byte
POD values; no engine pointer, reference, tuning string or ownership object
crosses the boundary. There is no message-manager operation or dynamic storage.
Object construction/destruction requires quiescent producers and consumer.

Two SPSC lanes each hold 256 commands. Note-ons cannot consume release capacity.
Accepted commands receive a shared sequence number. The consumer merges both
heads by sequence, refreshing a previously empty lane after acquiring the other
head: otherwise a later release can be emitted ahead of its earlier note-on.
Per-call work is bounded to 514 iterations and at most the smaller of output
capacity and514 returned commands; zero/null output preserves pending work.

A full attack lane drops the new attack. A full release lane requests an
out-of-band panic epoch; the consumer emits Panic before admitting commands from
that incarnation. Old ring items and cached heads are discarded by the consumer,
never reset by a control thread. A head acquired after the initial epoch load
must first cause an epoch refresh, so a new incarnation is not discarded as old.
Repeated panic requests may coalesce. Epoch/order arithmetic uses64bits; sequence
exhaustion requests a new epoch. Epoch exhaustion publishes terminal epoch0,
rejects further posts and requires a fresh quiescent object to reopen. No integer
incarnation is reused. The arithmetic boundary is unit checked; exercising2^64
full queue transitions is not claimed.

Notes/channels/string indices and finite fret/velocity are validated. Velocity
retains the existing0.05..1 clamp. Chord expansion happens off audio, accepts at
most128 input notes, ignores invalid individual notes and may partially accept
under saturation. Return values report rejected writes; the wrapping32bit drop counter counts
ring-full refusals, not malformed input. StringOn carries the unconverted fret, because reading live
engine tuning on a producer would preserve the current ownership race.

The drain operation linearizes delivery before a concurrent later panic; a panic
cannot revoke commands already returned to a caller. A processor adapter must
apply returned events in order and consume the next panic before rendering its
next slice. The contract does not promise immediate control-thread engine
mutation while audio is stopped.

## Tests and actual evidence

`Tests/PreviewCommandTests.cpp` covers FIFO across lanes, separate release
capacity, release overflow/panic, cached-head invalidation, malformed/chord
bounds, four concurrent serialized producers against one audio consumer, and
1000 stopped-consumer/resume epochs on one queue. Consumer allocation assertions
use the existing per-thread operator-new counter; they do not cover libc malloc
or replace the external callback probe.

The initial preserved implementation failed the fresh concurrency test in both
normal and ASan/UBSan runs. The diagnostic recorded order723 followed by722 in the
same epoch. TSan was silent: the bug was atomic publication ordering rather than
a C++ data race. Refreshing the opposite lane and refreshing epochs before stale
head discard corrects that failure. Earlier fail logs are retained in
`/workspace/luthier-evidence/20261025-preview-command-transport/` alongside exact
build commands and final normal/ASan/UBSan/TSan results: **7 cases /60 checks /0
failures in each mode**. A further25 normal concurrency repetitions passed,
covering1,000,000 producer iterations /2,000,000 post attempts. Shared JUCE core objects
are uninstrumented; the transport, ring, tests and runner are instrumented.

## Bounded production dependency and test plan

1. Establish the engine ownership boundary for conversion. Today string-preview
   producers read `engine.getTuningEngine()` and `getStringMidiNote()` before
   taking previewLock. Audio conversion must run under a valid engine lifetime
   protection, or consume an immutable tuning generation. The existing bridge
   trylock is later in processSlice than the preview merge. Moving conversion
   there would change which events modulation sources observe. Resolve this
   ordering with actual processor tests before wiring the queue.
2. Add one audio-owned adapter tracking the exact emitted note/channel for each
   preview string and held keyboard preview note. StringOff releases that recorded
   note even if tuning/voicing changes, with no producer engine read. Panic
   releases preview-held notes without killing unrelated host MIDI. Define the
   finite held-note bound and reserve release bytes within BoundedMidi; queue
   drain capacity alone does not prove output buffer safety. If insertion fails,
   retain a release obligation for the next slice rather than silently clear it.
3. Route every existing producer through this single queue: string note/release,
   keyboard note/release/chords, audition stop and stopEverythingButTheEngine.
   Preserve offset-zero insertion after MIDI Learn and the existing modulation,
   rhythm/controller-channel and audition-stop behavior. No second queue consumer
   or producer-side ring clear is allowed. Audition phrase publication/retirement
   remains a separate SpinLock ownership migration.
4. Define prepare/reprepare, host suspend, panic and destruction handshakes. Stop
   posts an epoch rather than resetting a live ring. First resumed audio must
   release old preview voices before a fresh attack; object destruction waits for
   actual callback/producers to stop. Inactivity timers do not establish this.
5. Test real processor preview/fretboard, piano/chord and audition flows first:
   exact notes/channels/offsets before and after tuning/source replacement,
   saturated host MIDI plus releases, repeated stop/start, no-message-pump host,
   multiple producers, UI teardown and prepare/reprepare. Force the ownership
   race windows and run ASan/TSan on the actual adapter and producers, not only
   this transport. Preserve all existing normalization/state/audio/host tests.
6. Only after real wiring passes, run a changed-scenario external libc/pthread
   callback probe with preview traffic and release overflow. Demonstrate that the
   preview trylock disappears while reporting every remaining engine/effects
   trylock and unhooked SpinLock. The old3840callback gate remains the baseline;
   an unwired queue is not a reason to rerun it or declare it green.
