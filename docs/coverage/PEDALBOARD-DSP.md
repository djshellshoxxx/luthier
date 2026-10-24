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

Basic finite/block/silence/feedback-bound tests exist. Actual48kHz measurements: default1kHz/−20dBFS peak THD(2..10)=0.183991%(passes0.1–1%); max sensitivityTHD=0.016624%(does not reach the stated3–5% range). Default-sensitivity5kHz/0dBFS peak alias=−102.7797dBc(passes−60). The sampled static curve was monotonic over−50..0dBFS in10dB increments at sensitivity0.25/0.5/0.75/1; maximum measuredGR35.695dB. This does not provide the missing simulation comparison. At−30→−10dBFS, the gain-law feedback makes measured63%GR attack0.479ms Fast/1.396ms Slow despite internal5/15ms envelope coefficients; the original5ms±50% Fast gate fails. Recovery did not reach the63%GR target within the1second observation window; it is censored, not reported as an exact1second recovery. An extended recovery measurement remains necessary. Thread CPU:0.025755s/2s=1.287770% with independent stereo signals, default2x/noiseoff48k128/O2; the original0.5% gate fails. Parameter-coefficient caching and reducing meter logarithms to once per block preserved captured audio bit-for-bit against the preoptimization run. After the independently reviewed short-polyphase helper and bounded-error mild-tanh fast path, stereo CPU measured1.083035%, still above0.5%; captured default THD and alias-waveform differences were below3.4e−16 versus the previous algorithm. Raw results and measurement source are in the adjacent evidence directory.

Still open: original static-curve comparison to simulation, attack/release numerical acceptance, FRF/THD/IMD, alias−60dBc,0.5%CPU,30minute soak, zipper-energy gate, hardware captures and variant impedance loading. The literal output mapping−60..+20dB produces−20dB at the default0.5 knob; no undocumented unity correction was applied. Only Mini attack differs by variant; M102 versus Script76 component variation is not represented. Board owns bypass transition; this model must not be presented as fully accepted while these items remain open.

## Coverage and exact omissions

Implemented helpers: smoothers, one-pole and shelf, biquad, TPT SVF, DC block, envelope, fractional delay(cubic+sinc), LFO/table sine, tempo sync, deterministic noise, soft clipping, bounded memoryless Newton, long candidate and separate short oversamplers. Dynamic symmetric feedback-diode support is a separately tested circuit primitive, not a registered TS808 or SD1 model.

WP8 Generic A structural coverage: double audio/state, normalized descriptor mapping, parameter ramps and coefficient updates on absolute sample clock, bit-exact block partitioning, hostile finite recovery, deterministic reset, no callback allocations. Generic Compressor supports explicit sidechain buffer lifetime: pointers are consumed only by the immediately following process call; absent/stale external input passes dry and reports unavailable, never silently falls back to internal detector. Host/engine sidechain wiring and disabled UI availability remain integration work owned elsewhere. Board owns latency-matched5ms bypass; processors do not add a second fade.

Not implemented in this worker: DS-1, TS808, SD-1, Distortion+, RAT, Big Muff NYC, Tone Wicker, Green Russian Muff, Deluxe Big Muff. Those source pairs remain skeletons and are not generic aliases. Reference TS808 spec requires further explicit reconciliation: its literal Rf/Zg circuit is not unity below100Hz at maximum drive(the80Hz gained leg is substantial); the bass-clean≤2dB endpoint condition cannot be inferred from the stated R/C values. Its small-signal diode conductance also affects the quoted unshunted resistor gain. No reference pedal gate is weakened to hide these conflicts.

BbdCore, LooperCore and GestureDecoder belong to timefx/engine; hub/host/UI/global registry files are not owned or edited here. Full application builds, PDC/bypass integration, UI/state/migration/MIDI routes, audio-device xruns and release acceptance belong to coordinator integration. No hardware captures were available, and no faithful-hardware claim is made.

## Test evidence

Focused native runner links the repository test framework and JUCE core only, with `LUTHIER_ALLOCATION_COUNTER=1` and thread-local global allocation interceptors. Rates44.1/48/88.2/96/192kHz, partitions1/16/37/64/481/2048 versus single-sample processing; input includes NaN,Inf,±1e6 and subnormals. Latest complete run:34 test cases,2,889,786 checks,zero failures, including shelf/RMS/circuit/ADAA2 tests and Dyna RT lifecycle allocation interception. Detailed latest result is saved alongside this ledger. Structural success does not turn the failing spectral/CPU measurements into passes.

## Additional independent oracle evidence

Short31/33-tap polyphase optimization `dcef63b3` retains identical coefficient design and15/23-sample latency; full-tap scalar nonlinear-convolution oracle covers8192 samples per2x/4x within1e−14, plus all-rate passband/latency checks:16,419 checks,zero failures. Peer timefx independently derived both parity cases and reported actual-chain spectra unchanged at reported precision.

Long candidate upper-stage33-tap transfer measured passband minimum−0.0000799dB through20kHz at stage2fs176.4kHz and image-band maximum−100.724dB above68.2kHz. Its independent full-tap convolution oracle uses a separate standard-library Kaiser Bessel calculation and a generic upsample/filter/downsample pipeline with nonlinear tanh17x; errors remain below1e−13 for2x/4x/8x. Combined long helper/oracle37,503 checks,zero failures. Scalar fallback was separately compiled with `-U__SSE2__` and retains passband/impulse latency. Cross-platform SIMD bit identity is not required or claimed; block partition identity within each compiled implementation remains mandatory.

## Remaining generic numeric coverage

The structural suite does not establish every tonal acceptance criterion. Not yet exhaustively measured: clean-boost THD limit; OD THD monotonicity/asymmetric harmonic levels/tone-corner tolerance; Fuzz bias RMS monotonicity, gate attack, Germanium10–25% sag and max-fuzz THD; compressor attack/release and dynamic linked/manual behavior. The Fuzz literal sag formula has an additional apparent conflict: its raw envelope cannot exceed a sustained−10dBFS peak sine's0.31623 amplitude, so `1−0.2*env` reduces the combined pre-gain by at most6.325%(or8.945% if−10dBFS is RMS referenced to1.0), below the stated10–25% sag range. No undocumented envelope normalization has been added; actual output-sag measurement/intent reconciliation remains open.

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
  is normalized at L=.55, yielding 13.126 dB maximum, rather than silently changing
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
