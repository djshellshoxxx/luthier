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

### AB and auxiliary outputs checkpoint (E54)

- AB mode now compiles two latency-matched lanes and uses a real 10 ms equal-power angular crossfade for A/B/A+B. A+B uses equal-power coefficients (correlated lanes sum +3.01 dB). Repeated explicit Toggle commands alternate A/B; unrelated model publications retain the sticky selection. Lane polarity and trims remain active.
- Approved E54 introduces `BoardAuxOutputs` with optional `pedalB`, `wet`, `dry` stereo float pointers and sample counts. `Board::setAuxOutputs` borrows pre-cleared caller buffers for one block. Each placement adds its own aligned segment sends; summed sends sanitize nonfinite values and clamp to +-16. Dual sends A to main and B only to the Pedal B bus, with no foldback if the bus is absent. Wet/dry aux taps are independent of board mix; disabled boards do not send. Availability is exposed by three explicit BoardMeters booleans. No pedal is rendered twice.
- Host must append optional buses after existing buses, clear their buffers, supply slice-relative pointers and call setAuxOutputs before beginBlock. Host wiring is owned by WP5; the engine API never allocates or retains pointers after endBlock.
- Focused native suite: **54 tests, 58,495 checks, 0 failures**. Tests verify all480 samples of an A-to-B crossfade, sticky toggle across unrelated publications, Dual main behavior with absent B bus, delayed A/undelayed B alignment, all3 sends accumulating PRE+POST, availability and zero callback allocations.
- Prior frozen checkpoint950dbf93 also passed **ASan+UBSan, leak detection enabled:52 tests,57,615 checks**. Engine, router and tests were instrumented; shared JUCE module objects were not. The standalone allocation counter needed a matching nothrow-new overload; no sanitizer suppression was used.
- Remaining advanced routing gate is feedback plus its mandatory safety mechanisms. Host aux integration,32 split-point coalescing, CPU policy and previously listed gates remain unclaimed.

### Feedback safety checkpoint (E56)

- Feedback compiles actual return-before-destination/send-after-source nodes. Endpoints must exist, destination precede source, and both share a placement/routing lane. Used loop references and feedback references are also validated in a standalone BoardModel, so malformed restores reject before any state/history mutation.
- The return uses a fixed64-sample stereo ring independent of host/subblock partition. Gain clamps to0.98 unless runaway is explicitly enabled (then max1.05), with a5ms ramp. Send processing applies lowpass, a2.0-ceiling tanh limiter,20Hz DC removal, denormal flushing and finite guards. Nonfinite feedback clears its state and latches fault. The pre-limiter loop-energy RMS watchdog trips above+12dBFS for>200ms, ramps gain to0 over50ms and latches until OFF→ON rearm. Final feedback memory is bounded to+-2; segment output retains the BoardLimiter/finite guard.
- Approved E56 adds momentary FeedbackSettings.kill and SetBoardParam("fbKill",0/1), preserving configured gain and ramping feedback gain to0 over5ms. Compiler intent revisions retain short kill pulses and OFF→ON rearms across coalesced publications. Persistence must serialize kill=false (state worker b1684072). UI/MIDI/extended-host control wiring is separately owned and remains an integration gate.
- Focused native suite: **57 tests,99,429 checks,0 failures**. Evidence: exact feedback delay64 and gain clamp; host partition sizes1/16/37/64/481 agree sample-for-sample; kill preserves configuredgain; direct watchdog stress proves bounded ring, trip/ramp and no silent rearm on endpoint edit; injected NaN produces silent fault latch; OFF→ON clears; standalone invalid endpoint/order/cross-placement restore and removal reject atomically.
- This checkpoint adds feedback core without claiming the extended host/MIDI control surface, CPU governor,32-point event coalescing, per-mode buffered/DSP bypass, dry-delay tap crossfade, or runtime-extra capture are complete.

### Runtime extras capture checkpoint (E55)

- Persistence `BoardModel::snapshot()` now copies stored state under its mutex, then invokes the engine's private capture hook outside that mutex. The message thread pins retained runtimes by shared ownership and overlays only matching UUID/model saveExtra data, preserving unrelated top-level extras. Callback teardown clears the hook before engine destruction. Callers must follow the existing message-thread BoardModel contract; this is not an audio-thread save API.
- Internal topology and legacy-facade snapshots use stored model data, so ordinary reorder does not turn a live recording snapshot into a new pool key. Prepare captures before release, loads captured extras after prepare, and keeps the stored-model identity/key. Configuration replacement captures the active matching runtime rather than resurrecting an older cached runtime. An explicit changed extras payload remains authoritative. Release retains only detached captured metadata for the next save/prepare, not a live DSP object.
- Model owners must implement saveExtra using atomically published data. Current production Delay/DD3T fixes are timefx e97fa570; upcoming looper wrappers use the published storage Capture status/hash contract. The engine does not read audio buffers or invent a completed asset when storage is pending/failed. These model-specific dependencies are required before integration.
- Focused native suite: **58 tests,107,520 checks,0 failures** (concurrent capture check count depends on scheduling). The capture fixture publishes an atomic frame count; repeated saves leave stored extras/history/topology unchanged, prepare and explicit release/prepare preserve it, configuration replacement/undo preserve it, concurrent audio/save reads are monotonic, and reorder/undo retain the same live state.
- Persistent IAssetStore installation/lifetime is approved separately as E59 and remains the next integration gate. Real looper pending/failed IO evidence belongs to the storage/model owner; this engine test verifies capture isolation and lifecycle rather than asserting completed file IO.

### Extended host scalar checkpoint (E57)

- Board::setExtendedHostAutomation consumes the shared 50-field physical-value/serial packet. Immutable plans compile separate per-field model intent revisions; the audio-owned values apply model intent first, then fresh host serials. Repeated serials persist, serial zero is unwritten, and comparison by inequality permits producer wrap with zero skipped. Explicit same-value model writes override older host values; applyHostEcho persists scalar values without creating intent or undo entries. Structural fields remain validated message-thread commands.
- The renderer uses live scalar values for mix law, reported latency, limiter, trails, MixTail, lane gain/polarity, A/B selection, loop enable/blend/trims, feedback enable/gain/filter/runaway/kill, and PRE reamp. The -120 dB host endpoint is exact silence. Feedback endpoints compile reserved nodes while OFF, enabling a preconfigured route without a message pump. A host Kill rising write is consumed once and ramps for 5 ms; no falling write is needed to release it. MixTail=false holds wet processing state once the smoothed mix is fully dry; controls still dispatch and wet processing resumes without reset when mix rises.
- Expression source packets forward to MidiRouter's real virtual-expression mapping. Tempo and tempo-source packets and model revisions forward to the canonical router clock (requires host worker 90ac4109); the old local BPM override was removed so HOST/AUTO priority and the clock's 50 ms glide remain authoritative. Pedal TimeInfo is still sampled at block start with linear per-slice PPQ extrapolation; exact within-block clock-glide queries are not claimed.
- Focused native suite: **62 tests,105,961 checks,0 failures**. Added tests verify no-timer lane controls with zero callback allocations, persistence across blocks, explicit same-value intent versus stale echoes, structural deferral, exact-silence endpoint, OFF-model feedback surviving repeated host blocks, one-shot kill release, MixTail process-count pause/resume, and host-selected tempo taking precedence over an independently automated internal BPM.
- Integration requires compiling new ExtendedControls.cpp and the shared ExtendedHostAutomation.h from host b4057e62/d87a26d2. Host APVTS append/bridge and virtual-expression implementation are worker-owned dependencies. This does not close CPU-cap/governor, 32-split coalescing, pedal parameter smoothing, buffered/DSP bypass, dry-delay tap crossfade, or external MIDI buffer-capacity gates.

### Persistent asset lifetime checkpoint (E59)

- Approved Board::setAssetStore(shared_ptr<IAssetStore>) is a stopped/message-thread API. Injection is accepted before explicit prepare or after releaseResources; replacing a prepared board or a pool with retained runtimes returns false. A null injection explicitly opts into RAM-only operation. BoardMeters.assetStoreAvailable reports service installation, not a promise that future filesystem writes succeed; individual model storage status/faults report actual IO outcomes.
- The pool lazily installs AssetStore at userApplicationDataDirectory/Luthier/ReferencePedals/assets when the first real pedal needs services. Construction starts its worker without creating directories; actual writes create the persistent directory off audio. No temporary fallback exists. Service construction failure rejects compilation with a concrete error. Every runtime pins the shared owner, declared before its pedal member so the pedal and its workers finish destruction before the final service reference is released. Prepare/release preserve the configured owner.
- Focused native suite: **63 tests,105,462 checks,0 failures**. The new test injects an AssetStore under a unique temporary directory, verifies no directory creation during injection, performs a content-hashed write/read, drops caller ownership, verifies service availability before prepare, rejects live replacement, exercises allocation-free audio, calls the raw service during pedal destruction, and replaces the store only after stopped release. Explicit null injection prepares the model without assets. The temporary directory is removed after all owned workers finish. Tests do not write into user app-data storage.
- Dependency: verified AssetStore implementation from state 6cf91208 (already integrated by coordinator), plus juce_cryptography. Real looper write/pending/failure behavior remains the model/storage integration test responsibility; no completed recording is invented by the engine.
