# Legacy callback ownership review

Read-only review of committed `b5bec374`, not subsequent Parameters edits or
E69 work. No processor, ParameterBridge, engine or vendor source changes are
included. This is an implementation proposal and an open acceptance gate.

## Current ownership and why removing the locks is unsafe

| Shared state | Callback access | Other owner and mutation |
| --- | --- | --- |
| previewMidi | processSlice try-locks previewLock, iterates MIDI then clears it (PluginProcessor.cpp:1842) | Fretboard/string preview, piano note/chord methods, stopAudition and stopEverythingButTheEngine take previewLock while appending or clearing (2580,2650–2705,3096). Producers are public control methods; do not assume every future caller is one message thread. |
| Legacy engine and ParameterBridge caches | processSlice try-locks getEngineLock around applyToEngine, engineCommands and rendering (1948 onward); unavailable ownership clears main audio. applyQualityForBlock also try-locks it (4964). | ParameterBridge::handleAsyncUpdate and applyAllNow take the same CriticalSection while applying structural and scalar changes (Parameters.cpp:2461,2469). Presets, restore, undo, ranges and UI changes reach these paths. |
| Audition phrase publication and retirement | processAuditionMidi uses a SpinLock try-lock to move waiting/active/retired shared_ptrs (2594) | startAudition publishes under that lock (2563); timerCallback takes retired objects under it (4582), then destroys them off audio. |

Try-locks do not wait, but still execute a callback lock operation, so they do not
satisfy a strict no-lock callback gate. The engine lock also encodes real lifetime
protection: its header records the previous ReverbPedal/RoomEngine double-free
when structural rebuilds overlapped render. Deleting the locks, replacing them
with an audio spin loop, or swapping atomic shared_ptrs is not a safe fix.

The existing LuthierEngine::ScopedStructuralChange mechanism is a separate
protocol. setGuitarType/applyWorkshopGuitar enter it; live compatible part swaps
already stage data and retire objects off audio. The park path uses a fade and
atomic swapState, and the message-side writer waits at most250ms. Its timeout
then permits mutation without a proven parked acknowledgement, and its
"audio running" test uses recent timestamps/thread IDs. Neither timeout nor a
quiet recent history proves a future callback cannot overlap. Do not treat that
protocol alone as a replacement for the bridge lock. In addition, engine code
before its swapParked early return touches taps/activity/noise state, while the
processor reads engine data before taking its bridge lock. A replacement must
cover all those accesses, not just engine.processBlock.

## Small independent migration: preview commands

Replace the shared MidiBuffer with fixed POD commands consumed only by audio.
Use a wait-free SPSC ring only after serializing all control producers with an
off-audio producer guard; alternatively use a bounded multi-producer design with
a separately reviewed contention/drop bound. Never reuse a spin-lock command
queue merely because it has a fixed capacity.

- Queue note-on/off, chord note-on/off and string/fret preview commands; merge at
  next-slice offset zero through existing BoundedMidi capacity rules.
- Resolve string tuning/frequency and the actual held preview note on audio.
  Current triggerPreviewNote/releasePreviewNote read live tuning/string state on
  the control side before acquiring previewLock; moving only buffer appends
  leaves those reads outside the ownership solution.
- Reserve release capacity or use a release-priority lane plus an atomic panic
  request so dropped note-ons cannot strand notes. Chord expansion is bounded;
  a huge input array must not cause an unbounded callback loop.
- A reset/panic publishes an epoch/command consumed by audio. Control code must
  not clear the consumer's ring or a live callback buffer. Old-epoch commands
  cannot resurrect a note after reset. A stopped-audio path is explicit.

For audition phrases, retain immutable off-audio construction and destruction,
but replace the SpinLock handoff with a bounded raw-pointer publication/retirement
contract (serialized writers and a single audio reader). Old active phrases must
remain alive until audio acknowledges retirement; queue exhaustion retains the
old phrase or drops a new request, never deletes an object on audio. Avoid
callback shared_ptr destruction or an atomic<shared_ptr> that can hide a lock.

## Legacy engine: minimum safe ownership contract

A full prepared-structure publication is the final design: compile/allocate
replacement body/room/FX/cabinet/string resources off audio, publish immutable
configuration plus owned prepared objects, adopt at a block boundary with bounded
work, and retire displaced allocations off audio after the last reader. Keep
scalar/modulation state audio-owned and feed it from cached parameter atomics or
bounded POD snapshots. Split off-audio preset/default derivation from DSP
application: a state-save flush must not mutate a graph the callback is rendering.
All structural entry points, quality changes and model readbacks need the same
source-generation and lifetime rules. Merely copying scalar fields cannot make
RoomEngine::rebuild or EffectsChain::setSlotType safe.

A smaller interim implementation can preserve today's muted structural transition
without copying the whole engine, but needs an explicit quiescence handoff:

1. Serialize every non-audio structural writer, including host restore, with a
   guard that is never called from audio. Publish a monotonically identified
   request, and keep the active runtime intact while awaiting audio permission.
2. At the outer callback boundary, before any guarded legacy-engine read/write,
   audio announces a single-reader protection and rechecks the request. Use a
   proven protect/recheck handshake (e.g.seq_cst), not a flag checked once followed
   by mutable access. The callback takes a bounded path and never waits or spins.
3. The writer mutates only after acknowledgement proves no protected reader can
   use that runtime and subsequent callbacks will take a defined safe fallback.
   A timeout defers/rejects the request; it never authorizes unsafe mutation.
   An explicit host-suspended lifecycle can provide separate quiescence, but
   inactivity timestamps cannot. Cover commands, quality, sources, metering and
   processor accesses outside engine.processBlock as well as render itself.
4. Publish the completed generation before audio resumes. Hold engine commands
   pending and preserve bounded MIDI release/panic behavior during fallback.
   Retain the specified fade/latency semantics. The writer owns allocation and
   destruction. This interim gate is not gapless prepared-graph publication.

Do not add a callback request which invokes message-manager machinery or allocates
through triggerAsyncUpdate and then call the result RT safe. Audit that path with
the actual callback probe too. The existing scalar no-message-pump semantics must
remain; a structural worker/service contract must explicitly define progress
when no UI/message loop is running.

## Required qualification, not completed by this review

- Actual callback C malloc/free and pthread mutex/try-lock probes over idle,
  preview, audition, quality, structural edits, concurrent restore and automation.
  Report every remaining framework/processor call with its actual stack.
- Forced ownership races: pause audio after reader announcement and before its
  recheck; pause a writer after request/acknowledgement; run stalled callbacks
  beyond250ms; verify no unacknowledged mutation or destruction.
- Concurrent preview producers, saturated release/panic queues, reset epochs,
  UI close/reopen, source replacement, prepare/reprepare and teardown. Check
  exact notes and offsets and instrument allocation/free, not just operator new.
- ASan and TSan against the real processor writer/reader paths, plus unchanged
  native state/normalization/audio fixtures and actual host validators.

No full-processor RT acceptance claim follows from zero allocations in the new
board router, from these source observations or from the existing try-locks.
