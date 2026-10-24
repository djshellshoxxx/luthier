# Reference pedal DSP coverage — 2026-10-24

Owner: WP7, WP8 Generic A, WP11–13 drive/dynamics. This is an implementation and numerical acceptance ledger, not a hardware-fidelity claim. Original numeric acceptance thresholds remain in force. Candidate changes below are **not release accepted** while their gates remain open. Parent coordinator owns serial specification amendments and global integration.

## Delivered checkpoints

- `f25ebd8e`, `72c2c689`, `e3b9c5a3`: double DSP primitives, bounded Newton solver, interpolation/smoothing/filter/envelope/LFO/noise/tempo tests and ownership markers.
- `63165935`: Generic Boost, Overdrive, Fuzz, Compressor; descriptor/registration, smoothing, finite guards, deterministic state and allocation-counter tests. No reference hardware algorithms are substituted with these generic processors.
- `c72cab40`: separate 31-tap 2x short FIR path for prebandlimited wet saturation, exactly 15 base samples; factor1 bypass has zero latency. This does not replace the high-gain drive alias gate.
- `1df2c15a`: optional 4x short path with a 33-tap second stage, exactly23 base samples; existing2x coefficients and processing remain unchanged.
- E40 helper by timefx `5f0b56e4` (local cherry-pick `8145813b`), reviewed here:16-tap Blackman-windowed sinc,257 prepared phase rows, linear phase interpolation, DC normalization, valid delay>=8 samples. Existing cubic API unchanged. Independent dense scan over four rates,257 phases,281 frequencies0–14kHz found worst deviation0.003349171dB, preserving the original±0.5dB gate. Timefx `db43be32` (local `fa5f2a70`) replaces bounded ring modulo with one conditional wrap; independent review confirms all supported ages remain less than storage length.

## Candidate E37 — original drive oversampling gates retained

Legacy `Source/DSP/Common/Oversampler.h` is unchanged. Reference-only `OversamplerAdapter` uses fixed-storage double linear-phase halfbands:257-tap Kaiser(beta6.5) first stage,129-tap Blackman-Harris upper stages. Factor1/2/4/8 reports0/128/160/176 base samples; impulse maxima match. Generic nonlinear descriptors advertise176 maximum. Preparation computes coefficients; process/reset allocate nothing. Halfband symmetry, four accumulation lanes, factor-specific processing and lazy waveform selection reduce work. Identical stereo histories process one channel until the first differing input; then all channel state is copied before processing that sample. Bit-exact comparison against independent mono instances validates the transition.

Original wrapper was not linear phase with exact integer group delay and measured approximately−5.1317dB at20kHz. Candidate preserves original±0.3dB20Hz–20kHz acceptance at44.1 and48kHz and exact impulse/PDC expectations. It does **not** yet satisfy the original alias/CPU gates. Peer timefx reviewed indexing/history/latency composition and deferred stereo state copy; no RT allocation or lock issue identified. That code review does not waive numerical gates.

Coherent spectral measurement:48kHz,1second warm-up plus1second capture,5kHz sine with1.0 peak amplitude, max drive, High8x. Remove DC and the genuine in-band harmonic bins5/10/15/20kHz(±1bin); report largest remaining spur. This full-scale stress input is explicit; it is not a low-level nominal guitar test.

| Processor | Largest spur | dBFS peak | dBc relative to fundamental |
|---|---:|---:|---:|
| Boost |19kHz|−53.286398|−49.382294|
| Overdrive |1kHz|−58.249875|−34.106238|
| Fuzz |19kHz|−40.221418|−36.743785|

These fail the applicable original alias thresholds; no threshold was changed. Longer final FIR does not remove nonlinear harmonics that have already folded inside the8x domain. Parent independently investigates second-order ADAA; neither that experiment nor first-order compensation is included in this checkpoint.

CPU measurement: `CLOCK_THREAD_CPUTIME_ID`, C++17 `-O2 -DNDEBUG`, no fast-math,48kHz/128,2seconds audio, Boost/OD/Fuzz/Compressor repeated twice, default params, Normal4x, equal left/right sine guitar input. Latest0.178262seconds CPU =8.913% of one core, above the original8% eight-pedal gate. This is a correlated mono workload; true stereo requires more work and is not claimed to pass. CPU frequency normalization to the spec3GHz reference has not been established. Source in `pedalboard-dsp-evidence/measure-generic.cpp` records the workload and clock. No CPU gate is relaxed.

## Candidate E38 — explicit numeric contradiction reconciliation

Boost original mandatory20Hz first-order HP is−3.0103dB at20Hz, incompatible with simultaneous±0.3dB20Hz–20kHz clean-flat acceptance. Coordinator authorized2Hz HP while retaining the original flatness threshold. Ideal20Hz loss becomes−0.0432dB. This increases low-frequency content and makes DC settling approximately10times slower; it is a tonal/time-response change, not merely an optimization. Clean mode,0dB gain, tone at its neutral value is the flat reference. Fat mode uses a second-order4dB low shelf plus first-order2dB shelf: the specified total third order and approximately6dB boost, not two biquads masquerading as third order.

Overdrive literal default chain measured+12.121427dB at1kHz/−18dBFS peak, conflicting with original±3dB reference unity. Original drive endpoints measured−1.6622 and+13.8774dB, exceeding the original6dB loudness-span gate. Coordinator authorized drive-dependent scalar reference normalization, with those numeric gates unchanged. A101-entry table, prepared off RT from256-point sine quadrature, includes the specified split bass gain, original compensation, soft-clip tanh and fundamental tone magnitude. Linear interpolation in drive controls only an output scalar, preserving the distortion spectrum's harmonic ratios at fixed controls. Calibration reference:1kHz,−18dBFS **peak**(0.125892541 amplitude,−21.0103dBFS RMS relative to1.0), default soft clipping/bass/tone, output0dB. This is not a hardware calibration and does not normalize every mode or arbitrary guitar waveform.

Tests retain±3dB reference unity and≤6dB drive span at44.1/48/96/192kHz, drive0/10/16/20/30/40dB. A separate test evaluates the alternate−18dBFS **RMS** interpretation(peak amplitude√2 greater), retaining default unity and full-drive span. Peer review requested this peak/RMS distinction explicitly. E38 remains coupled to the unaccepted E37 path for full-bandwidth acceptance.

## Dyna Comp candidate

The candidate follows the specified **estimated** feedback OTA gain law, sensitivity, diode detector, program-dependent release, optional deterministic noise, mix and Mini attack switch. Display name explicitly says estimated. The algebraic biased-tanh identity makes noise-off silence exactly zero. Short FIR reports15 samples(default2x),23 samples(HQ4x),0(off), with matching dry delay. Scope is an estimated algorithm from the spec, not hardware emulation validated by captures.

Basic finite/block/silence/feedback-bound tests exist. Still open: original static-curve comparison to simulation, attack/release numerical acceptance, FRF/THD/IMD, alias−60dBc,0.5%CPU,30minute soak, zipper-energy gate, hardware captures and variant impedance loading. The literal output mapping−60..+20dB produces−20dB at the default0.5 knob; no undocumented unity correction was applied. Only Mini attack differs by variant; M102 versus Script76 component variation is not represented. Board owns bypass transition; this model must not be presented as fully accepted while these items remain open.

## Coverage and exact omissions

Implemented helpers: smoothers, one-pole and shelf, biquad, TPT SVF, DC block, envelope, fractional delay(cubic+sinc), LFO/table sine, tempo sync, deterministic noise, soft clipping, bounded memoryless Newton, long candidate and separate short oversamplers. Dynamic symmetric feedback-diode support is a separately tested circuit primitive, not a registered TS808 or SD1 model.

WP8 Generic A structural coverage: double audio/state, normalized descriptor mapping, parameter ramps and coefficient updates on absolute sample clock, bit-exact block partitioning, hostile finite recovery, deterministic reset, no callback allocations. Generic Compressor supports explicit sidechain buffer lifetime: pointers are consumed only by the immediately following process call; absent/stale external input passes dry and reports unavailable, never silently falls back to internal detector. Host/engine sidechain wiring and disabled UI availability remain integration work owned elsewhere. Board owns latency-matched5ms bypass; processors do not add a second fade.

Not implemented in this worker: DS-1, TS808, SD-1, Distortion+, RAT, Big Muff Pi, Green Russian Muff, Op-Amp Muff, Deluxe Big Muff. Those source pairs remain skeletons and are not generic aliases. Reference TS808 spec requires further explicit reconciliation: its literal Rf/Zg circuit is not unity below100Hz at maximum drive(the80Hz gained leg is substantial); the bass-clean≤2dB endpoint condition cannot be inferred from the stated R/C values. Its small-signal diode conductance also affects the quoted unshunted resistor gain. No reference pedal gate is weakened to hide these conflicts.

BbdCore, LooperCore and GestureDecoder belong to timefx/engine; hub/host/UI/global registry files are not owned or edited here. Full application builds, PDC/bypass integration, UI/state/migration/MIDI routes, audio-device xruns and release acceptance belong to coordinator integration. No hardware captures were available, and no faithful-hardware claim is made.

## Test evidence

Focused native runner links the repository test framework and JUCE core only, with `LUTHIER_ALLOCATION_COUNTER=1` and thread-local global allocation interceptors. Rates44.1/48/88.2/96/192kHz, partitions1/16/37/64/481/2048 versus single-sample processing; input includes NaN,Inf,±1e6 and subnormals. Before the latest additional shelf/RMS/circuit tests,22 test cases produced2,062,694 checks,zero failures. Detailed latest result is saved alongside this ledger. Structural success does not turn the failing spectral/CPU measurements into passes.
