# Pedalboard engine acceptance ledger

Owner: WP2/WP3. Checkpoint 2026-10-23. This is a verified implementation slice, **not complete BOARD_CORE / WP2 / WP3 acceptance**.

## Delivered

- BoardModel applies/undoes/redoes/restores through a private compiler hook before committing state; rejected compilation leaves the model and history intact. Public facade model edits now reach audio.
- Immutable render plans publish through an atomic pointer. Audio borrows raw pointers; message-thread plans own shared pedal objects. Reorder/control publication retains a pedal's instance identity and DSP history. Audio publishes the oldest segment version it can still reference; reclamation runs on the message thread. `releaseResources`/`prepare` require stopped audio.
- Series and 2–4 parallel lanes compile into bounded node programs. Single model order drives all segment/lane traversal. Per-placement dry delay and lane latency equalisation; summed segment latency reporting; per-lane gain/polarity/normalisation.
- Fixed 64-sample scratch, bounded node dispatch, per-slot trims, latency-matched 5 ms enable ramp and tail-silent skip. Callback never reads BoardModel or registry. No callback allocation/destruction is present in this implementation; sustained allocator/lock instrumentation remains a gate below.
- Structural changes dip through dry for 6 ms per half, splitting exactly at the ramp boundary. Repeated edits continue the current fade-out, or restart from the current fade-in gain. Unchanged pedal state is retained.
- Equal-power, linear and third-law board mixing, exact zero/one endpoints, prepared sine table, 20 ms one-pole mix smoothing. Output finite guard and optional zero-lookahead soft limiter.
- Three compatibility prototype models registered through IPedal. Shared stereo phase, parameter smoothing, fixed sample-count coefficient cadence replace per-sample exponent/tangent coefficient calculations. They remain prototypes, not calibrated circuit models.
- Legacy facade shims forward to model edits; empty-slot ordering exists only for the legacy empty card API. Current real instances always use model order.
- UI metadata setter approved as additive interface erratum: `BoardModel::setUiState(juce::var)` stores a clone, no undo or audio publication. Snapshot/getter/restore nested variants are detached.
- MIDI map publication via model listener; router prepare/process/reclaim/time forwarded. MIDI binding/channel commands store detached data. MIDI output events are **not yet applied to pedals** in this checkpoint.

## Actual evidence

Focused native executable built with g++ C++17, JUCE core/audio_basics shared object files and the real Board, BoardEngine, BoardModel, registry, prototype, routing and MIDI sources. Test files: `RefPedalBoardCoreTests.cpp`, `RefPedalRoutingTests.cpp`, `ReferencePedalBoardTests.cpp`.

Result: **36 tests, 56,149 checks, 0 failures**.

Covered assertions include all prior core/prototype assertions plus:

- direct model command changes facade audio; unknown factory rejection preserves history;
- delay impulse placement and exact wet/dry endpoints;
- equal-power correlated +3.01 dB; finite limiter output;
- retained pool pointer identity after reorder/control changes;
- rendering equivalent to <1e-9 across block sizes 1,16,37,64,481,2048, including structural reorder at sample 4096;
- 1000 overlapping order/control/model edits versus a real audio thread, finite output, eventual reclamation (not a substitute for TSan);
- parallel unequal latency impulse alignment and inverted-lane cancellation;
- bypass/mix 0/0.5/1 impulse alignment; PRE/POST segment latency sums;
- UI state variants cannot mutate the model by alias.

Harness and executable used during development: `/tmp/luthier-engine-test/{build.py,tests}` (ephemeral, not a repository deliverable). The three test files are integrated into the existing LuthierTests source list.

## Remaining gates / limitations

- A/B, send/return, feedback and sidechain reamp compile requests explicitly reject; no silent unsupported graph. Their skeleton routing files are not claimed implemented. Stereo width node exists but has no model command yet.
- Board disable currently uses the one-pole mix fade; the specified finite 20 ms disable/trails state machine remains pending. Buffered/DSP bypass models require dedicated trails handling; the present wrapper is latency-matched dry bypass.
- Changing segment latency switches the dry read tap at the structural zero; the specified separate 10 ms read-tap crossfade is pending.
- CPU units are reported but not mapped to measured CPU cap; no automatic Eco governor. No false calibrated cap is imposed.
- MIDI events/foot/transport/raw MIDI and MIDI output drain, audio->model echo, sample-accurate host automation and wah toe requests remain pending. Router time alone is dispatched to pedals.
- Pedal extras loading is performed for newly constructed objects. Reusing an unchanged model with changed extras needs a dedicated safe replacement policy.
- Unknown models pass transparently only when their persisted `missing` flag is set. A standalone BoardModel remains a data model without a DSP compiler; an attached facade validates factories/latency/routing.
- No dual-mono CPU shortcut (both channel states remain coherent), oversampling calibration, CPU benchmark, sustained 10-minute RT probe, ASan/UBSan/TSan, full host build, plugin validation or three-OS run claimed.
- Retired plans remain until all formerly active placement segments advance, or the stopped-audio barrier is called. Host must call all three placement segments per block. No 8-item retire high-water cap/coalescing guarantee yet.

## Integration

Depends on MIDI checkpoint `9334f459` (locally cherry-picked as `cda82f15`): `MidiState::buildMap` and additive `MidiRouter::reclaim`. Integrate that package before this engine commit. No shared host/CMake/parameter files changed here. Existing Board public API is preserved. `prepare` and `releaseResources` are stopped-audio barriers; `reset` queues an audio-thread reset. No push or merge was performed by this worker.
