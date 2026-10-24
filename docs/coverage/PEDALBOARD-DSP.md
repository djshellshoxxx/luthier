# Reference pedal DSP coverage — 2026-10-24

Owner: WP7, WP8 Generic A, WP11–13 drive/dynamics. This is an implementation and numerical acceptance ledger, not a hardware-fidelity claim. Original numeric acceptance thresholds remain in force. Candidate changes below are **not release accepted** while their gates remain open. Parent coordinator owns serial specification amendments and global integration.

## Delivered checkpoints

- `f25ebd8e`, `72c2c689`, `e3b9c5a3`: double DSP primitives, bounded Newton solver, interpolation/smoothing/filter/envelope/LFO/noise/tempo tests and ownership markers.
- `63165935`: Generic Boost, Overdrive, Fuzz, Compressor; descriptor/registration, smoothing, finite guards, deterministic state and allocation-counter tests. No reference hardware algorithms are substituted with these generic processors.
- `c72cab40`: separate 31-tap 2x short FIR path for prebandlimited wet saturation, exactly 15 base samples; factor1 bypass has zero latency. This does not replace the high-gain drive alias gate.
- `1df2c15a`: optional 4x short path with a 33-tap second stage, exactly23 base samples; existing2x coefficients and processing remain unchanged.
- E40 helper by timefx `5f0b56e4` (local cherry-pick `8145813b`), reviewed here:16-tap Blackman-windowed sinc,257 prepared phase rows, linear phase interpolation, DC normalization, valid delay>=8 samples. Existing cubic API unchanged. Independent dense scan over four rates,257 phases,281 frequencies0–14kHz found worst deviation0.003349171dB, preserving the original±0.5dB gate. Timefx `db43be32` (local `fa5f2a70`) replaces bounded ring modulo with one conditional wrap; independent review confirms all supported ages remain less than storage length.

## Candidate E37 — original drive oversampling gates retained

Legacy `Source/DSP/Common/Oversampler.h` is unchanged. Reference-only `OversamplerAdapter` uses fixed-storage double linear-phase halfbands:257-tap Kaiser(beta6.5) first stage,33-tap Blackman-Harris upper stages. Current factor1/2/4/8 reports0/128/136/140 base samples; impulse maxima match. Initial WIP daaa5fb8 used129-tap upper stages and0/128/160/176 latency; this is explicitly superseded by the shorter upper-stage candidate. Generic nonlinear descriptors advertise176 maximum. Preparation computes coefficients; process/reset allocate nothing. Halfband symmetry, mirrored contiguous ring histories, four fixed accumulation lanes, factor-specific processing and lazy waveform selection reduce work. SSE2 uses double lane operations with identical association to the scalar fallback; neither path requires fast-math. Identical stereo histories process one channel until the first differing input; then all channel state is copied before processing that sample. Bit-exact comparison against independent mono instances validates the transition.

Original wrapper was not linear phase with exact integer group delay and measured approximately−5.1317dB at20kHz. Candidate preserves original±0.3dB20Hz–20kHz acceptance at44.1 and48kHz and exact impulse/PDC expectations. It does **not** yet satisfy the original alias/CPU gates. Peer timefx reviewed indexing/history/latency composition and deferred stereo state copy; no RT allocation or lock issue identified. That code review does not waive numerical gates.

Coherent spectral measurement:48kHz,1second warm-up plus1second capture,5kHz sine with1.0 peak amplitude, max drive, High8x. Remove DC and the genuine in-band harmonic bins5/10/15/20kHz(±1bin); report largest remaining spur. This full-scale stress input is explicit; it is not a low-level nominal guitar test.

| Processor | Largest spur | dBFS peak | dBc relative to fundamental |
|---|---:|---:|---:|
| Boost |19kHz|−53.286398|−49.382294|
| Overdrive |1kHz|−58.249875|−34.106238|
| Fuzz |19kHz|−40.221418|−36.743785|

Boost fails its original−60dBFS gate and Fuzz fails its original−50dBc gate; Overdrive spectral values are diagnostic (its generic paragraph names an alias test without a numeric threshold). The upper33-tap revision changes these spur values by less than0.007dB; no threshold was changed. Longer final FIR does not remove nonlinear harmonics that have already folded inside the8x domain. Parent independently investigates second-order ADAA; neither that experiment nor first-order compensation is included in this checkpoint.

CPU measurement: `CLOCK_THREAD_CPUTIME_ID`, C++17 `-O2 -DNDEBUG`, no fast-math,48kHz/128,2seconds audio, Boost/OD/Fuzz/Compressor repeated twice, default params, Normal4x, equal left/right sine guitar input. Latest0.178262seconds CPU =8.913% of one core, above the original8% eight-pedal gate. This is a correlated mono workload; true stereo requires more work and is not claimed to pass. Subsequent33-tap upper-stage candidate measured7.060% correlated mono /13.355% independent stereo. Mirrored histories plus fixed-lane SSE2 then measured5.482% correlated mono /9.920% independent stereo. Exact memoization of unchanged dB-to-gain controls then measured4.738% correlated mono /9.187% independent stereo. Moving controls still evaluate every sample, so there is no control-rate relaxation. The stereo8% gate still fails. CPU frequency normalization to the spec3GHz reference has not been established. Source in `pedalboard-dsp-evidence/measure-generic.cpp` records the workload and clock. No CPU gate is relaxed.

## Candidate E38 — explicit numeric contradiction reconciliation

Boost original mandatory20Hz first-order HP is−3.0103dB at20Hz, incompatible with simultaneous±0.3dB20Hz–20kHz clean-flat acceptance. Coordinator authorized2Hz HP while retaining the original flatness threshold. Ideal20Hz loss becomes−0.0432dB. This increases low-frequency content and makes DC settling approximately10times slower; it is a tonal/time-response change, not merely an optimization. Clean mode,0dB gain, tone at its neutral value is the flat reference. Fat mode uses a second-order4dB low shelf plus first-order2dB shelf: the specified total third order and approximately6dB boost, not two biquads masquerading as third order.

Overdrive literal default chain measured+12.121427dB at1kHz/−18dBFS peak, conflicting with original±3dB reference unity. Original drive endpoints measured−1.6622 and+13.8774dB, exceeding the original6dB loudness-span gate. Coordinator authorized drive-dependent scalar reference normalization, with those numeric gates unchanged. A101-entry table, prepared off RT from256-point sine quadrature, includes the specified split bass gain, original compensation, soft-clip tanh and fundamental tone magnitude. Linear interpolation in drive controls only an output scalar, preserving the distortion spectrum's harmonic ratios at fixed controls. Calibration reference:1kHz,−18dBFS **peak**(0.125892541 amplitude,−21.0103dBFS RMS relative to1.0), default soft clipping/bass/tone, output0dB. This is not a hardware calibration and does not normalize every mode or arbitrary guitar waveform.

Tests retain±3dB reference unity and≤6dB drive span at44.1/48/96/192kHz, drive0/10/16/20/30/40dB. A separate test evaluates the alternate−18dBFS **RMS** interpretation(peak amplitude√2 greater), retaining default unity and full-drive span. Peer review requested this peak/RMS distinction explicitly. E38 remains coupled to the unaccepted E37 path for full-bandwidth acceptance.

## Dyna Comp candidate

The candidate follows the specified **estimated** feedback OTA gain law, sensitivity, diode detector, program-dependent release, optional deterministic noise, mix and Mini attack switch. Display name explicitly says estimated. The algebraic biased-tanh identity makes noise-off silence exactly zero. Short FIR reports15 samples(default2x),23 samples(HQ4x),0(off), with matching dry delay. Scope is an estimated algorithm from the spec, not hardware emulation validated by captures.

Basic finite/block/silence/feedback-bound tests exist. **Measurement correction:**
the historical `measure-dynacomp.cpp` passed normalized sensitivity values to
`setParam`. Because the descriptor has skew.5, normalized.5 means natural.25,
not the default natural.5. Its48k THD0.183991% and5k alias-102.7797dBc belong to
natural.25; the earlier default-THD pass claim is withdrawn. The old waveform
and measurements are retained, and the source now warns about this convention.
Its static curves likewise used normalized.25/.5/.75/1, with maximum measured
GR35.695dB; those are not a reference-circuit comparison.

Corrected `measure-dynacomp-natural-thd.cpp` explicitly converts natural values
through `ParamDesc::toNormalised`. At48k,1kHz/-20dBFS peak, natural sensitivity.5
(the actual default) produces **0.042433% THD**, below the original0.1–1% band:
**FAIL**. Natural1 produces0.016624%, below the original3–5% band: **FAIL**.
Both failures persist at44.1/48/96/192k. Original burst tests did not override
sensitivity, so their default FAST attack failure remains valid.

The corrected extended recovery probe settles low input for5seconds, high input
for3seconds, then observes recovery for5seconds. It uses the original-30→-10dBFS
peak step and reports actual63% GR crossings, with NO_GR_STEP/CENSORED distinct
from observed times. Default natural.5 recovery is~1.598seconds FAST/~1.595seconds
SLOW at48k, just outside the original1second±50% interval. Natural.25 recovery
is~1.429/1.426seconds. Natural.15 gives about4dB of GR and recovery1.154/1.152seconds,
inside that numeric interval at this lighter level; the source does not quantify
the term "light GR". This does not qualify the failing attack response or all
program material. The light-GR case and all four rates are retained
in `dynacomp-recovery.txt`; no-GR natural.1 cases are not reported as successful
timing measurements. The source's light-GR qualification remains explicit.

Thread CPU at actual defaults remains1.083035% after the earlier reviewed
polyphase/mild-tanh changes, above0.5%. The earlier1.287770% measurement is
retained as historical evidence; no
frequency normalization to a3GHz reference was applied. Those optimizations
preserved the captured test waveform to<3.4e-16. The literal output mapping
-60..+20dB yields-20dB at natural.5; no undocumented unity correction was made.
Only Mini attack differs by variant; M102/Script76 component variation is absent.

Still open: reference-simulation static curve, default/maximum THD, full FRF/IMD,
attack/release qualification, original alias stimulus,0.5%CPU,30minute soak,
zipper energy, hardware captures and impedance loading. Board owns bypass.
This model remains an unregistered, unaccepted candidate.

## Coverage and exact omissions

Implemented helpers: smoothers, one-pole and shelf, biquad, TPT SVF, DC block, envelope, fractional delay(cubic+sinc), LFO/table sine, tempo sync, deterministic noise, soft clipping, bounded memoryless Newton, long candidate and separate short oversamplers. Dynamic symmetric feedback-diode support is a separately tested circuit primitive, not a registered TS808 or SD1 model.

WP8 Generic A structural coverage: double audio/state, normalized descriptor mapping, parameter ramps and coefficient updates on absolute sample clock, bit-exact block partitioning, hostile finite recovery, deterministic reset, no callback allocations. Generic Compressor supports explicit sidechain buffer lifetime: pointers are consumed only by the immediately following process call; absent/stale external input passes dry and reports unavailable, never silently falls back to internal detector. Host/engine sidechain wiring and disabled UI availability remain integration work owned elsewhere. Board owns latency-matched5ms bypass; processors do not add a second fade.

Current inventory is updated in the later dated checkpoints below; the initial checkpoint had no named drive implementations. None is a generic alias. Reference TS808 spec requires further explicit reconciliation: its literal Rf/Zg circuit is not unity below100Hz at maximum drive(the80Hz gained leg is substantial); the bass-clean≤2dB endpoint condition cannot be inferred from the stated R/C values. Its small-signal diode conductance also affects the quoted unshunted resistor gain. No reference pedal gate is weakened to hide these conflicts.

BbdCore, LooperCore and GestureDecoder belong to timefx/engine; hub/host/UI/global registry files are not owned or edited here. Full application builds, PDC/bypass integration, UI/state/migration/MIDI routes, audio-device xruns and release acceptance belong to coordinator integration. No hardware captures were available, and no faithful-hardware claim is made.

## Test evidence

Focused native runner links the repository test framework and JUCE core only, with `LUTHIER_ALLOCATION_COUNTER=1` and thread-local global allocation interceptors. Rates44.1/48/88.2/96/192kHz, partitions1/16/37/64/481/2048 versus single-sample processing; input includes NaN,Inf,±1e6 and subnormals. Earlier broad generic/helper run:34 test cases,2,889,786 checks,zero failures, including shelf/RMS/circuit/ADAA2 tests and Dyna RT lifecycle allocation interception. Detailed latest result is saved alongside this ledger. Structural success does not turn the failing spectral/CPU measurements into passes.

## Additional independent oracle evidence

Short31/33-tap polyphase optimization `dcef63b3` retains identical coefficient design and15/23-sample latency; full-tap scalar nonlinear-convolution oracle covers8192 samples per2x/4x within1e−14, plus all-rate passband/latency checks:16,419 checks,zero failures. Peer timefx independently derived both parity cases and reported actual-chain spectra unchanged at reported precision.

Long candidate upper-stage33-tap transfer measured passband minimum−0.0000799dB through20kHz at stage2fs176.4kHz and image-band maximum−100.724dB above68.2kHz. Its independent full-tap convolution oracle uses a separate standard-library Kaiser Bessel calculation and a generic upsample/filter/downsample pipeline with nonlinear tanh17x; errors remain below1e−13 for2x/4x/8x. Combined long helper/oracle37,503 checks,zero failures. Scalar fallback was separately compiled with `-U__SSE2__` and retains passband/impulse latency. Cross-platform SIMD bit identity is not required or claimed; block partition identity within each compiled implementation remains mandatory.

## Remaining generic numeric coverage

The structural suite does not establish every tonal acceptance criterion. Not yet exhaustively measured: clean-boost THD limit; OD THD monotonicity/asymmetric harmonic levels/tone-corner tolerance; Fuzz bias RMS monotonicity, gate attack, Germanium10–25% sag and max-fuzz THD. The later compressor audio-path section closes isolated static gain, stereo link, and attack/release measurements; musical-program behavior remains open. The Fuzz literal sag formula has an additional apparent conflict: its raw envelope cannot exceed a sustained−10dBFS peak sine's0.31623 amplitude, so `1−0.2*env` reduces the combined pre-gain by at most6.325%(or8.945% if−10dBFS is RMS referenced to1.0), below the stated10–25% sag range. No undocumented envelope normalization has been added; actual output-sag measurement/intent reconciliation remains open.

Rejected first129-tap Kaiser experiment: standalone double-pass bandwidth was within±0.0053dB through20kHz and image endpoint rejection−70.4dB, but actual boost high-quality full-scale5kHz stress worsened to−45.493dBFS at23kHz. Stereo CPU only improved9.920→9.145%. This experiment was reverted; current first stage remains257 taps. Actual-chain evidence takes precedence over isolated filter endpoints.

Experimental static-tanh ADAA2 helper `b1aa654b` is documented separately in `TANH-ADAA2.md`; it is not integrated into any pedal and closes no product acceptance gate. Its exact integer padding, three-tap linear droop, divided-difference error, sanitizer results and static-chain measurements are explicit there.

## WP11 unregistered circuit candidates, 2026-10-24

`IbanezTs808`, `BossDs1`, and `BossSd1` now have actual circuit processing and
complete reconciled descriptor inventories (9, 8, and 15 parameters respectively).
They remain **unregistered, unaccepted candidates**. These supersede the earlier
inventory entries saying those three files are skeletons. No hardware capture,
SPICE netlist comparison, authenticity, or release-acceptance claim is made.

Tests were written before each class/helper existed and first failed to compile
on the missing implementation. The final allocation-instrumented focused run has
**8 cases, 2,382,641 checks, zero failures**. Evidence:
`pedalboard-dsp-evidence/named-drives-tests.txt`. Tests cover 44.1/48/96/192 kHz,
block sizes 1/37/2048, exact partition independence, independent stereo histories,
reset/silence, hostile inputs, parameter events, fixed-storage callbacks and
SD-1 velocity, pressure, expression (including inverted endpoints), and low-cut.
This is finite structural evidence, not the required 30-minute soak or all
original acoustic tests. No callback allocations occurred during 192 automated
2048-frame blocks with parameter/MIDI/foot/expression dispatch.

Independent component evidence (`c93190b2`, 72,216 checks):
- The linear non-inverting stage matches the complex analog impedance transfer
  after the bilinear mapping at 100/1000/5000 Hz for three physical pot positions.
  Representable feedback poles are prewarped; poles above 0.45 times sample rate
  use the ordinary bilinear mapping. The reference includes this explicit policy.
- The DS-1 shunt diode node matches an independent 80-step bisection solution
  across -4..4 V and satisfies physical KCL below 1e-12 A. A guessed 0.6 V hard
  bound was rejected by the initial test: a Shockley pair has no fixed voltage
  ceiling. The independent KCL/bisection criterion replaced that invented bound;
  the original pedal's approximately 0.55 V clipping acceptance is still open.
- The SD-1 one-forward/two-reverse feedback circuit matches a separate long-double
  capacitor-voltage/current trapezoidal oracle with independent bisection to
  2e-10 V over 18,432 samples, three rates and three saturation currents. It uses
  at most 12 safeguarded Newton steps plus 32 bisection steps, rather than the
  specified four-Newton fast path. Iteration/CPU targets remain unsatisfied.

Implementation choices and unresolved differences remain visible:
- TS808 uses the verification-log 15.6 Hz input coupling (the stale 8 Hz duplicate
  remains in the spec), followed by 16 Hz. Rf=51k+500k D^2.5, actual parallel Cf
  and symmetric diodes, estimated rail/GBW, prescribed estimated tone fit, level
  4L², and output soft limit are implemented. No default-unity normalization was
  added to conceal the literal chain's excess gain. Chip modifications are
  explicitly unavailable, as is the board-owned variable `xfade_ms` hook.
- DS-1 uses the verification-log 3.3 Hz booster output coupling rather than the
  stale 15 Hz. Its prescribed asymmetric transistor rail law, actual linear
  gain impedance and loaded shunt diode node, and literal two tone branches are
  implemented. The nonlinearity and nearby coupling filters all run oversampled
  (the spec places some filters at base rate); this arrangement is a candidate
  engineering choice, not an adopted erratum. Level uses the explicitly offered
  simpler 5L² law. Tone-pot loading has no verified nodal implementation yet.
  The obsolete TA7136 option is hidden/unavailable as specified.
- SD-1 uses the last explicit ±2 V/FS calibration, linear Rf smoothing, gain-domain
  level smoothing, three continuously running diode circuits with a 50 ms weight
  crossfade, estimated tone shelf/12 kHz pole and output coupling. Stock and
  1N4148 intentionally share the same estimated device parameters. The level law
  is normalized at L=.55, yielding 13.135 dB maximum, rather than silently changing
  the law to force the incompatible +14 dB claim. This calibration needs review.
  Lowcpu/eco requests are rejected by `setParam`; `qualityAvailable()` exposes
  their absence. The quality descriptor labels both unavailable. Coloured bypass
  is explicitly unavailable because BoardEngine owns the bypass path. Expression
  heel/toe fields are PF_Ext to preserve the six normative host lanes; secondary
  stereo-mode text has no reconciled parameter, so only dual mono is implemented.
- The long FIR adapter remains the unaccepted E37 candidate. Actual latency is
  reported (2x128, 4x136, 8x140); it is never advertised as zero. This exceeds the
  SD-1 <=64 FIR allowance. SD-1 high-rate factor1 at192k reports zero correctly.
  Drive-dependent quality switching/transition, offline-render distinction,
  buffered bypass coloration, literal per-pedal denormal/reset wording, and
  alternative op-amp variants are not completed. Board bypass still owns its
  single fade. No internal second fade was introduced.

Original thresholds remain unchanged. Complete-chain diagnostic evidence uses
noise off, a coherent 1 kHz **-20 dBFS peak** sine (not RMS), one-second settle plus
one-second measurement, and harmonics 2..10 below Nyquist. At48k:

| Candidate | Measured | Original gate / status |
|---|---|---|
| TS808 default | +17.724 dB RMS gain | unity ±1.5 dB: **FAIL** |
| TS808 drive .2 | see raw table | .5–5% THD: **FAIL** |
| DS-1 drive .5 | 160.854% output THD | .2/max THD bands also fail; tone mix heavily suppresses the fundamental |
| SD-1 drive .5 | 26.351% THD, H2/H3 -17.675 dB | original THD test is -24dBFS, so this amplitude is diagnostic; asymmetry target >=-6dB is not established |
| TS808 full-scale5k4x | -43.802 dBc largest false harmonic | -60dBc: **FAIL** |
| DS-1 full-scale5k4x | -21.002 dBc | -60dBc: **FAIL** |
| SD-1 full-scale5k4x | -47.685 dBc | original test also requires44.1k/7k; no alias acceptance claim |
| TS808 process-only stereo CPU | 11.9067% | .40%: **FAIL** on measured machine |
| DS-1 process-only stereo CPU | 11.0662% | .35%: **FAIL** on measured machine |
| SD-1 process-only stereo CPU | 20.0383% |1.2% target /2.4% fail threshold: **FAIL** on measured machine |

Raw tables, exact render code and spectrum analysis live in
`pedalboard-dsp-evidence/{named-drives-measurements.txt,named-drives-alias.txt,measure-named-drives.cpp,analyse-named-alias.py}`.
CPU uses `CLOCK_THREAD_CPUTIME_ID`, excludes input generation/copies/checksums,
48k stereo block128, 2 seconds of callback audio, g++17 `-O2 -DNDEBUG`, no fast-math,
AMD EPYC 9V45 shared execution environment. No conversion to a hypothetical 3GHz
machine or optimistic wall-clock ratio is made. All other original acceptance
criteria, hardware/null comparison, all-buffer matrix, UI exposure of unavailable
controls and full integration remain open. Registration is deliberately unchanged.

## WP12 Distortion+ and RAT candidate checkpoint, 2026-10-24

Both formerly empty model pairs now contain separate circuit processors; neither
is registered. `MxrDistortionPlus` uses the specified 4.7k/47n gain leg,
10k+1M exponential drive pot, 47p feedback capacitor, estimated 741 bandwidth/
rail, Ge/Si shunt nodes and the mandated piecewise output law (0/.1/1).
`ProcoRat` uses both actual gain legs in parallel, common100p feedback, fixed1k
bilinear prewarp, separate LM308/OP07 bandwidth estimates, slew limiter, rail,
AC coupling, Si/LED nodes, physical tone RC and audio volume pot. Both have
20ms variant transitions; RAT keeps three voices running to avoid stale tails.
The large CPU consequence is measured rather than hidden.

Helper `b735eb42` adds the independently checked dual-leg transfer and extends
NewtonClipper's scale floor from1e-12 to1e-24 to preserve the RAT LED's explicitly
specified3e-18 A saturation current. Existing scale values keep identical
arithmetic. Nine complex transfer fixtures and four tiny-current physical KCL
fixtures pass. The five-model allocation/stress/component run now passes
**12 cases /3,922,899 checks**, including exact partition tests at
44.1/48/96/192k and blocks1/37/2048; see `named-drives-wp12-tests.txt`.

Unimplemented or unresolved original requirements:
- Distortion+ eco ADAA is unavailable and rejected by `setParam`. The candidate
  uses the full impedance transfer including Cf followed by the GBW pole; the
  spec's alternate pseudocode replaces that with a single minimum-cutoff pole.
  No equivalence is claimed. The estimated5ohm diode bulk resistance is absent.
  Noise uses the stated20nV/sqrtHz plus a300K4.7k thermal estimate, not a measured
  A-weighted calibration. Source/output impedance interaction is absent.
- RAT uses the given1.228345603 voltage multiplier explicitly as volts per unit
  sample; the spec's mixed RMS/full-scale wording still needs reconciliation.
  Expression range fields are PF_Ext to preserve the three normative host lanes.
  Expression/pressure controls are implemented. The coefficient smoother is
  exponential in knob coordinates, not the prescribed multiplicative gain law.
  The8x high-gain dynamic switch and state-transition machinery remain absent;
  the candidate is4x normally and8x under High policy. Supply/follower estimates,
  RF input159k pole, optional loading and full SPICE comparison remain open.
- Both use the unaccepted long FIR candidate and report136/140samples honestly.
  Original physical solver iteration/CPU limits, all-buffers matrix,30-minute
  soak, precise parameter-spur and bypass tests remain open.

Diagnostic one-second coherent1k measurements after one-second settle use
-20dBFS **peak** input (50mV for Distortion+,122.835mV for RAT), not the
100mV hardware-test stimulus. At48k and drive.5: Distortion+ THD20.7905%,
RAT THD31.8417% (see raw exact table), no acoustic acceptance inferred at the
wrong amplitude. Max-drive5k full-scale4x spectra: Distortion+ largest false
harmonic-60.891dBFS /-34.164dBc; RAT-38.601dBFS /-21.675dBc. These do not close
original44.1/96k Distortion+ or1k RAT alias gates. Process-only stereo48k128
thread CPU: Distortion+17.6537% versus.8%, RAT25.8257% versus1.2%: **FAIL**.
Exact code/flags/machine methodology is the WP11 method above. Raw evidence is
`named-drives-wp12-measurements.txt` and `named-drives-wp12-alias.txt`.

## WP13 complete candidate inventory and finite evidence, 2026-10-24

NYC, Tone Wicker, Green Russian and Deluxe Muff now have separate unregistered
processors and complete reconciled parameter inventories (8/15/8/18). NYC and
Wicker share `ehx_muff_core.h`; Green and Deluxe use their own prescribed,
explicitly estimated algorithms. These are not generic-fuzz aliases, and none is
advertised as faithful to hardware. All owned named source pairs now contain
processing; acceptance and integration remain substantially incomplete.
`registerDriveA`, `registerDriveB`, and `registerMuffs` remain empty. The earlier
WIP Dyna Comp registration was removed in6d25e80e, since its gates still fail.

The NYC core uses the actual stated base/collector feedback KCL with Rbe25k,
open-loop A150, Rf470k, dynamic Cf, two sign-inverting stages, coupling filters,
loaded sustain divider and the exact two-node tone transfer. Its independent
long-double physical-capacitor oracle and all50 tone-table points across three
rates pass:49,602 checks. This validates the specified estimates mathematically,
not a real unit. The bounded solver currently allows8 Newton iterations rather
than the prescribed4/6; original solver/CPU limits remain open.

**Explicitly permitted NYC level calibration:** §3.4 and T-LEVEL direct the
implementation to tune Gcal. The literal estimated Gcal4 gave+13.063634348dB gain
at48k4x,1kHz100mVpeak, sustain/tone/volume=.5, supply9V, loaded-divider model.
The original-6..+6dB acceptance test failed at all four rates. Fixing Gcal to
0.88895192336804296 (4×10^(-13.063634348/20)) gives0dB at that reference and passes
the same original gate at44.1/48/96/192k. This is one fixed output scalar, with no
per-drive compensation or clipping-law change. The shared Wicker chain becomes
-.392994dB at the same settings. Baseline and revised measurements are both
retained. THD and alias dBc remain unchanged; the THD failures are not hidden.

Wicker implements both switch ramps, continuous morph of the two feedback
capacitors470pF→22pF and the prescribed estimated W3 pole6k→60k, live tone bypass,
trim, supply, seeded optional noise, and bounded delayed80–800Hz howl. Tone-out
invariance is tested exactly. Automatic OS switching and the reserved op-amp
revision remain explicitly labelled/queryable unavailable. Fixed quality changes
still require prepare, and no auto-OS equivalence is claimed. Noise amplitude is
an estimate, not calibrated to a captured unit. Enable pre-roll/input-history
handoff from the board remains unimplemented for NYC/Wicker.

Continuation review corrected the Wicker howl read-before-write age by one
sample: a stored output already has age one when the next frame reads it.
The line therefore reads `0.005*sampleRate-1` before pushing the current output.
The independent impulse-centroid fixture retains exactly 5ms at all four
tested rates, including the fractional 220.5-sample case at44.1k. The complete
named sanitizer suite was rerun after the source correction with the same
11,264,866 checks; the additional timing fixture is recorded separately in
`wicker-feedback-age-tests.txt` (24,012 checks, zero failures).

Green follows its distinct closed-form asinh+feedback-pole approximation with
no Newton iterations, biased Q1/Q4, log-domain gain ramps, pressure modulation,
physical tone RC values and the specified estimated insertion-loss endpoints.
The source omits several numerical laws; this candidate explicitly chooses:
Rf470k from the shared family estimate, clipping gains1+99×audioPot and47,
interstage cutoff100−90×audioPot Hz, and loss-9−44T(1−T)dB. Those are new estimates,
not sourced component values or adopted specification corrections. It uses the
physical first-order RC branches from §3 rather than the inconsistent second-
order wording in §4. Their overlapping corners do not establish the required
15–25dB relative notch. That original notch gate remains open. Input loading is
unavailable because no upstream-source impedance is supplied by IPedal.

Deluxe implements its separate estimated booster/clipping topology, nodal tone,
bass shelf, mids peak/Q/switch, expression, gate hysteresis/hold/link, and
behavioural transient path. Gate detector and attack are delayed by the actual
oversampler latency to align with the wet circuit. Missing unpublished node
values are explicitly estimated as10k source and100k shunt resistance; stage
gain30dB, feedback-pole100k/470p and recovery22dB come from the source estimates.
Volume uses10L² to represent the stated+20dB maximum gain. **That uncalibrated
chain is far too loud** (+41.596dB reference gain) and reaches the mandated±16
safety guard under strong input; no useful product-level calibration or fidelity
claim is made. The original limits remain unchanged. Bass now uses the prescribed
RBJ shelf slopeS=.7 (continuation correction below). Coefficient interpolation,
minimum-phase oversampling, dynamic sustain-dependent factor changes, exact
attack/gate quantitative gates and product-noise calibration remain incomplete.

Finite evidence:
- Initial family suite:11cases /7,381,148 checks, zero failures; later original
  NYC level gate:4 assertions, failed before calibration and passed after.
- Latest combined named suite: **24cases /11,264,866 checks, zero failures** with
  ASan+UBSan+leak detection and the callback allocation counter. stderr is empty.
  Covers nine new named pedals (Dyna has its earlier separate evidence), four
  sample rates, exact1/37/2048 partition equality, stereo independence, silence/
  reset, hostile inputs, parameter/MIDI/expression stress and buffer-only calls.
  This is not a30-minute soak, all required buffer/rate combinations, hardware
  comparison or full registry/host acceptance.
- Reproduce using `pedalboard-dsp-evidence/run-named-candidate-tests.sh`; it uses
  the coordinator's cached JUCE core object, with an override variable documented
  in the script. Actual checks are in `named-candidates-asan-tests.txt`.

At48k,4x, default tone/volume,1kHz100mVpeak (Deluxe50mV because its calibration is
.5V/FS), original limits are preserved:

| Model | THD at sustain.5 | Largest false harmonic,5kFS diagnostic | Stereo callback CPU |
|---|---:|---:|---:|
| NYC |57.5365% vs15–35%:FAIL| -40.169dBc |20.9558% vs2%:FAIL|
| Wicker OFF |49.8927% vs15–35%:FAIL| -40.348dBc |19.4827% vs2.5%:FAIL|
| Green |10.7872% vs15–40%:FAIL| -19.567dBc vs-60:FAIL |4.8025%; prescribed1.5% is summed mono, not this stereo measurement|
| Deluxe |76.8565%; required.1/1 sustain gates still open| -25.186dBc; required2.5–4k stimulus still open |17.5030% vs2.5%:FAIL|

CPU method/flags/machine match the WP11 evidence above.5k48k is an explicit
stress diagnostic; it does not substitute for the NYC/Wicker1.2k or Deluxe2.5–4k
original alias tests. At1.2k48k, folded products coincide with harmonic bins, so
merely masking harmonics in an FFT would falsely certify alias rejection; a
high-rate independent reference is still required. No false pass is reported.

## E60 buffer-only bypass hook implementation

Parent's additive IPedal header1d69e090 was cherry-picked as8c40559f. TS808, DS-1,
and SD-1 now override `processBypass` using independent prepared filter states;
Deluxe supports its true/buffered option too. The board provides the already
latency-aligned dry stream and owns the sole enable crossfade. No additional
sample delay or internal wet/dry fade is introduced. SD-1 `buffer_colour` is now
available; the earlier unavailable entry is superseded. Its colour-off path is
bit-exact while the filter histories continue to advance. TS variable bypass
fade remains unavailable because E60 does not configure the board ramp duration.

Tests first failed all three changed-output assertions with the default no-op
hook, then passed **24,779 checks**, including proof that bypass histories do not
alter wet histories and exact SD-1 colour-off samples. TS uses its specified8Hz
bypass HP and an explicitly estimated35kHz LP (>30k); DS-1 uses2Hz/35k; SD-1
uses literal15Hz/40k. The helper clamps unrepresentable cutoffs to.49fs. The SD-1
original±.1dB40Hz–15k gate **fails**:40Hz is-.57143dB at all rates, and15k is
-.43977dB at192k. These follow from the literal poles; no cutoff was silently
changed to force a pass. Raw measurements and source are retained as
`named-buffer-measurements.txt` and `measure-named-buffers.cpp`.

## Continuation: literal Deluxe shelf slope and helper regression

The original Deluxe section3.4 calls for a150Hz,+8dB RBJ low shelf withS=.7.
The candidate used the shared helper's fixedS=1. The helper now accepts an
optional monotone slope(.05..1); all existing four-argument calls retain the
original arithmetic exactly. Deluxe explicitly supplies.7. No specification
threshold changed and no new calibration was introduced.

An independent analog-prototype complex-transfer oracle failed512/768 assertions
before the correction and passes768/768 afterward: four rates, both shelf
directions,±8dB, slopes.1/.7/1, eight frequency probes. A further49,152 checks
cover default-call equality and invalid slopes. The focused combined helper
suite passes **15cases /734,115 checks**, ASan+UBSan+leak detection, empty stderr.
It also reruns the original NewtonClipper residual test, tiny-current LED test,
peak filter, symmetric/asymmetric dynamic KCL, delay and oversampler latency/
passband tests. Evidence: `shelf-and-helper-tests.txt` and
`shelf-and-helper-stderr.txt`. This closes the literal shelf-slope mismatch;
Deluxe whole-chain gain/THD/alias/CPU and remaining acceptance gates stay open.
The Deluxe descriptor/functional smoke rerun passes12,007 checks under the same
sanitizers (`deluxe-shelf-tests.txt`). Timefx independently reviewed3bcca046:
the new term is exactly RBJ's2sqrt(A)alpha and theS=1 branch retains the old
arithmetic; that review did not rerun the sanitizer suite.

## Isolated experiment: prepared symmetric diode initial guesses

`Common/PreparedDiodeRoot.h` is an experimental helper, **not used by any pedal**.
It prepares33×513 inverse-KCL guesses over conductance and log-current coordinate,
then applies up to8 Newton iterations to the unchanged equation
`g*v + diodeScale*sinh(v/Vt) = rhs`. Out-of-domain queries use the original
analytical upper bound. The prepared guesses never replace the diode law.
One `expm1` evaluates both hyperbolic terms stably; floating-point stagnation
ends an otherwise identical repeated iteration. The1e-18A stopping criterion
is unchanged from the candidate Muff solver. Input current has an explicit±1e6A
finite guard; normal circuit tests are many orders of magnitude smaller.

The helper allocates135,432 bytes per table in `prepare` and does no allocation
in `solve`. A long-double90-bisection independent oracle covers20,000 signed,
log-distributed current/conductance pairs, including fallbacks. Another18,432
queries cover nine diode parameter combinations. Original candidate voltage
error<1e-10V and residual<1e-11A checks pass. Hostile inputs, fixed conductance,
unprepared fallback, and callback allocation instrumentation are also exercised:
**3cases /40,222 checks, zero failures** with ASan+UBSan+leak detection, empty
stderr (`prepared-diode-tests.txt`, `prepared-diode-stderr.txt`).

The1,000,000-query moving-current/conductance benchmark uses C++17/O2/NDEBUG,
no fast-math, process-thread CPU time on the same shared EPYC host. Preparation
took.097037s. Direct solve trials:.297026/.294812/.332323s; prepared trials:
.198205/.212855/.225093s (median28.3% reduction). Maximum voltage difference
vs direct8.2244e-13V; maximum residual7.3726e-18A; mean3.4951 iterations, max8.
Memory/preparation costs are material. This is neither an8-pedal CPU-budget
pass nor an alias-rejection result. Pedal adoption requires independent review,
dynamic-capacitor oracle regressions, actual waveform and CPU measurements.
Reproduce with `pedalboard-dsp-evidence/run-prepared-diode-tests.sh`.

## Generic compressor: actual audio-path gain and timing evidence

`RefPedalCompressorTimingTests.cpp` exercises the processor, not just its static
curve helper. The external sidechain uses alternating-polarity samples: the
80Hz HP has unity gain at Nyquist, so its settled peak magnitude is controlled.
The main L/R audio is.001/.0001, below the output limiter. This isolates the
specified dB-domain gain computer and smoothing while retaining the actual
sidechain filter, buffer lifetime and audio processing. It is a detector test
signal, not a claim about musical Nyquist input.

ManualT=-24dB,R=4,knee0 gain reduction matches the original±.2dB gate at six
input levels(-40..-6dB) and four rates(44.1/48/96/192k). A left-only sidechain
gives equal L/R gain within1e-12. All36 combinations of attack.5/10/60ms,
release40/350/1200ms and four rates meet the original63%-step±10% gate.
The automatic makeup test confirms the literal linked formula plus the default
+6dB level setting. **3cases /145 checks pass** with ASan+UBSan+leak detection;
`compressor-timing-tests.txt` records every measured time and stderr is empty.
No DSP code changed. This closes these isolated gain/timing requirements;
default loudness, musical-program behavior, external host wiring, zipper and
whole-board CPU acceptance remain separate requirements.

Prepared-root peer review subsequently confirmed endpoint indexing, hyperbolic
identities and RT bounds, but requested a physical bracket rather than relying
solely on the table's initial-guess accuracy. The revision prepares513 diode-only
upper bounds (another4,104 bytes) and brackets each Newton update; a step outside
the bracket uses bisection. A grid query uses the next current node's upper
bound, avoiding a new `asinh` call on the audio thread. Out-of-domain queries
retain the analytical bound. The iteration cap remains8 and the measured
residual still determines qualification; no universal convergence guarantee is
claimed for every possible guarded parameter combination.

The revised helper passes **4cases /60,223 checks** under ASan+UBSan+leaks and
allocation instrumentation, including10,000 queries across18 conductance
decades checked against the independent oracle and physical upper bound.
The same moving-input benchmark retains voltage difference<8.3e-13V and
residual<7e-18A. Both original and bracketed measurements are retained;
`prepared-diode-bracket-{tests,stderr,measurements}.txt` records the revision.
No pedal uses the helper yet. Original Muff component tests plus the new
moving-capacitor baseline pass49,610 checks before any substitution.

## Original named harmonic fixtures at their stated control positions

`measure-original-named-thd.cpp` now renders the original DS-1/TS808/SD-1/
Distortion+/RAT/Deluxe drive positions at44.1/48/96/192k, one second settling
and one second coherent1k measurement, noise disabled. The fixture records
both peak and RMS interpretations of the stated input level instead of silently
resolving source ambiguity. Distortion+ uses100mV andOUTPUT=1; RAT uses10mV at
drive0 and100mV at.5/1 withFILTER=0; SD-1 uses-24dBFS and measuresTHD+N as
specified. DS-1/TS808/Deluxe use-20dBFS. Voltage inputs use each candidate's
documented digital-to-voltage scale. THD is harmonics2..10; THD+N is residual
power after removing the coherent fundamental and DC. No thresholds changed.

Results: **56/136 individual harmonic-band fixtures pass;80 fail**. This is
acceptance evidence, not a passing unit-test summary or qualification of any
model. At48k with the stated level interpreted as peak:

| Model | Drive positions | Measured harmonic metric | Original bands |
|---|---|---|---|
| DS-1 |.2 /1|150.050% /211.862% THD|1–6% /25–40%:bothFAIL|
| TS808 |.2 /1|6.059% /13.534% THD|.5–5% FAIL /12–35% PASS|
| SD-1 |0 /.25 /.5 /.75 /1|10.003 /26.850 /27.384 /27.267 /26.936% THD+N|.5–2 /3–8 /8–20 FAIL;15–30 /20–40 PASS|
| Distortion+ |.25 /.5 /1|17.981 /25.459 /39.301% THD|5–15 FAIL;10–35 /25–60 PASS|
| RAT |0 /.5 /1|.000090 /39.926 /42.335% THD|<1 PASS;5–30 FAIL;>25 PASS|
| Deluxe |.1 /1|76.658 /78.399% THD|5–20 /25–60:bothFAIL|

The same pass/fail pattern holds across the tested rates and both amplitude
interpretations. The source's DS-1 tone branches substantially suppress the
1k fundamental, so outputTHD can legitimately exceed100%; this is retained as
a failure of the stated output criterion, not reclassified as a pass at another
node. SD-1 asymmetry gates and Deluxe low-sustain behavior remain failures.
Raw measurements preserve all136 cases in `original-named-thd.txt`.

## WIP NYC/Wicker prepared-root trial

After the independent review of3fd1455e's physical bracket, the two shared
MuffClipStage instances in each channel now use prepared initial guesses and
bracketed refinement. The original TPT capacitor-voltage/current history,
component values, tone, gain calibration and maximum8 iterations are unchanged.
The previous "no pedal uses the helper" statements above describe the isolated
checkpoints and are superseded by this **unregistered candidate-only** trial.
Timefx reviewed the revised bound/iteration mechanism and RT properties; it did
not rerun the oracle suite or qualify the complete pedals.

The original component oracle plus a new32,768-sample moving-R/C oracle passes
the same<1e-10V and<1e-12A limits. The complete named suite now passes
**28cases /11,314,802 checks**, ASan+UBSan+leak detection and callback allocation
counter, empty stderr. No callback allocation was introduced; table preparation
is off-thread. Evidence:`named-prepared-root-tests.txt` and
`named-prepared-root-stderr.txt`; the existing named-suite script reproduces it.

The actual two-model stereo automation corpus covers262,144 values at four
rates, changing sustain/tone/volume/Wicker controls and hostile-input recovery.
Maximum difference from the previous direct solver is3.5721e-13; largest RMS
difference9.0026e-15(-280.91dBFS). This is numerical agreement on that corpus,
not a fresh alias pass. The exact probe and comparison script are retained in
`measure-muff-prepared-root.cpp` and `compare-muff-root.py`; baseline processor
code is at50cf3a1f, before this candidate substitution.

At48k, stereo128-frame callbacks, O2/no-fast-math/threadCPU, the three-trial
medians changed from25.5782% to21.0234% for NYC and25.1301% to22.1339% for Wicker.
Both still **FAIL** their2%/2.5% budgets. Off-thread prepare increased from
about20microseconds to~.4seconds per pedal; four separate tables total558,144
bytes of coefficient storage per stereo pedal, excluding object metadata.
This startup/memory cost is material and remains a candidate limitation.
All original THD/alias/calibration/CPU acceptance failures remain visible.

The subsequent ownership-only revision shares one immutable root table across
all four NYC/Wicker stages in each stereo pedal. `shared_ptr<const ...>` copies
and releases occur only in prepare/destruction; process dereferences the same
const coefficients and each stage retains its own capacitor history. There is
no global table cache or audio-thread ownership mutation. Standalone component
preparation can still create its own table. Unprepared processing returns zero.

The sharing fixture checks no additional allocation when attaching prepared
coefficients, survival after releasing the caller's local handle, independent
histories, and resetting one stage without altering another. The complete named
suite passes **29cases /11,314,806 checks** under ASan+UBSan+leak detection and
the allocation counter. The262,144-value corpus is **bit-exact** against the
four-table candidate at every tested rate. Coefficient storage falls to139,536
bytes per stereo pedal and prepare measures~.095–.105seconds. New process-only
CPU medians are19.3017% NYC and18.8928% Wicker; both original budgets still fail.
Evidence:`named-shared-root-{tests,stderr}.txt`, `muff-root-shared-measurements.txt`
and `muff-root-sharing-comparison.csv`. Earlier tables and raw results remain
as before/after evidence.

Timefx independently reviewed the sharing mechanism: no reference-count
operations in process, immutable coefficients, and independent reset/state.
That review did not rerun the recorded corpus or qualify acoustic/CPU gates.

## Reviewed shared LFO wrap optimization

Timefx helper8dc4ca07 replaces `p-=floor(p)` in advance with one conditional
subtraction. Independent review confirms the public setters keep phase in[0,1]
(including a tiny negative value rounding to1) and increment in[0,.49], so the
floor can only be0 or1. The new operation preserves floating-point association.
Only the helper and test were imported here; timefx's coverage file was not
modified. A fresh local ASan+UBSan+leak-detection run passes **1,342,784 bitwise
oracle checks** across rates, boundary/hostile phases, setters and reprepare;
stderr is empty (`lfo-wrap-reviewed-{tests,stderr}.txt`). No latency, transfer,
state layout or callback ownership changes were introduced.


## E69 prepared-memory admission inventory

The additive E69 contract is implemented for all 14 DSP-owned processors.
Boost, Overdrive, Fuzz, Compressor, TS808, DS1, SD1, Distortion+, RAT and Green
Russian have only embedded prepared state and report `sizeof(model)`. GenericA
registrations supply their estimator callbacks. Named registrations remain empty;
these reports do not qualify any named model's outstanding acoustic or CPU gates.

Dynamic fresh-instance formulas include `sizeof(model)` plus:

- Dyna Comp: two delay vectors of `(23+16)*sizeof(double)` each.
- NYC: one immutable `PreparedDiodeRoot` object plus its 33×513-double vector,
  counted once across the four sharing clipping stages.
- Wicker: the same single root allocation plus two delay vectors of
  `(ceil(sampleRate*.005)+18)*sizeof(double)` each.
- Deluxe: four delay vectors of `(reportedOversamplerLatency+18)*sizeof(double)`
  each, with the exact current HQ/policy selection (128/136/140 samples).

Actual getters use vector **capacity**, including retained storage after a smaller
reprepare. Preflight bounds apply to fresh factories, as required by the pool;
repreparing an existing NYC/Wicker can temporarily retain its previous root while
constructing the replacement. The dynamic estimators reject nonfinite rates,
rates outside 8–192kHz, and nonpositive blocks before conversion. Blocks of 4096
are supported. Embedded-only estimators require no rate-dependent allocation or
conversion. All queries are allocation-free and all DSP processing is unchanged.

E69 measures logical owned allocation payload: embedded state is already covered
by `sizeof(model)`; ordinary stack scratch, allocator bookkeeping and shared_ptr
control-block metadata are excluded by the engine owner's explicit contract.
Fresh preparation uses no additional heap scratch, so `temporaryBytes` is zero.
The 257×16-double (32,896-byte) static shared sinc coefficient table remains a separate global inventory,
not a per-pedal allocation. Fresh vector capacities were checked against the
formulas on this Linux STL; other platform builds must repeat the same admission
suite rather than assume allocator capacity policy.

Tests were added before implementation and failed to compile because all model
`prepareMemory` methods were absent. The focused reproducible script is
`pedalboard-dsp-evidence/run-dsp-memory-tests.sh`; it includes all 14 processors,
ASan, UBSan, leak detection and the allocation counter. It checks preflight before
factory construction, actual capacity equality, reset/reprepare retention, all
mandatory rates plus 8kHz, all policies, invalid dynamic input rejection, and
Deluxe null/default/explicit HQ configuration. Final result recorded below.

Final focused run: **4 cases / 1,825 checks, zero failures**, ASan+UBSan+leak
detection, empty stderr. Allocation-counter checks pass for both preflight and
actual getters. GenericA registration also passes a standalone syntax compile.
The expanded hostile-parameter test caught an estimator-only defect: mapping
NaN directly through `fromNormalised` selected HQ off, while `PedalBase::setParam`
ignores nonfinite values and retains the fresh default HQ on. The estimator now
matches the setter for NaN and infinity; the failing output is preserved in
`dsp-memory-before-fix.txt`, with the final run in `dsp-memory-tests.txt`.

The engine owner independently reviewed all four dynamic formulas, the shared
root count, fresh-instance scratch convention and actual retained-capacity
reports; no counting mismatch was found. This review did not independently
rerun the sanitizer suite. Cross-platform admission checks and pool integration
remain the coordinator/engine build responsibility.


## Dyna Comp exact release-endpoint cache

The processor now prepares the existing exponential retention coefficients for
1000ms and 300ms once at its actual OTA internal sample rate. The original
program-dependent release formula still computes its duration every sample;
exact endpoint matches reuse the cached coefficient and the middle range keeps
the original exponential. Reset preserves these rate-dependent constants;
reprepare recomputes them. No polynomial approximation, gain law, threshold,
time constant or acoustic criterion changed.

Against baseline378c8913, an actual-processor corpus of **789,504 double values**
is bit-identical across44.1/48/96/192kHz, all three quality settings, independent
stereo inputs, sensitivity/output/attack/mix/noise automation, reset, and
nonfinite input recovery. Binary SHA256 and before/after measurements are saved
in `dynacomp-release-cache-*.txt`; the reproducible probe/script are adjacent.
A dedicated descending/ascending-rate and quality reprepare regression compares
reused versus fresh processors and verifies the E69 memory reporter includes the
two new embedded doubles. The focused suite passes **4cases /81,937checks** with
ASan+UBSan+leak detection and callback allocation counter, empty stderr.

Process-only threadCPU,48k/stereo128/O2/no-fast-math,100seconds of audio per trial,
three trials: median1.439583% before versus1.189473% after (~17.4% reduction).
Checksums match exactly. Both results exceed the unchanged0.5% numerical target. The coordinator has
identified concurrent native-build contention under a4-core quota, so these
fresh timings are exploratory and cannot qualify the absolute CPU gate. These
fresh measurements use independent stereo sine frequencies and do not replace
older probes with different signals. All existing timing/THD/alias qualification
limits and failures remain in force; Dyna Comp remains unregistered.

Timefx independently reviewed the cache lifecycle and exact endpoint arithmetic:
both lanes share the prepared internal rate, reset retains valid coefficients,
and reprepare updates them after rate/quality changes. No RT/ownership issue was
found. This was a read-only review, not an independent rerun of the corpus or a
CPU-budget approval.


## Explicit descriptor CPU budget scale

All 14 DSP-owned descriptors now append `cpuUnitsPerPercent=1000` under the
engine owner's additive metadata contract. Existing values are unchanged:
Boost300, Overdrive1000, Fuzz1200, Compressor300, TS808400, DS1350, SD11200,
Distortion+800, RAT1200, Dyna500, NYC2000, Wicker2500, Green Russian1500,
Deluxe2500. Thus DS1 denotes0.35% and NYC2%, not350% or2000%. These are the
specified budget allocations, **not measured achieved CPU**; earlier measured
failures remain unchanged. Named models remain unregistered. The engine owns
normalization and whole-board admission, including other families' different
explicit unit scales.

The linked descriptor fixture verifies all original amounts and all 14 scales:
**28 checks, zero failures**. Reproduce with
`pedalboard-dsp-evidence/run-dsp-cpu-units-tests.sh`; raw output is adjacent.
No audio processing, allocation, state serialization or threshold changed.


## Independently reviewed paired biquad helper

Timefx helpera56bf0e9 (locald35ce0f8) adds a static two-filter processing entry
point. SSE2 lanes retain each filter's existing direct-form-II association and
finite/denormal guards. Passing the same filter twice takes the explicitly
sequential scalar path. FMA/AVX2/fast-math targets use scalar processing to retain
the compiler's scalar arithmetic contract. Existing single-filter processing,
shelf slope, peak response, storage layout and reset are unchanged.

Independent read-only inspection found no mask, lane, state-order or aliasing
issue. Fresh local runs each pass **933,888 bitwise checks**: O1 ASan+UBSan+leak
detection, and O3 `-mavx2 -mfma` fallback. Both stderr files are empty. Raw outputs
and `run-biquad-pair-review.sh` are in the evidence directory. The oracle covers
seven rates, coefficient changes, resets, hostile inputs and shared-object use.
Windows/ARM compiler behavior still requires those platform builds.

This is helper acceptance only. Timefx measured no actual DD3T CPU improvement
and reverted its product use; no pedal CPU gate is closed by this helper.


## Single-exponential shunt-diode candidate (performance unqualified)

`NewtonClipper` retains the original monotone equation, initial upper bound,
eight-iteration limit and1e-12 residual stopping condition. The candidate uses
`em=expm1(v/Vt)` to calculate `sinh=.5*(em+em/(1+em))` and
`cosh=1+.5*em*em/(1+em)` from one transcendental call. Under the public guarded
range, `v/Vt<=asinh(16/1e-24)<60`, so the intermediate square is finite; expm1
preserves the small-voltage limit. The independent public residual still uses
std::sinh. No table, allocation, circuit component or state layout changes.

A new guarded-domain oracle passed before and after the revision: **143,763
checks** each, ASan+UBSan+leaks, empty stderr. It retains the previous algorithm
as one reference and a90-step long-double monotone KCL solve as an independent
reference. Maximum permitted voltage error is2e-12 and KCL residual2e-11,
including germanium/silicon/LED parameters and hostile-input recovery. The
complete named suite then passed **29cases /11,314,806checks** with sanitizer,
leak detection and callback allocation counter; stderr is empty.

The actual DS1/Distortion+/RAT/Deluxe corpus covers524,288 output values across
four rates, variant selection, all continuous-control automation, reset and
hostile inputs. Maximum differences are4.44e-16,2.22e-16,4.44e-16 and3.322e-13
respectively; detailed RMS/count evidence is in `newton-model-comparison.json`.
These differences do not establish hardware accuracy or an alias pass.

Fresh CPU timings were collected during coordinator native-build contention
under a4-core quota. They are **exploratory, not acceptance evidence**. Median
before→after percent figures: DS114.772→14.289, Distortion+22.785→21.409,
RAT37.286→38.507, Deluxe23.944→22.345. There is no consistent demonstrated
whole-model speed benefit, and no CPU gate is closed. Further timing is paused
until the coordinator releases the build reservation. The candidate is preserved
for evaluation, not requested for integration into the audited product baseline.

Timefx independently reviewed the expm1 algebra, bounded exponent, Newton upper
sequence and RT ownership; no mathematical/RT finding. That review did not rerun
the oracle or waive actual-model acceptance. All original acoustic/timing/CPU
failures remain visible and the ten named processors remain unregistered.
