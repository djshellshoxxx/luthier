## onboarding.md

TUNE-HELP has landed on this checkout: the curated first-run preset and state, the welcome banner, the 12-stop tour, first-week discovery marks, OS-following first-launch defaults, six example tunes, twelve MIDI clips, ten setlists, the TUNE and Workshop first-encounter hints, Restore first-run and the upgrade banner, with the `Onboarding`, `FirstRun`, `FirstEncounterHint`, `SampleContent` and `ReturningUser` suites registered. Still open: realism defaults in the first-run preset (OB-5), macOS/Linux OS reads (OB-10), backing tracks (OB-17), videos (OB-23), last-preset restore on a clean close (OB-24), a NEW-dots test (OB-30), the dated Backup move on migration (OB-31) and a tour test at UI scales (OB-T2).

| Req | Summary | Engine location | GUI location | Test | Status |
|---|---|---|---|---|---|
| OB-1 (§0.1-0.2) | Great sound in under 30 s with no modal on first run | `LuthierAudioProcessorEditor::applyFirstRunPreset` | n/a | `Onboarding.aFreshInstallStartsWhereSectionOneSays` | DONE |
| OB-2 (§0.5, §5) | First run does no network; update check, telemetry, crash, beta all off | `Updates/Telemetry.cpp` settings defaults | Options -> UPDATES/PRIVACY | `Telemetry::everythingIsOffByDefault`, `Telemetry::nothingIsSentWhileSwitchedOff` | DONE |
| OB-3 (§1) | Fresh preset + guitar ("Modern Overdrive" / single-cut: owner uses "Single-Cut Crunch") | `applyFirstRunPreset` | n/a | `Onboarding.aFreshInstallStartsWhereSectionOneSays` | DONE |
| OB-4 (§1, §5) | Easy mode, Live off, drawer collapsed, Workshop closed, Slide off, stock ranges, sidechain/recorder off | `UiState` defaults, param defaults | n/a | `Onboarding.aFreshInstallStartsWhereSectionOneSays` | DONE |
| OB-5 (§1) | Realism defaults (squeak 25 %, player-friendly setup style) — squeak param default is 0.25; the first-run preset sets no player-friendly setup style; no test | `squeak_amount` default 0.25 | n/a | - | PARTIAL |
| OB-6 (§2) | Welcome banner: Yes / Maybe later (3x) / Don't ask again, per version | `UI/Onboarding.*:WelcomeBanner`, `PluginEditorOnboarding.cpp` | banner under header | `Onboarding.theWelcomeBannerOffersTheTourUpToThreeTimes` | DONE |
| OB-7 (§2) | Help -> Take the tour | `HelpTab` "Take the tour" | HELP tab | `Onboarding.theBannerAndHelpBothStartTheTour` | DONE |
| OB-8 (§3) | 12-stop tour with Next/Back/Skip, Escape, end text | `TourOverlay`, `findTourTarget` | overlay callouts | `Onboarding.theTourHasTwelveStopsInTheSpecsOrder`, `Onboarding.theCalloutSitsBesideItsTargetInsideTheWindow` | DONE |
| OB-9 (§4) | First week: ? pulse, unused-tab dots, dice tooltip, wrench/TUNE/Slide pulses; quiet after 7 launches or days | `DiscoveryLayer`, `DiscoveryTooltip` | header, tabs | `Onboarding.theDiscoveryWeekIsSevenLaunchesOrSevenDays`, `Onboarding.theFirstWeekMarksTheWrenchTheTabsAndTheHelpIcons`, `Onboarding.theRandomiseTooltipShowsOnTheFirstHoverOnly` | DONE |
| OB-10 (§5) | Reduced motion, high contrast, DPI > 150 % -> 125 %, locale follow the OS on first launch — `FirstRunOs.cpp` reads Windows only; macOS/Linux answer "off" | `UI/FirstRun.*`, `UI/FirstRunOs.cpp` | n/a | `FirstRun.theOsPreferencesMapToSectionFivesDefaults`, `FirstRun.appliesOnceOnAFreshInstallAndNeverAgain` | PARTIAL |
| OB-11 (§6) | Factory presets across Electric, Acoustic, Classical, Bass, Utility (36; 44 ship) | `Presets/FactoryPresets` | preset browser | `Presets::everyFactoryPresetLoadsAndPlays` | DONE |
| OB-12 (§6) | Factory guitars as `.luthierguitar` files (27 ship) | `Resources/Guitars` | header guitar list / WORKSHOP | `Workshop::everyFactoryGuitarLoadsAndRoundTrips`, `Workshop::theFactoryLibraryIsThere` | DONE |
| OB-13 (§6) | 60+ factory parts (148 ship) | `Resources/Parts`, `PartLibrary` | WORKSHOP drawer | `Workshop::theFactoryLibraryIsThere` | DONE |
| OB-14 (§6) | Tune templates (12 in spec; ten per DECISIONS C-16) | `Tune/TuneTemplates.cpp` | TUNE NEW | `TuneBuilder::theTenTemplatesLoadInOrderAndAreValid` | DONE |
| OB-15 (§6) | Six example tunes | `TuneExamples::buildExampleTunes`, `Resources/Tunes/Examples` | TUNE New -> Example tunes | `SampleContent.theSixExampleTunesAreValidAndShipAsBuilt`, `SampleContent.anExampleTuneOpensFromTheTuneTabAndPlays` | DONE |
| OB-16 (§6) | Twelve example MIDI clips in `Resources/Examples` | `TuneExamples::buildMidiClips` | n/a | `SampleContent.theTwelveMidiClipsShipAndPlay` | DONE |
| OB-17 (§6) | Six royalty-free backing tracks — owner defers (TO LOOPER / audio export render tunes on demand); not recorded in DECISIONS.md | - | - | - | MISSING |
| OB-18 (§6) | Ten example setlists | `TuneExamples::installExampleSetlists` | LIVE setlist | `SampleContent.theTenExampleSetlistsInstallOnceOverTheFactoryBank` | DONE |
| OB-19 (§6) | The tour as a reusable walkthrough | `TourOverlay` | Help -> Take the tour | `Onboarding.theBannerAndHelpBothStartTheTour` | DONE |
| OB-20 (§7) | Advanced-range explainer, once, exact text | `UI/RangesUi.cpp:showExplainerIfFirstTime` | popover on first past-stock drag | `FirstRun.theRangeExplainerSaysSectionSevensWords` | DONE |
| OB-21 (§8) | TUNE first-encounter hint, once, first session | `FirstEncounterHint` in `TunePanel` | TUNE tab top | `FirstEncounterHint.showsOnceAndOnlyInTheFirstSession`, `FirstEncounterHint.theTuneTabAndTheBenchCarryTheirHints` | DONE |
| OB-22 (§9) | Workshop first-encounter hint in the bench header | `WorkshopPanel::showFirstEncounterHintIfDue` | bench header | `FirstEncounterHint.theTuneTabAndTheBenchCarryTheirHints` | DONE |
| OB-23 (§10) | Paths A/B/C documented in the manual and videos — manual and Help topic only; no videos (DEFER not recorded) | `docs/USER_MANUAL.md`, Help topic | HELP | `HelpLinks.otherWorkstreamsPanelsPinToTheirOwnTopics` | PARTIAL |
| OB-24 (§11) | Restore last preset if closed clean — no last-preset / clean-close memory; host/plugin state only | host/plugin state only | n/a | - | MISSING |
| OB-25 (§11) | Restore window size, mode, tab | `UiState::editorWidth/Height/advancedMode`, `UiPreferences` tab | n/a | `Editor::theWorkspaceTabWrapsAndIsRemembered` | DONE |
| OB-26 (§11) | Restore practice drawer state | `UiState::practiceDrawerOpen` | Practice drawer | `ReturningUser.thePracticeDrawerComesBackAsItWasLeft` | DONE |
| OB-27 (§11) | Restore Slide Mode and the last tune | `slide_mode` param; tune in plugin state | n/a | `TunePanel::theSessionRoundTripsThroughPluginState`, `TuneProcessor::thePluginStateKeepsTheTuneAndTheClickRoute` | DONE |
| OB-28 (§11) | Skip welcome unless armed; update banner when a check found one | `PluginEditor` update notification; welcome rules | header banner | `Onboarding.theWelcomeBannerOffersTheTourUpToThreeTimes` | DONE |
| OB-29 (§12) | Options -> Diagnostics "Restore first-run experience", modal, keeps libraries | `FirstRun::restoreFirstRunExperience` | `DiagnosticsPage::restoreFirstRun` | `FirstRun.theDiagnosticsPageRestores`, `FirstRun.restoreClearsTheSettingsAndTheOneTimeFlagsAndKeepsTheLibraries` | DONE |
| OB-30 (§13) | Upgrade banner once ("What's new"), NEW dots, changelog one click — NEW dots have no test (visual's `NewDots` test and `UI/NewFeatureDots` are not on this checkout) | `Onboarding::getWelcomeDue`, `Onboarding::isNewDotDue` (drawn by `DiscoveryLayer`) | banner; first-week NEW dots; Options UPDATES | `Onboarding.anUpgradeWelcomesOnceAndKeepsTheUsersData` | PARTIAL |
| OB-31 (§13) | Forward-compatible formats; migrate on load and move the old file to a dated Backup folder — the dated Backup move on migration is untested (visual's `Presets.backupsGoToThePresetsRootBackupFolder` is not on this checkout) | `GuitarMigration`, `PresetManager` backup | n/a | `GuitarMigration.aPresetNamingAnOldGuitarLoadsItsReplacement`, `Presets.savingBacksUpTheVersionItReplaces` | PARTIAL |
| OB-T1 (§14) | Fresh install produces the expected default state | - | n/a | `Onboarding.aFreshInstallStartsWhereSectionOneSays` | DONE |
| OB-T2 (§14) | Tour walks 12 steps without misalignment at every UI scale — test walks window sizes, not UI scales | - | n/a | `Onboarding.theTourWalksEveryStopAtEverySize` | PARTIAL |
| OB-T3 (§14) | Skipping the tour leaves the plugin playable | - | n/a | `Onboarding.skippingTheTourLeavesThePluginPlayable` | DONE |
| OB-T4 (§14) | OS high-contrast -> palette switch; DPI 200 -> scale snap | `FirstRun` | n/a | `FirstRun.theOsPreferencesMapToSectionFivesDefaults` | DONE |
| OB-T5 (§14) | Version upgrade: banner shows, user data intact | - | n/a | `Onboarding.anUpgradeWelcomesOnceAndKeepsTheUsersData` | DONE |
| OB-T6 (§14) | Restore first-run resets settings, keeps the user library | - | n/a | `FirstRun.restoreClearsTheSettingsAndTheOneTimeFlagsAndKeepsTheLibraries` | DONE |
| OB-T7 (§14) | First-encounter popovers fire exactly once until reset | - | n/a | `FirstEncounterHint.showsOnceAndOnlyInTheFirstSession` | DONE |

<!-- counts -->
