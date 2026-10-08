# Latency defect: bypass must not change latency (PB-07)

**IDs:** PB-07. **ED:** 0.5. **Status:** defect present (partial: latency is reported, but wrongly).

**Summary.** Reported latency must equal the nominal longest path whatever the bypass state. This ships first and needs no board code.

## User-facing behaviour
- Toggling any pedal (footswitch, button, MIDI) never changes the host-reported latency (no PDC jump, no DAW alignment shift).
- Latency is visible only when a structural change or a pedal's oversampling setting changes.

## Engine and data model
- **Defect.** `EffectsChain::getLatencySamples()` (`Source/DSP/Effects/EffectsChain.cpp` 268-277) sums only `! slot.bypassed`. `LuthierAudioProcessor::updateLatency()` (`PluginProcessor.cpp` 1386) reads it on every block (call near 2352), so a bypass toggle moves the value.
- **Fix.** Remove the `! slot.bypassed` term. Sum every occupied slot's `Pedal::getLatencySamples()`. `updateLatency()` already compares against `reportedLatency` and only notifies on change; keep that.
- **Pedal latency must not read bypass state.** Check `Pedal::getLatencySamples()` (`Pedal.cpp`) returns a value independent of `bypassFade`/`bypassed`. It may depend on oversampling factor (a setting, not a toggle).
- **Compensation (required by the fix).** Soft bypass crossfades wet (delayed by L) against dry (undelayed). Without a matching delay the 10 ms fade comb-filters. The bypassed leg must be padded by the pedal's nominal latency L (dry delay line, pre-allocated in `prepare`). Verify in `Pedal.cpp`; if absent, add it in this item. The same pad is used by True and Buffered bypass (file 12) so the path latency is constant.
- No audio-thread allocation: pad delay is sized in `prepareToPlay`.

## Parameters and data
None. No new parameters.

## State and migration
No state change. Presets that reported a bypass-dependent latency now report the nominal value; the value is not stored.

## Edition
Applies to Free and Pro identically.

## Performance budget
Dry pad: one delay line per pedal, length L. Negligible (<0.01 units total). No change to `performance-budget.md` totals.

## Tests
- **PB-07.** Toggle all pedals, in every bypass mode, random order, during playback: reported latency equals the nominal longest path at every block. Must **fail** on current code (it does: bypassed slots drop out of the sum). Add a sub-assert: wet/dry alignment of a soft bypass toggle shows no comb (impulse peak stays at the same sample within 0 samples).

## Effort and dependencies
ED 0.5. No dependencies. Ships before any board work. Files: `EffectsChain.cpp`, `Pedal.cpp`, `PluginProcessor.cpp` (only if the call site moves).
