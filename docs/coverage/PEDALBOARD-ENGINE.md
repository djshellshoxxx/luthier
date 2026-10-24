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

Result: **38 tests, 56,155 checks, 0 failures**.

Covered assertions include all prior core/prototype assertions plus:

- direct model command changes facade audio; unknown factory rejection preserves history;
- delay impulse placement and exact wet/dry endpoints;
- equal-power correlated +3.01 dB; finite limiter output;
- retained pool pointer identity after reorder/control changes;
- rendering equivalent to <1e-9 across block sizes 1,16,37,64,481,2048, including structural reorder at sample 4096;
- 1000 overlapping order/control/model edits versus a real audio thread, finite output, eventual reclamation (not a substitute for TSan);
- parallel unequal latency impulse alignment and inverted-lane cancellation;
- bypass/mix 0/0.5/1 impulse alignment; PRE/POST segment latency sums;
- UI state variants cannot mutate the model by alias; changed DSP extras replace runtime identity while MIDI metadata does not.
- Global allocation counter reports zero allocations across 100 callback scopes with structural edits, including beginBlock/three placement calls/endBlock.
- No-tail board disable stops after exactly 960 samples at 48 kHz (20 ms), then passes input bit-exact.

Harness and executable used during development: `/tmp/luthier-engine-test/{build.py,tests}` (ephemeral, not a repository deliverable). The three test files are integrated into the existing LuthierTests source list.

## Remaining gates / limitations

- A/B, send/return, feedback and sidechain reamp compile requests explicitly reject; no silent unsupported graph. Their skeleton routing files are not claimed implemented. Stereo width node exists but has no model command yet.
- Board disable now uses an exact 20 ms enable envelope and continues processing declared tails with muted input until silent. Buffered/DSP per-slot bypass models still require dedicated trails handling; the present wrapper is latency-matched dry bypass.
- Changing segment latency switches the dry read tap at the structural zero; the specified separate 10 ms read-tap crossfade is pending.
- CPU units are reported but not mapped to measured CPU cap; no automatic Eco governor. No false calibrated cap is imposed.
- MIDI events/foot/transport/raw MIDI and MIDI output drain, audio->model echo, sample-accurate host automation and wah toe requests remain pending. Router time alone is dispatched to pedals.
- Changed DSP extras now construct a fresh runtime (MIDI metadata is excluded from DSP identity); preparation precedes extra-state loading. Pool preparation allows High policy so model quality configuration is not capped at Normal.
- Unknown models pass transparently only when their persisted `missing` flag is set. A standalone BoardModel remains a data model without a DSP compiler; an attached facade validates factories/latency/routing.
- No dual-mono CPU shortcut (both channel states remain coherent), oversampling calibration, CPU benchmark, sustained 10-minute RT probe, ASan/UBSan/TSan, full host build, plugin validation or three-OS run claimed.
- Retired plans remain until all formerly active placement segments advance, or the stopped-audio barrier is called. Host must call all three placement segments per block. No 8-item retire high-water cap/coalescing guarantee yet.

## Integration

Depends on MIDI checkpoint `9334f459` (locally cherry-picked as `cda82f15`): `MidiState::buildMap` and additive `MidiRouter::reclaim`. Integrate that package before this engine commit. No shared host/CMake/parameter files changed here. Existing Board public API is preserved. `prepare` and `releaseResources` are stopped-audio barriers; `reset` queues an audio-thread reset. No push or merge was performed by this worker.

## Event/automation checkpoint (2026-10-24)

**43 tests, 56,350 checks, 0 failures** in the same focused harness (full header dependency rebuild).

- MIDI parameter, board mix/tempo/enable, transport and foot events dispatch at sample offsets. Pedal parameter overrides persist across blocks until a model change. The engine uses frozen descriptors only; no audio-thread registry/model access.
- E41 approved additive `IPedal::footControlsEngagement()` defaults true; loopers override false. `consumeEngageToggle()` defaults false and lets a wah request board-owned engagement. Foot engagement uses velocity-dependent 2–12 ms ramps; bounded SPSC engagement echo drains from `Board::reclaim`, coalesces by slot, verifies stable identity and baseline, then writes the model outside its mutex. Toe requests are consumed after a sub-block (at most 64 samples), not falsely advertised sample-exact.
- E43 approved additive `BoardModel::setMidiFocus(const InstanceId&)`: validates UUID/null, clones MIDI metadata, notifies map listener; no undo step or audio topology publication.
- E44 approved `HostAutomationSnapshot` and audio-only `Board::setHostAutomation`: 323 fixed raw physical values + serials. Layout: board enabled/mix/BPM, then enabled/16 descriptor lanes/input dB/output dB/position per slot. Position remains structural/off-audio. Serial zero is reserved for never-written; producer skips zero at wrap, consumers compare inequality. Host values apply once per new serial and persist; unchanged serials do not overwrite later model edits. Storage reuse/model replacement consumes old-source serials without applying the previous occupant's controls. A new host serial is needed after such a replacement.
- New tests verify sample-offset gain change, persistent MIDI override then model edit, foot transport isolation, initially-off MIDI wake, immediate host writes without a message pump/timer, serial persistence/wrap, stale slot-source rejection, and zero allocation with a host update.

Remaining event gates: raw MIDI and pedal MIDI output drain; parameter/board-value UI echo (engagement echo is implemented); bounded split-point coalescing (current exact offsets can exceed the spec's 32 distinct boundaries); UI Trigger pulses coalesced by snapshot publication; full foot mode/pressure policies. Host writes are sampled per block, not independently per every 64-sample interval inside a large host block. Board-level control model echoes still need generation arbitration. The preceding initial-checkpoint statement that no routed events are applied is superseded by this section.

Dependencies added: MIDI E36/E39 commit `5e77e3d0` (local `8cd3d77b`) and approved IPedal sidechain interface `7d91967c` (local `3157bf6a`). Neither dependency is part of this worker's scoped implementation commits; coordinator should integrate their originals first.

## Sidechain/reamp checkpoint

**44 tests, 56,356 checks, 0 failures**. `Board::setSidechain(const float*,const float*,int)` borrows the actual host input slice until endBlock. The executor converts only the current <=64 samples into fixed double scratch and supplies fresh pointers/count to every pedal; absence supplies null/zero. EndBlock clears borrowed pointers. The meter's atomic `sidechainAvailable` reports the most recent input availability.

Reamp settings now compile. PRE input adds or replaces with the connected sidechain, with the configured gain; it is injected once even when the first pedal is POST. A disabled input bus has zero effect, as required by BOARD_CORE 6.8. Source latency relative to the instrument remains uncompensated. Test verifies stereo replacement and disconnect through a board whose only pedal is POST. Host bus capture/copy is owned by WP23 and must call the hook before beginBlock. Runtime reamp source/gain switching does not yet have an independent input crossfade.

### Control intent and actual telemetry checkpoint (E47/E48/E50)

- Compiled controls carry per-control write revisions. An explicit same-value model command supersedes an older MIDI/host override. Board enabled/mix/tempo use the same revision mechanism; descriptor parameters, engagement and trims have independent revisions.
- Approved E50 adds message-thread `BoardModel::applyHostEcho` for scalar persistence, without undo entries or new audio write revisions. A stale timer echo can update persistence but cannot overwrite the current audio value. A fresh host serial wins when received in the same block as a model revision. Serial zero is reserved; producers skip it at wrap, consumers compare inequality. Slot identity/model changes consume old host serials without transferring values to the new occupant.
- Approved E48 sends the immutable model enabled revision to MidiRouter before routing; the router applies model intent before a fresh host serial. Requires host/MIDI dependency a9ba046c plus the follow-up that prevents map-only baseline updates from overriding revision-aware routing on host echoes.
- Approved E47 adds audio-only `IPedal::readUiMeters()` returning an availability-tagged fixed POD. The engine publishes full UUID, engagement, measured model tail and available transport/loop/tempo/progress/fault fields through lock-free atomic snapshots. UI `Board::readPedalMeters(slot)` retries at most three times and returns unavailable on contention. Models without an implementation report no available model fields; no loop progress or CPU usage is fabricated.
- Native focused suite: **46 tests, 56,383 checks, 0 failures**. New regressions cover stale host persistence, same persisted-value explicit writes, simultaneous model/new-host precedence, no host-echo undo, actual telemetry payload, concurrent snapshot coherence, removal and slot replacement identity. Existing callback allocation probe still passes with telemetry enabled.
- Pending: model-specific meter overrides belong to DSP/state owners; router map-baseline follow-up belongs to host/MIDI owner. UI trigger pulses, raw MIDI delivery/output, advanced routing and the previously listed gates remain open. No new sanitizer result is claimed in this checkpoint.

### Send/return, UI edges and bounded MIDI checkpoint (E52/E53)

- Send/return now executes real fixed-bus nodes in Series/Loop mode. Serial and KillDry return only the loop bus; Parallel linearly blends latency-matched dry and loop, keeping a correlated identity loop at unity. Send/return trims apply independently; disabled loops return delayed dry, mute loop input and retain a 5 ms blend transition. Two disjoint loops are supported. Missing/reversed endpoints, overlapping loops, cross-placement loops and loops in branched modes reject atomically.
- UI Trigger/Momentary writes use a fixed 256-entry SPSC edge queue, in addition to the immutable latest snapshot. The audio thread delivers accepted edges in order to the matching UUID/model, including rapid press/release before the next block. Drop-new overflow is counted; consumed edges do not repeat. A pending edge waits for its matching retained runtime during a structural transition.
- Approved E52 adds `Board::drainMidiOut(MidiEvent*,int) noexcept`. Short raw input messages are copied into a fixed 256-entry block queue, delivered at their block offsets only to `wantsRawMidi` models while the board is enabled. Every pedal output drain is capped at 64 events per process slice; model output offsets are relative to that immediately preceding slice. The engine translates them to beginBlock-relative offsets and inserts into a stable, sorted 256-entry output queue. Overflow drops new events; undrained previous-block output is discarded at the next beginBlock. All drops contribute to BoardMeters.midiDropped. No host MidiBuffer is grown by this API. The existing endBlock overload stays compatible; the caller owns the final host-buffer capacity boundary.
- Approved E53 adds `BoardModel::cancelTransaction()`: restore and republish the outer baseline, restore its prior undo/redo (including preexisting redo), and clear all transaction nesting. No active transaction returns false. Rejected restoration leaves the active transaction unchanged.
- Corrected MIDI engagement ramp to 25..2 ms for press velocity and 12 ms for release with unknown velocity. Legacy sparse16-slot compatibility and test-registry isolation were delivered separately in da61b859.
- Focused native suite: **52 tests, 57,615 checks, 0 failures**. New evidence includes all loop kinds/enabled states with impulse latency17, endpoint/overlap atomic rejection, three rapid UI trigger pulses without repeat/allocation, nested cancellation/history, raw MIDI exact offsets17/72, disabled-board MIDI inertness, allocation-free fixed output drain, sorted output and deterministic overflow. Concurrent reclamation test now creates a deterministic retired plan after the worker joins; it no longer assumes the concurrent publisher has left unreclaimed work.
- Remaining: AB routing/Dual aux, feedback routing/safety, model-specific raw MIDI behavior, universal host-output capacity guarantees, bounded32 split-point coalescing, CPU governor/calibration, dry latency tap crossfade, and previously listed gates. No claim that all WP2/WP3 gates pass.
