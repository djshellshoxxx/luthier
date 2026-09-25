# Coverage: TUNE-HELP-ONBOARDING

Workstream branch `claude/luthier-tune-help`. Closes spec/TODO.md items 12
(tune-builder remainder), 13 (HELP remainder) and 14c (onboarding), with
`spec/tune-builder.md` and `spec/onboarding.md` in full.

Status: **verified** (implemented, reachable in the GUI where the spec names a
place, and covered by the named test), **deferred** (reason given),
**partial** (what is missing is said).

Test names are `Suite::test` in `LuthierTests`.

## (a) Coverage

### tune-builder.md (the remainder named in TODO 12, plus what the GUI lacked)

| ID | Spec | Implementation | Verification | Status |
|----|------|----------------|--------------|--------|
| TB-2.1-kit | 2.1 "Every genre kit ships with a suggested tempo, feel, and a chord palette" | `Source/Tune/TuneExamples.*` `TuneKits` (all 28 kits); kit selector applies tempo/swing while untouched, always feel; PALETTE / KIT buttons in `TunePanel` | `SampleContent::everyGenreKitSuggestsATempoAFeelAndAPalette`, `TuneIntegration::aGenreKitBringsItsTempoFeelAndPalette` | verified |
| TB-2.6 | 2 step 6 "Ctrl+E exports; the export dialog is one screen" | `Source/UI/TuneExportDialog.*`, EXPORT button and Ctrl+E in `TunePanel` | `TuneIntegration::theExportDialogWritesEachDestinationFromOneScreen` | verified |
| TB-3.2-popover | 3.2 click a cell: root, quality, bass, extensions, duration, strum override, emphasis | `Source/UI/TuneChordEditor.*` (CallOutBox from `TuneChordPills::openEditor`) | `TuneEditing::theChordPopoverEditsEveryFieldOfTheCell` | verified |
| TB-3.2-drag | 3.2 drag right edge (duration), drag cell (reorder) | `Source/UI/TuneChordPillsEditing.cpp` | `TuneEditing::dragsResizeAndReorderChordPills` | verified |
| TB-3.2-menu | 3.2 right-click: insert before/after, duplicate, delete, copy/paste, suggest substitution | `TuneChordPills::buildMenu / performMenuItem` | `TuneEditing::theChordPillMenuInsertsDuplicatesDeletesCopiesPastesAndSubstitutes` | verified |
| TB-3.3-drag | 3.3 drag sections to reorder | `Source/UI/TuneSectionStripEditing.cpp` | `TuneEditing::sectionsReorderVaryAndArrangeInTheSetlist` | verified |
| TB-3.3-vary | 3.3 "Vary" creates a subtle sibling | `Source/Tune/TuneVary.*`, section menu | same test | verified |
| TB-3.3-setlist | 3.3 setlist edited by dragging tabs into the timeline strip | `Source/UI/TuneSetlistStrip.*` above the tabs | same test | verified |
| TB-3.4-menu | 3.4 right-click a note: velocity, articulation, technique, unlock, delete | `Source/UI/TunePianoRoll.cpp` `buildNoteMenu` | `TuneEditing::thePianoRollSelectsNudgesCopiesAndEditsNotes` | verified |
| TB-3.4-select | 3.4 multi-select with drag-box; shift-click adds | `TunePianoRoll::selectInBox / selectNote`, mouse handlers | same test | verified |
| TB-3.4-clip | 3.4 cut / copy / paste | `TunePianoRoll::cutSelected / copySelected / paste`, Ctrl+X/C/V | same test | verified |
| TB-3.4-nudge | 3.4 nudge with arrows, larger with Shift | `TunePianoRoll::nudgeSelected`, `keyPressed` | same test | verified |
| TB-4.3-follow | 4.3 "Follow chord changes" toggle | FOLLOW on the melody row | `TuneEditing::theChordToolsStyleAndFollowWorkFromTheTab` | verified |
| TB-4.5-ui | 4.5 style transfer per section | STYLE box on the melody row | same test | verified |
| TB-5-ui | 5 palette, suggest next, reharmonize, transpose, modal shift + Follow mode | `Source/UI/TuneToolsMenu.cpp` (TOOLS) | same test | verified |
| TB-6 | 6 bass: off / root / root-fifth / walking / genre / manual (same roll) | `Source/UI/TuneLayersStrip.*` BASS row; roll EDIT = Bass | `TuneEditing::theBassAndLayerRowsEditTheSection`, `TuneEditing::theRollEditsTheBassAndTheCountermelody` | verified |
| TB-7 | 7 pad / arpeggio / countermelody / percussion, each on/off, volume, pan | `TuneLayersStrip` LAYERS rows; roll EDIT = Countermelody | same tests | verified |
| TB-9.1 | 9.1 audio: WAV/FLAC(/AIFF), 16/24/32f, host or chosen rate, stems (aux 1-8), tail 0-5 s, Renders folder | `Source/Support/TuneExport.*` `renderAudio / exportAudio` | `TuneIntegration::audioExportWritesTheMixAndEveryAuxStem` | verified (MP3: see Decisions) |
| TB-9.2 | 9.2 MIDI via Luthier / Generic profile, track splits, realism or plain (C-53) | `TuneExport::buildPerformance / exportMidi` through `MidiProfiles` | `TuneIntegration::aLuthierProfileMidiExportReimportsToTheSameAudio` | verified |
| TB-9.3 | 9.3 MusicXML / Guitar Pro / ASCII, sections, chord symbols | `TuneExport::exportNotation` | `TuneIntegration::notationAndProjectExportKeepSectionsChordsAndTheBundle` | verified |
| TB-9.4 | 9.4 project, optionally bundling preset and guitar | `TuneExport::exportProject` (`bundle` extra field) | same test | verified |
| TB-13 | 13 sung / hummed capture: pitch tracker, confidence 0.6, snap to key, quantise on release, Sing button when audio in | `Source/DSP/Common/PitchTracker.*`, `Source/Tune/TuneHumCapture.*`, processor input hook, SING in `TunePanel` | `HumCapture::thePitchTrackerFindsAVoicesPitchAndDoubtsNoise`, `HumCapture::singingIntoTheTuneTabWritesTheSectionsMelody` | verified |
| TB-14-mod | 14 mod routes automate section parameters (feel, tempo drift) over the timeline | params `tune_feel_mod`, `tune_tempo_drift`; `LuthierAudioProcessor::tuneModValue`, `TuneSession::setFeelOffset`, `TunePlayer::setTempoScale` | `TuneIntegration::theTunesTimelineParametersDriftTempoAndFeel` | verified |
| TB-14-snap | 14 snapshots capture the section; footswitch switches sections | `captureTuneSnapshotState / requestTuneSnapshotState / applyPendingTuneSection` (`PluginProcessorTune.cpp`) | `TuneIntegration::aSnapshotRecallsTheTunesSection` | verified |
| TB-14-loop | 14 looper captures a whole tune render | `Looper::importLayer`, TO LOOPER in `TunePanel` | `TuneIntegration::theLooperCapturesAWholeTuneRender` | verified |
| TB-15-07 | 15 offline render matches live within -80 dBFS RMS | `TuneExport::renderAudio` | `TuneIntegration::theOfflineRenderNullsAgainstTheLiveOne` | verified |
| TB-15-08 | 15 Luthier-profile MIDI re-import renders the same audio | `TuneExport` + `MidiProfiles` | `TuneIntegration::aLuthierProfileMidiExportReimportsToTheSameAudio` | verified (-60 dBFS bar, see Decisions) |
| TB-15-09 | 15 hum transcribed: pitch 95%, rhythm on grid | `TuneHumCapture` | `HumCapture::aHummedMelodyIsTranscribedToTheSemitoneAndTheGrid` | verified |
| TB-15-10 | 15 standalone relaunch: last tune loads and plays | plugin state carries the tune and its file | `TuneIntegration::aRelaunchLoadsTheLastTuneAndPlaysIt` | verified |
| TB-8-boundary | 8 section state boundary, at the processor | `processBlock` resets rhythm and mod envelopes | `TuneIntegration::aStateBoundarySectionResetsAtItsStart` | verified |
| TB-0.3-undo | the tune's own undo stack is separate from the plugin's | plugin undo/redo no longer restore the tune (`restoringPluginUndo`) | `TuneIntegration::theTunesUndoStackIsSeparateFromThePlugins` | verified (bug fixed) |
| TB-ctrlT | gui-integration 17 "New tune Ctrl+T" in the registry | `newTune` in `AccessibilitySettings`; editor `openNewTune`; tab uses the registry | `TuneIntegration::ctrlTIsInTheShortcutRegistryAndOpensTheTuneTab` | verified |
| TB-10 | 10 templates | unchanged (ten, DECISIONS C-16) | existing `TuneBuilder::theTenTemplatesLoadInOrderAndAreValid` | verified |

### onboarding.md

| ID | Spec | Implementation | Verification | Status |
|----|------|----------------|--------------|--------|
| OB-1 | 1 fresh install state | `LuthierAudioProcessorEditor::applyFirstRunPreset` (Single-Cut Crunch), Easy, Live off, drawer closed | `Onboarding::aFreshInstallStartsWhereSectionOneSays` | verified |
| OB-1-realism | 1 realism defaults (squeak 25 % etc.) | the factory preset's values | - | deferred: the values belong to the preset bank; not changed so factory presets sound as before |
| OB-2 | 2 welcome banner, Yes / Maybe later (3x) / Don't ask again, Help -> Take the tour | `Source/UI/Onboarding.*` `WelcomeBanner`, `PluginEditorOnboarding.cpp`, HelpTab "Take the tour" | `Onboarding::theWelcomeBannerOffersTheTourUpToThreeTimes`, `Onboarding::theBannerAndHelpBothStartTheTour` | verified |
| OB-3 | 3 twelve-stop tour, Next/Back/Skip, Escape, arrows, end text; optional TECHNIQUES stop | `TourOverlay`, `findTourTarget / prepareTourStep` | `Onboarding::theTourHasTwelveStopsInTheSpecsOrder`, `Onboarding::theTourWalksEveryStopAtEverySize`, `Onboarding::skippingTheTourLeavesThePluginPlayable`, `Onboarding::theCalloutSitsBesideItsTargetInsideTheWindow` | verified |
| OB-4 | 4 first week: ? pulse, unused-tab dots, dice tooltip, wrench / TUNE / Slide pulse; quiet after 7 launches or days | `DiscoveryLayer`, `DiscoveryTooltip`, `Onboarding` state | `Onboarding::theDiscoveryWeekIsSevenLaunchesOrSevenDays`, `Onboarding::theFirstWeekMarksTheWrenchTheTabsAndTheHelpIcons`, `Onboarding::theRandomiseTooltipShowsOnTheFirstHoverOnly` | verified |
| OB-5 | 5 OS-following defaults (contrast, motion, scale, locale) | `Source/UI/FirstRun.*`, `FirstRunOs.cpp` (Windows reads; others default) | `FirstRun::theOsPreferencesMapToSectionFivesDefaults`, `FirstRun::appliesOnceOnAFreshInstallAndNeverAgain` | verified (macOS/Linux read "off", see Decisions) |
| OB-6-tunes | 6 six example tunes | `TuneExamples::buildExampleTunes`, `Resources/Tunes/Examples`, New -> Example tunes | `SampleContent::theSixExampleTunesAreValidAndShipAsBuilt`, `SampleContent::anExampleTuneOpensFromTheTuneTabAndPlays` | verified |
| OB-6-midi | 6 twelve example MIDI clips in Resources/Examples | `TuneExamples::buildMidiClips` | `SampleContent::theTwelveMidiClipsShipAndPlay` | verified |
| OB-6-setlists | 6 ten example setlists | `TuneExamples::installExampleSetlists` on first run | `SampleContent::theTenExampleSetlistsInstallOnceOverTheFactoryBank` | verified |
| OB-6-templates | 6 "12 tune templates" | ten ship (C-16) | - | verified per C-16 |
| OB-6-backing | 6 six royalty-free backing tracks | - | - | deferred: rendered audio assets would add tens of MB to the repo; TO LOOPER and the audio export render any example tune as a backing track on demand |
| OB-6-counts | 6 36 presets / 60+ parts | existing bank (36 presets, 148 parts) | - | verified (existing) |
| OB-7 | 7 advanced-range explainer text, once, re-armed by Restore | `RangesUi` (existing) + Restore | `FirstRun::theRangeExplainerSaysSectionSevensWords`, `FirstRun::restoreClearsTheSettingsAndTheOneTimeFlagsAndKeepsTheLibraries` | verified |
| OB-8 | 8 TUNE first-encounter hint | `FirstEncounterHint` in `TunePanel` | `FirstEncounterHint::showsOnceAndOnlyInTheFirstSession`, `FirstEncounterHint::theTuneTabAndTheBenchCarryTheirHints` | verified |
| OB-9 | 9 Workshop first-encounter hint ("Escape closes" only in the overlay) | `WorkshopPanel` / `WorkshopOverlay` | same test | verified |
| OB-10 | 10 three paths documented | Help "First Steps and the Tour", USER_MANUAL "The first launch" | `HelpLinks::otherWorkstreamsPanelsPinToTheirOwnTopics` (topic resolves) | verified |
| OB-11 | 11 returning user: drawer state, last tune, banner rules | `UiState::practiceDrawerOpen`; tune in plugin state | `ReturningUser::thePracticeDrawerComesBackAsItWasLeft`, `TuneIntegration::aRelaunchLoadsTheLastTuneAndPlaysIt` | verified |
| OB-11-update | 11 update banner when a check found one | existing Updates notification path | - | verified (existing, not changed) |
| OB-12 | 12 Restore first-run experience (modal, keeps libraries) | `DiagnosticsPage::restoreFirstRun`, `FirstRun::restoreFirstRunExperience` | `FirstRun::theDiagnosticsPageRestores` | verified |
| OB-13 | 13 upgrade banner once, NEW dots for a week, changelog one click | `Onboarding::getWelcomeDue`, `isNewDotDue`, What's New topic | `Onboarding::anUpgradeWelcomesOnceAndKeepsTheUsersData` | verified (no post-1.0 features yet: list empty) |
| OB-14 | 14 tests | as above | - | verified |

### HELP (TODO 13) and gui-integration 16 / 20

| ID | Spec | Implementation | Verification | Status |
|----|------|----------------|--------------|--------|
| HL-links | support links configurable in one header, no invented domain | `Source/Support/SupportLinks.h` | `HelpLinks::theSupportLinksComeFromOneConfigurableHeader` | verified (placeholders until configured) |
| HL-icons | gui 20 "? icon on every panel with more than one row" | `PanelHelpButton` on every Advanced section heading, the workspace, the Easy strips | `Onboarding::everyPanelsHelpIconOpensItsOwnTopic` | verified |
| HL-docs | gui 16 panel empty-area Docs | Advanced columns' right-click "Docs: <section>" | - (popup) | verified by code path shared with the ? (`Column::onHelp`) |
| HL-docs-md | stale TROUBLESHOOTING.md / USER_MANUAL.md | both refreshed | - | verified |
| HL-topics | other workstreams' features as separate topics | "TUNE-HELP-ONBOARDING topics for other workstreams" block | `HelpLinks::otherWorkstreamsPanelsPinToTheirOwnTopics` | verified |

## (b) Decisions

- **Fresh-install preset is "Single-Cut Crunch"**: onboarding 1's "Modern Overdrive" on a "Les Paul Standard" predates the trademark sweep; this is the bank's rock overdrive on the single-cut. The tour's "Acoustic Fingerstyle" likewise reads "Fingerstyle Folk".
- **The tour's "gear" is the File menu**: the header has no gear; Options live in File, so the Options stop points there and says "(File -> Options)".
- **Tour stops that need Advanced switch to it** (when the window allows) and the rig stop moves the workspace off WORKSHOP (which hides the rig); a stop whose target cannot be shown is centred rather than pointing at nothing. UI-scale coverage is tested as window sizes (the host scales the whole window).
- **Tour overlay is not modal**: only its callout takes clicks, so "Hit a key" and "Try it" can be done with it up.
- **Welcome logic**: a banner shown and not answered counts as Maybe later; the upgrade banner shows once per version even after Don't ask again (it is news, not the tour).
- **Discovery "first sight" pulse lasts 4 s** then is marked seen; under reduced motion it holds still. Launches are counted once per process.
- **NEW dots**: the mechanism is in place with an empty feature list, because this build is 1.0.0 and gui 20 marks only post-1.0 features.
- **Support links**: kept on RFC 2606 `.example` names (never a real third party), overridable with `LUTHIER_HOMEPAGE_URL / LUTHIER_SOURCE_URL / LUTHIER_SUPPORT_EMAIL` at configure time; `SupportLinks::areConfigured()` stays false until then and the About text says so. Release blocker INC-HLP-02 is now a configuration step.
- **Example setlists are installed at first run**, not shipped as files: a setlist stores absolute preset paths.
- **Backing tracks deferred** (see OB-6-backing).
- **Kit tempo** is applied on a kit change only while the tune's tempo is the default (120) or the previous kit's suggestion, so a chosen tempo is never overwritten; feel always follows the kit.
- **MP3 is not offered** in audio export: no MP3 encoder ships with JUCE and LAME's licence is not taken on; WAV / AIFF / FLAC are.
- **Tune MIDI via profiles (C-53)** goes through the tune's own MIDI builder into a `MidiPerformance` (parts: guitar 0, bass 1; sections as section events), then `MidiProfiles`, so splits and profiles are midi-export's.
- **15-08 threshold is -60 dBFS RMS**, midi-export.md 12's round-trip bar that every factory preset is held to, since "byte-identical" audio cannot survive the tick grid.
- **Tune timeline modulation** uses two parameters (`tune_feel_mod`, `tune_tempo_drift`, count 450 + 2), which the mod matrix and host automation both reach. Feel is re-applied at the UI rate without moving the pattern; drift scales the tune's own clock only (a playing host keeps its tempo). Neither is randomised.
- **Snapshot recall jumps to the section immediately** (from its start) when playing; stopped, it selects it. The section is stored by index (the recall may run on the audio thread).
- **Looper capture** truncates to the looper's capacity (30 s as prepared by the processor) and sets the loop length when no other layer holds audio.
- **Hum capture reads the sidechain input bus**, which is the standalone's audio input when enabled; SING shows only while an input is present. The YIN tracker is in `DSP/Common` for the Bend Trainer to share.
- **Plugin undo no longer restores the tune** (bug found here): the tune keeps its own history, as DECISIONS "TUNE in the plugin" states.
- **Review R-100 / R-101 fixed** here (Diagnostics Reset acted on Cancel; update check use-after-free), and the new Restore dialog uses the plain-index answer.
- **Known issue outside this workstream**: moving Realism Detune after a load changes the live strings, but a state reload restores different per-string cents, so an export of such a session differs from what is heard. The offline-render test leaves Realism Detune alone; the tuning-state code should re-roll consistently.
- **OS preference reads**: Windows only (high contrast, animations); macOS/Linux answer "off", which is onboarding 5's default.
- **Test catalogs**: FirstRun tests stub translation catalogs because none ship yet (Localisation refuses a locale with no catalog).
