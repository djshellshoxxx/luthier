## proposals/visual-polish.md

Most of this approved proposal is built and tested: palette, fonts, plates, guitar lighting, amp and pedal faces, and the merged VU needle meter, room light and user/follow-the-guitar accent (`UI/StageTouches.*`, Options > Appearance, `AppearanceTests.cpp`: `Accent.*`, `StageTouches.*`, `Screenshots.everyPanelInEveryPalette`). Remaining gaps: no test that opening Advanced with every face visible stays in the UI frame budget, and no assertion that focus rings and the brand mark render in the accent.

| Req | Summary | Engine location | GUI location | Test | Status |
|---|---|---|---|---|---|
| VP-1 (§0.1, §6) | Section 6 Luthier theme replaces theme.md's look | `UI/Theme.h:Palette`, `Theme.cpp` LookAndFeel | whole UI | `Theme.theDefaultIsTheGuitarShop` | DONE |
| VP-2 (§0.2) | 4.5:1 text contrast on Default/HC/Light; HC turns textures and sheen off | `Palette::textured` | whole UI | `Theme.everyTextPairMeetsContrastOnTheThreePalettes`, `Theme.highContrastIsFlat` | DONE |
| VP-3 (§0.3) | Textures/lighting cached, redrawn only on change | `UI/Faces/*` cached images | faces | `Faces.facesRenderIdenticallyTwice`, `FacesIntegration.facesAreDrawnOnceAndAgainOnlyWhenTheyChange` | DONE |
| VP-4 (§0.4) | No motion beyond already-live elements | n/a | n/a | `Faces.facesRenderIdenticallyTwice` | DONE |
| VP-5 (§1) | Guitar key light + diffuse falloff + edge specular | `GuitarRenderer::buildLighting` | Easy illustration, WORKSHOP | `GuitarIllustration.contactSheetForReview` (by eye), `GuitarIllustration.highContrastHasNoLighting` | DONE |
| VP-6 (§1) | Lacquer sheen by gloss (>0.6 band, satin faint, oil/natural none) | `GuitarRenderer` sheen | illustration | `GuitarIllustration.contactSheetForReview` | DONE |
| VP-7 (§1) | Metal hardware reflection gradient + hot highlight per hardware colour | `GuitarRenderer` metalFill | illustration | `GuitarIllustration.contactSheetForReview` | DONE |
| VP-8 (§1) | Pickups/bridge/pickguard short soft drop shadows | `GuitarRenderer` shadow | illustration | `GuitarIllustration.contactSheetForReview` | DONE |
| VP-9 (§1 test) | High-contrast guitar has no gradient or sheen | `GuitarRenderer` | illustration | `GuitarIllustration.highContrastHasNoLighting` | DONE |
| VP-10 (§2) | Amp head face per family: Tolex, grille, faceplate, generic logo plate, pilot follows Standby | `UI/Faces/AmpFace`, `AmpFacePanel` | ADVANCED AMP; Easy rig amp card | `Faces.thePilotFollowsStandbyAndTheLedFollowsBypass`, `Faces.noFaceTextNamesABrand`, `Faces.everyAmpFaceDrawsInsideItsBoundsInEveryPalette` | DONE |
| VP-11 (§2) | Pedal faces: enclosure colour, footswitch, LED follows bypass, pedal knob layout, generic names | `UI/Faces/PedalFace`, `PedalRack` | effects racks | `Faces.everyPedalFaceDrawsInsideItsBoundsInEveryPalette`, `FacesIntegration.everyRackSlotHasItsControlsOnItsFace` | DONE |
| VP-12 (§2) | Same knobs/params; layout unaffected | faces | AMP / racks | `FacesIntegration.theAdvancedAmpSectionHasItsControlsOnTheFace`, `FacesIntegration.theEasyAmpCardHasItsKnobsOnTheFace` | DONE |
| VP-13 (§3) | Model-specific knob caps on faces only, value arc kept | `UI/Faces/KnobCaps` | faces | `Faces.everyKnobCapRendersAndKeepsTheArc` | DONE |
| VP-14 (§4) | Tube glow follows drive, greys when stale | `AmpFace` drive/driveStale | ADVANCED AMP | `FacesIntegration.theValvesGlowWithTheDriveAndGreyWhenStale` | DONE |
| VP-15 (§4) | Optional VU needle meter (header / Easy rig strip), ballistics, stale grey | `VuMeter` (`UI/StageTouches.cpp`) | Easy rig strip (`EasyPanel`), Options > Appearance toggle | `StageTouches.theVuNeedleHasBallisticsAndGreysWhenStale` | DONE |
| VP-16 (§4) | Room light: ROOM card warms/widens with size and wet | `RoomLight` (`UI/StageTouches.cpp`) | ROOM card (`EasyPanel`) | `StageTouches.theRoomLightFollowsSizeAndWet` | DONE |
| VP-17 (§5) | Default / High contrast / Light palettes complete | `Accessibility` PaletteId | Options > APPEARANCE | `Theme.everyTextPairMeetsContrastOnTheThreePalettes`, `Theme.aPaletteChangeReachesBuiltComponents` | DONE |
| VP-18 (§5) | User accent: brass + 5 others, each >= 4.5:1 on every palette | `AccessibilitySettings::getAccentNames` | Options > Appearance accent box (`OptionsPages.cpp`) | `Accent.everyChoiceMeetsContrastOnEveryPalette` | DONE |
| VP-19 (§5) | Follow-the-guitar accent from finish colour, contrast-adjusted | follow-the-guitar accent (`Accent` code) | Options > Appearance "Follow the guitar" | `Accent.theWindowTakesTheAccentAndFollowsTheGuitar` | DONE |
| VP-20 (§6.1) | Palette values #1E1511 / #2A1E17 / #EFE3CC / #B9A58A / #D4A24C / #6FA58A; Light maple/cream | `Theme.h` | whole UI | `Theme.theDefaultIsTheGuitarShop` | DONE |
| VP-21 (§6.2) | Condensed display heading face, warm sans, tabular numbers (bundled open-licence fonts) | `Resources/Fonts`, `Theme.cpp` | whole UI | `Theme.theBundledFontsLoad` | DONE |
| VP-22 (§6.2-6.3) | Engraved-plate headers; bell knob with outer arc; mini toggles; brass fader; framed panels with corner screws | `Theme::drawRotarySlider/drawLinearSlider/drawCornerScrews`, plates | whole UI | `Theme.controlsRenderInEveryPaletteAndRepeatExactly` | DONE |
| VP-23 (§6.4) | Brand mark: inlaid brass headstock outline; output LED kept - no assertion | `Theme.cpp` brand mark | header | `Screenshots.everyPanelInEveryPalette` (by eye, no assertion) | NO-TEST |
| VP-24 (§6.5) | Focus rings restyled to the accent and still visible - colour IDs set, not tested | `Theme.cpp` focusedOutlineColourId | whole UI | - | NO-TEST |
| VP-T1 (§7) | Test: every lit/textured surface renders identically twice | | | `Faces.facesRenderIdenticallyTwice` | DONE |
| VP-T2 (§7) | Test: HC no gradients/sheen/textures | | | `Theme.highContrastIsFlat`, `Faces.highContrastFacesAreFlat` | DONE |
| VP-T3 (§7) | Test: every accent option meets 4.5:1 on every palette |  |  | `Accent.everyChoiceMeetsContrastOnEveryPalette` | DONE |
| VP-T4 (§7) | Test: Standby and bypass change pilot and LEDs | | | `Faces.thePilotFollowsStandbyAndTheLedFollowsBypass` | DONE |
| VP-T5 (§7) | Test: knob/toggle/slider render in all three palettes (PNG review) | | | `Theme.controlsRenderInEveryPaletteAndRepeatExactly`; (visual) `Screenshots.everyPanelInEveryPalette` | DONE |
| VP-T6 (§7) | Test: opening Advanced with every face visible stays in the UI frame budget - none |  |  | - | MISSING |

<!-- counts DONE=27 NO-GUI=0 NO-TEST=2 PARTIAL=0 MISSING=1 DEFERRED=0 -->
