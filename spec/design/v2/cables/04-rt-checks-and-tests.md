# CB-13: RT checks and tests

**Items:** CB-13
**ED:** 3
**Source:** `cables-and-brands.md` 0 (rules 5), 10 (CB-13), 11

## Summary

Prove that changing any tier mid-run allocates nothing and recomputes nothing on the audio thread, and define the compiled-board swap that makes this true.

## Status

Missing for the cable and pedal tier code (it does not exist). The repo already has an RT allocation harness: `Source/Tests/QA_RtSafetyAlloc.cpp` and `Source/Tests/AudioThreadSafetyTests.cpp`. This file reuses that harness.

## User-facing behaviour

None directly. A tier change takes effect on the next block after the change, with no click or dropout beyond the effect itself.

## Engine / DSP design

**Compiled-board swap.** The audio thread never sees a half-built board.

1. The message thread builds a new `CompiledBoard` (file 03) from the board block.
2. It publishes the pointer with `std::atomic<const CompiledBoard*>` (release store).
3. `process` loads the pointer once per block (acquire load) and uses that snapshot for the whole block.
4. The old snapshot is not deleted on the audio thread. The swap pushes it onto a lock-free single-producer queue read by the message thread, which deletes it on a timer.

**Parameter ramps.** Tier-derived gains change on a 20 ms linear ramp per object, set at compile and ticked per sample in `process` (addition and multiply only). Biquad coefficients are not changed on a tier change without a fade: a tier change swaps the snapshot and the object's filter state is kept, so no coefficient is recomputed in `process`.

**Denormals.** `process` runs under `juce::ScopedNoDenormals`. Feedback-free paths only in this area, so the risk is low; the scope keeps it safe.

**Double precision** for all state that feeds biquads, the hum phase and the pop index.

## Data model and parameters

None. This file adds a probe, not state.

## State, file format and migration

None.

## Edition gating

None.

## Performance budget

Test cost only. The runtime cost is that of files 01, 02, 06, 08, 09.

## Test plan

- **CB-13 (RT).** 60 s render at 48 kHz, block 128, with tiers changing mid-run:
  - One tier change every 1 s, cycling through all five tiers on all patch cables and pedals.
  - Zero audio-thread allocations, counted with the `QA_RtSafetyAlloc` hook armed around each `process` call.
  - No coefficient recomputed in `process`: `BoardCompiler::computeCoefficients` and the pop-buffer builder increment a probe counter. The counter is read before and after the render loop. It must be equal to the number of compiles that were requested from the message thread, and zero in `process`.
  - No NaN or Inf in output. Output peak stays within the file-level bound of 0 dBFS.
- Added: snapshot swap during a pop (file 02) does not reset the pop unless a new toggle occurs.
- Added: the deferred-delete queue is drained by the message thread only; a debug assert fires if `process` calls `delete`.

## Effort and dependencies

- ED 3.
- Depends on: `pedalboard-v2.md` 3.3 (compile on message thread); `performance-budget.md` 0 (units); existing `QA_RtSafetyAlloc` harness. All other files supply the code under test.
