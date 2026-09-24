## docs/PRESET_FORMAT.md

The on-disk format matches the doc's structure: magic `luthier.preset` with legacy `format` accepted, schema 1 and newer schemas loaded, unknown top-level keys preserved, normalised `parameters`, the `strings` block, `midiMap`, atomic `TemporaryFile` saves, dated backups and the 30-day sweep, and the host-state superset. Four claims are wrong. The field table documents `format` instead of `magic`. `midiMap` writes every mapped CC, not only non-default ones. The MidiTarget value table is off by one from 11 upward. And, the one engine defect, a key missing from `parameters` keeps the previous sound's value instead of falling back to the default, so the "minimal preset" example does not behave as described.

| Req | Summary | Engine location | GUI location | Test | Status |
|---|---|---|---|---|---|
| PF-1 (intro) | Plain JSON, `.luthierpreset` | `Presets/PresetManager.cpp:toVar/saveToFile` | n/a | `Presets::stateRoundTripsExactly` | DONE |
| PF-2 (Where) | Factory in bundle `Resources/Presets/Factory/<Category>/`; user `Documents/Luthier/Presets/User/<Category>/` | `PresetManager.cpp:~60-141` | n/a | - | NO-TEST |
| PF-3 (Where) | Extra folders registered in Options; sub-folder = category, root = "User" | `PresetManager::addSearchFolder`, scan `info.category` | Options > FILE LOCATIONS "Add a preset folder..." | - | NO-TEST |
| PF-4 (Where) | Factory preset never overwritten; saving makes user copy | `PresetManager::saveCurrent` (`isFactory`) + editor Save As fallback | Ctrl+S / File > Save | - | NO-TEST |
| PF-5 (Safety) | Atomic save: temp file, flush, rename | `PresetManager.cpp:~1059 juce::TemporaryFile::overwriteTargetFileWithTemporary` | n/a | - | NO-TEST |
| PF-6 (Safety) | Replaced version copied to `Backup/<yyyy-mm-dd>/`, several per day | `PresetManager.cpp:~948-1012` | n/a | `Presets::savingBacksUpTheVersionItReplaces` | DONE |
| PF-7 (Safety) | Startup sweep deletes backups > 30 days by folder name | `PresetManager::pruneOldBackups`, `kBackupRetentionDays = 30` | n/a | - | NO-TEST |
| PF-8 (Safety) | Unknown fields survive load + re-save | `PresetManager::fromVar` `unknownFields` | n/a | `Presets::unknownFieldsSurviveARoundTrip` | DONE |
| PF-9 (Safety) | Magic checked first; non-presets refused; legacy `"format": "luthierpreset"` accepted | `PresetManager::fromVar` `kMagic/kLegacyMagic` | n/a | `Presets::aFileWithoutTheMagicMarkerIsRefused`, `ErrorLog::arefusedPresetLoadIsRecordedAndChangesNothing` | DONE |
| PF-10 (Metadata) | Metadata table and minimal example use `magic: "luthier.preset"`; legacy `format` noted as accepted | `PresetManager::toVar:356` | n/a | `Presets::aFileWithoutTheMagicMarkerIsRefused` | DONE |
| PF-11 (Metadata) | schemaVersion 1; higher still loads (logged) | `kSchemaVersion = 1`, `NEWER_SCHEMA` branch | n/a | `MidiExport::importWarnsOfAdvancedRangesAndNewerSchemas` (MIDI path); no preset test | NO-TEST |
| PF-12 (Metadata) | pluginVersion, name, category (overrides folder), author, description, tags (searchable in browser) | `toVar:356-369`, scan `:256-271`, `PresetBrowserPanel` search "by name or tag" | Preset browser | - | NO-TEST |
| PF-13 (parameters) | Every automatable param by ID, normalised 0-1 | `toVar:376-402` | n/a | `Presets::stateRoundTripsExactly`, `Presets::anAdvancedValueSurvivesTheRoundTrip` | DONE |
| PF-14 (parameters) | A missing key falls back to that parameter's default — `fromVar` only sets params present in the block (`params->hasProperty`), so absent ones keep the previously loaded value | `PresetManager::fromVar:~605-615` | n/a | - | MISSING |
| PF-15 (IDs) | Prefix table: `macro_*` = seven Easy macros plus `macro_assign_a/b` | `Parameters.h` ParamIDs, `EffectsChain::kNumSlots = 8` | n/a | `Parameters::everyParameterHasAUniqueIdAndSaneDefault` | DONE |
| PF-16 (IDs) | Slot `_type/_bypass/_mix/_p0.._p9` read from pedal descriptors | `ParamIDs::slotParam`, `Pedal::kMaxParams = 10` | Pedal rack | `GuiReach::everySlotTypeBypassAndMixHasAControl`, `PresetPedals.*` | DONE |
| PF-17 (strings) | numStrings 1-12, useCustomTuning, openFrequencyHz, detune, realismDetune (persisted), fineTune, customGauge, muted, customTemperament x12 | `toVar:426-453`, `fromVar:705-735`, apply `:776+` | Advanced Strings / headstock popover | `ReviewRegression::aPresetWithoutAStringsBlockClearsThePreviousDetune`, `Presets::audioIsIdenticalAfterARoundTrip` | DONE |
| PF-18 (midiMap) | Doc: every mapped CC is written, defaults included; load starts from defaults, a missing CC keeps its default, 0 unmaps | `toVar:455-467` | n/a | - | DONE |
| PF-19 (midiMap) | Value table: 11 = Slide guitar, 12 Pinch, ... 21 Body — actual enum: 10 SlideGuitarToggle, 11 PinchHarmonic, 12 NaturalHarmonic, 13 Tap, 14 StrumSpeed, 15 StrumDirection, 16 Humanize, 17 Drive, 18 Tone, 19 Space, 20 Body, 21 Attack | `MidiInterpreter.h:MidiTarget` | n/a | - | MISSING |
| PF-20 (midiMap) | MIDI Learn mappings not in preset, in plugin state | `PluginProcessor.cpp "midiLearn"` | n/a | `ReviewRegression::midiLearnLearnsAppliesAndSurvivesAClear` | DONE |
| PF-21 (Host state) | Superset: preset, midiLearn (parameter/cc/channel/min/max/inverted), ui (advancedMode, tooltips, selectedString, easterEggFound, editor size), lockedParameters, slotBActive | `PluginProcessor.cpp:getStateInformation ~2189-2210`, `MidiLearnManager::toVar` | n/a | `HostState::aSessionSurvivesThePrepareThatFollowsIt`, `HostState::theSameParametersGiveTheSameStateHoweverTheyArrived` | DONE |
| PF-22 (By hand) | Drop in user folder, Rescan presets in Options, minimal preset valid | `FileLocationsPage::rescanButton`, `fromVar` | Options > FILE LOCATIONS | - — and see PF-14 | NO-TEST |
| PF-23 (By hand) | Choice normalisation = index/(n-1); guitar_type has 25 entries, Les Paul = 2/24 | `GuitarLibrary.h:GuitarType` (25 types, LesPaul = 2) | n/a | `Parameters::everyParameterTextRoundTrips` | DONE |
| PF-24 (Compat) | Times in seconds/Hz; identical at other sample rates | params in ms/Hz | n/a | `Engine::sampleRateChangesAreSurvived` | DONE |
| PF-25 (Compat) | IR missing -> modal body and procedural cab; preset never fails to load | `IrLibrary`, `Cabinet` fallback | n/a | `Cabinet::procedualFallbackRemovesTheFizz`, `Editor::aFailedPresetLoadAndAMissingIrEachRaiseABannerOnce` | DONE |

<!-- counts DONE=15 NO-GUI=0 NO-TEST=8 PARTIAL=0 MISSING=2 OWNED=0 -->
