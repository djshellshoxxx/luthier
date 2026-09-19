# PERFORMANCE BUDGET SPEC

Per-module CPU and memory budgets, and the enforcement rules that keep
the plugin within them. Any module that exceeds its budget in
measurement gets a failing test and blocks the build.

Baseline: 48 kHz, 128 sample block, single instance, mid CPU class
(Ryzen 5 5600X or Apple M2 Pro). One "unit" of CPU = 1% of a single
core.

## 0. Ground rules

1. Every module has a **budget** and a **measured cost**. Measured cost
   is captured in CI on every merge to main.
2. Modules are **isolated for cost measurement**: each is exercised in
   the offline renderer with a fixed input, and its per-block cost is
   averaged across 60 seconds.
3. A regression of more than 10% over the previous release's number is
   a red flag; more than 20% blocks the build.
4. The **audio callback allocates zero bytes**. Any allocation call from
   inside `processBlock` is a test failure (checked via a heap-hook in
   test mode).
5. The **audio callback locks nothing**. Locks are replaced with
   lock-free FIFOs or atomic pointer swaps per ui-wiring.md section 4.

## 1. Per-module CPU budgets (single instance, 48 kHz, 128 samples)

| Module | Budget (units) | Notes |
|---|---|---|
| StringEngine (12 strings) | 2.5 | Waveguide + interpolation, 4x oversampled |
| CouplingMatrix | 0.4 | O(N) per sample with N=12, per section 5.4 rules |
| BodyEngine | 0.6 | Partitioned convolution, up to 4 s IR |
| PickupEngine | 0.5 | Per-string positional comb + electrical tank |
| WhammyEngine | 0.1 | Only active when whammy present |
| CableSim | 0.05 | Simple filter |
| PreEffectsChain (8 slots) | 1.0 | Average pedal cost; extremes covered below |
| AmpEngine | 1.5 | 4x oversampled preamp, tone stack, power amp |
| PostEffectsChain (8 slots) | 1.0 | Same as pre |
| CabinetEngine (2 mics) | 0.4 | Convolution |
| RoomEngine | 0.3 | Convolution |
| MasterBus | 0.15 | Limiter, metering |
| MidiInterpreter + Technique | 0.05 | Message thread mostly |
| RhythmEngine | 0.1 | Control-rate, message thread |
| ModMatrix (control-rate) | 0.15 | 1024 route stress test |
| MeterFIFO / display path | 0.1 | Peak/RMS calc |
| Feedback path (when active) | 0.3 | Per ambiguity-resolutions.md section 1 |
| Freeze layer (when active) | 0.2 | Captured-loop synth |

**Total idle** (silent input, no notes): <= 1.5 units.
**Total steady state** (Rock preset, 4-voice): <= 8 units.
**Total heavy** (Metal Chug, 6-voice, all effects on): <= 22 units.

## 2. Per-pedal cost cap

No single pedal may exceed 0.5 units. If a pedal needs more, it needs a
"Quality" parameter with an eco option.

Ship pedal budgets:
- Reverb (large hall convolution): 0.4 with 8k tap FIR at 48 kHz.
- Delay: 0.05.
- Chorus: 0.05.
- Compressor: 0.03.
- Overdrive: 0.15 (oversampled).
- Fuzz: 0.15.
- Distortion: 0.2.
- Wah: 0.04.
- Phaser: 0.06.
- Tremolo: 0.02.
- Vibrato: 0.05.
- EQ (8-band): 0.1.
- Pitch shifter: 0.35.
- Doubler: 0.15.

## 3. Memory budget

RSS (resident set size):
- Baseline instance: <= 350 MB.
- With all IR variants loaded: <= 700 MB.
- Hard cap per instance: 900 MB.

Breakdown:
- Code: ~40 MB.
- IR library resident: 200 MB (720 IRs at 96 kHz float32, decimated to
  current SR on load).
- Factory presets: 5 MB.
- Backing sample buffers: 100 MB (metronome clicks, worker rings).
- Per-instance DSP state: 30 MB.
- UI (theme, fonts, illustration cache): 40 MB.
- Reserved for user IRs, session recorder ring buffer: variable.

Session recorder ring buffer is user-configurable; default 60 minutes at
48 kHz stereo float32 = ~1.4 GB, which sits outside the 900 MB cap
because it is disabled by default.

## 4. Latency budget

- Total main-out latency: <= 128 samples at 48 kHz (2.67 ms), excluding
  oversampling group delay (which is reported to the host).
- Per-string aux out: <= 32 samples (0.67 ms).
- DI aux out: <= 32 samples.
- Reported to the host correctly per routing-io.md section 7.

## 5. Boot time budget

- Cold instantiation to first block processed: <= 400 ms on mid CPU.
- Warm instantiation (second and subsequent instances in a session):
  <= 200 ms.
- Standalone application launch to audible: <= 1.5 seconds cold.

## 6. Voice-count scaling

CPU should scale roughly linearly with active voice count:
- 1 voice active: ~30% of steady-state cost.
- 4 voices: 100% (steady state).
- 8 voices: ~180%.
- 12 voices (all strings): ~250%.

Sympathetic coupling is O(N) per sample and does not scale with active
voices; unheld strings still participate.

## 7. Sample-rate scaling

- 44.1 kHz: baseline.
- 48 kHz: 1.09x.
- 88.2 kHz: 2.0x (partitioned convolution stages double, others less).
- 96 kHz: 2.2x.
- 176.4 kHz: 4.0x.
- 192 kHz: 4.4x.

Above 96 kHz, some oversampled modules downgrade internal oversampling
factor (4x -> 2x -> 1x) to stay within budget. Downgrade is transparent
and does not affect the module's audible behaviour above 20 kHz.

## 8. CPU relief mechanisms

If the plugin approaches CPU exhaustion (rolling 200 ms average > 85%
of block budget for the audio callback), it engages the following relief
in order:

1. Drop the display FIFO drain rate (UI updates slow, audio unaffected).
2. Suspend the scrolling data stream animation.
3. Reduce mod-matrix control rate by 2x (audio still smoothed by
   destination interpolation).
4. Reduce reverb tap count in convolution reverbs (audible; only if
   still exhausted).
5. As a last resort, drop the least-recently-active string's audio
   (audible; shows a "CPU limit" warning banner).

Relief mechanism 5 is opt-out; users can disable it in Options ->
Diagnostics and take the underrun instead. Default is on.

## 9. Enforcement

Every merge to main runs:
- Per-module cost measurement in the offline renderer.
- Total steady-state cost across all factory presets.
- Memory pressure measurement.
- Latency measurement.
- Boot time measurement.

Results published as a dashboard; regressions flagged automatically.

## 10. Tests

- Per-module budget: measure, verify <= budget * 1.10.
- Total idle: <= 1.5 units.
- Total heavy preset: <= 22 units.
- Memory ceiling: 60 min session, all features exercised, peak RSS
  <= 900 MB, no monotonic growth per QA spec.
- Allocation check: run 5 min of playback in test mode with a
  heap-alloc trap on the audio thread; zero triggers.
- Lock check: same, with a mutex-lock trap; zero triggers.
- Latency accuracy: send delta impulse, measure output delay, verify
  within 1 sample of reported.
- Sample-rate scale: measure per SR, verify curve matches expectation
  within 10%.
