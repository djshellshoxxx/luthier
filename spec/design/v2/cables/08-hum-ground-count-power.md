# CB-03, CB-04, CB-05, CB-12: Hum, ground count and power mode

**Items:** CB-03, CB-04, CB-05, CB-12
**ED:** 6
**Source:** `cables-and-brands.md` 2, 3.1, 3.2, 6.2, 10, 11; `noise-floor.md` 2.5

## Summary

Make cable hum a function of length, shield attenuation and balance; make the ground-loop level a function of the number of powered grounded units `G`; and add Daisy and Isolated power modes that set how pedals add mains hum.

## Status

Partial. `noise_ground_loop` (one level), `noise_amp_buzz` (pickup hum), `noise_mains_hz` and the rectifier wavetable (`noise-floor.md` 2.5) exist. Missing: per-cable hum, `G`, power mode, the balanced link, and every check in this file.

## User-facing behaviour

- A long unbalanced cheap cable hums audibly. A balanced link or a better shield reduces it by the amounts in the table.
- Adding pedals to a Daisy board raises the loop (3 dB per extra unit, to +9 dB). Isolated power does not.
- Power mode is a board setting: Daisy (one shared supply) or Isolated (each pedal has its own supply).
- The player can cure the loop with the Ground Isolator (file 06) or by switching to Isolated power.

## Engine / DSP design

### Cable hum (3.1)

For each patch cable and the input cable with `hum on`:

```
hum_rel_dB = HUM_BASE - (shieldAtten - 20) + 20 log10(lengthM / 3) + (balanced ? -20 : 0)
HUM_BASE   = -18 dB (D)
humLin     = noise_amp_buzz * 10^(hum_rel_dB / 20)
```

- `shieldAtten` per tier (section 2): Economy 20, Standard 30, Pro 40, Boutique 45 dB.
- Waveform: the rectifier wavetable from `noise-floor.md` 2.5 (50/60 Hz line plus harmonics 2-5), read at this cable's rate. The line frequency is `noise_mains_hz`.
- Phase: each cable has a `double` phase accumulator, initial phase from `seed ^ kSaltHum ^ cableIndex`. Phases are not reset on a tier change, so a change does not click.
- Summation: sources are summed in power (uncorrelated paths). Design decision, flagged: the spec does not specify coherent or incoherent sum.
- Gain changes ramp over 20 ms.

### Ground loop (3.2)

```
G       = number of powered, grounded units on an unbalanced shield path
loopDb  = min( 3 * max(0, G - 1), 9 )         // D: +3 dB per extra path, cap +9 dB
loopLin = noise_ground_loop * 10^(loopDb / 20)
```

- A balanced link removes that link from `G`.
- Ground Isolator Lift removes one path from `G` (floor 1). Isolate removes the loop component and applies -30 dB residual (D) (file 06).
- The loop is independent of pickup and volume (`noise-floor.md` 2.5). Only `G` changes it.
- `G` is recomputed at compile. `process` reads `loopLin`.

### Power mode (6.2)

- **Daisy:** each pedal adds a mains line at -60 dBFS (D) and raises `G` by one.
- **Isolated:** each pedal adds a mains line at -86 dBFS (D). `G` does not rise with pedal count.
- **Mixed:** not shipped (section 12 item 6).
- Mains lines use `noise_mains_hz` and the same wavetable as cable hum, each with its own phase, power-summed with the cable hum.
- Pedal mains lines are part of the per-pedal budget (section 9).

### RT safety

- Compile computes `humLin` per cable, `loopLin`, and the mains level per pedal as `double`.
- `process` only advances phases and multiplies by the ramped gain.
- No allocation. `reset()` keeps phases (so reset does not restart the hum) and clears only smoothing state.

## Data model and parameters

- No host parameter. `noise_ground_loop`, `noise_amp_buzz`, `noise_mains_hz` are existing.
- Patch cable: `balanced` (bool), `tier` (shared with file 03), `lengthM`.
- Board: `powerMode` (daisy, isolated, mixed) in the board block (file 03).
- `G`, `loopDb`, `humLin`: derived, not stored.

## State, file format and migration

- v1 presets: `powerMode = daisy`, zero patch cables, so the only hum is the input cable. Section 7 states this renders the same when the hum sources are zero. Input-cable hum is included only in v2 (see open point below).
- Open point: the input cable's hum is new. A v1 preset with `noise_ground_loop > 0` would change if `G` is now computed from pedals. Proposed: a v1 board has `G = 1` (one powered unit) and no loop change until a pedal is added, so v1 renders the same. Confirm.

## Edition gating

| Feature | Free | Pro |
|---|---|---|
| Power mode | Daisy only | Daisy, Isolated, Mixed later |
| Cable hum and balanced | calculated for all tiers | all |
| Ground Isolator | no | yes |

Balanced and length are not gated by section 8. Proposed: not gated. Confirm.

## Performance budget

- Cable hum: 0.005 units per cable with hum on; 24 cables = 0.12 (section 9).
- Per pedal with a tier: 0.01 units, which includes its mains line.
- Compile cost is not in the budget (message thread).

## Test plan

- **CB-03 (hum table).** Every row of table 3.1 holds within 1 dB, both as formula evaluation and as a render (pickup hum set to 0 except the cable's, measured RMS relative to `noise_amp_buzz`).
- **CB-04 (balanced).** A balanced link reduces hum by 20 dB within 1 dB, for the 10 m Economy case.
- **CB-05 (ground count).**
  - Each added unbalanced powered unit adds 3 dB, to +9 dB (G = 1 gives 0 dB; G = 4 and above give 9 dB).
  - Lift removes one path (`G` decreases by one; floor at 1).
  - Isolate reduces the loop component by 30 dB within 1 dB.
- **CB-12 (power).** Daisy and Isolated differ in the mains-line level by at least 20 dB for the same pedal count (expected 26 dB).
- Added: power sum: two identical cable hum sources give +3 dB, not +6 dB.
- Added: phase continuity across a tier change (no sample discontinuity above the 20 ms ramp bound).

## Effort and dependencies

- ED 6.
- Depends on: `noise-floor.md` 2.5 (wavetable, ground loop, changes additive); `cables-and-brands.md` 6.1 (existing params); `character-wear.md` 1 (seed); file 06 (Lift and Isolate); file 03 (power mode field); file 04 (RT probe).
