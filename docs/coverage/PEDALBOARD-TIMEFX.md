# Pedalboard time-effects implementation evidence

Status: incremental implementation, not full WP9/10/14/15/16 completion.

## WP9 first slice

Chorus, phaser, tremolo, and wah DSP implement the specified public-domain families.
Chorus/phaser/tremolo have registry factories. Wah remains unregistered because
its toe switch must request board-owned enable changes; IPedal has no return
channel, and the board owner has confirmed integration is pending. The DSP and
expression path are testable independently. No branded model is newly registered.

Executed `RefPedalGenericBTests.cpp` in an isolated JUCE-core-linked runner with
GCC/C++17, -O1, NDEBUG: 56,039 checks, zero failures. Tests establish:

- bit-identical processing for 2048-sample blocks versus irregular partitions;
- finite bounded output with every parameter at extremes, 44.1/48/96/192 kHz;
- exact dry endpoints (including tremolo depth zero at unity level);
- tremolo depth valley/peak and 4 Hz cycle count, equal-power stereo pan;
- sample-count tap interval and wah expression routing;
- beat-phase convergence after one second at 120 BPM.

Still unmeasured: chorus delay/pitch-depth and full frequency-response calibration,
phaser notch counts, mode-switch transient bounds, harmonic crossover response,
allocator hook, CPU budgets, board-level bypass integration and UI goldens. These
are acceptance gaps, not claims of hardware or release validation.

## WP10 in progress

Delay tests currently pass (878,056 checks): impulse timing and reset at all four
sample rates, 120 BPM eighth-note sync, tap-state persistence, alternating
ping-pong echoes and spillover, exact block independence and bounded feedback in
all three voicings. Combined WP9+delay runner: 934,095 checks, zero failures.

Delay mode-switch transient and 10-second hold energy measurements remain pending.
Feedback is always capped at 0.98 outside HOLD because no board runaway permission
is exposed to IPedal. Registry inclusion awaits the remaining gates.

Reverb FDN, plate, and spring work is in progress; no reverb factory is registered.

## WP14–16

Branded modulation/delay, BBD core, looper core, three first-generation loopers,
and RC-5 rhythm/storage/MIDI are not implemented in this slice. Their scaffolds
remain unregistered. Existing `PhraseLooper.h` and its regression tests remain
unchanged. No capacity, fidelity, persistence, or pattern-completeness claims are
made for these remaining packages.

Full repository/plugin build, sanitizer runner, and host integration are managed
by the coordinator and have not yet been run on this slice.
