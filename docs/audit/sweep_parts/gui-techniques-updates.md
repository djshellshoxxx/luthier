## gui-techniques-updates.md

Nothing of this delta is on this checkout: there is no TECHNIQUES tab (`AdvancedPanel` tab list ends CONTROLLERS | HELP, with a comment that TECHNIQUES is not built), no Easy pill row, no technique overlays, Mute Row, "Uses Techniques" chip or tour stop, and none of the ~40 scrape/slap parameters has an attached control. The techniques branch has the work: `UI/Techniques/*` (TechniquesPanel rail, seven pages, TechniquePillRow with hold popover and right-click, TechniqueOverlay's five layers, TechniqueMirrors, CascadeView), RHYTHM Mute Row, preset-browser chip in `Overlays.cpp`, onboarding anchors, and 14 `TechniquesUi.*` tests (spot-checked the tab order in `AdvancedPanel::buildWorkspace`, the chip and `PresetInfo::armedTechniques`). The tour stop itself is on tune-help (`Onboarding::getTourSteps(includeTechniques)`). Owner gaps: the CHARACTER mirrors are not inside Right Hand / PLAYING groups; the popover/right-click opens from the Easy pill only; no explicit "sub-tabs at 1280x800" size assertion found beyond the render test.

| Req | Summary | Engine location | GUI location | Test | Status |
|---|---|---|---|---|---|
| GT-1 (§0.1, §3, §12) | Additive only; header unchanged; existing gui-integration tests still pass | n/a | (branch) HeaderBar untouched | (branch) full suite incl. `Editor*`, `GuiReach.*` | OWNED |
| GT-2 (§0.2) | TECHNIQUES tab in Col 4 between CONTROLLERS and HELP | n/a | (branch) `AdvancedPanel::buildWorkspace` `{"TECHNIQUES", techniquesPanel}` | (branch) `TechniquesUi.theTabAndItsRail` | OWNED |
| GT-3 (§0.4, §1, §10) | Vertical sub-tab rail SCRAPE/SLIDE/SLAP/MUTE/TAP/BEND/CASCADE with vertical text + divider | n/a | (branch) `UI/Techniques/TechniquesPanel`, `RailButton` | (branch) `TechniquesUi.theTabAndItsRail` | OWNED |
| GT-4 (§1) | SCRAPE sub-tab: string-scraping.md 2 controls (scrape_* now unattached) | `DSP/Noise/ScrapeEngine` (params exist) | (branch) `TechniquePages:ScrapePage` | (branch) `TechniquesUi.everySubTabRendersItsControls` | OWNED |
| GT-5 (§1) | SLIDE sub-tab: slide-technique-controls.md 1 (Slide Mode toggle stays in header) | (branch) `SlideEngine` controls | (branch) `SlidePage` | (branch) `TechniquesUi.everySubTabRendersItsControls` | OWNED |
| GT-6 (§1) | SLAP sub-tab: string-slap-technique.md 1 (slap_*/pop_*/ghost_*/double_thump_*) | `DSP/Slap/SlapEngine` | (branch) `SlapPage` | (branch) `TechniquesUi.everySubTabRendersItsControls` | OWNED |
| GT-7 (§1) | MUTE sub-tab: 16-step grid + muting-rhythm 3 controls | (branch) `MuteEngine` | (branch) `UI/MuteGroup`, `MutePage` | (branch) `TechniquesUi.everySubTabRendersItsControls` | OWNED |
| GT-8 (§1) | TAP sub-tab | (branch) `TapEngine` | (branch) `TapPage` | (branch) `TechniquesUi.everySubTabRendersItsControls` | OWNED |
| GT-9 (§1) | BEND sub-tab | (branch) `BendEngine` | (branch) `BendPage` | (branch) `TechniquesUi.everySubTabRendersItsControls` | OWNED |
| GT-10 (§1, §10) | CASCADE: strings-across / techniques-down grid of live arm state + conflicts | (branch) `CascadeResolver` masks | (branch) `CascadeView` | (branch) `TechniquesUi.theCascadeViewFollowsTheEngine` | OWNED |
| GT-11 (§1) | Arm pill at top of each sub-tab; AttachedKnob pattern | (branch) `*_armed` params | (branch) `TechniquePages` arm pill | (branch) `TechniquesUi.everySubTabRendersItsControls` | OWNED |
| GT-12 (§0.3, §2) | Easy Playing strip pill row [SCRAPE][SLIDE][SLAP][MUTE][TAP][BEND]; single tap toggles arm | n/a | (branch) `TechniquePillRow` in `EasyPanel` | (branch) `TechniquesUi.thePillsArmOnAClick` | OWNED |
| GT-13 (§2) | Hold opens non-modal popover (3-5 controls), Escape closes | n/a | (branch) `TechniquePopover` | (branch) `TechniquesUi.holdOpensThePopoverAndEscapeClosesIt` | OWNED |
| GT-14 (§2) | Right-click / long-press opens full sub-tab in Advanced | n/a | (branch) editor wiring | (branch) `TechniquesUi.rightClickOpensTheSubTabInAdvanced` | OWNED |
| GT-15 (§2, §10) | Pills rounded (theme radius), filled armed / outlined not; pulsing fire dot | (branch) fire counters | (branch) `TechniquePill` | (branch) `TechniquesUi.thePillsArmOnAClick`, `TechniquesUi.reducedMotionStopsThePulse` | OWNED |
| GT-16 (§4) | Fretboard overlays: scrape trail, tap squares (100 ms fade), mute-zone band, bend arc + cents badge, slap flash; each toggleable; layer 33+; < 2 ms | n/a | (branch) `TechniqueOverlay` layers | (branch) `TechniquesUi.theOverlaysDrawInsideTheirBudget` | OWNED |
| GT-17 (§5) | CHARACTER Right Hand > Tapping; PLAYING > Microtonal (mirrors) — placed as one block at CHARACTER foot | n/a | (branch) `TechniqueMirrors` in `CharacterPanel` | (branch) `TechniquesUi.theCharacterMirrorsAttachTheSameParameters` | OWNED |
| GT-18 (§6) | RHYTHM pattern editor Mute Row, all-open default, paintable | (branch) `RhythmPattern::setMuteStep` | (branch) `RhythmPanel::muteRow` | (branch) `Muting.theMuteControlsDriveTheModel` | OWNED |
| GT-19 (§7) | Preset browser "Uses Techniques" multi-select chip | (branch) `PresetInfo::armedTechniques` | (branch) `Overlays.cpp` preset browser `techniqueChip/techniqueMenu` | (branch) `TechniquesUi.thePresetChipFilters` | OWNED |
| GT-20 (§8) | Optional skippable onboarding stop at Techniques, pointing at cascade | n/a | (branch) anchors `onboarding.techniques(.cascade)`; (tune-help) `Onboarding::getTourSteps(includeTechniques)` | (branch) `TechniquesUi.theOnboardingAnchorsAreThere` | OWNED |
| GT-21 (§9) | Pill labels "X technique, armed/not armed"; Tab/Space/Enter; non-colour armed icon | n/a | (branch) `TechniquePill` accessibility handler, check glyph | (branch) `TechniquesUi.thePillsArmOnAClick` | OWNED |
| GT-22 (§9) | Reduced motion: overlay animations become instant | n/a | (branch) `TechniqueOverlay` fades | (branch) `TechniquesUi.reducedMotionStopsThePulse` | OWNED |
| GT-23 (§11) | Feature-to-location index rows added to gui-integration §19 | n/a | (branch) HELP topic `techniques` | (branch) `HelpTab.theWorkspaceTopicNamesEveryTabThatExists` | OWNED |
| GT-T1 (§12) | Test: every sub-tab renders correct controls at 1280x800 | | | (branch) `TechniquesUi.everySubTabRendersItsControls` | OWNED |
| GT-T2 (§12) | Test: tour reaches Techniques without breaking flow | | | (tune-help) `OnboardingTests` | OWNED |

<!-- counts DONE=0 NO-GUI=0 NO-TEST=0 PARTIAL=0 MISSING=0 OWNED=25 -->
