# Split level parameters (PB-P01)

**IDs:** PB-P01. **ED:** 1.0. **Status:** missing.

**Summary.** Append eight automatable split-level parameters and keep the host layout test green at 216 parameters.

## User-facing behaviour
- Each split has an A level and a B level, -60 to 0 dB, default 0 dB, shown as a slider on its board row in Easy and Advanced (file 06).
- Automatable and MIDI-learnable like any v1 parameter.

## Engine and data model
- Index `k` is the split's creation order (1..4). Deleting a split leaves its IDs in place; the levels are then unused and ignored by the compiler.
- Levels apply as a gain on the split's output copy (file 14), smoothed with the existing parameter smoother.
- Parameters are read by the compiler's gain ops; a level change is a parameter change, not a structural edit (no recompile).

## Parameters and data
Inside `// ==== BEGIN V2-BOARD params ====` / `// ==== END V2-BOARD params ====`:

| ID | Range | Default | Automatable |
|---|---|---|---|
| `board_split{k}_level_a` (k = 1..4) | -60 to 0 dB | 0 dB | yes |
| `board_split{k}_level_b` (k = 1..4) | -60 to 0 dB | 0 dB | yes |

Eight IDs. Host count goes 208 -> 216 (the 208 v1 IDs are frozen). Update the literal sum in `Source/Tests/IntegrationTests.cpp` per `docs/HANDOFF.md`.

## State and migration
Levels are host parameters, so they live in the normal parameter state. v1 presets have no splits and load unchanged. A v1 reader ignores the new IDs.

## Edition
Free: no splits (file 05), so the parameters are hidden and not automatable there. Pro: all four.

## Performance budget
Negligible (smoothed gain per split).

## Tests
- **PB-P01.** Host count is 216. `everyAutomatableParameterHasAVisibleControl` passes: each new ID has a row slider in Easy and Advanced.

## Effort and dependencies
ED 1.0. Depends on 14 (split node) and 13 (state).
