# CABLES AND BRANDS V2 SPEC

Every cable on a real rig is a small circuit. The guitar cable is a capacitor
across the pickup. A long cheap run picks up mains hum. A cable that flexes
crackles. Every pedal is a circuit too: its input impedance loads the pickup,
its noise floor comes from its parts, and its bypass switch may pop.

v1 already models the guitar cable as a capacitance set by `cable_length` and
`cable_quality` (`volume-knob-interaction.md` 2), and the noise floor already
has a cable-movement source and a ground-loop source (`noise-floor.md`). What
is missing is the choice a player makes when buying cables and pedals: which
tier. This file adds tiers, the cables between pedals, two utility pedals that
cure hum and loading, and a pedal tier that changes each pedal's voice and
noise. Every magnitude is stated honestly.

## 0. Ground rules

1. **Tiers are generic.** Unmodelled, Economy, Standard, Pro, Boutique. No
   brand, product or trademark name appears in code, data or UI. Users may
   type their own labels; the app never supplies one.
2. **Physics first.** A tier is a bundle of physical values (pF per metre,
   shield attenuation, input impedance, noise level), not a tone preset.
3. **Honest magnitudes.** Each number is tagged **M** (measured, or from a
   source already in the repo), **I** (derived here by a stated formula from M
   values) or **D** (design default, to be measured before shipping). Section 12
   lists the D values.
4. **Unmodelled is exactly zero.** It adds nothing beyond v1. Every v1 preset
   is Unmodelled and renders bit-identically (CB-M01).
5. **Seeded and RT-safe.** Randomness comes from the character seed
   (`character-wear.md` 1) with a fixed per-source salt, as `NoiseFloor` does
   with `kSaltCable`. Coefficients are computed on the message thread when the
   board compiles (`pedalboard-v2.md` 3.3), never in `process`.
6. **Structural, not automatable.** Patch-cable and pedal tiers, power mode and
   utility pedals are board state. The existing guitar-cable parameters
   (`cable_on`, `cable_length`, `cable_quality`) stay as they are.

## 1. Physics the tiers feed

### 1.1 Capacitance and resonance

A cable adds `C_cable = length x pF_per_m` across the pickup inductance, so
resonance falls as capacitance rises:

```
f_res = 1 / (2 pi sqrt( L x (C_pickup + C_cable) ))
```

`GuitarCircuit` (`volume-knob-interaction.md` 1.1) solves the full network with
the pots. This formula gives the unloaded reference that makes the tiers
checkable by hand. Inputs: `L = 3 H` (D, a placeholder for the factory pickup's
`Ls` in `part-acoustics.md`), `C_pickup = 100 pF` (M, `volume-knob-interaction.md` 1).

| Tier | pF/m | 0 m | 3 m | 6 m | 10 m |
|---|---|---|---|---|---|
| Boutique | 35 (D) | 9.2 kHz | 6.4 kHz | 5.2 kHz | 4.3 kHz |
| Pro | 52 (M, v1 Studio) | 9.2 kHz | 5.7 kHz | 4.5 kHz | 3.7 kHz |
| Standard | 98 (M, v1 Standard) | 9.2 kHz | 4.6 kHz | 3.5 kHz | 2.8 kHz |
| Economy | 160 (M, v1 Cheap) | 9.2 kHz | 3.8 kHz | 2.8 kHz | 2.2 kHz |

All values are I (unloaded). At 0 m every tier is identical. The gap opens
with length: at 6 m Economy sits at 2.8 kHz and Boutique at 5.2 kHz. That gap
is the "dull over a long run" the player pays to avoid, and it is why a short
patch cable costs nothing in tone.

### 1.2 Length and coiled cable

Length range is 0.3-10 m (`pedalboard-v2.md` 2.3). v1's `Vintage` choice is a
coiled cable at 220 pF/m (M). It is kept as a construction flag, not a tier:
Economy construction at 220 pF/m, with the microphonic penalty in 3.4.

## 2. Cable tiers

| Tier | pF/m | Shield attenuation (D) | Hum base, section 3.1 | Cable-movement scale, 3.4 |
|---|---|---|---|---|
| Unmodelled | v1 only | - | none | none |
| Economy | 160 | 20 dB | -18 dB re pickup hum, 3 m | x2 (+6 dB) |
| Standard | 98 | 30 dB | -28 dB, 3 m | x1 (0 dB) |
| Pro | 52 | 40 dB | -38 dB, 3 m | x0.5 (-6 dB) |
| Boutique | 35 | 45 dB | -43 dB, 3 m | x0.25 (-12 dB) |

- v1 `cable_quality` maps to tiers: Studio = Pro, Standard = Standard,
  Cheap = Economy, Vintage = Economy construction at 220 pF/m. The v1 choice IDs
  and pF values do not change.
- Shield attenuation is the reduction of 50/60 Hz coupling into the signal
  conductor, relative to an unshielded cable.

## 3. Hum, ground loops and the utilities that cure them

### 3.1 Hum on a cable

Mains couples into an unbalanced cable in proportion to its length and inversely
to its shield attenuation. Relative to the pickups' own hum (`noise_amp_buzz`):

```
hum_rel_dB = HUM_BASE - (shieldAtten - 20) + 20 log10(length / 3 m) + balanced
```

`HUM_BASE` = -18 dB (D). `balanced` = -20 dB for a balanced link, else 0.

| Tier | Length | Unbalanced | Balanced |
|---|---|---|---|
| Economy | 3 m | -18 dB | -38 dB |
| Economy | 10 m | -8 dB | -28 dB |
| Standard | 3 m | -28 dB | -48 dB |
| Standard | 10 m | -18 dB | -38 dB |
| Pro | 3 m | -38 dB | -58 dB |
| Boutique | 10 m | -33 dB | -53 dB |

Read it as "the cable's hum sits this far below the pickup's own hum". A
humbucker cancels the pickup's hum but not the cable's, so a humbucker does not
fix a long cheap cable. The hum is a 50/60 Hz line plus harmonics 2-5 in the
rectifier shape of `noise-floor.md` 2.5; it reuses that wavetable at the
cable's read rate.

### 3.2 Ground loops and the count of grounded units

`noise-floor.md` 2.5 has one ground-loop level at the amp input, set by
`noise_ground_loop`. v2 makes it depend on wiring. Let `G` be the number of
powered, grounded units joined by an unbalanced shield path.

- The loop rises 3 dB for each path beyond the first, capped at +9 dB (D). One
  powered unit has no loop; five unbalanced units reach +9 dB.
- A balanced link removes that link from `G`.
- Ground Isolator **Lift** removes one path from `G`. **Isolate** removes the
  loop component with -30 dB residual (D), and costs 0.5 dB insertion loss,
  flat to 20 kHz within 0.2 dB (D).

The loop is independent of pickup and volume (`noise-floor.md` 2.5), so only a
change in `G` cures it.

### 3.3 Utility pedals: Buffer and Ground Isolator

The two new utility types in `pedalboard-v2.md` 4.1.

**Buffer** (type 23): unity gain (+/-0.05 dB, 20 Hz-20 kHz). Input impedance
1 MOhm and output impedance 1 kOhm (D); the input is what isolates the cable's
capacitance from the pickup. Noise -100 dBFS (D), seeded. Latency 0. Its
bypass is always buffered by definition.

**Ground Isolator** (type 24), parameter Lift (Off, Lift, Isolate). Off is
transparent. Lift removes one path from `G` and cuts shield-coupled hum by 15 dB
(D). Isolate as in 3.2. Latency 0.

### 3.4 Microphonic crackle

`noise_cable_movement` is a triboelectric source at -42 dB mean event peak for
a 3 m Standard cable (`NoiseFloor.cpp`, `kCablePeakDb`). v2 scales it by the
tier's cable-movement factor (section 2). A coiled cable adds +3 dB (D), since
the coil flexes at every pick. This is the tier effect audible on the cheapest
cable, which is the intent.

## 4. Buffered input and loading

### 4.1 What loads the pickup

The pickup sees the cable capacitance and whatever is at the end of the cable:
the first pedal's input impedance, or the amp's `amp_input_impedance` (v1,
1 MOhm default). A low input impedance damps the resonance peak.

### 4.2 Input impedance by tier (D)

| Tier | Input impedance | Effect on a passive pickup, 3 m Standard cable |
|---|---|---|
| Unmodelled | v1 `amp_input_impedance` | none |
| Economy | 100 kOhm | strongest: the peak drops and damps |
| Standard | 470 kOhm | moderate |
| Pro, Boutique | 1 MOhm | light |

A pedal in **buffered bypass** (`pedalboard-v2.md` 3.5) presents its own 1 MOhm
buffer, so the cable before it is not loaded by the next pedal whatever that
pedal's tier. A pedal in **true bypass** presents the next pedal's input, so tier
matters. The Buffer pedal (3.3) is the explicit version.

### 4.3 Active pickups

`circuit_active` (`volume-knob-interaction.md` 1.4) already buffers the pickup,
so cable capacitance does not reach it. Tiers still set hum (3.1) and
cable-movement noise (3.4).

## 5. Pedal tiers

A pedal's tier, separate from its cables, sets four things: voicing offset,
noise floor, input impedance (4.2) and true-bypass pop. The pedal's type,
parameters and core sound do not change; the tier is a small, bounded delta.

### 5.1 Tier table

| Tier | Voicing offset | Noise floor, output (D) | Input impedance | True-bypass pop peak (D) |
|---|---|---|---|---|
| Unmodelled | 0 dB | none | v1 | none (v1 soft bypass) |
| Economy | up to +/-3 dB, biased to the top | -72 dBFS | 100 kOhm | -34 dBFS |
| Standard | 0 dB (reference) | -84 dBFS | 470 kOhm | -40 dBFS |
| Pro | up to +/-1.5 dB | -92 dBFS | 1 MOhm | -44 dBFS |
| Boutique | up to +/-1.5 dB, opposite tilt | -96 dBFS | 1 MOhm | -46 dBFS |

Noise is referred to pedal output at unity gain. Drive pedals add 0.5 dB of
hiss per dB of drive gain (D).

### 5.2 Voicing offsets

A voicing is a fixed EQ after the pedal's output, one precomputed biquad per
offset, bounded to +/-3 dB (CB-10). Offsets are per type, in `PedalTiers.cpp`,
with the design reason in a comment. Unmodelled offsets are never applied. For
example, Economy Overdrive is a +2 dB shelf above 2 kHz and a -1 dB dip at
300 Hz (D). It is a voicing, not a model of any product.

### 5.3 Pedal noise

Each non-Unmodelled pedal owns one white source, seeded from the character seed
XOR a salt from its instance number, filtered by a one-pole highpass at 20 Hz so
it reads as hiss, and summed at the tier's level at the pedal output. It is a
drone, so it stays out of `NoiseEngine` (`pick-noise.md` 1).

### 5.4 True-bypass pop

`pedalboard-v2.md` 3.5 defines true bypass. The pop is a seeded 4 ms transient,
half-Hann shaped, peaking at the tier's level in table 5.1 at the pedal output.
A real relay pop is a DC step plus a short ring; the model reproduces peak and
width only. Off when `popEnabled` is off. The levels are D (section 12).

## 6. Parameters, state and power

### 6.1 Existing, unchanged

| ID | Type | Default | Role in v2 |
|---|---|---|---|
| `cable_on` | bool | true | Guitar cable in or out |
| `cable_length` | float m | 3.0 | Input cable length |
| `cable_quality` | choice | Standard | Input cable tier (section 2 mapping) |
| `noise_ground_loop` | float | 0 | Level, scaled by `G` (3.2) |
| `noise_cable_movement` | float | 0 | Source level, scaled by tier (3.4) |

No new host parameter is added. The new state lives in the board block
(`pedalboard-v2.md` 5.1): per patch cable `tier`, `lengthM`, `balanced`,
`coiled`; per pedal `tier`, `bypassMode`, `popEnabled`; per board `powerMode`.
Utility pedals keep `lift` and `isolate` in their pedal state.

### 6.2 Power mode (board state)

- **Daisy**: one shared supply. Each pedal adds a mains line at -60 dBFS (D)
  and raises `G` by one.
- **Isolated**: each pedal has its own supply. Mains line -86 dBFS per pedal (D);
  `G` does not rise with pedal count.
- **Mixed**: per pedal. Ship Daisy and Isolated first.

The mains line uses the `noise_mains_hz` choice.

## 7. Migration

- v1 presets have no patch cables, so nothing changes. `cable_quality` and
  `cable_length` keep their values and meaning.
- Migrated patch cables and pedals are Unmodelled; new ones are Standard.
- Migrated boards use Daisy power, the v1 behaviour, which renders the same
  when the hum sources are zero.
- Buffer and Ground Isolator appear only in new boards.

## 8. Edition

| Feature | Free | Pro |
|---|---|---|
| Patch-cable and pedal tiers | Standard only; Unmodelled read-only for migrated | all |
| Buffer, Ground Isolator | no | yes |
| Power mode | Daisy only | all |
| Input cable `cable_quality` | all four v1 choices | all four |

Free shows the tier popovers greyed out with "Pro".

## 9. Performance budget

Units per `performance-budget.md` 0.

- Per pedal with a tier: 0.01 units (precomputed biquads and one seeded source).
- Buffer 0.01; Ground Isolator 0.02 (fixed residual filter in Isolate).
- Cable hum: 0.005 units per cable with hum on. 24 cables: 0.12.
- Whole section: at most about 0.3 units on a full board, inside the 6-unit
  board budget (`pedalboard-v2.md` 7).

## 10. Tests

- **CB-01 (capacitance).** Table 1.1 matches the formula within 2 %; `GuitarCircuit`
  with a 100 pF pickup and no pots matches it within 2 %.
- **CB-02 (ordering).** At fixed length, resonance falls strictly from Boutique to
  Economy; at fixed tier, it falls strictly with length.
- **CB-03 (hum table).** Every row of 3.1 holds within 1 dB, formula and render.
- **CB-04 (balanced).** A balanced link reduces hum by 20 dB within 1 dB.
- **CB-05 (ground count).** Each added unbalanced powered unit adds 3 dB, to +9 dB.
  Lift removes one path; Isolate reduces the loop by 30 dB within 1 dB.
- **CB-06 (microphonic).** Cable-movement level per tier equals v1 times the
  section 2 scale within 0.5 dB; a coiled cable adds 3 dB within 0.5 dB.
- **CB-07 (buffer isolates).** A 10 m Economy cable before a Buffer matches 0 m
  at 5 kHz within 0.5 dB (= PB-09).
- **CB-08 (pop).** Peak per tier within 1 dB of table 5.1; width 2-6 ms; none
  with `popEnabled` off.
- **CB-09 (loading).** Economy's 100 kOhm loads a passive pickup at volume 10
  more than 1 MOhm; the 5 kHz difference is at least 1.5 dB.
- **CB-10 (voicing bound).** Every offset within +/-3 dB over 20 Hz-20 kHz;
  Unmodelled is bit-exact 0 dB.
- **CB-11 (noise).** Each tier within 2 dB of table 5.1. Same seed gives identical
  renders; a different seed gives the same RMS within 0.1 dB.
- **CB-12 (power).** Daisy and Isolated differ in the mains line by at least
  20 dB for the same pedal count.
- **CB-13 (RT).** 60 s with tiers changing mid-run: zero audio-thread allocations,
  and no coefficient recomputed in `process`.
- **CB-M01 (migration).** An all-Unmodelled v1 preset renders bit-identically
  (shared with PB-M01).
- **CB-E01 (Free).** Free writes Standard tiers only and offers no Buffer or Isolator.
- **CB-U01 (manual).** The popovers read correctly to a player who has not read
  this spec, at 200 % UI scale.

## 11. Effort and dependencies

| Work | ED |
|---|---|
| Tiers, cable model, resonance checks (CB-01, 02) | 4 |
| Hum, ground count, power mode (CB-03 to 05, 12) | 6 |
| Microphonic scale and coiled flag (CB-06) | 1 |
| Buffer and Ground Isolator DSP (CB-07) | 4 |
| Pedal tiers: voicing, noise, loading (CB-09 to 11) | 8 |
| True-bypass pop (CB-08) | 2 |
| State, migration, Free rules (CB-E01, M01) | 3 |
| Popovers (CB-U01; `pedalboard-v2.md` 2.3, 2.4) | 4 |
| RT checks and tests (CB-13) | 3 |
| **Total** | **35 ED, about 7 weeks** |

Dependencies: `pedalboard-v2.md` (board state, bypass modes, pedal types 23-25);
`volume-knob-interaction.md` and `GuitarCircuit` (CB-01 cross-check);
`noise-floor.md` (ground loop, cable movement; changes are additive: a tier
scale and the `G` input); `character-wear.md` 1 (seed).

## 12. Open questions and values to measure

1. **Pickup inductance.** Replace `L = 3 H` with the factory pickup's `Ls` and
   re-derive table 1.1.
2. **Pop.** Measure a true-bypass relay with a reference cable; tag M.
3. **Shields.** 20/30/40/45 dB are D; measure sample cables to replace them.
4. **Hum base.** -18 dB (3.1) must sit below the pickup's own hum on a real 10 m
   run through a real amp.
5. **Voicings.** Each offset is a design choice, never a model of a product.
6. **Mixed power mode.** Worth the UI, or is Daisy/Isolated enough? Recommend
   Daisy and Isolated first.
7. **Guitar cable in the popover.** Show the four v1 names in the patch-cable
   popover so the player learns one vocabulary. Recommend yes.
