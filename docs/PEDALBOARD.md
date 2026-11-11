# Pedalboard user guide

This guide describes the reference pedalboard as the code at commit `ca14934e` behaves.
Statements are taken from the source; the comment after each paragraph names the file
and line it comes from. Anything not checked against a running build is marked
"not verified".

**Status.** The pedalboard is not release-qualified. See
[KNOWN_ISSUES.md](KNOWN_ISSUES.md), sections "Pedalboard" and "Platform qualification".
For the controls of each model see [EFFECTS_REFERENCE.md](EFFECTS_REFERENCE.md).

## What the pedalboard is

The pedalboard is a separate effects subsystem from the two eight-slot pedal racks in
the Advanced panel. The two exist side by side and neither replaces the other. The
board is stereo-capable and holds up to 16 pedals.

<!-- Source/PluginEditor.cpp:74 ("legacy racks are untouched"); Source/ReferencePedals/BoardTypes.h:22 (kMaxSlots = 16); Source/ReferencePedals/Board.h:6 -->

The board is **off by default**. When it is off, or when it has no pedals, the audio
path is the one the plugin had before the board existed.

<!-- Source/ReferencePedals/BoardTypes.h:197 (enabled = false, "board absent == legacy path"); Source/ReferencePedals/Board.h:69 (isActive: enabled and at least one non-empty pedal) -->

## Opening the board

Click the pedalboard button in the editor header (tooltip "Show/hide pedalboard
(Ctrl+Shift+B)"), or press Ctrl+Shift+B (Cmd+Shift+B on macOS) while the editor has
keyboard focus. The same shortcut also works while the board itself has focus.

<!-- Source/PluginEditor.cpp:77-85 (button, tooltip); Source/PluginEditor.cpp:811-813 (editor shortcut); Source/UI/ReferencePedals/BoardRoot.cpp:150 (shortcut inside the board) -->

To make the board process audio, switch **Board ON** on in the board toolbar and add at
least one pedal.

<!-- Source/UI/ReferencePedals/BoardRoot.h:55 ("Board ON" toggle); Source/ReferencePedals/Board.h:69 -->

## Pop-out window and docked drawer

The board can appear in two ways.

- **Pop-out window.** This is the default. The board opens in its own window, which has
  a "Keep on top" option.
- **Docked drawer.** The board sits in a drawer along the bottom of the plugin editor.
  Use the **Dock** button in the board toolbar to dock, and **Pop out** to float it
  again.

<!-- Source/UI/ReferencePedals/BoardUiState.h:9 (windowMode default "popout"); Source/PluginEditor.cpp:1734-1740 (toggleReferenceBoard); Source/UI/ReferencePedals/BoardRoot.cpp:31,63 (Dock, Pop out, Keep on top buttons) -->

**Window-height rule.** Docking needs an editor at least 840 pixels tall. If the editor
is shorter, the dock request is refused with the message "A taller editor is needed for
the docked pedalboard. The pop-out remains available." A docked board is also closed
automatically if the editor is resized below 840 pixels. The board never overlays the
main controls.

<!-- Source/PluginEditor.cpp:632-633 (close below 840), 1739 (dock only if height >= 840), 1761-1762 (refusal message) -->

If the host does not allow floating windows, the board falls back to the dock when the
editor is tall enough, and otherwise shows "Floating windows unavailable. Enlarge the
editor to use the docked pedalboard."

<!-- Source/PluginEditor.cpp:1752-1755 -->

When the board window is narrower than 760 pixels, the pedal browser on the left is
hidden; use the **+ Add** button instead.

<!-- Source/UI/ReferencePedals/BoardRoot.cpp:116 (layoutContent: browser needs width >= 760) -->

## Adding pedals

Three ways, all of which add one pedal at the end of the signal chain, in the "PRE"
placement and lane 0 unless you change those afterwards:

1. Select a model in the browser on the left (search box and an All / Generic /
   Reference filter) and press **Add**, double-click it, or press Return on it.
2. Press **+ Add** in the toolbar and choose a model from the menu.
3. Drag a model from the browser onto the board canvas and drop it where you want it.

The browser and the menu list only the registered models: 13 today. The Add button is
disabled at 16 pedals with the tooltip "Board capacity is 16 pedals. Remove a pedal
first."

<!-- Source/UI/ReferencePedals/PedalBrowser.cpp:9-24, 40-44; Source/UI/ReferencePedals/BoardRoot.cpp:119-122 (showAddMenu); Source/UI/ReferencePedals/BoardCanvas.cpp:44-47 (drop); Source/ReferencePedals/BoardCommand.h:17-23 (Add defaults: append, PRE, lane 0); Source/ReferencePedals/Models/RegisterPrototypes.cpp:11-16 -->

## Removing, duplicating, moving and bypassing

Each pedal has a context menu (right-click, or Shift+F10 when the pedal has focus) with
Move earlier, Move later, Duplicate, Remove and Learn footswitch.

<!-- Source/UI/ReferencePedals/PedalView.cpp:296, 307-314 -->

| Action | Mouse | Keyboard (pedal focused) |
|---|---|---|
| Remove | context menu | Delete or Backspace |
| Duplicate | context menu | Ctrl+D |
| Move one place earlier or later | context menu, or drag | Alt+Left / Alt+Right |
| Move to first or last | drag | Alt+Home / Alt+End |
| Change routing lane | pedal inspector | Alt+Up / Alt+Down (lanes 0 to 3) |
| Change placement (PRE, LOOP, POST) | pedal inspector | Alt+Shift+Left / Alt+Shift+Right |
| Bypass or engage | footswitch on the pedal | Space or Return |
| Open the pedal inspector | "Pedal information" button | I |
| Move focus to the neighbouring pedal | click | Left / Right |

<!-- Source/UI/ReferencePedals/PedalView.cpp:267-297 (every row of this table); inspector: PedalView.cpp:59-62,73-80 -->

A duplicate keeps the original's control values and has its own identity.

<!-- test RefPedalBoardCore.duplicatePreservesControlsAndHasIndependentIdentity (Source/Tests/); not re-run for this document -->

Bypass is per pedal. The board owns the bypass crossfade for buffered models (5 ms,
equal power) so that engaging and bypassing do not click, and the board's reported
latency does not change with bypass.

<!-- Source/ReferencePedals/BoardTypes.h:30 (kBypassXfadeMs = 5); tests RefPedalRouting.slotBypassAndBoardDryHaveIdenticalLatency, RefPedalBufferedTimeFx.boardOwnsFadeAndBypassHistoryIsIndependent (not re-run here) -->

The inspector also holds input and output trim (each -12 to +12 dB), mounting style,
MIDI channel and footswitch mode (latch, momentary or auto).

<!-- Source/UI/ReferencePedals/PedalView.cpp:59-82 -->

## Controls on a pedal

Controls are knobs, switches and buttons. Keyboard use of a focused knob: arrow keys
change the value by 0.01 of the range, Shift+arrow by 0.1, Ctrl+arrow by 0.001; Return
opens a value editor; right-click also opens the value editor; the mouse wheel changes
by 0.01. Values here are fractions of the control's range.

<!-- Source/UI/ReferencePedals/KnobControl.cpp:8-24 -->

## Drag operation

Drag a pedal by its body (after moving at least 4 pixels) to reorder it; a ghost image
follows the pointer. Press Escape during a drag to cancel it. A whole drag is recorded
as one undo step.

<!-- Source/UI/ReferencePedals/PedalView.cpp:301-306 (4 px threshold), BoardCanvas.cpp:105 (ghost), BoardCanvas.cpp:113 and PedalView.cpp:270 (Escape); tests RefPedalUi.dragDwellThrottleFinalCommitAndUndo, RefPedalUi.canvasGhostCancellationAndPlacementDropUseOneTransaction (not re-run here) -->

## Undo and redo

The toolbar has **Undo** and **Redo** buttons. Ctrl+Z undoes, Ctrl+Shift+Z or Ctrl+Y
redoes (Cmd on macOS). History holds 100 steps. Dragging a control or a pedal counts as
one step.

<!-- Source/UI/ReferencePedals/BoardRoot.cpp:151 (shortcuts), 32 (buttons); Source/ReferencePedals/BoardModel.h:63 and BoardModel.cpp:259 (100 steps); BoardModel.h:63 ("drags coalesce") -->

## Board toolbar

| Control | Meaning |
|---|---|
| Style | Visual board style (seven choices: plywood, velcro, clamps, routed, rail, ceramic, loose). |
| Routing mode | Series, Parallel, AB or Loop. |
| Board ON | Turns the whole board on or off. |
| Wet/dry mix and mix law | Universal mix of the whole board; double-click resets to 100 % wet. Law: Equal power, Linear mix or Third law. |
| Tempo | Board tempo, 20 to 300 BPM. |
| MIDI Learn | See below. |
| Cables | Show or hide cables. |
| Fit, -, +, zoom | Zoom 50 % to 200 %. Ctrl+0 fits, Ctrl+= and Ctrl+- zoom. |
| Settings | Opens the board settings and MIDI mapping panel. |
| Dock, Pop out, Keep on top | Window controls described above. |

<!-- Source/UI/ReferencePedals/BoardRoot.cpp:19-31 (toolbar), 153 (zoom shortcuts), 103 (zoom range); Source/ReferencePedals/BoardTypes.h:48-57 (enums) -->

Zoom, scroll position, cables and window mode are saved with the board.

<!-- Source/UI/ReferencePedals/BoardRoot.cpp:92-101 (saveView/flushView); BoardUiState.cpp -->

Complex cable routing is not fully built in the interface (not verified beyond the
handoff record `docs/handoff/LUTHIER-CURRENT-COMPLETION-2026-10-27.md`).

## Snapshots and scenes

The board model can store eight snapshots (also called scenes, indices 0 to 7) and
recall them as one undoable step. Each snapshot holds a full board and its name, and
snapshots are saved with the plugin state.

<!-- Source/ReferencePedals/BoardSnapshots.h:6-14; Source/ReferencePedals/BoardModel.cpp:284-312; Source/ReferencePedals/BoardState.cpp:327 (at most 8); tests RefPedalState.eightSnapshotsSurvivePersistence, RefPedalBoardCore.sceneCommandsPreserveBankAndMakeRecallOneUndoableIntent (not re-run here) -->

**Not available yet:** a user-facing way to store or recall them. A search of the
board interface code and the MIDI code found no on-screen control and no MIDI action
that calls `BoardSnapshots::capture` or `recall`, or the `SetScene` and `ApplySnapshot`
commands; they are called from tests only. Treat snapshots as an engine feature that is
not yet reachable from the interface (not verified in a running build).

<!-- grep of Source for BoardSnapshots::, SetScene, ApplySnapshot outside Source/Tests and the model itself: no callers (Source/UI/ReferencePedals/*, Source/ReferencePedals/Midi/*) -->

## MIDI learn

1. Switch **MIDI Learn** on in the toolbar (or press Ctrl+L).
2. Click a control, or the board's mix slider, or a pedal's footswitch. With a pedal
   focused you can press L instead.
3. Send a MIDI message: CC, velocity, aftertouch or pitch bend. The status line shows
   "MIDI mapping captured".

Learn waits 20 seconds and then gives up ("MIDI learn timed out"). Escape cancels the
wait or leaves learn mode. The Settings panel holds the board's MIDI mapping options.

<!-- Source/UI/ReferencePedals/BoardRoot.cpp:64 (learn toggle text), 36-43 (mix learn), 132-142 (timeout, capture), 153 (Ctrl+L), 155-161 (Escape); PedalView.cpp:285-295 (L key) -->

MIDI support on the board is partial. There is no MIDI clock output, and macro, NRPN,
scene and bank workflows are not complete, according to the handoff record
`docs/handoff/LUTHIER-CURRENT-COMPLETION-2026-10-27.md` (not verified here). Binding,
learn, footswitch notes and expression handling have unit tests (`RefPedalMidiCore`).

## Where the board sits in the signal chain

Each pedal has a placement:

| Placement | Position |
|---|---|
| PRE | Before the legacy pre-effects rack (the default for a new pedal). |
| LOOP | After the legacy post-effects rack, before the cabinet. |
| POST | After the cabinet, before the room. |

<!-- Source/ReferencePedals/BoardTypes.h:38-44; Source/LuthierEngine.cpp:3485 (PRE before preEffects), 3553 (LOOP after postEffects), 3606 (POST after the cabinet); Source/UI/ReferencePedals/PedalView.cpp:69 (labels) -->

The guitar signal is mono up to the first effects stage; the board receives it as two
identical channels.

<!-- Source/LuthierEngine.cpp:3472-3482 -->

## Limits

- 16 pedals on the board.
- 8 snapshots.
- 4 routing lanes and 100 undo steps.
- A total latency above 8192 samples is rejected when adding.

<!-- Source/ReferencePedals/BoardTypes.h:22-31 (kMaxSlots, kMaxBranches, kMaxLatencySamp); BoardState.cpp:327; BoardModel.cpp:259 -->

## What is not available yet

- The six named models planned for the first pedalboard release (a phaser, two
  bucket-brigade choruses, a digital delay, a compressor and a looper) are unregistered
  prototypes. They do not appear in the browser.
- No on-screen or MIDI way to store and recall snapshots (above).
- Native macOS and Windows qualification of the board has not been done.
- Whether the Free edition differs from Pro on the board is not verified; no edition
  check was found in the board code.

<!-- Source/ReferencePedals/Models/RegisterModDelay.cpp:11, RegisterDriveB.cpp:11, RegisterRc5.cpp:11 (empty register functions); Source/ReferencePedals/PedalRegistry.cpp:240-252 -->
