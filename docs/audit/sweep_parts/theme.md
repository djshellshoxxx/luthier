## theme.md

A 2026-09-23 user decision in DECISIONS.md overrides theme.md's look for Luthier with the guitar-shop theme (visual-polish.md §6). The palette neutrals, the Inter font, flat knobs, sliders, panels, section headers, "no faux wood/metal" and the diagonal notch are therefore DEFERRED as superseded; the High Contrast palette is the flat theme.md look. theme.md still governs layout and metrics. The 8 px grid, radii, knob sizes, the 3 px external value arc, 28 px buttons, the 4 px slider track with its 16x24 thumb, meter peak hold, the 400 ms tooltip, the mono 9 px version footer and the output LED all match the spec. Hardly any of this is under test. The remaining gaps:
- The 80 ms value animation is unused.
- The vertical-resize cursor is missing.
- The 100 ms flash is only a while-pressed tint.
- The matrix data stream is built (`DataStreamDisplay` in `UI/Widgets.h`, `DataStream.itKeeps200StopsAfter500msAndHonoursReducedMotion`).

| Req | Summary | Engine location | GUI location | Test | Status |
|---|---|---|---|---|---|
| TH-1 (Colour) | Core neutrals #0E1116/#171B22/#2A303A, text #E6E8EC/#8A929E - superseded by DECISIONS 2026-09-23 (guitar-shop theme, visual-polish 6) | n/a | `UI/Theme.h:Palette`; `Accessibility::buildPalette` | `Theme.theDefaultIsTheGuitarShop` (asserts the override) | DEFERRED |
| TH-2 (Colour) | Only one accent re-tinted per plugin | n/a | `Palette::accent`; Options > Appearance accent | `Theme.aPaletteChangeReachesBuiltComponents` | DONE |
| TH-3 (Colour) | Success/warning/clip colours | n/a | `Palette::success/warning/clip` | `Theme.everyTextPairMeetsContrastOnTheThreePalettes` | DONE |
| TH-4 (Colour) | Shadow rgba(0,0,0,.55) 8px blur y+2 — knob uses radius 6, thumb 5; unverified | n/a | `Theme.cpp` `DropShadow (Palette::shadow, 6, {0,2})` | - (no test of shadow radius) | PARTIAL |
| TH-5 (Type) | Inter UI font, weights 500/600 (Lato + Bebas ship; `Theme.theBundledFontsLoad`) - superseded by DECISIONS 2026-09-23 (guitar-shop theme, visual-polish 6) | n/a | `Theme.cpp:Fonts::ui/display` | `Theme.theBundledFontsLoad` | DEFERRED |
| TH-6 (Type) | Numeric readouts JetBrains/IBM Plex Mono, tabular | n/a | `Theme.cpp:Fonts::mono` (JetBrains → Plex → fallbacks) | - | NO-TEST |
| TH-7 (Type) | Labels 11px uppercase +0.08em; values 13-14px — tracking 0.06em; knob value 12px mono | n/a | `Fonts::drawTrackedText`; `LuthierKnob::paint` | - (no test of label tracking or value size) | PARTIAL |
| TH-8 (Knobs) | 48/36/64 px knob sizes | n/a | `Theme.h:Metrics::knobDefault/Small/Large` | `Theme::knobSizesAreTheSpecsThree` | DONE |
| TH-9 (Knobs) | Flat radial-gradient body #232833→#14181F — superseded: black bell knob (visual-polish 6.3); flat only in High Contrast | n/a | `LuthierLookAndFeel::drawRotarySlider` | `Theme.highContrastIsFlat` | PARTIAL |
| TH-10 (Knobs) | 2px indicator line centre→rim | n/a | `drawRotarySlider` pointer (2.0f) | `Theme::knobDrawsItsValueAndFeelsRight` (render differs min vs max) | DONE |
| TH-11 (Knobs) | 270° value arc, 3px, 4px outside gap, unfilled edge 60% | n/a | `Metrics::arcStart/arcEnd/arcThickness/arcGap`; `drawRotarySlider` | `Faces.*` (uses arcGap/arcThickness in `FacesTests.cpp:430`) | DONE |
| TH-12 (Knobs) | 4px centre dot, accent when active, muted at default | n/a | `drawRotarySlider` centre dot | `Theme::knobDrawsItsValueAndFeelsRight` | DONE |
| TH-13 (Knobs) | Double-click resets; right-click value entry | n/a | `LuthierKnob` (`setDoubleClickReturnValue` via attachment); `Widgets.cpp:showParameterContextMenu` "Enter value..." | `MidiLearn.*` (menu path only) | NO-TEST |
| TH-14 (Knobs) | Label below; value above only on hover/drag | n/a | `Widgets.cpp:LuthierKnob::paint` | `Theme::knobDrawsItsValueAndFeelsRight` (value row changes on hover) | DONE |
| TH-15 (Sliders) | 4px track, accent fill, 16x24 thumb, 1px accent stroke; ticks outside — no ticks drawn; brass thumb (visual-polish) | n/a | `LuthierLookAndFeel::drawLinearSlider` | - | PARTIAL |
| TH-16 (Buttons) | 4px radius, 28px tall; off = panel/muted; on = 15% accent fill, accent text + border | n/a | `Metrics::buttonHeight`; `drawButtonBackground` | `Theme::radiiAndButtonHeightMatchTheSpec` (28 px) | PARTIAL |
| TH-17 (Buttons) | 100ms accent flash on momentary press — tint only while held, no timed flash | n/a | `drawButtonBackground` `isDown` | - | PARTIAL |
| TH-18 (Meters) | Smooth gradient teal→orange→yellow→red | n/a | `Widgets.cpp:LevelMeter::paint`, `meterColourFor` | `Theme::meterGradientRunsCoolToRed` | DONE |
| TH-19 (Meters) | Peak hold 1px, 1.5s hold, 20dB/s fall; mono peak readout top-right | n/a | `LevelMeter::timerCallback` (45 ticks @30Hz), readout | - | NO-TEST |
| TH-20 (Layout) | 8px grid (4 fine) | n/a | `Metrics::grid/gridHalf` | `EasyLayout.*`, `FacesIntegration.*` | DONE |
| TH-21 (Layout) | 1px separators, no boxes-in-boxes — walnut panels with screws (visual-polish) | n/a | `AdvancedPanel` column paint | - | PARTIAL |
| TH-22 (Layout) | Section header uppercase + 2x12 accent bar — bar only in High Contrast; brass plate otherwise | n/a | `LuthierLookAndFeel::drawSectionHeader` | - | PARTIAL |
| TH-23 (Layout) | Corner radii 6/4/2 | n/a | `Metrics::windowCorner/panelCorner/controlCorner` | `Theme::radiiAndButtonHeightMatchTheSpec` | DONE |
| TH-24 (Layout) | ≥16px window-edge padding | n/a | `Metrics::windowPadding` | `EasyLayout.*` | DONE |
| TH-25 (Layout) | No skeuomorphism / faux wood / metal — superseded (DECISIONS 2026-09-23) | n/a | `Palette::textured` | `Theme.highContrastIsFlat` | PARTIAL |
| TH-26 (Feel) | Value changes animate 80ms ease-out — `Metrics::animationMs` unused (also on visual) | n/a | - | - | MISSING |
| TH-27 (Feel) | Hover brightens (12%, spec ~8%); vertical-resize cursor on knobs (done) | n/a | `LuthierKnob` slider `UpDownResizeCursor`; hover 12% | `Theme::knobDrawsItsValueAndFeelsRight` (cursor) | PARTIAL |
| TH-28 (Feel) | Vertical drag; Shift coarse; Ctrl/Cmd ultra-fine | n/a | `LuthierKnob::KnobSlider::mouseDrag` | `Theme::knobDrawsItsValueAndFeelsRight` (Shift 70 < 180 < Ctrl/Cmd 1200) | DONE |
| TH-29 (Feel) | Tooltip dark pill, muted text, 400ms | n/a | `Metrics::tooltipDelayMs`; `LuthierLookAndFeel::drawTooltip` | `Theme::tooltipAppearsAfter400ms` (constant + window present; no JUCE getter for the delay) | PARTIAL |
| TH-30 (Header) | 32px header strip — 48px per spec.md (header wins) | n/a | `Metrics::headerHeight` | - | PARTIAL |
| TH-31 (Header) | Name left; gear, preset selector, A/B right; separator — File menu instead of gear | n/a | `HeaderBar` (`presetName`, `compareA/B`, `fileMenuButton`) | - (`Editor.itLaysOutAndPaintsAcrossItsResizeRange` paints only) | NO-TEST |
| TH-32 (Signature) | 2px accent diagonal notch top-left (brass headstock mark, `drawSignatureNotch`) - superseded by DECISIONS 2026-09-23 (guitar-shop theme, visual-polish 6) | n/a | `LuthierLookAndFeel::drawSignatureNotch`; `PluginEditor::paint` | - | DEFERRED |
| TH-33 (Signature) | Version bottom-right 9px muted mono | n/a | `PluginEditor::paint` footer `Fonts::mono (9.0f)` | - | NO-TEST |
| TH-34 (Anim) | Output LED: grey at -inf → white near 0dB; red above 0dB | `MasterBus::getPeakDb` | header `HeaderBar::led` (`Widgets.cpp:OutputLed`) | - | NO-TEST |
| TH-35 (Anim) | ~10-line matrix data stream in empty space: real data, green, faded edges, stops when idle, off by Options toggle | `DataStreamDisplay` (`UI/Widgets.cpp`) | `PluginEditor.cpp` dataStream child; Options > APPEARANCE `dataStreamToggle` | `DataStream::itKeeps200StopsAfter500msAndHonoursReducedMotion` | DONE |

<!-- counts DONE=12 NO-GUI=0 NO-TEST=6 PARTIAL=15 MISSING=1 OWNED=0 DEFERRED=0 -->
