# SUSTAIN AND DECAY SPEC

A plucked string does not decay along one straight line in dB. It opens
with a bright, slightly sharp attack. It then falls quickly for the
first half second, while the polarisation that pushes hardest on the
bridge gives its energy to the body, and settles into a long, slow
tail. At the end it does not stop dead: the fretting finger lifts over
a few milliseconds, and the pitch sags as the string leaves the fret.
Luthier's string today has one T60, a fixed pitch and an instant
release. That is a clean synth decay, and ears hear it as one.

This file adds four physical behaviours to `StringEngine`, plus one
style macro. **Every new parameter defaults to the value that
reproduces today's string exactly**, so factory presets are unchanged
until a user turns something on.

## 0. Ground rules

1. **Shape the loop, do not add layers.** Every behaviour is a change to
   the waveguide's delay, loop gain or loop-filter cutoff, or (one case)
   a short resonator on the string's output. There is no envelope
   multiplied onto the output.
2. **Control rate.** New coefficients update every 32 samples (a
   "tick"), never per sample. The per-sample loop gains one multiply for
   the pitch ratio, and only while it is not 1.
3. **Legacy is a code path, not a coincidence.** Each behaviour at its
   neutral value skips its code entirely, so `loopGain`, `loopCutoffHz`
   and the delay are bit-identical to the current build.
4. **Deterministic.** Nothing here is random.
5. **Honest magnitudes.** The attack is sharp by 0.5-2 cents on plain
   trebles and 5-12 cents on a hard-picked wound bass. The two-stage
   knee sits 3-10 dB down. The release pitch sag is 10-30 cents, lasting
   10-40 ms.

## 1. What exists today (the delta starts here)

- `StringEngine::updateLoopCoefficients` computes one T60 from
  `physical.sustainSeconds × sustainScale × t60Scale(damping) ×
  (110/f0)^0.40`. Higher notes already decay faster. That model becomes
  the **slow** stage below. It is not replaced.
- `sustain_scale` ("Sustain", 0.25-3) and the per-note sustain scale
  (dead spots, fret wear, nut, slide, parts, magnet pull) keep working.
  They scale the slow stage.
- `applyNoteOff` calls `StringEngine::release(false)`, which sets
  `Damping::Released` at amount 1 at once: cutoff to 1200 Hz and T60 ×
  0.13. The only other release behaviour is `noise_release`'s fret-noise
  thump.
- The loop has no amplitude-dependent pitch. `Excitation` sets initial
  brightness from velocity and material, but the loop filter is
  constant through the note.
- `Source/Tests/SustainTests.cpp` (group `Sustain`) covers Freeze and
  E-Bow (`ambiguity-resolutions.md` 2), not decay shape. These tests go
  in a new `SustainDecayTests.cpp`, group `SustainDecay`.

## 2. Attack transient

Two parts, both scaled by `sustain_attack_transient` (A) and the note's
velocity v.

**2.1 Brightness overshoot.** A hard pick leaves the string with a
sharper corner than the loop filter's steady state. The upper partials
start above their steady level and lose that excess in tens of
milliseconds. The loop-filter cutoff multiplier on each tick is:

```
b(t) = 1 + 0.8 · A · v · exp(-t / τa)       τa = sustain_attack_time
cutoff = clamp(open × b(t), 120 Hz, 0.48 sr)
```

t counts from the last non-legato `excite`. A hammer-on or pull-off
restarts t at 0.5 × strength. **Budget:** only while `b(t) - 1 > 1e-4`,
roughly 10 τa.

**2.2 Longitudinal ping.** A pluck also launches a longitudinal wave.
Its frequency does not depend on the note's pitch:

```
f_L = (1 / 2 L_vib) · sqrt(E · A_core / μ)
```

With the build's `StringSpec` numbers: plain .010 steel on 648 mm is
**about 3.9 kHz**. The .046 wound low E (0.52 mm core) is **about 2.0
kHz**. Fretting at 12 doubles both.

The ping is a biquad resonator (Q 30) at f_L, with a second at 2 f_L,
6 dB down. It is struck at `excite` with amplitude `0.03 · A · v²` ×
the excitation peak and rings for τ = 15 ms. That puts it 30-35 dB
below the note at A = 1, v = 1: the faint "clank" that makes a wound
string sound like metal. It is added to the string's output with the
surface noise (pre-body), so body, pickups and circuit colour it.
Phantom partials (sum tones driven by tension modulation) are out of
scope.

## 3. Two-stage decay

A real string has two transverse polarisations. The vertical one drives
the bridge into the top and loses energy fast. The horizontal one
couples weakly and rings on. Their energies add, so the envelope is:

```
E(t) = a · exp(-2t/τf) + (1 - a) · exp(-2t/τs)
τs = T60_legacy / 6.9078              // today's decay is the slow tail
τf = ρ · τs
a  = sustain_fast_share,  ρ = sustain_fast_ratio
```

A single waveguide gets this envelope without a second delay line
through a time-varying loss. On each tick, compute the envelope's
instantaneous decay rate

```
r(t) = [ (a/τf)·exp(-2t/τf) + ((1-a)/τs)·exp(-2t/τs) ] / E(t)
m(t) = τs · r(t)          // >= 1, and -> 1 as the fast stage dies
T60_eff = T60_legacy / m(t)
```

and feed `T60_eff` into the existing loop-gain formula. Two `exp()` per
tick per sounding string. `m` is treated as 1, and skipped, once it is
below 1.0005.

- **The knee** sits at `10·log10(1 - a)` dB. With a = 0.5 that is -3 dB.
  With a = 0.8 it is -7 dB.
- Worked example: a = 0.5, ρ = 0.2, T60 4.5 s. At 0.5 s the note is 9.7
  dB down against the legacy 6.7 dB, and past 1.5 s the slope equals the
  legacy slope.
- Damping (palm mute, released, choke) multiplies T60 as it does today,
  so a palm-muted note keeps its shorter, mostly single-stage decay.
- t follows the same excite clock as section 2. The E-Bow and the
  feedback path add energy, so they reset t when they engage.
- The two polarisations would also beat slowly (0.2-2 Hz). That is left
  to `body-coupling.md`, which owns the bridge admittance.

## 4. Amplitude-driven pitch (tension modulation)

A string swinging with amplitude y stretches: `ΔL ≈ π² y² / (4L)`.
Tension rises by `EA · ΔL / L`, and `f ∝ sqrt(T)`, so:

```
Δf/f = κ · level²,
κ = S · (E · A_core · π² · k²) / (8 · T · L²)
```

- `k = FretBuzz::kMmPerLevelUnit × 1e-3` m is the build's existing
  calibration from the level follower to displacement (a hard open low
  E ≈ 1.8 mm).
- `S = sustain_tension_mod`, where 1 = physical.
- `E`, `A_core` and `T` come from `StringSpec`, via two new
  `StringEngine::Physical` fields, `coreDiameterMm` and `tensionNewtons`.
  `L` is the vibrating length at the current fret.

**Magnitudes at S = 1**, velocity 127, open:

| String | EA / T | Initial sharpness | Back under 1 cent after |
|---|---|---|---|
| High E .010 plain | ~140 | 0.5 – 1 cent | < 0.2 s |
| G .017 plain | ~400 | 1.5 – 3 cents | ~0.4 s |
| Low E .046 wound | ~560 | 6 – 12 cents | ~0.8 s |

level² falls at twice the amplitude's rate in dB, so the pitch settles
quickly. That produces the characteristic hard-picked bass "boing" that
flattens into tune.

**Application.** On each tick, `ratio = 1 + κ · level²`, clamped to +25
cents in stock and +50 cents in advanced. The ratio ramps linearly
across the tick and divides the smoothed delay:
`delayUsed = smoothedDelay.next() / ratio`. `getCurrentFrequency()`
reports the modulated pitch, so tuners and the coupling matrix see what
the string actually plays. The 0.2% loop-coefficient recompute
threshold still applies. The feedback is weak and bounded (a clamped
pitch shift of a stable loop), so it cannot run away.

## 5. Physical note-off release

When `applyNoteOff` fires without `letRing`, and the E-Bow is not
holding the string:

1. **Damping ramp.** `Released` damping ramps from 0 to 1 over
   `sustain_release_time` (T_r), one step per tick, and not instantly.
   T_r = 0 is exactly today's path.
2. **Release sag** (fretted notes only, fret > 0). As the fingertip lifts
   it rides the string behind the fret, adding length d:
   `Δcents = -1200 · log2((L_f + d) / L_f)` with `L_f = L · 2^(-fret/12)`.
   The target frequency glides to that over T_r, while the damping ramp
   kills the note.
   - d = 5 mm at fret 5 gives -17.7 cents. At fret 12 it gives -26.6
     cents.
   - Open strings, harmonics, slide-under-bar and fretless notes do not
     sag.
3. **Release ring** (`sustain_release_ring`, R). A clumsy lift is a small
   accidental pull-off. For a fretted note with R > 0, the target snaps
   to the open string with a 1 ms glide. The loop takes a one-period
   gain of R. Damping then becomes `LightTouch` at 0.6 instead of
   `Released`. So the open string rings quietly (R = 0.3 is -10.5 dB) and
   is choked by the next note on that string.
4. `noise_release`'s thump is unchanged. It fires at the start of the
   ramp.

## 6. Parameters

These go in a new eighth range family, **`string`**: the string's own
behaviour and its terminations' hold on it. `tuning-stability.md` shares
it, and `string-aging.md` should when it lands.

- **Delta to `advanced-ranges.md`:** `RangeFamily` gains `string`,
  appended after `modulation` as the eighth member, and `numFamilies`
  becomes 8. The "other six are stock" legacy-load test there becomes
  "other seven".
- **Delta to `file-formats.md`:** the `ranges.families` object gains a
  `"string"` key. An absent key already reads as `"stock"`
  (`advanced-ranges.md` 4), so there is no migration.
- The implementer records this in `DECISIONS.md`. Seven families was a
  count of the specs that existed, not a design limit.

| ID | Name | Stock | Advanced | Default | Unit |
|---|---|---|---|---|---|
| `sustain_attack_transient` | Attack Transient | 0 – 1 | 0 – 3 | 0 | ratio |
| `sustain_attack_time` | Attack Time | 5 – 80 | 1 – 300 | 30 | ms |
| `sustain_fast_share` | Fast Decay Share | 0 – 0.9 | 0 – 0.99 | 0 | ratio |
| `sustain_fast_ratio` | Fast Decay Ratio | 0.05 – 0.5 | 0.01 – 0.9 | 0.2 | τf/τs |
| `sustain_tension_mod` | Tension Pitch | 0 – 1.5 | 0 – 6 | 0 | × physical |
| `sustain_release_time` | Release Time | 0 – 60 | 0 – 300 | 0 | ms |
| `sustain_release_sag` | Release Sag | 0 – 8 | 0 – 20 | 0 | mm |
| `sustain_release_ring` | Release Ring | 0 – 0.3 | 0 – 1 | 0 | ratio |
| `sustain_style` | Sustain Style | choice | – | Legacy | – |

Every default is neutral: it selects the legacy code path. **Net new
parameters: +9.** The existing `sustain_scale` keeps its ID and range,
and joins `string` with its declared 0.25-3 as stock (advanced 0.05-4,
matching `StringEngine`'s clamp).

### 6.1 Sustain macros (styles)

`sustain_style` is the macro. Picking a style writes the eight values
below, and any later edit shows "(modified)", as `squeak_style` does.
The style never changes `sustain_scale`. Styles are not modulation
targets. A user who wants to sweep "realism" routes the individual
parameters in the mod matrix.

| Style | Trans. | τa | Share | Ratio | Tension | Rel. ms | Sag | Ring |
|---|---|---|---|---|---|---|---|---|
| **Legacy (default)** | 0 | 30 | 0 | 0.2 | 0 | 0 | 0 | 0 |
| Electric, natural | 0.5 | 25 | 0.45 | 0.20 | 1.0 | 15 | 4 | 0 |
| Acoustic, natural | 0.7 | 35 | 0.70 | 0.12 | 1.0 | 20 | 5 | 0 |
| Nylon | 0.3 | 50 | 0.60 | 0.15 | 0.4 | 30 | 6 | 0.05 |
| Compressed / modern | 0.3 | 20 | 0.15 | 0.40 | 0.6 | 10 | 3 | 0 |
| Dead-string thud | 0.1 | 15 | 0.85 | 0.08 | 1.2 | 25 | 5 | 0.1 |

`onboarding.md`'s ship default stays **Legacy**. `factory-content.md`
may move individual presets to a style in a later content pass. That is
a deliberate re-voicing, listed there, never a side effect of this spec.

## 7. Engine insertion and per-string state

All of this lives in `StringEngine`, which the per-string loop already
calls.

- New `StringEngine::setSustainShape (const SustainShape&)`, a plain
  struct of the eight values, set at block rate by
  `LuthierEngine::updatePerBlockModulation` from
  `ParameterBridge::applyToEngine`'s cache.
- `excite()` resets the excite clock (`samplesSinceExcite = 0`), strikes
  the ping, and stores `exciteVelocity`.
- `processSample()`: every 32 samples, call a new
  `updateShapeTick()`, which computes b(t), m(t), κ·level² and the
  release ramp step, and sets `needsLoopUpdate` only when a value moved.
  `updateLoopCoefficients()` multiplies the cutoff by b and divides the
  T60 by m, after its existing damping and pitch scaling. The delay read
  divides by the ramped ratio.
- `release (bool letRing)` becomes
  `release (bool letRing, double fret, double sagMm)`. It starts the
  ramp, the sag glide and the ring as in section 5. `applyNoteOff`
  passes `currentFret[s]` and the parameter.
- `refreshStringPhysics()` copies `coreDiameterMm` and `tensionNewtons`
  from `StringSpec` and precomputes κ₀ (κ at S = 1) and f_L per string.
  Fret changes rescale both by `L/L_vib`.

**Per-string state added:** `samplesSinceExcite` (int64),
`exciteVelocity`, `tickCounter`, `pitchRatio` (current and target),
`releaseRamp` (0-1), `releaseActive`, κ₀, f_L, and the ping resonators
(2 biquads plus an active flag). That is about 30 doubles per string,
all allocated with the engine.

## 8. UI

- **Column 1 STRINGS** (`gui-integration.md` 4.1) gains a **DECAY** row:
  the sustain style dropdown, the existing Sustain knob, and a small
  **decay sketch**. The sketch plots the predicted envelope in dB over 4
  s for the open low E (legacy dotted, current solid), computed on the
  message thread from the same formulas. It redraws on parameter change
  only.
- **CHARACTER tab**, new **SUSTAIN SHAPE** group: all eight parameters
  in three rows (Attack, Decay, Release), and a live **pitch-offset
  readout** per string that shows the tension-modulation cents. It
  drains at 30 Hz from a lock-free per-string atomic and greys after 2 s
  stale (`gui-engine-dataflow.md`).
- `gui-integration.md` 19 gains the row: Sustain shape | StringEngine |
  Adv Col 1 STRINGS (style) + Col 4 CHARACTER SUSTAIN SHAPE.
- Undo: knob-move (`action-and-undo.md` 3.1). Styles are discrete
  switches (3.2) and group all eight writes into one entry.

## 9. State and serialization

All nine are APVTS parameters in the preset's `parameters` block, and
`string` joins the `ranges` block. The per-string runtime state is not
saved: `reset()` zeroes it, per engine rule 8. Older presets lack the
IDs and load at the neutral defaults, so they sound unchanged. MIDI
export needs nothing new: every behaviour follows from notes and
velocities already exported.

## 10. Performance and realtime safety

- Budget: **+0.3 units** on the `StringEngine (12 strings)` row, with
  every behaviour on and 12 strings sounding. Per string per tick: 2
  `exp` (decay), 1 `exp` (attack), a few multiplies. Per sample: 1
  divide (pitch ratio) and 2 biquads for about 100 ms after each pluck
  (ping). At the Legacy style the added cost is one branch per tick.
- No allocation, no locks, no randomness. Every new filter is bounded
  (ping Q 30, unity-normalised). The loop gain stays under the existing
  `kMaxLoopGain`. The pitch ratio is clamped, and the NaN guard in
  `sanitise` covers the new paths.
- Times are in seconds and ms. The tick is 32 samples at every rate,
  and every time constant is converted per `prepare()`. Sample-rate
  independence is tested (SUS-15).

## 11. Tests

Group `SustainDecay`, `Source/Tests/SustainDecayTests.cpp`. The renders
drive a single `StringEngine` or `LuthierEngine` with the body bypassed,
at 48 kHz. Pitch is measured by autocorrelation over 40 ms windows.
Envelope is RMS over 20 ms windows.

- **SUS-01 Legacy is bit-identical.** With every new parameter at its
  default: a 10 s performance (plucks, hammer-ons, palm mutes, note-offs,
  bends) is sample-identical to the same render with the shape code
  bypassed by test hook.
- **SUS-02 Two-stage knee.** Open A, a = 0.5, ρ = 0.2: the envelope slope
  over 50-250 ms is at least 3× the slope over 2-3 s. The 2-3 s slope
  is within ±10% of the legacy render's.
- **SUS-03 Knee depth.** For a in {0.3, 0.5, 0.8}, the intercept of the
  late-slope line at t = 0 is `10·log10(1-a)` ±1 dB.
- **SUS-04 Tension magnitude.** S = 1, velocity 127, open low E: 50-90
  ms after the pluck, the pitch is sharp by 5-12 cents against the same
  note at velocity 10. After 1.5 s it is within 1 cent of target.
  Open high E: under 2 cents at 50-90 ms.
- **SUS-05 Tension scales with level².** The offset at velocity 127
  divided by the offset at velocity 64 is in [3, 5].
- **SUS-06 Brightness overshoot.** A = 1, τa = 30 ms: the spectral
  centroid over 0-30 ms is at least 15% above the A = 0 render. Over
  300-330 ms it is within 3%.
- **SUS-07 Longitudinal ping.** Take the difference render (A = 1 minus
  A = 0, which is exact because nothing is random). Open plain high E
  has its largest bin above 1 kHz at 3.9 kHz ±10%. Open wound low E at
  2.0 kHz ±15%. The low E fretted at 12 at 2× its open value ±10%.
- **SUS-08 Release ramp.** T_r = 40 ms: 10 ms after note-off, the level
  is at least 6 dB above the legacy instant release. By 250 ms both are
  more than 40 dB below the level at note-off.
- **SUS-09 Release sag.** Fret 5, d = 5 mm, T_r = 30 ms: the pitch 5-25
  ms after note-off is 10-25 cents flat. An open string in the same
  render shows under 1 cent of sag.
- **SUS-10 Release ring.** R = 0.3, fret 7: 100 ms after note-off the
  pitch is the open string ±5 cents, and the level is -10.5 ±2 dB
  against the level just before the release.
- **SUS-11 Bounded at the limits.** Advanced maxima (S = 6, A = 3, a =
  0.99, ρ = 0.01), 60 s of velocity-127 plucks on all 12 strings: every
  sample is finite, the pitch offset is ≤ 50 cents, and the loop gain is
  ≤ `kMaxLoopGain`.
- **SUS-12 Styles.** "Electric, natural" writes exactly its table row.
  Editing any value makes `sustain_style` report modified. "Legacy"
  restores the defaults, and SUS-01 then holds.
- **SUS-13 Letting ring skips release.** With `letRing` (sustain pedal)
  or the E-Bow holding the string, none of section 5 runs. The render is
  identical to T_r = 0, d = 0, R = 0.
- **SUS-14 Cost and safety.** 12 strings, all behaviours on: the
  measured `StringEngine` cost is ≤ its budget + 0.3 units. Zero heap
  allocations over the run (heap hook).
- **SUS-15 Sample-rate independence.** At 44.1 and 96 kHz: the knee time
  (the t at which the envelope is 1 dB off the late line) is within ±5%.
  SUS-04's offset is within ±1 cent. SUS-07's ping frequency is within
  ±3%.
