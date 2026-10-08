# CB-01, CB-02: Tiers, cable model and resonance

**Items:** CB-01, CB-02
**ED:** 4
**Source:** `cables-and-brands.md` 1, 2, 4.1, 10 (CB-01, CB-02), 11

## Summary

Add the cable tier model (pF/m, shield, tier enum), derive the effective capacitance of a cable run, and feed it to the existing `GuitarCircuit` so resonance follows tier and length.

## Status

Partial. v1 has the input cable capacitance from `cable_quality` (160, 98, 52, 220 pF/m equivalents) and `cable_length`, consumed by `GuitarCircuit` (`volume-knob-interaction.md` 1.1). Missing: the tier enum shared by patch and input cables, patch-cable capacitance, the run rule below, and the CB-01 and CB-02 checks.

## User-facing behaviour

- At 0 m all tiers give the same resonance (9.2 kHz with the 3 H placeholder).
- With length, resonance falls faster for worse tiers. At 6 m: Boutique 5.2 kHz, Pro 4.5, Standard 3.5, Economy 2.8 kHz (unloaded, I values in table 1.1).
- A short patch cable costs nothing in tone. A long run through true-bypassed pedals costs the sum of its capacitance (below).

## Engine / DSP design

**Capacitance per cable.** `C_cable = lengthM * pFperM(tier)`, pF. `pFperM`: Economy 160, Standard 98, Pro 52, Boutique 35, Unmodelled 0 (v1 path). Input-cable Vintage uses 220 pF/m with the coiled flag (file 01).

**Effective run (design derivation, I).** The pickup sees the capacitance of every cable electrically in series with the pickup output before a point that presents a resistive load:

- Walk the chain from the input cable. Each patch cable adds `C_cable`.
- A pedal in `true` bypass is a wire, so the walk continues through it.
- A pedal in `soft` or `buffered` bypass, or engaged, presents its input impedance (file 09). The walk stops there.
- The amp (`amp_input_impedance`) ends the walk.

`C_run = C_input + sum(C_patch on the walk)`. The spec's section 4.1 implies this; it is written out here because the spec does not give the rule. Confirm before coding.

**Resonance.** The existing `GuitarCircuit` takes the cable capacitance as an input. Pass `C_run` in pF (converted to F with `1e-12`). The unloaded reference used by the tests:

```
f_res = 1 / (2 pi sqrt( L * (C_pickup + C_run) )),  L = 3 H (D), C_pickup = 100 pF (M)
```

**Patch-cable tone.** A patch cable at a pedal output sees the pedal's output impedance (1 kOhm, D). Its corner is `1 / (2 pi * 1k * C)`, about 330 kHz for 3 m Economy. So a patch cable has no audible tone effect on its own. Its audible effects are the walk above (through true-bypassed pedals), hum (file 08) and crackle (file 01). This is a design statement; the spec does not give a pedal output impedance, so 1 kOhm is a D value to add to section 12.

**Precision.** All capacitance and `f_res` math in `double`.

### RT safety

- `C_run` and the `GuitarCircuit` inputs are computed on the message thread at compile.
- `process` reads only the compiled circuit coefficients.
- `reset()` does not change capacitance.

## Data model and parameters

- No host parameter.
- Patch cable: `tier`, `lengthM` (0.3-10.0 m), as in file 03.
- Input cable: existing `cable_quality`, `cable_length` (`Parameters.h`).
- Table of pF/m per tier: a `constexpr` array in `CableTiers.h` (new), with the design reason in a comment.

## State, file format and migration

Covered by file 03. No new keys beyond `tier` and `lengthM`. v1 input cable is unchanged.

## Edition gating

| Feature | Free | Pro |
|---|---|---|
| Patch-cable tier | Standard | all |
| Input `cable_quality` | all four | all four |
| Run rule and resonance | all | all |

## Performance budget

Compile-time only. No per-sample cost beyond `GuitarCircuit`, which exists.

## Test plan

- **CB-01 (capacitance).**
  - Each cell of table 1.1 matches the formula within 2 %.
  - `GuitarCircuit` with a 100 pF pickup, no pots, and `C_run` set from the table matches the formula within 2 %.
- **CB-02 (ordering).**
  - At fixed length (e.g., 6 m), resonance falls strictly Boutique > Pro > Standard > Economy.
  - At fixed tier, resonance falls strictly with length.
- Added: run rule. A 3 m patch cable after a `true`-bypassed pedal adds its capacitance to `C_run`; after a `buffered` pedal it does not.
- Added: a 10 m Economy patch cable after a Buffer (file 06) leaves `C_run` at the input cable only.

## Effort and dependencies

- ED 4.
- Depends on: `volume-knob-interaction.md` 1.1 and `GuitarCircuit` (cross-check); `part-acoustics.md` (factory `Ls` to replace the 3 H placeholder, section 12 item 1); `pedalboard-v2.md` 3.5 (bypass modes, which decide the walk); files 03, 09.
