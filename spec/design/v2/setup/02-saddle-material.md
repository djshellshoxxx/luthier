# 02 Saddle material

**IDs:** SU-06. **ED:** 3. **Status:** partial (saddle is a bridge part and a piezo source, `PartAcoustics.cpp` 648; no acoustic saddle coefficient).

**Summary.** The saddle's material changes the bridge termination's reflection, so sustain and attack change. Pitch does not change.

## 1. User-facing behaviour

- Setup panel: Saddle, one of bone, brass, graphite, Tusq, steel.
- Changing it changes sustain and attack only. Tuning is unchanged.
- Free edition: bone only. The other four show a Pro badge.

## 2. Engine and model

Table (D until measured, `part-acoustics.md` 115 shape):

| Material | `m_saddle` |
|---|---|
| bone | 0.74 |
| brass | 0.84 |
| graphite | 0.70 |
| Tusq | 0.72 |
| steel | 0.90 |

The factor is applied to the **bridge termination reflection only**, not to the nut, not to string length or tension:

```
r_bridge = clamp( r_bridge_base x (m_saddle / m_bone), 0, 0.999 )
```

Normalising by `m_bone` makes bone a factor of 1, so the v1 render is unchanged. The clamp prevents a reflection above 1 (steel on a high base value would otherwise exceed it).

Pitch effect: none. The factor does not enter the length or tension path, so the cents difference is 0.

Precompute: `r_bridge` is recomputed on the message thread when `saddle_material` changes and stored as a `double`. The audio thread reads it at the next block boundary. No allocation.

## 3. Data model and parameters

- `saddle_material`: choice (state), bone | brass | graphite | Tusq | steel, default bone. Not automatable.
- Table constants live in `Source/Model/Setup/SetupState.cpp`.

## 4. State, file format, migration

- Setup block field `saddle` (string: `bone`, `brass`, `graphite`, `tusq`, `steel`). Missing means bone.
- v1 presets load as bone, factor 1, bit-identical (SU-M01).

## 5. Edition

- Free: bone.
- Pro: all five.

## 6. Performance budget

- One multiply per block folded into the existing termination coefficient, or zero if precomputed. Under 0.001 units.
- Double precision.
- `reset()`: keeps the stored material; the coefficient is recomputed from it.

## 7. Test plan

- **SU-06.** Brass versus bone, same note, same pick: 2-second decay differs by **at least 0.2 s**; the pitch difference is **0 cents** (same FFT frame, peak bin interpolated). The 0.2 s threshold is D. Re-derive it from the normalised mapping after the first render, and record the measured value in the test comment.
- Bone render is bit-identical to a render with no setup block (SU-M01 subset).
- Free edition rejects a non-bone saddle at the setter.

## 8. Effort and dependencies

- ED 3 (roadmap 9).
- Depends on: `part-acoustics.md` 115 (table shape) and the bridge termination in `part-acoustics.md`; `PartAcoustics.cpp` 648.

## 9. Open

- Values are D. Source them from the research file for bridge parts, or drop the saddle option in v2.0 (roadmap 10.2).
