# UI_BOARD_STYLES: pedalboard GUI + board-style spec (dev-ready)

Ver 1.1 | 2026-10-21 | Status SPEC (reconciled; IMPLEMENTATION_PLAN sec 1 wins on conflict). Engine side: BOARD_CORE.md (BoardModel, BoardCommand, Topology, sec 14 UI contract). Pedal faces: per-model specs. Theme: `Source/UI/Theme.*`. Namespace `luthier::reference`, files under `Source/UI/ReferencePedals/` (sec 17).
Tags: [DOC]=repo fact, [EST]=design choice, tune by profiling/eye. All visual numbers = [EST] unless tagged. Pure presentation: zero audio effect (BOARD_CORE sec 14), except `loose` cosmetic jitter (sec 9.7).
Legal: all art = original procedural vector (juce::Path/Graphics). No bitmaps, no manufacturer photos/logos. Brand names only as text "inspired by".

## 0. Ground facts [DOC]
- Existing prototype: `Source/UI/ReferencePedalDrawer.{h,cpp}` (`reference::Card`, `reference::Drawer`, slide-out strip, height 232). Kept as the **fallback strip** (sec 3.4); its Cards are replaced by `PedalView` (sec 6) but the Drawer shell/tests survive.
- Theme: `Palette::*` live colours (inline juce::Colour), `Palette::textured` (false = High contrast: no grain/sheen/screws/gradients), `Palette::revision` (bumps on `Palette::apply`), `Metrics::{grid=8, panelCorner=4, animationMs=80, knob*}`, `Fonts::{ui,display,mono,label,sectionHeader}`, `LuthierLookAndFeel::{drawPanel, drawCornerScrews, drawMiniToggle, drawSeparator}`.
- `PaintCache::draw(g, area, key, paintStatic, opaque)` caches static painting as Image at physical density; key must fold everything static content depends on (area, scale, `Palette::revision` already folded).
- `AnimationPolicy::get()`: `mayAnimate(Transition)`, `transitionMs(ms)`, `Registration`, `notePaint(c)` first line of paint(); poll-only timers need allow-list entry (`getPollOnlyAllowList`, CQ-22). `AccessibilitySettings::get().isReducedMotion()`.
- Screenshot tests: `LUTHIER_TEST(Suite, name)` + `Component::createComponentSnapshot` (Source/Tests/AppearanceTests.cpp, RealismUiTestsC.cpp, ThemeSweepTests.cpp). No DocumentWindow in repo yet (grep: none) => new.

## 1. Scope / goals
U1 Separate pop-out `BoardWindow` (juce::DocumentWindow) toggled from main editor; never modal, never blocks main controls.
U2 Collapsible fallback strip in main editor (reuse Drawer) when pop-out unavailable or user prefers docked.
U3 Drag-reorder with live audio-order update; add/remove via browser; per-pedal bypass footswitch; universal wet/dry knob; MIDI-learn UI; cable drawing incl. routing + feedback loops.
U4 Seven board styles + mounting graphics, user selectable, saved.
U5 60 fps with 16 pedals; keyboard + screen-reader complete.

## 2. State (UI-owned, persisted in `referencePedals` JSON, BOARD_CORE 10.1; additive keys, unknown preserved)
```
"window":"drawer|popout",                       // already in schema
"ui":{"popout":{"open":false,"x":120,"y":80,"w":1100,"h":520,"mon":"<displayId>","maximised":false},
      "dock":{"open":false,"h":232},
      "zoom":1.0,"scrollX":0.0,"scrollY":0.0,
      "showCables":true,"showBrowser":true,"snap":true,
      "mountingDefault":"auto"},                // "auto" = style default (sec 5 table); else per-instance "mounting"
"boardStyle":"plywood|velcro|clamps|routed|rail|ceramic|loose"
```
- `boardStyle` default `plywood`. Unknown string => `plywood`, original string preserved in `ui.unknownStyle`.
- Instance `mounting` (existing key): `screws|velcro|clamps|routed|rail|ceramic|none`. Absent/"auto" => style default. Per-instance override allowed (mixed boards, e.g. a clamped pedal on plywood is legal; renderer draws what the instance says; style supplies only surface + default).
- `x` (instance, existing): lateral offset in slot units for `loose` style jitter seed (sec 9.7). Seed = hash(instanceId) so layout is deterministic across sessions.
- All saved window geometry validated on load (sec 3.3). Missing/invalid => defaults.
- State write: message thread only, on change debounced 500 ms (juce::Timer one-shot) and on window close/destroy. Part of undo? No: view state excluded from undo (BOARD_CORE 10.6 applies to model edits). Style change IS undoable (`BoardCommand::SetStyle`).

## 3. Windowing
### 3.1 `BoardWindow : juce::DocumentWindow`
- Ctor: `BoardWindow(BoardModel&, LuthierAudioProcessor&, juce::Component* anchor)`; title "Pedalboard"; `setUsingNativeTitleBar(true)`; `setResizable(true,true)`; `setResizeLimits(480,260, 4096,2160)`; default 1100x520 [EST]; `setContentOwned(new BoardRoot, false)`; `setBackgroundColour(Palette::backgroundDeep)`; `setAlwaysOnTop(false)` default (option "Keep on top" in window menu, off by default; DAW floating windows often need on-top, so offer it); `setWantsKeyboardFocus(false)` on window, root takes focus.
- Owner: `LuthierAudioProcessorEditor` holds `std::unique_ptr<BoardWindow> boardWindow` + `juce::SafePointer`. Editor destructor closes it (`boardWindow.reset()`). Window lifetime must not outlive the processor reference: BoardWindow stores `juce::SafePointer<Editor>`; editor dtor first `board.removeListener`, then reset.
- Standalone and plugin both: same class. Do NOT use `JUCEApplication` features; no `setFullScreen`.
- `closeButtonPressed()`: hides (not destroys): `setVisible(false)`, `ui.popout.open=false`, toggle button state updated, geometry saved. Re-open = `setVisible(true)` + `toFront(false)`. Window object created lazily on first open, kept for session (preserves scroll/zoom/focus).
- Toggle button `PEDALS` in `HeaderBar` (ids: `ref_board_toggle`): `setClickingTogglesState(true)`, tooltip "Show/hide pedalboard (Ctrl+Shift+B)". State == window visible. ToggleStateChangeListener keeps both in sync (X button, shortcut, session restore). Command: `ApplicationCommandManager` id `refBoardToggle`, default key Ctrl/Cmd+Shift+B; if the editor has no command manager, handle in `Editor::keyPressed` and `BoardRoot::keyPressed` so shortcut works from both windows.
- Mode switch in window menu / toggle long-press: "Pop-out" vs "Docked strip". `window` state mirrors. Switching hides one, shows the other, bound to the same BoardModel (BOARD_CORE 14). Audio untouched.
### 3.2 Non-blocking rules
- Never `enterModalState`/`runModalLoop` in board UI (menus via `PopupMenu::showMenuAsync`, dialogs via `AlertWindow::showAsync`/`DialogWindow` with `launchAsync`). Pedal browser = child panel in BoardRoot, not a modal.
- Main editor controls stay fully interactive while window open. Window does not steal focus on open (`toFront(false)`), except via explicit user action key.
- No timers in the window run when hidden (`visibilityChanged` stops them; Registration unregisters).
- Window must not alter plugin editor size or `getConstrainer` of the host's editor.
### 3.3 Geometry persistence + host constraints
- Store as `getWindowStateAsString()`-independent ints (x,y,w,h in logical px) + `mon` display id string `Desktop::Displays::Display::` index/`totalArea` hash. On open: `Desktop::getInstance().getDisplays().getDisplayForRect(saved)` null or rect fully outside any `userArea` => centre on editor's display at default size. Clamp: size to `userArea` minus 32 px; position so >= 64x32 px of the title bar is inside `userArea`.
- DPI / scale change (monitor moved): handled by `Component::parentHierarchyChanged`/`displayChanged` => re-clamp.
- Plugin host constraints: some hosts (older AU/VST3 in Logic/Reaper-embedded, Linux hosts without Desktop, sandboxed) cannot create top-level peers. Detection: `isOnDesktop()` after `addToDesktop`, `getPeer()!=nullptr`. If peer fails or `juce::Desktop::getInstance().getComponent(0)==nullptr` (headless) => fall back to docked strip, show one-time info toast "Pedalboard docked: host does not allow floating windows". Also fallback if `PluginHostType` known-bad table [EST, start empty, populate from QA].
- Window is a sibling top-level, not a child of the host's window: keyboard events do not reach host. Provide `BoardRoot` focus handling (sec 12) and forward unhandled space/transport keys? NO (hosts differ); document: transport keys not forwarded; MIDI keyboard still works via plugin MIDI input.
- Mac: `setAlwaysOnTop` default false but `NSWindowLevel` floating relative to host app auto-managed by JUCE for plugin windows; test in Logic/Reaper/Live.
- Windows: use `Desktop::setKioskModeComponent` never. Linux: X11 window may not get focus on `toFront(false)`; call `grabKeyboardFocus` on first click only.
### 3.4 Docked collapsible strip (fallback + user pref)
- Reuse `reference::Drawer`: `setOpen(bool)`; collapsed height 0 [DOC `getPreferredHeight`], expanded default 232, user-resizable 160..420 via drag handle on top edge (cursor `UpDownResizeCursor`), persisted `ui.dock.h`. Strip hosts `BoardView` (sec 4) in compact mode: zoom auto-fit height, cables thin, no browser panel (browser as popup callout), style graphics drawn simplified (sec 10.3 LOD).
- Header summary chip (collapsed): "N pedals | ON/OFF | mix%" read from model, 24 px tall, click toggles.
- Must not push controls offscreen: editor reserves `dockHeight` only when open; never exceeds `displayWorkArea - editorMinHeight`; if no room => strip overlays bottom with scrollbar and remains collapsible; auto-collapse to 0 when window height < 600 [EST] and offer pop-out.
- A single `BoardView` instance exists at a time (re-parent between strip and window; `removeFromParent` + `addAndMakeVisible`), so state (focus, scroll) survives the switch.

## 4. Layout: `BoardRoot` (window content)
```
+--------------------------------------------------------------------------------+
| TOOLBAR (fixed 48px = Metrics::headerHeight): [Style v][+Add][Undo][Redo] | Mode v | [BYPASS] | MIX knob  | [MIDI Learn] [Cables] [Fit][-][zoom%][+] [Dock] |
+--------------+-----------------------------------------------------------------+
| BROWSER      |  BOARD VIEWPORT (scroll+zoom) : IN o--cable--[P1]--[P2]..--o OUT |
| 200px, hide  |   lanes stacked rows; AMP marker; loops; feedback arcs          |
| (toggle)     |                                                                  |
+--------------+-----------------------------------------------------------------+
| STATUS (20px): latency | CPU | fb warnings | MIDI last msg | learn target       |
+--------------------------------------------------------------------------------+
```
- Toolbar never scrolls (BOARD_CORE 14). Min window 480x260: browser auto-hides < 760 px wide, status hides < 360 px tall.
- Components: `BoardRoot`, `Toolbar`, `PedalBrowser`, `BoardViewport` (juce::Viewport subclass), `BoardCanvas` (content, sized by layout), `BoardSurface` (style background painter, sec 5/9; sub-region of Canvas, cached), `PedalView[n]`, `CableLayer` (transparent overlay, `setInterceptsMouseClicks(false,false)` except cable handles), `DragLayer` (drag ghost), `MidiLearnOverlay`, `StatusBar`.
- Z-order (bottom to top): BoardSurface, MountingLayer (per-pedal mount art, sec 8, drawn by PedalView's sibling `MountView` so it sits under pedal but over surface), PedalView, CableLayer, MidiLearnOverlay, DragLayer, toasts.
### 4.1 Board geometry (logical px at zoom 1.0)
- Pedal cell: `cellH = 240`. FOOTPRINT RULE (reconciles GENERIC_PEDALS 0.4 and model specs): physical size in MILLIMETRES (generic units in GENERIC_PEDALS 0.4 are mm; model specs give mm), `px = mm * 1.6` at zoom 1.0 (BOSS compact 73x129 mm -> 117x206; generic Standard 66x112 -> 106x179; Wide 90x112 -> 144x179; Compact 54x90 -> 86x144; wah treadle 54x150 -> 86x240 = cellH; two-footswitch Muff ~145x120 -> 232x192). `PedalFace::designSize` default 120x200 (compact) / 150x230 (wide) applies ONLY to models with no stated mm size.
- Gap between pedals: `kPedalGap = 56` (cable room) [EST]; edge margin 48; lane height 300 (pedal + cable gutter 60).
- Canvas content width = `margin*2 + sum(pedalW) + gap*(n-1)` + AMP marker 72 + IN/OUT jacks 64 each. Height = lanes*300 + loops row(s).
- Board plate (the physical board) extends 24 px beyond pedals (style rim). Min plate 6 slots wide so empty board looks like a board; "empty slot" ghost outlines (dashed, `Palette::edge` @0.4) at next-free positions in browser-drag mode.
### 4.2 Zoom / scroll
- Zoom 0.5..2.0 step 0.1; `Ctrl/Cmd+wheel` zooms around cursor, pinch via `mouseMagnify`; "Fit" = fit all pedals in width (min 0.5); persisted. Implementation: canvas `setTransform(AffineTransform::scale(z))` on `BoardCanvas` (children laid out at 1.0). Hit-testing automatically correct. Rendering note: transformed Component repaints are not cached by Image at the transform scale; cache keys include zoom (sec 10).
- Horizontal scroll: wheel/shift+wheel, drag on empty surface with middle button or Space+drag (hand tool), scrollbars always shown (accessible). Vertical when lanes > 1. `Viewport::setScrollBarsShown(true,true)`; `setScrollBarThickness(12)`.
- Auto-scroll to focused/selected/dropped pedal (`viewport.setViewPosition` animated via `ComponentAnimator` 120 ms, snap if reduced motion).
- Edge auto-scroll while dragging near viewport edge (<48 px): 600 px/s ramp, timer 16 ms active only during drag.

## 5. Board styles (user selectable)
Selector in toolbar: `Style` ComboBox/popup with 7 swatch thumbnails (48x32 procedural mini-render of surface, same draw fn at LOD0). Command `SetStyle(style)` undoable; crossfade surface 250 ms (or instant when reduced motion/animation Off).

| id | name | surface | default mounting | rim/frame | typical wear |
|---|---|---|---|---|---|
| plywood | Plywood board | birch plywood, light | screws | bare edge + 2 mm bevel, rubber feet | grain, tape marks |
| velcro | Velcro board | dark carpet-felt over plywood | velcro | black tape edge | lint, fibre noise |
| clamps | Clamp board | brushed aluminium plate | clamps | folded flange | scratches |
| routed | Routed block | thick walnut/oak block | routed | chamfered thick edge | stain, tool marks |
| rail | Rail frame | open tubular metal framework, floor visible between rails | rail | tube frame | chrome sheen |
| ceramic | Ceramic tile | glazed ceramic tiles with grout | ceramic | tile border + bevel | glaze crazing |
| loose | Loose on floor | stage floor (dark boards or carpet) | none | none | scuffs, gaffer tape, tangled cables |

Mixed mount rule: the `mounting` of each instance is drawn regardless of style. `Mounting` enum values: `screws, velcro, clamps, routed, rail, ceramic, none`. Style `ceramic` default mounting `ceramic` = "adhesive pads/silicone dots" (see 8.6).
Accessibility name per style (screen reader): "Plywood board", ... Description = default mounting phrase.

## 6. PedalView
- `class PedalView : public juce::Component, public juce::DragAndDropTarget(optional)`; one per `instanceId` (key = uuid, reused across reorder: never destroyed on move, BOARD_CORE "stable across reorder").
- Child content = model-supplied `PedalFace` (per-model spec supplies `designSize`, `paintFace(g)`, control list with positions). Generic pedals use `GenericFace` templated from param descriptors: knobs in grid (<=3 per row, 48 px `Metrics::knobDefault`), toggle switches via `LuthierLookAndFeel::drawMiniToggle`, LED, footswitch.
- Parts: face (cached static, sec 10), controls (live), `FootswitchButton` (bottom, 40 px circular steel stomp; click = toggle `enabled`; state animates 60 ms depress), LED (red default; per-model colour; lit = enabled, read back from model/audio per ARCHITECTURE "feedback must reflect actual DSP state": LED = model.enabled AND not `MissingPedal`), drag handle (top 16 px grip "grab bar" or full-face long-press / drag on non-control area), jacks (IN left / OUT right, side-mounted 1/4" jack drawn at 12 px), DC jack on top (decor), title plate (brass plate `Palette::plate`).
- Models with multiple I/O (BOARD_CORE 6): extra jacks (send/return, aux, exp) drawn; they are cable endpoints (sec 11).
- Bypass: footswitch click, key `Space`/`Enter` on focused pedal, MIDI. LED + face dims (face overlay alpha 0.0 on, 0.18 off [EST], plus `Palette::textured` false => outline only).
- Selection: single click on face selects (brass 2 px focus ring, `Palette::accentBright`), selected id drives inspector/context ops. Multi-select Shift/Ctrl for group-move? v1: single only.
- Missing model (BOARD_CORE 10.1): grey face, "Missing model: <modelId>" text, bypassed, footswitch disabled, remove allowed.
- Context menu (right-click / Menu key): Bypass, Move Left/Right/First/Last, Duplicate, Reset, Remove, Mounting > [auto|screws|velcro|clamps|routed|rail|ceramic|none], MIDI Learn this pedal, Info/Provenance, Move to PRE/LOOP/POST.

## 7. Drag-to-reorder with live audio-order update
### 7.1 Gestures
- Start: `mouseDown` on grip/non-control face area, then `mouseDrag` > 4 px [EST] (`MouseDragThreshold`). Controls (knobs, switches, footswitch) never start a drag. Touch: long-press 250 ms on grip.
- Drag ghost: PedalView lifts: scale 1.04, shadow grows (blur 14, offset 10 px, alpha .55), z top, slight tilt rotation `+-2 deg` by horizontal velocity (clamp). Follows pointer with offset kept. Other pedals slide (ComponentAnimator, 140 ms ease-out, `AnimationPolicy::transitionMs(140)`) to open a gap at the insertion index.
- Insertion index = pedal-centre-x rank among fixed slot centres (hysteresis 12 px to prevent flicker).
- Cross-lane (BOARD_CORE 6.2): vertical drop selects lane band under pointer (highlight band `Palette::accent`@0.12).
- Cross-AMP drop (BOARD_CORE 6.5): dropping across AMP marker highlights target group (PRE|LOOP|POST) and on release changes `placement`; toast with Undo "Moved to LOOP (after amp)".
- Drop from browser: same machinery, ghost = face thumbnail; creates instance at index (`BoardCommand::Add(modelId, index)`).
- Drop outside board onto trash zone (toolbar trash icon, or drag past viewport bottom edge by > 40 px) => remove with undo toast.
- Esc during drag: cancel, animate back.
### 7.2 Live audio-order update (single source of truth)
- While dragging, audio order is **unchanged** until a gap is committed? Requirement: audio order == visual order always. So: each time insertion index changes (after hysteresis and dwell >= 80 ms [EST] to avoid thrashing), UI posts `BoardCommand::Move(instanceId, newIndex)` (BOARD_CORE 4: lock-free publish, 6 ms dip-through-dry crossfade per half). The user hears reorder live. Rate limit: <= 8 commits/s; coalesce last-wins; final commit on mouse-up always sent.
- Option "Apply on drop" (`ui.liveReorder`, default true; toolbar overflow) = commit only on release (for cautious users / CPU). Audio never lags visuals >1 frame after commit; visuals never ahead of the model by more than the dwell, since the PedalView positions come from the model order + drag ghost offset.
- Model is the authority: PedalView positions computed from `BoardModel::getOrder()`. Drag only sends commands; on `BoardModel::Listener::orderChanged` the layout animates to new positions. If the engine rejects (e.g. over latency cap, kMaxLatencySamp) => model unchanged, pedals snap back, toast reason.
- Undo grouping: all Move commands within one drag = single undo step (`UndoManager::beginNewTransaction` on drag start; coalesce).
- Visual order direction: left -> right = audio order in series; arrow glyph chevrons on cables (sec 11). Lanes: top to bottom lane index.
### 7.3 Keyboard reorder
`Alt+Left/Right` on focused pedal = Move +-1 (BOARD_CORE 14); `Alt+Home/End` first/last; `Alt+Up/Down` lane; `Alt+Shift+Left/Right` across AMP marker (changes placement, announces). Each = one command, one undo step, immediate (no dwell). Announce via accessibility "Moved to position 3 of 6" (sec 13).

## 8. Mounting graphics (per pedal, drawn by `MountView` under the PedalView)
Each mount is a pure function `paintMount(Graphics&, MountParams)` with `MountParams{ Rectangle<float> footprint (pedal rect on surface), float seed, Style style, float zoom, bool textured, float dropT /*0..1 settle*/, bool lifted }`. Footprint = pedal outer rect (face rect inflated 0 px). Mount art may extend beyond footprint by <= 14 px (screws, clamps) — MountView bounds = footprint inflated 16 px. Geometry in logical px at zoom 1; all lines >= 0.75 px hairlines.
Common shadow under pedal (all except loose/rail which have own): rounded-rect `footprint.expanded(0)` offset (0,3), blur via `DropShadow(Colours::black.withAlpha(0.45f), 8, {0,3})` -> prebaked into cache (no blur on paint path).
### 8.1 Screws (plywood default): "screws on each side of the pedal"
- Two metal **mounting brackets** (one on left side, one on right side, vertically centred): each = flat tab 10 x 36 px, 2 px corner, mild-steel gradient (#b9b9b4 -> #7d7d78 top-to-bottom), overlapping pedal side by 4 px (tab rests on pedal's flange; drawn above pedal bottom edge), with a **slotted/Phillips pan-head screw** at the tab's outer part: head radius 4.2 px, radial gradient light (#e4e0d6, center offset -1,-1) to (#6a665e), 0.8 px dark rim (#3a3834 @0.8), cross-recess two lines 5.2 px long 1 px #2a2824 @0.7 rotated by `seed` (random angle 0..90 deg per screw so they are not aligned), specular dot r 0.9 px #fff @0.5 at upper-left. Under-head countersink ring: ellipse r 5.4 px #000 @0.28.
- Screw hole shadow: ellipse r 4.8 offset (0.6,0.9) #000 @0.35 under head (cast shadow on wood).
- Optional second pair top/bottom for tall pedals (>= 200 px): also 2 brackets on top & bottom centered (rotated 90 deg), toggled by `ui.extraScrews` default false.
- Drop animation: screw heads spin: 270 deg rotation over 220 ms (ease-out) when pedal settles (`dropT` 0->1), tab slides in 6 px. Remove/lift: reverse, screws pop out 2 px (head scale 1.0->1.15, alpha shadow lengthens).
- Wood damage: tiny dark wood-dent ring (r 6.5) around screw hole, drawn once.
### 8.2 Velcro (hook-and-loop, "sticky")
- Under-pedal strips: 2 horizontal strips per pedal, each 70% of pedal width, height 10 px, positioned at 22% and 78% of pedal height, mostly hidden under pedal; visible edge sliver 3 px beyond pedal left/right/bottom => draw strip only protruding where it exceeds footprint (strip width = pedal width + 6 px so 3 px each side).
- Strip texture: hook side (black/dark): base #1a1a1a; 1 px stipple pattern (dots 0.6 px) grid pitch 2 px staggered, alpha .5, plus fine noise tinted #3a3a3a. Loop surface on board (carpet felt): base #232526, fibre noise: 400 random 1-2 px short strokes alpha 0.06-0.12 per 100x100 px, seeded (cached).
- Edge: strip corners rounded 1.5 px; a slightly lifted adhesive edge: 1 px lighter line #4a4a4a @0.6 along top.
- Stick/peel animation: on drop, strip edge "bites": 6 ms-cadence no; use 120 ms scale-Y squash of pedal 0.985 -> 1.0 (sec 9.2). On lift: "rip" visual: stripes darken, a short 3-frame scratchy flicker of the strip alpha (skipped when reduced motion).
- Lint decoration (textured): 6-10 tiny fibre curves (Path quad, 0.5 px, #6a6a6a @0.4) around strip, seeded.
### 8.3 Metal clamps / brackets
- Four pedal-corner **pedal-clamp brackets** or two side bars: default "side clamp pair": left and right U-bracket each 14 x 64 px, thick steel 3 px: shape = `Path`: vertical bar at the pedal side, upper & lower lips (hook) 8 px long folding over pedal's top face edge by 3 px (drawn above pedal top edge, 2.5 px thick, with 1.2 px highlight line on top edge #f2f2ee@0.7). Material: brushed zinc gradient horizontal #c8c8c4 -> #8a8a86 -> #b2b2ae; brushed micro-lines: 12 horizontal 0.5 px lines alpha .08.
- Each clamp has an **Allen/hex bolt** on the board side: hex head r 4.6 px (6-sided polygon via `Path::addPolygon(centre,6,r,rot)`), gradient #d6d6d0 -> #66665f, hex socket inner polygon r 2.2 #1a1a18 @0.8; bolt thread shadow.
- Clamp screws travel: animation on drop: clamp slides inward 10 -> 0 px (180 ms), bolt rotate 1 turn. On lift: bolt loosens, clamp slides out 6 px.
- Hex nut washer: ring r 6.0 stroke 1 px #fff@0.2.
### 8.4 Routed recesses (wooden block)
- Block surface is solid wood; each pedal sits in a **routed pocket**: pocket rect = footprint inflated +2 px (clearance), corner radius 3 px, depth illusion: inner shadow on top & left edges (linear gradient 6 px black@0.55 -> 0) and highlight on bottom & right edge (white@0.12 -> 0, 3 px) => carved look; pocket floor darker wood (#3a2616 mix, with end-grain patch) visible as 2 px gap around pedal. Router bit marks: parallel faint cut lines 0.4 px at pitch 3 px along pocket floor visible in gap (alpha .15).
- Cable channel: shallow groove 8 px wide between adjacent pockets (routed, inset look same recipe, lower contrast) used by cable segments (cables laid *in* the groove: cable draws at groove, shadow reduced).
- Drag: pocket remains as an empty "ghost recess" at the old index after drag until Move commits; then pockets are re-computed in the Canvas layout (animated: pocket "rubs out" crossfade 160 ms since a wooden block's pockets are fixed; document: in `routed` style, slots snap to evenly spaced fixed pockets; pocket widths vary per pedal width => on reorder pockets are re-cut with a 160 ms wipe: fine, visually cheap).
- Drop: pedal drops 2 px into pocket (scale 1.02 -> 1.0 and shadow contracts) 140 ms.
### 8.5 Rail framework (metal frame/rail)
- Surface = open frame: two long horizontal tubes (rails) running under the pedals at y1 = 30% and y2 = 78% of cell height, each 12 px diameter tube: vertical gradient chrome (#f4f4f2 top highlight 0.0, #b8b8b6 0.35, #6e6e6c 0.7, #9a9a98 1.0) + thin specular line 1 px #fff@0.8 at 22% height. End caps: rounded dome with radial gradient. Cross-members (uprights) every 280 px: 8 px tube vertical. Floor visible between rails (dark `Palette::backgroundDeep` with subtle ground shadow).
- Pedal attach: **U-bolt / rail clamps**: for each pedal 2 clamp saddles per rail? Simplify: 2 per pedal on the lower rail: small steel plate 24 x 8 px with 2 hex nuts r 3.2 px, clamping around the rail, bridging to the pedal's bottom flange (pedal bottom edge overlaps rail). Pedal bottom rests on the upper surface of rails (z-shadow along rail).
- Tube shading is cached: gradient bar drawn once and tiled.
### 8.6 Ceramic tile (base)
- Surface = tile grid: tile size 96 x 96 px [EST], grout 3 px (#5a5750 mid-grey, noise), tile base glaze colours variant per tile hash (3 variants from palette: pale blue-grey #c9d3d6, pale cream #d9d2bf, neutral white #e4e2dc), glossy: top-left diagonal highlight (linear gradient white @0.30 -> 0 over 60% of tile), bevel 2 px (top-left white@0.35, bottom-right black@0.18), craquelure: 2-5 hairline `Path` cracks per tile (random walk 3-5 segments 0.4 px #5a554a@0.25), seeded per tile index.
- Mount: **silicone adhesive dots** (clear rubber feet): 4 translucent circles r 5 px at pedal corners inset 8 px: radial gradient white@0.35 -> clear@0.08, 1 px rim black@0.15, visible only where they peek out (draw above surface and below pedal: invisible except when `lifted`). Visible mount cue: thin rubber **non-slip mat** strip 6 px beneath pedal edge sides: dark grey (#2b2b2b) rounded rect with fine ribbed pattern (vertical lines 1 px pitch 3 px alpha .25) — drawn as 3 px bleed at bottom edge.
- Drop animation: pedal settles with 2-frame micro-bounce (0.5 px) since ceramic is slippery; "squeak" none.
### 8.7 Loose on the floor ("sloppy")
- No mount hardware. Each pedal placed at jittered pose: `offsetX = hashf(seed,0) * 14 - 7` px, `offsetY = hashf(seed,1) * 24 - 12`, `rotation = (hashf(seed,2) - 0.5) * 7 deg` (about +-3.5), applied via `PedalView::setTransform` (rotation about centre) — layout slot unchanged so hit-testing and keyboard order unaffected. Deterministic from `hash(instanceId)`; stored `x` modifies `offsetX` only if user nudges (drag horizontally off slot by <= 24 px allowed in `loose` style only; snapped back for others). Drop animation: settle with 160 ms ease and a 3 deg wobble damped.
- Floor: dark wooden stage boards (#2a2018 / #231a13, plank width 120 px, gaps 2 px black) or carpet toggle variant `ui.floorVariant` boards|carpet; scuff marks (white-grey short curves alpha 0.06), tape (gaffer tape strip: 38 px wide dark #1b1b1c with fabric hatch, rotated random, 2-3 per board), cable ties.
- Contact shadow: soft ellipse-ish rounded shadow offset (4,6) blur 12 alpha .5, larger than other styles (pedal rests loosely).
- Cable tangling: see 11.5.
### 8.8 Dwell/lift visuals (all mounts)
`lifted=true` while dragging: mount art hidden / fades to 0 in 100 ms, leaving screw holes (small dark circles r 2.6 px @0.5) / velcro ghost rectangles / pocket empties / tile unchanged. On drop at new slot mount art fades in (dropT) as in each section. `AnimationPolicy` off => instant.
Persistent "wear marks" (holes) at the previous slot are cosmetic, cleared when the board is rebuilt or style changes; cap 32 marks. Marks are NOT saved.

## 9. Drawing recipes: board surfaces (procedural, seeded, cacheable)
All surfaces drawn by `BoardSurfacePainter::paint(Graphics&, Rectangle<int> area, Style, SurfaceParams)`. Deterministic RNG: `juce::Random(seed)` re-seeded per tile cell (cell = 128x128 px) so area-independent => tile cache works and partial repaint is stable. `textured=false` (High contrast) => flat fill + 1 px outline + simple 2 px mount glyphs, no noise/gradient/shadow.
### 9.1 Plywood
- Base `#d9bd8c` -> vertical gradient to `#c9a873` (top light, bottom slightly darker) 100% area. Veneer sheet: horizontal grain bands: 140 grain lines per 128 px tile height; each line: `Path` with 6 control points y = base + sin(x*f1+phase)*a1 + sin(x*f2+phase2)*a2 (f1 ~ 0.015, a1 ~ 1.2 px, f2 ~ 0.07, a2 ~ 0.4 px), colour `#8a6a3a` alpha 0.05-0.16, thickness 0.5-1.1 px; **cathedral figure**: 1 per 256x256 region, nested 6 arcs (elliptical `Path::addArc`) stroke `#8a6a3a`@0.12, spacing 6 px. Knots: 1 per ~600x600 [EST]: dark ellipse r 6-10 px radial gradient (#4a3014 centre @0.7 -> transparent) with ring grain. Plywood edge (visible at board rim): layered laminate stripes, 7 layers 1.2 px alternating `#d6b883`/`#a98650`.
- Rim: 2 mm bevel: top highlight 1 px white@0.35, bottom 1 px black@0.2, rubber feet (4 black rounded rect 18x10 under corners, drawn outside plate, bottom shadow).
- Tape marks: 2 translucent tape strips (`#e8e0c8`@0.25) near corners, plus pencil marks (alignment ticks 0.8 px `#3a3a3a`@0.4 at each pedal's mount line).
- Light model: key light from top-left; gradient overlay 45 deg white@0.10 -> black@0.10 across the plate.
### 9.2 Velcro board
- Plywood carcass visible only at the rim (black gaffer tape edge, 8 px, `#16161a`, fabric hatch of 45 deg lines alpha .06 pitch 2 px). Interior loop surface: base `#26282a`; noise: per 128 tile, 900 pixel-sized dots with random alpha .03-.10 white; plus 120 curved fibres (quad Path, length 3-6 px, alpha .05-.1, `#4a4d50`). Subtle large-scale mottling: 3 radial gradients white@0.03.
- Pre-existing strip marks: "ghost" lighter rectangles where strips previously stuck (decor, 2 per board), alpha .05.
### 9.3 Clamp board (aluminium plate)
- Base `#a5a8ab` -> `#8d9093` vertical; brushed: 260 horizontal hairlines per tile with varying length 40-128 px, alpha 0.03-0.10 alternating white/black. Large sheen: diagonal linear gradient white@0.18 band at 35% width. Perforation grid optional: M5 holes pitch 40 px grid (tiny dark ellipses r 2.4 px with bevel ring) in strips along rail lines where clamps bolt (drawn at 40 px pitch under clamp lines only) => suggests T-slot channels.
- Edge flange: 6 px folded edge lighter `#c8cbce`, 1 px black@0.3 line.
### 9.4 Routed block
- Wood: walnut `#5a3a22` base, ring grain: 90 near-horizontal wavy lines per 128 px, colour `#2a170a` alpha .08-.2, `#8a5e38` alpha .06 interleaved for contrast; end-grain concentric ring decorative area on the block's front face (bottom 18 px band): `Path::addEllipse` rings pitch 3 px alpha .1; oil sheen: top-left white@0.08 gradient. Edge chamfer 6 px (top highlight `#a0714a`@0.5, side `#2a170a`@0.5).
- Tool marks per sec 8.4. Chunky: block thickness side shown as bottom rim 14 px darker band.
### 9.5 Rail frame
- Floor underlay `Palette::backgroundDeep` + radial vignette black@0.35 at corners; frame tubes per 8.5. Hex-head bolts at every tube junction (r 3.6 px, as 8.3 recipe). Rubber feet: 4 cylinders at frame corners (dark grey r 9 px).
### 9.6 Ceramic: per 8.6. Border: raised bevel tile edge 10 px `#f0eee8` with 1 px shadow.
### 9.7 Loose floor: per 8.7. Plank grain light (30 lines per plank) so floor is calm.
### 9.8 Theming of surfaces
Surface colours above are *material* colours and do NOT follow `Palette` accent swap; they are blended with palette luminance only for: (a) `Palette::textured=false` (flat `Palette::panel`), (b) Light palette: surface luminance clamped so board never brighter than 0.78 L, (c) colourblind palettes: no change (materials, not status colours). Status colours (LED, warnings, cable highlights) always from `Palette::{accent, success, warning, clip, secondary}`. Text/plates: `Palette::plate/plateText`, `Palette::textPrimary`.

## 10. Rendering / performance
### 10.1 Targets
60 fps while dragging and during animation with 16 pedals + 40 cables at zoom <= 1.5 on reference machine (4-core 3 GHz, 1080p) [EST]; < 3% UI-thread CPU idle (no continuous animation when idle); no allocation in paint hot paths after first paint (preallocated Paths/Fonts); window hidden => zero timers.
### 10.2 Caching (all use `PaintCache::draw`, key folds the listed fields; `Palette::revision` + physical scale folded automatically)
| layer | cached image | key fields | repainted when |
|---|---|---|---|
| BoardSurface | one Image per 512x512 tile (virtualised: only tiles intersecting viewport clip are built, LRU 64 tiles ~ 64 MB at 4x? limit total 48 MB, evict oldest) | style, seed, tileX, tileY, zoomBucket(0.5,0.75,1,1.5,2), textured | style/palette/zoom bucket changes |
| MountView | one Image per mount per pedal footprint (static art, w/o animation state) | style/mounting, footprint size, seed | pedal size/mounting change |
| PedalView face | one Image per (modelId, rev, face size, textured) shared across instances through `FaceImageCache` singleton keyed by those | modelId, rev, size, scale | palette/zoom bucket |
| Cables | NOT cached (dynamic) except static cable *shadows* in segment image? (no: stroked per frame, cheap, see 11.6) | n/a | on model/layout/drag |
| Knob values / LEDs / meters | live, painted on top of cached face | | per change |
- Zoom bucket: cache rendered at nearest bucket >= current zoom * deviceScale; between buckets the cached image is scaled by the transform (acceptable blur, re-rasterised after 150 ms idle via `Timer::callAfterDelay`).
- Cache images opaque RGB for surface (no blend); pedal faces ARGB.
- Image memory budget total UI cache 96 MB hard cap; over cap => drop mount/face caches LRU, downgrade LOD (10.3).
### 10.3 LOD
- LOD0 full (zoom >= 0.8): grain, noise, specular, per-screw detail.
- LOD1 (0.5-0.8 or > 12 pedals or low quality mode `QualityLevel` from cpu-quality-modes): drop noise/fibre/craquelure, simplified gradients, screws as flat discs.
- LOD2 (strip dock compact, thumbnails, `textured=false`): flat colours, outline glyph mounts.
- Never LOD-switch mid-drag (hysteresis 300 ms).
### 10.4 Repaint regions
- `setOpaque(true)` on BoardSurface and BoardViewport content where possible (surface covers canvas; PedalViews and CableLayer non-opaque).
- Dirty rectangles only: `repaint(PedalView.getBounds().expanded(cableGutter))` on knob/LED; `CableLayer::repaintCable(id)` unions old+new path bounds (`Path::getBounds().expanded(thickness+4)`) per cable; never full-canvas repaint except style/zoom/palette change.
- During drag: only ghost bounds (old + new) and gap-animation bounds repaint; CableLayer repaints union of affected cable bounds; BoardSurface untouched (cached).
- `Component::setBufferedToImage(true)` on PedalView during animations (layer promotion) and off after settle; on DragLayer ghost always buffered.
- Animator: single `juce::VBlankAttachment` per window (not per component), fan-out callbacks; uses `AnimationPolicy::Registration` of class `BoardView` (MotionClass Transition for slides, LiveReadout for LEDs/meters). Add `BoardCanvas` to allow-list ONLY if any poll-only timer used; prefer vblank-driven (no poll).
- Idle: no timers; VBlankAttachment stopped when no animation active (`animationsActive==0`).
### 10.5 Frame budget (16.6 ms)
Drag frame: layout animate (<=16 setBounds) 0.3 ms; surface blit 0.5 ms; ghost paint 0.6 ms; cables (<=40, 4-8 segments each, stroked 2 passes: shadow + core) 1.5 ms; total target < 5 ms [EST]. Test `T-UI-PERF` (sec 15).
### 10.6 Threading
All UI on message thread. Audio->UI read-backs via atomics polled at 30 Hz by one `Timer` (poll-only, allow-list entry `BoardStatusPoll`: LED/tail/limiter/fb_fault) running only while window or strip visible. No `MessageManager::callAsync` flood: coalesce by flag (`std::atomic<bool> dirty`).

## 11. Cables, routing, feedback visualisation
### 11.1 Model
`CableLayer` draws edges from `Topology` (BOARD_CORE 6): nodes = IN jack, pedal jacks, OUT jack, AMP marker jacks, Split/Sum boxes, loop SEND/RETURN, FbSend/FbReturn. Edges: audio (series), lane, loop, feedback. Endpoints are real jack positions on PedalView (`jackPos(Side, index)`).
### 11.2 Series cable geometry
- Jack-to-jack **patch cable** from pedal i OUT (right side) to pedal i+1 IN (left side). Path: cubic Bezier from p0 to p3 with control points p1 = p0 + (dx*0.5 + sag? ... ): horizontal tangents `p1=(p0.x+ max(24, dx*0.5), p0.y)`, `p2=(p3.x - max(24, dx*0.5), p3.y)`; plus gravity sag: add `+sag` to both control y: `sag = clamp(0.12*|dx|, 8, 28)` for floor-resting look (`loose` style sag 1.5x and 2 jitter control points). Same-row pedals dx ~ gap 56 => short loop.
- Plug: jack connector 10 x 14 px rounded rect steel with gradient and a 3 px strain-relief boot (black) rendered at each end, angled by tangent. 
- Cable body: stroke width 3.2 px; colour per cable type (below) with 1.2 px highlight line (white@0.25, offset (-0.6,-0.6)); shadow pass: same path offset (0,2.5) stroke 4.5 px black@0.35 (skip for `routed` groove: shadow offset (0,1) alpha .2).
- Direction chevrons: small `>` glyph 6 px every 80 px along path, alpha .45 (shows signal direction; hidden if cables toggle "minimal").
- Colour coding (palette roles): series audio `Palette::textMuted` (grey-cream); lane A `Palette::accent`; lane B `Palette::secondary`; loop send/return `Palette::accentDim`/`secondaryDim` dashed? solid; feedback `Palette::warning`; muted/bypassed segment dashed alpha .5. Colourblind: also differing dash patterns (series solid, lane B dash 8-4, loop dot-dash, feedback dash 3-3 + double stroke) so colour is never sole carrier (accessibility 13).
- Signal activity: optional animated bead (circle r 2.2) moving along path at speed ~ 120 px/s when signal present (`ui.cableBeads`, default on only if AnimationPolicy mayAnimate and `LiveReadout` allowed; level from `getTailEnergyDb`/post-pedal peak; off at silence).
### 11.3 Routing visualisation
- Parallel lanes: Split box (small steel "Y" junction 18 px with 2 jacks) after IN and Sum box before OUT; lane label tabs "A","B" (brass plates). Latency-equalised lanes show a small clock glyph `delay 2.3 ms`.
- A/B split ("ABY"): selected lane highlighted bright, other dim .45; switch uses 10 ms audio xfade; UI crossfades 120 ms.
- Send/return loops: a **loop bracket**: a brass-trimmed rounded outline enclosing the pedals between SEND and RETURN with label "LOOP 1 (kind, mix)"; cables from SEND tap go up/over (arc above row) to the loop row; loop mix knob on bracket header.
- AMP marker: a drawn small amp-head icon (original vector: rounded rect 72x56, grille cloth hatch, 3 knobs) between PRE and LOOP segments with IN (left) OUT (right) jacks and label "AMP"; dashed outline when amp bypassed in engine (no state => always solid). Cables run through it.
- Dual output (BOARD_CORE 6.3 `Dual`): second OUT jack "B / AUX" at lane B end, label with bus name.
### 11.4 Feedback loop visualisation
- Feedback edge from FbSend (tap after pedal s, jack on its top) arcing **over** the pedals to FbReturn (top jack of pedal r, r<=s): Path = Bezier rising `arcH = 60 + 12*(s-r)` px above the row, `Palette::warning` 3.2 px, dashed 6-4 when loop gain < 0.98, solid thick 4 px + animated pulse (glow stroke alpha pulsing 0.3<->0.8 at 1.2 Hz, still glow when reduced motion) when `g_fb > 0.98` ("runaway-capable").
- Feedback badge at arc apex: round pill with gain readout `FB 62%` and mini knob (drag vertical to set `g_fb`, double-click reset); kill button (momentary "KILL", bound key K) 22x22 px.
- State overlays: `fb_tripped` => arc turns `Palette::clip` red, badge shows "TRIPPED" + [RE-ARM] button; `fb_fault` => hatched + toast "Feedback fault: NaN guard triggered". Status bar mirrors.
- Creating feedback: right-click output jack of pedal s > "Feedback to..." then click target pedal's top input jack; or drag from the SEND jack handle (dot) to a target jack: valid targets highlighted green (r<=s), invalid (s<r) red with tooltip "Feedback must return upstream (compiler rejects acyclic breaks)"; reject = no change. Max 1 loop (kMaxFb=1): second attempt replaces after confirm toast with Undo.
- Also draws unit-delay note in the badge tooltip ("+1.3 ms @48k min").
### 11.5 Loose-style tangle
- Cables use 2 extra random control points (seeded by cable id) to produce slack S-curves; occasional crossing/overlap with neighbouring cables (z-order alternation per cable `cableZ = hash`), small knotted loop (figure-8 loop path, 1 per ~5 cables, radius 10 px) when slack > 24 px; tape/cable tie decor optional. Tangle is purely cosmetic: geometry never alters hit testing of jacks (plugs drawn at true endpoints; cable body hit-test uses simplified straight chord polyline with 8 px tolerance).
- Other styles: tidy cables: clipped to rails via small cable clips (3 per cable at 25/50/75%: 6 px steel loop `Path`) in `routed` style show cable in grooves, `rail` style cable zip-ties (4 x 8 px black rect with tail) at rail crossings.
### 11.6 Cable rendering cost
Cables painted into a transparent `Image` only when the topology/layout changes ("cable bake") is NOT used because endpoints move during drag; instead: per-cable Path built once per layout change (cached in `std::vector<CablePath>`), per frame only paths with moved endpoints are rebuilt; stroke via `Graphics::strokePath(path, PathStrokeType(w, curved, rounded))`. Max 64 cables drawn. At >32 cables, drop highlight line & shadow pass (LOD1).
### 11.7 Cable interaction
- Hover cable: thicker (4.5 px), tooltip "Pedal A OUT -> Pedal B IN" and latency; click selects; Delete/Backspace removes a user-made feedback/loop cable (series cables not deletable: derive from order). Series cables are display-only. A "Cables: Full | Minimal | Off" toggle (`ui.showCables`, 3-state, default Full) for perf/clutter.
- Jack hover: ring `Palette::accentBright`; dragging from jack with Alt held starts loop/feedback creation (sec 11.4).

## 12. Pedal browser (add/remove)
- `PedalBrowser` (left panel 200 px, collapsible, or popup callout in dock mode): search field (typed filter on name/maker/family; focus Ctrl+F), category tabs: Generic | Reference (by family: Drive, Fuzz, Mod, Delay, Reverb, Dynamics, Filter, Looper, Utility), `ListBox` of items (row 56 px: 48x40 thumbnail = LOD2 face render from FaceImageCache, name, "inspired by" maker line, evidence grade chip DOC/MEAS/EST, controls count).
- Add: double-click, Enter, or "Add" button => `BoardCommand::Add(modelId, index = after selected or end)`; drag item onto board (sec 7.1). Capacity: if `kMaxSlots` (16) reached => Add disabled with tooltip; if CPU budget (BOARD_CORE 13) refuses => toast with actionable message (ARCHITECTURE 6).
- Remove: context > Remove, Delete key on focused pedal, drag to trash. Always undoable; toast "Removed X [Undo]" 6 s. Removed pedals' DSP retired via model (no UI audio concerns).
- Info: select item shows side panel with description, control list, provenance/legal ("Inspired by; not affiliated"), "Reference model status: PROTOTYPE|VALIDATED" (from ARCHITECTURE gates).
- Browser items from `PedalRegistry::all()` (engine registry, BOARD_CORE sec 11); face thumbnails generated lazily on message thread, budget 4 per frame.

## 13. Universal wet/dry knob, bypass, MIDI learn
### 13.1 Toolbar
- `MIX` knob: `LuthierLookAndFeel` bell knob `Metrics::knobSmall` (36), param `ref_board_mix` 0..1, shows "WET 100%"/"DRY 0%" text; double-click = 1.0 default; Shift-drag fine; value popup via `shouldShowValue`. Centre label; mix law selector in knob context menu (`ref_board_mix_law`). Attached via `SliderAttachment`-equivalent bound to APVTS `ref_board_mix`. Smooth (no zipper: audio side smoothing 20 ms, BOARD_CORE 7).
- `BYPASS` toggle (ref_board_enabled): large 28 px toggle with LED; label "BOARD ON/OFF". Toggle never moves/hides pedals; trails behaviour per BOARD_CORE 7.
- Mode selector (series/parallel/ABY/loops): ComboBox bound to `mode`.
### 13.2 MIDI learn UI
- Toggle `MIDI LEARN` (toolbar, key Ctrl+L): enters learn mode: overlay tint (`Palette::accent`@0.08) and every learnable control (knob, switch, footswitch, board mix, exp targets) gets a pulsing 2 px outline dashed (`Palette::accentBright`); status bar: "Learn: click a control, then move a CC / press a note / aftertouch".
- Flow: click control => armed (solid outline, status shows target `Pedal 2 Drive`); next incoming MIDI event (CC, note-on velocity, channel/poly aftertouch, pitchbend) from the plugin MIDI input is captured (message-thread side queue from audio thread, SPSC ring, BOARD_CORE 9.5) => mapping created; control shows mapped badge (small 12 px chip "CC74" / "AT" / "VEL" / "PB") top-right of control. Timeout 20 s auto-disarm. Esc cancels armed; Esc again exits mode.
- Mapping editor (right-click mapped control > "MIDI mapping..."): `CallOutBox` with: source (CC#, Note, Aftertouch ch/poly, Velocity), channel (omni/1-16), range `lo`..`hi` (normalised), curve (lin/log/s), mode (absolute | relative | toggle | momentary | trigger), takeover (jump | pickup | scale). Clear button. All edits are commands to the model (undoable).
- Footswitch mapping: velocity and aftertouch emulate press: mapping panel exposes "Press velocity -> debounce/hold" (BOARD_CORE 9.6): fields `velThreshold`, `aftertouchThreshold` and "soft press = momentary / hard press = latch" if the engine exposes it; UI only configures.
- Conflict: new mapping to an already-used CC prompts replacing (callout with Replace/Add both/Cancel).
- Live feedback: mapped controls animate with incoming MIDI (knob follows, throttled to 30 Hz).
- Learn data persisted in `midi.map` (BOARD_CORE 10.1) not in UI block. Learn mode itself is not persisted.
- Accessibility of learn: Tab to control, `L` arms learn on the focused control without entering global mode.

## 14. Interaction & keyboard
| key | action |
|---|---|
| Ctrl/Cmd+Shift+B | toggle pop-out |
| Tab / Shift+Tab | move focus pedal-to-pedal then within pedal (footswitch, knobs L->R) |
| Left/Right (pedal focused) | focus prev/next pedal |
| Up/Down (control focused) | +-1 step; Shift +-10%; Ctrl+- fine 0.1% |
| Space/Enter | footswitch toggle on focused pedal / press focused button |
| Alt+Left/Right/Home/End/Up/Down | reorder (sec 7.3) |
| Delete / Backspace | remove focused pedal (undo toast) |
| Ctrl+D | duplicate |
| Ctrl+Z / Ctrl+Shift+Z (Ctrl+Y) | undo/redo |
| Ctrl+F | focus browser search |
| Ctrl+L | MIDI learn mode |
| Ctrl+0 / Ctrl+= / Ctrl+- | fit / zoom in / out |
| Esc | cancel drag / learn / close callout; if nothing active and window focused, return focus to editor |
| F6 | cycle regions toolbar > browser > board > status |
| Menu / Shift+F10 | context menu on focused item |
- `setWantsKeyboardFocus(true)` on PedalView, focus order = audio order (`getExplicitFocusOrder` = index+10). Focused pedal auto-scrolled into view; focus ring 2 px `Palette::accentBright` + 1 px `Palette::backgroundDeep` outline (>= 3:1 contrast).
- Mouse: click footswitch, double-click knob = reset, wheel on knob = +-1%, right-click = menu, middle-drag/space-drag = pan, Ctrl+wheel zoom.
- Touch: 44 px min hit target for footswitch (extend hit area via `hitTest` override), long-press = context menu.

## 15. Accessibility
- Every interactive item has `AccessibilityHandler` with name/description/role/state: PedalView role `group`, title "<Model name>, position n of N, <on|off>, <placement>"; FootswitchButton role `toggleButton` "Bypass <model>"; knobs `slider` "<param> <value text>"; jacks `button` "<Pedal> output, connected to <Pedal> input"; BoardSurface `image` "Pedalboard: <style name>, <mounting> mounting"; MIX knob "Board wet/dry mix, 100 percent wet".
- Live announcements (via `AccessibilityHandler::notifyAccessibilityEvent`/`postAnnouncement` where supported, else focus-change description update): reorder "Moved <model> to position 3 of 6", add/remove, bypass state, feedback tripped, window opened/closed.
- Reduced motion (`AccessibilitySettings::isReducedMotion`, `AnimationPolicy::mayAnimate(Transition)` false): all slides/spins/wobble/pulses become instant or static; LED/beads off; feedback glow static. Minimal flashing (< 3 flashes/s) always.
- High contrast (`Palette::textured=false`): flat fills, 2 px outlines, mount glyphs reduced to text-bearing icons; focus ring 3 px; cables with dash patterns + labels. Colour never sole info carrier.
- Screen-reader full parity for reorder without drag (sec 7.3) and for add/remove (browser list is native `ListBox`).
- Contrast: text on pedal faces/plates >= 4.5:1 (checked by existing ThemeSweep style test); UI chrome >= 3:1.
- Window title updates "Pedalboard - 6 pedals". Provide `window` accessible name "Pedalboard".
- Text scaling: layout uses `Fonts::*` heights scaled by editor scale factor; knob labels never < 9 px logical.

## 16. Theme integration (Source/UI/Theme.*)
- Chrome (toolbar, browser, status, plates, tooltips): `LuthierLookAndFeel` instance set on `BoardRoot` via `setLookAndFeel(&lnf)` (editor's shared LNF pointer, so palette changes propagate); `drawPanel`, `drawSectionHeader`, `drawSeparator` used for toolbar/browser/status; buttons/combos/sliders from LNF; fonts `Fonts::ui/label/sectionHeader/mono`.
- Corners `Metrics::panelCorner/controlCorner`, spacing `Metrics::grid` multiples, header 44 -> `Metrics::headerHeight` (48) allowed to use 44 only if toolbar; use 48 (`Metrics::headerHeight`) for consistency [DOC]; update sec 4 to 48 in impl.
- Palette reaction: listen `Palette::revision` / AccessibilitySettings change (`juce::ChangeListener` or `lookAndFeelChanged`) => `Palette::remap(root, from, to)` for setColour'd widgets, invalidate caches by key (revision folded), repaint.
- `drawCornerScrews` reused for toolbar/browser panel corners; board-mount screws (sec 8.1) are a different, larger drawer `MountPainter::drawScrew` but share colour constants (head gradient `#d8d2c4 -> #6e6860` [DOC from drawCornerScrews]).
- Materials registry `BoardMaterials.h` holds all hex constants in this spec (single place) with named roles; kept separate from Palette (materials do not recolour with accent, sec 9.8). No new `Palette::` entries unless review demands: if needed add `Palette::boardMat*` as inline colours with default and apply() mapping (append-only).
- Pedal face colour schemes come from model specs, tonemapped in Light/ColourBlind palettes only via `Palette::textured`/contrast guard, not recoloured.

## 17. Files (modular, each <= ~400 lines)
```
Source/UI/ReferencePedals/
  BoardWindow.{h,cpp}        DocumentWindow, geometry persist, host fallback
  BoardRoot.{h,cpp}          layout toolbar/browser/viewport/status, focus, shortcuts
  Toolbar.{h,cpp}            style picker, add, undo/redo, mix knob, bypass, learn, zoom
  PedalBrowser.{h,cpp}       list/search/drag source
  BoardViewport.{h,cpp}      scroll+zoom, pan, auto-scroll
  BoardCanvas.{h,cpp}        layout engine (positions from BoardModel order), lanes, AMP marker
  BoardSurfacePainter.{h,cpp} + styles/{Plywood,Velcro,Clamp,Routed,Rail,Ceramic,Loose}.cpp   surface recipes
  MountPainter.{h,cpp}  + mounts/{Screws,Velcro,Clamps,Routed,Rail,Ceramic,None}.cpp           mount recipes
  BoardMaterials.h           constants/hex/seeds
  PedalView.{h,cpp}          wrapper, footswitch, LED, drag handle, a11y
  PedalFaceCache.{h,cpp}     shared face images
  GenericFace.{h,cpp}        generic pedal face from descriptors
  DragController.{h,cpp}     gestures, insertion index, hysteresis, commit rate limit
  CableLayer.{h,cpp}         paths, jack positions, feedback visuals, hit-test
  MidiLearnOverlay.{h,cpp}   learn mode, mapping callout
  StatusBar.{h,cpp}
  BoardUiState.{h,cpp}       ui JSON (de)serialise, validation
Source/UI/ReferencePedalDrawer.{h,cpp}   kept: dock shell hosting BoardRoot in compact mode
Source/Tests/BoardUiTests.cpp            (new)
```
CMake: add to the existing source glob; no new deps. Test registration follows `LUTHIER_TEST`.

## 18. Test hooks (headless screenshot + behaviour tests)
Hooks (public, test-only, `#if LUTHIER_TESTING` or always-on const): `BoardRoot::snapshot(Rectangle)`, `BoardWindow::forceHeadlessContent()` (creates BoardRoot without peer; `createComponentSnapshot` works off-screen), `BoardUiState` pure functions, `BoardSurfacePainter::paint` and `MountPainter::paint` are static pure functions of (Graphics, params, seed) => directly renderable to `juce::Image` in tests, `DragController` driven by synthetic `MouseEvent`s / direct `dragTo(Point)`, deterministic seed `kTestSeed`, clock injection (`AnimationPolicy` off) so animations resolve instantly: `BoardCanvas::settleAnimations()`.
Tests (suite `BoardUi`, in `Source/Tests/BoardUiTests.cpp`, style of AppearanceTests/RealismUiTestsC):
1. T-UI-SNAP-STYLE: for each of 7 styles x {textured,flat} render 800x320 board with 4 generic pedals; assert image valid, not uniform (stddev luminance > threshold per style), and compare to golden PNG in `Source/Tests/Golden/ReferencePedals/board_<style>_<mode>.png` (existing dir is `Golden/`, capital G; PNGs are TEST fixtures only, A10 'no bitmap assets' concerns shipped UI) with per-pixel tolerance 6/255 and <= 0.5% differing pixels (goldens regenerated by env `LUTHIER_UPDATE_GOLDENS=1`); dimensions identical across OS. Skip text-bearing regions (font variance): masks defined per golden.
2. T-UI-MOUNT: render each mounting at footprint 120x200: assert mount pixels exist outside footprint (screws/clamps/velcro) in expected side regions (left & right band both non-empty for screws; both sides symmetric within tol); `none` => no pixels outside except shadow.
3. T-UI-SEED: same seed => identical images; different pedal ids => different screw angles.
4. T-UI-WINDOW: create BoardWindow headless, `setVisible(true/false)` x 20: toggle state mirrors, no leaked components (`JUCE_LEAK_DETECTOR`), geometry round-trips; invalid geometry (offscreen, 0 size, huge) clamps into display. Close button path keeps model unchanged (audio order hash same).
5. T-UI-FALLBACK: simulate peer failure => docked strip shown, `window` state "drawer".
6. T-UI-DOCK: Drawer open/close heights 0/232; resize persisted; editor controls remain at same bounds when strip collapsed (no forced editor resize).
7. T-UI-NONBLOCK: with window open, main editor controls respond (set param via component API); assert `Component::isCurrentlyModal()` false everywhere; no `enterModalState`.
8. T-UI-REORDER: synthetic drag pedal 0 over slot 3: with live reorder, after each commit `BoardModel::getOrder()` equals visual order; final order matches; model audio order hash == UI order; undo = one step restores; Alt+Right moves one; AMP crossing flips placement; cancel (Esc) restores.
9. T-UI-ORDER-AUDIO: render processor audio for [OD, Delay] order, drag-swap, re-render => matches BOARD_CORE 4.3 golden (difference > -30 dBFS RMS) and no clicks (max |diff sample-to-sample| below bound in crossfade).
10. T-UI-MIX: knob drives `ref_board_mix`; value text; double-click default; automation write round-trip.
11. T-UI-CABLE: series cable endpoints equal jack positions (+-0.5 px) for all styles and zoom; chevrons present; feedback arc geometry above row; invalid target rejected; fb_tripped colour state; hit-test tolerance.
12. T-UI-FB: create feedback via UI path; state saved (`feedback` JSON) and visualised after reload.
13. T-UI-LEARN: arm control, inject CC/aftertouch/velocity events, mapping created and badge shown; timeout disarm; conflict callout; mapped control follows value.
14. T-UI-A11Y: every PedalView/footswitch/knob/jack has non-empty accessible name; focus order == audio order; announcement on reorder; reduced-motion => `settleAnimations` not needed (instant); high contrast render has no gradients (check pixel unique-colour count lower than threshold and focus ring present).
15. T-UI-THEME: `Palette::apply` each palette (Dark/Light/Colourblind/High contrast): chrome colours update, `PaintCache` keys change, no stale cache image (snapshot differs), no exceptions; ThemeSweep integration: register BoardRoot in ThemeSweepTests list.
16. T-UI-PERF: build 16-pedal board with 40 cables at zoom 1.0 and 1.5; run 300 synthetic drag frames headless measuring paint time: p95 < 8 ms, p99 < 16 ms on CI (non-gating warn if CI noisy => gating threshold 2x) ; cache memory <= 96 MB; repaint-count: idle for 1 s => 0 repaints; drag => only expected bounds (inspect via a `RepaintTracker` hook counting `repaint()` area, assert total repainted area < 25% of canvas per frame).
17. T-UI-STATE: round-trip `ui` + `boardStyle` JSON; unknown style preserved; old state without `ui` loads defaults; legacy sessions unchanged (T-LEGACY: board never constructed UI when `referencePedals` absent; UI creation lazily on first open).
18. T-UI-STRESS: random op fuzz 10k ops (add/remove/move/style change/zoom/toggle window/learn) seeded; invariants: order==visual, no crash, no leaks, caches within budget; ASan/UBSan clean; run also with window hidden/shown at random.
19. T-UI-DPI: render at scale 1.0/1.5/2.0 and zoom buckets; cache re-raster after idle; no 1 px seams between surface tiles (check seam rows luminance continuity <= 3).
Manual QA checklist (release gate): Reaper/Live/Logic/FL/Bitwig/Cubase window open/close/focus; multi-monitor move; standalone; Linux X11/Wayland; screen reader (NVDA, VoiceOver); touch.

## 19. Acceptance criteria (UI)
A1 Toggle opens/closes separate window; X and button agree; position/size restored next launch (incl. validated against displays). A2 Main editor stays interactive; no modal. A3 Dock strip works and collapses to 0 height; host fallback automatic. A4 Drag reorder updates audio live; visual==audio order at every committed frame; one undo per drag. A5 All 7 styles render with distinct mounts as specified, persisted. A6 Wet/dry knob adjusts `ref_board_mix`, MIDI-learnable. A7 Cables show routing, lanes, loops, feedback with correct states; no colour-only cues. A8 60 fps target, caches in budget, idle zero repaint. A9 Full keyboard + screen-reader parity; reduced motion and high contrast honoured. A10 No bitmap assets added; all T-UI-* pass; no audio-thread impact.

## 20. Open items
O1 Final dock default (open/closed) per Easy vs Advanced edition. O2 Whether `Palette` needs `boardMat*` roles (sec 16). O3 Host-specific window quirks table (start empty). O4 Command manager vs direct key handling in editor. O5 Multi-select group drag (post v1). O6 Glossy pedal-face rendering budget when many identical models (cache sharing verified). O7 Toolbar height 44 vs `Metrics::headerHeight` 48 (pick 48). O8 Wayland window position restore unsupported (position ignored, size honoured).
