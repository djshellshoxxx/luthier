# Compile, swap and retire

**IDs:** PB-04 (determinism), PB-05 (no allocation), PB-06 (click-free swap). **ED:** 7.0. **Status:** partial (`EffectsChain::applyPendingSwaps` try-lock swap and `retired` list exist for v1; no board compiler).

**Summary.** Compile a `BoardModel` into a flat `BoardProgram` on the message thread, swap it at a block boundary, and free old pedals off the audio thread.

## User-facing behaviour
- Structural edits (add, remove, re-wire, retype, bypass mode change, cable tier/length change, pop change) take effect at the next block with no dropout.
- Deleting a pedal fades it out over 50 ms before it leaves the program, so no click.
- Level and mix changes are parameter changes and do not recompile.

## Engine and data model
1. **Edit** (message thread) on `BoardModel`, validated (file 14).
2. **Compile** (message thread). `BoardCompiler` topologically sorts the graph, assigns buffers from a pool sized `maxDepth x maxWidth x 2` channels, and emits `BoardProgram`: a flat op array of `RunPedal`, `Copy`, `Sum`, `Gain`, `Sink` (plus LoopSend/LoopReturn, file 15). Surviving instances are reused (same `Pedal` object, state kept). Tier and bypass filter coefficients are computed here, never in `process`.
3. **Hand over.** Publish via the same try-lock swap `EffectsChain::applyPendingSwaps` uses; the audio thread adopts the new program at the next block boundary.
4. **Retire.** Removed pedals go to the existing `retired` list, freed off the audio thread.
- `BoardProgram::process` loops over the fixed op array. No `new`, no `delete`, no lock, no `std::function`, no `std::vector` growth.
- Pool bounds: 24 pedals bound depth, 8 bound width.
- **Determinism (PB-04):** compile is a pure function of `BoardModel` (stable topo sort with instance-id tiebreak). Two compiles give identical ops and samples.
- **Click-free (PB-06):** the swap crossfades old and new program outputs over 50 ms for any removed or re-routed path; surviving pedals keep state.
- Mode changes that remove a pedal from the path (True, Buffered) use the same 50 ms path fade.

## Parameters and data
None.

## State and migration
`BoardProgram` is runtime only; never saved. The legacy adapter in `EffectsChain` builds a program from v1 racks so v1 presets use this path too (required for PB-M01).

## Edition
Compile is not gated; the validator is (file 14).

## Performance budget
Routing: 0.005 units per op; 40 ops add about 0.2. Board budget (pedals plus routing) capped at 6 units; over budget still plays with an amber notice.

## Tests
- **PB-04.** Two compiles of one model give identical ops and output samples.
- **PB-05.** 30 s of 50 random edits under audio: zero allocations in `BoardProgram::process` (allocation hook in the test build).
- **PB-06.** Largest sample step during a mid-note edit stays within 1.5 x the steady-state maximum.

## Effort and dependencies
ED 7.0 (largest engine item). Depends on 14 (model, validator), 02 (seams). Blocks 04, 07, 08 (ops), 09, 11, 12, 13 (migration path), 15.
