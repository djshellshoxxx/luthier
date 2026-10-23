# Reference pedalboard UI acceptance ledger

Owner WP17–22. Implementation checkpoint, 2026-10-23. This is a partial implementation of the complete UI_BOARD_STYLES acceptance contract, not a release sign-off.

## Implemented paths

- The editor PEDALS toggle and Ctrl/Cmd+Shift+B open a non-modal native BoardWindow. Window X synchronises the toggle. A single Drawer-owned BoardRoot is reparented into the window and back into the dock without transferring ownership. The editor destroys the window before the Drawer/root. No modal loop is used.
- Dock layout reserves a bottom region before sizing Easy/Advanced panels. It refuses dock mode below 840 logical pixels in editor height and offers the pop-out. Resize handle clamps 160–420. The host editor is not resized by opening the board.
- BoardCanvas reads model.getOrder(), retains PedalView identity across reorder/undo, and submits live drag Move commands at most eight times a second in one model transaction. Native controls remain separate from draggable face surfaces. Keyboard reorder, duplicate, bypass, delete and undo/redo are available.
- Browser search/category filtering reads the real PedalRegistry; Add, Enter, double click and drag/drop create real model instances. Capacity disables Add; model rejections appear in the status bar.
- Descriptor parameter controls include enum labels, natural-unit text, double-click defaults, native keyboard/accessibility, and per-slot enable/placement/lane/trims/MIDI channel/profile/mounting. Unknown models get a removable placeholder. Three board APVTS attachments bind enabled, universal mix, tempo; model commands control structural edits. Host/MIDI owner mirrors slot controls by stable storage slot.
- Seven deterministic procedural board surfaces and seven mounting types exist, with high-contrast fallback. Faces use original procedural workshop enclosure art and descriptor labels, with “Inspired by” text and an explicit EST research status. There are no manufacturer logos/photos/manual excerpts.
- Face images use a process-shared bounded 96 MiB LRU keyed by model/revision/size/DPI/palette/high contrast. Board zoom, Ctrl-wheel/pinch and scrolling are native. Cables have directional chevrons and a labelled feedback arc.
- MIDI learn arms the actual router, supports its CC/note/aftertouch/pitch-bend capture types, cancels on hide/Escape, times out after 20 seconds, and writes instance UUID + exact parameter ID bindings through model commands. Settings exposes existing mappings in asynchronous editors with channel/source/range/curve/mode/pickup/clear controls.
- Window rectangle, dock height/open, zoom, scroll and cable visibility write through BoardModel::setUiState; nested unknown UI properties are retained.

## API errata approved by coordinator

- Additive BoardWindow::setBoardContent(BoardRoot&) accepts non-owned content. Drawer owns and outlives the root; editor destroys/detaches BoardWindow first. Existing frozen constructor and headless hook remain.
- BoardModel::setUiState(juce::var) is implemented by the engine owner. Presentation-only edits neither enter audio publication nor undo history.

## Automated tests and current evidence

| Test | Contract | Status |
|---|---|---|
| RefPedalUi.geometryKeepsTitlebarReachable | Invalid/offscreen/oversize geometry clamps to work area | Test written; runtime pending shared test build |
| RefPedalUi.viewUsesModelOrderAndKeepsIdentity | Reorder and undo retain component identity and model-derived visual order | Test written; runtime pending |
| RefPedalUi.rejectedEditIsVisible | Failed command updates visible status text | Test written; runtime pending |
| RefPedalUiStyles.sevenSurfacesAreDeterministicAndDistinct | Pixel hashes repeat and differ across styles | Test written; runtime pending |
| RefPedalUiStyles.highContrastAndMixedMountingsPaint | Flat theme and mixed mounts render without exception | Test written; runtime pending |
| RefPedalUiFaces.descriptorsHaveRealFacesAndControlMappings | Every registered descriptor has non-null face, jacks, footswitch and parameter index mapping | Test written; runtime pending |

Initial red evidence: the geometry test's requested clampWindow API was absent (assertion recorded before implementation). Compilation currently verified with GCC C++17 `-fsyntax-only` for PedalView, BoardCanvas, BoardUiState, BoardSurfacePainter, GenericFace, PedalFaceCache, PedalBrowser and BoardMidiPanel. Remaining hub compilation and runtime results will be appended after integration. `git diff --check` passes.

## Remaining finite acceptance gates (unfinished)

1. T-UI-WINDOW/FALLBACK/DOCK/NONBLOCK native host matrix: repeated close/reopen/editor destruction, actual denied-peer fallback, screen/DPI changes and keyboard focus. Session open restoration is implemented but untested. Dock refusal at short editor heights is a deliberate partial fallback: no small-height overlay is used.
2. T-UI-ORDER-AUDIO, 10k operation stress, 16-pedal frame budget and idle repaint measurements. No animation timers run in the board; the only timer is armed MIDI capture polling and is allow-listed. Drag ghost, animated gaps, cancellation/edge auto-scroll/cross-AMP drop are unfinished.
3. Full per-model physical footprints/front-panel layouts and 28 dedicated model face artworks/looper live LED rings/LCD/BPM indicators. Current descriptor cards expand vertically to keep all controls reachable. They are functional fallback faces, not finished WP20/WP21 visual acceptance.
4. Full photorealistic surface/mount recipes, LOD0/1/2, wear marks, loose deterministic pose jitter, golden image baselines and DPI seam checks. Current procedural surfaces/mounts are functional but simplified.
5. Complete Series/Parallel/AB/Loop routing illustration with AMP markers, lane splits/joins, loop-send/return and feedback fault/watchdog readback. Current adjacent signal cables and feedback arc do not certify complex routing accuracy.
6. Board mix MIDI learn, conflict-resolution prompt, mapped badges, focused-control L learn, accessibility announcements and screen-reader parity must be checked/completed. Mapping capture success must be tested through the actual audio/MIDI router and state reload, including reordered/deleted instances.
7. Loop/feedback configuration widgets, keep-on-top menu and settings inspector coverage. Current settings includes scalar board, MIDI, lane, A/B and reamp settings; loop/feedback commands are not yet fully exposed.
8. GuiReach registration must populate real reference slots and traverse the dock view. Only mapped descriptor lanes are meaningful; reserved empty automation lanes must not be represented by dummy sliders. Host parameter count/order append-only checks remain with host owner/coordinator.
9. View-state write debouncing and session/per-monitor/maximised geometry metadata are incomplete. Unknown fields are preserved, but writes currently occur on each view change and close/destruction rather than through a 500 ms debounce.
10. Full project tests, ASan/UBSan/TSan, pluginval/clap-validator, Linux/macOS/Windows and DAW screenshots remain required. No runtime, host, performance, legal or accessibility sign-off is claimed at this checkpoint.
