# Original release criterion: five-rate b5 survey

2026-10-25. Data-only investigation. No production code, damping, fixture windows or
acceptance thresholds changed. **The original whole-output 35 dB gate remains open.**

## Scope and retained oracle

25 original guitar-type selections × 44.1/48/88.2/96/192 kHz × two builds = 250 renders.
The baseline uses the immutable b5bec374 native objects. The second build changes only
CabinetEngine.cpp and IrLibrary.cpp from cabinet-key correction 71b8dffa. Both use the
same working factory bank established by the [preset setup diagnosis](LEGACY-NORMALIZATION-PRESET-SETUP.md).
All 1,253 original resource files are unchanged; the bank is a separately generated copy.

The original sequence is retained: reset; set guitar type; zero the three coupling
controls; body bypass; room off; apply parameters; four silent 256-sample blocks; velocity
100 note-on at sample zero; note-off at 0.5 seconds; render a 0.4-second tail. The note is
`lowestPlayableNote()+2`. RMS windows remain the original 30 ms before note-off and
250–280 ms after the scheduled note-off. The output guard, idle-RMS and minimum-level
conditions are unchanged. Raw-string attenuation is separately checked against 40 dB.

The private measurement harness is derived from exact b5 ComboHarness.h, under a distinct
namespace. Extra read-only taps capture pre-body string sum, post-amp/pre-cabinet, mic 1
and final mono output. Actual voice activity comes from StringActivityQueue. The amp aux
bus is enabled to expose its existing tap. No DSP samples or controls are rewritten by
the observer. A smaller DI-tap probe repeated three control types at all five rates;
all previously captured raw/amp/mic/output metrics matched exactly after adding that tap.

Actual fixture state matters: requested and effective guitar-type enums match in all
cases, but bass selections retain six strings and play note 42. The 12-String selection
reports twelve strings; all other selections report six. This is the original host-
parameter sequence, not an assertion that every native factory tuning was reconstructed.
No user-gesture replacement or low-bass note 30 was substituted.

## Results

Each build renders 10,536,750 frames. Every raw-string case passes 40 dB; the minimum
is **52.794220 dB**. Only PStyle at 44.1 and 48 kHz fails the original 35 dB output check.
Each 1,000-check measurement run has exactly these two output-gate failures; there are
no other observation/finite/timing failures. All 250 renders have zero nonfinite samples,
missing tap blocks or activity-queue drops. Host and engine reported latency agree.
Raw and pre-cabinet float words match bit-for-bit between the two builds over every frame.

| PStyle sample rate | Raw attenuation | Original output | Corrected-cabinet output |
|---|---:|---:|---:|
| 44.1 kHz | 54.758725 dB | **34.474360 dB** | **34.452378 dB** |
| 48 kHz | 54.755222 dB | **34.702611 dB** | **34.738995 dB** |
| 88.2 kHz | 54.695332 dB | 36.447045 dB | 36.440946 dB |
| 96 kHz | 54.700965 dB | 36.523405 dB | 36.540032 dB |
| 192 kHz | 54.660748 dB | 36.182285 dB | 36.208273 dB |

## What reduces the apparent attenuation

At 48 kHz, the original PStyle signal moves through these existing stages, measured in
exactly the same original time windows:

| Stage | Before RMS | After RMS | Attenuation |
|---|---:|---:|---:|
| Pre-body string sum | .05675151 | .0001038045 | 54.755222 dB |
| DI after circuit/input trim | .22284829 | .0004290937 | 54.309144 dB |
| Amp output | .74186920 | .0078882854 | 39.466894 dB |
| Cabinet mic 1 | .22903293 | .0036995309 | 35.835026 dB |
| Final output | .22984671 | .0042296821 | 34.702611 dB |

1. **Most attenuation is lost across the amp path, not at note release.** Its effective
   RMS transfer is 3.329× before release and 18.384× in the late tail: a 14.842 dB
   difference. This is consistent with the existing nonlinear amp compressing a strong
   held signal more than its small tail; these RMS ratios also include its filtering,
   so they are not a static waveshaper derivative. The preset uses American Twin,
   gain .35, master .70, input/master trim 0 dB. The source has cascaded nonlinear
   preamp/power stages and transformer soft clipping. PStyle drives the DI harder than
   the J-Style control (.22285 versus .16895 RMS), while both DI attenuation figures
   remain above 54 dB. No delayed/raw-release failure is needed to explain this part.

2. **The cabinet contribution is reproduced by its actual linear impulse response.**
   A separately reset, already prepared cabinet with the exact selected asset/settings
   was excited with a unit impulse. Across all ten rate/build combinations, the first
   sample above 1e-8 is sample 256, matching reported cabinet latency. Convolving the
   recorded amp output with this measured kernel reproduces the actual mic's late tail
   with RMS error 3.8e-10–1.12e-9. At 48 kHz it reproduces 35.835025 dB versus the actual
   35.835026 dB. The kernel's group delay at note 42 (92.4986 Hz) is 10.644 ms original,
   10.423 ms corrected, including the 5.333 ms fixed convolution latency. The additional
   frequency-dependent delay belongs to the IR/filter response. This accounts for the
   cabinet effect as ordinary linear response and timing, not unexplained tail growth.

3. **The final stage is likewise largely accounted for.** An offline replay of the
   source's cabinet 10 Hz DC blocker, master 5 Hz DC blocker and fixed 72-sample master
   lookahead predicts 34.717125 dB in the ORIGINAL windows; actual output is 34.702611 dB.
   The residual is .014514 dB. Across rates/builds the residual is below .0182 dB. This
   limited replay does not model the master limiter's gain-envelope history or every
   intervening float rounding. It is a path-attribution calculation, not a substitute
   output signal for acceptance. Neither acceptance window was moved.

The final 48 kHz tail is predominantly the played note: 78.90% of rectangular-window
spectral energy lies in 65–125 Hz, versus 84.50% before release. Only .62% lies in 0–35 Hz
in the tail. DC is therefore not the dominant cause of the failure. These are finite
30 ms window estimates, not resolved narrow spectral lines: intrinsic resolution is
about 33.3 Hz and rectangular leakage is retained. The analysis zero-pads 16× solely
for band integration, verifies Parseval energy, and reports fixed bands plus note-centred
fundamental/second/third-harmonic probes. It does not claim zero-padding adds resolution.
All bands and windows are retained, including results that do not support a hypothesis.

## Event timing and reported latency

The first actual voice note-off is exactly the scheduled 0.5-second sample in every case.
The observed on/off notes are both 42. The first note-on is delayed by the ordinary
Poly chord/scheduling path. `MidiInterpreter::getLatencySamples()` reports the chord
window; its held-note `handleNoteOff` releases at the incoming block offset. Thus that
reported MIDI latency is not evidence of a late note-off.

| Rate | Actual first note-on | Reported MIDI | Reported total | Total minus MIDI | Amp aux | Mic aux |
|---|---:|---:|---:|---:|---:|---:|
| 44.1k | 90 | 88 | 415 | 327 | 93 | 349 |
| 48k | 97 | 95 | 428 | 333 | 100 | 356 |
| 88.2k | 180 | 176 | 569 | 393 | 181 | 437 |
| 96k | 196 | 191 | 596 | 405 | 196 | 452 |
| 192k | 393 | 383 | 927 | 544 | 383 | 639 |

All values are samples and are constant across this fixture's 25 selections at each
rate. The exact 256-sample cabinet onset was measured; the other totals are the public
reported contract, not a claim that every IIR stage has a frequency-independent delay.
No latency value was used to shift a pass/fail window.

## Separate candidate finding, not a correction

The measured cabinet gain at 92.4986 Hz rises about 6.11 dB from 48 to 192 kHz in both
builds (original −9.8691 to −3.7623 dB). JUCE's installed convolution implementation
resamples first, then L2-normalizes to `0.125 / sqrt(sum(h*h))` when Normalise::yes.
That convention explains approximately +3 dB per rate doubling for the same continuous
IR; it differs from the explicit originalRate/processRate scaling in Normalise::no.
This is a concrete candidate for a separate sample-rate gain-invariance review. It
changes absolute gain but is not a demonstrated fix for this before/after decay ratio.
No normalization convention, IR data, level calibration or DSP code changed here.

The bounded investigation identifies no justified production damping correction. The
raw 40 dB requirement passes; the original whole-output proxy still fails for the two
cases above. Those failures remain release blockers unless separately resolved under
the unchanged requirements.

## Evidence and reproduction

Full executable/source/commands/audio artifacts:
`/workspace/luthier-evidence/release-survey-b5/`. Immutable objects:
`/workspace/luthier-evidence/native-b5-link-snapshot-20261025/`. Exact headers are the
archived b5 source used by the preceding normalization investigation. No changing root
objects were borrowed. CPU timing reads in the private render harness are disabled;
this is not a performance benchmark.

Committed [evidence directory](pedalboard-dsp-evidence/release-survey-b5/) contains:

- All 250 primary metrics, 30 extra-DI metrics, original failing checks and finite/timing evidence.
- All 20,000 band rows (`bands.csv.gz`), readable PStyle rows, kernel and downstream replay tables.
- Harness-generation script, three probe sources, analysis scripts and exact compressed
  compile/link command arrays. The generator reproduces both private headers byte-for-byte.
- Object/source/binary/resource provenance and SHA256 for every primary captured audio file.
- [Original PStyle 48k audio](pedalboard-dsp-evidence/release-survey-b5/pstyle48-original.wav)
  and [corrected-cabinet audio](pedalboard-dsp-evidence/release-survey-b5/pstyle48-cabinet.wav):
  actual mono output at its recorded gain, float WAV, no peak normalization or edited tail.

The primary `.f32` files are interleaved raw/amp/mic/final float32, .9 seconds each.
Additional DI files have raw/DI/amp/mic/final channels. Kernel files have mic/final-cab
channels. Internal DSP remains unchanged; pre-body double samples are copied to float
only for the diagnostic record. The original final-output RMS is computed on the same
float samples as the existing fixture. NumPy 2.3.5 / SciPy 1.17.0 analyze those records.
