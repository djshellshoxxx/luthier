## action-and-undo.md

On this checkout undo is still the simple whole-state snapshot stack in `LuthierAudioProcessor` (`undoStack`, max 200, one entry per host gesture, entries carry only a description). There is no action class, target, timestamp or boundary flag, no 200 ms grouping, no Ctrl-Y / Ctrl-Alt-Z, no history dropdown, and undo also restores view state (advanced mode, tab, Live Mode) and the tune. Mod-matrix, snapshot, setlist, MIDI Learn, pedal add/move/remove, practice and character-engine edits push nothing. Workshop part swaps and bench drags, ranges toggles, and the Tune Builder's own grouped history (TuneSession) are done and tested. Nearly all of the rest is built on `claude/luthier-visual` (Support/UndoHistory, `pushUndoBoundary`, UndoTests.cpp, 20+ tests). Three items are missing on every branch: the Show Undo Depth counter, the history search filter and the family-switch undo warning.

| Req | Summary | Engine location | GUI location | Test | Status |
|---|---|---|---|---|---|
| AU-1 (§0.1) | One user change = one entry (gesture-based) | `PluginProcessor.cpp:parameterGestureChanged` | any control; Header undo | `Undo::stepsOneActionAtATimeBothWays`, `WorkshopBench::aDragIsOneUndoEntryWithItsBeforeAndAfter` | DONE |
| AU-2 (§0.2, §5) | Boundaries divide the stack; undo stops there — none here | - | - | - (visual `Undo::aPresetLoadIsABoundary`) | OWNED |
| AU-3 (§0.3, §4) | 200 ms same-class same-target grouping — none here | - | - | - (visual `Undo::gesturesGroupWithin200ms`, 199/201 ms) | OWNED |
| AU-4 (§0.4, §7) | Never undoable: live audio/MIDI, recorder, banners, A/B, panic, tap, browsing — by construction, untested | gesture-only entries | n/a | - | NO-TEST |
| AU-5 (§0.5) | Per-instance stack | `PluginProcessor::undoStack` member | n/a | - | NO-TEST |
| AU-6 (§0.6, §10) | Not persisted / not in preset / empty on new instance | not in `getStateInformation` | n/a | `Undo::stepsOneActionAtATimeBothWays` (fresh processor has nothing) | DONE |
| AU-7 (§1) | Entry: class, target, before/after, timestamp, description, boundary — here only state/redoState/description | `PluginProcessor.h:UndoEntry` | n/a | - (visual `Support/UndoHistory.h:Entry`) | OWNED |
| AU-8 (§1, §9) | Undo History dropdown, newest first, click undoes to that point — none here | - | - | - (visual HeaderBar "Undo history...", `Undo::theHistoryListsNewestFirstAndUndoesToAPoint`) | OWNED |
| AU-9 (§2) | Max 200, oldest dropped — implemented, overflow untested here | `addUndoEntry`, `kMaxUndoSteps` | n/a | - (visual `Undo::overflowDropsTheOldest`, 250 -> 200) | OWNED |
| AU-10 (§2) | Redo cleared on new action | `addUndoEntry` | n/a | `Undo::stepsOneActionAtATimeBothWays` | DONE |
| AU-11 (§3.1) | Description "Change X from A to B" — here "Change X" | `parameterGestureChanged` | Header undo tooltip | - (visual `Undo::aGestureIsOneEntry` "Change Gain from 0.20 to 0.70") | OWNED |
| AU-12 (§3.1) | Drag pausing > 200 ms splits into two entries | - | - | - (visual `Undo::gesturesGroupWithin200ms`) | OWNED |
| AU-13 (§3.1, §11) | No entries from modulation / automation / MIDI CC — message-thread + gesture check here, untested | `parameterGestureChanged` thread check | n/a | - (visual `Undo::writesWithoutAGestureMakeNoEntries`, 1000 writes) | OWNED |
| AU-14 (§3.2) | Discrete switch rapid scroll -> one entry | - | - | - (visual grouping) | OWNED |
| AU-15 (§3.3) | Toggles "Turn on/off X" — here "Change X" (Slide Mode explicit) | `parameterGestureChanged`, `HeaderBar` slide | Header / toggles | - (visual `Undo::aToggleSaysTurnOnOrOff`) | OWNED |
| AU-16 (§3.4) | Part swap one entry naming slot and part (wording "Fitted X (was Y)") | `WorkshopBench::fit*` | WORKSHOP tab drawer cards | `WorkshopPanel::clickingACardFitsItAsOneUndoEntry`, `WorkshopBench::fittingAPartSaysWhatItReplaced` | DONE |
| AU-17 (§3.4, §5) | Family switch is a boundary — plain entry here | `PluginProcessor.cpp` "Change guitar family" | WORKSHOP guitar category | - (visual `pushUndoBoundary("Change guitar family to ...")`) | OWNED |
| AU-18 (§3.5) | Illustration drag one entry "Move handle X mm -> Y mm" | `WorkshopBench::beginGesture/endGesture/commit` | WORKSHOP bench illustration | `WorkshopBench::aDragIsOneUndoEntryWithItsBeforeAndAfter`, `WorkshopPanel::aPickupDragIsOneEntryAndTheRulerValueFollows` | DONE |
| AU-19 (§3.6) | Mod-matrix create/delete/edit/source entries — ModMatrixPanel pushes nothing | `ModMatrixPanel` | ADVANCED > MOD tab | - (visual `Undo::rightClickModulationIsUndoable`, `DragToModulate::aDroppedSourceRoutesAt25PercentAsOneEntry`) | OWNED |
| AU-20 (§3.7) | Snapshot save/recall/rename/colour/delete/move entries — LivePanel pushes nothing | `SnapshotBank`, `LivePanel` | LIVE tab | - (visual `Undo::snapshotSaveAndRecallAreEntries`) | OWNED |
| AU-21 (§3.8) | Preset load = boundary "Load preset [name]" — plain unnamed entry; host PC and setlist step push nothing | `HeaderBar.cpp`, `Overlays.cpp`, `PluginEditor.cpp` pushUndoState("Load preset") | Header preset browser | - (visual `Undo::aPresetLoadIsOneNamedEntry`, `Undo::aPresetLoadIsABoundary`) | OWNED |
| AU-22 (§3.8) | Save / rename preset not on the stack — by construction, untested | PresetManager save paths | Header | - | NO-TEST |
| AU-23 (§3.9) | Tune classes (section/chord/melody edit/record/generate), same-target 200 ms grouping, tune load boundary (clears history) | `Tune/TuneSession::edit/undo`, `TuneEditClass` | TUNE tab (Ctrl-Z in `TunePanel`) | `TunePanel::sessionUndoGroupsSameTargetEditsWithin200ms` | DONE |
| AU-24 (§3.10) | Setlist load / step / edit entries — none here | `PluginProcessor` setlist | LIVE tab setlist | - (visual `pushUndoBoundary("Load setlist"/"Setlist step")`, LivePanel entries) | OWNED |
| AU-25 (§3.11) | Ranges lock/unlock is one undoable entry ("Unlock all ranges" etc.) | `PluginProcessor::changeRanges`, `RangesUi::apply` | Options > Ranges; right-click menu | `RangesUi::rightClickUnlocksAndRestrictsOneControl`, `Undo::stepsOneActionAtATimeBothWays` | DONE |
| AU-26 (§3.12) | MIDI Learn learn/delete entries — none here | `Support/MidiLearn` | right-click Learn | - (visual `Undo::aMidiLearnIsOneEntry`) | OWNED |
| AU-27 (§3.13) | Pedal param / bypass entries via slot attachments (gestures) — untested | slot attachments | FX racks | - | NO-TEST |
| AU-28 (§3.13) | Pedal add/remove/move entries — PedalRack writes without gestures | `UI/PedalRack` | FX racks | - (visual `Undo::movingAPedalIsOneEntry`) | OWNED |
| AU-29 (§3.14) | Practice scale/looper settings entries; deleted loop layer restorable; backing track load entry | `Practice/Looper` (own 1-level undo) | PRACTICE drawer | `PracticeLooper::layerUndoAndRedo` (looper-local) (visual `Undo::aClearedLoopLayerComesBack`) | OWNED |
| AU-30 (§3.15) | Character edits one entry — APVTS params yes, CharacterEngine seed/wear edits push nothing | `Character/CharacterEngine` | CHARACTER tab | - (visual tier 4 character entries) | OWNED |
| AU-31 (§3.17) | UI state not undoable — undo here restores advancedMode/tab/liveMode and tune from the snapshot | `PluginProcessor::undo` -> `setStateInformation` | n/a | - (visual `Undo::doesNotMoveTheViewOrTheTune`) | OWNED |
| AU-32 (§5) | Ctrl-Alt-Z crosses a boundary (C-28: replaces Shift-Ctrl-Z) with confirmation banner | - | - | - (visual `undoAcrossBoundary` shortcut + banner) | OWNED |
| AU-33 (§5, §9) | Dropdown shows a rule at each boundary with "Preset: [name]" subtitle | - | - | - (visual HeaderBar history separators) | OWNED |
| AU-34 (§6) | Multi-target actions (preset load, snapshot recall, family ranges) one atomic entry | whole-state snapshot in `pushUndoState` | n/a | - (visual `Undo::aPresetLoadIsOneNamedEntry`, `Undo::resetEverythingIsOneEntry`) | OWNED |
| AU-35 (§8) | Undoing a family switch warns that parts added since are lost — nowhere | - | - | - | MISSING |
| AU-36 (§8) | Undo after save leaves the file unchanged — by construction, untested | - | n/a | - | NO-TEST |
| AU-37 (§9) | Ctrl/Cmd-Z undo, Ctrl/Cmd-Shift-Z redo | `Accessibility.cpp` "undo"/"redo" | keyboard + Header | `Accessibility::shortcutDefaultsMatchTheCanonicalTable` | DONE |
| AU-38 (§9) | Ctrl-Y redo (Windows) | - | - | - (visual "redoAlt") | OWNED |
| AU-39 (§9) | History search filter at the top of the dropdown — nowhere (visual uses a PopupMenu) | - | - | - | MISSING |
| AU-40 (§12) | Options > Diagnostics "Show Undo Depth" footer "Undo: N / 200; Redo: M" — nowhere | `getNumUndoSteps` exists | none | - | MISSING |
| AU-41 (§13) | Per-class fixture tests (entry type, description, reverse, grouping) | - | n/a | - (visual UndoTests.cpp, one per class) | OWNED |
| AU-42 (§13) | Concurrent audio: undo mid-play no dropouts | - | n/a | - (visual `Undo::undoMidPlayProducesNoGarbage`) | OWNED |

<!-- counts DONE=8 NO-GUI=0 NO-TEST=6 PARTIAL=0 MISSING=3 OWNED=25 -->
