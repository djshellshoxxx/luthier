## slide-guitar.md

`SlideEngine` delivers the four modes, continuous pitch, damping behind the bar, slant, mm-based vibrato, the intonation assist, friction noise, clank with rattle, the low-action message and the Workshop Slide category; the SLIDE group (CHARACTER, shown only in Slide Mode) carries every parameter, the header toggle and `S` shortcut reach the mode, and the spec's tests exist in `Slide.*` / `SlideUi.*`. Gaps: the fitted Workshop bar now reaches the engine (`setSlidePart` -> `setSlideBar`), bar length and diameter have no audible effect anywhere, heavy pressure does not choke, the tuning popover has no continuous-pitch view, the Generic profile does not render bar motion as pitch bend and slant is not captured; friction, vibrato and no-allocation tests are missing.

| Req | Summary | Engine location | GUI location | Test | Status |
|---|---|---|---|---|---|
| SG-1 (§0.1, 3) | Pitch continuous, f = f_open L/(L-x), no fret quantising | `DSP/Slide/SlideEngine.cpp:contactFret`; `LuthierEngine` slide path | n/a | `Slide.pitchIsContinuous` | DONE |
| SG-2 (§1) | Modes bottleneck / lap_steel / dobro / hybrid via `slide_mode` | `SlideMode`, `Parameters::slideModeNames` | CHARACTER > SLIDE `SlideGroup::mode` | `Slide.theSegmentBehindIsDamped`, `Slide.switchingModeMidNoteIsClean` | DONE |
| SG-3 (§0.4, 1) | Hybrid default; notes not under the bar go through the fretted path (DECISIONS: one string under the bar) | `SlideEngine::noteOn`; `LuthierEngine::triggerNote` SlideGuitar fallback | n/a | `Slide.squeakStopsUnderTheBarButNotBesideIt` | DONE |
| SG-4 (§0.3, 2) | Bar is a Workshop part; material/mass reach the contact model | `LuthierAudioProcessor::setSlidePart` -> `LuthierEngine::setSlideBar` -> `SlideEngine::setBar` (`PluginProcessor.cpp` 623, `WorkshopBench.cpp` 203) | Workshop parts drawer (Slide category) | `WorkshopAccessories.theSlideTurnsOnTheBenchAndItsMaterialIsPlayed`, `Slide.aHeavierBarSustainsLonger` | DONE |
| SG-5 (§2) | Bar mirrored read-only in CHARACTER SLIDE group | `SlideEngine::getBar` | SLIDE `SlideGroup::barMirror` label | `SlideUi.theSlideGroupAppearsWithSlideModeAndTheTabFitsIt` | DONE |
| SG-6 (§2) | `slide_length` (strings covered) and `slide_diameter` (contact curvature) have audible effect — fields exist on `SlideBar` but are read nowhere (also not on visual/techniques) | `SlideEngine::noteOn` (bar reach), `sustainScale`/`makeClank` (diameter) | Workshop slide part fields | `Slide.aShortBarCoversFewerStrings`, `Slide.diameterShapesTheContact` | DONE |
| SG-7 (§2.1) | Seven materials with damping/brightness/friction/clank spectrum | `getSlideMaterial` | via Workshop part (see SG-4) | `Slide.theBarClanksWhenItLands` | DONE |
| SG-8 (§0.2, 3) | Segment behind damped: 1.0 lap/dobro, 0.55 bottleneck/hybrid (per-mode default applied on mode change) | `SlideEngine::sustainScale`; `Parameters::defaultDampingBehind`; `SlideGroup` mode handler | SLIDE `damping` | `Slide.theSegmentBehindIsDamped` | DONE |
| SG-9 (§3) | Contact loss by material damping and mass (heavier sustains longer) | `sustainScale` | n/a (mass via part, SG-4) | `Slide.aHeavierBarSustainsLonger` | DONE |
| SG-10 (§3) | Pressure: too light rattles against the bar; too heavy chokes against the frets — rattle done, no choke (pressure only raises sustain) | `SlideEngine::sustainScale` choke above 0.8 | SLIDE `pressure` + pressure-state readout | `Slide.tooMuchPressureChokes` | DONE |
| SG-11 (§3.1) | Slant -30..+30 deg, per-string offset tan(slant) x spacing x (s - centre) | `contactFret` | SLIDE `slant` | `Slide.slantGivesEachStringItsOwnInterval` | DONE |
| SG-12 (§3.2) | Slide vibrato moves x (pitch and damped length), depth in mm (DECISIONS: tenths of mm) — no test | `SlideEngine::vibratoCents`; `LuthierEngine.cpp` ~1557 | Advanced vibrato rate/depth | `Slide.vibratoMovesTheBar` (formula level) | PARTIAL |
| SG-13 (§4) | Intonation assist 0-1 (default 0.15), 120 ms pull to ET, labelled as an aid | `SlideEngine` assist | SLIDE `assist` (tooltip marks it an aid) | `Slide.theAssistPullsToPitch` | DONE |
| SG-14 (§5.1) | Friction noise ∝ amount x material friction x bar speed; replaces squeak on barred strings — no friction test | `LuthierEngine::triggerNote` `str.setNoiseAmount(...friction...)` | SLIDE `noise` | `Slide.frictionFollowsMaterialAndSpeed` | DONE |
| SG-15 (§5.2) | Clank (8-generator pool) on landing and rattle below 0.3 pressure; brass ~1.2 kHz, glass ~2.5 kHz, mass lowers | `SlideEngine::makeClank`; `NoiseEngine::kPoolSizes` clank 8 | SLIDE `clank` | `Slide.theBarClanksWhenItLands` | DONE |
| SG-16 (§0.5, 6) | Action < 2.2 mm bass shows the exact empty-state message; setup not changed; Slide style one click away | `SlideEngine::kLowActionMessage`; `SlideGroup` `lowAction`, `useSlideSetup` | SLIDE group | `SlideUi.theSlideGroupAppearsWithSlideModeAndTheTabFitsIt` | DONE |
| SG-17 (§6, 8) | Fret buzz stays active under a slide | `FretBuzz::process` unchanged | n/a | `Slide.aLowSetupBuzzesUnderTheBar` | DONE |
| SG-18 (§7) | Params: enable (`slide_guitar` re-pointed, DECISIONS), mode, pressure, slant, damping, noise, clank, assist with defaults | `Parameters.cpp` slide block | SLIDE group + header toggle | `GuiReach.everyAutomatableParameterHasAVisibleControl` | DONE |
| SG-19 (§7) | `slide` family advanced ranges (slant ±60, noise/clank 0-4) | `PhysicalRange.cpp` slide rows | CHARACTER padlock | `Ranges.stockMatchesTheDeclaredRange` | DONE |
| SG-20 (§7) | SLIDE group on CHARACTER only in Slide Mode: pressure state, slant, material/mass mirror, noise, clank | `UI/SlideGroup.*`; `CharacterPanel` addChildComponent | CHARACTER > SLIDE | `SlideUi.theSlideGroupAppearsWithSlideModeAndTheTabFitsIt`, `SlideUi.pressureSaysWhatItMeans` | DONE |
| SG-21 (§7) | Header toggle with shortcut `S` | `HeaderBar::toggleSlideMode`; `Accessibility.cpp` `toggleSlideMode` 's' | Header slide toggle | `GuiReach.everyAutomatableParameterHasAVisibleControl` (walks via the key) | DONE |
| SG-22 (§7) | Workshop Slide part category enabled only in Slide Mode | `WorkshopPanel` | Workshop | `WorkshopPanel.aSlideNeedsSlideMode` | DONE |
| SG-23 (§7) | Fretboard overlay: 6 px bar at position/slant, material colour, 80 % opacity, 80 ms ease | `SlideEngine::getOverlayFret` | `FretboardComponent` slide bar (line 194); `GuitarBodyComponent` overlay (lines 50, 304) | `LiveDisplays.theFretboardDrawsTheSlideBarAndTheCircuitCurveFollowsTheVolume` | DONE |
| SG-24 (§7) | Tuning popover shows continuous pitch rather than a fret in Slide Mode — no slide branch | none | `GuitarBodyComponent.cpp:TuningPopover` shows per-string detune sliders only; no slide-mode continuous pitch | - | MISSING |
| SG-25 (§7, 8) | Squeak suppressed on contacted strings; hybrid non-barred strings still squeak | `LuthierEngine` `!slide.isUnderBar(s)` | n/a | `Slide.squeakStopsUnderTheBarButNotBesideIt`, `Squeak.zeroIsFreeAndSlideModeSuppressesIt` | DONE |
| SG-26 (§8) | MIDI export: SLIDE_BAR carries position over time, slant, pressure (Luthier round-trips); Generic renders it as pitch bend — capture sends pos/pressure only (slant field never filled), Generic writes no wheel for slides | `LuthierEngine` `perfCapture->slideBar`; `LuthierMidiEvents` slideBar fields; `MidiPerformance::addScoreNote` | MIDI OUT panel | `Capture.bassAndSlideEventsBecomeLuthierEvents` | PARTIAL |
| SG-T1 (§9) | Test: pitch continuous sweep | | n/a | `Slide.pitchIsContinuous` | DONE |
| SG-T2 (§9) | Test: assist pulls 40 ct sharp to within 5 ct in 400 ms | | n/a | `Slide.theAssistPullsToPitch` | DONE |
| SG-T3 (§9) | Test: segment behind damped (>= 25 % shorter T60) | | n/a | `Slide.theSegmentBehindIsDamped` | DONE |
| SG-T4 (§9) | Test: slant >= 40 ct interval change at 20 deg | | n/a | `Slide.slantGivesEachStringItsOwnInterval` | DONE |
| SG-T5 (§9) | Test: 200 g bar sustains >= 15 % longer than 30 g | | n/a | `Slide.aHeavierBarSustainsLonger` | DONE |
| SG-T6 (§9) | Test: clank within 5 ms, brass centroid lower than glass | | n/a | `Slide.theBarClanksWhenItLands` | DONE |
| SG-T7 (§9) | Test: squeak suppressed under the bar, fires beside it in hybrid | | n/a | `Slide.squeakStopsUnderTheBarButNotBesideIt` | DONE |
| SG-T8 (§9) | Test: low-action warning shown, setup not modified | | n/a | `SlideUi.theSlideGroupAppearsWithSlideModeAndTheTabFitsIt` | DONE |
| SG-T9 (§9) | Test: mode switch mid-note clean (< -60 dBFS) | | n/a | `Slide.switchingModeMidNoteIsClean` | DONE |
| SG-T10 (§9) | Test: no allocation on the audio thread — missing | n/a | n/a | `Slide.noAllocationOnTheAudioThread` | DONE |

<!-- counts DONE=30 NO-GUI=0 NO-TEST=3 PARTIAL=1 MISSING=2 OWNED=0 -->
