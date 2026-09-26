# GAPS-HOST coverage

Gap-fill workstream for area HOST (branch `claude/luthier-gaps-host`), against
`docs/audit/SPEC_SWEEP.md` rows for: installer, host-integration, qa-polish,
updates-telemetry, controllers, live-performance, performance-budget,
midi-export, notation-export. Only MISSING / PARTIAL / NO-TEST / NO-GUI rows
are in scope; OWNED and DONE rows belong to other workstreams.

Note: this checkout is ahead of the audit in places (e.g. `PerfBudgetTests.cpp`,
`ContentPackageTests.cpp`, `InstallLayout`, `StandaloneApp.cpp` already exist,
and UT-9's beta-channel behaviour was already implemented and tested). Rows
below were re-checked against the live code before being touched; a few are
marked "already done" rather than re-implemented.

| Row ID | What was done | Test | Status |
|---|---|---|---|
| HI-2 / HI-31 | `CMakeLists.txt`: `NEEDS_MIDI_OUTPUT` FALSE -> TRUE (producesMidi() was already true) | `PluginBuses::midiOutputIsAnnounced` | DONE |
| HI-10 | `PluginProcessor::isBusesLayoutSupported`: main output must be stereo (mono branch removed), per spec/host-integration.md section 2 | `PluginBuses::monoMainOutputIsRefused` | DONE |
| HI-8 | `LUTHIER_BUILD_STRING` compile definition (git short SHA / `LUTHIER_BUILD_STRING` env override, e.g. a CI run id), `Diagnostics::getFullVersionString()`, used in the troubleshooting report and the Diagnostics page's export-button tooltip | `HostState::versionCarriesABuildString` | DONE |
| HI-20 | Root JSON state now carries `formatVersion` (`LuthierAudioProcessor::kCurrentStateFormatVersion`) | `HostState::theStateCarriesAFormatVersion` | DONE |
| HI-24 | Unknown root-level keys (a newer build's section) are kept in `unknownHostSections` and re-emitted on the next save; a `formatVersion` ahead of this build raises one banner via `takeGuitarNotices()` | `HostState::unknownSectionsSurviveWriteBack` | DONE |
| HI-25 | A blob older than `kCurrentStateFormatVersion` (or missing the key) is backed up verbatim to `Diagnostics/state-backup-<date>.json` before the rest of `restoreState` runs | `HostState::anOldBlobIsBackedUpBeforeMigration` | DONE |
| HI-54 | `docs/HOST_COMPATIBILITY.md` written, covering every host-integration.md section 9 quirk in user-facing language, flagging the still-open gaps (MPE auto-detect, Logic PC mapping control, standalone device polling / virtual MIDI-out toggle, macOS document types) | n/a (docs) | DONE |
| HI-16, HI-22, HI-29, HI-37, HI-39, HI-41, HI-45, HI-47 | Not started | - | left for a follow-up helper (see below) |
| CT-2 | The chosen controller profile id now travels in `LuthierAudioProcessor` (`controllerProfileId`, saved/restored at root key `controllerProfile`); `ControllersPage` reselects and re-applies it on construction | `Controllers::theChosenProfileSurvivesTheSessionRoundTrip` | DONE |
| CT-7 | Fixed the real bug: `ParameterBridge::applyToEngine` rewrote `mpe_enabled`/`bend_range` from their parameters every block, undoing an MPE profile's flag and 48-semitone bend on the next block. `ControllersPage::applySelectedProfile` now pushes the profile's values into those parameters via `setValueNotifyingHost` | `Controllers::anMpeProfileSurvivesTheParameterBridge` | DONE |
| CT-17 | MPE master-channel notes are now ignored: `MidiInterpreter::setMpeMasterChannel`/`getMpeMasterChannel`, checked in `handleNoteOn`'s `GuitarController` branch when `mpeEnabled`; `ControllerProfileLibrary::apply` wires `profile.mpeMasterChannel` through | `Controllers::mpeMasterChannelNotesAreIgnored` | DONE |
| CT-9, CT-10, CT-11, CT-12, CT-18/26, CT-19, CT-22/27 | Not started | - | left for a follow-up helper |
| UT-9 | Already implemented and tested (`Telemetry::updateCheckReadsTheManifest`'s beta-channel sections) - the sweep's NO-TEST status was stale for this checkout | (existing) `Telemetry::updateCheckReadsTheManifest` | already done |
| UT-2, UT-4, UT-12, UT-13, UT-16..19, UT-21, UT-22, UT-26, UT-30, UT-31 | Not started | - | left for a follow-up helper |
| Other installer / qa-polish / live-performance / performance-budget / midi-export / notation-export rows | Not started this session | - | left for a follow-up helper |

## Notes for the next helper

- Many installer.md and qa-polish.md work-list items require editing
  `.github/workflows/*` (release.yml install-test jobs, pluginval strictness,
  runner matrix) which this session's instructions put off-limits ("the CI
  cadence there is set by the product owner"). Those rows are marked DEFERRED
  in spirit but were not written into spec/DECISIONS.md (out of scope for this
  helper) - flag them to the product owner before attempting.
- `live-performance.md` LP-11 (live-action MIDI-learn targets: next/prev/by-
  value snapshot, tap, kill, panic, setlist nav) is the one high-leverage item
  that unblocks LP-1, LP-14, LP-21, LP-26, LP-29, LP-37 in one pass - a good
  first pick for whoever continues this area.
- `midi-export.md` MX-3 and `notation-export.md` NE-1 (moving export off the
  message thread) touch the same shape of problem (capture the performance on
  the message thread, run the writer on a worker, post the result back with
  `MessageManager::callAsync`) - worth doing together.
- Before implementing a row, re-check it against the live code rather than
  trusting the sweep's status verbatim: this checkout has moved on from the
  audit in several places (see the note at the top of this file).
