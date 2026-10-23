# Reference pedalboard host / MIDI implementation ledger

2026-10-23. Workstream WP5, WP6, WP23. Acceptance is finite: the named automated tests below plus the explicitly missing gates. This is not a claim of complete MIDI_EXPRESSION coverage.

## Interface errata

E27 (approved by coordinator): `MidiRouter::reclaim() noexcept -> int` is additive to frozen I-5. Message thread only, called by Board::reclaim. Immutable snapshots remain owned until two completed process calls after publication; process never deletes maps. prepare/destruction require audio stopped, as before.

WP6 shared pure interfaces, previously empty skeletons: `MidiState::buildMap(orderedInstances, settings)` resolves UUID + descriptor param id to fixed map indices; `MidiState::validate(var,error)` bounds persisted maps. `GestureDecoder` exposes timestamped down/up/advance/pop with configurable tap/double/hold/erase thresholds, and stopped/fast-tap switches. `FootController` owns latch/momentary/auto engagement. `ExprEngine` exposes per-sample one-pole state and pitch-bend conversion. `BoardMidiLearn` owns message-thread timeout/confirmation around frozen router capture API. `GestureEngine` exposes travel/engage/tap compensation and held-key poly-pressure reduction. These APIs do not add GUI dependencies.

WP23 host extension: `LuthierEngine::setReferencePedalTransport(HostTransport)` receives the already validated processor clock read; the board never calls the host playhead itself.

## Actual native checks

Standalone native harness against JUCE 8 core/audio_basics (2026-10-23):

- RefPedalMidiCore::mappedOffsetsAndReservedGuard — 6 checks, passing.
- RefPedalMidiCore::clockStartStopLoss — 6 checks, passing.
- RefPedalMidiCore::boundedFloodProtectsClock — 2 checks, passing.
- RefPedalMidiCore::learnNeedsDistinctValuesAndSuppresses — 5 checks, passing.
- RefPedalMidiGesture::thresholdsAndDouble — 9 checks, passing.
- RefPedalMidiGesture::momentaryAutoDebounce — 6 checks, passing.
- RefPedalMidiGesture::expressionStepResponse — 3 checks, passing.

## Missing gates / current limitations

- Host files and persistent map resolution not compiled in initial native harness; integration/full test build pending.
- Host automation is a message-thread bidirectional model mirror, not sample-accurate. Fixed 320 slot lanes + enabled/mix/tempo are append-only; continuous audio-thread overrides and dynamic lane names remain pending.
- Clock median estimate/basic source priority and transport/loss work; specified PLL phase lock, jitter budget, source hysteresis/glide, exact mid-block source arbitration, tap external-master rejection and clock output remain pending.
- Source map routing supports CC/CC14, notes/velocity, PB, channel/poly pressure, PC, absolute/relative/toggle/momentary/trigger. Step mode currently emits no write; board-target writes have no RouteOut representation and are not applied. Soft takeover, source priority/hold/add, scene recall, local/focus defaults and custom PCHIP curves remain pending.
- Curve LUT 256 linear interpolation exists; analytic-error budget for steep log curves not yet established. Expression smoother is pure-tested but pedal consumers must advance it; smoothMs is carried through RouteOut, integration pending.
- Learn distinct-value capture + suppression exists; ambiguous-source competition, duplicate dialog, learn-all and device/UI feedback remain pending.
- Gesture decoder thresholds and controller modes are pure-tested; individual looper action consumption, pressure-hold and sample-accurate engage crossfade remain pending.
- Hard event/ring capacities drop newest events and count overload; clock has reserved capacity but an unbounded clock-only flood can overflow the priority ring. Spec absolute 'never dropped' cannot hold with bounded storage under unbounded realtime input.
- Allocation detector, concurrent snapshot publication/reclaim sanitizer stress, 60s flood performance, 30min fuzz, all sample-rate/block partitions, AU automation crash reproduction, DAW/manual device QA remain pending.
