# GUI requirements audit, branch I (integration/luthier-completion, ca14934e)
Read-only static audit. Nothing was compiled or run. Paths are under /home/user/luthier.

## 0. Size computation (the key numbers)
Constants: Source/PluginEditor.h:57-60 -> defaultWidth 1200, defaultHeight 720, minimumWidth 940, minimumHeight 560.
Constrainer: Source/PluginEditor.cpp:246-252 -> setResizable(true,true); setSizeLimits(940,560,2400,1440); setFixedAspectRatio(1200/720 = 5:3 = 1.6667).
Restore: PluginEditor.cpp:255-256 setSize(max(min, ui.editorWidth), max(min, ui.editorHeight)) - no upper clamp, no aspect snap, no screen clamp (the constrainer is bypassed by setSize). A saved 1280x900 is therefore applied as-is until the user drags; PR #85's "1280x900 -> 1280x768" is the constrainer's result on a drag (1280/1.6667 = 768).

| Quantity | Value |
|---|---|
| Default | 1200x720 (5:3) |
| Min (effective, with aspect) | 940x564 (940/1.6667); declared 940x560 |
| Max | 2400x1440 (exact 5:3) |
| Largest fit, 1920x1000 usable | height-bound: 1666x1000 (width 1666.7). Width-bound would be 1920x1152 (does not fit) |
| Largest fit, 1920x1000 minus 60 px host header (=940 high) | 1566x940 |
| Full-screen 1920x1080 | not reachable (5:3 -> 1800x1080 or 1920x1152). Tests use 1920x1080 directly via setSize (EditorTests.cpp:231), which the constrainer would never produce |
| Fits R1 | default 1200x720 and the 1666x1000 / 1566x940 maxima all fit. The 2400x1440 max exceeds 1080p but is only reachable on bigger screens |

Aspect vs 16:9: CONFLICT (mild). At 1000 px high a 16:9 window would be 1778 wide; 5:3 yields 1666, leaving ~254 px (13%) of a 1920 screen unused; in a DAW with 940 px available, 1566 of 1920 (18% unused). Dragging wider forces height up (the width drag is clamped by the aspect). The aspect lock buys no layout benefit: Easy/Advanced lay out from width/height independently. Recommendation: drop setFixedAspectRatio (or loosen to a 3:2..16:9 band) and set max to min(2400x1440, screen userArea).
Docking floor: PluginEditor.cpp:632 and 1739/1754/1761 -> `getHeight() < 840` closes/refuses the docked pedalboard. At 5:3, 840 high means >=1400 wide; the default 720 never docks (pop-out only). On 1920x1080 it works at the 1666x1000 and 1566x940 maxima, but not at the default or anything <1400 wide.
Advanced floor: PluginEditor.h:175, AdvancedPanel.h:172 minimumUsableWidth = 1000 > minimumWidth 940: at the minimum size, Advanced is forced to Easy with a notice (EditorTests.cpp:800-850 tests it). Default 1200 >= 1000 OK.
UI scale: PluginEditor.cpp:703-735 applyUiScale -> setScaleFactor(largestScaleThatFits(wanted, display->userArea, 940, 560)) (Accessibility.cpp:466). Test PolishTests.cpp:216 (UiScale, aTooSmallScreenPicksTheLargestScaleThatFits), AppearanceTests.cpp:610-630.

## 1. Requirements

### R1 fits on 1920x1080 - MET (default+max); UNKNOWN for "no clipping" (see R10)
Evidence: section 0. Default 1200x720 < 1920x1000 by wide margin; constrainer max reachable on 1080p is 1666x1000.
Test: EditorTests.cpp:231 (3 sizes incl. 1920x1080, checks only that something paints and header/easy have non-zero size); none asserting bounds <= screen.
Gap: restored size is unclamped (PluginEditor.cpp:255); a size saved on a 4K display (up to 2400x1440) reopens oversize on a 1080p display. Standalone (Source/StandaloneApp.cpp:60-67) uses juce::StandaloneFilterWindow with no screen clamp of its own (UNVERIFIED how JUCE clamps at runtime).
Fix (S): clamp ui.editorWidth/Height to the display userArea (minus ~60 px) in the editor ctor, snap to constrainer. Test (S): set ui.editorWidth=2400, construct editor with a mocked 1920x1000 area (add a static `clampToArea(size, area)` helper like largestScaleThatFits) and assert result <= area.

### R2 resizable, sensible min - MET (with caveat)
Evidence: PluginEditor.cpp:246-252. Standalone: StandaloneApp.cpp:62 uses StandaloneFilterWindow (it takes the editor's resizability/constrainer; UNVERIFIED at runtime). Min 940x564 is sensible for Easy; Advanced is blocked below 1000.
Test: EditorTests.cpp:800-850 (advanced forced to Easy at min). None for the constrainer itself (limits/aspect values).
Gap: aspect lock (see 0); no test pins the limits.
Fix (S): see R1; test (S): `editor->getConstrainer()` checkBounds on 100x100, 5000x5000 and a 16:9 request, assert within limits.

### R3 tabs for major areas - MET
Evidence: AdvancedPanel.h:180-262, AdvancedPanel.cpp:1458+ workspace tabs (WORKSHOP, MOD, RHYTHM, LIVE, ROUTING, TONE MATCH, CHARACTER, CONTROLLERS, TUNE, RIFFS, TECHNIQUES, HELP...), UI/WorkspaceTabStrip.h scrolls on overflow. Hand-rolled buttons (no juce::TabbedComponent). Easy mode has no tabs (single page + overlays).
Test: RiffPanelTests.cpp:135 (workspaceTabStripOverflowsCleanlyAt1200And1920), RiffPanelTests.cpp:249; GuiReach tests.
Gap: none material.

### R4 scrollbars / wheel - PARTIAL
Evidence: AdvancedPanel.cpp:487-490, 507-508 viewports (ScrollHintViewport, AdvancedPanel.h:61, chevrons+tooltip "More controls above/below: scroll ..." at AdvancedPanel.cpp:26); thickness 8 px (setScrollBarThickness(8)); LookAndFeel scrollbar draws accent thumb at every state (Theme.cpp:1167-1195) and arrow buttons (Theme.h:222 areScrollbarButtonsVisible=true; Theme.cpp:1215 drawScrollbarButton). Knob wheel: LuthierKnob::KnobSlider::mouseWheelMove (Widgets.cpp:954-965, WheelPassSlider Widgets.h:166-187) - wheel passes to the viewport unless Ctrl/Cmd. Owner's knob complaint is FIXED for LuthierKnob.
Test: ScrollWheelTests.cpp:46 (theWheelScrollsAColumnUnlessCtrlIsHeld), :150 (popupRowsAndScrollbarButtonsAreReadable).
Gaps: (a) juce::ComboBox never calls setScrollWheelEnabled(false) (grep: zero hits in Source) and JUCE ComboBox::mouseWheelMove nudges the selection (ThirdParty/JUCE/modules/juce_gui_basics/widgets/juce_ComboBox.cpp:599-619). 166 ComboBox references in Source/UI headers; every dropdown inside a scrolling column still hijacks the wheel - the same bug as the knob, for dropdowns. (b) Plain juce::Slider/WheelPassSlider users outside LuthierKnob (80 plain Slider members in UI headers) not audited: UNVERIFIED which are inside viewports. (c) 8 px thickness is thin for "obvious"; no tooltip on the scrollbar itself (chevrons have tooltips). (d) workspaceViewport and other overlays' viewports (PresetBrowser, ModMatrix, Techniques) not confirmed to be ScrollHintViewport.
Fix (S-M): add a WheelPassComboBox / helper `AccessibleSetup::configureComboBox` (used at e.g. RhythmPanel.cpp:685) to call setScrollWheelEnabled(false) (or require Ctrl), raise thickness to 12; Test (S): walk every ComboBox under AdvancedPanel (collect<>), send a mouseWheelMove, assert selectedId unchanged and viewport moved; extend ScrollWheelTests.

### R5 collapsible sections - PARTIAL (mostly NOT MET)
Evidence: AdvancedPanel::Column (AdvancedPanel.h:282-310) sections are static headings (addSection/getSections), no collapse state. Collapsible exist only in Practice drawer (PracticePanel), StringRoll (test StringRollTests.cpp:208 theNotationTabHostsItAndRemembersTheCollapse), PedalRack, Riff drawer, Easy riff drawer.
Test: StringRollTests.cpp:208 only.
Gap: Advanced columns (the long scrolling ones) cannot be collapsed; no accordion class.
Fix (M): give Column section headers a chevron + collapsed flag persisted via UiState, layout() skips collapsed controls; Test (M): collapse a section, assert its controls are invisible and layout height shrinks; state survives editor rebuild.

### R6 guitar-type selection in Workshop - MET
Evidence: WorkshopPanel.h:189-206 (showCategory, switchFamily), drawerCategories()[0]=="Guitar" (first category = family, 5 parts); WorkshopPanel.cpp:1422-1450 switchFamily with one-time confirmation; :1547.
Test: WorkshopPanelTests.cpp:295 (WorkshopPanel, theGuitarCategorySwitchesFamily); GuitarRendererTests.cpp:411.
Gap: none for function. Visual check at narrow width (<700 pt categories become dropdown, WorkshopPanel.cpp:1173/1951) is untested.

### R7 pedalboard for six pedals - PARTIAL / UNKNOWN on intent
Evidence: Capacity is still 16 (Source/ReferencePedals/BoardTypes.h:22 kMaxSlots=16 "UI shows 8 by default"; PedalBrowser.cpp:23 "Board capacity is 16 pedals"). No constant, test or layout anywhere refers to 6 slots. "18" in repo docs is the pedal MODEL count (spec/reference-pedalboard/HANDOFF.md:11), so the owner's "18 -> 6" is not implemented in code as a slot count (UNVERIFIED how the owner meant it; confirm against PR #85 text, gh not queried successfully).
Layout: BoardCanvas.cpp:20-30 one horizontal row, x starts 72, step = physicalSize + 56, physicalSize = designSizeMm*1.6 px (PedalView.cpp:188; generic.boost 54x90 mm -> 86x144 px; others UNVERIFIED), canvas min width 1100, min height 300 (BoardCanvas.cpp:30). Six pedals ~ 72 + 6*142 + 72 = ~1000 px: fits the 1400+ docking width without horizontal scroll (estimate). 
Drawer: ReferencePedalDrawer.h:34 height 232 default, limits 160..420 (BoardUiState.cpp:27; Drawer::mouseDrag). Toolbar 64 px + 6 px grip -> viewport ~162 px tall, but canvas needs >=300 -> vertical scroll/cropped pedals in the default drawer (only ~350 px at max 420). BoardRoot.cpp:104-116: compact mode hides the pedal browser and status bar. Docking floor 840 (PluginEditor.cpp:632,1739,1754,1761). BoardWindow (pop-out) setResizeLimits(480,260,4096,2160) BoardWindow.cpp:12; saved geometry clamped (BoardUiState::clampWindow, test BoardUiTests.cpp:11).
Test: BoardUiTests.cpp:80 (canvasUsesPhysicalFaceSize), :156 (cachedSixteenPedalPaintBudget), :201 (compact dock rejection). None asserts six pedals visible in the drawer.
Gap: no six-pedal-sized layout; default drawer too short; docking unavailable at default 720; fixed 840 magic number duplicated 5 times.
Fix (M): constant kDockMinEditorHeight; make default drawer height computed from pedal height (>= ~48+maxPedalH+64+6), or auto-zoom (zoomTo(min(w/canvasW, h/canvasH))) on dock; optionally lower UI slot cap to 6 if the owner wants. Test (M): editor at 1566x940 with 6 pedals added via model.apply(cmd::Add), dock open, assert every PedalView bounds (mapped) lies inside the viewport at fit zoom.

### R8 keyboard navigation + visible focus - PARTIAL
Evidence: Tab walk reaches every knob (AccessibilitySweepTests.cpp:161 Keyboard, aTabWalkReachesEveryKnob); setWantsKeyboardFocus on knob slider (Widgets.cpp:488); arrow-key steps (Widgets.h:283); tab strip stepping Ctrl+[ / ] (AdvancedPanel.h:255); shortcuts docs/KEYBOARD_SHORTCUTS.md. Focus ring drawing exists in custom widgets (QualityBadge.cpp:120, NormalizationBadge.cpp:57, RightHandGroup.cpp:69, JamLaneView.cpp:161, TechniquesPanel.cpp:29, StringRoll.cpp:38, pedal KnobControl.cpp:62, PedalView.cpp:264).
Gap: LuthierKnob (Widgets.cpp:701 paint), the LookAndFeel buttons/tab buttons/toggles (Theme.cpp - only ComboBox/TextEditor focus colours, Theme.cpp:367,385) draw no focus indicator, and nothing in Source calls Component::setHasFocusOutline (grep: 0 hits; JUCE 8 only draws an outline when that flag is set, juce_Component.h:1566). So Tab-focused knobs and tab buttons are likely invisible-focus. UNVERIFIED by rendering.
Test: no test asserts a visible focus mark.
Fix (M): in LuthierLookAndFeel implement createFocusOutlineForComponent + call setHasFocusOutline(true) on LuthierKnob, buttons, workspace tab buttons; or paintOverChildren ring when hasKeyboardFocus(true). Test (M): focus a knob/button, render before/after (render() at ScreenshotTests.cpp:57) and assert pixel diff in ring area.

### R9 high-DPI - PARTIAL (framework-level MET)
Evidence: no hard-coded pixel ratios; PaintCache keys on getPhysicalPixelScaleFactor (UI/PaintCache.h:61-72); user scale 75-200% with fit-to-screen (PluginEditor.cpp:703). No explicit DPI settings in CMake (JUCE 8 default per-monitor aware: UNVERIFIED for the Standalone manifest).
Test: PolishTests.cpp:216, AppearanceTests.cpp:610 (scale factor applied). No test renders at scale != 1 or checks hi-res image backing.
Gap: no verification of crisp rendering (guitar illustration/cached images at 2x), none of window-size-in-scaled-units (min 940x560 at 200% = 1880x1120 > 1080p; handled by the fit rule, which drops to 150%/175%).
Fix (S): test that renders the editor with setScaleFactor(2.0) and checks painted at 2x via PaintCache key; manual Windows/macOS check.

### R10 no clipped/overlapping controls at min and 1920x1080 - NOT MET / UNKNOWN
Evidence: no automated overlap/clipping assertion. EditorTests.cpp:231-250 only checks "painted something" at 940x560 / 1200x720 / 1920x1080. EasyLayoutTests.cpp:160 checks amp knob face >= 40 px at 1200x720 only. Screenshots test (ScreenshotTests.cpp:149-265) renders Easy 1200x720 & 1280x800, Advanced 1600x900 & 1280x800 - never 940x564 or 1920x1080, and only asserts non-empty.
Owner images (guiissues.png, liveguiissue.png, committed 2026-09-23, d91a1eea): (1) MOD: dropdown popup with ~8 px rows, labels overlapping ("Sine/Triangle/Ramp Up..."). Fix landed 2026-09-30 (9eb3d621): LuthierLookAndFeel::getIdealPopupMenuItemSize min 22 px (Theme.h:237, Theme.cpp:1277), test ScrollWheelTests.cpp:150 - so likely FIXED, but UNVERIFIED visually. (2) LIVE: SnapshotGrid is 16x8 = 128 pads (LivePanel.h:50-51); gridHeight = clamp(width/16*2, 80, 200) (LivePanel.cpp:561) -> at a ~650 px column, pads are ~40x10 px: tiny boxes - UNCHANGED (LivePanel.cpp last touched 2026-09-26, after the screenshot, no relevant change). Tooltip exists on the grid (LivePanel.cpp:35) but the owner asked for more hover help.
Gap: LIVE grid tiny; no layout-sanity test; RHYTHM/MOD dropdown widths unverified (RhythmPanel.cpp:685+, ModMatrixPanel.cpp:541-551).
Fix (M): (a) LivePanel: let the grid scroll / use 8x16 or paging so cells >= 18 px tall, or paint labels at bank-of-16; (b) generic layout checker. Test (M): see section 3.

## 2. Existing relevant tests (name -> file:line)
- ScrollWheel.theWheelScrollsAColumnUnlessCtrlIsHeld  Source/Tests/ScrollWheelTests.cpp:46
- ScrollWheel.popupRowsAndScrollbarButtonsAreReadable  ScrollWheelTests.cpp:150
- GuiReach.everyAutomatableParameterHasAVisibleControl  Source/Tests/GuiReachabilityTests.cpp:766 (parameters reachable; not layout)
- Editor sizes test (name at EditorTests.cpp ~:200)  Source/Tests/EditorTests.cpp:231
- advanced-mode-below-minimum test  EditorTests.cpp:800-850
- EasyLayout.theWindowMatchesSection3  EasyLayoutTests.cpp:101; EasyLayout.ampKnobsHaveRoomAtCompactWindowSize :160
- Keyboard.aTabWalkReachesEveryKnob  AccessibilitySweepTests.cpp:161
- RiffPanel.workspaceTabStripOverflowsCleanlyAt1200And1920  RiffPanelTests.cpp:135
- WorkshopPanel.theGuitarCategorySwitchesFamily  WorkshopPanelTests.cpp:295
- UiScale.aTooSmallScreenPicksTheLargestScaleThatFits  PolishTests.cpp:216
- Screenshots.everyPanelInEveryPalette  ScreenshotTests.cpp:245
- RefPedalUi.* (BoardUiTests.cpp:11-201; RefPedalUi*Tests.cpp) - model/drag/faces, not layout bounds.

## 3. How to render the editor headlessly (for follow-up screenshots)
Existing pattern: Source/Tests/ScreenshotTests.cpp
- render(): lines 57-62: `juce::Image image(juce::Image::ARGB, w, h, true, juce::SoftwareImageType()); juce::Graphics g(image); c.paintEntireComponent(g, true);`
- openEditor(): lines 132-146: `processor.createEditor(); editor->setVisible(true); editor->setSize(w,h); keyPressed(toggleAdvanced)` (setSize bypasses the 5:3 constrainer, so arbitrary 1920x1080 / 940x560 works).
- Shooter::shoot writes PNG with juce::PNGImageFormat (lines 117-130).
- Run: `LUTHIER_SCREENSHOTS=/tmp/shots xvfb-run -a build/LuthierTests Screenshots` (file header lines 12-13; target LuthierTests, CMakeLists.txt:360). Without the env var it only checks non-empty. Sizes are hard-coded in shootWindows (lines 149-243: Easy 1200x720/1280x800; Advanced 1600x900/1280x800).
- Second example: EasyLayoutTests.cpp:~139-152 writes `<tmp>/luthier-guitar-renders/_easy.png` at 1200x720.
- Committed outputs: docs/screenshots/easy_1200x720.png, advanced_1600x900_*.png (5 files).
- scripts/render_previews.sh is AUDIO preview rendering, not GUI.
To add: parametrise shootWindows sizes (add {940,564}, {1666,1000}, {1566,940}, {1920,1080}) or add env LUTHIER_SCREENSHOT_SIZES. Pedalboard: render `editor` after `referencePedalDrawer.setOpen(true)` (needs height >= 840) or render BoardRoot directly.
Caveat: software rasteriser, fonts depend on the host (Xvfb/Linux), so pixel diffs vs Windows owner screenshots are approximate.

## 4. Proposed acceptance tests (summary)
1. GuiLayout.noOverlapAtMinAndHD: for sizes {940x564, 1200x720, 1666x1000, 1566x940, 1920x1080}, Easy and every Advanced tab, collect visible leaf controls (knob/button/combo/label), assert none has width/height < 18 px (hit-target floor), none extends beyond its viewport/panel clip (except via viewport), no two siblings in the same parent intersect (whitelist painted overlays), text controls' getBestWidth fits.
2. Editor.constrainerLimits: min/max/aspect/clamp-to-screen (R1/R2).
3. ScrollWheel.comboBoxesDoNotHijack (R4).
4. Focus.focusIsVisible (R8).
5. PedalBoardUi.sixPedalsFitDockedDrawer at 1566x940 (R7).
6. LivePanel.snapshotPadsAreAtLeast18pxTall at 940x564 and 1666x1000 (R10).
7. Collapse.sectionStatePersists (R5).
8. Screenshots at the sizes above, stored as CI artifacts (R1/R10 visual review).

## 5. UNVERIFIED
- All runtime behaviour (nothing built or run). PR #85 body not read (gh output empty), so the "18 -> 6" intent and the 840 px floor wording are inferred from code.
- Pedal sizes other than generic.boost; actual six-pedal width.
- Standalone window clamping behaviour and DPI manifest.
- Focus visibility on knobs/tab buttons (code reading only).
- Whether every column ComboBox is inside a viewport (assumed).
- Owner screenshots predate the popup-height fix; not re-rendered.
