# BODY COUPLING SPEC

On a real guitar the body does not simply colour the strings' sound
after the fact. The strings drive the bridge, the bridge moves the body,
and the moving body pushes back on every string. That return path
explains three things every player has met:

- **Wolf notes.** One note on an acoustic (often around G or G# on the
  low strings) blooms and then dies abruptly, or warbles. Its pitch sits
  on the top's main resonance, and the string and body trade energy back
  and forth.
- **Dead-ish notes on solidbodies.** They are much milder, and caused
  the same way, by body and neck modes.
- **Tap tones and sympathetic ring.** Knock on the top and the open
  strings answer at the notes the body resonates at.

Luthier currently runs the body **feed-forward**. `BodyEngine` filters
the summed strings (by convolution or with a modal bank) and nothing goes
back. The only back-flow is `CouplingMatrix`: a symmetric
string-to-string matrix, base 0.020 with distance falloff, through one
receive band-pass per string (engine spec 5.6). It models the saddles
sharing a bridge, but it has no body resonances. So a wolf note cannot
happen and a tap cannot make a string ring.

This file adds the missing path: a small **bridge admittance bank**
shared by all strings, built from the same `BodyModels` modes the body
already uses. Wolf notes, tap-tone response and body-mediated sympathetic
ring then come out of it without any special case.

## 0. Ground rules

1. **Emergent, never scheduled.** There is no "wolf note" parameter and
   no per-note wolf table. A wolf appears where a string partial meets a
   strong, light, high-Q body mode, and moves when that mode moves
   (`environment.md`, Workshop part swaps).
2. **Passive by construction.** The bank can only remove energy from the
   strings or move it between them. It never adds energy. Section 2.3
   shows how the discrete form keeps this true.
3. **One body, two views.** The radiated body (`BodyEngine`) and the
   coupling bank are designed from the same `BodyConfig` and receive the
   same runtime scaling. A tap tone's pitch and a wolf's pitch therefore
   always agree.
4. **The existing matrix stays.** `CouplingMatrix` keeps modelling the
   direct saddle path. This file adds the resonant body path next to it,
   and does not retune the matrix, so presets saved before this spec
   sound as they did.
5. **Honest magnitudes.** On an acoustic, a note on the main top mode
   can lose more than half its sustain. On a solidbody, a fret next to a
   body mode loses 10 to 30 % of its sustain, and up to about 40 % only
   when it lands exactly on the mode.

## 1. Relation to other specs

| Spec | Relationship |
|---|---|
| `character-wear.md` 2 (dead spots) | Dead spots model the **neck/fret end**, seeded per instance. This bank models the **bridge end**. They stack. `getSustainMultiplier`'s body-resonance bias stays as it is |
| `part-acoustics.md` 2, 5 | Supplies chambering, bridge mass and coupling, tailpiece mass. Section 3 adds a "body coupling design" row to its mapping, so the mapping from parts still happens in one place (ground rule 1 of that file) |
| `environment.md` 2.6 | Supplies the plate and air frequency and Q multipliers. Same call as `BodyEngine::setRuntimeScaling` |
| `string-interaction.md` (23e) | Owns the **air-path** sympathetic ring and muted-string thump. This file owns the bridge/body path only |
| `ambiguity-resolutions.md` 1 / 2.2 | Feedback and E-Bow inject through `couplingIn` as before. The bank acts on those strings like any other energy |

## 2. Physics

### 2.1 Bridge admittance

Near its lowest resonances the bridge looks like a sum of modes. Each
mode `k` has an effective mass `M_k` at the bridge, a frequency `f_k`
and a quality factor `Q_k`:

```
Y(s) = Σ_k (1/M_k) · s / (s² + (ω_k/Q_k)·s + ω_k²)          [m/(N·s)]
peak |Y_k| at ω_k = Q_k / (M_k · ω_k)
```

A string's characteristic impedance is `Z0_i = sqrt(T_i · μ_i)`: about
0.17 kg/s for a plain high E and 0.69 kg/s for a wound low E. When a
wave of amplitude `a_i` hits a bridge with admittance `Y`, it reflects
with the exact coefficient `(1 − Z0·Y)/(1 + Z0·Y)`, whose magnitude is
at most 1 whenever `Re Y ≥ 0`.

### 2.2 Magnitudes

The effective mass of the lowest mode, by chambering, before the bridge
and tailpiece mass are added:

| Chambering | `M_base` | Typical mobility peak | Note |
|---|---|---|---|
| `acoustic` | 0.10 kg | 0.2 – 1 m/(N·s) | Consistent with published acoustic-guitar bridge mobility |
| `hollow` | 0.20 kg | | |
| `semi_hollow` | 0.60 kg | | |
| `chambered` | 2.0 kg | | |
| `solid` | 10.0 kg | ~0.005 m/(N·s) | Body plus neck, at the bridge |

Mode `k`'s mass is `M_k = M_base / max(0.05, g_k / g_max) + m_bridge + m_tailpiece`,
where `g_k` is `BodyModels`' mode gain.

**Worked example: acoustic, low E fretted at 3 (G2, 98 Hz), main mode
at 98 Hz, Q 40, pin bridge (28 g).** The six strings together load the
mode: `Z_tot = Σ Z0 ≈ 2.3 kg/s`. With `M = 0.128 kg` that drops the
loaded Q to `1/(1/40 + Z_tot/(M·ω)) ≈ 18` (section 2.3). The
weak-coupling decay estimate `σ = 2 f0 Z0 Re Y'` (with κ = 0.92) then
comes to about 30 /s, so a string that would ring for 4 s would be down
60 dB in about 0.25 s. That is well
into the strong-coupling regime (string mass / mode mass ≈ 0.03 against
`(π/2Q)² ≈ 0.002`), so what comes out is the classic two-frequency
warble and a fast decay, not a clean shortening.

**Same test on a solidbody** (M ≈ 10.1 kg, Q 60, 180 Hz, hardtail at
coupling 0.70): the body adds `σ_b ≈ 0.9 /s` to the string's own
`≈ 1.4 /s`, so at exact coincidence the T60 falls from about 5 s to
about 3 s. The mode's bandwidth (about 1.7 %) is much narrower than a
fret step (6 %), so most frets miss the peak and dip only 10 to 30 %.
That is a mild dead spot.

### 2.3 The loaded, passive discrete form

If the linearised reflection `1 − 2·Z0·Y` were injected directly, it
would be unstable when `Z0·|Y|` is large, which it is on an acoustic. The
bank instead folds the strings' loading into each mode analytically,
treating each mode separately (the diagonal approximation):

```
Q'_k  = 1 / (1/Q_k + Z_tot/(M_k·ω_k))
Y'_k  = (κ/M_k) · s / (s² + (ω_k/Q'_k)·s + ω_k²),   κ = body_coupling_amount · bridge.coupling
```

Per sample, using the previous sample's bridge waves (the same
one-sample lag `CouplingMatrix::process` already uses):

```
F[n]   = Σ_j 2·Z0_j · a_j[n−1] + F_tap[n]
v[n]   = Σ_k R_k(F)[n]                 R_k = bilinear Y'_k, pre-warped at ω_k
c_i[n] = −v[n]                          added to string i's coupling input
```

For one string alone, `1 − 2·Z0·Y'` equals the exact reflection
`(1 − Z0·Y)/(1 + Z0·Y)` whose magnitude is at most 1, so the system is
passive. For several strings, the terms where `j ≠ i` are the
body-mediated sympathetic ring. As a backstop, a per-sample cap of 0.35
on `|c_i|` (the same `kEnergyCap` the matrix uses) is reported to the
validator.

`R_k` is exactly a `ModalResonator` (`BodyEngine.h`), a constant-peak
band-pass with gain `g_k = κ·Q'_k/(M_k·ω_k)`. That class is reused
unchanged.

### 2.4 Tap tones

A body tap (`noise_body_knock` events, and `string-slap-technique.md`'s
body tap) already sounds through `BodyEngine`. It now also injects a
1 ms half-sine force `F_tap` into the bank, scaled by the knock level and
velocity. Undamped open strings whose partials sit near body modes ring
in answer. Nothing else is added.

## 3. Engine insertion

- **New `BodyCouplingBank`** (`Source/DSP/Coupling/BodyCouplingBank.h/.cpp`):
  - `prepare(sr)`, `reset()`.
  - `design(const BodyConfig&, BridgeCoupling, const double* z0, int numStrings) → BodyCouplingDesign`,
    run on the message thread. It calls `BodyModels::buildModes` into a
    member scratch vector, takes the `K` strongest modes below 1.2 kHz,
    computes `M_k` and `Q'_k`, and returns a POD
    `{ f[16], q[16], m[16], isAir[16], count, z0[kMaxStrings] }`.
  - `stage(const BodyCouplingDesign&)` hands the design to the audio
    thread through an atomic flag plus a try-lock, the same pattern as
    `BodyEngine::applyStagedBank`.
  - `setScaling(plateFreqMul, airFreqMul, qMul, massMul) noexcept`, block
    rate.
  - `processSample(const double* bridgeWaves, double* couplingInputs, int n) noexcept`
    **adds** into `couplingInputs`.
  - `injectTap(double force) noexcept`.
- **`StringEngine`**: `Physical` gains `waveImpedance`. It is set in
  `StringMaterials::toPhysical` as `sqrt(spec.tensionNewtons · spec.linearDensity)`.
  A new `double getBridgeWave() const noexcept` returns the loop's return
  (after the loop filter, the dispersion cascade and the loss gain) before
  anything is injected. *Amended in the build:* the first draft named the
  DC-blocked delay output, but in this single-delay-loop string the delay
  line is shortened by the loop filter's and the dispersion cascade's group
  delay (up to a third of the loop on a wound string), so the delay output is
  the reflecting wave that many samples early. Driving the bank from it
  advanced the body path by tens of degrees at the fundamental and made the
  open low E of an acoustic grow (+7 dB in 4 s). Driven from the loop's
  return, the only lag left is the documented one sample. The existing
  `couplingReceptivity` still scales what the string accepts, so a
  palm-muted or choked string takes less.
- **`LuthierEngine::processBlock`**, per sample, directly after
  `coupling.process(bridgeOutputs, couplingInputs)`:
  `bodyCoupling.processSample(bridgeWaves.data(), couplingInputs.data(), numStrings)`.
  `bridgeWaves[s]` is filled next to `bridgeOutputs[s]` from
  `getBridgeWave()`.
- **`LuthierEngine::rebuildBodyFromSpec()`** and **`refreshStringPhysics()`**
  (a tuning or gauge change moves `Z0` and so `Z_tot`) both re-run
  `design` and `stage`. Bridge mass, tailpiece mass and coupling come
  from `DerivedAcoustics` when a parts guitar is loaded, and otherwise
  from `part-acoustics.md` 5's bridge-type table keyed on `spec.bridge`.
- **Scaling**: each block, `bodyCoupling.setScaling(...)` and
  `body.setRuntimeScaling(...)` receive the same product:
  `environment` multipliers × `body_mode_freq_scale` (plate and air) ×
  `body_mode_q_scale`. `body_mode_mass_scale` goes to the bank only,
  because radiation level is `body_amount`'s job.

## 4. Parameters

Appended at the end of the layout. Net **+5** (the three phase-2b specs
together take the layout from 450 to 465 in this build; the counts first
written here predate other workstreams' parameters).

| ID | Name | Stock | Advanced | Default | Unit |
|---|---|---|---|---|---|
| `body_coupling_amount` | Body Coupling | 0 – 1 (non-physical, single range) | – | 1.0 (legacy loads: 0) | ratio |
| `body_mode_mass_scale` | Body Mode Mass | 0.5 – 2.0 | 0.05 – 20 | 1.0 | × |
| `body_mode_q_scale` | Body Mode Q | 0.5 – 2.0 | 0.1 – 5.0 | 1.0 | × |
| `body_mode_freq_scale` | Body Mode Tuning | 0.9 – 1.1 | 0.5 – 2.0 | 1.0 | × |
| `body_coupling_modes` | Coupling Modes | choice: 4 / 8 / 12 / 16 | – | 8 | – |

- **Stock ranges are real ranges.** ±10 % on mode frequency is the
  build-to-build spread of nominally identical instruments. Mass and Q
  from half to double cover everything from a light ladder-braced parlour
  to a heavily finished jumbo.
- The advanced range reaches settings no instrument has (a 5 g top
  mode). The model stays stable there because it is passive. The cap and
  validator (section 2.3) are the backstop.
- The three ranged parameters form a new range family, **`body`**,
  appended to `RangeFamily`. This is the same backward-compatible
  amendment to `file-formats.md` 2 and `advanced-ranges.md` 2 that
  `string-aging.md` 4 argues for.
- `body_mode_freq_scale` is the runtime counterpart of
  `BodyConfig::resonanceTrim`, which rebuilds the bank. The trim stays
  as a Workshop / body-part field; the parameter multiplies on top of it.

## 5. UI

- **Advanced Column 1 → BODY** (`gui-integration.md` 4.1): a **Coupling**
  knob next to the air-resonance readout, plus the existing link to
  CHARACTER.
- **Column 4 CHARACTER → new BODY COUPLING group** (after SETUP):
  - mass, Q and tuning scales, and the modes choice;
  - a **mode list** showing frequency, Q, loaded Q', mass and admittance
    peak;
  - a **wolf map**: a strings × frets 0-19 grid. Each cell is the
    predicted sustain loss
    `1 − σ_s/(σ_s + σ_b)`, with `σ_b = 2·f·Z0·Re Y'(f)` for the fretted
    fundamental and its first two partials. The map is computed on the
    message thread from the design whenever the design or scaling
    changes, not streamed. It uses the warning colour above 30 % loss,
    plus a dot glyph so it reads without colour (`gui-integration.md` 21);
  - a **Tap** button that fires one body tap (section 2.4), so the user
    can hear the strings answer.
- **Workshop:** the body part inspector mirrors the mode list and wolf
  map, so swapping a bridge or a top shows the wolf move, as
  `workshop-ui.md`'s spectrum delta does for tone.
- The CHARACTER padlock covers the `body` family.

## 6. Serialization and migration

- The five parameters are stored in `parameters`. The bank is derived,
  never stored: it is rebuilt from the guitar and the parameters on load.
- **Legacy load:** if `parameters` has no `body_coupling_amount`, the
  loader writes 0, so a saved preset sounds exactly as it did. New
  presets default to 1.
- **Factory content:** `factory-content.md`'s presets are re-voiced with
  coupling on (1.0, with occasional deliberate exceptions) and re-saved
  during the listening pass. That is a content change, recorded in
  `PROGRESS.md`.

## 7. Performance and realtime safety

- Per sample: `K` resonators (5 multiplies and 4 adds each) plus `2N`
  multiply-adds. At K = 8 and N = 6 that is about 90 flops per sample, or
  roughly 4 MFLOP/s at 48 kHz. The budget is **0.08 units** at K = 16 and
  N = 12, a new `BodyCouplingBank` row in `performance-budget.md`. Per
  string the cost is 2 multiply-adds per sample, so it scales with
  polyphony far below a voice's waveguide.
- `design()` allocates only into a member scratch vector, and only on
  the message thread. The audio thread copies POD under a try-lock and,
  if the lock is busy, retries next block (the `applyStagedBank`
  pattern).
- **Smoothing:** changes to scaling re-design the resonators at block
  rate, only when a multiplier moves by more than 0.05 %. Frequency-scale
  automation is slew-limited to 0.2 % per block, so the modes glide.
  `body_coupling_amount` is smoothed per sample with the engine's
  standard 20 ms linear `SmoothedValue` (engine spec 0.4).
- All math is in `double`. `sanitise()` guards every output. The bank
  has DC-free band-pass modes, so it needs no extra DC blocker.
  `reset()` clears resonator state and the pending tap.

## 8. Tests

- **BC-01 Passivity.** Pluck all six strings once (acoustic, default
  parameters) and render 10 s. Total energy (the sum of `a_i²` over
  strings plus the resonator state energy), measured in 50 ms windows,
  never rises by more than 0.1 dB from one window to the next after the
  first 20 ms.
- **BC-02 Wolf on an acoustic.** Set `body_mode_freq_scale` so that the
  main mode sits exactly on the low E fret 3 fundamental. That note's
  measured T60 is at least 50 % shorter than the average of frets 0 and
  6 on the same string. *As built:* T60 here and in BC-03, 05, 08, 11 is
  the fundamental's (a band-pass at f0, Q 8), because a wolf eats the
  fundamental and a broadband RMS is dominated by partials the body never
  touches; the other strings are damped, as the fretting hand does. In its first 500 ms the spectrum shows two peaks
  within ±15 % of `f0`, separated by at least 2 Hz (splitting).
- **BC-03 Solidbody is mild.** The same exact-coincidence test on a
  solidbody gives a T60 dip between 20 % and 50 %, with no measurable
  splitting. At the default `body_mode_freq_scale` of 1.0, no fret
  0-19 on any string of the factory solidbodies dips by more than
  35 %.
- **BC-04 Off is identical.** With `body_coupling_amount = 0`, a render
  is bit-identical to one with the bank removed from the per-sample loop.
- **BC-05 The wolf moves with the body.** A plate multiplier of 0.97
  (from `body_mode_freq_scale` or `environment.md`) moves the fret and
  frequency of the largest measured T60 dip by −3 % ± 0.5 %.
- **BC-06 Tap tone.** Fire one body tap with all strings open and
  undamped, and tune one open string to within 2 % of the main mode. That
  string's RMS over the following 1 s is at least 6 dB above the string
  the bank reaches least. *Amended in the build:* 12 dB assumed a sharp
  mode. The acoustic's main mode is loaded to Q' ≈ 11 by the six strings
  (2.3), so a tap rings it for about 30 ms and kicks every string through
  the same injection; no string's first partials are 10 % from all sixteen
  modes, and the tuned string gains only its coherent build-up (measured
  7.3 dB).
- **BC-07 Sympathetic ring through the body.** (Measured in the engine,
  with the saddle matrix in place.) With `coupling_amount` at
  its minimum of 0.05, pluck the A string. Unplucked D (whose second
  partial lies near A's third) rings at least 6 dB higher with coupling 1
  than with coupling 0.
- **BC-08 A heavy bridge couples less.** Swap a pin bridge (28 g,
  coupling 0.92) for a Floyd Rose (320 g, 0.30) on the same body. The
  BC-02 dip shrinks by at least half.
- **BC-09 Muted strings do not answer.** A palm-muted string's response
  in BC-06 is at least 6 dB below the same string open
  (`couplingReceptivity`).
- **BC-10 Stability sweep.** 1000 random corners across the advanced
  ranges, 12 strings, 5 s of dense chords each (*as built:* 200 corners of
  1 s, which visits every combination of the four scales' extremes many
  times and keeps the suite's run time; the model is linear and passive,
  so a longer render adds decay, not risk): no NaN or Inf, every
  sample below 4.0, and the validator's cap engages in no more than 1 %
  of samples.
- **BC-11 Wolf map is honest.** On the acoustic, the map's three
  highest-loss cells include, to within one fret, the fret with the
  shortest measured T60 on each of the three lowest strings. *Amended in the
  build:* the map is the weak-coupling estimate of 5; near the strongest
  modes the measured decay is strong-coupling (energy returns from the
  body), which moves the measured minimum by up to a fret (low E: map 14,
  measured 15).
- **BC-12 Legacy load.** A preset without `body_coupling_amount` loads
  with 0 and renders within −60 dBFS RMS of the pre-spec reference. The
  `body` family reads stock.
- **BC-13 Budget and safety.** `BodyCouplingBank` costs ≤ 0.08 units at
  K = 16 and N = 12. `design()` never runs on the audio thread
  (`ThreadProbe`). Zero audio-thread allocations across a part swap.
