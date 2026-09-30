## gui-techniques-updates.md

Most of this delta is not built: no TECHNIQUES tab (`AdvancedPanel` tab list ends CONTROLLERS | HELP; comment at AdvancedPanel.cpp:1053), no Easy pill row, technique overlays, Mute Row or "Uses Techniques" chip, and the scrape params have no attached control. Present: slap controls in the bass-only CHARACTER `SlapGroup`, `SlideGroup`, HELP topics for techniques, and a `getTourSteps(includeTechniques)` tour stop with nothing to point at. Re-verified.

| Req | Summary | Engine location | GUI location | Test | Status |
|---|---|---|---|---|---|
| GT-1 (§0.1, §3, §12) | Additive only; header unchanged; existing gui-integration tests still pass | n/a | header unchanged; nothing added | existing `Editor*`, `GuiReach.*` suites | PARTIAL |
| GT-2 (§0.2) | TECHNIQUES tab in Col 4 between CONTROLLERS and HELP | n/a | - (no TECHNIQUES tab; `UI/AdvancedPanel.cpp:1053` comment says not built) | - | MISSING |
| GT-3 (§0.4, §1, §10) | Vertical sub-tab rail SCRAPE/SLIDE/SLAP/MUTE/TAP/BEND/CASCADE with vertical text + divider | n/a | - (no TECHNIQUES tab; `UI/AdvancedPanel.cpp:1053` comment says not built) | - | MISSING |
| GT-4 (§1) | SCRAPE sub-tab: string-scraping.md 2 controls (scrape_* unattached) | `DSP/Noise/ScrapeEngine` (scrape_* params) | - (no scrape controls attached in `UI/`; only `NoiseGroups` pick scrape) | `Scrape.*` engine tests | NO-GUI |
| GT-5 (§1) | SLIDE sub-tab: slide-technique-controls.md 1 (Slide Mode toggle stays in header) | `DSP/Slide/SlideEngine` | `UI/SlideGroup` in CHARACTER (`CharacterPanel.cpp:348`); no SLIDE sub-tab | `SlideTests.cpp` | PARTIAL |
| GT-6 (§1) | SLAP sub-tab: string-slap-technique.md 1 (slap_*/pop_*/ghost_*/double_thump_*) | `DSP/Slap/SlapEngine` | `UI/SlapGroup` in CHARACTER (bass only, `CharacterPanel.cpp:353`); no SLAP sub-tab | `BassTechniques.theSlapGroupIsShownOnlyOnABass` | PARTIAL |
| GT-7 (§1) | MUTE sub-tab: 16-step grid + muting-rhythm 3 controls | - (no MuteEngine) | - (`WIP/UI/MuteGroup.*` not compiled) | - | MISSING |
| GT-8 (§1) | TAP sub-tab | - (no TapEngine) | - | - | MISSING |
| GT-9 (§1) | BEND sub-tab | - (no BendEngine) | - | - | MISSING |
| GT-10 (§1, §10) | CASCADE: strings-across / techniques-down grid of live arm state + conflicts | - (no CascadeResolver) | - | - | MISSING |
| GT-11 (§1) | Arm pill at top of each sub-tab; AttachedKnob pattern | `scrape_armed`, `slap_armed` params only | - | - | MISSING |
| GT-12 (§0.3, §2) | Easy Playing strip pill row [SCRAPE][SLIDE][SLAP][MUTE][TAP][BEND]; single tap toggles arm | n/a | - (`UI/EasyPanel.h` has no technique pill row) | - | MISSING |
| GT-13 (§2) | Hold opens non-modal popover (3-5 controls), Escape closes | n/a | - | - | MISSING |
| GT-14 (§2) | Right-click / long-press opens full sub-tab in Advanced | n/a | - | - | MISSING |
| GT-15 (§2, §10) | Pills rounded (theme radius), filled armed / outlined not; pulsing fire dot | - (no fire counters) | - | - | MISSING |
| GT-16 (§4) | Fretboard overlays: scrape trail, tap squares (100 ms fade), mute-zone band, bend arc + cents badge, slap flash; each toggleable; layer 33+; < 2 ms | n/a | - | - | MISSING |
| GT-17 (§5) | CHARACTER Right Hand > Tapping; PLAYING > Microtonal (mirrors) | scrape/slap params only | - (no `TechniqueMirrors` in `CharacterPanel`) | - | MISSING |
| GT-18 (§6) | RHYTHM pattern editor Mute Row, all-open default, paintable | - (no mute step in `RhythmPattern`) | - (no muteRow in `UI/RhythmPanel`) | - | MISSING |
| GT-19 (§7) | Preset browser "Uses Techniques" multi-select chip | - (no `PresetInfo::armedTechniques`) | - (no technique chip in preset browser) | - | MISSING |
| GT-20 (§8) | Optional skippable onboarding stop at Techniques, pointing at cascade | n/a | `UI/Onboarding.cpp:208-231` `getTourSteps(includeTechniques)` adds a Techniques stop; `PluginEditorOnboarding.cpp:168` targets the TECHNIQUES tab, which is not built (empty rect); no cascade stop | `Onboarding.theTourHasTwelveStopsInTheSpecsOrder` (checks the stop list only) | PARTIAL |
| GT-21 (§9) | Pill labels "X technique, armed/not armed"; Tab/Space/Enter; non-colour armed icon | n/a | - | - | MISSING |
| GT-22 (§9) | Reduced motion: overlay animations become instant | n/a | - | - | MISSING |
| GT-23 (§11) | Feature-to-location index rows added to gui-integration §19 | n/a | HELP topics `techniques` and `technique-*` in `UI/HelpContent.cpp:338-571` (describe unbuilt UI); gui-integration section 19 rows not added | `HelpTab.theWorkspaceTopicNamesEveryTabThatExists` | PARTIAL |
| GT-T1 (§12) | Test: every sub-tab renders correct controls at 1280x800 | n/a | - | - | MISSING |
| GT-T2 (§12) | Test: tour reaches Techniques without breaking flow | n/a | tour stop scaffolding only (see GT-20) | `Onboarding.theTourHasTwelveStopsInTheSpecsOrder` | PARTIAL |

<!-- counts DONE=0 NO-GUI=1 NO-TEST=0 PARTIAL=6 MISSING=18 DEFERRED=0 -->
