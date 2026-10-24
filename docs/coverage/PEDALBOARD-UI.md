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
2. T-UI-ORDER-AUDIO rendered-audio comparison, full-board40-cable/1.5x/dirty-area performance gate. The10k seeded Canvas stress, drag ghost, E53 cancellation, edge scroll and tagged placement drops now pass focused tests. Animated gaps, touch dwell and trash drops remain unfinished. Native idle observation is recorded below; DAW/hardware CPU remains a separate gate.
3. Named DSP descriptor integration and live looper readback/rings/LCD/BPM. All28 static procedural profiles and physical mm layout are implemented, but named fixtures do not certify unregistered DSP models. Actual front controls and scrolling extended controls are tested for the integrated generic/prototype registry.
4. Full photorealistic surface/mount recipes, LOD0/1/2, wear marks, loose deterministic pose jitter, golden image baselines and DPI seam checks. Current procedural surfaces/mounts are functional but simplified.
5. Complete Series/Parallel/AB/Loop routing illustration with AMP markers, lane splits/joins, loop-send/return and feedback fault/watchdog readback. Current adjacent signal cables and feedback arc do not certify complex routing accuracy.
6. Board-mix learn is implemented; conflict-resolution prompt, mapped badges, focused-control L learn, accessibility announcements and screen-reader parity must be checked/completed. Mapping capture success must be tested through the actual audio/MIDI router and state reload, including reordered/deleted instances.
7. Loop/feedback settings, slot-channel profile toggle and keep-on-top are now exposed. Their backend acceptance and real host behavior require tests; unavailable backend commands correctly report rejection.
8. GuiReach registration must populate real reference slots and traverse the dock view. Only mapped descriptor lanes are meaningful; reserved empty automation lanes must not be represented by dummy sliders. Host parameter count/order append-only checks remain with host owner/coordinator.
9. Root zoom/scroll/cable state uses a 500 ms change debounce and immediate close/destruction flush. Window movement geometry still writes on each move. Per-monitor/maximised metadata and DPI-change reclamping remain incomplete.
10. Full project tests, ASan/UBSan/TSan, pluginval/clap-validator, Linux/macOS/Windows and DAW screenshots remain required. No runtime, host, performance, legal or accessibility sign-off is claimed at this checkpoint.

## Additional known event-delivery limit

Trigger/Momentary buttons send actual down/up values. Engine checkpoint950dbf93 supplies the bounded edge queue, with separate engine tests for rapid pulses. UI `looperMainFootswitchSendsRealEdgesWithoutBypassAlias` verifies the main physical button writes1 then0 and does not change slot enable. Audio edge ordering remains certified by engine evidence, rather than inferred from a slider or UI-only test.

## Selected MIDI focus integration

The coordinator-approved E43 `bool BoardModel::setMidiFocus(const InstanceId&)` is called when the pedal group or its child controls receive focus. It uses the stable instance UUID, not visual position. `RefPedalUi.focusedPedalSelectsMidiTargetWithoutUndoStep` checks selected MIDI truth and verifies focus adds no undo step. This integration depends on engine checkpoint d1752db1 and its approved dependencies.

Latest focused rebuild: 11 tests / 80 checks / 0 failures. Native controls now refresh from model truth when a command is rejected; the regression test first reproduced the stale toggle state and then passed after rebuilding the Canvas rejection handler. The failing regression log and final green log are retained with the harness evidence. Root destruction disconnects Canvas focus/learn callbacks before its viewport members are destroyed.

## WP20/WP21 procedural face profiles (continued implementation)

E49 geometry interpretations approved by coordinator: Green Russian is W69.9 × D114.3 mm. Original spec said “portrait” but listed 114.3 ×69.9; its x18/35/52 controls identify width69.9. RC5 physical footprint is73 ×129; external CTL widgets belong in the inspector rather than widening the board object to110mm. Explicit MXR landscape111 ×60 layouts remain unchanged.

Eighteen named faces now have original vector enclosure profiles, individual colours, physical dimensions, title/legend positions and suffix-to-exact-descriptor control mappings from the per-model section7. The ten generic profiles use their specified Compact/Standard/Wide/Treadle footprints and physical controls; extended parameters belong in the inspector. Artwork fixture tests are explicitly not DSP validation: `RefPedalUiFacesDrive.namedFacesUseSpecifiedMillimetres`, `frontControlsMapByExactDescriptorIdentity`, `RefPedalUiFacesMod.namedFacesKeepPhysicalFootprints`, `physicalLooperFootswitchIsRealMomentaryControl`: 4 tests /66 checks /0 failures. The initial geometry regression produced19 failed dimension checks before profiles were implemented. Existing11-test/80-check evidence is preserved separately. Physical board layout/inspector integration and actual live looper readback are subsequent slices, so this profile checkpoint alone does not certify them.

## Physical board controls and host automation slice

Physical component bounds now use each face's actual millimetres at 1.6 logical px/mm. Front controls are positioned by exact descriptor identity; every remaining parameter is a real labelled native control in a scrolling inspector. The inspector is owned by its pedal, nonmodal, closes on outside click/Escape, and detaches before model removal. Evidence/info captions sit beneath compact enclosures rather than covering physical controls. Main looper momentary controls send actual down/up; bypass is separately available in the inspector when the main footswitch is occupied.

Mapped slot enable, physical lanes and input/output trims now use APVTS attachments with stable storage-slot IDs. Configuration/ext/trigger/momentary controls remain model commands where excluded from host lanes. Attached controls avoid duplicate model writes; the host service owns persistence. Custom knob key/wheel edits send bounded begin/end gestures, while native slider drag/text and selector/button attachments retain JUCE gestures. Natural-unit parsing and physical defaults are restored after attachment construction.

New focused tests: `canvasUsesPhysicalFaceSize`, `inspectorIsNonmodalAndClosesWithItsPedal`, `everyDescriptorHasARealFaceOrInspectorControl`, `physicalLaneAttachmentsAndConfigurationUseRealTargets`. Coverage traverses the actual generic/prototype registry in this frozen snapshot plus explicit named artwork fixtures; reserved empty host lanes have no dummy controls. This slice does not certify unintegrated named DSP descriptors or full host automation playback. The initial attachment test exposed reentrant construction through a bypass initial update; its refresh guard regression is retained in the test.

Focused physical-control result: **19 tests /539 checks /0 failures**, including APVTS normalized values, actual enum configuration, native host feedback and begin/end gestures for custom keyboard/wheel editing. Exact build/main/results are retained in `/workspace/luthier-evidence/pedalboard-ui/physical-controls/`.

## Live drag, rollback and seeded stress slice

DragController now owns an actual model transaction with80ms target dwell and125ms commit spacing (at most8/s), immediate final commit, and an apply-on-drop preference read from `ui.liveReorder`. Canvas uses a lifted snapshot ghost above its separate cable overlay, retains grab offset, dims the original, and scrolls at viewport edges on a16ms timer active only during drag. Reduced motion removes lift scaling. Escape/hide/destruction cancel through approved E53 `cancelTransaction()`; rollback preserves preexisting undo/redo. PRE/LOOP/POST drop bands change placement and Move inside the same gesture. The persisted order remains a single strip with placement tags; no canonical sorting is introduced. Animated gap easing, velocity tilt, touch dwell and trash drop remain unfinished.

`dragDwellThrottleFinalCommitAndUndo`, `cancelledDragPreservesExistingRedoAndApplyOnDrop`, and `canvasGhostCancellationAndPlacementDropUseOneTransaction` cover timings, final index, placement, grouped undo, existing redo, apply-on-drop, actual ghost cleanup and alpha restoration. `tenThousandSeededOperationsKeepModelAndViewCoherent` executes10,000 operations with seed0x4c555448 against actual Canvas/model: drag/rollback, parameter/enabled/mount/placement, move, undo/redo, remove/add and UI persistence; surviving UUID component identity, physical order and focus order are checked after every operation. Actual removal followed by redo may recreate its component; identity is required only while the instance remains present. Run:24 tests /673,618 checks /0 failures; stress1757.12ms.

`cachedSixteenPedalPaintBudget` renders300 native1100x300 viewport snapshots with16 real generic pedals and17 real series cables: p954.481ms, p995.253ms; shared static face cache49,536 bytes, unchanged after warmup and below96MiB. This is an O0 focused harness observation, not the separate40-cable/full-board/1.5x/dirty-area gate. Canvas now refreshes only changed instances for parameter notifications and skips unchanged layout settings.

A real native-window Xvfb idle probe failed before measurement with X_ChangeProperty BadAtom(Atom0), reproduced with native and custom titlebars. No idle CPU/zero-paint success is claimed; the exact driver/error are retained. Offscreen actual BoardRoot PNGs exist for7 styles at1280x800, plus2x and inspector images; they are unattended renderer evidence, not manual host signoff. Exact focused build/main/red/results are under `/workspace/luthier-evidence/pedalboard-ui/drag-stress/`; visual artifacts under `.../visual/`.

## Compact control visual review and native idle evidence

The before/after actual1280x800 BoardRoot PNGs exposed clipped permanent value fields and non-ASCII literal encoding. Compact physical controls now show actual value pointers and selected mini-switch positions; natural values use real hover readouts and an owned nonmodal text entry opened by Enter/right click, with bounded host gestures and real normalized/model updates. The corresponding regression `compactKnobHasUnclippedNaturalValueEditor` verifies natural-unit entry, no modal component, and cleanup on removal. Inspector trim/lane labels are explicit and its viewport background is opaque. MODE is inside the compact card, FEEDBACK is purposefully abbreviated FDBK with the full accessible/tooltip name, BPM has enough width, and the scrolling toolbar reveals the full focused control. Root focus scrolls to the actual focused child at small sizes; resize closes stale inspectors.

Before PNGs remain at `.../visual/`; corrected7-style native-render PNGs, inspector,2x2560x1600 images,480x260 minimum image and contact sheet are at `.../visual/after/`. These are unattended renderer artifacts. ExistingFileOutputStream appends by default, so regeneration now deletes only the generated destination file before writing; after images are single PNG streams.

The native peer idle failure was resolved using the repository's existing QualityModeUiTests `PreInternWindowManagerAtoms` semantics before JUCE initialization; no product error suppression or window settings changes were needed. Actual native Xvfb DocumentWindow with BoardRoot and16 real generic pedals:750ms settle then1000ms observation, **Canvas0 paints; processCPU0.7419%**. Exact native driver/results/build are retained at `/workspace/luthier-evidence/pedalboard-ui/idle/`; original BadAtom error retained separately. This is one unattended Linux/Xvfb observation, not a DAW/hardware benchmark.

## Actual pedal telemetry slice (E47)

BoardRoot now supplies `Board::readPedalMeters(storageSlot)` to a policy-controlled30Hz LiveReadout poll. Hidden views stop via AnimationPolicy, reduced quality caps polling, no source creates no timer, and identical snapshots do not repaint. PedalView rejects any present snapshot whose fullUUID differs from the instance. Only available transport/loop/tempo/fault fields are shown. Actual audio enabled/tail data drive LEDs; the RC1 ring derives segment count from actual playhead/length; RC5 has a compact real transport/BPM/position display and full inspector summary. No clock or transport phase is inferred from wall time. Fault/transport changes notify accessibility value changes. Missing or unavailable payloads remain explicit pending/no-readback states.

`actualMetersRequireFullIdentityAndAvailability` tests wrong UUID, available-field gating, unchanged readings, transport/BPM/fault text and disappearance. `looperMainFootswitchSendsRealEdgesWithoutBypassAlias` validates actual momentary1/0 writes and unchanged enabled state.27 focused tests /673,648 checks /0 failures. This depends on approved engineE47 and E53; the named loopers are still unregistered in this isolated snapshot and their owners have not yet published all payloads. `visual/after/looper_artwork_fixtures{,_2x}.png` explicitly banner their unregistered fixture status with no fabricated live progress. Descriptor arrays in the fixture driver now have whole-driver lifetime, as required by the registry's non-owning descriptor pointers.
