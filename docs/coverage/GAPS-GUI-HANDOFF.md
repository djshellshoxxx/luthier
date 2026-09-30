# GAPS-GUI handoff

Written at the token/tool-call budget checkpoint in `docs/helpers/GAP_FILL_INSTRUCTIONS.md`.
Three batches landed, all green, all pushed to `claude/luthier-gaps-gui`. The
next helper should pick up from here rather than re-deriving what is already
done - re-read `docs/coverage/GAPS-GUI.md` first (the row-by-row table this
file summarizes) and `docs/audit/SPEC_SWEEP.md`'s `gui-integration.md`,
`ui-wiring.md`, `theme.md`, `accessibility.md` and `gui-engine-dataflow.md`
sections (that audit does not update itself - re-check status against the
live code, not just this note).

## Environment note

`scripts/setup_linux.sh` and the full build/test cycle were already run this
session; `build/` exists and is configured. A fresh session still needs to
run setup (per the instructions) since it is a new container, but the repo
itself needs no further preparation.

## Rows done (3 batches, 3 commits + 2 doc commits, all pushed)

See `docs/coverage/GAPS-GUI.md` for the full table. Short list:

- **GD-5, GD-8**: `MasterBus::getProcessedBlockCount()` (new shared infrastructure), `OutputLed` and `LevelMeter`
  staleness/timing fixes to match spec.
- **TH-4, TH-6, TH-7, TH-8, TH-10, TH-12, TH-13, TH-14, TH-15, TH-16, TH-18, TH-19, TH-23, TH-27, TH-31, TH-34**:
  theme.md metrics fixes and new tests (most were "code already right, just needed a test").
- **UW-30**: `LevelMeter` test accessors + peak-hold/stale test.
- **A11Y-14**: arrow-key fine/coarse stepping on `LuthierKnob`.
- **A11Y-25**: `LevelMeter` shape (narrow strip + overload bracket), not colour alone.
- **UW-14**: `LuthierToggle::setMomentary()` (capability only - not wired to the Live kill pill; see below).
- **GI-7, GI-35**: WHAMMY group hidden (not greyed) on a hardtail bridge.

All verified with targeted suite runs (`Theme`, `Accessibility`, `Widgets`, `Editor`, `RangesUi`,
`BassTechniques`, `SlideUi`, `GuiReach`, `QualityModeUi`, `FacesIntegration`, `Master`, `Live`, `Undo`,
`MidiLearn`, `HeaderBar`, `Faces`, `ModelGapsUi`) - no new failures introduced. **Not yet run**: the full
`LuthierTests` suite (35 min) and the VST3/Standalone build. Run both before this workstream calls itself
done, per the DONE criteria in `GAP_FILL_INSTRUCTIONS.md`.

Three pre-existing failures were found and confirmed unrelated (present before this branch touched
anything, in files this branch never edited):
- `GuiReach::everyAutomatableParameterHasAVisibleControl` on `scrape_*`/`slap_*`/`macro_assign_*`/`pickup_blend`
  (GI-1, explicitly the techniques workstream's).
- `QualityModeUi::CQ22_builtEditorsRegisterEveryTableClassThatExists` on `StringAnimator` (cpu-quality-modes.md,
  not in this workstream's spec list).
- `Accessibility::shortcutsRebindAndRefuseClashes` / `noTwoShortcutsShareADefaultKey` (a 3-way default-key clash
  among `toggleNormalization`/`toggleStringAnimation`/`cycleCpuQuality`) and `FacesIntegration::theAdvancedAmpSectionHasItsControlsOnTheFace`
  (amp face sizing). Worth a look if no other workstream claims them, but they are not gui-integration/ui-wiring/
  theme/accessibility/gui-engine-dataflow rows as far as this session could tell - do not assume they are this
  workstream's without checking the audit again.

## Two test-writing traps worth knowing before you hit them again

- `juce::Button::triggerClick()` and a param's `setValueNotifyingHost()` -> `ComboBoxAttachment`/`SliderAttachment`
  path both post through message-loop machinery (`triggerClick` posts a message; parameter attachments are
  `AsyncUpdater`s) that a console test never pumps. Use `button->onClick()` directly, or `Button::setState(...)`
  (synchronous via `sendStateMessage`), or drive the `ComboBox`/`Slider` itself with `sendNotificationSync`
  instead of going through the parameter. `EditorTests.cpp`'s `clickButton()` helper documents this trap already;
  it bit this session twice more before being remembered.
- `juce::KeyPress::operator==(int keyCode)` returns false outright if *any* modifier is down - it is meant for
  matching an unmodified key, not "is this the right/up/left/down key regardless of modifiers". Compare
  `key.getKeyCode()` instead when modifiers matter (see `KnobSlider::keyPressed` for the fixed version).
- A component's own opaque background (e.g. `LevelMeter`'s `panelSunken` fill) makes an alpha-only pixel probe
  useless for "is anything drawn here" - compare colour against the background colour, not just alpha.

## Rows explicitly not started, with the reasoning already done

Full list is in `docs/coverage/GAPS-GUI.md`'s "Remaining rows in scope" section. Highlights, in roughly the
order they are worth doing next (effort S/M, low risk, building on what already landed):

1. **GD-26 + UW-14 follow-through**: add a `kill_switch_active` parameter (appended; see CODE RULES in
   `GAP_FILL_INSTRUCTIONS.md` for the `GAPS-GUI` param block markers) and wire `LiveStrip::killButton` to a
   `LuthierToggle::setMomentary(true)` attached to it, replacing the direct `KillSwitch` object call. The toggle
   capability this needs is already built and tested (`Widgets::aMomentaryToggleSetsOnPressAndClearsOnRelease`).
2. **GI-16 / GD-4 / GD-5's other half**: add input+output `LevelMeter` instances to `HeaderBar` (30 Hz poll).
   `LevelMeter`'s own stale/peak-hold logic is already correct and tested (batch 1); this is purely wiring two
   instances into the header and feeding an input-side peak (`MasterBus` has no input meter yet - `GD-4` is a
   bigger row, a DI/sidechain peak tap, possibly worth splitting from the output-meter half).
3. **GI-13, GI-17, GI-18, GI-19, GI-20**: header Save button, tap/kill/gear/dice/reset icons, the <1280px
   collapse, tooltip+shortcut completeness. All isolated `HeaderBar.cpp`/`.h` additions with existing
   File-menu handlers to reuse (`showFileMenu`, `randomiseParameters`, `resetEverything` already exist per the
   audit's own notes).
4. **GI-33**: one button in `AdvancedPanel`'s "Body" section selecting the CHARACTER Col-4 tab. Small, contained,
   same file this session already edited for GI-7/35 (`updateBridgeVisibility`'s neighbourhood).
5. **GI-70**: slide glide-target label in `GuitarBodyComponent`'s tuning popover, reading `SlideEngine`'s target
   when Slide Mode is on.
6. **GI-72, GI-73**: snapshot Shift-click write vs plain-click recall in `SnapshotStrip::mouseDown`
   (`Source/UI/LiveStrip.cpp`), plus a render-probe test for the 12-char truncation and active-slot outline.
7. **A11Y-7, A11Y-9**: call `AccessibleSetup::configureMeter` from `LevelMeter`'s constructor/`setSource`, and
   give `SnapshotStrip` real accessible children. Both are named, scoped tasks in accessibility.md's own work list.
8. **GD-9, GD-13, GD-14**: chord-readout dimming instead of clearing, buzz-heatmap 500 ms stale+fade, slide-bar
   freeze-at-60%-alpha. Each is a small change to one existing timer/paint method (`EasyPanel::timerCallback`,
   `BuzzHeatmap::isStale` in `Source/UI/SetupGroup.cpp`, `FretboardComponent` around line 168).

## Rows deliberately deferred with a documented reason (not just "not started")

- **TH-17** (100 ms timed button flash): needs per-button timer state across every `TextButton`, judged too
  invasive to retrofit quickly; left for a dedicated pass.
- **UW-15** (`LuthierChoice::rebuildItems`): `AudioParameterChoice::choices` is `const` in stock JUCE, so no
  parameter on this checkout can actually have a changing choice list today - the row may need a product/
  architecture decision (a new dynamic-choice parameter type) before it is actually actionable, not just an
  S-effort code change.
- **GI-95** ("Assign to macro" menu item): macros are already reachable through the existing "Modulate -> Macro"
  submenu in the control right-click menu (`Widgets.cpp:buildParameterContextMenu`). Whether the spec wants a
  *separate* top-level "Assign to macro" entry as well, or considers the existing path equivalent, is a product
  call this session did not make unilaterally - flag it rather than guessing.
- Everything under "Larger, higher-risk items" in `GAPS-GUI.md`: the SPSC command queue (UW-6), the per-subsystem
  display FIFO (UW-20), the locale-change broadcaster (UW-49), the `Tests/Ui/Dataflow/` suite (GD-35), and the
  string-catalog/14-locale work (A11Y-4/32/33) are all real, multi-file, higher-risk efforts explicitly saved
  for a session that can give each one a full run rather than a rushed slice.

## Before calling this workstream DONE

1. Re-run the full `LuthierTests` suite once (~35 min) and confirm no new failures beyond the pre-existing ones
   listed above.
2. Build `Luthier_VST3` and `Luthier_Standalone` and confirm they link.
3. Work through the "Rows explicitly not started" list above (or the live audit, if it has moved) until every
   remaining MISSING/PARTIAL/NO-TEST/NO-GUI row in this workstream's five spec files is DONE or DEFERRED with a
   one-line reason, per `GAP_FILL_INSTRUCTIONS.md`.
