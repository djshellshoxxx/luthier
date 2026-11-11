# Experimental static-tanh ADAA2 helper — 2026-10-24

`Common/TanhADAA2.h` is a standalone second-order divided-difference antiderivative helper. It is **not integrated into generic/reference pedals**, does not change a pedal's acceptance criteria, and is not a replacement for the clean boost's rational fourth-root shaper or the fuzz's composite saturation/starvation law.

The mathematical family is described in [Bilbao, Esqueda, Parker and Välimäki, Antiderivative Antialiasing for Memoryless Nonlinearities (2017)](https://www.research.ed.ac.uk/en/publications/antiderivative-antialiasing-for-memoryless-nonlinearities/). The earlier [Parker, Zavalishin and Le Bivic DAFx2016 paper](https://dafx.de/paper-archive/2016/dafxpapers/20-DAFx-16_paper_41-PN.pdf) motivates antiderivative filtering and explicitly discusses its delay and spectral effects. This implementation uses the symmetric second divided difference `2 F2[x[n],x[n-1],x[n-2]]`, with `F2''=tanh`, rather than claiming every second-order formulation is identical.

## Interface and exact delay contract

- `prepareFactor(1/2/4/8)` resets history and selects padding; the helper **does not upsample**. Call `process` once for every high-rate sample inside the caller's oversampling path.
- The exact small-signal limit is `(x[n]+x[n-1]+x[n-2])/3`, with one high-rate sample of group delay and three-tap amplitude droop.
- An additional `factor-1` high-rate samples of fixed-array padding makes the total linear-limit delay exactly one base-rate sample. `latencyHighRateSamples()` returns factor; `latencyBaseSamples()` returns1. Cascading two padded helpers adds two base samples.
- There is no silent droop compensation. At20kHz/44.1kHz base rate, even8x's three-tap response is approximately−0.37dB, already beyond the clean boost's±0.3dB bandwidth gate. A caller must explicitly resolve that requirement before adoption.
- Input gain/bias automation goes before this **static** shaper. History stores the actual input coordinates and primitives; there is no primitive cached under an old gain/bias configuration. A mutable waveshaping function would require a separate API and current-parameter recomputation of historical primitives.
- All audio/state is double, storage is fixed, finite input guards map nonfinite values to0 and clamp finite inputs to±1e6, output remains in±1. There are no process allocations, locks, I/O or lazy initialization.

## Numerical construction

For|x|<=1, a degree12 Chebyshev approximation evaluates the even analytic function `F2(x)/x^3` in `x^2`. Coefficients came from the coordinator's independent high-precision fit; primitive absolute error alone is not used to certify the divided differences. For larger magnitudes, the primitive uses the quadratic/log2 term plus a16-term dilogarithm series in `-exp(-2|x|)`.

Three nodes are sorted because the divided difference is symmetric. Close pairs use a symmetric secant expansion through separation^6 with the analytic first primitive and tanh derivatives. A total span<0.01 uses a sixth-order Taylor/complete-homogeneous-polynomial expansion. In same-sign large regions, the known quadratic and linear primitive terms are removed analytically before differencing; only the exponentially small tail is differenced. Separated repeated nodes use their correct confluent limit: `(100,-100,100)` is near0.5, not `tanh(mean)` near1. No denominator is divided by zero.

## Independent evidence and retained limitations

Persistent tests include86 high-precision reference triples, repeated/near-repeated nodes across approximation boundaries, exact padded impulse and three-tap linear-limit checks,100,000 slow/rapid-gain input samples, hostile finite/nonfinite values and exact reset silence. The initial helper-only run passed201,323 checks. The full focused suite including this helper, current DSP candidates and allocation interception passed34 test cases /2,889,786 checks with zero failures.

`pedalboard-dsp-evidence/generate-adaa-fixtures.py` independently computes100-digit mpmath/dilogarithm divided differences. A larger1,860-case run covers very close/repeated nodes, transition boundaries, randomized±100 inputs,±1e6 input, and slow/abrupt gain-and-offset automation histories. Worst absolute output error was **2.9624080966073052e−12**. The worst-case log is saved alongside the generator. This is numerical evidence over that finite set, not an exhaustive proof for all doubles.

Actual candidate FIR plus helper: first257-tap Kaiser / upper33-tap Blackman-Harris,48kHz base rate, coherent5kHz sine with100 peak amplitude into tanh,1second warm-up plus1second capture. Exclude genuine harmonics5/10/15/20kHz and DC, then report the largest false harmonic:

| Factor | Combined impulse peak / declared latency | Largest false harmonic dBFS | dBc |
|---|---:|---:|---:|
|2x|129 /129|−46.299206|−48.318042|
|4x|137 /137|−71.139761|−73.218336|
|8x|141 /141|−89.627139|−91.720325|

This is a **static tanh chain**, not a generic pedal acceptance result. With a separate ideal1025-tap final FIR at8x, the implemented helper measured−89.354dBFS largest false harmonic, high-rate peak1.0. The complete actual-chain source is `pedalboard-dsp-evidence/measure-adaa.cpp` and requires the explicitly unaccepted E37 FIR candidate to reproduce those exact combined-latency values.

Process-only `CLOCK_THREAD_CPUTIME_ID`, C++17 `-O2 -DNDEBUG`, no fast-math, precomputed independent stereo inputs,48kHz,4x:0.034656CPU seconds/2seconds audio=1.732801% at gain0.1;0.022286seconds=1.114293% at gain100. Small-signal processing is more expensive here. These timings do not establish any full pedal or board CPU pass, and no3GHz normalization has been established.

Still required before product use: independent implementation review, all-rate actual-pedal spectral tests, original bandwidth/THD/alias/CPU gates, bypass/PDC integration of the added base sample, automation transition spectra, and treatment of non-tanh/composite nonlinearities. No original acceptance threshold has been relaxed.

An independent standalone probe reran all1,860 high-precision cases under AddressSanitizer and UndefinedBehaviorSanitizer with no reported errors (`-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer`).
