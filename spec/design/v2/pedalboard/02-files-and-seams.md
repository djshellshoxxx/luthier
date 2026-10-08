# Source layout and integration seams

**IDs:** none (enabling work for all PB items). **ED:** 0.5. **Status:** missing.

**Summary.** Create the new directories and mark the small edits outside them, so each later item edits only what it names.

## User-facing behaviour
None.

## Engine and data model
- New: `Source/Model/Board/*` (BoardModel, validation, serialisation), `Source/DSP/Board/*` (BoardCompiler, BoardProgram, buffer pool), `Source/UI/Board/*` (canvas, library, popovers, face).
- Marked seams (per `docs/HANDOFF.md`, each edit wrapped in a `// ==== V2-BOARD ====` marker):
  - `LuthierEngine.cpp` (processing calls, ~3459-3525).
  - `EffectsChain.*`: legacy adapter so v1 racks feed the board (file 16).
  - `Parameters.*`: appended block (files 03, 10).
  - `PresetManager.cpp`: board block read/write (file 13).
  - `PluginProcessor.cpp`: latency read only (file 01).
- Rule: no board logic in the seam files; they forward to `Source/DSP/Board`.

## Parameters and data
None.

## State and migration
None; the seams keep v1 behaviour when no board block exists.

## Edition
None.

## Performance budget
None.

## Tests
Build passes on Windows, macOS, Linux Standalone and plugin targets. No new behaviour tests; the existing suite stays green (covered by file 18).

## Effort and dependencies
ED 0.5. Blocks 13, 14, 16. Depends on 01 merged.
