## fret-buzz.md

The setup geometry and sensed buzz are in and well tested: `SetupGeometry::clearanceMm` (nut-to-saddle line plus a parabolic relief peaking at fret 7, only frets past the finger), block-rate `FretBuzz::sense/process` feeding metallic burst-at-f0 generators, the threshold trim, sitar mode, the six setup styles and the live heatmap, all in CHARACTER > SETUP (`SetupGroup`), with action/relief/nut mirrored in the Workshop. Missing: fret material does not set brightness (no fret-material field exists), fret wear does not move buzz, bends do not change clearance, and there are no budget, no-allocation or heatmap-staleness tests.

| Req | Summary | Engine location | GUI location | Test | Status |
|---|---|---|---|---|---|
| FB-1 (§0.1, 3.1) | Buzz sensed from amplitude vs clearance, never scheduled; worst fret per string per block | `DSP/Noise/FretBuzz.cpp:FretBuzz::sense/process`; called in `LuthierEngine` block loop | n/a | `Buzz.lowActionBuzzesAndHighActionDoesNot` | DONE |
| FB-2 (§0.2, 1) | Geometry in mm: action treble/bass, relief, nut depth x6, fret height with spec ranges/defaults | `Parameters.cpp` setup block -> `SetupGeometry` (`Parameters.cpp` ~1176) | CHARACTER > SETUP (`SetupGroup` sliders); Workshop setup strip (`WorkshopPanel` actionTreble/actionBass/relief/nut) | `BuzzUi.setupStylesApplyAsOneStepAndReadModified`, `GuiReach.everyAutomatableParameterHasAVisibleControl` | DONE |
| FB-3 (§1) | Clearance: nut -> 12th interpolation plus relief parabola at fret 7 | `SetupGeometry::clearanceMm` | n/a | `Buzz.reliefMovesWhereItBuzzes` | DONE |
| FB-4 (§1) | Only frets between the finger and the bridge can buzz | `clearanceMm` (fret <= fretted -> 1e9) | n/a | `Buzz.onlyFretsAheadOfTheFingerBuzz` | DONE |
| FB-5 (§2) | Modal amplitude sum_k A_k sin(k pi x/L) at each fret, block rate | `FretBuzz::displacementMm` (3 modes, pluck weights) | n/a | `Buzz.theHeatmapAgreesWithTheGenerator` | DONE |
| FB-6 (§3.2) | `setup_buzz_threshold` trims +/-0.15 mm, not a mute | `sense` (trim = (t-0.5) x 0.3) | SETUP `threshold` | `Buzz.theThresholdIsATrimNotAMute` | DONE |
| FB-7 (§4) | Generator: burst per contact at the fundamental while excess > 0 | `process` (`NoiseEvent::burstHz`) | n/a | `Buzz.buzzStopsAsTheNoteDecays` | DONE |
| FB-8 (§4) | Spectrum metallic 3-6 kHz, centre rising with contact fret — no test | `process` (startHz 3-6 kHz by fret, metallic texture) | n/a | - | NO-TEST |
| FB-9 (§4) | Fret material sets brightness (nickel-silver dull, stainless bright, EVO between) — no fret-material field exists | none | n/a | - | MISSING |
| FB-10 (§4) | Level min(1, excess/0.3) scaled by fret height | `FretBuzz::levelFor` | SETUP `fretHeight` | `Buzz.fretHeightChangesLevelNotPosition` | DONE |
| FB-11 (§4) | Envelope 0.5 ms attack, decay tracks excess (buzzes on attack, cleans up) | `process` (setSustainLevel / release) | n/a | `Buzz.buzzStopsAsTheNoteDecays` | DONE |
| FB-12 (§4) | Injection pre-body, also Aux 8 | `NoiseEngine::processSample` surface path | n/a | `PluginBuses.aux8CarriesThePlayingNoiseAndObeysItsStrip` | DONE |
| FB-13 (§0.4) | Light buzz 30-40 dB under the note — no level-vs-note test | `levelFor` (-22 dB ref x excess/0.3) | n/a | - | NO-TEST |
| FB-14 (§0.5, 5) | Sitar mode: continuous grazing contact, threshold bypassed, long decay | `process` sitarMode branch (decay 400 ms) | SETUP `sitarMode` toggle | `Buzz.sitarModeIsContinuous` | DONE |
| FB-15 (§6) | SETUP group on CHARACTER: action T/B, relief, nut x6, fret height, threshold, sitar, heatmap | `UI/SetupGroup.*` | CHARACTER > SETUP (`CharacterPanel::setupGroup`) | `BuzzUi.setupStylesApplyAsOneStepAndReadModified`, `GuiReach.everyAutomatableParameterHasAVisibleControl` | DONE |
| FB-16 (§6.1) | Six setup styles, Player-friendly ship default, "(modified)" | `getSetupStyle`; `SetupGroup::applySetupStyle/describeSetupStyle` | SETUP style dropdown | `BuzzUi.setupStylesApplyAsOneStepAndReadModified`, `Buzz.playerFriendlyBuzzesOnlyWhenAttackedHard` | DONE |
| FB-17 (§6.2) | Heatmap: warning near (within 0.05 mm), accent buzzing, dot glyph | `FretBuzz::getHeat/getBuzzingFret`; `BuzzHeatmap::stateFor/paint` | SETUP `heatmap` | `BuzzUi.heatmapCellsReadInMonochromeTerms`, `Buzz.theHeatmapAgreesWithTheGenerator` | DONE |
| FB-18 (§6.2) | Heatmap updates at 30 Hz, greys after 2 s stale — implemented, untested | `BuzzHeatmap` (startTimerHz 30, isStale) | SETUP `heatmap` | - | NO-TEST |
| FB-19 (§7) | Params with stock/advanced ranges in `buzz` family, +16 | `PhysicalRange.cpp` buzz rows | CHARACTER padlock | `Ranges.stockMatchesTheDeclaredRange`, `Ranges.everyPhysicalRangeIsValid` | DONE |
| FB-20 (§7) | Legacy `fret_action` superseded by the geometry | `LuthierEngine::setSetupGeometry` (in-loop clipper from same setup) | hidden (GuiReach `intentionallyHidden`) | `GuiReach.everyAutomatableParameterHasAVisibleControl` | DONE |
| FB-21 (§8) | Fret wear: worn fret lower (less buzz there), uneven crown raises neighbours — not implemented | none (`CharacterEngine` wear affects sustain only) | n/a | - | MISSING |
| FB-22 (§8) | Slide Mode suggests the Slide setup style | `SlideGroup` `useSlideSetup` + low-action warning | CHARACTER > SLIDE | `SlideUi.theSlideGroupAppearsWithSlideModeAndTheTabFitsIt` | DONE |
| FB-23 (§8) | Bass defaults lower action (Factory low) | `Model/Guitar/BassDefaults.cpp` setupStyle 0 | n/a | `BassTechniques.bassDefaultsApplyOnLoad` | DONE |
| FB-24 (§8) | Slap/pop drive the string into the frets (clack via buzz generator) | `SlapEngine::makeContactBuzz` with `fretBuzzModel` | n/a | `Slap.theClackIsTheFretBuzzGenerator`, `SlapWiring.theClackComesFromTheBuzzGenerator` | DONE |
| FB-25 (§8) | Squeak and buzz both fire, no ducking — untested | separate pools | n/a | - | NO-TEST |
| FB-26 (§8) | Bends lift the string: less buzz at the fretted position, more further up — not implemented (`process` gets `currentFret`, no bend input) | none | n/a | - | MISSING |
| FB-T1 (§9) | Test: low action buzzes, high does not | | n/a | `Buzz.lowActionBuzzesAndHighActionDoesNot` | DONE |
| FB-T2 (§9) | Test: buzz stops as the note decays | | n/a | `Buzz.buzzStopsAsTheNoteDecays` | DONE |
| FB-T3 (§9) | Test: only frets ahead of the finger | | n/a | `Buzz.onlyFretsAheadOfTheFingerBuzz` | DONE |
| FB-T4 (§9) | Test: relief moves where it buzzes (>= 3 frets) | | n/a | `Buzz.reliefMovesWhereItBuzzes` | DONE |
| FB-T5 (§9) | Test: fret height changes level not position | | n/a | `Buzz.fretHeightChangesLevelNotPosition` | DONE |
| FB-T6 (§9) | Test: threshold is a trim | | n/a | `Buzz.theThresholdIsATrimNotAMute` | DONE |
| FB-T7 (§9) | Test: sitar mode continuous | | n/a | `Buzz.sitarModeIsContinuous` | DONE |
| FB-T8 (§9) | Test: heatmap matches audio — random 400-block run, not a 30 s engine performance | | n/a | `Buzz.theHeatmapAgreesWithTheGenerator` | DONE |
| FB-T9 (§9) | Test: block-rate sensing within 0.2-unit budget at 6 voices — missing (visual's `PerfBudget.everyModuleWithinBudget` has no FretBuzz row) | | n/a | - | NO-TEST |
| FB-T10 (§9) | Test: no allocation on the audio thread — missing | | n/a | - | NO-TEST |

<!-- counts DONE=27 NO-GUI=0 NO-TEST=6 PARTIAL=0 MISSING=3 OWNED=0 -->
