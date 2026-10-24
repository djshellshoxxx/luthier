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

Full repository/plugin build and host integration are managed by the coordinator.
Isolated sanitizer evidence is recorded below.


## WP14 implemented behavioral candidates, registration gated

BbdCore, Phase90 Script/Block, CE2, CE2W and DD3T now contain functioning DSP.
They remain unregistered while the original acoustic/performance gates below are
unmet. No hardware captures or hardware-equivalence claims are available.

The complete isolated GCC C++17/O1 ASAN+UBSAN runner passes 2,980,129 assertions,
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

Initial measured failures (unchanged original criteria; E51 correction below):
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


## Reviewed corrections and follow-up evidence

E51 was approved by the coordinator after independent DSP mathematical review.
The CE2 text requires clipping above the −6 dBFS reference, but its literal global
`tanh(x)` distorts low-level signals too strongly. The estimated replacement is:

```
y = x                                      when abs(x) <= 0.5
    sign(x) * (0.5 + 0.5*tanh((abs(x)-0.5)/0.5)) otherwise
```

This is C2 continuous, monotonic, unity slope below −6.0206 dBFS peak, and retains
the original ±1 ceiling. It removes low-level cubic coloration. It is an estimated
headroom model, not a measurement of hardware. CE2W's separate 4.5 V law is not
changed by E51. The original numeric acceptance limits remain unchanged.

Coherent total-output CE2 THD, depth zero, noise off, 1 kHz, peak convention:

| Sample rate | Literal tanh at −12 dBFS | E51 at −12 dBFS | Literal tanh at −3 dBFS | E51 at −3 dBFS |
| --- | ---: | ---: | ---: | ---: |
| 44.1 kHz | 2.344898% | <0.000001% | 12.941872% | 2.683061% |
| 48 kHz | 2.260864% | <0.000001% | 12.632477% | 2.583329% |
| 96 kHz | 1.975764% | <0.000001% | 11.498957% | 2.249638% |
| 192 kHz | 1.915484% | <0.000001% | 11.241090% | 2.179377% |

The low-level <0.3% and high-level 2–6% gates now pass all four rates. Comparison
source is preserved in `probes/ce2-headroom-comparison.cpp`; the actual model has
12 persistent all-rate THD assertions. The BBD-mode-specific 0.5–3% gate remains
open and is not replaced by these default-mode checks.

Actual E51 BBD chain probes preserve 1 kHz small-signal gain within 0.000103 dB,
10 kHz attenuation from −36.3 to −44.9 dB, and original impulse timing. At full
scale 5 kHz, the measured folded 5th/7th/9th/11th components are at most
−114.903 dBc (48 kHz, folded 9th); 44.1 kHz worst is −140.19 dBc. The baseline
and revised frequency responses differ by less than 0.000003 dB at the stated
small-signal probes. These probes do not substitute for a full-frequency sweep.

BBD mode now reconstructs the last clock event between host samples. If the
clock advances multiple ticks, the latest event age is residualClockPhase divided
by clocksPerHostSample; its value is linearly reconstructed from the preceding
and current pre-hold wet values. With no tick, the previous hold remains. This
avoids an inert option whenever the BBD clock exceeds host rate. DSP independently
reviewed the approximation; distinct-output, reset and all-rate tests pass.

Performance work caches fixed time constants and settled-control powers, caches
DD3T quantization scale, and interpolates Phase90 allpass coefficients on a
sample-counted eight-sample clock. Existing partition, timing, phase and notch
regressions pass. Shared `MildTanh` uses degree-13 Horner evaluation only for
|x| <= 0.25, otherwise libm tanh. The dense million-point maximum absolute error
is 1.32236e-12; the alternating remainder bound is 1.36e-12. It changes numerical
cost, not the modeled shaper law, and has independent DSP approval.

The initial wall-time CPU figures above were contaminated by parallel-build
contention and are superseded. The probe now times only process calls using
CLOCK_THREAD_CPUTIME_ID; input generation is outside the timed region. Before
the CE2W continuous-history follow-up, measured O3 results at 48 kHz were CE2
0.471% (still above 0.4%), DD3T 0.593% (above 0.5%), CE2W 0.340% (below 1%), and
Phase90 0.329% (below 0.4%). No CPU threshold has been raised.

The ASAN+UBSAN immutable snapshot passes 2,980,129 assertions with no diagnostics,
including E51, fast tanh, bounded delay wrapping, all ten pedal allocation checks,
and 60 seconds of random controls/block sizes for each named model at all four
rates. An earlier sanitizer build was invalid: model headers were edited during
sequential compilation, mixing an old stack object size with a newer constructor.
Its stack overflow report is discarded as mixed-revision evidence, not suppressed.
All subsequent sanitizer builds use immutable copied source trees.

Verified follow-up: CE2W now advances all three mode histories,
preventing stale audio when returning to a previously inactive mode. A dedicated
regression feeds audio, changes mode, waits one second of silence, then returns
and requires energy below 1e-20. A 30-minute-per-rate/model randomized soak is
available under LUTHIER_PERF=1; default test duration remains 60 seconds.

Registrations remain gated. CE2W CE1 THD, full FRF/IMD/sideband grids, transition
criteria, state/audio restoration and remaining CPU checks are still explicit
acceptance work. The CE1 preamp's estimated +2–3 dB high-frequency color is not
yet calibrated; no separate CE1 clipping-law alteration is authorized or applied.


The final polyphase short FIR was independently reviewed for even/odd phase and
mirrored history indexing. A separate scalar full-convolution oracle contributes
16,384 comparisons; all pass with the original 1e-14 tolerance. Actual BBD-chain
spectral probes are unchanged at the reported precision. A representative CPU
run with all three CE2W voices active gives CE2 0.433%, DD3T 0.531%, Phase90
0.330%, and CE2W 0.844%; CE2/DD3T remain above their original targets. Small
subsequent scalar-loop changes have not established a stable CPU pass and do not
change that status. In particular no machine-noise cherry-picking is used to
claim the narrow remaining margins are acceptable.

DD3T E47 reports actual stored tap tempo (or current valid host BPM during sync)
and the active Short Loop length, playhead and progress. It does not invent
transport states or storage progress. E41 now gives DD3T ownership of every raw
footswitch event: a short release requests board-owned engagement, while a
2-second hold changes tap function without an unintended bypass transition.
Tap mode and Short Loop never request an engagement toggle. Engine ownership
review confirmed this fits the existing contract and its <=64-sample consume
latency; a dedicated short-release/long-hold regression covers the distinction.


Long-soak outcome: LUTHIER_PERF=1 completed 30 simulated minutes for each of the
four reference models at each of 44.1/48/96/192 kHz: 2,736,720,000 stereo frames,
32 aggregate finite/bounded-output checks, zero failures. This immutable snapshot
includes E51, clock-event reconstruction and continuously running CE2W histories;
it predates the mathematically equivalent polyphase FIR and later UI/gesture/
serialization changes. The post-polyphase 60-second-per-rate/model sanitizer
suite passes separately. No denormal-slowdown timing claim is inferred from the
finite-output soak alone.

Two additional Phase90 criteria remain explicitly unsatisfied. At fc=3300 Hz,
the specified bilinear allpass coefficient gives the upper notch at 7359.34 Hz
for 44.1 kHz and 7442.65 Hz for 48 kHz, rather than the analog target 7966 Hz:
errors −7.63% and −6.58%, exceeding the original 3% limit. The passing static
center tests check the exact digital allpass formula and do not waive this sweep
endpoint criterion. Also, the specified fixed Block feedback is 0.35, but the
acceptance text asks for a 0.6-feedback soak while exposing no such parameter.
The actual 0.35 variant passes its stability test; the distinct 0.6 gate remains
unmet. Both conflicts require an independently reviewed algorithm/spec decision.

E55 runtime-save audit fixed generic Delay and DD3T tap-state data races.
Generic Delay publishes a lock-free atomic double. DD3T publishes one atomic
double whose magnitude is the always-positive interval and whose sign encodes
hasTapped; serialization loads once and derives interval, flag and BPM from the
same snapshot. No precision bits are discarded and no message-thread reader
accesses changing DSP members. The platform must provide lock-free double atomics
(static assertion). loadExtra remains an inactive-instance lifecycle operation.
Concurrent audio taps/message-thread serialization passes ThreadSanitizer with
four checks and no race diagnostics. ASAN+UBSAN concurrent/precision round trips
pass 11 checks, including preservation of the adjacent-double mantissa value.

A separate acceptance-only numerical probe now exercises the Phase90 recurrence
at feedback 0.6 for 60 seconds at 44.1/48/88.2/96/192 kHz, with a 16 Hz moving
sweep. Its 15 finite/peak/tail-energy assertions pass. This closes the independent
0.6 stability experiment without changing the physical Block variant's fixed
0.35 feedback or adding a new control. The complete focused Phase90 suite has
42 passing checks.

Bounded Phase90 oversampling proposal (`probes/phase90-2x-proposal.cpp`): running
the same allpass formula at 2x and placing the 31-tap FIR around the common dry/wet
sum moves the high endpoint notch to 7796.96 Hz at 44.1 kHz (−2.133%) and 7822.54 Hz
at 48 kHz (−1.812%), satisfying the original 3% frequency tolerance. It genuinely
adds 15 host samples, violating the explicit zero-sample latency requirement.
Its measured core-only stereo CPU is 0.5395%, already above 0.4% before host,
parameter and buffer overhead. Independent DSP review confirms that common-path
filtering preserves cancellation phase; wet-only filtering would alter it. The
proposal is not adopted and does not amend either original latency or CPU gate.


E58 remains a bounded **proposal**, with the literal four-equal-AP1 implementation
unchanged. `probes/phase90-e58-proposal.cpp` separately prewarps the two original
analog notch anchors, then factors the second-order denominator into two stable
real AP1 coefficients, each used twice. The smaller root is evaluated as P/r to
avoid subtraction cancellation. Alternating roots preserve all four existing
nonlinear sites; grouped ordering is measured separately because the shapers
make stage ordering audible in principle. This changes the literal coefficient
formula, not the original notch, latency, distortion or CPU thresholds.

The committed `probes/phase90-e58-results.txt` records old/new coefficients,
notches, allpass gain, maximum phase change, coherent THD, 60-second 0.6-feedback
stability and core CPU at five rates. Both revised notch residuals are below
2.1e-14; wet magnitude deviation is below 5.4e-14. At the 3300 Hz endpoint the
maximum phase correction across 20 Hz–14 kHz is 15.9601 degrees at 44.1 kHz and
13.3176 degrees at 48 kHz. This is a real frequency-response/tonal correction,
not a claim that the old and proposed transfer functions are identical. The
probe optionally writes all 30,015 old/new phase and mixed-magnitude points to
CSV: compile C++17 -O3, then run with the desired output CSV path.

THD uses 1 kHz, one second settling plus one second coherent projection through
ten harmonics, static fc=3300, noise off. The stated +4 dBu = +6.218487496 dBV
full-scale RMS calibration maps -20/-6 dBV to digital peaks
0.069117914080/0.346410161520 (peak and RMS are not interchanged). Original and
both revised orderings give <0.012% and <0.30% THD, below the unchanged 0.5%/3%
limits for this condition. All old/new moving 16 Hz, feedback 0.6 impulse trials
are finite and decay to zero over 60 seconds. The candidate adds no buffering
or explicit sample latency; as an IIR it retains frequency-dependent group delay.
Core-only stereo thread-CPU is 0.3372% original / 0.3556% revised. These exclude
parameter, mismatch and host overhead, so the actual-model 0.4% gate is **not**
claimed. The prototype tests zero mismatch; adapting seeded per-stage mismatch
still needs an explicitly reviewed mapping and actual-model measurements.

The earlier 30-minute reference soak used deterministic LCG seed 73,
multiplier 1664525 and increment 1013904223, blocks 1/4/16/64/512/2048 and
parameter/expression/foot/enabled changes every 31 blocks. Its aggregate frame
count and exact snapshot limitations remain as stated above.

Reverb type changes now use the explicitly permitted 50 ms dip: 25 ms down,
reset the new tank at zero wet gain, then 25 ms up. This prevents stale audio
from an inactive tank returning when a user revisits a type. Rapid changes can
reverse the dip or select another pending type without dropping a partly
weighted voice. Constant-time FractionalDelay resets avoid clearing megabytes
on the callback. The change also caches unchanged wet-input highpass coefficients
and the fixed tail-envelope coefficient. Focused Delay/Reverb tests pass
2,887,281 checks, including all-rate/type stale-history return, repeated type
changes with exact partition independence, existing RT60/freeze and reset tests.

New acceptance probes exposed outstanding original Reverb gates rather than
certifying the earlier structural implementation: at 48 kHz, full-wet defaults,
10 seconds seeded white input with the last 3 seconds measured, wet/dry RMS is
Room -3.6217, Hall -5.8324, Plate -5.3551, Spring -3.4638, Ambient -6.7989 dB.
These do not meet the original +/-3 dB normalization gate. Conservative mono
average (L+R)/2 stays about -3 dB versus L, passing the original >-6 dB criterion.
Hall raw-impulse normalized echo density in a 20 ms window around 200 ms is
0.8207 (predelay 0) / 0.8371 (default predelay 15 ms), below the original >0.9.
Definition: fraction of samples whose mean-removed magnitude exceeds the window
standard deviation, divided by the Gaussian probability 0.317310507862914.
These acoustic gates remain open; no threshold or test label was weakened.

The FDN now fulfills the originally specified mutually-prime late-delay lengths:
prepare builds a nearest-prime lookup off the audio thread, and SIZE automation
uses bounded table reads. All four input diffuser lengths now scale with SIZE as
specified. Tests verify actual integer lengths, primality, pairwise coprimality
and capacity bounds across five rates, four sizes and all three FDN families.
Focused Delay/Reverb results: 2,890,881 checks, zero failures, including the
unchanged Hall RT60 +/-10% and freeze +/-1 dB gates. These structural fixes alone
do not close echo density: an independent prime-delay Hall probe at 48 kHz,
MOD .25, 200 ms gives raw NED L0.8174/R0.8765; at 192 kHz L0.6812/R0.7353.
The original >0.9 threshold remains unchanged and unsatisfied. High-rate raw
impulses also expose sparse above-audible-band samples, so any future bandlimited
measurement will be reported alongside, rather than replacing, this raw result.

O(1) FractionalDelay reset performance check (O3, precomputed input/delay arrays,
4.8 million pushes plus reads, 100 seconds equivalent audio): cubic median CPU
0.0242162 s before / 0.0253978 s after, a 4.88% helper cost increase but only
0.00118 percentage points of a core for this single-delay scenario. Sinc medians
0.0850024 / 0.0846859 s were within run variation. All old/new checksums matched.
The extra history-validity comparison is not claimed to be free.
