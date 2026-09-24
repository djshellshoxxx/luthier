## error-recovery.md

The preset load/refuse path (named banner, session untouched, JSON-lines error log with monthly file), missing-reference fallbacks with banners, sample-rate/block-size re-prepare with an info banner, the licence-grace countdown and the auto-dismissing banner strip are in place and tested. Almost everything else is thin: only PresetManager and PluginProcessor write to the error log, the verbose toggle and 30-day prune are never wired, a newer schema is loaded instead of refused (C-20), only one banner shows at a time with no error level (C-22), and MIDI flood/Learn timeout, crash minidumps, corrupt-config recovery, standalone device polling, A/B-empty and most Workshop/Tune failure banners do not exist. The visual branch owns the migration banner, CPU-limit banner, update-download discard, content packages and the fuzz/stress robustness tests.

| Req | Summary | Engine location | GUI location | Test | Status |
|---|---|---|---|---|---|
| ER-1 (§0.1) | Never crash on bad input — presets fuzzed here; visual adds byte-flip of every factory preset/guitar and MIDI storm tests | `Presets/PresetManager.cpp:fromVar`, loaders | n/a | `Presets::mutatedPresetsNeverCrashTheLoader` (visual: `Presets::everyByteOfAFactoryPresetFlippedIsRefusedOrLoads`, `Workshop::everyByteOfAFactoryGuitarFlippedIsRefusedOrLoads`, `Stress::midiStormDuringLoadsRecallsAndGuitarLoads`) | OWNED |
| ER-2 (§0.2) | Never silently degrade — preset/IR/part/SR banners exist; save failures, setlist load, A/B empty, MIDI drops silent | `UI/Notifications.cpp:NotificationCentre`; `PluginEditor::pollForNotifications` | banner strip | `Editor::aFailedPresetLoadAndAMissingIrEachRaiseABannerOnce` | PARTIAL |
| ER-3 (§0.3) | Never destroy user work — preset backup before overwrite and migration backup; guitar/tune/setlist saves have no backup | `PresetManager::backupBeforeOverwrite/backupMigratedOriginal` | n/a | `Presets::savingBacksUpTheVersionItReplaces`, `ModelGapsUi::aMigratedPresetKeepsItsOriginal` | PARTIAL |
| ER-4 (§0.4) | Prefer partial success (missing IR/part -> fallback + warning) | `IrSlot::fromVar`, `PartLibrary::loadGuitar` report | banners "ir-missing", "missing-part" | `Workshop::aMissingPartFallsBackAndSaysSo`, `WorkshopPresets::aMissingGuitarFileFallsBackToItsType` | DONE |
| ER-5 (§0.5) | Every failure logged to `Diagnostics/errors-<yyyymm>.log` — only PresetManager (9) and PluginProcessor (7) call `ErrorLog::write` | `Support/ErrorLog.cpp:write` | n/a | `ErrorLog::failuresAreLoggedAsReadableJsonLines` | PARTIAL |
| ER-6 (§0.6) | Recoverable -> banner; disruptive -> banner + halt; unrecoverable -> modal with reset instructions — no modal path, no "halt" semantics | `Notification::Level {info, warning}` | banner strip | - | PARTIAL |
| ER-7 (§1) | File not found -> "Preset X not found" banner, keep state | `PresetManager::loadPreset` FILE_NOT_FOUND | banner "preset-load" | `Editor::aFailedPresetLoadAndAMissingIrEachRaiseABannerOnce` | DONE |
| ER-8 (§1) | Permission denied -> "Cannot read X (permission denied)" — not distinguished from empty file | `loadPreset` FILE_UNREADABLE | banner "preset-load" | - | PARTIAL |
| ER-9 (§1) | Not UTF-8 -> "not a valid Luthier file", no guessing — falls through to JSON parse failure | `loadPreset` | banner | - | PARTIAL |
| ER-10 (§1) | Not JSON -> banner + suggest re-saving from a working install — no re-save hint | `loadPreset` NOT_JSON | banner | - | PARTIAL |
| ER-11 (§1) | Magic missing/wrong -> "does not appear to be a Luthier file" | `PresetManager::fromVar` BAD_MAGIC | banner | `Presets::aFileWithoutTheMagicMarkerIsRefused`, `ErrorLog::arefusedPresetLoadIsRecordedAndChangesNothing` | DONE |
| ER-12 (§1) | Newer schema -> refuse "made by a newer Luthier version" — loads and logs NEWER_SCHEMA (C-20 decided: refuse) | `fromVar` NEWER_SCHEMA | - | - | PARTIAL |
| ER-13 (§1) | Older schema without migration -> "format no longer supported" — schema <= 0 refused with generic text | `fromVar` BAD_SCHEMA | banner (generic) | - | PARTIAL |
| ER-14 (§1) | Migration: original to Backup + 5 s info banner "Migrated X from schema N to M" — backup here; banner on visual | `PresetManager::backupMigratedOriginal` | (visual) banner | `ModelGapsUi::aMigratedPresetKeepsItsOriginal` (visual: `Editor::aMigratedPresetRaisesOneInfoBanner`) | OWNED |
| ER-15 (§1) | Migration fails partway -> "Could not migrate X, original preserved", no disk change | - | - | - | MISSING |
| ER-16 (§1) | Referenced guitar/IR/part missing -> info banner, load succeeds | `IrSlot::fromVar`, `PartLibrary`, guitar-name migration | banners "ir-missing", "missing-part" | `Editor::aFailedPresetLoadAndAMissingIrEachRaiseABannerOnce`, `GuitarMigration::anUnknownGuitarKeepsThePresetAndSaysSo` | DONE |
| ER-17 (§1) | Referenced file corrupt -> same as missing — IR decode failure falls back; untested | `IrSlot::load`, `PartLibrary::loadGuitar` | banner | - | NO-TEST |
| ER-18 (§1) | Cyclic reference refused with banner, cycle logged | - | - | - | MISSING |
| ER-19 (§2) | Destination not writable -> banner, nothing changed — logged SAVE_UNWRITABLE, no named banner | `PresetManager::writeToFile` | Save As overlay stays open | - | PARTIAL |
| ER-20 (§2) | Disk full -> same | `writeToFile` | - | - | PARTIAL |
| ER-21 (§2) | Rename failed -> temp deleted, banner — temp removed by `TemporaryFile`, logged, no banner | `writeToFile` SAVE_RENAME_FAILED | - | - | PARTIAL |
| ER-22 (§2) | Concurrent save from two instances -> later wins + "overwrote another change" banner | - | - | - | MISSING |
| ER-23 (§2) | Session recorder picks a different unique name and continues | `Practice/Looper` timestamped names | Options > DIAGNOSTICS recorder toggle | - | NO-TEST |
| ER-24 (§2) | Every save temp-file, fsync, rename (file-formats 13) — preset saves only; `flush()` not fsync; guitar/tune/settings writers use `replaceWithText` | `PresetManager::writeToFile` | n/a | - | PARTIAL |
| ER-25 (§3) | Sample-rate change -> re-prepare, IRs re-resampled, info banner | `LuthierAudioProcessor::prepareToPlay`, `claimSampleRateChange` | banner "sample-rate" | `Editor::aSampleRateChangeIsAnnouncedOnceAndTheFirstOneIsNot`, `Engine::sampleRateChangesAreSurvived` | DONE |
| ER-26 (§3) | Block-size change -> re-prepare | `prepareToPlay` | n/a | `Engine::blockSizeChangesAreSurvived` | DONE |
| ER-27 (§3) | Bus layout change re-prepares; unadvertised layout returns false — refusal untested (visual: `Stress::busLayoutChangesMidPlay`) | `PluginProcessor::isBusesLayoutSupported` | n/a | `Routing::everyLayoutRendersCleanly` (render only) | NO-TEST |
| ER-28 (§3) | NaN/denormal guard -> zero, log with module id, Diagnostics counter — `sanitise` everywhere, no log, no counter | `DSP/*:sanitise` | Options > DIAGNOSTICS (no counter) | `Engine::fastSlidesProduceNoNansOrDenormals` | PARTIAL |
| ER-29 (§3) | CPU overrun -> relief ladder, "CPU limit reached" banner — on visual | (visual) `Support/CpuRelief.cpp` | (visual) banner | (visual: `CpuReliefUi::theBannerComesOncePerEpisodeAndGoes`, `CpuRelief::laddersUpAtEightyFivePercentAndBackDown`) | OWNED |
| ER-30 (§3) | Sustained overrun after relief -> warning colour; +10 s offer to switch off heaviest module in-banner — not on visual either | - | - | - | MISSING |
| ER-31 (§3) | Host underrun -> log + Diagnostics counter, no banner | - | - | - | MISSING |
| ER-32 (§3) | Convolution returns garbage -> bypass + "Cabinet IR failed to load; bypassed" — procedural fallback, no banner | `DSP/Amp/CabinetEngine.cpp:processFallback` | - | `Cabinet::procedualFallbackRemovesTheFizz` | PARTIAL |
| ER-33 (§4) | Malformed MIDI event -> log, drop — dropped, not logged | `Controllers/MidiInterpreter` | n/a | - | PARTIAL |
| ER-34 (§4) | CC out of range -> clamp | JUCE 7-bit `MidiMessage`; parameter clamping | n/a | - | NO-TEST |
| ER-35 (§4) | Unknown SysEx ignored silently | `MidiInterpreter` | n/a | - | NO-TEST |
| ER-36 (§4) | Corrupt Luthier-profile SysEx -> log, drop, continue — dropped, not logged | `MidiInterpreter` / `Notation` profile reader | n/a | - | PARTIAL |
| ER-37 (§4) | MIDI flood > 5000 ev/s -> process what fits, throttled "MIDI flood: N events dropped" banner every 5 s | - | - | - | MISSING |
| ER-38 (§4) | MIDI Learn arm 30 s with no MIDI -> disarm + "MIDI Learn cancelled" banner | `Support/MidiLearn.h` (no timer) | header MIDI Learn button | - | MISSING |
| ER-39 (§5) | Part swap under CPU queues to block boundary, < 50 ms | `Workshop/WorkshopBench` swap parking | WORKSHOP tab | `WorkshopSwap::aPartSwapDuringANoteIsClickFree`, `WorkshopSwap::aNotePlayedWhileParkedIsKeptNotDropped` | DONE |
| ER-40 (§5) | Incompatible part -> warning badge (advisory per C-21; refusal row does not apply) | `PartLibrary` compatibility warning | WORKSHOP card | `Workshop::incompatiblePartsFitWithAWarning` | DONE |
| ER-41 (§5) | Family change during playback -> held notes decay, "Changed to family X..." info banner | - | - | - | MISSING |
| ER-42 (§5) | Shadow audition not confirmed in 200 ms -> UI reverts, log | - | - | - | MISSING |
| ER-43 (§5) | Part-acoustics mapping gives invalid coefficients -> refuse swap, "Cannot use part X" banner | `Model/Workshop/PartAcoustics` (no validity check) | - | - | MISSING |
| ER-44 (§5) | Invalid `.luthierguitar` save refused with reason, stays unsaved — generic failure banner, no validation reason | `LuthierAudioProcessor::saveGuitarAs` | banner "save-guitar" | - | PARTIAL |
| ER-45 (§6) | Malformed progression -> offending token highlighted, "Cannot parse: reason" status line | `Tune/TuneHarmony` parse errors; `TunePanel` underline | TUNE tab progression field | `TunePanel::theProgressionFieldWritesTheSectionAndShowsErrorsWhereTheyAre`, `TuneBuilder::malformedShorthandIsRefusedWithANamedError` | DONE |
| ER-46 (§6) | Melody generation produces no notes -> keep previous melody + banner | `Tune/TuneMelody` | TUNE tab | - | MISSING |
| ER-47 (§6) | Hum capture low confidence -> "Could not detect pitch reliably" banner, no notes — capture itself is on tune-help (no banner there yet) | (tune-help) `Tune/TuneHumCapture` | TUNE tab | (tune-help: `HumCapture::thePitchTrackerFindsAVoicesPitchAndDoubtsNoise`) | OWNED |
| ER-48 (§6) | Removing the last section refused, "A tune must have at least one section" — `Tune::removeSection` allows it | `Tune/TuneModel.cpp:removeSection` | TUNE section menu Delete | - | MISSING |
| ER-49 (§6) | Loop lookahead exceeds RAM -> loop best effort, log, "loop tail truncated" indicator | - | - | - | MISSING |
| ER-50 (§7) | Snapshot recall during preset load queued; discarded with info banner if load fails — both run serially on the message thread; no discard banner | `PluginProcessor::recallSnapshot` | Live strip | - | PARTIAL |
| ER-51 (§7) | Snapshot recall while looper records -> state-boundary event in the layer's MIDI | - | - | - | MISSING |
| ER-52 (§7) | Preset load mid-tune -> at next section boundary if < 4 s, else now + info banner | - | - | - | MISSING |
| ER-53 (§7) | Undo with nothing to undo -> shortcut/button disabled, API no-op | `HeaderBar::updateUndoRedoState`, `canUndo` | header Undo | - | NO-TEST |
| ER-54 (§7) | A/B compare with empty B -> "B slot is empty; save current state to B first" — `recallSlot` silently no-ops | `PluginProcessor::recallSlot` | header A/B | - | MISSING |
| ER-55 (§8) | Update check network error -> silent, retry next window, log to error log — silent yes, not written to ErrorLog | `Updates/Telemetry.cpp:checkForUpdate` | n/a | `Telemetry::noNetworkIsSilentRatherThanAnError` | PARTIAL |
| ER-56 (§8) | Update download interrupted -> partial file discarded, retry — on visual | (visual) `Updates/UpdateDownloader.cpp` | Options > UPDATES | (visual: `Updates::theDownloadLandsInDownloadsUnderItsOwnName`) | OWNED |
| ER-57 (§8) | Crash upload failure -> dump stays, banner with path | `Telemetry::uploadPendingCrashReport` (no banner) | - | - | MISSING |
| ER-58 (§8) | Licence activation offline -> offline flow (licensing.md deferred) | `Telemetry` License | Options | `Telemetry::licenceActivationAndGrace` | OWNED |
| ER-59 (§8) | Revalidation failed during grace -> countdown banner | `PluginEditor::postStartupNotifications` "licence-grace" | banner | `Telemetry::revalidationCountdownAndOfflineTolerance` | DONE |
| ER-60 (§8) | Grace expired -> modal + demo mute 2 s every 60 s (licensing.md deferred) | - | - | - | OWNED |
| ER-61 (§9) | Host closes instance -> state saved, resources released | `getStateInformation`, JUCE lifecycle | n/a | `Presets::stateRoundTripsExactly`, `HostState::anUnpreparedInstanceSavesTheSameStateAsAPreparedOne` | DONE |
| ER-62 (§9) | Host crash -> restore from host's saved state | `setStateInformation` | n/a | `HostState::aSessionSurvivesThePrepareThatFollowsIt` | DONE |
| ER-63 (§9) | Standalone audio device lost -> poll, switch to compatible device + banner, else "No audio device" with retry | - | - | - | MISSING |
| ER-64 (§9) | Standalone MIDI input lost -> poll, auto-recover, banner | - | - | - | MISSING |
| ER-65 (§10) | User config unreadable -> rename `.corrupted-<ts>`, fresh defaults, "Preferences reset" banner — `UiPreferences::load` treats it as absent and later overwrites | `UI/UiPreferences.cpp:load`, `AccessibilitySettings::load`, `Telemetry` settings | - | - | MISSING |
| ER-66 (§10) | User config invalid schema -> same | - | - | - | MISSING |
| ER-67 (§10) | Expected folder missing -> create + log — created lazily, not logged | `writeToFile` `createDirectory` | n/a | - | PARTIAL |
| ER-68 (§10) | Content-update folder missing -> log, treat as not installed — content packages on visual | (visual) `Updates/ContentPackage.cpp` | - | (visual: `ContentPackage::*`) | OWNED |
| ER-69 (§11) | No writable Documents -> prompt for alternative location, saved to install-adjacent config | - | - | - | MISSING |
| ER-70 (§11) | OS below minimum -> modal + exit | - | - | - | MISSING |
| ER-71 (§11) | No audio device (Standalone) -> prompt to connect one | JUCE standalone default | - | - | MISSING |
| ER-72 (§12) | Crash -> minidump `Diagnostics/crash-<ts>.dmp` — reader exists, no writer / crash handler | `Telemetry` reads `crash-*.dmp` | - | - | MISSING |
| ER-73 (§12) | Troubleshooting bundle captured at crash — manual export only | `Support/Diagnostics.cpp:writeTroubleshootingReport` | Options > DIAGNOSTICS | - | PARTIAL |
| ER-74 (§12) | Next launch, opted in -> upload prompt — banner exists but never fires (no dumps written) | `PluginEditor::postStartupNotifications` "crash" | banner -> PRIVACY | `Telemetry::crashReportDescribesItself` | PARTIAL |
| ER-75 (§12) | Next launch, opted out -> "crashed last session; the dump is at [path]" info banner — no path, same banner | same | banner | - | PARTIAL |
| ER-76 (§13) | Log format: JSON lines ts/severity/module/code/message/context | `ErrorLog::write` | n/a | `ErrorLog::failuresAreLoggedAsReadableJsonLines` | DONE |
| ER-77 (§13) | debug/info only when Diagnostics verbose is on (Options > Diagnostics) — `setVerbose` never called, no toggle | `ErrorLog::setVerbose` | Options > DIAGNOSTICS (no toggle) | `ErrorLog::failuresAreLoggedAsReadableJsonLines` | NO-GUI |
| ER-78 (§13) | Rotates monthly | `ErrorLog::getLogFile(yyyymm)` | n/a | - | NO-TEST |
| ER-79 (§13) | Old logs pruned by the 30-day sweep — `pruneOldLogs` exists, never called | `ErrorLog::pruneOldLogs` | n/a | - | PARTIAL |
| ER-80 (§14) | At most 3 banners visible, 4th replaces oldest — one visible, rest queued (C-22 decided: show up to three) | `NotificationCentre` | banner strip | `Editor::notificationBannersQueueDismissAndRespectTheirActions` | PARTIAL |
| ER-81 (§14) | Priority errors > warnings > info (warning colour / accent) — no error level, FIFO order | `Notification::Level` | banner strip | - | PARTIAL |
| ER-82 (§14) | Auto-dismiss after 5 s unless action required | `NotificationCentre::autoDismissMs` | banner strip | `Editor::notificationBannersQueueDismissAndRespectTheirActions` | DONE |
| ER-83 (§15) | Every failure mode has a fixture in `Tests/Fixtures/Errors/<section>` and a test | - | - | - | MISSING |
| ER-84 (§15) | Bug-bash "break the plugin" pass maps every symptom to a response | - | - | - | MISSING |

<!-- counts DONE=14 NO-GUI=1 NO-TEST=7 PARTIAL=27 MISSING=27 OWNED=8 -->
