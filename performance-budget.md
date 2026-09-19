# PERFORMANCE BUDGET SPEC

Per-module CPU and memory budgets, enforcement rules. Any module that
exceeds its budget gets a failing test and blocks the build.

Baseline: 48 kHz, 128 sample block, single instance, mid CPU class
(Ryzen 5 5600X or Apple M2 Pro). One "unit" of CPU = 1% of a single
core.

## 0. Ground rules

1. Every module has a **budget** and a **measured cost**, captured in
   CI on every merge.
2. Modules are **isolated for measurement**: exercised in the offline
   renderer with fixed input, cost averaged across 60 s.
3. Regression > 10% over the previous release is a red flag; > 20%
   blocks the build.
4. The **audio callback allocates zero bytes**. Any allocation from
   inside `processBlock` is a test failure (heap-hook in test mode).
5. The **audio callback locks nothing**. Locks replaced with lock-free
   FIFOs and atomic pointer swaps per ui-wiring.md.

## 1. Per-module CPU budgets

| Module | Budget (units) | Notes |
|---|---|---|
| StringEngine (12 strings) | 2.5 | Waveguide + interpolation, 4x oversampled |
| CouplingMatrix | 0.4 | O(N) per sample, N=12 |
| BodyEngine | 0.6 | Partitioned convolution, up to 4 s IR |
| PickupEngine | 0.5 | Per-string positional comb + electrical tank |
| GuitarCircuit | 0.05 | Biquad pair, control-rate coeff updates (replaces CableSim) |
| WhammyEngine | 0.1 | Only active when fitted |
| PreEffectsChain (8 slots) | 1.0 | Average pedal cost |
| AmpEngine | 1.5 | 4x oversampled |
| PostEffectsChain (8 slots) | 1.0 | Same as pre |
| CabinetEngine (2 mics) | 0.4 | Convolution |
| RoomEngine | 0.3 | Convolution |
| MasterBus | 0.15 | Limiter, metering |
| MidiInterpreter + Technique | 0.05 | Message thread mostly |
| RhythmEngine | 0.1 | Control-rate |
| TuneBuilder (playback) | 0.05 | Writes into rhythm engine, no audio path |
| ModMatrix (control-rate) | 0.15 | 1024-route stress test |
| MeterFIFO / display | 0.1 | Peak / RMS calc |
| Feedback path (when active) | 0.3 | Per ambiguity-resolutions.md 1 |
| Freeze layer (when active) | 0.2 | Captured-loop synth |
| NoiseEngine::Squeak | 0.4 | 16 generators pool, per string-squeak.md 12 |
| NoiseEngine::PickClick | 0.15 | Per-note transient synth |
| NoiseEngine::PickChirp | 0.10 | Wound-string release chirp |
| NoiseEngine::PickScrape | 0.10 | Rake events only |
| NoiseEngine::FretBuzz | 0.2 | Amplitude-vs-clearance sensing per string |
| NoiseEngine::Clank | 0.05 | Slide bar events |
| SlideEngine | 0.3 | State machine + damping + continuous pitch |
| BassTechniques (slap collision, limiter) | 0.25 | Bass-family only |

**Totals**:
- **Idle** (silent input, plugin loaded): <= 1.5 units.
- **Steady state** (Rock preset, 4-voice): <= 8 units.
- **Realism heavy** (fingerstyle acoustic, squeak 100%, buzz on, pick
  chirp on): <= 12 units.
- **Slide preset** (bottleneck electric with feedback amount 30%):
  <= 18 units.
- **Bass heavy** (funk slap with ghosts, both hands active): <= 15
  units.
- **Heavy preset** (Metal Chug, 6-voice, all effects on): <= 22 units.

## 2. Per-pedal cost cap

No single pedal exceeds 0.5 units. Pedals needing more must expose a
Quality parameter with an eco option.

Ship pedal budgets:
- Reverb (large hall): 0.4 with 8k tap FIR at 48 kHz.
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

RSS:
- Baseline instance: <= 350 MB.
- With all IR variants loaded: <= 700 MB.
- With full parts library loaded: <= 800 MB.
- With tune library and factory tunes loaded: <= 820 MB.
- Absolute cap per instance: 900 MB.

Breakdown:
- Code: ~40 MB.
- IR library resident: 200 MB.
- Factory presets, guitars, tunes: 15 MB.
- Parts library resident: 80 MB.
- Backing sample buffers (metronome clicks, worker rings): 100 MB.
- Per-instance DSP state: 30 MB.
- NoiseEngine textures (pre-synthesised per material): 40 MB.
- UI (theme, fonts, illustration cache): 40 MB.
- Reserved for user IRs, user parts, session ring buffer: variable.

Session recorder ring buffer is user-configurable; default 60 minutes
at 48 kHz stereo float32 = ~1.4 GB, outside the 900 MB cap because it
is disabled by default.

## 4. Latency budget

- Total main-out latency: <= 128 samples at 48 kHz (2.67 ms),
  excluding oversampling group delay (reported to host).
- Per-string aux: <= 32 samples.
- DI aux (Aux 1 post-circuit): <= 32 samples.
- DI aux (Aux 1 pre-circuit): <= 32 samples (bypasses GuitarCircuit).
- Aux 8 noise bus: <= 128 samples (rides body / pickup path).
- Reported correctly per routing-io.md 7.

## 5. Boot time budget

- Cold instantiation to first block: <= 400 ms on mid CPU.
- Warm instantiation: <= 200 ms.
- Standalone launch to audible: <= 1.5 s cold.
- Guitar load: <= 300 ms message thread; audio uninterrupted.
- Tune load: <= 100 ms message thread.
- Part swap: <= 50 ms message thread.
- Spectrum-delta worker: <= 40 ms per delta.

## 6. Voice-count scaling

- 1 voice: ~30% of steady state.
- 4 voices: 100% (steady state).
- 8 voices: ~180%.
- 12 voices (all strings): ~250%.

Sympathetic coupling is O(N) per sample and does not scale with active
voices; unheld strings still participate.

## 7. Sample-rate scaling

- 44.1 kHz: baseline.
- 48 kHz: 1.09x.
- 88.2 kHz: 2.0x.
- 96 kHz: 2.2x.
- 176.4 kHz: 4.0x.
- 192 kHz: 4.4x.

Above 96 kHz, some oversampled modules downgrade internal factor (4x
-> 2x -> 1x) to stay within budget. Downgrade is transparent above
20 kHz.

## 8. CPU relief mechanisms

Rolling 200 ms average > 85% of block budget:

1. Drop display FIFO drain rate (UI slows, audio unaffected).
2. Suspend scrolling data stream.
3. Reduce mod-matrix control rate 2x.
4. Drop NoiseEngine pool active generators to 8 (from 16) per class.
5. Reduce reverb tap count in convolution reverbs (audible; only if
   still exhausted).
6. Freeze the shadow `GuitarSpec` audition (if active).
7. Drop least-recently-active string audio (audible; "CPU limit"
   banner).

Relief 7 is opt-out in Options -> Diagnostics; default on.

## 9. Enforcement

Every merge to main:
- Per-module cost measurement in offline renderer.
- Total steady-state cost across all factory presets.
- Total realism-heavy cost across three fixture performances.
- Memory pressure measurement.
- Latency measurement.
- Boot time.

Results published to a dashboard; regressions flagged automatically.

## 10. Tests

- Per-module budget: measure, verify <= budget * 1.10.
- Total idle: <= 1.5 units.
- Total heavy preset: <= 22 units.
- Total realism heavy: <= 12 units.
- Total slide: <= 18 units.
- Total bass heavy: <= 15 units.
- Memory ceiling: 60 min session with all features exercised, peak RSS
  <= 900 MB, no monotonic growth.
- Allocation check: 5 min playback in test mode with heap-alloc trap
  on the audio thread; zero triggers.
- Lock check: same, with mutex-lock trap; zero triggers.
- Latency accuracy: send delta impulse, measure output delay within
  1 sample of reported.
- Sample-rate scale: measure per SR, verify curve matches expectation
  within 10%.
- Boot time: cold and warm, verify budget.
- Part swap: verify budget across 100 random swaps.
- Shadow audition: verify budget across 100 Alt-hover events.
- Spectrum delta: verify budget across 100 shadow renders.
