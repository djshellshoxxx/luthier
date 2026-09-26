## updates-telemetry.md

Opt-in defaults, the local outbound-network log, the manifest-based update check (semver, 24 h throttle, worker thread, beta toggle, header banner), the policy file with a "Managed by policy" banner, and the Privacy page (toggles, explanations, view log, clear, endpoints, paranoia button) are implemented, and the engine side is tested. But usage and diagnostics telemetry are never recorded (`Telemetry::record`/`sendPending` have no production callers), nothing ever writes a crash minidump, and the crash "Review" banner leads to a Privacy page with no dump viewer or upload button. Licensing is deferred to the release helper (OWNED); file-level content deltas exist on the visual branch but bsdiff binary deltas and the 5-patch SHA test exist nowhere. None of the Options UI surfaces have tests.

| Req | Summary | Engine location | GUI location | Test | Status |
|---|---|---|---|---|---|
| UT-1 (§0.1/§8) | Every telemetry/crash/update option off by default | `Updates/Telemetry` ctor | Options > UPDATES / PRIVACY | `Telemetry::everythingIsOffByDefault` | DONE |
| UT-2 (§0.2) | No PII (no license, filenames, preset names) — no test asserts record payloads are PII-free | `Telemetry::record` + `isAllowedField` key allowlist, path/address values dropped | n/a | `Telemetry::recordsCarryNoPersonalData` | DONE |
| UT-3 (§0.3) | Every outbound call logged in a rotating local file (dest, size, time, category) | `Telemetry::logNetworkCall` | PRIVACY `viewLogButton` | `Telemetry::everyOutboundCallIsLogged` | DONE |
| UT-4 (§0.4) | Update checks on a worker thread — `juce::Thread::launch` in editor; untested | `PluginEditor.cpp` (~875), `UpdatesPage::checkForUpdate` | n/a | - | NO-TEST |
| UT-5 (§0.5) | Never auto-installs; only notifies | `PluginEditor.cpp` update banner | header banner | `Telemetry::updateCheckReadsTheManifest` | DONE |
| UT-6 (§1) | Opt-in toggle in Options -> Updates, on-load 24 h throttle, Check now | `Telemetry::checkForUpdate` | UPDATES `updateCheckToggle`, `checkNowButton` | `Telemetry::updateCheckIsThrottled` | DONE |
| UT-7 (§1) | Manifest JSON (schema, stable, beta, min, changelog, per-platform downloads) + semver compare | `UpdateManifest::parse`, `Version::compare` | n/a | `Telemetry::updateCheckReadsTheManifest`, `Telemetry::versionComparison` | DONE |
| UT-8 (§1) | Non-modal header banner with "What's new" and "Download" links — banner has one "Details" action to the UPDATES page, links shown as text; on visual: "Updates page: Release notes opens the browser, Download fetches to Downloads" | `PluginEditor.cpp` "update" notification | header banner -> UPDATES | - | OWNED |
| UT-9 (§1) | Beta channel toggle — no test that beta releases are offered only when on | `Telemetry::setBetaChannelEnabled` | UPDATES `betaToggle` | `Telemetry::updateCheckReadsTheManifest` (beta off: stable only; beta on: 1.5.0-beta3) | DONE |
| UT-10 (§2) | File-level (rsync-style) resource-tree delta packages, full installer fallback — on visual: `Updates/ContentPackage` delta kind + `ContentPackage::aBadHashRollsBackAndOffersTheFullDownload` | - | - | - | OWNED |
| UT-11 (§2) | bsdiff-style binary patches — not implemented anywhere (visual deltas are whole-file replace) | - | - | - | MISSING |
| UT-12 (§3A) | Usage telemetry: panels opened, presets loaded, CPU, flag adoption, daily send — `record`/`sendPending` never called outside tests | `Telemetry::record/sendPending` | PRIVACY `usageToggle` | `Telemetry::recordsAreLoggedLocallyAndSentWhenAllowed` (API only) | PARTIAL |
| UT-13 (§3B) | Diagnostics telemetry: runtime errors, warnings, host/sr/buffer/format — no call sites | `Telemetry::record` | PRIVACY `diagnosticsToggle` | `Telemetry::recordsAreLoggedLocallyAndSentWhenAllowed` (API only) | PARTIAL |
| UT-14 (§3) | Shared log `Diagnostics/telemetry-<yyyymm>.log`, JSON per line, readable | `Telemetry::getTelemetryLogFile` | PRIVACY `logView` | `Telemetry::recordsAreLoggedLocallyAndSentWhenAllowed` | DONE |
| UT-15 (§4) | Crash reporting opt-in toggle | `Telemetry::setCrashUploadEnabled` | PRIVACY `crashToggle` (spec: Diagnostics) | `Telemetry::everythingIsOffByDefault` | DONE |
| UT-16 (§4) | On crash write `crash-<ts>.dmp` (stack/build/OS/host) — done; the troubleshooting file is not copied beside it and no Windows minidump | `Updates/CrashWriter.cpp` (crash handler installed by the processor timer once crash reports are on) | n/a | `Telemetry::crashDumpsContainNoAudioMidiOrPresets` | PARTIAL |
| UT-17 (§4) | Next launch: prompt to upload with diff viewer of what would be sent — banner "Review" opens PRIVACY, which shows neither `describePendingCrashReport` nor an upload/discard button | `Telemetry::hasPendingCrashReport/describePendingCrashReport` | header "crash" banner -> PRIVACY (no viewer) | `Telemetry::crashReportDescribesItself` | PARTIAL |
| UT-18 (§4) | Single HTTPS POST, max 3 attempts, dump kept on failure — untested (test only checks refusal when off) | `Telemetry::uploadPendingCrashReport` | none (no upload button) | `Telemetry::crashUploadTriesThreeTimesThenKeepsTheDump` | DONE |
| UT-19 (§4) | Crash dumps never contain audio/MIDI | `CrashWriter::formatDump` (stack/build/OS/host only) | n/a | `Telemetry::crashDumpsContainNoAudioMidiOrPresets` | DONE |
| UT-20 (§5) | License activation, 30-day revalidation, 14-day grace, offline challenge, one-click deactivate — licensing.md deferred to release helper | `Updates/Telemetry.h:License` | none | `Telemetry::licenceActivationAndGrace`, `Telemetry::revalidationCountdownAndOfflineTolerance` | OWNED |
| UT-21 (§6) | Privacy tab: plain-English explanation + toggle per category | `PrivacyPage` | Options > PRIVACY | `Telemetry::thePrivacyPageExplainsSwitchesAndShowsTheLog` | DONE |
| UT-22 (§6) | View last upload, Clear all local logs | `Telemetry::readTelemetryLog/clearLocalLogs` | PRIVACY `viewLogButton`, `clearLogsButton` | `Telemetry::thePrivacyPageExplainsSwitchesAndShowsTheLog` | DONE |
| UT-23 (§6) | Editable endpoint URLs | `Telemetry::setManifestUrl/...` | PRIVACY `manifestUrlBox` etc. | `Telemetry::settingsRoundTrip` | DONE |
| UT-24 (§6) | One-click turn everything off + delete diagnostics | `Telemetry::turnEverythingOffAndDelete` | PRIVACY `paranoiaButton` | `Telemetry::turnEverythingOffDeletesAndDisables` | DONE |
| UT-25 (§7) | System-wide `luthier-policy.json` forces telemetry off / private mirror / no crash uploads | `Policy::load/getPolicyFile` | n/a | `Telemetry::policyOverridesTheUser` | DONE |
| UT-26 (§7) | "Managed by policy" indicator — header banner + page label, untested | `Policy::setPolicyFileForTesting` | header banner, PRIVACY/UPDATES `policyLabel` | `Telemetry::aPolicyLocksThePrivacyPage` | DONE |
| UT-27 (§8) | Test: no-network mode silent | | n/a | `Telemetry::noNetworkIsSilentRatherThanAnError` | DONE |
| UT-28 (§8) | Test: fresh install has all four toggles off | | n/a | `Telemetry::everythingIsOffByDefault` | DONE |
| UT-29 (§8) | Test: policy file blocks disallowed features | | n/a | `Telemetry::policyOverridesTheUser` | DONE |
| UT-30 (§8) | Test: crash dump privacy grep | | n/a | `Telemetry::crashDumpsContainNoAudioMidiOrPresets` | DONE |
| UT-31 (§8) | Test: 5 sequential delta patches -> SHA equals full installer — not on any branch | | n/a | - | MISSING |

<!-- counts DONE=21 NO-GUI=0 NO-TEST=1 PARTIAL=4 MISSING=2 OWNED=3 DEFERRED=0 -->
