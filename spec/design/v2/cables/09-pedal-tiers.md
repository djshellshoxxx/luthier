# CB-09, CB-10, CB-11: Pedal tiers, voicing, noise and loading

**Items:** CB-09, CB-10, CB-11
**ED:** 8
**Source:** `cables-and-brands.md` 4.1, 4.2, 5.1, 5.2, 5.3, 10, 11

## Summary

Give each pedal a tier that sets a bounded voicing EQ, a seeded output noise floor and an input impedance that loads the pickup chain.

## Status

Missing. v1 has `amp_input_impedance` (the amp's load) and the noise floor for the guitar path. No per-pedal tier, voicing, pedal noise or pedal input impedance exists.

## User-facing behaviour

- A tier changes how a pedal sounds slightly (voicing, up to +/-3 dB at some frequencies), how much hiss it adds, and how much it loads the pickup.
- Economy pedals hiss more and load a passive pickup more, which dulls the top at high volume.
- The pedal's type, parameters and core sound do not change.
- Unmodelled: no voicing, no hiss, v1 input behaviour. Migrated pedals are Unmodelled.

## Engine / DSP design

**Signal order at a pedal's output** (decision): pedal core -> voicing biquad(s) -> pedal noise sum -> true-bypass pop (file 02). Voicing and noise apply only when the pedal is engaged; in soft or buffered bypass the pedal's circuit is out of the path. Flagged as a design decision.

### Voicing (5.2)

- One voicing per (type, tier), defined in `PedalTiers.cpp` with the reason in a comment. Offsets bounded to +/-3 dB.
- Example from the spec: Economy Overdrive is a +2 dB high shelf above 2 kHz and a -1 dB dip at 300 Hz (D). That is two shapes, so a voicing is a cascade of up to **two** biquads. This conflicts with "one precomputed biquad per offset" in 5.2. Decision: allow up to two sections per offset; cost is still inside 0.01 units. Confirm.
- Coefficients: RBJ cookbook forms, `double`, computed at compile.
- Bound check at compile: evaluate `|H(f)|` at 20 Hz-20 kHz on a log grid (e.g. 512 points). If the peak exceeds 3 dB, the compile clamps the offset and logs a debug message. This makes CB-10 a property of the code path, not only of the table.
- Unmodelled: no biquads at all. The path is bit-exact.

### Pedal noise (5.3)

- One white source per engaged non-Unmodelled pedal: `seed ^ kSaltPedalNoise ^ instanceIndex`.
- One-pole highpass at 20 Hz: `a = exp(-2 pi 20 / fs)`, `y[n] = a (y[n-1] + x[n] - x[n-1])`, `double`.
- Level: RMS at the pedal output at unity gain, from table 5.1 (Economy -72 dBFS, Standard -84, Pro -92, Boutique -96).
- Drive pedals: add `0.5 * max(0, driveGainDb)` dB to the level (5.1 note).
- Scaling: the white source is normalised to unit RMS at compile, then multiplied by the level. The pedal noise is a drone and stays out of `NoiseEngine` (`pick-noise.md` 1).

### Input impedance and loading (4.1, 4.2)

- Table: Economy 100 kOhm, Standard 470 kOhm, Pro and Boutique 1 MOhm (D).
- The load at the end of the cable run is the first pedal on the walk (file 05) that is engaged (its tier impedance) or in buffered bypass (1 MOhm). A true-bypassed pedal is skipped. If the walk reaches the amp, `amp_input_impedance` applies.
- The derived load replaces the resistive load in `GuitarCircuit` at compile (CB-09). It does not change the volume-pot model.
- Active pickups (`circuit_active`) are buffered before the cable, so this load does not reach them (4.3).

### RT safety

- Biquad coefficients, noise normalisation, and the derived load are computed at compile.
- `process`: per pedal one biquad (or two), one HP, one RNG step per sample. State is kept across tier changes (file 04).
- No allocation. `reset()` clears biquad, HP and RNG-state history but keeps the seeded RNG position unchanged so a reset does not change the noise sequence. Decision: RNG position is part of the seed, not of the reset.
- Denormals handled by the `ScopedNoDenormals` scope (file 04).

## Data model and parameters

- No host parameter.
- Pedal state: `tier` (file 03), `bypassMode` (file 02).
- Compile outputs (not stored): per pedal `voicingCoeffs[2][5]`, `noiseLevelLin`, `inputImpedanceOhm`.
- Table constants: `PedalTiers.cpp`.

## State, file format and migration

- Migrated pedals: Unmodelled, so no state change and no render change (CB-M01).
- New pedals: Standard (file 03).
- No new keys beyond file 03.

## Edition gating

| Feature | Free | Pro |
|---|---|---|
| Pedal tier | Standard only (Unmodelled read-only for migrated) | all |
| Standard voicing and noise | yes | yes |
| Economy, Pro, Boutique | greyed "Pro" | yes |

## Performance budget

- Per pedal with a tier: 0.01 units (section 9): up to two biquads, one HP, one RNG step per sample.
- Compile: not in budget.

## Test plan

- **CB-09 (loading).** With a 3 m Standard cable and a passive pickup at volume 10, Economy's 100 kOhm load gives a level difference at 5 kHz of at least 1.5 dB relative to 1 MOhm.
- **CB-10 (voicing bound).** Every offset within +/-3 dB over 20 Hz-20 kHz (evaluated from the compiled coefficients). Unmodelled path is bit-exact 0 dB (bit-identical output to a no-tier pedal).
- **CB-11 (noise).**
  - Each tier's output RMS is within 2 dB of table 5.1 at unity gain.
  - Same seed gives identical renders (bit-identical).
  - A different seed gives the same RMS within 0.1 dB.
  - Drive: +0.5 dB noise per dB of drive gain, within 0.5 dB.
- Added: a true-bypassed pedal does not add its noise or voicing to the output.
- Added: the compile-time bound clamp fires for a deliberately out-of-bound offset in a unit test.

## Effort and dependencies

- ED 8.
- Depends on: `cables-and-brands.md` 4 and 5; `pedalboard-v2.md` 3.5 (bypass path); `character-wear.md` 1 (seed); `pick-noise.md` 1 (keep out of `NoiseEngine`); `volume-knob-interaction.md` (`GuitarCircuit` load input); file 05 (run walk); file 02 (pop at output); file 04 (RT).
