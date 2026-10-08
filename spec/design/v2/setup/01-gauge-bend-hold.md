# 01 Gauge and bend hold

**IDs:** SU-04, SU-05 (roadmap 3.3). **ED:** 3. **Status:** partial.

**Summary.** Add a tension factor to the friction bend residual so a heavier set reads as holding a bend better, and show the result as "Bend hold: good / fair / poor".

**Current state.** Tension `T = (2 L f)^2 mu` exists (`part-acoustics.md` 95). The friction residual `stuck = 0.03 x mu_friction x B` exists (`tuning-stability.md` 2.2). No gauge term connects them.

## 1. User-facing behaviour

- The bend card shows one word for the current gauge, bend size and nut: good, fair or poor. No number is shown.
- Changing `string_gauge` updates the word at once.
- Band thresholds live in the bend card's constants. They are not player-editable in v2.0.

## 2. Engine and model

```
stuck  = 0.03 x mu_friction x B                       (tuning-stability 2.2, cents)
stuck' = stuck x (T_ref / T)^0.5
T      = (2 L f)^2 mu     at the same pitch f and scale L
T_ref  = tension of 10-46 on the same scale and pitch
```

Since `T` is proportional to `mu` at fixed pitch, and `mu` is taken as proportional to `d^2` (plain-string approximation, wound-string mass ignored), the exponent 0.5 gives:

| Set vs 10-46 | Sum of d^2 (1e-6 in^2) | Tension ratio | Residual factor |
|---|---|---|---|
| 9-42 | 3822 vs 4646 | 0.823 | 1.10 |
| 9-42, top string only (9 vs 10) | 81 vs 100 | 0.81 | 1.11 |
| 11-49 | 5688 vs 4646 | 1.224 | 0.90 |

Errata: the roadmap gives 1.2 for 9-42. The computed value is 1.10 to 1.11. The exponent 0.5 is kept as the physical `T^-1/2` form (D). The SU-04 band is set from the computed values, not the roadmap's.

Band rule (D, tune on measurement): residual in cents, good below 2 ct, fair from 2 to 5 ct, poor above 5 ct.

Precompute: the gauge factor `(T_ref/T)^0.5` is computed once per gauge change on the message thread and cached as a `double`. The bend path multiplies by it.

## 3. Data model and parameters

- No new automatable parameter. The gauge term is a formula.
- Constants: `kBendHoldRefGauge = 10-46`, exponent `0.5`, band thresholds `2 ct`, `5 ct`.
- Reads existing `string_gauge`.

## 4. State, file format, migration

- None. The band is derived and never saved.
- v1 presets are unchanged. No setup block field is needed.

## 5. Edition

Free and Pro (both). Matches the roadmap table in section 6.

## 6. Performance budget

- One `pow` per gauge change on the message thread. Zero per block.
- Bend path: one multiply per bend event on the audio thread. Under 0.001 units.
- Double precision for the residual and the factor.
- `reset()`: clears the cached band text, not the factor.
- No allocation on the audio thread.

## 7. Test plan

- **SU-04.** Same scale (648 mm), same bend (100 ct), same nut (`mu_friction` 0.75, bone). Residual ratio for 9-42 against 10-46 lies in **[1.08, 1.14]**. For 11-49 it lies in **[0.87, 0.93]**. (Roadmap band 1.15 to 1.25 is not reproduced by the computed factor.)
- **SU-05.** The word changes only at the threshold values. The card contains no numeric residual (assert the card's text has no digits).
- Gauge change invalidates the cache: after changing `string_gauge`, the factor equals a fresh computation within 1e-12.

## 8. Effort and dependencies

- ED 3 (roadmap 9).
- Depends on: `tuning-stability.md` 2.2 (friction residual), `part-acoustics.md` 95 (tension), `string_gauge`.

## 9. Open

- Exponent 0.5 and the band thresholds are D. Measure bend hold on two gauges of one guitar before the word ships (roadmap 10.1).
