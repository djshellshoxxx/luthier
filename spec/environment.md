# ENVIRONMENT SPEC

A guitar is a steel-and-wood machine held at a precise tension, and both
materials respond to the room. Carry a guitar from a cold car into a warm
room and the wound strings go flat within a minute, then drift partway
back as the neck warms. Leave an acoustic in a humid summer club and the
top bellies up, the action rises and the fretted notes play sharp. In a
dry winter the neck back-bows and the low frets start to buzz.

`character-wear.md` 9 already names this ("Environmental controls"), and
`CharacterEngine` implements a placeholder for it. That placeholder has
three problems:

- Temperature is three enum steps (cold / room / warm) with a fixed
  0.125 cents/K offset, scaled by the character amount. Real steel on
  wood moves about 1 cent/K on a plain string and about 4 cents/K on a
  wound one, which is 8 to 30 times more.
- Humidity has two multipliers, `getHumidityQMultiplier` and
  `getHumidityComplianceMultiplier`, that **nothing in the engine
  reads**. A search of `Source/` finds only their definitions and
  `CharacterTests.cpp`.
- There is no sense of time. A guitar does not reach the room's
  temperature the instant the room changes.

This file replaces that placeholder with a physically derived, lagged,
seekable model. It changes only things the engine already has: the
per-string tuning offset, the setup geometry `fret-buzz.md` reads, and
the body mode frequencies and Qs.

## 0. Ground rules

1. **Derived from the string's own numbers.** Thermal detuning is
   computed from each string's tension, core area and modulus in
   `StringSpec`. There is no per-string fudge table.
2. **Everything lags.** Ambient conditions are what the user sets. The
   instrument's parts follow them with physical time constants: string
   wire in seconds, neck in minutes, wood moisture in days.
3. **Room defaults are a bit-exact no-op.** At 22 °C ambient, tuned at
   22 °C, 45 % RH and the Static profile, every delta is exactly 0.
4. **Environment is state, not wear.** It is **not** scaled by the
   character amount (`macro_character`). A player who turns character
   down to zero still has a guitar that responds to the room.
5. **Deterministic on the host timeline.** The same project position
   always gives the same instrument state, so a bounce matches playback.
6. **Honest magnitudes.** A 10 K warm-up detunes a plain high E by about
   10 cents and a wound low E by about 40. Humidity moves an acoustic's
   action by tenths of a millimetre over days, and barely at all within
   one session.

## 1. What moves where

| Concern | Before | After |
|---|---|---|
| Tuner drift LFO and its settling envelope (`character-wear.md` 4) | `CharacterEngine` | Stays in `CharacterEngine`, unchanged |
| Temperature offset (`getTemperatureOffsetCents`) | `CharacterEngine`, added inside `getTunerDriftCents` | Removed from `CharacterEngine`. Now `EnvironmentModel` (section 4) |
| Humidity Q and compliance multipliers | `CharacterEngine`, never consumed | Removed. Now `EnvironmentModel`, and actually applied |
| Session clock (`sessionSeconds`) | `CharacterEngine::advance` | `CharacterEngine` keeps it for the drift envelope. The environment has its own clock (section 3.4) |
| Retune button | `CharacterEngine::retune()` | The same command also calls `EnvironmentModel::retune()` |
| Environment UI section | CHARACTER tab (`character-wear.md` 10) | The CHARACTER tab's **ENVIRONMENT** group (section 7) |
| Test "temperature step of 20 K" (`character-wear.md` 12) | `CharacterTests` `temperatureProducesTheExpectedOffset` | Replaced by ENV-02 |

`tuning-stability.md` (23i) owns string settling, nut binding and tuner
backlash. This file owns only thermal and moisture effects.

## 2. Physics

### 2.1 Thermal detuning

A string stretched over a neck has strain `ε_i = T_i / (E_i · A_core,i)`.
Take the high E: 0.010" steel at 72 N gives ε ≈ 7.1e-3. Take a wound low
E: a 0.53 mm core at 76 N gives ε ≈ 1.75e-3. The small core strain is
why wound strings are so sensitive. When string and neck temperatures
move by `ΔT_s` and `ΔT_n` from their state when the guitar was tuned:

```
Δε_i   = −α_s,i · ΔT_s,i + α_n · ΔT_n
cents_i = 865.6 · Δε_i / ε_i          // 1200/ln2 · ½, since f ∝ √T and ΔT/T = Δε/ε
```

| Constant | Value | Source |
|---|---|---|
| `α_s` steel core (every steel-cored material, including bronze-wound) | 12e-6 /K | Carbon steel |
| `α_s` nylon / fluorocarbon (trebles and floss cores) | 80e-6 / 120e-6 /K | Polymer tables |
| `α_n` neck, along the grain | 4e-6 /K | Longitudinal wood, 3-5e-6 |

Steady state (string and neck both moved by ΔT): high E ≈ −0.98
cents/K, low E ≈ −4.0 cents/K. Heat makes strings go flat. In the
transient, the string alone has warmed, so the slope is 1.5× as steep,
and then it recovers partway as the neck catches up.

### 2.2 Time constants

- **String wire:** `τ_s = 5 s · (d_i / 0.25 mm)`. This is lumped
  convection of a thin wire in still air: about 5 s for a plain high E
  and about 23 s for a wound low E.
- **Neck:** `τ_n = 900 s`. **Body wood:** `τ_b = 1200 s`.
- **Wood moisture:** `τ_h = 86 400 s` (a day). The `env_humidity_pct`
  parameter is the humidity the guitar has **acclimatised to**. Session
  profiles move the ambient humidity, and the wood follows it only
  through `τ_h`.

### 2.3 Moisture content

Equilibrium moisture content (EMC) of wood against acclimatised RH,
piecewise linear through the sorption isotherm at room temperature:

| RH % | 0 | 10 | 20 | 30 | 45 | 55 | 65 | 75 | 85 | 95 | 100 |
|---|---|---|---|---|---|---|---|---|---|---|---|
| EMC % | 0 | 2.5 | 4.5 | 6.0 | 8.5 | 10.0 | 12.0 | 14.5 | 18.0 | 23.0 | 27.0 |

`ΔMC = EMC(RH_a) − 8.5`. The reference is 45 % RH, which is the
storage humidity the industry recommends. The SETUP group's values
(`fret-buzz.md` 1) are what a technician measured at that humidity.

### 2.4 Humidity: setup geometry

- **Relief:** `Δrelief = +0.015 mm · ΔMC`. A humid neck gains forward
  bow; a dry one back-bows.
- **Top rise at the bridge:** `Δh_b = k_top · ΔMC`, with `k_top` by
  chambering (`part-acoustics.md` 2.1): acoustic 0.10, hollow 0.06,
  semi-hollow 0.02, chambered 0.005, solid 0 mm per % MC.
- **Action at fret 12**, both sides: `Δa12 = 0.5 · Δh_b`.

Going from 30 % to 70 % RH (ΔMC ≈ 7) on an acoustic gives +0.35 mm of
action and +0.10 mm of relief. That matches the seasonal changes
technicians report. A solidbody gets only the relief change.

### 2.5 Fretting stretch (intonation)

Pressing a string down stretches it by
`ΔL ≈ (h_n²/2)·(1/x_n + 1/(L − x_n))`, where `h_n` is the open-string
clearance at fret `n` from `FretBuzzModel`'s geometry. The engine does
not model the absolute stretch; its tuning already includes it. This
file adds only **the change** caused by the environment's geometry
deltas:

```
fretCents_i(n) = 865.6 · (ΔL_env − ΔL_nominal) / (L · ε_i)
```

Adding 0.35 mm of action on a wound low E makes fret 12 about 3.6 cents
sharp; on the high E, about 0.9 cents.

### 2.6 Humidity and temperature: body

- **Plate modes:** `f × (1 − 0.008·ΔMC) × (1 − 0.002·(T_b − 22))`. Wood
  stiffness falls and density rises with moisture; stiffness also falls
  with heat.
- **Plate Q:** `Q × 1 / (1 + 0.04·ΔMC)`, clamped to [0.5, 2]. Wood loss
  (tan δ) rises about 4 % for each % of moisture content.
- **Air (Helmholtz) mode:** `× sqrt((273.15 + T_amb) / 295.15)`, from the
  speed of sound. For acoustic and hollow bodies it is also multiplied by
  `(1 − 0.002·ΔMC)`, because a softer top adds compliance.

These multipliers reach `BodyEngine` and `BodyCouplingBank`
(`body-coupling.md`). A **wolf-note shift** therefore emerges on its
own: the wolf sits where a string partial meets a body mode, and the
body mode has moved.

### 2.7 Corrosion hook

`k_RH = clamp(1 + 0.03·(RH_a − 45), 0.5, 3.0)` is published to
`string-aging.md` 3.1.

## 3. Profiles, reference and clock

### 3.1 Session drift profiles (`env_profile`)

Ambient temperature follows
`T_amb(t) = T_set + ΔT·(1 − e^(−t/τ_p))`, and ambient humidity follows
the same form. The wood's acclimatised humidity trails ambient through
`τ_h`.

| Profile | ΔT (K) | ΔRH (%) | τ_p | Scenario |
|---|---|---|---|---|
| Static | 0 | 0 | – | Default |
| Stage lights | +10 | −10 | 600 s | Warming up under a rig |
| Outdoor evening | −8 | +15 | 1800 s | Sun goes down |
| Cold case to room | Instrument starts 17 K below ambient | 0 | – | Tuned on arrival, then warms |
| Air-conditioned studio | −4 | −15 | 1200 s | |
| Humid club | +6 | +25 | 900 s | |

For "Cold case to room", the string and neck start at `T_set − 17` and
the reference is that initial state, so the guitar starts in tune and
goes flat as it warms.

### 3.2 Reference ("tuned at")

The reference is a pair, `(T_s,ref, T_n,ref)`. After `reset()` both are
`env_tuned_at_c`, except in the Cold case profile, and the parts start
at their steady state. **Retune** sets the reference to the parts'
current temperatures, which zeroes every offset instantly. It then
writes `env_tuned_at_c` to the current string temperature, rounded to
0.1 °C, for display. That write is one undo entry.

### 3.3 Closed form

Every lag responding to an exponential ambient input has a closed form:

```
T(t) = T0 + ΔT·[1 − (τ_p·e^(−t/τ_p) − τ·e^(−t/τ)) / (τ_p − τ)]      (τ ≠ τ_p; limit form at equality)
```

The profile part is evaluated from this analytically at any `t`.
Automation of `env_temperature_c` and `env_humidity_pct` is added by
superposition, through block-rate one-poles with the same `τ`s. The
system is linear, so the sum is exact. After a seek, the automation part
restarts at its steady state (documented behaviour).

### 3.4 Clock (`env_clock`)

- **Host timeline** (default): `t` is the playhead's
  `getTimeInSeconds()` while playing. When the host is stopped, `t`
  free-runs from the last position.
- **Free-running:** `t` is seconds since `reset()`.

Because the profile part is analytic, the host timeline is seekable and
bounce-exact.

## 4. Engine insertion

- **New `EnvironmentModel`** (`Source/Character/EnvironmentModel.h/.cpp`)
  owned by `LuthierEngine` next to `character`. It is POD with
  fixed-size arrays. Its output struct:
  `EnvironmentState { openCents[kMaxStrings], invStrain[kMaxStrings],
  reliefDeltaMm, actionDeltaMm, plateFreqMul, plateQMul, airFreqMul,
  corrosionRate, stringTempC[kMaxStrings], neckTempC, rhAcclimatised }`.
- **`refreshStringPhysics()`** calls
  `environment.setStringMaterial(i, ε_i, α_s,i, d_i)` using the
  `StringSpec` it has just computed, on the same thread discipline as
  `setPhysical`.
- **`LuthierEngine::processBlock`**, at the `character.advance(...)` site:
  `environment.advance(seconds, hostSeconds)`. The per-string loop
  becomes
  `tuning.setCharacterDriftCents(s, (character.isEnabled() ? character.getTunerDriftCents(s) : 0) + env.openCents[s])`,
  and it now runs even when character is disabled.
- **Per-block pitch loop** (the `tuning.computeFrequency(s, fret, bend + whammy + vib + …)` call):
  add `env.fretCents(s, currentFret[s])` to the cents argument.
- **Setup:** `setSetupGeometry` keeps the value it was given in
  `requestedSetup`. A new private `applyEffectiveSetup()` sets
  `fretBuzzModel.setGeometry(requested + env deltas)` whenever a delta
  moves by more than 0.005 mm. The buzz heatmap then shows
  environment-driven buzz.
- **Body:** `BodyEngine::setRuntimeScaling(plateFreqMul, airFreqMul, plateQMul) noexcept`
  is new, and shared with `body-coupling.md`. At block start (after
  `applyStagedBank`) it re-sets each `ModalResonator` from
  `activeModes[k]` times the multipliers, only when a multiplier has
  moved by more than 0.05 %. Air modes are tagged:
  `BodyModels::buildModes` sets a new `BodyMode::isAir` flag on the two
  `airHz` modes. `getAirResonanceHz()` returns the scaled value, so
  character dead spots follow it. In Convolution mode an IR cannot be
  warped; the modal half (Hybrid) and the coupling bank still move, and
  the UI says so (section 7).

## 5. Parameters

Appended at the end of the layout. Net **+5** (see `body-coupling.md` 4
for this build's totals).

| ID | Name | Stock | Advanced | Default | Unit |
|---|---|---|---|---|---|
| `env_temperature_c` | Ambient Temperature | 5 – 40 | −30 – 70 | 22 | °C |
| `env_tuned_at_c` | Tuned At | 5 – 40 | −30 – 70 | 22 | °C |
| `env_humidity_pct` | Humidity (acclimatised) | 20 – 85 | 5 – 100 | 45 | % RH |
| `env_profile` | Session Profile | choice, section 3.1 | – | Static | – |
| `env_clock` | Profile Clock | choice: Host timeline / Free-running | – | Host timeline | – |

The three float parameters form a new range family, **`environment`**,
appended to `RangeFamily` after `modulation` (and after `strings` if
`string-aging.md` landed first). This amends `file-formats.md` 2 and
`advanced-ranges.md` 2 in the same backward-compatible way:
`string-aging.md` 4 gives the argument. The advanced range is where
the model stays stable. Freezing, and nylon's glass transition, are not
modelled, and the tooltip says so.

## 6. Serialization and migration

- *As built:* `env_humidity_pct` sets the acclimatised humidity directly
  (it is a statement of how the guitar has been kept, not an ambient to
  follow); only a session profile's humidity reaches the wood through
  `τ_h`. Changing the profile mid-play is a new scene: the parts restart at
  steady state and the reference is re-derived as on `reset()`, with the
  profile clock unchanged so a bounce still matches. The air modes keep
  their own Q; the plate-Q multiplier reaches only plate modes, and
  `body_mode_q_scale` reaches both.
- The parameters are stored in `parameters`. The reference pair and the
  automation lag state are stored in the `character` block as
  `"environment": {"ref_string_c": [...], "ref_neck_c": x}`. When the
  block is absent, the reference is derived from `env_tuned_at_c`.
- **Legacy load.** `CharacterEngine::fromVar` still reads `temperature`
  and `humidity` for one schema, and the loader converts them:
  - Temperature: cold / warm is converted to the `env_temperature_c`
    that makes the mean steady-state offset across the preset's strings
    equal the old `±2.5 · amount` cents, with tuned-at 22 and profile
    Static. At the default amount of 0.25 this is a fraction of a
    kelvin, and the preset keeps its sound.
  - Humidity: the old multipliers never reached audio, so dry / humid
    maps to **45 %**. Mapping to 30 or 70 would change the sound of a
    saved preset. The mapping is logged at info level
    (`error-recovery.md`).
- The `CharacterEngine` temperature and humidity accessors, and their
  `toVar` keys, are removed once the migration test (ENV-11) passes.

## 7. UI

- **Column 4 CHARACTER → ENVIRONMENT group**, replacing
  `character-wear.md` 10's environment section. It holds:
  - ambient temperature, tuned at, humidity, profile and clock controls;
  - **Retune** (shared with tuner drift);
  - live readouts of string, neck and body temperature, acclimatised
    RH, and per-string cents, drained at 10 Hz and greyed after 2 s
    stale (`gui-engine-dataflow.md`);
  - a 60 s sparkline of the low-E offset.

  When the body is in Convolution mode, the group shows the note "Body
  shift applies to modal bodies and to string coupling" (`gui-integration.md`
  14 empty-state style).
- The **SETUP** group's heatmap already reflects the effective geometry.
  Its action and relief readouts show the environment delta as a
  secondary `+0.12 mm (humidity)` line.
- The CHARACTER padlock covers the `environment` family.
- Easy mode: nothing. Environment is an Advanced setting.

## 8. Performance and realtime safety

- The cost is block-rate only: about 3 `exp()` per string plus a handful
  per model, and 48 resonator re-designs only when a body multiplier
  moves by more than 0.05 %. The budget is **0.02 units**, which adds an
  `EnvironmentModel` row to `performance-budget.md`.
- All math is in `double`. There is no allocation and no lock. The
  host time is read from the `AudioPlayHead` the processor already
  queries.
- **Smoothing is physical:** a step in automation reaches the string
  only through `τ_s ≥ 5 s`, so a step cannot click. The pitch goes
  through `TuningEngine` and the string's existing glide.
- There is a NaN guard on every output. The multipliers are clamped
  (frequency to [0.7, 1.3], Q to [0.5, 2]), and so are the offsets
  (±300 cents, advanced extremes only).

## 9. Tests

- **ENV-01 Room is a no-op.** Defaults render bit-identical to a build
  with `EnvironmentModel` disabled (6-string chord, 4 s).
- **ENV-02 Steady slope.** Tuned at 22 °C, ambient 32 °C, Static, after
  settling (t = 2 h; the model is reset with those inputs, so the parts
  start at their steady state - after a live step, 2 h is 8 neck time
  constants and leaves 3e-4 of the neck's part, above the 1e-6 bound): each string's offset equals
  `865.6·(−(α_s − α_n)·10)/ε_i` within 1e-6 relative. The plain high E
  (0.010" nickel-plated steel, 648 mm) is between −13 and −7 cents; the
  wound low E is between −48 and −32 cents.
- **ENV-03 Transient overshoot.** Cold case to room, low E: the offset
  reaches its minimum between 40 s and 300 s. The magnitude of that
  minimum is at least 1.3 times the final offset. At 60 min the offset
  is within 5 % of steady state.
- **ENV-04 Air mode.** Going from 22 °C to 42 °C ambient scales the air
  resonance by 1.033 ± 0.002. Plate modes are unchanged within 0.5 %
  before the body temperature follows.
- **ENV-05 Humidity geometry.** Acclimatised 75 % RH gives: plate
  frequency multiplier 0.952 ± 0.002, relief +0.090 ± 0.001 mm, action
  +0.30 ± 0.01 mm on the acoustic body and 0 on the solid body.
- **ENV-06 Wood is slow.** Over 90 min of the Humid club profile, the
  acclimatised RH moves by less than 1.75 % RH (about 6 % of the +25 %
  ambient change).
- **ENV-07 Fretting stretch.** With +0.35 mm action on the low E,
  fret 12's pitch minus the open offset is +3.6 ± 0.6 cents, and it
  matches the section 2.5 formula within 2 %.
- **ENV-08 Dry neck buzzes more.** Player-friendly setup at 25 % RH
  versus 45 %: the `FretBuzzModel` maximum excess on the low E at fret 3
  (velocity 110) rises by at least 0.01 mm. *Amended in the build:* 25 %
  RH takes 0.049 mm off the relief (2.4), and `fret-buzz.md` 1's parabolic
  board, deepest at fret 7, passes only about a quarter of that to the frets
  just above fret 3 (measured 0.013 mm). 0.04 mm would need the whole relief
  change at those frets.
- **ENV-09 Seek determinism.** Host clock, Stage lights profile:
  rendering bars 9 to 17 after a seek and inside a render from bar 1
  gives per-string offsets within 1e-9 at every block of bars 9 to 17.
- **ENV-10 Retune.** After 10 min of Stage lights, Retune makes every
  offset 0 ± 0.01 cents. 5 min later the offsets are non-zero again,
  following the profile.
- **ENV-11 Legacy migration.** A preset with `character.temperature =
  cold` and amount 0.25 loads with a mean string offset of +0.625 ± 0.05
  cents. A preset with `humidity = humid` loads with 45 % and renders
  identically to one with `normal`.
- **ENV-12 No steps.** Stepping `env_temperature_c` from 22 to 40 inside
  one block changes no string's pitch by more than 0.1 cents per 512-sample
  block at 48 kHz. *Amended in the build:* 0.05 was below the physics. The
  fastest string, the plain high E (τ_s = 5.08 s, ε = 7.16e-3), slews at
  865.6 · 12e-6 · 18 / (7.16e-3 · 5.08) ≈ 5.1 cents/s when the string alone
  has warmed, which is 0.055 cents in a 10.7 ms block. That is a glide far
  under the pitch JND, not a step.
- **ENV-13 Wolf follows the body.** With `body-coupling.md` enabled, a
  plate multiplier of 0.97 moves the measured wolf frequency by
  −3 % ± 0.5 % (the same measurement as BC-05).
- **ENV-14 Independence from character.** With `macro_character = 0`,
  ENV-02 still passes.
- **ENV-15 Budget and safety.** `EnvironmentModel` costs ≤ 0.02 units at
  12 strings with every parameter on an LFO. Zero audio-thread
  allocations. There are no NaNs at any corner of the advanced ranges
  (1000 random corners, 5 s each).
