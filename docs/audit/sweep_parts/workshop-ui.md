## workshop-ui.md

The bench is largely DONE and the visual-branch work is merged: pick, slide and capo overlays and drags, nut-slot drags, per-string overrides, narrow-width fallbacks, the spoken spectrum summary, reduced-motion fades and the budget tests. Remaining gaps: pickup screw handles and tilt drag, the fret wear brush, the buzz heatmap on the bench, painted inspector rows without tooltips or per-row accessibility, the 30 ms audition return check, and untested Swap/Revert, saddle, auto-zoom and drawer-empty paths.

| Req | Summary | Engine location | GUI location | Test | Status |
|---|---|---|---|---|---|
| WU-1 (§0.6) | The bench is not modal; the instrument keeps playing | `WorkshopPanel` (non-modal) | Adv WORKSHOP tab / Easy wrench overlay | `WorkshopPanel::itPaintsAndTheWorkshopTabTakesOverColumnsThreeAndFour` | DONE |
| WU-2 (§0.3) | Three feedbacks for every visible interaction - pickup, string, nut, pick, slide, capo covered | `WorkshopBench`, `SpectrumDelta` | bench | `WorkshopBench::aMovedPickupIsSeenReadAndHeard`, `WorkshopStrings::aPerStringOverrideIsSeenReadHeardAndOneEntry`, `WorkshopNut::aSlotDragIsOneEntryInRealUnitsAndReachesTheEngine`, `WorkshopAccessories::*` | DONE |
| WU-3 (§1) | Layout: header, illustration, inspector, drawer, setup strip, spectrum | `WorkshopPanel::resized` | WORKSHOP | `WorkshopPanel::itPaintsAndTheWorkshopTabTakesOverColumnsThreeAndFour` | DONE |
| WU-4 (§1) | Header: name (modified), Save As Guitar, A/B | `WorkshopPanel` `guitarName/saveAsButton/slotButtons` | WORKSHOP header | `WorkshopPresets::saveAsGuitarWritesAFileAndPointsThePresetAtIt`, `WorkshopBench::abRecallRoundTrips` | DONE |
| WU-5 (§1) | Below 900 the inspector collapses to a drawer; below 700 the drawer becomes a dropdown | `WorkshopPanel::resized` | bench | `WorkshopLayout::theBenchCollapsesItsInspectorAndDrawerWhenNarrow` | DONE |
| WU-6 (§1) | Easy overlay works down to the window minimum | `WorkshopOverlay` | Easy wrench | `WorkshopEditor::theWrenchOpensTheBenchInEasyModeAndTheTabInAdvanced` | DONE |
| WU-7 (§2) | Ruler: mm from the saddle under the pickup rail | `BenchIllustration::paint` | bench | `WorkshopPanel::aPickupDragIsOneEntryAndTheRulerValueFollows` | DONE |
| WU-8 (§2) | Live overlays: pick at its angle, slide at its slant, capo at its fret | `GuitarRenderer::pickPath/slidePath/capoPath` | bench | `WorkshopAccessories::thePickIsDraggedAndTurnedOnTheBench`, `theSlideTurnsOnTheBenchAndItsMaterialIsPlayed`, `theCapoIsDrawnAndDraggedByFrets` | DONE |
| WU-9 (§2) | Buzz heatmap overlay when the setup strip has focus | - (heatmap only in `BuzzHeatmap`, `UI/SetupGroup.cpp`) | - | - | MISSING |
| WU-10 (§2) | Repaint budget 8 ms full / 2 ms overlay, overlays a separate layer - overlay budget untested | cached scene + `paintOverlay` | n/a | `GuitarIllustration::fullRenderIsFastEnough` | NO-TEST |
| WU-11 (§3.1) | Hover outline + tooltip name and summary value; hover never selects or flickers the inspector | `BenchIllustration` hovered vs selected | bench | `WorkshopPanel::hoverDoesNotSelect`, `Editor::everyHitRegionOnTheIllustrationDescribesItself` | DONE |
| WU-12 (§3.1) | Click selects, sticky, full outline | `BenchIllustration::select` | bench | `WorkshopPanel::theInspectorShowsTheSelectedPart` | DONE |
| WU-13 (§3.2) | Alt-hover card audition on a shadow spec; inspector greyed; delta shown | `WorkshopPanel::hoverCard`, `WorkshopBench` audition | drawer | `WorkshopPanel::auditionFromTheDrawerNeverCommits`, `WorkshopBench::auditionNeverCommits` | DONE |
| WU-14 (§3.2) | Release crossfades back over 30 ms - no timing check | `WorkshopBench` audition (release path) | drawer | - | NO-TEST |
| WU-15 (§3.3) | Per-string select shows the set plus override fields; overridden string drawn in its material | `StringOverride`, `WorkshopPanel` strings inspector | bench inspector | `WorkshopStrings::aCardOntoAStringOverridesItAndASetClearsOverrides`, `anOverriddenStringIsDrawnInItsOwnMaterial` | DONE |
| WU-16 (§4) | Hit regions generated from drawing geometry | `GuitarScene` hits | bench | `GuitarIllustration::hitTestingFindsThePartOnTop` | DONE |
| WU-17 (§4) | Pickup drag along the string axis: 1 mm, Shift 0.1 mm, Alt free; collision stop with reason | `BenchIllustration` `Drag::pickup`, `WorkshopBench::movePickup` | bench | `WorkshopBench::snapIsOneMillimetreFineWithShiftFreeWithAlt`, `WorkshopBench::aPickupStopsBeforeItOverlapsAndSaysWhy` | DONE |
| WU-18 (§4) | Pickup height by screw handles or scroll, 0.1 mm, 0.5-6 mm - scroll and keys only, no screw handles | `BenchIllustration::mouseWheelMove`; Up/Down keys; `WorkshopBench::setPickupHeights` | bench (wheel; no screw handles) | `WorkshopBench::heightsAndSetupEditsAreOneEntryEach`, `WorkshopRanges::theTabCarriesAPadlockAndHeightsStopAtStock` | PARTIAL |
| WU-19 (§4) | Pickup tilt by dragging one screw handle - Shift/Alt wheel only, no handle drag, no test | `mouseWheelMove` (Shift treble / Alt bass); no tilt drag | bench | - | PARTIAL |
| WU-20 (§4) | Bridge saddles drag +/-6 mm - drag and key path untested | `Drag::saddle` -> `WorkshopBench::setIntonation`; keyboard bridge left/right | bench | `WorkshopBench::heightsAndSetupEditsAreOneEntryEach` (setIntonation only) | NO-TEST |
| WU-21 (§4) | Nut slots drag down per string, 0.05 mm, 0-1.2 mm — setup-strip knobs on base | `Drag::nut`, nut keys | bench | `WorkshopNut::aSlotDragIsOneEntryInRealUnitsAndReachesTheEngine` | DONE |
| WU-22 (§4) | Frets: click to select, brush to wear - select only; no brush on the bench | `CharacterEngine` fret wear (not wired to the bench) | CHARACTER (not bench); bench frets select only | - | PARTIAL |
| WU-23 (§4) | Pick drag + rotate at its corner; slide drag + slant; capo drag 0-12 | `Drag::pick/pickRotate/slide/slideRotate/capo` | bench | `WorkshopAccessories::*` (3 tests) | DONE |
| WU-24 (§4) | Live value follows the pointer in the inspector; comb notches live during a pickup drag | `onPickupDragged`, `SpectrumDelta::combNotches` | bench + spectrum | `WorkshopPanel::aPickupDragIsOneEntryAndTheRulerValueFollows`, `WorkshopSpectrum::combNotchesSitWhereThePickupIsANode` | DONE |
| WU-25 (§5) | Inspector: name, origin, fields with units, compatibility | `WorkshopPanel::refreshInspector` | inspector | `WorkshopPanel::theInspectorShowsTheSelectedPart` | DONE |
| WU-26 (§5) | Editing a factory part makes a user copy and offers "Save as user part" | `WorkshopBench::editField` | inspector `savePartButton` | `WorkshopPanel::editingAFieldMakesAUserCopy`, `WorkshopPresets::saveAsPartMakesAUserPartAndFitsIt` | DONE |
| WU-27 (§5) | Swap (drawer filtered) and Revert controls - untested | `WorkshopPanel` `swapButton/revertButton` | inspector | - | NO-TEST |
| WU-28 (§5) | Fields carry parameter-like tooltips and accessibility - buttons do, painted inspector rows are not components | - (inspector rows are painted; buttons/toggles carry tooltips) | inspector (painted rows) | - | PARTIAL |
| WU-29 (§5) | Plain clamps from part-acoustics, no stock/advanced marking | `Part` / `PartAcoustics` | inspector | `PartAcoustics::theMappingIsMonotonic` | DONE |
| WU-30 (§6) | Fixture render committed vs candidate, dB delta; flat 0 dB shown plainly | `Workshop/SpectrumDelta` | spectrum pane | `WorkshopSpectrum::aRealChangeShowsAndIsDescribed`, `WorkshopSpectrum::aNullChangeIsFlat` | DONE |
| WU-31 (§6) | Y axis fixed +/-12 dB with auto-zoom toggle - untested | `paintSpectrum`, `autoZoomToggle` | spectrum pane | - | NO-TEST |
| WU-32 (§6) | Worker thread, 40 ms budget, coalesced (latest wins) | `SpectrumDelta` (`juce::Thread`) | n/a | `WorkshopSpectrum::theWorkerCoalescesAndStaysInBudget` | DONE |
| WU-33 (§7) | Eight A/B slots: store/recall (undoable), uiState, Shift-click clears | `UiState::benchSlots` | header slot buttons | `WorkshopBench::abRecallRoundTrips` | DONE |
| WU-34 (§8) | One undo entry per commit with real-unit text; drag = one entry; swaps not grouped; audition never pushes | `WorkshopBench` | n/a | `WorkshopBench::aDragIsOneUndoEntryWithItsBeforeAndAfter`, `WorkshopBench::fittingAPartSaysWhatItReplaced`, `WorkshopBench::aClickWithoutAMoveChangesNothing` | DONE |
| WU-35 (§9) | Empty user-parts text; one-option category still shown; incompatible card warns and still auditions - untested | `WorkshopPanel::paintDrawer` ("No ... parts are installed") | drawer | - | NO-TEST |
| WU-36 (§9) | Slide category greyed with "Turn on Slide Mode (S)" | `WorkshopPanel.cpp` drawer | drawer Slide | `WorkshopPanel::aSlideNeedsSlideMode` | DONE |
| WU-37 (§10) | Every hit region focusable with name and value, in builder order - one component, not one per region | `BenchIllustration::builderOrder`, `setDescription` | bench Tab (one component whose description changes) | `WorkshopPanel::keyboardNudgesMatchADrag` | PARTIAL |
| WU-38 (§10) | Arrow nudge by snap, Shift fine | `BenchIllustration::keyPressed` | bench | `WorkshopPanel::keyboardNudgesMatchADrag` | DONE |
| WU-39 (§10) | Spectrum announced as a sentence | `getSpectrumSummary`; `postAnnouncement` | spectrum pane | `WorkshopSpectrum::theSummaryIsAnnouncedForScreenReaders` | DONE |
| WU-40 (§10) | Everything reachable by drag reachable by keyboard - pickup tilt and fret wear have no key path; saddle keys untested | `BenchIllustration::keyPressed` (pickup, height, saddle, nut, accessories) | bench | `WorkshopPanel::keyboardNudgesMatchADrag` | PARTIAL |
| WU-T1 (§11) | Every part hit-testable; nothing else reported | - | n/a | `WorkshopPanel::everyFittedPartIsReachableAndNothingElseIs` | DONE |
| WU-T2 (§11) | Hover does not select, pushes nothing | - | n/a | `WorkshopPanel::hoverDoesNotSelect` | DONE |
| WU-T3 (§11) | Audition does not commit, and audio returns within 30 ms - no audio-return check | - | n/a | `WorkshopBench::auditionNeverCommits` | PARTIAL |
| WU-T4 (§11) | Drag = one undo entry with before/after; drag constrained; snap 7.4 -> 7 / 7.4 | - | n/a | `WorkshopBench::aDragIsOneUndoEntryWithItsBeforeAndAfter`, `WorkshopBench::aPickupStopsBeforeItOverlapsAndSaysWhy`, `WorkshopBench::snapIsOneMillimetreFineWithShiftFreeWithAlt` | DONE |
| WU-T5 (§11) | Three feedbacks for each draggable part — pickup only on base | - | n/a | `WorkshopBench::aMovedPickupIsSeenReadAndHeard`, `WorkshopNut::*`, `WorkshopAccessories::*`, `WorkshopStrings::*` | DONE |
| WU-T6 (§11) | Null change is flat within +/-0.05 dB | - | n/a | `WorkshopSpectrum::aNullChangeIsFlat` | DONE |
| WU-T7 (§11) | Spectrum delta within 40 ms for every factory part swap | - | n/a | `WorkshopSpectrum::hundredShadowRendersStayUnder40ms` | DONE |
| WU-T8 (§11) | Nothing on the audio thread during bench interaction (no FS, allocation, spectrum work) | - | n/a | `WorkshopSwap::noFileIsTouchedFromTheAudioThreadDuringASwap`, `WorkshopBench::hundredAuditionsStayInBudget` | DONE |
| WU-T9 (§11) | A/B recall round-trips after six part changes | - | n/a | `WorkshopBench::abRecallRoundTrips` | DONE |
| WU-T10 (§11) | Keyboard parity for every drag interaction - pickup, height, nut, accessories only; tilt and saddle keys untested | - | n/a | `WorkshopPanel::keyboardNudgesMatchADrag` | PARTIAL |

<!-- counts DONE=35 NO-GUI=0 NO-TEST=6 PARTIAL=8 MISSING=1 DEFERRED=0 -->
