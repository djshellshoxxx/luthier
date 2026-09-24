## workshop-ui.md

The bench is largely DONE on this checkout. Hover never selects, Alt-hover auditions on a shadow spec, and pickup drags snap and stop on collision. The inspector offers Swap, Revert and Save as user part, the spectrum delta runs on a worker, there are eight A/B slots, and undo takes one entry per commit in real units. `claude/luthier-visual` has substantial bench work that is not here. It adds the pick, slide and capo overlays and drags, nut-slot drags, per-string overrides, the narrow-width fallbacks, the spoken spectrum summary, the reduced-motion bench fades, the WORKSHOP padlock and the budget tests. Those rows are OWNED. On neither branch: screw-handle height and tilt, the fret wear brush, the buzz heatmap, painted inspector rows with tooltips and accessibility, the 30 ms audition return test, and a saddle keyboard path.

| Req | Summary | Engine location | GUI location | Test | Status |
|---|---|---|---|---|---|
| WU-1 (§0.6) | The bench is not modal; the instrument keeps playing | `WorkshopPanel` (non-modal) | Adv WORKSHOP tab / Easy wrench overlay | `WorkshopPanel::itPaintsAndTheWorkshopTabTakesOverColumnsThreeAndFour` | DONE |
| WU-2 (§0.3) | Three feedbacks for every visible interaction — pickup only on base; nut/pick/slide/capo/strings on visual | `WorkshopBench`, `SpectrumDelta` | bench | `WorkshopBench::aMovedPickupIsSeenReadAndHeard`; (visual) `WorkshopStrings::aPerStringOverrideIsSeenReadHeardAndOneEntry` | OWNED |
| WU-3 (§1) | Layout: header, illustration, inspector, drawer, setup strip, spectrum | `WorkshopPanel::resized` | WORKSHOP | `WorkshopPanel::itPaintsAndTheWorkshopTabTakesOverColumnsThreeAndFour` | DONE |
| WU-4 (§1) | Header: name (modified), Save As Guitar, A/B | `WorkshopPanel` `guitarName/saveAsButton/slotButtons` | WORKSHOP header | `WorkshopPresets::saveAsGuitarWritesAFileAndPointsThePresetAtIt`, `WorkshopBench::abRecallRoundTrips` | DONE |
| WU-5 (§1) | Below 900 the inspector collapses to a drawer; below 700 the drawer becomes a dropdown | (visual) `WorkshopPanel::resized` | (visual) bench | (visual) `WorkshopLayout::theBenchCollapsesItsInspectorAndDrawerWhenNarrow` | OWNED |
| WU-6 (§1) | Easy overlay works down to the window minimum | `WorkshopOverlay` | Easy wrench | (visual) `WorkshopEditor::theWrenchOpensTheBenchInEasyModeAndTheTabInAdvanced` | OWNED |
| WU-7 (§2) | Ruler: mm from the saddle under the pickup rail | `BenchIllustration::paint` | bench | `WorkshopPanel::aPickupDragIsOneEntryAndTheRulerValueFollows` | DONE |
| WU-8 (§2) | Live overlays: pick at its angle, slide at its slant, capo at its fret | (visual) `GuitarRenderer::pickPath/slidePath/capoPath` | (visual) bench | (visual) `WorkshopAccessories::thePickIsDraggedAndTurnedOnTheBench`, `WorkshopAccessories::theSlideTurnsOnTheBenchAndItsMaterialIsPlayed`, `WorkshopAccessories::theCapoIsDrawnAndDraggedByFrets` | OWNED |
| WU-9 (§2) | Buzz heatmap overlay when the setup strip has focus — no branch | - | - | - | OWNED |
| WU-10 (§2) | Repaint budget 8 ms full / 2 ms overlay, overlays a separate layer — overlay budget untested | cached scene + `paintOverlay` | n/a | `GuitarIllustration::fullRenderIsFastEnough` | OWNED |
| WU-11 (§3.1) | Hover outline + tooltip name and summary value; hover never selects or flickers the inspector | `BenchIllustration` hovered vs selected | bench | `WorkshopPanel::hoverDoesNotSelect`, `Editor::everyHitRegionOnTheIllustrationDescribesItself` | DONE |
| WU-12 (§3.1) | Click selects, sticky, full outline | `BenchIllustration::select` | bench | `WorkshopPanel::theInspectorShowsTheSelectedPart` | DONE |
| WU-13 (§3.2) | Alt-hover card audition on a shadow spec; inspector greyed; delta shown | `WorkshopPanel::hoverCard`, `WorkshopBench` audition | drawer | `WorkshopPanel::auditionFromTheDrawerNeverCommits`, `WorkshopBench::auditionNeverCommits` | DONE |
| WU-14 (§3.2) | Release crossfades back over 30 ms — unverified | `WorkshopBench` | drawer | - | OWNED |
| WU-15 (§3.3) | Per-string select shows the set plus override fields; overridden string drawn in its material | (visual) `StringOverride`, `WorkshopPanel` | (visual) bench inspector | (visual) `WorkshopStrings::aCardOntoAStringOverridesItAndASetClearsOverrides`, `WorkshopStrings::anOverriddenStringIsDrawnInItsOwnMaterial` | OWNED |
| WU-16 (§4) | Hit regions generated from drawing geometry | `GuitarScene` hits | bench | `GuitarIllustration::hitTestingFindsThePartOnTop` | DONE |
| WU-17 (§4) | Pickup drag along the string axis: 1 mm, Shift 0.1 mm, Alt free; collision stop with reason | `BenchIllustration` `Drag::pickup`, `WorkshopBench::movePickup` | bench | `WorkshopBench::snapIsOneMillimetreFineWithShiftFreeWithAlt`, `WorkshopBench::aPickupStopsBeforeItOverlapsAndSaysWhy` | DONE |
| WU-18 (§4) | Pickup height by screw handles or scroll, 0.1 mm, 0.5-6 mm — scroll only | `BenchIllustration::mouseWheelMove` | bench (wheel) | `WorkshopBench::heightsAndSetupEditsAreOneEntryEach`; (visual) `WorkshopRanges::theTabCarriesAPadlockAndHeightsStopAtStock` | OWNED |
| WU-19 (§4) | Pickup tilt by dragging one screw handle — Shift/Alt wheel only | `mouseWheelMove` | bench | - | OWNED |
| WU-20 (§4) | Bridge saddles drag +/-6 mm — untested, no keyboard path | `Drag::saddle` | bench | - | OWNED |
| WU-21 (§4) | Nut slots drag down per string, 0.05 mm, 0-1.2 mm — setup-strip knobs on base | (visual) `Drag::nut` | (visual) bench | (visual) `WorkshopNut::aSlotDragIsOneEntryInRealUnitsAndReachesTheEngine` | OWNED |
| WU-22 (§4) | Frets: click to select, brush to wear — select only; no brush on any branch | `CharacterEngine` fret wear | CHARACTER (not bench) | - | OWNED |
| WU-23 (§4) | Pick drag + rotate at its corner; slide drag + slant; capo drag 0-12 | (visual) `Drag::pick/pickRotate/slide/slideRotate/capo` | (visual) bench | (visual) `WorkshopAccessories::*` (3 tests) | OWNED |
| WU-24 (§4) | Live value follows the pointer in the inspector; comb notches live during a pickup drag | `onPickupDragged`, `SpectrumDelta::combNotches` | bench + spectrum | `WorkshopPanel::aPickupDragIsOneEntryAndTheRulerValueFollows`, `WorkshopSpectrum::combNotchesSitWhereThePickupIsANode` | DONE |
| WU-25 (§5) | Inspector: name, origin, fields with units, compatibility | `WorkshopPanel::refreshInspector` | inspector | `WorkshopPanel::theInspectorShowsTheSelectedPart` | DONE |
| WU-26 (§5) | Editing a factory part makes a user copy and offers "Save as user part" | `WorkshopBench::editField` | inspector `savePartButton` | `WorkshopPanel::editingAFieldMakesAUserCopy`, `WorkshopPresets::saveAsPartMakesAUserPartAndFitsIt` | DONE |
| WU-27 (§5) | Swap (drawer filtered) and Revert controls — untested | `WorkshopPanel` `swapButton/revertButton` | inspector | - | OWNED |
| WU-28 (§5) | Fields carry parameter-like tooltips and accessibility — rows are painted, not components | - | - | - | OWNED |
| WU-29 (§5) | Plain clamps from part-acoustics, no stock/advanced marking | `Part` / `PartAcoustics` | inspector | `PartAcoustics::theMappingIsMonotonic` | DONE |
| WU-30 (§6) | Fixture render committed vs candidate, dB delta; flat 0 dB shown plainly | `Workshop/SpectrumDelta` | spectrum pane | `WorkshopSpectrum::aRealChangeShowsAndIsDescribed`, `WorkshopSpectrum::aNullChangeIsFlat` | DONE |
| WU-31 (§6) | Y axis fixed +/-12 dB with auto-zoom toggle — untested | `paintSpectrum`, `autoZoomToggle` | spectrum pane | - | OWNED |
| WU-32 (§6) | Worker thread, 40 ms budget, coalesced (latest wins) | `SpectrumDelta` (`juce::Thread`) | n/a | `WorkshopSpectrum::theWorkerCoalescesAndStaysInBudget` | DONE |
| WU-33 (§7) | Eight A/B slots: store/recall (undoable), uiState, Shift-click clears | `UiState::benchSlots` | header slot buttons | `WorkshopBench::abRecallRoundTrips` | DONE |
| WU-34 (§8) | One undo entry per commit with real-unit text; drag = one entry; swaps not grouped; audition never pushes | `WorkshopBench` | n/a | `WorkshopBench::aDragIsOneUndoEntryWithItsBeforeAndAfter`, `WorkshopBench::fittingAPartSaysWhatItReplaced`, `WorkshopBench::aClickWithoutAMoveChangesNothing` | DONE |
| WU-35 (§9) | Empty user-parts text; one-option category still shown; incompatible card warns and still auditions — untested | `WorkshopPanel::paintDrawer` | drawer | - | OWNED |
| WU-36 (§9) | Slide category greyed with "Turn on Slide Mode (S)" | `WorkshopPanel.cpp` drawer | drawer Slide | `WorkshopPanel::aSlideNeedsSlideMode` | DONE |
| WU-37 (§10) | Every hit region focusable with name and value, in builder order — one component whose description changes | `BenchIllustration::builderOrder` | bench Tab | `WorkshopPanel::keyboardNudgesMatchADrag` | OWNED |
| WU-38 (§10) | Arrow nudge by snap, Shift fine | `BenchIllustration::keyPressed` | bench | `WorkshopPanel::keyboardNudgesMatchADrag` | DONE |
| WU-39 (§10) | Spectrum announced as a sentence | `getSpectrumSummary`; (visual) accessibility hook | (visual) spectrum pane | (visual) `WorkshopSpectrum::theSummaryIsAnnouncedForScreenReaders` | OWNED |
| WU-40 (§10) | Everything reachable by drag reachable by keyboard — saddles, tilt have no key path | (visual) nut/accessory keys | bench | - | OWNED |
| WU-T1 (§11) | Every part hit-testable; nothing else reported | - | n/a | `WorkshopPanel::everyFittedPartIsReachableAndNothingElseIs` | DONE |
| WU-T2 (§11) | Hover does not select, pushes nothing | - | n/a | `WorkshopPanel::hoverDoesNotSelect` | DONE |
| WU-T3 (§11) | Audition does not commit, and audio returns within 30 ms — no audio-return check | - | n/a | `WorkshopBench::auditionNeverCommits` | OWNED |
| WU-T4 (§11) | Drag = one undo entry with before/after; drag constrained; snap 7.4 -> 7 / 7.4 | - | n/a | `WorkshopBench::aDragIsOneUndoEntryWithItsBeforeAndAfter`, `WorkshopBench::aPickupStopsBeforeItOverlapsAndSaysWhy`, `WorkshopBench::snapIsOneMillimetreFineWithShiftFreeWithAlt` | DONE |
| WU-T5 (§11) | Three feedbacks for each draggable part — pickup only on base | - | n/a | `WorkshopBench::aMovedPickupIsSeenReadAndHeard`; (visual) `WorkshopNut::*`, `WorkshopAccessories::*` | OWNED |
| WU-T6 (§11) | Null change is flat within +/-0.05 dB | - | n/a | `WorkshopSpectrum::aNullChangeIsFlat` | DONE |
| WU-T7 (§11) | Spectrum delta within 40 ms for every factory part swap | - | n/a | (visual) `WorkshopSpectrum::hundredShadowRendersStayUnder40ms` | OWNED |
| WU-T8 (§11) | Nothing on the audio thread during bench interaction (no FS, allocation, spectrum work) | - | n/a | `WorkshopSwap::noFileIsTouchedFromTheAudioThreadDuringASwap`; (visual) `WorkshopBench::hundredAuditionsStayInBudget` | OWNED |
| WU-T9 (§11) | A/B recall round-trips after six part changes | - | n/a | `WorkshopBench::abRecallRoundTrips` | DONE |
| WU-T10 (§11) | Keyboard parity for every drag interaction — pickup and height only on base | - | n/a | `WorkshopPanel::keyboardNudgesMatchADrag` | OWNED |

<!-- counts -->
