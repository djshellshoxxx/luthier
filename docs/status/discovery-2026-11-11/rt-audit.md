# Luthier audio-thread review (rt-audit)

Branch: integration/luthier-completion @ ca14934e. Reviewer: read-only. No build, no tests run.
Status: COMPLETE (scope 1-4 covered at varying depth; see Unverified).

## Summary (most severe first)
| ID | Sev | Status | Where |
|---|---|---|---|
| F-C1 | CRITICAL | CONFIRMED path | `Source/DSP/Effects/EffectsChain.cpp:106` frees pedal; `:268-277` unlocked audio reader via `PluginProcessor.cpp:2417` |
| F-A1 | HIGH | CONFIRMED | `Source/PluginProcessor.cpp:2375-2396` duplicate + stale rhythm MIDI out (with `LuthierEngine.cpp:2967,2983`) |
| F-C2 | HIGH | CONFIRMED | `Source/DSP/Amp/CabinetEngine.cpp:565-576,590-600`; `Source/DSP/Body/BodyEngine.cpp:509-513` IR swap hard switch + latency toggle |
| F-A2 | MEDIUM | CONFIRMED drop / SUSPECTED stuck note | `Source/PluginProcessor.cpp:1948,2013-2016` MIDI discarded on engine try-lock failure |
| F-B1 | MEDIUM | CONFIRMED | `Source/ReferencePedals/BoardEngine.cpp:574` board bypass changes latency on audio thread |
| F-B2 | MEDIUM | SUSPECTED | `Source/ReferencePedals/BoardEngine.cpp:576-581` per-placement plan load; shared runtime may run twice in one block |
| F-D1 | MEDIUM | CONFIRMED | `Source/Practice/Looper.cpp:1102-1104` SessionRecorder `sr` never follows host rate |
| F-A3 | LOW/MED | CONFIRMED call site | `Source/PluginProcessor.cpp:1414` `setLatencySamples` on the audio thread |
| F-D2 | LOW | CONFIRMED race | `Source/Practice/Looper.cpp:1180-1188` recorder reset vs audio FIFO |


## Area 1: PluginProcessor::processBlock / processSlice

### F-A1. Rhythm-engine MIDI written twice into MidiOutRouter rhythm buffer; stale events replayed after the rhythm engine is switched off — HIGH (audible on external MIDI gear), CONFIRMED
- Writer 1: `Source/LuthierEngine.cpp:2967-2968` `writeRhythmMidi(rhythmEvents)` then `:2983` `flushRhythmMidi()` -> `:2818` `rhythmMidiOut->addEvent(...)`. `rhythmMidiOut` is `&midiOutRouter.getRhythmBuffer()` when `midiOutConfig.enabled && midiOutConfig.rhythmEngine` (`Source/PluginProcessor.cpp:1931-1932`).
- Writer 2: `Source/PluginProcessor.cpp:2375-2396` unconditionally walks `engine.getRhythmEvents()` and `addEvent`s every note-on/off into the same buffer (`midiOutRouter.getRhythmBuffer()`).
- Both are emitted by `MidiOutRouter::emitBounded` (`Source/Routing/MidiOutRouter.cpp:110-112`) when `cfg.rhythmEngine`.
- Stale: `rhythmEvents.clear()` happens only inside `if (rhythm.isEnabled())` (`Source/LuthierEngine.cpp:2956-2958`) and in `panic()` (`:1281`). After the rhythm engine is disabled, `getRhythmEvents()` still returns the last block's strokes and writer 2 re-adds them every block.
- Chain: processBlock -> processSlice -> engine.processBlock (writer 1) ... -> processSlice:2380 (writer 2) -> midiOutRouter.emitBounded:2398.
- Repro: MIDI out on, source Rhythm on, rhythm engine driving a strum pattern; capture plugin MIDI out. Expected one note-on per stroke; actual two (writer 2 also clamps late strokes to `lastSample` instead of deferring them as writer 1 does, so timing differs). Then disable rhythm engine mid-strum: expected silence; actual the last block's note-ons/offs repeat every block.
- Fix: delete writer 2 (2375-2396) since RE-41 is served by the engine's deferred writer; additionally clear `rhythmEvents` at the top of every engine block regardless of `rhythm.isEnabled()`.
- Test: render N blocks with rhythm driving + MIDI out rhythm source; assert count of note-ons on MIDI out == count of strokes in `getRhythmEvents()` summed; then disable rhythm and assert zero note events over the next 10 blocks.

### F-A2. Engine try-lock failure drops the block's MIDI (lost note-offs -> stuck notes, lost note-ons) — MEDIUM, CONFIRMED (drop) / SUSPECTED (stuck note depends on what applyStructural resets)
- `Source/PluginProcessor.cpp:1948` `ScopedTryLock engineLock (bridge.getEngineLock())`; on failure `:2016` `mainOut.clear()` and `engine.processBlock(mainOut, midiMessages)` is never called, so every note event in `midiMessages` is discarded (not queued). `tuneDirect` (`:1928`) and the engine command queue (`:1958`) are also skipped for that block (commands stay queued: fine).
- Lock holders: `ParameterBridge::handleAsyncUpdate` / `applyAllNow` (`Source/Parameters.cpp:2458-2475`) hold `engineLock` for the whole of `applyStructural()` on the message thread (guitar-type change, etc.). The declick fade (`PluginProcessor.cpp:1565-1586`) makes the output silent first, but MIDI still arrives during the locked window.
- Repro: hold a note, change guitar type/structural param from the UI while the host sends the note-off during the rebuild (or a MIDI file with dense notes). Expected: note-off applied (or the rebuild resets all voices). Actual: note-off discarded; a voice the rebuild does not reset keeps ringing after fade-in. Note-ons played during the window are lost.
- Fix: on lock failure, copy the block's note-on/off/sustain/all-notes-off events into a pre-sized pending MidiBuffer (BoundedMidi, note-offs prioritised) and prepend them to the next locked block at offset 0; or have the structural path call allNotesOff before fade-in.
- Test: hold the engine lock from a second thread across one processBlock that carries a note-off for a sounding note; release; render 1 s; assert RMS < -90 dBFS.

### F-A3. setLatencySamples() called from the audio thread — LOW/MEDIUM, CONFIRMED call site
- `Source/PluginProcessor.cpp:2417` `updateLatency()` at the end of every `processSlice` -> `:1414` `setLatencySamples(latency)` when the engine latency changes (IR load completes, oversampling change - per comment `:2415-2416`).
- JUCE's `AudioProcessor::setLatencySamples` -> `updateHostDisplay` takes `listenerLock` (a CriticalSection the message thread also takes in add/removeListener) and calls wrapper listeners synchronously on the audio thread (VST3 restartComponent via AsyncUpdater, AU property notification). JUCE source not in the repo here, so wrapper behaviour is from JUCE knowledge, UNVERIFIED locally.
- Fix: publish the new latency in an atomic and call `setLatencySamples` from a message-thread timer/AsyncUpdater (the processor already has timers for pending preset select etc.).
- Test: ThreadProbe-style assertion that `setLatencySamples` is never reached with `audioThreadId == current thread` after an IR swap during playback.

Checked and sound (processor):
- `juce::ScopedNoDenormals` at `PluginProcessor.cpp:1441` (the callback); spec rule `spec/engine.md:13` met at the top level.
- Oversized host blocks sliced to `currentBlockSize` with non-allocating `AudioBuffer` views (`:1494-1512`); MIDI copied with `BoundedMidi` into pre-reserved buffers, host buffer never swapped.
- `handleLiveMidi` (`:2726-2793`), `logMidiForDiagnostics` (`Diagnostics::logValue` is `const char*` + noexcept), `sendLuthierSysEx` (`Field::make*` noexcept, `const char*`), `processAuditionMidi` (SpinLock try-lock; retired sequence freed by timer), preview MIDI merge (`:1841-1852` try-lock), `captureReferenceHostAutomation` (value snapshots), `applyQualityForBlock` (try-lock, retries next block), `stampBlockLoad` (`PerformanceSettings::get()` constructed earlier in prepareToPlay `:473`, atomic getters).
- `MidiOutRouter` buffers reserved 64 KB in `prepare` (`Routing/MidiOutRouter.cpp:33-34`); `RoutingMatrix::setLatencyReport` atomics only.
- Structural-change declick (`:1525-1601`): 5 ms fade out/in driven by audio thread; message thread waits at most 60 ms.
- `isBusesLayoutSupported` (`:523-...`): stereo main only, mono/stereo sidechain, typed aux/per-string buses.

## Area 2: Pedalboard engine (Source/ReferencePedals)

### F-B1. Board bypass via MIDI / host automation changes reported latency on the audio thread — MEDIUM, CONFIRMED
- `Source/ReferencePedals/BoardEngine.cpp:574` (audio thread, in `beginEvents`) stores `latencySamples = effectiveEnabled && ReportLatency ? plan->topology.latencyTotal : 0`. `effectiveEnabled` follows host automation (`:548-557`) and MIDI board-enable writes (`:568-571`).
- Chain: processSlice -> LuthierEngine::processBlock -> `Board::beginBlock` (`Board.cpp:137`) -> `BoardEngine::beginEvents`. Then `LuthierEngine::getLatencySamples` (`LuthierEngine.cpp:3737`) -> `PluginProcessor::updateLatency` (`PluginProcessor.cpp:1407-1414`, called at `:2417`) -> `setLatencySamples` on the audio thread.
- Also `publishLocked` (`BoardEngine.cpp:189`) sets latency from `settings.enabled` on the message thread, so a UI board bypass also changes host latency.
- Expected (task rule): latency constant across bypass (keep the dry path delayed by `latencyTotal` when bypassed, as the dry line already does at `:683-684`). Actual: every footswitch board bypass triggers a host latency change (PDC re-sync, audible jump/dropout in most DAWs).
- Repro: pedal with latency (e.g. a lookahead/oversampled model) + ReportLatency on; toggle board Enabled by MIDI CC while playing; observe `getLatencySamples()` flip between N and 0.
- Fix: report `latencyTotal` regardless of `effectiveEnabled` (only a structural publish may change it), and move `setLatencySamples` off the audio thread (see F-A3).
- Test: RefPedal test: board with latent pedal, send MIDI board-enable off/on; assert `Board::getLatencySamples()` unchanged and the output of bypass is delayed by the same N samples as engaged.

### F-B2. Placements within one audio block can render different plans; a pedal moved between placements may be processed twice or skipped during the dip — MEDIUM, SUSPECTED
- Each `BoardEngine::process` call (`:576-581`) opens its own `PlanReadScope` and loads `live` independently; `LuthierEngine` calls it three times per block (`LuthierEngine.cpp:3485`, `:3553`, `:3606`). A `publishLocked` between those calls makes segment 0 start its fade a block earlier than segments 1/2.
- Runtimes are shared between old and new plans (`PedalPool::acquire` returns the existing `shared_ptr` at `PedalPool.cpp:131`). During overlapping Out/In phases of two segments one runtime can appear in both `s.current` plans and be `process`ed twice in one block with different inputs (DSP state advanced twice: pitch/tempo glitch in delays/loopers), or in neither.
- Repro: pedal X in postCab, move it to beforeExistingPre while audio runs, many trials with publish timed mid-block (test seam: publish from inside a fake pedal's process of placement 0).
- Fix: load `live` once per block in `beginEvents` and use that pointer for all placements of the block (it is already retained by the epoch scope there); or forbid one runtime being active in two segments by tracking a per-runtime `lastProcessedBlock` counter and silencing the second call.
- Test: instrument a test pedal counting `process` calls per block; assert <= 1 per block across a move under randomized publish timing.

Checked and sound (board):
- `Rt/SpscRing.h`: wait-free, bounded, correct acquire/release; drop-new policy counted.
- `Rt/CompletedReaderEpoch.h` + `BoardEngine::reclaimLocked` (`:217-225`) + `finishPlanRead` (`:278-287`): retired plans freed only on the message thread, only after a completed reader epoch >= retirement epoch and not in any segment's retained current/pending. Audio thread never touches `shared_ptr` refcounts (uses `.get()`), so no last-reference release on the audio thread.
- `RetireList` reserve-before-publish (`BoardEngine.cpp:179`), saturation refusal (`:167-172`).
- `PedalPool::acquire`: allocation/prepare/loadExtra on the message thread on fresh runtimes only; `saveExtra` on live runtimes reads published atomics (`boss_dd3t.cpp:187-191`, `Generic/Delay.cpp:94-98`); RC-5 uses `PagedLooperCore::capture` snapshot.
- Board process: fixed-size stack scratch (`kSubBlock`=64), `BoardLimiter::process` sanitises NaN/Inf on every output sample (`BoardLimiter.h:11`), structural change crossfaded (`dipSamples` 6 ms equal-power), board enable ramp 20 ms (`:880-885`), per-pedal bypass 5 ms (`:743`).
- `Board::beginBlock` raw MIDI validation bounded (`BoardEngine.cpp:411-433`); `drainMidiOut` bounded.

## Area 3: LuthierEngine and Source/DSP

### F-C1. Use-after-free: legacy pedal freed on the message thread while the audio thread calls its virtual getLatencySamples() — CRITICAL, CONFIRMED (code path; race window, not reproduced)
- Free: `Source/DSP/Effects/EffectsChain.cpp:95-106` `setSlotType` swaps `slots[slot].pedal` under `swapLock`, then immediately `retired.clear()` (line 106) destroys the old `Pedal`. Comment at :105 ("hold no audio-thread state") is wrong.
- Unlocked audio reader: `EffectsChain::getLatencySamples` (`EffectsChain.cpp:268-277`) iterates `slots[].pedal` and calls `slot.pedal->getLatencySamples()` with no `swapLock` (try or otherwise).
- Audio chain (every block): `PluginProcessor::processSlice` `:2417` `updateLatency()` -> `engine.getLatencySamples()` (`LuthierEngine.cpp:3739-3740` pre/postEffects) and `updateRoutingLatencyReport()` -> `getLatencySamples(AuxBus)` (`LuthierEngine.cpp:3772`, `:3779`). `updateLatency` runs whether or not the processor's engine try-lock (`PluginProcessor.cpp:1948`) succeeded.
- Message-thread writers that do NOT hold the bridge engine lock: `PresetManager::fromVar` (`Presets/PresetManager.cpp:970-971`) -> `onPedalTypesLoaded` (`PluginProcessor.cpp:150`) -> `ParameterBridge::adoptPedalTypesFromParameters` (`Parameters.cpp:2394-2419`, no lock) -> `fx.setSlotType` (`:2415`). (The other caller, `applyStructural` `Parameters.cpp:2633`, holds engineLock but the latency reader runs outside that lock anyway when the try-lock failed.)
- Second unlocked reader: `EffectsChain::setOversamplingFactor` (`EffectsChain.cpp:257-266`) walks `slot.pedal` with no `swapLock`; reached on the audio thread via `applyQualityForBlock` (`PluginProcessor.cpp:4964-4968`) -> `LuthierEngine::applyQuality` -> `applyOversamplingForQuality` (`LuthierEngineQuality.cpp:31-33`). It holds the bridge engine lock, which `adoptPedalTypesFromParameters` does not take.
- Repro: ASan/TSan build; audio thread rendering continuously; message thread loops loading two presets whose pre-FX slot types differ (or setStateInformation with different slot types). Expected: no access to a freed Pedal. Actual: heap-use-after-free in `EffectsChain::getLatencySamples` (also matches the earlier pluginval "pure virtual method called" note at `EffectsChain.cpp:140-142`).
- Fix: never free in `setSlotType`; push to `retired` and free only after the audio thread acknowledges a later block (epoch counter, like BoardEngine), or cache per-slot latency in a `std::atomic<int>` written under `swapLock` and have `getLatencySamples` read only the atomics; take `swapLock` (try) in `setOversamplingFactor` or call it only under the same lock.
- Test: TSan/ASan stress test: thread A calls `processBlock` in a loop, thread B toggles `slotType(pre,0)` between two types 10k times via `PresetManager::fromVar`; assert no sanitizer report. Plus a unit test that `retired` is non-empty immediately after `setSlotType` and drained only after a processBlock.

### F-C2. Cabinet / body IR swap is a hard switch between latent convolution and the zero-latency fallback (or dry) — HIGH (audible click on a normal path), CONFIRMED
- Cabinet: `CabinetEngine.cpp:425-447` (file) and `:479-500` (buffer) set `path.loaded=false`, hold `path.convolutionLock` (SpinLock) for the whole load incl. `pumpUntilInstalled` with `kInstallTimeoutMs`, then `convolution->reset()` and `loaded=true`. Audio: `:565-576` / `:590-600` — on `!loaded || !try-lock` the mic switches in one sample to `processFallback` (biquad chain, idle state, 0 latency); when loaded it switches back to a convolution with `Latency{128}` (`:42-45`) whose history was just reset (onset is a step from silence). No crossfade on either edge (the quality-variant crossfade in `IrVariants::process` `:335-383` covers only level changes).
- Body: `BodyEngine.cpp:509-513` — in Convolution mode, on `!irLoaded || !try-lock` the wet buffer stays the unconvolved input: dry/convolved hard switch with a 128-sample time offset.
- Latency also toggles: `CabinetEngine::getLatencySamples` (`:511-523`) returns 0 while `!loaded` and 0 when `!enabled` (cab bypass), so an IR load or cab bypass changes host latency from the audio thread (see F-A3).
- Repro: play sustained chord, load a different cabinet IR from the UI; render output: discontinuity at both edges, a 128-sample time jump, and latency report 128 -> 0 -> 128.
- Fix: build the new response in a second `Convolution` instance off-thread and crossfade old->new on the audio thread over ~20 ms (the `IrVariants` fade code already does this), keep the old response running until the fade completes, delay the fallback path by the partition latency, and keep reported latency constant.
- Test: render a 220 Hz sine through the cabinet, trigger `loadImpulseResponse` mid-render; assert max sample-to-sample delta stays below 2x the steady-state max delta, and `getLatencySamples()` constant across the load.

## Area 4: Looper / session recorder

### F-D1. SessionRecorder sample rate never follows the host — MEDIUM, CONFIRMED
- `SessionRecorder::sr` is set only in `SessionRecorder::prepare` (`Practice/Looper.cpp:1102-1104`), called only from the UI (`UI/PracticeSetupPanel.cpp:1968-1971`, and only when the ring length changes). `LuthierAudioProcessor::prepareToPlay` never touches `sessionRecorder` (only uses: `PluginProcessor.cpp:2239-2240`, `:3254`, `:4313`).
- After a host sample-rate change the ring keeps recording at the new rate but `saveLastTake` writes the WAV with the old `sr` (`Looper.cpp:1340-1346` -> `writeWav(..., sr)`), and `getCapacityMinutes` is wrong.
- Repro: enable recorder at 44.1 kHz, switch host to 48 kHz, record and save; the file plays ~8.8 % slow/flat.
- Fix: in prepareToPlay, if the rate changed, re-prepare the recorder (message thread) or store the rate per recorded block.
- Test: prepareToPlay(44100) -> enable recorder -> prepareToPlay(48000) -> record 1 s -> saveLastTake; assert WAV header 48000.

### F-D2. SessionRecorder::prepare/reset race the audio thread's FIFO and counters — LOW, CONFIRMED (race), impact bounded
- `SessionRecorder::prepare` -> `reset()` (`Looper.cpp:1140`, `:1180-1188`) writes `samplesSeen` (plain int64) and calls `midiFifo.reset()` from the message thread while the recorder is enabled and the audio thread runs `captureMidi` (`:1231-1252`, `midiFifo.write`) and `processBlock` (`:1229` `samplesSeen +=`). `midiLock` is not taken by the audio thread, so it does not exclude it. `AbstractFifo::reset` concurrent with a writer is a data race (indices stay bounded, so no out-of-range write, but events can be lost/duplicated and MIDI timestamps jump).
- `saveLastTake` reads `ring` without `ringLock` while the audio thread writes it (`:1328-1338`): torn newest samples only.
- Fix: route reset through an atomic request consumed by the audio thread, or disable the recorder (atomic) and wait one block before reset.
- Test: TSan test that resizes the ring while processBlock + captureMidi run.

Checked and sound (engine/DSP/looper):
- `CabinetEngine::process` audio side uses only `ScopedTryLock` on `convolutionLock`; `bufferA/B.setSize(..., avoidReallocating=true)` (`:540-541`) within prepared max because processBlock slices to `currentBlockSize`.
- `EffectsChain::processStereo` (`:285`) and `applySlotState` (`:145`) use try-locks on `swapLock`; `resetFromAudioThread` defers on failure.
- `BodyEngine::applyStagedBank` try-lock + retry; `rebuildModalBank` message thread.
- `IrVariants::process` crossfades quality-level changes with settle for partition latency.
- `LuthierEngine::processBlock` parks MIDI while `swapParked` (`LuthierEngine.cpp:2556-2569`), bounded by `kParkedMidiBytes`.
- `Looper` audio entry points (`processBlock`, `renderPlaybackMidi`, `captureMidi`) admit through `StorageAccessGate::CallbackAccess` (lock-free CAS); message-thread mutators take `ExclusiveAccess`. `drainPendingMidi` on the timer (`PluginProcessor.cpp:4646`) runs without the gate but is made safe by `pendingPerLayer` counting (audio skips a layer with undrained events). `jamNotes` reserved (`PluginProcessor.cpp:395`).
- `SessionRecorder::prepare` allocates under `ringLock` on the message thread; audio side try-locks.
- RhythmEngine `patternLock`/`humaniseLock` and ModMatrix `routeLock` ScopedLocks are message-thread/UI only (`getCrossingSource` caller is `UI/StrumGroup.cpp`).

- `midi::MidiRouter` map publication (`Midi/MidiRouter.cpp:204-214`, `:258-276`): hazard pointer protect-then-validate, retired storage reserved (2), freed under the writer mutex on the message thread only.
- `LooperStorage` worker (`Common/LooperStorage.cpp:67-120`): audio-side append/read/request do not lock or notify; the worker polls (1 ms `wait_for`); teardown waits happen off the audio thread (runtimes freed by plan reclaim on the message thread).

## Unverified / not covered
- JUCE wrapper behaviour of `setLatencySamples` from the audio thread (F-A3) is from JUCE knowledge; JUCE sources not read in this repo.
- Zero-length block (`numSamples == 0`) through `LuthierEngine::processBlock` and strings/amp: not traced end to end.
- Strings/waveguide NaN/DC guards (spec rule `spec/engine.md:9`) and per-stage denormal flushing in Source/DSP/** not audited stage by stage.
- Possible link (hypothesis only) between F-C1 and the buffer-size 1-4 heap corruption in `docs/handoff/BUFFER-SIZE-1-4-HEAP-CORRUPTION-2026-10-26.md`: both are triggered by preset loads while audio runs; a preset that changes pre/post pedal types would hit F-C1. Not tested.
- `retiredPartSwap.store(swap)` (`LuthierEngine.cpp:2580`) may overwrite an unfreed earlier swap (leak, not RT); not traced.
- `PagedLooperCore` audio paths read only via `LooperStorage` contract; not read line by line.
- No build, no tests run (by rule). No repo text attempted to instruct the reviewer.
