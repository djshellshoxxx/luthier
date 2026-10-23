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
CPU budgets, board-level bypass integration and UI goldens. These
are acceptance gaps, not claims of hardware or release validation.

## WP10 in progress

Delay tests currently pass (878,056 checks): impulse timing and reset at all four
sample rates, 120 BPM eighth-note sync, tap-state persistence, alternating
ping-pong echoes and spillover, exact block independence and bounded feedback in
all three voicings. Combined WP9+delay runner: 934,095 checks, zero failures.

Delay mode-switch transient and 10-second hold energy measurements remain pending.
Feedback is always capped at 0.98 outside HOLD because no board runaway permission
is exposed to IPedal. Delay and reverb have factories for their implemented generic algorithms; acoustic
and host-integration acceptance is still incremental.

Reverb has distinct Room/Hall/Ambient 8-line Householder FDNs, a figure-eight
plate tank, and three 24-stage dispersive spring paths. All five types pass exact
block partition comparisons, reset/silence, and finite-output tests at the four
sample rates. Hall RT60 (backward Schroeder integration, -5 to -35 dB extrapolated)
passes the +/-10% gate at 1/2/4/8 seconds; freeze energy stays within 1 dB over
10 seconds. The initial fixed lowpass feedback implementation failed RT60 by
12%; damping now derives its HF shelf loss from the requested RT60 ratio.

Remaining reverb gates: echo density by 200 ms, 60-second randomized sweeps,
mono-sum compatibility, normalized wet RMS, type-switch transient limit, CPU,
and freeze behavior across all types/settings. Plate uses published reference-rate
lengths with figure-eight feedback, but full output-tap calibration remains open.

The allocator hook reports zero allocations across all six generic pedal process,
setParam, foot, expression, and enable callbacks with parameter changes. No locks
or I/O appear in their callback call paths. Latest runner before added acoustic
notch tests: 2,944,199 checks, zero failures.

## WP14–16

A bounded RAM `LooperCore` slice now implements sample-timestamped SPSC commands,
record/play/overdub/stop/clear, sparse preallocated overdub layers, cancellation,
and bit-exact undo/redo. Tests pass command timing/dry-through, null comparisons
before/after undo and redo, cancelled overdub, auto-close at capacity, strict RAM
budget, queue overflow reporting, and zero callback allocations. Clear is O(1).
The core deliberately refuses overdubs at its configured 1–8 layer capacity;
unlimited compaction, worker spill/persistence, multithreaded ownership testing,
non-destructive seamless loop crossfades, and named-pedal gesture integration are
still outstanding. It does not replace the prototype.

Branded modulation/delay, BBD core, three first-generation looper wrappers,
and RC-5 rhythm/storage/MIDI are not implemented in this slice. Their scaffolds
remain unregistered. Existing `PhraseLooper.h` and its regression tests remain
unchanged. No capacity, fidelity, persistence, or pattern-completeness claims are
made for these remaining packages.

Full repository/plugin build, sanitizer runner, and host integration are managed
by the coordinator and have not yet been run on this slice.
