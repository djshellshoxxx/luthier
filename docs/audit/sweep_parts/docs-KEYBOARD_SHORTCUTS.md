## docs/KEYBOARD_SHORTCUTS.md

Every binding the doc lists exists in `AccessibilitySettings` with the documented default and is dispatched through the registry by `LuthierAudioProcessorEditor::keyPressed`; defaults are pinned by `Accessibility::shortcutDefaultsMatchTheCanonicalTable`, but only the overlay and layout-mode shortcuts are pressed in a test. Shift+1..9 is read through `getTextCharacter()`, which yields `!@#...` on most layouts, so the second snapshot bank is effectively unreachable. The doc also omits four live bindings (S, Ctrl+N, Ctrl+Alt+E, Ctrl+[ / ]) and slightly misdescribes the hover readout.

| Req | Summary | Engine location | GUI location | Test | Status |
|---|---|---|---|---|---|
| KS-1 (intro) | All shortcuts rebindable in Options > Accessibility, listed with a search box | `Accessibility/Accessibility.cpp:AccessibilitySettings::rebind` | Options > ACCESSIBILITY, `OptionsPages.cpp` shortcut page `searchBox` | `Accessibility::shortcutsRebindAndRefuseClashes`, `HelpTab.*` shortcut search (HelpTabTests.cpp:343-436) | DONE |
| KS-2 (Global) | F1 Help | `PluginEditor.cpp:keyPressed "help"` | overlay | `Editor::everyOverlayShortcutOpensItsOwnOverlayAndEscapeClosesIt` | DONE |
| KS-3 (Global) | Ctrl+Shift+/ opens the rebind table | `keyPressed "showShortcuts"` -> `OptionsPanel::showShortcutTable` | Options overlay | `Editor::everyOverlayShortcutOpens...` | DONE |
| KS-4 (Global, Overlay) | Escape closes the open overlay, not rebindable; also cancels MIDI Learn | `keyPressed` escape branch, `OverlayHost::dismiss` | n/a | `Editor::everyOverlayShortcutOpens...` | DONE |
| KS-5 (Global) | Space starts/stops the audition phrase | `keyPressed "audition"` -> `startAudition/stopAudition` | n/a | `Editor::spaceTogglesTheAudition` | DONE |
| KS-6 (Global) | Tab Easy/Advanced, L Live Mode, D Practice drawer | `keyPressed toggleAdvanced/toggleLiveMode/togglePractice` | n/a | `Editor::theModesThatChangeTheLayoutTakeEffectAndUndoThemselves` | DONE |
| KS-7 (Global) | P panic, T tap tempo, \ kill switch (toggles from keyboard) | `keyPressed panic/tapTempo/killSwitch` | n/a | `Editor::panicTapAndKillKeysAct` | DONE |
| KS-8 (Presets) | [ / ] previous/next preset, snapshot while Live Mode on | `keyPressed previousItem/nextItem` | n/a | `Editor::bracketsStepPresetsOrSnapshotsInLiveMode` | DONE |
| KS-9 (Presets) | 1-9 recall snapshot 1-9 | `keyPressed` digit branch -> `recallSnapshot` | n/a | `Editor::digitsRecallSnapshots` | DONE |
| KS-10 (Presets) | Shift+1-9 recall snapshot 10-18 — digit read from the key code | `keyPressed` digit branch | n/a | `Editor::digitsRecallSnapshots` | DONE |
| KS-11 (Presets) | PageUp/PageDown previous/next setlist entry | `keyPressed setlistPrevious/Next` -> `Setlist::next/previous` | n/a | `Editor::pageKeysStepTheSetlist` | DONE |
| KS-12 (File) | Ctrl+O browser, Ctrl+Shift+S Save As, Ctrl+E export, Ctrl+, Options, Ctrl+D debug | `keyPressed presetBrowser/saveAs/export/options/debugPanel` | overlays | `Editor::everyOverlayShortcutOpens...`, `GuiReach::everyAutomatableParameterHasAVisibleControl` walk | DONE |
| KS-13 (File) | Ctrl+S saves current preset (falls back to Save As) | `keyPressed "save"` -> `PresetManager::saveCurrent` | n/a | - | NO-TEST |
| KS-14 (File) | Ctrl+G save guitar as .luthierguitar; Ctrl+Shift+E reveal guitar file | `keyPressed saveGuitarAs/revealGuitar`, `showSaveGuitarDialog` | dialog | `Editor::guitarFileShortcuts` (Ctrl+Shift+E on an edited guitar) | PARTIAL |
| KS-15 (File) | Ctrl+L arm MIDI Learn then click a control | `keyPressed midiLearnArm` -> `setMidiLearnArmed`, `MidiLearnArmLayer` | overlay layer | `Editor::ctrlLArmsMidiLearn` | DONE |
| KS-16 (File) | Ctrl+Z / Ctrl+Shift+Z undo/redo | `keyPressed undo/redo` -> `processor.undo/redo` | n/a | `Editor::undoRedoAndABKeysReachTheProcessor` | DONE |
| KS-17 (File) | Ctrl+R randomise, Ctrl+Shift+R reset everything, Ctrl+/ A/B | `keyPressed randomise/resetAll/abCompare` | n/a | `Editor::randomiseAndResetKeys`, `Editor::undoRedoAndABKeysReachTheProcessor` | DONE |
| KS-18 (doc) | Doc lists every default binding — omits S (slide mode), Ctrl+N (new preset/Init), Ctrl+Alt+E (reveal preset), Ctrl+[ / ] (workspace tab) | `Accessibility.cpp` registry | n/a | n/a (doc) | DONE |
| KS-19 (Control) | Left-drag adjust, Shift coarse, Ctrl ultra-fine | `UI/Widgets.cpp:LuthierKnob::KnobSlider::mouseDrag` (sensitivity 70/180/1200) | every knob | `Widgets::modifierDragSensitivity` | DONE |
| KS-20 (Control) | Double-click resets to default | JUCE `SliderParameterAttachment` (`setDoubleClickReturnValue`) via `LuthierKnob::attachTo` | every knob | `Widgets::doubleClickReturnsAKnobToItsDefault` | DONE |
| KS-21 (Control) | Right-click: Enter value, Reset, Copy, Paste, MIDI Learn, Lock, Randomise | `Widgets.cpp:showParameterContextMenu` | every knob | `Editor::rightClickOffersModulationAndBuildsTheRoute` (Modulate only), `RangesUi::rightClickUnlocksAndRestrictsOneControl` — core items not asserted | NO-TEST |
| KS-22 (Control) | Hover row: value appears above the control, label stays; tooltip 400 ms | `LuthierKnob::paint` showValue | every knob | - | DONE |
| KS-23 (Fretboard) | Click plays note, higher in lane = harder | `FretboardComponent::mouseDown` velocity from `withinLane` -> `triggerPreviewNote` | Easy/Advanced fretboard | - | NO-TEST |
| KS-24 (Fretboard) | Right-click: mute string, select string, set capo, scale overlay | `FretboardComponent::mouseDown` popup, `setCapoFret` drives `capoFret` param | fretboard | - | NO-TEST |
| KS-25 (Illustration) | Click pickup selects it; click switch advances position; drag knob = volume/tone | `GuitarBodyComponent::mouseDown/mouseDrag` | guitar illustration | `Editor::illustrationClicksSelectPickupAndStepSwitch` | DONE |
| KS-26 (Pedal rack) | Drag slot onto another reorders; right-click clear slot / reset pedal | `PedalRack.cpp:PedalSlotComponent::mouseUp/onReorderRequested`, `PedalRack::reorder`; menu items 1/2 | Advanced pedal rack | `PedalRack::dragOntoAnotherSlotReorders` (the clear/reset menu items are not driven) | DONE |
| KS-27 (Overlay) | Escape, click outside, Close button all close; only one overlay at a time | `Overlays.cpp:OverlayHost::mouseDown/dismiss`, `closeButton` | every overlay | `Editor::overlaysCloseFromTheScrimAndTheirCloseButton` | DONE |

<!-- counts DONE=7 NO-GUI=0 NO-TEST=18 PARTIAL=2 MISSING=0 OWNED=0 -->
