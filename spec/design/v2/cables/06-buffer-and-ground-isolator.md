# CB-07: Buffer and Ground Isolator DSP

**Items:** CB-07 (the Ground Isolator's Lift and Isolate counts are tested under CB-05 in file 08)
**ED:** 4
**Source:** `cables-and-brands.md` 3.2, 3.3, 4.2, 10 (CB-07), 11; `pedalboard-v2.md` 4.1 (types 23, 24)

## Summary

Add two utility pedals: a unity Buffer (type 23) and a Ground Isolator (type 24) with Off, Lift and Isolate.

## Status

Missing. Types 23 and 24 are not in `Source/`.

## User-facing behaviour

- **Buffer:** unity gain, no tone change, no latency. Its input is high impedance, so a player can put it at the guitar end to stop a long cable from loading the pickup.
- **Ground Isolator:** a switch with Off, Lift and Isolate. Off is transparent. Lift removes one ground path and cuts shield-coupled hum by 15 dB. Isolate does what Lift does and also cuts the ground loop by 30 dB, at 0.5 dB insertion loss.

### Spec defect to resolve (CB-07)

CB-07 says "a 10 m Economy cable before a Buffer matches 0 m at 5 kHz". This cannot hold physically. The cable's capacitance is across the pickup, and a Buffer after the cable does not remove it. Measured: 10 m Economy (C = 1600 pF) with a 3 H placeholder gives about 2.2 kHz resonance whatever the load. Only moving the Buffer to the guitar end (short cable before it) gives the 0 m result.

Two fixes, to choose:
1. Reword CB-07 to "a 10 m Economy cable **after** a Buffer matches 0 m at 5 kHz within 0.5 dB". With the Buffer's 1 kOhm output driving 1600 pF the corner is about 100 kHz, so the 5 kHz error is about 0.01 dB. This is the correct test for the Buffer's output isolation.
2. Keep the wording and change the scenario to a Buffer at the guitar end with a 0.3 m cable, compared to the 0 m reference.

This file implements the physics. The test uses fix 1 unless the spec owner decides otherwise.

## Engine / DSP design

### Buffer (type 23)

- Gain: unity. Response within +/-0.05 dB over 20 Hz-20 kHz (no filter; identity path).
- Input impedance 1 MOhm, output impedance 1 kOhm (D). These are the loading and driving values used by files 05 and 09 in the run walk.
- Noise: -100 dBFS (D), white, seeded `seed ^ kSaltBuffer ^ instance`, summed at output. Same source type as file 09 (one-pole HP at 20 Hz).
- Latency: 0 samples.
- Bypass: always buffered by definition. The Buffer has no bypass state in the UI.

### Ground Isolator (type 24)

Parameter `lift` in {off, lift, isolate}.

- **Off:** identity. Bit-exact pass-through.
- **Lift:** 0 dB insertion loss (the spec gives loss only for Isolate). Removes one path from `G` (file 08) and attenuates the board's shield-coupled cable hum by 15 dB (D). Design decision: the spec gives a single 15 dB number without position, so it applies to the total cable-hum sum, not per cable.
- **Isolate:** same as Lift, plus:
  - the ground-loop component (file 08) is reduced by 30 dB (D);
  - insertion loss 0.5 dB (D), flat within 0.2 dB from 20 Hz to 20 kHz.

Implementation of Isolate's loss: one fixed biquad, precomputed on compile (`performance-budget` lists it as a fixed residual filter), double precision, with a gain of -0.5 dB at DC-to-20 kHz and a flatness within 0.2 dB. Coefficients come from a design of a flat shelf with the required gain; a unit test checks the response, not the formula.

### RT safety

- Coefficients computed on the message thread at compile (ground rule 5).
- Buffer: no state. Isolator: one biquad, state kept across lift changes. A switch from Off to Isolate ramps the gain over 20 ms; the biquad is always in the path when the Isolator is in the chain, with its state kept.
- No allocation in `process`. `reset()` clears the biquad state only.

## Data model and parameters

- Buffer: no parameters.
- Ground Isolator: `lift` (string enum off, lift, isolate; default off), stored in its pedal state (section 6.1).
- Pedal type IDs 23 and 24 from `pedalboard-v2.md` 4.1.

## State, file format and migration

- New pedal types, so only new boards contain them (section 7). A v1 preset never contains them.
- Old readers that do not know types 23-24 are not supported; the schema bump (file 03) covers it.

## Edition gating

| Feature | Free | Pro |
|---|---|---|
| Buffer | no | yes |
| Ground Isolator | no | yes |

A Pro board containing them, loaded in Free, is handled by file 03 (transparent, locked).

## Performance budget

- Buffer: 0.01 units (section 9).
- Ground Isolator: 0.02 units (one biquad in Isolate, identity when Off).

## Test plan

- **CB-07 (buffer isolates).** Per the fix chosen above (default: 10 m Economy after a Buffer matches 0 m at 5 kHz within 0.5 dB). Also: the same cable before a Buffer with a 10 m run is not claimed to match.
- Added: Buffer response within +/-0.05 dB over 20 Hz-20 kHz; latency 0; Buffer output impedance measured as 1 kOhm within 5 %.
- Added: Ground Isolator Off is bit-exact to the input.
- Added: Isolate insertion loss -0.5 dB within 0.2 dB, flat within 0.2 dB from 20 Hz to 20 kHz.
- Lift and Isolate hum and loop counts are asserted under CB-05 in file 08.

## Effort and dependencies

- ED 4.
- Depends on: `pedalboard-v2.md` 3.3, 3.5, 4.1 (types 23, 24); file 08 (G, hum); file 05 (run walk); file 09 (noise source type); `character-wear.md` 1 (seed).
