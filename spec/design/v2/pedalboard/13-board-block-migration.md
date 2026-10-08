# Board block, file format and v1 migration

**IDs:** PB-M01 (migration), PB-M02 (round trip), PB-M03 (forward). **ED:** 5.0. **Status:** missing (v1 presets exist; no board block).

**Summary.** Add an optional `board` block to presets. A v1 preset with no block migrates to a board that renders bit-identically.

## User-facing behaviour
- Opening a v1 preset shows it as a board with the same sound.
- Saving a v1 preset without edits writes it byte-identical to v1.
- Saving a board with v2-only features writes the block. A v1 reader loads the pedals it knows and shows "This preset uses board features."

## Engine and data model
- `BoardModel` (de)serialisation in `Source/Model/Board`, called from `PresetManager.cpp`.
- Migration (`5.2`): (1) pre slots 1-8 in order, empty slots dropped, instance = slot - 1, soft, Unmodelled; (2) Source, pre pedals, `fx_saturation` (a fixed stage, not a board node), AmpIn; (3) the amp, post slots 1-8 (instance = slot - 1 + 8), Sink; (4) one cable per adjacency, Unmodelled. `cable_quality` and `cable_length` stay on the input cable.
- Unmodelled applies no voicing, noise or loading; Soft is the v1 crossfade. This is why the output matches.
- Board preset (`5.3`): a preset whose board block is the whole board; tagged "Board" in the browser (`preset-browser-previews.md`).
- Writing rule: a linear board with only v1 features is written in v1 format.

## Parameters and data
Board block JSON (roadmap 5.1). Instances 0-15 keep values in `preN_`/`postN_` keys. Instances 16-23 keep ten normalised values in `pedals[i].params`. `board.version` = 1 for this format.

## State and migration
- PB-M02: a v1 preset saved without edits is byte-identical.
- PB-M03: a preset with `board.version` 2 loads the v1 racks with the notice "This preset uses board features."
- Migration runs once on load; the migrated board is in memory and only written back on an explicit save.

## Edition
Board save is Pro (file 05). Free can load boards and v1 presets.

## Performance budget
Load-time only. Migration cost is on the message thread.

## Tests
- **PB-M01.** Every factory preset renders bit-identically via the v1 and migrated paths at 44.1, 48 and 96 kHz.
- **PB-M02.** Round trip without edits is byte-identical.
- **PB-M03.** Forward version handled with the notice.

## Effort and dependencies
ED 5.0. Depends on 14 (model), 16 (legacy adapter in `EffectsChain`), 10 (instance mapping).
