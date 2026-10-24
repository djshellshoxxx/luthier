# Pedalboard time-effects implementation evidence

Status: incremental implementation, not full WP9/10/14/15/16 completion.

## WP9 first slice

Chorus, phaser, tremolo, and wah DSP implement the specified public-domain families.
All four generic models have registry factories. Wah uses the E41 board-owned
engage request: treadle >=0.97 emits one toggle, <=0.9 rearms it. The audio
callback consumes each request once; Auto/LFO motion does not toggle engagement.
Engine E41 integration applies the request after a subblock of at most64 samples.
No branded model is newly registered.

Executed `RefPedalGenericBTests.cpp` in an isolated JUCE-core-linked runner with
GCC/C++17, -O1, NDEBUG: 56,039 checks, zero failures. Tests establish:

- bit-identical processing for 2048-sample blocks versus irregular partitions;
- finite bounded output with every parameter at extremes, 44.1/48/96/192 kHz;
- exact dry endpoints (including tremolo depth zero at unity level);
- tremolo depth valley/peak and 4 Hz cycle count, equal-power stereo pan;
- sample-count tap interval and wah expression routing;
- beat-phase convergence after one second at 120 BPM.

Additional passing tests establish static chorus wet timing/reset, all analytic
phaser notch counts (2/4/6/8/12 stages), and toe hysteresis/single-consumption.
E40 preserves the original chorus +/-0.5 dB through14 kHz gate: four-point
Lagrange cannot meet it at worst fractional phase (~-1.9 dB at14 kHz/48k).
The approved16-tap windowed-sinc path uses an immutable32KiB phase table built
in prepare. Independent DSP review swept4 rates,257 phases and281 frequencies;
worst error0.003349171 dB. Existing cubic reads remain unchanged for other models.

Still unmeasured: chorus pitch-depth and complete wet-chain calibration,
mode-switch transient bounds, harmonic crossover response,
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

## WP15–16 handoff

A bounded RAM `LooperCore` slice now implements sample-timestamped SPSC commands,
record/play/overdub/stop/clear, sparse preallocated overdub layers, cancellation,
and bit-exact undo/redo. Tests pass command timing/dry-through, null comparisons
before/after undo and redo, cancelled overdub, auto-close at capacity, strict RAM
budget, queue overflow reporting, and zero callback allocations. Clear is O(1).
The core deliberately refuses overdubs at its configured 1–8 layer capacity;
unlimited compaction, worker spill/persistence, multithreaded ownership testing,
non-destructive seamless loop crossfades, and named-pedal gesture integration are
still outstanding. It does not replace the prototype.

WP15/16 ownership transferred to the state worker after commits a3ceccaa and
30eeb3f7. That worker owns subsequent LooperCore/storage/named-loopers changes.
Existing `PhraseLooper.h` and its regression tests remain unchanged here.

Full repository/plugin build, sanitizer runner, and host integration are managed
by the coordinator and have not yet been run on this slice.


## WP14 implemented behavioral candidates, registration gated

BbdCore, Phase90 Script/Block, CE2, CE2W and DD3T now contain functioning DSP.
They remain unregistered while the original acoustic/performance gates below are
unmet. No hardware captures or hardware-equivalence claims are available.

The latest complete isolated GCC C++17/O1 runner passes2,944,741 assertions,
including the generic/core tests above, bandlimited and short FIR tests, all four
reference models, independent stereo states, and11 allocation-counter checks.
Named checks cover44.1/48/96/192 kHz, exact2048-vs1-sample partitions, seeded
reset, impulse timing and dry endpoints, sync/rate endpoints, CE2W wet polarity
cancellation and vibrato dry suppression, Phase90 analytic notches (also88.2k),
DD3T12.5/50/200/800ms endpoints, tap subdivisions, slow-clock halving,2-second
hold, bit-exact Short Loop replay/release, and60-second feedback stability.
The allocation counter sees no allocations in process/parameter/foot/expression/
enable paths for all10 generic/reference pedals and LooperCore.

E21: mono hardware models default to independent dual-mono channel histories.
Appended PF_Config stereo_mode explicitly selects mono summing; mono_out_dup then
selects duplicated versus original dry routing. CE2W retains MonoInStereoOut.
E41: DD3T owns Short Loop and tap foot gestures and suppresses board engagement
for those modes. The parent engine supplies the engagement contract.

DD3T section9 asks500ms QT tap to clamp to50ms, contradicting section6 and
IMPLEMENTATION_PLAN which require333.3ms within the full12.5..800ms range.
The coordinator explicitly selected the latter normative behavior; regression
checks333.3ms without weakening its timing tolerance.
CE2W section9's0.3/~1.7/10Hz expectations conflict with its revised section3,
parameter derivation and IMPLEMENTATION_PLAN0.3..4Hz (10Hz withdrawn).
Implementation follows the revised0.3*(4/.3)^rate law; the conflicting original
criterion remains unresolved and is not reported as a passing10Hz test.

The dedicated31-tap2x wet saturator reports15samples at44.1/48k and is bypassed
at96/192k. Its delay is subtracted from the wet delay age, preserving CE2's
physical2.2ms minimum and zero reported/dry latency. It is independent of the
long drive-pedal oversampler. The actual CE2 BBD chain at0.001 peak has1k gain
within0.00011dB and10k attenuation−36.3 to−44.9dB over the four rates. At5k,
full-scale peak, measured folded5th/7th/9th/11th components are below−146dBc
at44.1/48k (coherent one-second complex projections after one-second settling).
This is evidence for those probes, not a full alias-frequency sweep.

Unmet measured gates (unchanged original criteria):
- CE2 specified Vc=1 tanh, depth0,1k sine at−12dBFS peak gives2.2609% total-output
  THD versus <0.3%; at−3dBFS gives12.6325% versus2..6%. BBD mode character and
  clock tracking remain approximate. No clip-level correction is silently made.
- CE2W CE1 wet output at0dBFS peak gives0.8065% THD versus1..3%, with specified
  4.5V clip level. Standard wet−10dBFS gives0.0585% versus <1% (passes).
- Local O3 single-core wall-time CPU,480k samples,48k stereo: CE2 .648% vs <.4;
  DD3T .821% vs <.5; Phase90 .819% vs <.4; CE2W .398% vs <1 (passes). These are
  machine-specific preliminary measurements, not CI budget certification.
- Full FRF/sideband/IMD grids, switch transient limits,30-minute randomized
  soaks, state/audio resume identity, UI goldens, host routing and complete CPU
  certification remain open. Phase90 <1 LSB24-bit bypass delta wording requires
  clarification about input slope versus transition residual before claiming it.

Standalone measurement sources are preserved in docs/coverage/probes/timefx-spectrum.cpp
and timefx-thd.cpp; compile with C++17/O3, repository-root include path, JUCE modules,
the four model sources, BbdCore.cpp and juce_core. Stable regression assertions
live under Source/Tests. THD uses depth0/noise0, coherent1k/48k with one second
settling then one-second projections of harmonics1..9. All stated input levels
are dBFS peak, not RMS. CE2 measures total output; CE2W extracts wet=(A−B)/2;
DD3T subtracts dry and disables feedback/12-bit quantization.
