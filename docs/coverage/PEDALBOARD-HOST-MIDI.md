# Reference pedalboard host / MIDI implementation ledger

2026-10-23. Workstream WP5, WP6, WP23. Acceptance is finite: the named automated tests below plus the explicitly missing gates. This is not a claim of complete MIDI_EXPRESSION coverage.

## Interface errata

E33 (approved by coordinator): `MidiRouter::reclaim() noexcept -> int` is additive to frozen I-5. Message thread only, called by Board::reclaim. Immutable snapshots remain owned until two completed process calls after publication; process never deletes maps. prepare/destruction require audio stopped, as before.

E36 (approved): ParamWrite slot0xff selects BoardTarget enum (Mix0, Tempo1, Enabled2), with normalised tempo20..300 BPM. Other slots retain descriptor indices.

E39 (coordinator interpretation): OFF preserves bit-identical audio and suppresses pedal writes/foot/raw/output MIDI. Clock tracking and the enabled binding remain live for reenable/sync; other board targets are gated while OFF.

WP6 shared pure interfaces, previously empty skeletons: `MidiState::buildMap(orderedInstances, settings)` resolves UUID + descriptor param id to fixed map indices; `MidiState::validate(var,error)` bounds persisted maps. `GestureDecoder` exposes timestamped down/up/advance/pop with configurable tap/double/hold/erase thresholds, and stopped/fast-tap switches. `FootController` owns latch/momentary/auto engagement. `ExprEngine` exposes per-sample one-pole state and pitch-bend conversion. `BoardMidiLearn` owns message-thread timeout/confirmation around frozen router capture API. `GestureEngine` exposes travel/engage/tap compensation and held-key poly-pressure reduction. These APIs do not add GUI dependencies.

WP23 host extension: `LuthierEngine::setReferencePedalTransport(HostTransport)` receives the already validated processor clock read; the board never calls the host playhead itself.

## Actual native checks

Standalone native harness against JUCE 8 core/audio_basics (2026-10-23): 23 cases / 209 checks passing under AddressSanitizer + UndefinedBehaviorSanitizer; the earlier 13-case / 64-check snapshot independently passed ThreadSanitizer. Own routing sources instrumented; reused JUCE module objects are uninstrumented.

- RefPedalMidiCore::mappedOffsetsAndReservedGuard — 6 checks, passing.
- RefPedalMidiCore::clockStartStopLoss — 6 checks, passing.
- RefPedalMidiCore::boundedFloodProtectsClock — 2 checks, passing.
- RefPedalMidiCore::learnNeedsDistinctValuesAndSuppresses — 5 checks, passing.
- RefPedalMidiGesture::thresholdsAndDouble — 9 checks, passing.
- RefPedalMidiGesture::momentaryAutoDebounce — 6 checks, passing.
- RefPedalMidiGesture::expressionStepResponse — 3 checks, passing.

Additional passing cases: noAllocationsDuringRouting (3), concurrentPublicationAndReclamation (1; 200 snapshot replacements), boardTargetsRetainOffsets (5), softTakeoverAndExactChannelPrecedence (5), heldNotesReleaseOriginalSlotAcrossReorder (7), disabledBoardOnlyAllowsEnableTarget (6).

Compiler syntax checks passed for Parameters.cpp, LuthierEngine.cpp, PluginProcessor.cpp, HostStateFormat.cpp and RefPedalHostTests.cpp. Host execution remains pending the full build.

## Missing gates / current limitations

- Host files and host tests compiler-checked; full native execution is pending the shared build. Persistent map resolution compiler-checked.
- Host automation is a message-thread bidirectional model mirror, not sample-accurate. Fixed 320 slot lanes + enabled/mix/tempo are append-only; continuous audio-thread overrides and dynamic lane names remain pending.
- Clock median estimate/basic source priority and transport/loss work; specified PLL phase lock, jitter budget, source hysteresis/glide, exact mid-block source arbitration, tap external-master rejection and clock output remain pending.
- Source map routing supports CC/CC14, notes/velocity, PB, channel/poly pressure, PC, absolute/relative/toggle/momentary/trigger. Step mode uses descriptor-derived step count, soft takeover uses initial model value/pickup crossing, and source priority uses 300ms hold. Board-target writes carry E36 routing data; engine consumption pending coordinated integration. Additive combine, scene recall and custom PCHIP UI remains pending; custom curve state and evaluation are implemented. Selected focus and channel-local descriptor profiles are now compiled below explicit bindings.
- Adaptive256-entry LUT interpolation meets <=1e-4 analytic error in the implemented sweep, including log/exp p4; custom PCHIP monotonicity is tested. Expression smoother is pure-tested but pedal consumers must advance it; smoothMs is carried through RouteOut, integration pending.
- Learn distinct-value capture + suppression exists; ambiguous-source competition, duplicate dialog, learn-all and device/UI feedback remain pending.
- Gesture decoder thresholds and controller modes are pure-tested; individual looper action consumption, pressure-hold and sample-accurate engage crossfade remain pending.
- Hard event/ring capacities drop newest events and count overload; clock has reserved capacity but an unbounded clock-only flood can overflow the priority ring. Spec absolute 'never dropped' cannot hold with bounded storage under unbounded realtime input.
- 60s flood performance, 30min fuzz, all sample-rate/block partitions, AU automation crash reproduction, DAW/manual device QA remain pending.

## Host checkpoint

- Engine prepare uses promised maxBlock; release calls board releaseResources.
- Each engine sub-block calls beginBlock/endBlock; PRE/LOOP retained, POST inserted between cabinet and room with prepared double scratch.
- Processor passes its existing validated host clock, including processor/engine slice offsets. Main latency includes board; message timer reclaims and reports latency.
- 323 parameters appended after FEAT-SAT: board enabled/mix/BPM, then 16*20 slot enable, p01..p16, input/output trims, position. Bidirectional message-thread mirror preserves model edits when host values did not change and resolves position requests together.
- Host root referencePedals is a recognized section. Lossless BoardState String codec writes raw JSON and exact opaque doubles; extraction restores original newer-schema bytes. Missing block resets empty/OFF.
- RefPedalHost execution cases added: appendOnlyLaneIdsAndDefaults; boardStateSurvivesHostReloadAndRateChange; missingBoardBlockRestoresLegacyDefault; repeatedRateAndSmallBlockLifecycle; forwardBoardSectionPreservesRawBytes. These are compiled, not yet run.
- Host limitations still pending: sample-accurate host automation, dynamic host lane names, per-placement auxiliary latency accounting, actual AU automation failure reproduction, DAW validation and UI control coverage integration.

Clock follow-up: hostTransportPriorityAndStopEdge (7 checks) now verifies AUTO STOP exactly once; clockTempoIndependentOfRateAndBlockPartition (40 checks) covers 44.1/48/96/192kHz and 1/37/64/333/2048 block partitions.

## E44 host automation follow-up (2026-10-24)

Processor host callbacks now publish normalized write payloads into HostAutomationCapture's fixed atomics, with release serials and physical-unit conversion. Each audio slice copies the snapshot into Board::setHostAutomation; no model/registry calls or timer are required for delivery. Thread-local EchoScope suppresses synchronous model echoes only on the echoing thread, preserving concurrent host payloads. A repeated write at the same value still advances its serial. The message service reconciles/persists and echoes afterward; position remains structural per BOARD_CORE9.4.

HostAutomationCapture is an additive pure engine header exposing write(index,norm), snapshot(), serial(index), value(index), and nested EchoScope. E44 shared snapshot/facade types are owned by the engine workstream.

hostCaptureUnitsAndThreadLocalEcho adds 11 passing ASan/UBSan checks, including a concurrent 10,000-write host source during 10,000 suppressed model echoes. Full processor tests automationWithoutMessagePump and automationThousandWritesPerSecond are compiler-checked, not executed yet. These supplement the five previous host execution cases.

Remaining E44 integration gate: router E39 OFF gating must receive the live host-enabled override before routing, independent of model echo; proposed hook under coordinator review. Full processor/board automation execution and UI-echo race under TSAN remain pending.

E46: MidiRouter::setHostEnabled(bool,uint32_t) is an approved additive audio-thread hook called before process. Zero serial is unwritten; changed nonzero serial wins once, later actual model enabled changes restore model truth. prepare resets applied host serial. hostEnableGatesMidiWithoutModelEcho passes five ASan/UBSan checks for offset37 delivery while model map is OFF, later host disable, model change and a new host disable serial. Same-value model write intent remains an engine metadata integration gate.

Host sidechain now forwards the actual processor-copied input slice to Board::setSidechain before every engine sub-block, with inner offsets and explicit null clearing. It uses no allocation or GUI code; compiler check pending/recorded with the next checkpoint.

E48: MidiRouter::setModelEnabled(bool,uint32_t) is an approved additive audio-thread hook for explicit model write intent, including equal-value writes. New model revision applies before a new host serial. A consumed host serial does not override later model intent; a fresh host serial can. prepare resets applied model/host revisions. sameValueModelDisableDefeatsConsumedHostWrite passes five ASan/UBSan checks including UINT32_MAX->1 host serial wrap and 96kHz reprepare.

Default-map follow-up: additive `addDefaultMaps(MapSnap&,ordered,settings)` builds message-thread focus/local maps. MapSnap binding layers are explicit0 > focus1 > local2; earlier chain profiles claim overlapping CCs on the same explicit channel. Focus uses settings.midi.selectedInstance, first pedal fallback; CC70..77 first eight eligible controls, CC11 first expression target, CC64 selected physical footswitch. Profiles activate only with explicit slot extras.midi.chan or selected useProfile; no-MIDI hardware profiles remain gated by their midi_enable descriptor. Generic fallback uses CC20..31/85. Canonical keyboard destinations stay storage slots. highResolutionCcIgnoresOrphanLsb (6 checks) and stableUuidFocusAndLocalProfilePrecedence (10 checks) pass in coherent native Board/Registry/MIDI ASan/UBSan harness, 20 cases/148 checks total.

Focused HostAutomationCapture ThreadSanitizer stress passed 100,000 real host writes concurrent with suppressed echoes and snapshot reads; final serial100000/value0.75, no reported races. Full processor echo-intent integration remains pending E50.

E50 routing regression: once E48 model intent is supplied, map booleans no longer independently reset the gate. staleHostEchoMapCannotReenableBoard passes four ASan/UBSan checks for hostON, newerhostOFF, stale ON-map publication with unchanged model revision, and a later explicit ON revision.

Curve API follow-up: CurvePoint/CustomPoints, validPoints, pchipSlopes/pchipValue and buildCurve are additive pure MIDI helpers. CurveLut stores up to256 adaptive x/y nodes with bounded binary-search interpolation; linear/inverse need only endpoints and step curves retain exact discontinuities. This keeps fixed storage while avoiding uniform-grid log(p4) error. MapSnap carries custom points outside the frozen32-byte Binding. MIDI JSON curve.pts accepts exactlyfive numeric[ x,y ]pairs, x endpoints0/1, min dx.01; monotone y required unless allowNonMonotone isexplicit. Unknown curve/mode/source enums are retained in state and do not alias known bindings.

adaptiveCurvesMeetAnalyticErrorBudget (50 checks) scans100,001 input values per curve/parameter combination and passes <=1e-4 plus exactendpoints/monotonicity; customPchipPreservesMonotoneShape (7 checks) tests a .01-wide firstsegment and opt-in nonmonotonicity. Coherent native Board/Registry/MIDI ASan/UBSan:23cases209checks pass.

Pinned JUCE standalone MIDI reservation patch: scripts/patch_juce_midi_capacity.py modifies only AudioProcessorPlayer's constructor to reserve65536 bytes before audio. It canonicalizes line endings only for SHA256 comparison, accepts one exact JUCE8.0.10 preimage or its exact patched postimage, preserves the upstream licence and original line endings, writes atomically, refuses unexpected source, and supports --check. Four isolated-copy Python tests pass (apply/idempotence/licence, unknownpreimage rejection, check-only refusal, LFretention). Shared ThirdParty/JUCE was not modified during the coordinator's source freeze. Configure hook and LUTHIER_STANDALONE_MIDI_RESERVED_BYTES=65536 definition must be added by coordinator only after successful patch verification, before juce_audio_utils compilation.

E50 host message service now uses BoardModel::applyHostEcho for enabled/mix/BPM, lane values and trims; position remains an ordinary structural command. Compiler checks pass against engine2d07. Added hostPersistenceEchoDoesNotCreateUndo execution case (compiled, awaiting full processor run). Late scalar persistence does not advance audio intent or add undo steps.
