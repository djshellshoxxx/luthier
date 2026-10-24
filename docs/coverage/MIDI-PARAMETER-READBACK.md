# MIDI parameter readback: pure mailbox and integration gate

`Midi/ParameterReadbackMailbox.h` is an isolated transport module. Nothing in
Board, BoardModel, PluginProcessor or a UI component publishes or consumes it.
General MIDI parameter persistence, APVTS readback and save-state convergence
remain **unimplemented**. Existing UiEv Activity is presentation feedback, not
an authorized persistence command; its SPSC queue must retain one consumer.

## Record and bounded transport

One audio producer coalesces the latest normalized value independently for each
of `kTargetBoardBase + 16` targets. Capacity derives from the actual slot and
parameter constants, with the existing explicit board reserve. At the current
16 slots and 32 descriptor parameters this is 528 cells. Each cell has seven
atomic 64-bit payload words and a publication sequence; all accesses to shared
payload words are atomic. A plain-data seqlock would contain a C++ data race and
is deliberately not used. Construction requires lock-free uint64 atomics.

The record carries the complete 16-byte UUID, a nonzero model-source generation,
a nonzero restore generation, expected model-control intent, expected resolved
host intent, target and finite normalized value. Strings, borrowed descriptors,
model pointers, heap ownership and truncated/hash model identifiers never cross
this boundary. Action/trigger/momentary records are rejected when classified as
`ReadbackPersistence::Action`; future integration must obtain that classification
from the real target descriptor/command, not guess from the value.

There is one externally serialized control consumer. It takes at most two
snapshot attempts per cell. A successful coherent read acknowledges that
publication only; a concurrent newer publication remains pending. Stale records
are discarded without touching output. Busy reads retain the pending record.
Repeated writes overwrite older unread values, rather than allocating a queue.
Publish/read execute no allocation, destruction, lock, logging or model access.

## Identity and lifecycle rules the engine must provide

These are requirements for future integration, not facilities already added to
the model:

- A model-source generation is an exact off-audio-issued incarnation token for
  the full UUID/model-ID/descriptor-revision association. It must rotate for
  replacement, slot reuse and A-to-B-to-A source changes, including reuse of the
  same UUID. A hash, truncated model string or widened 32-bit plan version cannot
  supply this guarantee. Ordinary edits use separate per-control intent tokens.
- Every accepted full restore advances restore generation before old readbacks
  can be accepted. Undo/snapshot/explicit same-value control edits advance the
  relevant model-control intent under mutation protection. Host persistence
  echoes must not masquerade as a new UI/control intent.
- Resolved host intent must include newer actual host writes, including
  same-value writes and writes arriving while a timer echo is in progress.
  Simply casting a wrapping uint32 serial to uint64 does not make a unique
  incarnation. Zero is reserved for unwritten source/restore generation; full
  counter exhaustion requires a quiescent reset/invalidation policy before reuse.
- Prepare, teardown and producer/consumer replacement require stopped access.
  Reconstructing the mailbox clears publications and consumer acknowledgments;
  no live reset or second consumer is supported. Sequence arithmetic skips zero,
  but the focused wrap helper test does not establish arbitrary full-cycle ABA
  immunity or authorize reusing source generations.

## Conditional persistence is still the blocking design gate

`readMatching` compares a caller-supplied snapshot. It does **not** authorize a
subsequent `applyHostEcho` call: the model or a host writer can change after that
comparison. The engine/model owner must supply a real conditional check-and-
commit under model mutation protection, with a linearization rule for concurrent
host callbacks. A model mutex alone cannot guard a host writer that never takes
that mutex. Checking an atomic host serial, then unconditionally writing the
model or an APVTS parameter, still leaves a race.

The future guard must cover both persistence and any APVTS notification: a stale
MIDI echo cannot erase a later host value in the parameter object, capture
mailbox, serialized host-intent block or board model. Thread-local echo suppression
only classifies synchronous notifications; it does not make these operations a
transaction. No blocking lock may be introduced into the host/audio callback.
If the guard rejects a record, leave the newer host/model intent untouched.

Future acceptance requires actual-processor races: a host write before read,
after read and during conditional commit; accepted restore/model replacement
between read and commit; same-value UI/host edits; source ABA and slot reuse;
wrapped host serials; save immediately after each ordering; and verification that
a restored trigger/kill/capture does not replay. A toy test mutex is not evidence
that these production paths possess the guard.

## Actual pure-module evidence

`RefPedalMidiReadbackTests.cpp` currently registers six cases and 60 checks:
100,000 coalesced publications, zero audio-thread allocations, independent
pedal/board targets, complete UUID/model/restore/control/host token mismatch,
finite/range/capacity checks, action exclusion, skip-zero sequence arithmetic,
wrapped host intent mismatch, and concurrent 100,000 audio publications plus
10,000 simulated restores. The concurrency fixture explicitly owns a test model
mutex; it verifies transport coherence only, not the production conditional
commit described above.

Focused runs pass normally, under ASan/UBSan and under TSan. Logs are
`/workspace/luthier-evidence/midi-readback-{plain,address-undefined,thread}.log`.
The production native suite and actual engine/model/processor integration remain
separate gates; no existing MIDI persistence/UI coverage is marked complete by
these pure-module results.
