# TUNING STABILITY SPEC

A real guitar goes out of tune for reasons, and players learn them.
New strings go flat as they stretch in. A string bent hard sticks in
the nut and comes back sharp until it "pings". A sloppy tuner lets go
after the first big bend. A floating bridge detunes every other string
when you drop the low E. A capo pulls everything sharp. Luthier today
has only the random kind: `tuning_drift`'s random walk and
`character-wear.md` 4's tuner-drift LFO. Neither responds to how the
guitar is played.

This file adds event-driven tuning offsets, each with its physical
cause taken from the fitted parts, and the retune actions that clear
them. **The master amount defaults to 0**, so every existing preset
plays exactly in tune as it does today.

## 0. Ground rules

1. **Caused, not random.** Every offset follows from something that
   happened: a bend, a whammy excursion, a tuning change, a capo, or
   playing time on new strings. The only chance involved is *when* a
   bound string releases, and that is a seeded hash.
2. **Physics from the parts.** Tuner ratio, stability and locking, nut
   friction, bridge type, capo pressure, the string's core and tension
   all come from `DerivedAcoustics` (`part-acoustics.md`). The
   parameters here only scale them.
3. **Strain to cents, one formula.** A change in length ΔL on a string
   of length L gives `cents = 865.6 · (EA/T) · ΔL/L` (that is `1200/ln 2
   × ½`, because f ∝ √T). `EA/T` comes from `StringSpec` (core
   diameter, tension): about 140 for a plain .010, about 400 for a plain
   .017 and about 560 for a wound .046. That is why wound strings
   and the plain G are the ones that wander.
4. **Deterministic and resettable.** `reset()` (transport start, preset
   load, panic) zeroes every event-driven offset. An offline render
   always starts in tune and always repeats.
5. **Honest magnitudes.** A sticky nut after a full-tone bend: +1-3
   cents. A vintage tuner's backlash: 1-7 cents. Drop D on a floating
   bridge: +7-15 cents on the other strings. A trigger capo: +2-9
   cents. New steel strings: 5-10 cents flat over the first minutes.

## 1. What exists today (the delta starts here)

- `TuningEngine::StringTuning` already sums `detuneCents`,
  `realismDetuneCents` (preset-persisted), `driftCents` (the
  `tuning_drift` random walk, ±5 c), `characterDriftCents`
  (`CharacterEngine` tuner-drift LFO, driven by `tuner_looseness`) and
  `fineTuneCents` into `getOpenFrequencyBeforeCapo`. This spec adds one
  more field, **`stabilityCents`**, to the same sum. Nothing else in
  the pitch path changes.
- The capo is a fret (`ambiguity-resolutions.md` 4.5, `capo_fret`, the
  string mask). Capo *bias* (section 2.6) is new. The capo part is the
  player's accessory, not a guitar slot: its pressure is read when it is
  chosen, its gap from its kind (screw 4 mm, trigger and partial 6 mm),
  and with no capo part 0.7 and 5 mm. The derived "Capo bias: +2.2 to
  +8.6 c" figure is shown under the TUNING STABILITY offset strip, since
  the Workshop inspector has no capo slot.
- `CharacterEngine::retune()` zeroes all tuner drift, and CHARACTER's
  Retune button calls it. **Shared:** that button becomes "Retune all"
  (section 3), which also clears this spec's offsets. character-wear's
  looseness LFO is a separate model (a machine head slipping over
  minutes). This spec does not change it. `tuners.stability` feeds only
  the backlash term here. It does not rescale `tuner_looseness`, which
  stays the character engine's own control.
- Part fields `tuners.ratio / stability / locking`, `nut.friction` and
  `capo.pressure` exist in the factory parts but are consumed by nothing
  (`DECISIONS.md`, "Part fields not yet consumed"). This spec is their
  consumer. `mapSpec` gains a `TuningHardware` block in
  `DerivedAcoustics`.

## 2. Mechanisms

Each mechanism's contribution is scaled by
`stability_amount × stability_<mechanism>`. Their per-string sum is
clamped to ±50 cents in stock and ±200 in advanced.

### 2.1 String settling

New strings keep stretching after they are tuned. Per-string stretch
state σ is set at preset load from `string_age`: Fresh 1.0, Broken-in
0.2, Old 0.

```
W  += level² · dt  +  0.5 · (B/100)²   per bend release with peak B cents
c_settle = -C_s · σ · (1 - exp(-W / W0))       W0 = 5.4 (about one minute
C_s = 8 c × material factor                     of moderate playing)
```

The material factor is 1 for steel and bronze, 1.5 for silk and steel,
and 4 for nylon and fluorocarbon, which really do go 20-40 cents flat
while new. On a retune, `σ ← σ · exp(-W/W0)` and `W ← 0`. The stretch
already taken out stays out, so each retune drifts less, which is the
real experience of a new set.

### 2.2 Nut binding

When a bend releases (peak B ≥ 20 cents, and the bend has fallen below
25% of the peak), the string slid through the nut under tension and
friction holds it:

```
stuck += 0.03 · μ · B        μ = nut.friction (graphite 0.1, bone 0.3-0.35, brass 0.4)
```

A 200-cent bend on bone (0.35) leaves **+2.1 cents**. A whammy dive of D
cents leaves `-0.015 · μ · D`, which is flat. A locking nut (Floyd-type
bridge) forces μ to 0. Stuck is capped at ±6 cents in stock.

**The ping.** Each later pluck on the string, at velocity v, releases
the bind with probability `p = 0.25 · v · (1 - μ)`. The draw is
`noiseUniform(seed, s·65536 + pluckIndex)`. On release, `stuck` ramps to
0 over 20 ms.

### 2.3 Tuner backlash

Gear slack only matters if the string was last brought to pitch **from
above**: tuned down, so the gear is not loaded against the string's
pull. A string becomes *armed* when its open pitch is lowered by a
tuning-preset change, a lower `detune` or `fine_tune`, or a partial
retune that lowered it. The Retune action (section 3) always comes up
from below, as a tech does, so it disarms.

An armed string gives way on its next tension excursion: a bend of at
least 50 cents, a pull-up of at least 50 cents, or a pluck at velocity
≥ 0.9. It then drops flat by `b` over 30 ms and disarms.

```
b = 865.6 · (EA/T) · r_post · θb / L_total
θb = 0.5° · (1 - tuners.stability) · (15 / tuners.ratio)
r_post = 3 mm,  L_total = L + 120 mm (headstock run)
```

| Tuner | θb | High E | Low E |
|---|---|---|---|
| Vintage open-back (0.6, 14:1) | 0.21° | 1.8 c | 7.1 c |
| Modern sealed (0.85, 18:1) | 0.06° | 0.5 c | 2.1 c |
| Locking (0.95, 21:1) | 0.02° | 0.2 c | 0.6 c |

### 2.4 Saddle creep and bridge equilibrium

Covers every effect of the bridge moving under a changed tension load.
The bridge type comes from `spec.bridge` (`WhammyEngine::BridgeType`).
An acoustic's bridge rotates with its top.

1. **Floating equilibrium, immediate.** On a floating bridge the springs
   balance the *total* string tension. A tension change ΔT_j on string j
   moves every other string i by
   `Δc_i = -k_f · 100 · ΔT_j / T_total`, with k_f = 3.0 for FloydRose,
   2.0 for VintageTrem, 1.0 for Bigsby and 0 otherwise (TransTrem is
   designed to hold intervals). Worked example: Drop D takes the low E
   from 78 N to 62 N (-3.6% of about 450 N), so every other string goes
   **+10.8 cents** on a Floyd.
2. **Creep, slow.** After any tuning change (not a bend), each string
   relaxes toward `-k_c · 100 · |ΔT_i| / T_i` with τ_c = 90 s. k_c is
   0.05 for a hardtail, 0.3 for VintageTrem, 0.2 for FloydRose, 0.4 for
   Bigsby, 0.1 for TransTrem and 0.2 for an acoustic top. Drop D on a
   vintage trem is 6 cents flat on the low E after about 3 minutes.
3. **Return error.** After a whammy excursion of |X| ≥ 1 semitone
   returns to centre, every string under the bridge moves by
   `±k_r · |X|` cents. k_r is 0.8 for VintageTrem, 0.6 for Bigsby, 0.15
   for FloydRose and 0.3 for TransTrem. The sign comes from
   `noiseHash(seed, excursionIndex)`. It is capped at ±8 cents in stock.

### 2.5 Bend memory

Heavy bends let the string slip at the ball end and the post wraps,
and stretch it permanently. Each bend release with peak B ≥ 50 cents
adds:

```
c_mem -= 0.04 · (B/100)² · (1 + 2σ) · (tuners.locking ? 0.3 : 1)
```

This is capped at -10 cents in stock. Twenty full-tone bends on fresh
strings is about **-9.6 cents**. On old strings it is about -3.2.

### 2.6 Capo bias

A capo pushes the string down behind the fret, which stretches it:

```
ΔL = h² / (2 g),   h = 0.5 · capo.pressure · setup_fret_height,
g  = capo gap (Trigger 6 mm, Screw 4 mm, Partial 6 mm; 5 mm when no capo part)
bias_i = 865.6 · (EA/T)_i · ΔL / L_c,   L_c = L · 2^(-capoFret/12)
```

The bias applies only to strings the capo clamps (`getCapoFretFor(s) >
0`). With a trigger capo at fret 2 it gives **high E +2.2, G +6.1, low E
+8.6 cents**. Retuning with the capo on stores
`capoComp_i = -bias_i`, which persists. Take the capo off and those
strings are flat by the same amount, exactly as on a real guitar tuned
with the capo on.

## 3. Retune actions

- **Retune string n**: clears settling (committing σ), stuck, backlash
  (and disarms), bend memory, creep, return error and `driftCents`
  (new `TuningEngine::clearDrift(s)`) for that string, and calls a new
  `CharacterEngine::retuneString(s)`. It sets `capoComp` as in 2.6. On
  a floating bridge the correction itself moves the other strings by
  2.4.1. It does not touch `realism_detune`, which is a preset property.
- **Retune all**: strings in order from lowest to highest pitch, each
  one's correction feeding 2.4.1 to the rest. A single pass on a
  floating bridge leaves a residual. Pressing again converges, which is
  the real Floyd tuning dance. Calls `CharacterEngine::retune()`.
- **Glide.** A ringing string moves to pitch over 250 ms. A silent
  string snaps.
- **Auto-retune** (`stability_auto_retune`):
  - **Off.**
  - **Idle** (default): Retune all once every string has been below
    level 1e-3 for 10 s. That is the between-songs tune-up. It never
    fires while anything rings, and only with the amount above 0 and an
    offset to clear, so the default leaves the character engine's drift
    of an existing preset alone.
  - **Transport stop.**
  - **Idle + stop.**
- **Threading.** Buttons write an atomic string mask
  (`requestRetune(mask)`) through `ui-wiring.md`'s command channel. The
  audio thread consumes it at block start.
- **Undo.** Retune is not undoable. Like tuning a real guitar, it is a
  performance action on runtime state, not a value edit
  (`action-and-undo.md` 3.17's reasoning). It is a MIDI Learn action
  target (a rising edge at CC ≥ 64 fires Retune all). It has no default
  keyboard binding, to stay clear of `gui-integration.md` 17.

## 4. Parameters

Family **`strings`**, the family introduced by `sustain-and-decay.md` 6
(`DECISIONS.md` "Phase 2b range families"). Every mechanism scale defaults to 1
(physical), so enabling the master amount gives the physically derived
behaviour at once.

| ID | Name | Stock | Advanced | Default | Unit |
|---|---|---|---|---|---|
| `stability_amount` | Tuning Instability | 0 – 1 | 0 – 4 | 0 | ratio |
| `stability_settling` | String Settling | 0 – 2 | 0 – 8 | 1 | × physical |
| `stability_nut_binding` | Nut Binding | 0 – 2 | 0 – 8 | 1 | × physical |
| `stability_backlash` | Tuner Backlash | 0 – 2 | 0 – 8 | 1 | × physical |
| `stability_saddle_creep` | Bridge / Saddle | 0 – 2 | 0 – 8 | 1 | × physical |
| `stability_bend_memory` | Bend Memory | 0 – 2 | 0 – 8 | 1 | × physical |
| `stability_capo_bias` | Capo Bias | 0 – 2 | 0 – 8 | 1 | × physical |
| `stability_auto_retune` | Auto Retune | choice | – | Idle | – |

**Net new parameters: +8.** The retune actions are commands, not
parameters. A parameter that fires on change would re-fire on every
automation read.

## 5. Engine insertion and per-string state

New class `StabilityModel` (`Source/Model/Playing/StabilityModel.h/.cpp`),
owned by `LuthierEngine`. It is control-rate only and never touches
audio samples.

- `setHardware (const TuningHardware&)` is called from
  `applyWorkshopGuitar` / `applySpec`. `TuningHardware` holds the tuner
  ratio, stability and locking, nut μ, bridge type, acoustic flag, capo
  pressure and gap, and per-string EA/T, L and T from `refreshStringPhysics`.
- `onTuningChanged (s, oldHz, newHz)` handles arming, equilibrium and
  creep targets. It is called from `setTuningPreset`,
  `applyTwelveStringTuning` and the detune / fine-tune bridge.
- `onBendSample (s, bendCents)` is fed each block with
  `midi.getStringBendCents(s)` for peak and release detection.
  `onWhammy (cents)` gets `whammy.getCentOffset`. `onPluck (s, v)` is
  called from `triggerNote`.
- `advance (numSamples, levels[], capoMask)`, once per block in
  `updatePerBlockModulation` **before** the frequency loop, updates W,
  the creep integrators, the release ramps and the retune glides, and
  writes `tuning.setStabilityCents (s, total_s)` only when the value
  changed by more than 1e-4 cents.
- When `stability_amount` is 0, `advance` returns at once, and
  `stabilityCents` is exactly 0.0, so it drops out of the sum.

**Per-string state:** σ, σ_committed, W, stuck, stuckReleaseRamp,
backlashArmed, backlash, creep and creepTarget, returnErr, bendMem,
capoComp, bendPeak, bendActive, pluckIndex, glide (from, to, remaining).
That is about 20 doubles × 12 strings, in `std::array`. The instance
also holds T_total, excursionIndex, idleSeconds and the atomic retune
mask.

**Caps, stock and advanced.** The stock caps (±50 total, ±6 stuck, -10
memory, ±8 return error) are x4 once any stability value is past its
stock end, which is only reachable with the family unlocked.

**Floating equilibrium bookkeeping.** The equilibrium offsets are held
unscaled, like the other mechanisms; a retune's correction, which is the
offset heard (already scaled), is divided back by amount x scale before
it is added to the other strings.

**Reset and determinism.** `reset()` zeroes every event offset and W,
and restores σ from `σ_committed`. σ commits on a retune only while the
host transport is stopped. Playback renders therefore start from the
same σ every time, while live noodling does wear strings in.

## 6. UI

- **CHARACTER tab** (`gui-integration.md` 4.4): the tuner-drift section
  becomes **TUNING STABILITY**. It holds the existing looseness slider
  and Retune-all button, plus the amount, the six scales and the
  auto-retune dropdown. It adds a **per-string offset strip**: 6-12
  horizontal bars at ±20 cents full scale, coloured by the dominant
  cause, with a dot glyph so it reads in monochrome
  (`gui-integration.md` 21). The tooltip gives the breakdown. Click a
  bar to retune that string. The strip drains at 10 Hz from per-string
  atomics and greys after 2 s stale (`gui-engine-dataflow.md`).
- **Easy mode headstock popover** (`gui-integration.md` 3.1): each
  string shows "+3 c" when |offset| ≥ 1 cent, with a Retune button.
- **Workshop inspector**: the tuners, nut and capo inspectors show the
  derived figure: "Backlash on low E: 7.1 c", "Nut binding after a full
  bend: +2.1 c", "Capo bias: +2.2 to +8.6 c".
- `gui-integration.md` 19 gains the row: Tuning stability |
  StabilityModel | Adv Col 4 CHARACTER TUNING STABILITY (+ Easy
  headstock popover).

## 7. State and serialization

- The eight parameters are in the preset's `parameters` block. Older
  presets load at amount 0 and play in tune.
- Per-string σ_committed and capoComp are **session** state. They go in
  the plugin state's extras object, next to `"character"`, as
  `"stability": {"sigma":[...], "capo_comp":[...]}` (each value written
  as 17-significant-digit text, because JSON's number formatting rounds
  and TS-15 asks for an exact round trip), so a DAW project
  reopens on the same strings. A preset load recomputes σ from
  `string_age` and clears capoComp.
- The other offsets are runtime-only and cleared by `reset()`.
- MIDI export sends nothing new. The offsets are a deterministic
  function of the exported notes, bends, whammy and capo.

## 8. Performance and realtime safety

- Budget: **0.02 units**. It is control rate: at most 12 strings × a
  few `exp` per block. `performance-budget.md` 1 gains a
  `StabilityModel` row.
- No allocation (fixed arrays) and no locks (atomic retune mask). Times
  are in seconds, converted per block from `numSamples / sr`.
- Pitch changes arrive through `setTargetFrequency`'s existing smoother.
  Ramps (ping 20 ms, backlash 30 ms, retune 250 ms) are advanced per
  block, so no single block step exceeds 0.5 cents.

## 9. Tests

Group `TuningStability`, `Source/Tests/TuningStabilityTests.cpp`. The
tests drive `LuthierEngine` offline at 48 kHz with 128-sample blocks
and read `getTuningEngine().getStringTuning(s).stabilityCents` and
`getStringFrequency(s)`.

- **TS-01 Off is inert.** At amount 0, a 60 s performance with bends,
  dives, a tuning change and a capo keeps `stabilityCents` at 0.0 on
  every string, and the audio is sample-identical to the build with
  `StabilityModel` bypassed.
- **TS-02 Nut binding.** A 200-cent bend and release on string 3 with
  bone (μ 0.35) leaves +2.1 ±0.3 cents. The graphite locking nut leaves
  ≤ 0.8 cents. A FloydRose bridge leaves 0.
- **TS-03 Ping.** After TS-02, plucks at v=1 release the bind within 20
  plucks. The releasing pluck index is identical across two runs, and
  differs for at least one of 8 other seeds. After release, |stuck| <
  0.05 cents within 25 ms.
- **TS-04 Backlash needs a downward approach.** Standard to Drop D arms
  the low E. A later 100-cent bend makes it flat by the 2.3 formula
  ±20%. After Retune, a further bend adds < 0.05 cents.
- **TS-05 Floating equilibrium.** On a FloydRose, Standard to Drop D
  moves each other string by +7 to +15 cents immediately. A hardtail
  moves them by < 0.5 cents. One Retune all leaves max |offset| ≤ 40% of
  the pre-retune maximum. Two leave ≤ 15%.
- **TS-06 Creep time constant.** After a tuning change on VintageTrem,
  the creep reaches 63 ±7% of its target at 90 s, and ≥ 95% at 270 s.
- **TS-07 Bend memory.** Twenty 200-cent bends on Fresh strings: the
  cumulative memory is -9.6 ±1 cents. With locking tuners it is ≤ 35% of
  that. Fifty bends never pass the -10 cent stock cap.
- **TS-08 Settling.** Fresh strings, 60 s at level ~0.3: the offset is
  between -3 and -8 cents. Then Retune, and 60 s more: the new offset is
  < 40% of the first. Old strings: |offset| < 0.5 cents.
- **TS-09 Capo bias.** A trigger capo at fret 2: every string is sharp
  by 1-12 cents, and G and low E match the 2.6 formula ±10%. A partial
  capo biases only masked strings. Retune with the capo on, then remove
  it: the clamped strings are flat by their bias ±0.2 cents.
- **TS-10 Retune scope.** Retune string 2 zeroes only string 2 (the
  others are unchanged to 1e-9). Retune all zeroes every string, and
  `CharacterEngine::getTunerDriftCents(s)` is 0 immediately after.
- **TS-11 Auto-retune on idle.** Offsets present and all strings silent:
  retune starts at 10 ±0.2 s. With a string ringing above 1e-3 it never
  fires. With Off it never fires.
- **TS-12 Reset and determinism.** Two renders of the same MIDI give
  identical per-block `stabilityCents` traces. `reset()` zeroes every
  offset and restores σ_committed. σ is unchanged by a render with
  transport running.
- **TS-13 Smooth glides.** During a 250 ms retune of a ringing string,
  no consecutive-block change in `stabilityCents` exceeds 0.5 cents,
  and the measured pitch moves monotonically.
- **TS-14 Sample-rate independence.** TS-06's time constant and TS-11's
  idle time hold within ±2% at 44.1 and 96 kHz.
- **TS-15 Serialization.** The session state round-trips σ_committed and
  capoComp exactly. A preset save contains no `stability` block. A
  preset load resets σ from `string_age`.
- **TS-16 Cost and safety.** 12 strings, every mechanism at advanced
  max, 10 minutes of random bends and dives: the total is clamped at
  ±200 cents, every value is finite, the cost is ≤ 0.02 units, and
  there are zero heap allocations.
