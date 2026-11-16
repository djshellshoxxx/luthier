# Rhythm MIDI continuity — 2026-11-16

Parent: chorus qualification `bf797637adcea5484df83896da2eb2a9962cf74d`.
This addresses F-A1 from the November 11 real-time audit and its follow-up
findings, plus a bounded carry for the F-A2 engine-lock MIDI-loss path. Other
output sources and full release qualification remain separate work.

## Changes

- `RhythmMidiWriter` is the engine's sole rhythm MIDI converter. The processor's
  second writer is removed. A fixed queue retains future strum crossings in
  chronological order, including the host block's sub-block offset.
- A string's off resolves to the pitch actually emitted. Re-strikes first close
  the previous note; orphan offs and dead-string attacks do not create MIDI.
- Rhythm stop, chord release, and transport stop cancel future attacks and
  release held notes. Disabled rhythm processing still consumes pending releases;
  its event list is cleared every sub-block. Releases also reach the audio
  scheduler when rhythm ceases driving.
- Fixed-queue overflow releases held notes and cancels pending attacks. Bounded
  output writes retain failed releases for retry before new attacks.
- The output router tracks rhythm notes actually delivered. Source/global
  disable emits their final offs once, without echoing input. A destination too
  small for every off retains the remainder, even across re-enable or zero-frame
  callbacks. Failed rhythm offs are retried before pass-through next block.
- Both engine-bound input streams (played MIDI and direct tune/calibration MIDI)
  survive a failed structural try-lock. Delayed events precede current input at
  sample zero on the next admitted callback. Upstream consumers do not see them
  again. Fixed-capacity overflow substitutes pedal releases and all-notes/sound-off
  on all 16 channels. It cannot recover the audio/timing of an already missed block.
- A source disable is latched until the engine lock is available, so re-enabling
  cannot revive pending strums from before the disable.
- No extra allocation is introduced in the writer or router callback paths;
  the queue and note table are fixed storage. Native allocation-counter coverage
  remains part of full product qualification.

## Evidence and limits

GCC 14 with actual JUCE 8.0.10, ASan+UBSan: 12 focused tests / 45 checks pass.
Compiling the original router implementation against the regression suite first
produced five failed checks: source/global stop, small-buffer retry, and release
retry after pass-through filled the destination. The writer's ordered stream is
identical at block sizes 1, 37, 512, and 2048; additional checks cover reset/stop,
overflow, dead strikes, and actual held pitches. A seeded stress test covers 4,096 blocks with source/global switches, stop,
future crossings, and five destination capacities; no duplicate active notes,
out-of-block timestamps, or final held notes remain. Three deferred-input tests
cover ordering, single consumption, reset, and overflow on all MIDI channels.
Local leak detection was off
because the traced execution environment cannot run LeakSanitizer.

The reproducible focused target is `scripts/midi_checks`. The parked PR #99
full-processor regression is retained as `RhythmMidiOutContinuityTests.cpp`, with
its missing `TestContext` parameter corrected. It checks seven strums, exact
note counts and beat starts, pairing, switch-off, and all four block sizes.
The final processor, engine, and both integration regressions pass a local
C++17 syntax check. The reproducible CMake MIDI target also built and ran all
12 tests / 45 checks under ASan+UBSan. GCC optimized sanitizer compilation hit
a local memory limit; the successful CMake run used Debug and one build job. Full native execution of that regression is still pending; leaf tests
and syntax checking do not prove processor integration or platform behavior.

Quick-check runs `RhythmMidiWriter`, `RhythmMidiRouter`, `RhythmMidiOut`, and
`DeferredEngineMidi`, plus the existing `BackendRelease` selection, for
both editions. Prior PR #103 quick-check run 43884171299 passed both Free/Pro
native smoke selections and the real-JUCE ThreadSanitizer recorder job. That
run validates the recorder parent, not these subsequent MIDI or chorus changes.

Remaining release work includes native F-A1/F-A2 qualification, latency/IR audit findings, full processor
stress and native macOS results, final optimized plugin validation, packaging,
and consolidation. No new Windows/macOS release is qualified by this checkpoint.
