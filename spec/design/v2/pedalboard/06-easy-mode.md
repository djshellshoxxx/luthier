# Easy Mode board strip

**IDs:** PB-U02 (manual); notices pinned by PB-13 (file 14). **ED:** 2.5. **Status:** missing.

**Summary.** A simplified board: one row per pedal in graph order, with bypass, mix, and split levels. No cables.

## User-facing behaviour
- One row per pedal in topological order. Splits indent their branches.
- Row: name, footswitch LED, bypass button (tooltip: "Latency does not change."), mix knob. Split rows add level A and B sliders (file 03).
- Cables are hidden. Anything audible that Easy cannot show gets a summary line, e.g. "Split: two paths merged before AMP IN."
- Notices, one line each: empty board, dead end, unmerged split, over budget (file 14 validation, PB-13). Over budget is amber.
- Standalone uses the same panel.

## Engine and data model
- Pure view over `BoardModel` (file 14): the row list is derived from the topological order. No DSP state.
- Bypass and mix are the same parameters/state as Advanced (files 09, 12). Easy never owns separate state.
- Summary lines are generated from the graph by a function in `Source/UI/Board` so PB-U02 can assert them.

## Parameters and data
Uses existing pedal state and file 03 split levels. No new IDs.

## State and migration
None. A migrated board shows one row per v1 slot, pre rows then the amp row, then post rows.

## Edition
Free rows for locked pedals show "Pro" text (file 05). Easy has no separate gating.

## Performance budget
None (UI only). Rows are rebuilt on structural edits, not per block.

## Tests
- **PB-U02 (manual).** A two-path board shows its Easy summary line.
- **PB-13 (via file 14).** Each notice fires on a constructed board.

## Effort and dependencies
ED 2.5. Depends on 14 (model and validation), 09 (face controls), 03 (split sliders).
