## include.md

The Help section is complete and tested by `HelpTab.theContentCoversWhatIncludeMdAsksFor`: workflow, GUI, version, licence, install and preset troubleshooting, and a debug button. The support links are still `luthieraudio.example` placeholders. The File menu is complete (save, open, options, reveal folder, randomise, reset, export, MIDI import and export). The build ships VST3 + Standalone (+ AU on macOS). CLAP and Linux are wired through CI, with pluginval and clap-validator. The remaining gaps:
- WAV export and its success popup have no test.
- The right-click Reset and Enter value items have no test.
- The standalone's audio/MIDI device settings are only described in Options, not reachable from it.
- Drag-and-drop covers IRs and MIDI, but not backing-track audio.
- The troubleshooting file has no licence or MIDI section.
- Neither the crash-log default nor the hard reset is tested.

| Req | Summary | Engine location | GUI location | Test | Status |
|---|---|---|---|---|---|
| INC-1 (Help) | Help explains each feature, the workflow and the GUI | `UI/HelpContent.cpp` topics | ADVANCED > HELP tab `HelpTab`; F1; header `?` | `HelpTab.theContentCoversWhatIncludeMdAsksFor`, `HelpTab.f1AndTheHeaderOpenHelpOnThePanelYouAreIn` | DONE |
| INC-2 (Help) | Version number | `HelpTab::getVersionText` | HELP > About; footer | `HelpTab.theContentCoversWhatIncludeMdAsksFor` | DONE |
| INC-3 (Help) | Licence text + live state | `HelpContent` "about"; `License` | HELP > About | `HelpTab.theContentCoversWhatIncludeMdAsksFor` | DONE |
| INC-4 (Help) | GitHub / homepage / support email links — placeholders `luthieraudio.example` in `HelpContent.h:92-94` | `HelpContent::homepageUrl/sourceUrl/supportEmail` | HELP > About buttons | `HelpTab.theContentCoversWhatIncludeMdAsksFor` | OWNED (tune-help 8fc948c: links in one configurable `SupportLinks` header; real URLs are a release item) |
| INC-5 (Help) | Troubleshooting: manual install/uninstall | `HelpContent` "troubleshooting" | HELP | `HelpTab.theContentCoversWhatIncludeMdAsksFor` | DONE |
| INC-6 (Help) | Where to put preset files (this machine's folder) | `HelpContent` "presets" | HELP | `HelpTab.theContentCoversWhatIncludeMdAsksFor` | DONE |
| INC-7 (Help) | Debug button in Help opens the debug window | `HelpTab::onOpenDebug` | HELP > Debug → Debug overlay | `HelpTab.theContentCoversWhatIncludeMdAsksFor` | DONE |
| INC-8 (Icon) | Unique app icon | `CMakeLists.txt` ICON_BIG/ICON_SMALL `Resources/icon.png`, `luthier.ico` | n/a | - | NO-TEST |
| INC-9 (Presets) | Preset bank with descriptive names | `Presets/FactoryPresets` | header preset browser | `Presets.everyFactoryPresetLoadsAndPlays` | DONE |
| INC-10 (Reset) | Reset returns all settings **and functions** to defaults — running looper/backing/TUNE/rhythm aren't stopped (see ISS-9) | `PluginProcessor::resetEverything` | Easy `EasyPanel::resetButton`; File > "Reset all settings to default"; Ctrl+Shift+R | `Presets.resetRestoresDefaults` | PARTIAL |
| INC-11 (Menus) | File dropdown: Save / Save As / Open / Options | `PresetManager` | header `HeaderBar::showFileMenu` items 1-3, 10 | `Presets.stateRoundTripsExactly`, `Editor.everyOverlayShortcutOpensItsOwnOverlayAndEscapeClosesIt` (saveAs, options) | DONE |
| INC-12 (Menus) | Options: tooltips on/off — no test that toggling it changes the TooltipWindow delay | `UiState::tooltipsEnabled`; `PluginEditor.cpp:506` | Options > Appearance `tooltipsToggle` | `StateModel.loadingAPresetLeavesTheLayersAboveItAlone` (persistence only) | NO-TEST |
| INC-13 (Menus) | Options: choose MIDI and audio card — only an info box pointing at the standalone toolbar / host | - | Options > Audio `AudioPage::deviceButton` (message only) | - | PARTIAL |
| INC-14 (Menus) | "Open location in explorer" | `File::revealToUser` | File > "Open user preset folder", "Open render folder" | `Editor.newPresetLoadsInitAndRevealSaysSoWhenThereIsNoFile` | DONE |
| INC-15 (Export) | Export audio to WAV with basic quality options — `AudioExporter` has no test | `Support/AudioExporter` | File > "Export audio..." / Easy `exportButton` → `ExportPanel` (depth, rate, length, normalise) | - | NO-TEST |
| INC-16 (Export) | Success popup: location, length, name, quality | `ExportPanel::startExport` callback | Export overlay | - | NO-TEST |
| INC-17 (MIDI) | Import and export MIDI | `Export/MidiImportTargets`, `Support/MidiCapture`, `Export/MidiPerformance` | File > "Import MIDI...", "Save last MIDI take..."; MIDI OUT tab | `MidiImport.*`, `MidiCapture.capturesAndWritesAFile`, `MidiExport.*` | DONE |
| INC-18 (Right click) | Right-click any control: MIDI map, reset to default, set a value — only Learn is tested (engine side), not Reset / Enter value | `Support/MidiLearn` | every `LuthierKnob` → `Widgets.cpp:showParameterContextMenu` | `MidiLearn.mapsAndUnmapsCleanly`, `ReviewRegression.midiLearnLearnsAppliesAndSurvivesAClear` | NO-TEST |
| INC-19 (DnD) | Drag and drop wherever samples are imported — IR card yes; backing-track audio (Practice > Track) has chooser only; no DnD test | `ToneMatchPanel.cpp:IrSlotEditor::filesDropped` | TONE MATCH IR cards; Practice Track `TrackTab::openButton` (no DnD) | `MidiImport.aDropOnTheWindowImports` (MIDI only) | PARTIAL |
| INC-20 (Tooltips) | Hover tooltips everywhere | `LuthierKnob::attachTo` default tooltip; `TooltipWindow` | all controls | `Editor.everyHitRegionOnTheIllustrationDescribesItself` | DONE |
| INC-21 (Randomise) | Randomise: new settings each press; resets to defaults first | `PresetManager::randomise` | File > Randomise; Ctrl+R; Easy dice | `Presets.randomiseRespectsLocks`, `Presets.randomiseNeverProducesSomethingBroken` | DONE |
| INC-22 (Build) | Test every combination; logic-error and GUI-workflow pass | `Tests/CombinationTests.cpp` | n/a | `Combo.*` | OWNED (claude/luthier-audit owns the failing Combo.* and BETA_TEST_REPORT) |
| INC-23 (Build) | VST3 + Standalone; future CLAP and Linux | `CMakeLists.txt` `LUTHIER_FORMATS` (+AU on Apple), `clap_juce_extensions_plugin`; `scripts/package_linux.sh` | n/a | CI `.github/workflows/build.yml` (pluginval, clap-validator) | DONE |
| INC-24 (Debug) | Debug window shows raw data live as settings change | `Support/Diagnostics` ring buffer | Debug overlay (Ctrl+D; Help > Debug) `DebugPanel` | `Diagnostics.ringBufferAndSelfTestWork`, `Editor.everyOverlayShortcutOpensItsOwnOverlayAndEscapeClosesIt` | DONE |
| INC-25 (Debug) | "Create log on crash" checkbox, dated file, off at every load, report copied at its top | `Diagnostics::crashLogEnabled {false}`, `flushCrashLog`; `PluginProcessor.cpp:156` | Debug overlay checkbox | - | NO-TEST |
| INC-26 (Debug) | Tell the user where the logs are, how to send them, send both on hard crash | `HelpContent` "debug" | HELP > Debug; Debug overlay "Open folder" | `HelpTab.theContentCoversWhatIncludeMdAsksFor` | DONE |
| INC-27 (Debug) | Obvious signals of misconfiguration | `Diagnostics::runSelfTest` | Debug overlay; troubleshooting file | `Diagnostics.ringBufferAndSelfTestWork` | DONE |
| INC-28 (Debug) | "Reset all settings to default" hard reset + clear caches | `PluginProcessor::hardResetAndClearCaches` | Debug overlay `hardResetButton`; Options > Diagnostics | - | NO-TEST |
| INC-29 (Debug) | Export troubleshooting file: self-test, settings, audio + MIDI config, **licence**, version, DAW — no licence section, no MIDI input/mapping summary | `Diagnostics::buildTroubleshootingReport` | Debug overlay `troubleshootButton`; Options > Diagnostics | `Diagnostics.ringBufferAndSelfTestWork` | PARTIAL |
| INC-30 (Easter egg) | Pixel click reveals a hidden effect tab with a close button and "secret" tooltips — exists (`SecretPanel`, Wolf), reveal untested | `LuthierEngine.cpp` §8b hidden effect | notch pixel → `SecretPanel` | `Effects.secretEffectIsStableAtMaximumRegeneration` | OWNED (CLI/easter-egg items deferred to last per brief) |
| INC-31 (theme) | Implement theme.md | see theme.md part | - | - | PARTIAL |

<!-- counts DONE=16 NO-GUI=0 NO-TEST=7 PARTIAL=5 MISSING=0 OWNED=3 -->
