# ACTION AND UNDO SPEC

The taxonomy of what counts as one undo action, what pushes a state
boundary, what groups within 200 ms, and what is never undoable.
Without this, every drag becomes 40 undo entries and Ctrl-Z becomes
unusable.

## 0. Ground rules

1. **One user-intended change equals one undo entry.** A drag from A
   to B is one entry, not 40 intermediate values.
2. **State boundaries divide the undo stack.** Preset load, tune load,
   guitar load, family switch: each pushes a boundary. Undo across a
   boundary requires Shift-Ctrl-Z (gui-integration.md 18).
3. **Grouping window is 200 ms.** Identical action classes on the same
   target within 200 ms merge into a single entry.
4. **Some actions are never undoable.** Live audio events, live MIDI
   events, session recorder writes, error banner dismissals.
5. **Undo doesn't cross the instance boundary.** Each plugin instance
   has its own undo stack. Two instances editing the same file both
   have their own histories.
6. **Undo doesn't persist across sessions.** The stack is session
   state (state-model.md 1); it clears when the plugin instance closes.

## 1. Undo entry shape

Every entry captures:
- **Action class**: knob-move, part-swap, mod-route-create, snapshot-
  save, etc. (enumerated in section 3).
- **Target**: parameter ID, part slot, snapshot index, mod route ID.
- **Before / after state**: enough to reverse and reapply.
- **Timestamp**: for grouping.
- **Description**: user-facing text for the undo dropdown ("Move
  Bridge Pickup", "Change Body to Alder Double-Cut", "Add LFO to Amp
  Gain").
- **Boundary flag**: true for entries that push a state boundary.

The undo dropdown (View menu -> Undo History) shows a scrollable
list of these descriptions.

## 2. Stack limits

- Max entries per stack: **200**.
- On overflow, oldest entries drop.
- No time limit on entries; only count.
- Redo stack cleared whenever a new action is pushed.

## 3. Action classes

Enumerated with their grouping behaviour.

### 3.1 Parameter changes (knob-move, slider, drag on illustration)

- **Grouping**: same parameter within 200 ms merges. The entry's
  before-value is the value at the start of the drag; the after-value
  is the value at the end of the last group-eligible movement.
- **Boundary**: no.
- **Description**: "Change [param name] from X to Y".
- **Special cases**:
  - A drag that pauses > 200 ms then resumes counts as two separate
    entries (this is often what the user wants: undo just the
    resumed portion).
  - Modulation-caused movement never generates undo entries;
    modulation is not user intent.
  - Automation-caused movement never generates undo entries; the host
    owns the automation lane.
  - MIDI-CC-caused movement never generates undo entries.

### 3.2 Discrete parameter switches (amp model, pickup selection)

- **Grouping**: switches to the same parameter within 200 ms merge to
  the last final value. A rapid scroll through amp models becomes one
  entry showing the initial and final choice.
- **Boundary**: no.

### 3.3 Toggles (bypass, feedback on, slide on, easy/adv mode switch)

- **Grouping**: no (toggles are discrete events; usually a user
  toggles once).
- **Boundary**: no.
- **Description**: "Turn on [feature]" / "Turn off [feature]".

### 3.4 Part swaps (Workshop)

- **Grouping**: no. Each swap is its own entry.
- **Boundary**: no.
- **Description**: "Change [slot] to [new part name]".
- **Exception**: a family switch is a state boundary (section 5).

### 3.5 Direct-manipulation drags on the illustration

- **Grouping**: yes, same handle within 200 ms.
- **Boundary**: no.
- **Description**: "Move [handle] from X mm to Y mm".

### 3.6 Mod-matrix edits

- **Create route**: entry class `mod-route-create`. Description "Add
  [source] to [destination] depth X".
- **Delete route**: entry class `mod-route-delete`. Description
  "Remove [source] from [destination]".
- **Change route depth / offset / curve**: entry class
  `mod-route-edit`. Grouping: same route within 200 ms merges.
- **Change source parameters (LFO rate, envelope times)**: entry
  class `mod-source-edit`. Grouping: same source within 200 ms
  merges.
- **Boundary**: no.

### 3.7 Snapshot operations

- **Save snapshot**: entry class `snapshot-save`. Description "Save
  snapshot [index] [name]". Grouping: no.
- **Recall snapshot**: NOT undoable via the parameter path (recall
  pushes an entry, but the entry represents the state change; undo
  reverses to the pre-recall state). Description "Recall snapshot
  [index] [name]".
- **Rename snapshot**: entry class `snapshot-rename`. Grouping: yes
  (typing).
- **Change snapshot colour**: entry class `snapshot-color`. Grouping:
  no.
- **Delete snapshot**: entry class `snapshot-delete`. Description
  "Delete snapshot [index]".
- **Rearrange snapshot**: entry class `snapshot-move`. Description
  "Move snapshot [old index] to [new index]".

### 3.8 Preset operations

- **Load preset**: **state boundary**. Description "Load preset
  [name]".
- **Save preset**: not on the undo stack (saving a file is not undoable;
  the file exists).
- **Rename preset in browser**: not on the plugin's undo stack (this
  is a file-system operation).

### 3.9 Tune operations

- **Load tune**: **state boundary**.
- **Play tune**: not undoable.
- **Edit tune section (add/remove/reorder)**: entry class
  `tune-section-edit`. Grouping: no.
- **Edit chord in section**: entry class `tune-chord-edit`. Grouping:
  yes, same chord cell within 200 ms.
- **Edit melody note (draw mode)**: entry class `tune-melody-edit`.
  Grouping: yes, same note within 200 ms.
- **Record melody take (any of the 5 methods)**: entry class
  `tune-melody-record`. Description "Record melody [method]".
  Grouping: no. Boundary: no.
- **Generate melody (auto)**: entry class `tune-melody-generate`.
  Description "Generate melody with [constraints]".

### 3.10 Setlist operations

- **Load setlist**: entry class `setlist-load`. Boundary: no (setlist
  is passive; loading it doesn't change audio).
- **Advance / retreat setlist step**: entry class `setlist-step`. This
  triggers a preset load which is a boundary; the setlist step is the
  boundary's driver.
- **Edit setlist (add/remove/reorder entries)**: entry class
  `setlist-edit`. Grouping: no.

### 3.11 Ranges toggle (advanced-ranges.md)

- **Toggle a family or per-control lock**: entry class
  `ranges-toggle`. Grouping: same target within 200 ms.
- **Boundary**: no.
- **Description**: "Unlock [family/control] range" / "Lock [family/
  control] range".

### 3.12 MIDI Learn

- **Learn a mapping**: entry class `midi-learn`. Grouping: no.
- **Delete a mapping**: entry class `midi-mapping-delete`.
- **Boundary**: no.

### 3.13 Effects rack operations

- **Add pedal**: entry class `pedal-add`.
- **Remove pedal**: entry class `pedal-remove`.
- **Reorder pedal**: entry class `pedal-move`.
- **Change pedal parameter**: entry class `pedal-param`. Grouping:
  same param within 200 ms.
- **Bypass pedal**: entry class `pedal-bypass`.
- **Boundary**: no.

### 3.14 Practice tools

- **Change scale trainer scale/key**: entry class `practice-scale`.
  Grouping: yes.
- **Change loop layer settings**: entry class `looper-layer`.
  Grouping: yes.
- **Recording a loop layer**: **not undoable** (audio is
  destructive). Deletion of a layer is undoable via a separate
  restore mechanism.
- **Load backing track**: entry class `practice-track-load`.

### 3.15 Character controls (character-wear.md)

- **Change a character control**: entry class `character-edit`.
  Grouping: yes.
- **Boundary**: no.

### 3.16 Guitar / part edits (Workshop)

Covered in 3.4 (part swaps) and 3.5 (direct manipulation).

### 3.17 UI state changes (uiState VT)

- **Change tab**: not undoable (view state is not a value change).
- **Open / close panel**: not undoable.
- **Toggle Easy / Advanced Mode**: not undoable.
- **Toggle Live Mode**: not undoable.
- **Open Workshop**: not undoable.
- **Change theme / palette**: not undoable (it's an option, not a
  value; use Options to revert).

## 4. Grouping rules in detail

Two entries A and B merge if all of the following hold:
- A and B have the same action class.
- A and B target the same specific target (same parameter ID, same
  part slot, same route ID, same snapshot index, etc.).
- B occurs within 200 ms of A's most recent contribution.
- A and B are not separated by any other entry (grouping is only
  merges of adjacent same-class same-target entries).
- Neither A nor B is a state boundary.

Merged entry:
- Description carries A's before-value and B's after-value.
- Timestamp is B's (the latest).
- Undo reverses to A's before-value.

If a new class of entry (a different target, or a different action)
occurs between two candidate-merges, the merge does not happen (and
future same-class same-target entries start their own grouping
window).

## 5. State boundaries

A boundary is an undo entry with `boundary_flag: true`. Boundaries:
- Preset load.
- Tune load.
- Guitar load.
- Family switch (which is a special guitar load).
- Setlist load's implicit preset load.

Behaviour at a boundary:
- Ctrl-Z stops at the boundary. The undo entry for the boundary itself
  can be undone (reverses the load), but undo won't automatically
  cross to entries older than the boundary.
- Shift-Ctrl-Z crosses the boundary explicitly.
- The UI's undo dropdown shows a horizontal separator at each
  boundary and a subtitle "Preset: [name]" or similar.

## 6. Multi-target actions

Some actions touch many parameters at once (a preset load, a snapshot
recall, a ranges toggle affecting a whole family). These are single
undo entries; their before / after captures every affected parameter.

Reverse: applies every affected parameter's before-value in one
atomic swap.

## 7. Actions that skip the undo stack entirely

- **Audio events**: notes played, MIDI in, string vibration, feedback.
- **MIDI out events**.
- **Session recorder writes** (writing to the ring buffer is
  passive).
- **Meters, LEDs, live overlays**: read-only visualisations.
- **Error banners** (appearing or dismissing).
- **Tooltips**.
- **Preset browser navigation** (browsing does not commit).
- **A / B compare** flipping A <-> B (this is a viewport, not a
  change).
- **Panic** (mutes held notes; not a value change).
- **Tap tempo** (tempo detection, not a value change per se; if the
  user drops tap and reverts to host tempo, that's automatic).

## 8. Reversibility guarantees

Every undoable action can be perfectly reversed if:
- The action's before-state is captured completely.
- No external state has changed since (a file on disk, a MIDI event,
  etc.).

Two edge cases where reverse cannot be exact:
- A part swap that triggered a family switch: reversing puts the old
  family back with the old parts, but any parts added since the
  switch (in the new family) are lost. Warning at time of undo.
- An action after a file save: reversing the action changes state
  but the file on disk is unaffected. The user's next save reflects
  the reverted state.

## 9. UI

- Ctrl-Z / Cmd-Z: undo.
- Ctrl-Shift-Z / Cmd-Shift-Z: redo (also Ctrl-Y on Windows).
- Ctrl-Alt-Z: undo across boundary (with the confirmation banner).
- View menu -> Undo History: opens the dropdown.
- Dropdown:
  - Scrollable list of entries newest first.
  - Boundaries drawn as horizontal rules with subtitle.
  - Clicking an entry undoes back to that point (asks for
    confirmation if crossing a boundary).
  - Search filter at the top.

## 10. Persistence

- Session state: not saved between plugin instance closes.
- Not written to the preset file.
- Not synchronised across instances.

Rationale: undo represents the current session's intent trail;
carrying it between sessions produces surprising behaviour.

## 11. Automation and undo

Host automation writing a value is not undoable inside Luthier. The
host owns the automation lane; the user reverses via the host's own
undo. Luthier's undo captures only user-driven changes from within
the plugin's UI.

If host automation and user UI edits conflict on the same parameter
(rare; usually while an automation lane is playing back and the user
moves the knob), the user's edit takes momentary precedence and the
next automation write reverts. This is the expected DAW behaviour;
no special handling.

## 12. Diagnostics

Options -> Diagnostics -> "Show Undo Depth" adds a small counter to
the footer: "Undo: N / 200; Redo: M". Useful for support tickets
and for verifying grouping is working (a user doing a slow drag
should see the depth grow by 1 per drag, not per intermediate value).

## 13. Tests

- Every action class enumerated in section 3 has a fixture and a test
  verifying:
  - It creates the right entry type.
  - Its description matches the expected pattern.
  - Its before / after state captures reverses cleanly.
  - Its grouping rule (or lack thereof) is honoured.
- Boundary tests: every boundary-creating action is verified to push
  a boundary; Ctrl-Z stops correctly; Shift-Ctrl-Z crosses correctly.
- Stack overflow: 250 actions in a row; verify oldest 50 are dropped
  and undo still reverses cleanly to the 51st.
- Redo clear: any new action clears the redo stack.
- Grouping window: pairs of same-class same-target actions at 199
  and 201 ms merge / don't merge.
- Multi-target atomic: a preset load creates one entry; undo reverses
  every touched parameter in one swap.
- Concurrent audio: undo mid-play doesn't cause dropouts.
- Session end: undo stack empty on new plugin instance.
- No mod / automation / MIDI entries: 1000 mod-driven parameter
  changes create zero undo entries.
