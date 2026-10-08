# Advanced canvas and library drawer

**IDs:** none directly (PB-U01 covers the gestures, file 11). **ED:** 7.5. **Status:** missing.

**Summary.** The BOARD tab in Advanced: a zoomable canvas of jacks, cables, lanes and the amp divider, with a library drawer that adds and inserts pedals.

## User-facing behaviour
- **Jacks.** Mono pedal: one in, one out. Stereo pedal: L and R on each side. Split: one in, two out. Merge: two in, one out. AMP IN: one mono jack. OUT L and OUT R: the stereo pair.
- **Cables.** Beziers from output to input, each with a tier badge (opens file 04).
- **Lanes.** Columns by depth. A layout hint only, not signal order (ground rule 1).
- **Amp divider.** Dashed line at the AMP IN lane. Left is pre-amp, right is post-amp. Not draggable. In-loop pedals sit on the loop segment (file 15).
- **Zoom.** Ctrl+wheel or pinch, 50-200%.
- **Library drawer,** grouped Dynamics, Drive (including Preamp Stage), Filter and pitch, Modulation, Time and space, EQ, Utility (Buffer, Ground Isolator). Click adds an instance. Drag onto a cable inserts it (file 11). Items locked by edition read "Pro".
- Adding an instance is an undoable edit.

## Engine and data model
- Canvas is a view over `BoardModel` (file 14); positions are `layout` data (x, y, lane) saved per node.
- Lane computation: depth from topological order, recalculated on structural edits only.
- Zoom is view state; it is not saved in the board block.
- Instance allocation for a new pedal: the lowest free instance in its range (0-15 first), per file 10.

## Parameters and data
None. Layout in the board block as `pos`.

## State and migration
Migrated boards get a generated layout (grid by lane). Positions never affect sound.

## Edition
Locked library items and locked settings show "Pro" (file 05).

## Performance budget
None at audio time. Paint cost is UI only; rebuild on structural edits.

## Tests
- Manual (PB-U01 gestures). Layout is covered by PB-P01's visible control check (file 03) for split sliders only.
- Automated where cheap: a canvas move leaves the `BoardProgram` hash unchanged.

## Effort and dependencies
ED 7.5 (largest UI item). Depends on 14 (model), 11 (gestures), 09 (face), 04 (cable popover), 05 (gating).
