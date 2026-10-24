# Reference pedalboard UI acceptance ledger

Owner WP17–22. Implementation checkpoint, 2026-10-24. This is a partial implementation of the complete UI_BOARD_STYLES acceptance contract, not a release sign-off.

## Implemented paths

- The editor PEDALS toggle and Ctrl/Cmd+Shift+B open a non-modal native BoardWindow. Window X synchronises the toggle. A single Drawer-owned BoardRoot is reparented into the window and back into the dock without transferring ownership. The editor destroys the window before the Drawer/root. No modal loop is used.
- Dock layout reserves a bottom region before sizing Easy/Advanced panels. It refuses dock mode below 840 logical pixels in editor height and offers the pop-out. Resize handle clamps 160–420. The host editor is not resized by opening the board.
- BoardCanvas reads model.getOrder(), retains PedalView identity across reorder/undo, and submits live drag Move commands at most eight times a second in one model transaction. Native controls remain separate from draggable face surfaces. Keyboard reorder, duplicate, bypass, delete and undo/redo are available.
- Browser search/category filtering reads the real PedalRegistry; Add, Enter, double click and drag/drop create real model instances. Capacity disables Add; model rejections appear in the status bar.
- Descriptor parameter controls include enum labels, natural-unit text, double-click defaults, native keyboard/accessibility, and per-slot enable/placement/lane/trims/MIDI channel/profile/mounting. Unknown models get a removable placeholder. Momentary/trigger descriptors use real press/release buttons; sliders implement 1% wheel/keys, Shift 10% keyboard and fine drag, Command/Ctrl 0.1% keyboard steps, and natural-unit text parsing. Three board APVTS attachments bind enabled, universal mix, tempo; model commands control structural edits. Host/MIDI owner mirrors slot controls by stable storage slot.
- Seven deterministic procedural board surfaces and seven mounting types exist, with high-contrast fallback. Faces use original procedural workshop enclosure art and descriptor labels, with “Inspired by” text and an explicit EST research status. There are no manufacturer logos/photos/manual excerpts.
- Face images use a process-shared bounded 96 MiB LRU keyed by model/revision/size/DPI/palette/high contrast. Board zoom, Ctrl-wheel/pinch and scrolling are native. Cables have directional chevrons and a labelled feedback arc.
- Board-mix and pedal MIDI learn arm the actual router, supports its CC/note/aftertouch/pitch-bend capture types, cancels on hide/Escape, times out after 20 seconds, and writes instance UUID + exact parameter ID bindings through model commands. Settings exposes existing mappings in asynchronous editors with channel/source/range/curve/mode/pickup/clear controls.
- Window rectangle, dock height/open/mode, zoom, scroll and cable visibility write through BoardModel::setUiState; nested unknown UI properties are retained.

## API errata approved by coordinator

- Additive BoardWindow::setBoardContent(BoardRoot&) accepts non-owned content. Drawer owns and outlives the root; editor destroys/detaches BoardWindow first. Existing frozen constructor and headless hook remain.
- BoardModel::setUiState(juce::var) is implemented by the engine owner. Presentation-only edits neither enter audio publication nor undo history.

## Automated tests and current evidence

| Test | Contract | Status |
|---|---|---|
| RefPedalUi.geometryKeepsTitlebarReachable | Invalid/offscreen/oversize geometry clamps to work area | PASS focused native/Xvfb |
| RefPedalUi.viewUsesModelOrderAndKeepsIdentity | Reorder and undo retain component identity and model-derived visual order | PASS focused native/Xvfb |
| RefPedalUi.rejectedEditIsVisible | Failed command updates visible status text | PASS focused native/Xvfb |
| RefPedalUiStyles.sevenSurfacesAreDeterministicAndDistinct | Pixel hashes repeat and differ across styles | PASS focused native/Xvfb |
| RefPedalUiStyles.highContrastAndMixedMountingsPaint | Flat theme and mixed mounts render without exception | PASS focused native/Xvfb |
| RefPedalUiFaces.descriptorsHaveRealFacesAndControlMappings | Every registered descriptor has non-null face, jacks, footswitch and parameter index mapping | PASS focused native/Xvfb |

Focused runtime command: `LD_LIBRARY_PATH=/workspace/luthier-toolchain/sysroot/usr/lib/x86_64-linux-gnu xvfb-run -a /tmp/luthier-ui-tests/tests`. Result: **11 tests, 80 checks, 0 failures**, including additional `stateRoundTripRetainsUnknownFields`, `keyboardReorderUsesOneUndoStep`, `keyboardKnobUsesFineAndCoarseSteps`, `focusedPedalSelectsMidiTargetWithoutUndoStep`, and `rejectedEditRestoresNativeControlState`. The focused target compiles all application translation units from the UI branch; only matching pure JUCE module objects are reused. Registry coverage in this isolated branch is three registered prototypes; full 28-model integration remains a separate gate. Fontconfig emitted a cache-directory warning in this restricted container. Initial fixture failure (a temporary var released its raw DynamicObject) was fixed by retaining the owning var; rerun passed.

Exact harness, driver main and build/result logs are retained at `/workspace/luthier-evidence/pedalboard-ui/`.

Initial red evidence: the geometry test's requested clampWindow API was absent (assertion recorded before implementation). Compilation currently verified with GCC C++17 `-fsyntax-only` for PedalView, BoardCanvas, BoardUiState, BoardSurfacePainter, GenericFace, PedalFaceCache, PedalBrowser and BoardMidiPanel. BoardRoot, BoardWindow, MappingCallout, Drawer and PluginEditor also pass complete GCC C++17 syntax checks. Runtime results above apply to the focused target, not the full project. Original CRLF/LF file endings are retained with scripts/fixeol.sh.

## Remaining finite acceptance gates (unfinished)

1. T-UI-WINDOW/FALLBACK/DOCK/NONBLOCK native host matrix: repeated close/reopen/editor destruction, actual denied-peer fallback, screen/DPI changes and keyboard focus. Session open restoration is implemented but untested. Dock refusal at short editor heights is a deliberate partial fallback: no small-height overlay is used.
2. T-UI-ORDER-AUDIO, 10k operation stress, 16-pedal frame budget and idle repaint measurements. No animation timers run in the board; the only timer is armed MIDI capture polling and is allow-listed. Drag ghost, animated gaps, cancellation/edge auto-scroll/cross-AMP drop are unfinished.
3. Full per-model physical footprints/front-panel layouts and 28 dedicated model face artworks/looper live LED rings/LCD/BPM indicators. Current descriptor cards expand vertically to keep all controls reachable. They are functional fallback faces, not finished WP20/WP21 visual acceptance.
4. Full photorealistic surface/mount recipes, LOD0/1/2, wear marks, loose deterministic pose jitter, golden image baselines and DPI seam checks. Current procedural surfaces/mounts are functional but simplified.
5. Complete Series/Parallel/AB/Loop routing illustration with AMP markers, lane splits/joins, loop-send/return and feedback fault/watchdog readback. Current adjacent signal cables and feedback arc do not certify complex routing accuracy.
6. Board-mix learn is implemented; conflict-resolution prompt, mapped badges, focused-control L learn, accessibility announcements and screen-reader parity must be checked/completed. Mapping capture success must be tested through the actual audio/MIDI router and state reload, including reordered/deleted instances.
7. Loop/feedback settings, slot-channel profile toggle and keep-on-top are now exposed. Their backend acceptance and real host behavior require tests; unavailable backend commands correctly report rejection.
8. GuiReach registration must populate real reference slots and traverse the dock view. Only mapped descriptor lanes are meaningful; reserved empty automation lanes must not be represented by dummy sliders. Host parameter count/order append-only checks remain with host owner/coordinator.
9. Root zoom/scroll/cable state uses a 500 ms change debounce and immediate close/destruction flush. Window movement geometry still writes on each move. Per-monitor/maximised metadata and DPI-change reclamping remain incomplete.
10. Full project tests, ASan/UBSan/TSan, pluginval/clap-validator, Linux/macOS/Windows and DAW screenshots remain required. No runtime, host, performance, legal or accessibility sign-off is claimed at this checkpoint.

## Additional known event-delivery limit

A Trigger/Momentary button sends actual down/up values. A press and release completed before audio adopts the intermediate topology can be coalesced by the current backend. The engine owner has been notified; reliable rapid pulse delivery requires an event queue. This is not certified by the UI tests.

## Selected MIDI focus integration

The coordinator-approved E43 `bool BoardModel::setMidiFocus(const InstanceId&)` is called when the pedal group or its child controls receive focus. It uses the stable instance UUID, not visual position. `RefPedalUi.focusedPedalSelectsMidiTargetWithoutUndoStep` checks selected MIDI truth and verifies focus adds no undo step. This integration depends on engine checkpoint d1752db1 and its approved dependencies.

Latest focused rebuild: 11 tests / 80 checks / 0 failures. Native controls now refresh from model truth when a command is rejected; the regression test first reproduced the stale toggle state and then passed after rebuilding the Canvas rejection handler. The failing regression log and final green log are retained with the harness evidence. Root destruction disconnects Canvas focus/learn callbacks before its viewport members are destroyed.
