# Drag, drop and keyboard editing

**IDs:** PB-U01 (manual). **ED:** 4.0. **Status:** missing.

**Summary.** Every canvas gesture has a keyboard path. Every edit is one undo entry.

## User-facing behaviour
- Drag a pedal to move it: no sound change (ground rule 1).
- Drag a pedal onto a cable to insert it (the cable splits into two, with the pedal between).
- Drag a cable end to empty canvas to delete the cable.
- Jack to jack draws a cable. Output to output is refused, with a short message.
- Keyboard: Tab walks pedals in graph order; arrows move a pedal one lane; Enter opens the face (file 09); Ctrl+arrow moves a cable end to another jack; Space toggles bypass.
- Insert, delete and move each have a keyboard equivalent (menu action or the same keys).

## Engine and data model
- Gestures produce `BoardModel` edits (structural, file 16) or view changes (position only, no recompile). Position is layout state (file 17), not signal order.
- Refusals (output to output, cycle, Free-locked) are decided by the validator (file 14), not the UI.
- Each completed gesture is one undo transaction (`action-and-undo.md` 3.13). A cancelled drag makes no entry.
- Focus model: one focused element (pedal, jack, or cable). Tab order follows topological order from file 14.

## Parameters and data
None.

## State and migration
Move positions are saved in the board block as `pos` (layout only). Migrated boards get a generated grid layout.

## Edition
Locked actions show "Pro" and are unavailable (file 05).

## Performance budget
None (UI).

## Tests
- **PB-U01 (manual).** Every drag in section 2.5 completes by keyboard on Windows, macOS and Linux Standalone.
- Automated where cheap: a move produces no `BoardModel` signal change (hash of `BoardProgram` unchanged).

## Effort and dependencies
ED 4.0. Depends on 17 (canvas), 14 (validator), 04 (cable popover is the keyboard alternative for cable fields), 09 (face).
