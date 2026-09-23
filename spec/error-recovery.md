# ERROR RECOVERY SPEC

Every failure mode the plugin can hit, and what it does about each.
Currently scattered across updates-telemetry, installer, and
gui-integration's notification list; this file consolidates.

## 0. Ground rules

1. **Never crash on bad input.** Corrupt files, missing references,
   nonsense MIDI, exhausted memory: all handled gracefully.
2. **Never silently degrade.** If Luthier can't do what was asked, it
   says so. Notifications are non-modal but visible.
3. **Never destroy user work.** A refused load leaves the file
   untouched. A migration writes to a new file and backs up the
   original. A failed save leaves the previous version intact.
4. **Prefer partial success.** Load what you can; note what you
   couldn't. A preset with one missing IR still loads with the fallback
   IR and a warning; a tune with one missing section still plays the
   sections it has.
5. **Log everything.** Every failure writes to
   `~/Documents/Luthier/Diagnostics/errors-<yyyymm>.log` in a
   human-readable format, whether or not telemetry is enabled.
6. **Surface errors at the right level.** Recoverable: notification
   banner. Recoverable but disruptive: banner + halt current
   operation. Unrecoverable: modal with reset instructions.

## 1. File-load failures

| Failure | Response |
|---|---|
| File does not exist at path | Banner: "Preset X not found." Fall back to previously loaded state; do not clear the current session. |
| File is not readable (permissions) | Banner: "Cannot read X (permission denied)." Same fallback. |
| File is not UTF-8 | Banner: "File X is not a valid Luthier file." Do not attempt to guess encoding. |
| File is not JSON | Banner + suggest re-saving from a working install. |
| `magic` field missing or wrong | Banner: "File X does not appear to be a Luthier file." |
| `schema` field is newer than plugin supports | Banner: "File X was made by a newer Luthier version. Update to open." |
| `schema` field is older and no migration exists | Banner: "File X uses a format Luthier no longer supports." |
| Migration required and available | No banner; migration runs; original moved to Backup; info banner "Migrated X from schema N to schema M" (auto-dismisses after 5 s). |
| Migration fails partway | Banner: "Could not migrate X, original preserved." No changes to disk. |
| Referenced file (guitar, IR, part) missing | Info banner: "X referenced Y, not found; using default." Load succeeds. |
| Referenced file corrupt | Same as missing. |
| Cyclic reference (guitar A references part that references guitar B that references A) | Refuse with banner, log cycle. |

## 2. Save failures

| Failure | Response |
|---|---|
| Destination folder not writable | Banner: "Cannot save to X (permission denied)." Save aborted; nothing on disk changed. |
| Disk full | Same. |
| Save succeeded but rename failed | Delete the temp file, banner. Never leave partial. |
| Concurrent save from two plugin instances | The later writer wins with a "Warning: your save overwrote another change" banner. Backup rule preserves both. |
| Auto-save (session recorder) hits the same folder as a running user save | Session recorder saves under a different unique name and continues. |

Every save follows the atomicity rule in `file-formats.md` 13:
temp-file, fsync, rename.

## 3. Audio-engine failures

| Failure | Response |
|---|---|
| Sample-rate change from host mid-play | Reprepare all engines at the new rate on the next block; brief silent gap (< 20 ms); IRs and circuit filters re-resampled; info banner "Sample rate changed to X kHz, IRs re-resampled." |
| Block-size change from host mid-play | Reprepare buffer sizes; same brief gap. |
| Bus layout change from host | Reprepare routing; if the requested layout is not advertised, refuse with the host's standard error path (JUCE returns false from `isBusesLayoutSupported`). |
| Denormal or NaN detected on the audio thread | Guard catches, replaces with zero, logs to error log with the offending module id. Does not surface a banner (would spam); a Diagnostics-panel counter shows the running total. |
| CPU overrun (rolling 200 ms average > 85% block budget) | Engage CPU-relief mechanisms per `performance-budget.md` 8 in order; if all five stages engage, banner "CPU limit reached; some strings muted to keep audio running." |
| CPU overrun sustained after all relief | Banner promotes to warning colour. If sustained 10 s more, offer to switch off the heaviest optional module (feedback / freeze / reverb) in-banner. |
| Audio underrun reported by host | Log; increment Diagnostics counter; no banner (host owns underrun display). |
| Convolution stage returns garbage (usually an IR load edge case) | Fall through to bypass, banner "Cabinet IR failed to load; bypassed." |

## 4. MIDI failures

| Failure | Response |
|---|---|
| Malformed MIDI event | Log; drop the event. |
| MIDI CC out of range | Clamp; do not treat as failure. |
| Unknown SysEx (not Luthier profile) | Ignore silently. |
| Corrupt Luthier-profile SysEx | Log; drop that event; continue processing others. |
| MIDI flood (> 5000 events per second) | Process what fits in the block budget; drop the rest with a "MIDI flood: N events dropped" throttled banner (once every 5 s). |
| MIDI Learn arm timed out (no MIDI received in 30 s) | Silently disarm; banner "MIDI Learn cancelled (no MIDI received)." |

## 5. Workshop failures

| Failure | Response |
|---|---|
| Part swap during heavy CPU | Command queues, applies at next block boundary; no user-visible delay under 50 ms. |
| Attempted swap of incompatible part (bass pickup onto guitar) | UI card shows warning badge, part is not offered for that slot; if forced via API, refuse with banner "This part is not compatible with the current guitar family." |
| Guitar family change during playback | Held notes decay through the new engine's parameters; brief silent bridging if the string count changes; info banner "Changed to family X; released strings will continue on old settings until they decay." |
| Shadow audition timeout (Alt-hover on a card released but audio thread doesn't confirm within 200 ms) | UI reverts to committed spec; log; no banner. |
| Part-acoustics mapping produces invalid coefficients | Refuse the swap, keep committed spec, banner "Cannot use part X (invalid physical parameters)." |
| User saves a `.luthierguitar` that fails validation | Banner "Guitar has invalid configuration: [reason]." Save refused; UI stays in unsaved-changes state. |

## 6. Tune Builder failures

| Failure | Response |
|---|---|
| Malformed chord progression text | Highlight the offending token in red; live status line "Cannot parse: [reason]." Playback stops for the affected section only. |
| Melody generation produces no notes (rare) | Fall through to previous melody; banner "Could not generate melody with current constraints; try widening range." |
| Sung / hummed capture pitch confidence too low across the whole recording | Banner "Could not detect pitch reliably. Try recording again with less background noise." No notes added. |
| Section reorder produces zero-length setlist | Refuse the last removal; banner "A tune must have at least one section." |
| Loop lookahead exceeds available RAM (very long tune) | Loop as best possible; log; UI shows "loop tail truncated" indicator. |

## 7. Preset / snapshot failures

| Failure | Response |
|---|---|
| Snapshot recall during preset load | Queue the recall; apply after load completes. If load fails, discard the queued recall with an info banner. |
| Snapshot recall while looper is recording | Recall applies; looper continues recording under the new state; layer's stored MIDI captures the state change as a state boundary event. |
| Preset load while tune is playing | Load applies at the next section boundary if within 4 seconds, otherwise immediately with an info banner "Preset changed mid-tune, some section state reset." |
| Undo when there is nothing to undo | UI's Undo shortcut is disabled; if invoked via API, no-op. |
| A / B compare with an empty B slot | Banner "B slot is empty; save current state to B first." |

## 8. Network failures

| Failure | Response |
|---|---|
| Update check network error | Silent; retry on next check window. Log to error log. |
| Update download interrupted | Partial file discarded; user can retry. |
| Crash upload failure | Dump stays on disk; banner "Could not upload crash report; the dump is at [path]." User can attach manually. |
| License activation offline | Fall through to offline activation flow. |
| License revalidation failed during grace | Countdown banner shows days remaining. |
| License grace expired | Modal "Please reactivate your license." Plugin continues in demo mode (audio muted every 60 seconds for 2 seconds) until reactivation. |

## 9. Host disconnect

| Failure | Response |
|---|---|
| Host closes plugin instance | Save state via `getStateInformation`; release resources cleanly. |
| Host crashes | Nothing the plugin can do; on next host launch, plugin restores from host's saved state (if any). |
| Standalone loses audio device (unplugged interface) | Poll audio device list; when a compatible device appears, switch to it and banner "Audio device changed to Y." If none appears, banner "No audio device available" with a retry button. |
| Standalone loses MIDI input | Same: poll, auto-recover on reappearance, banner. |

## 10. Corrupted user state

| Failure | Response |
|---|---|
| User-global config unreadable | Rename to `.corrupted-<timestamp>`, write a fresh default; banner "Preferences reset (previous file corrupted, backed up)." |
| User-global config invalid schema | Same. |
| A folder Luthier expects (e.g. `~/Documents/Luthier/Presets/`) is missing | Create it; log. |
| A referenced content-update folder is missing | Log; treat updates as not installed. |

## 11. First-run in a broken environment

| Environment | Response |
|---|---|
| No writeable Documents folder | Prompt user for an alternative location for user data; save the choice to the plugin's install-adjacent config. |
| OS below minimum supported version | Modal "Luthier requires Windows 10 / macOS 13 / Ubuntu 22.04 or later." Plugin exits. |
| No audio device (Standalone) | Prompt user to install / connect one before continuing. |

## 12. Plugin crash

Handled by the crash reporter per `updates-telemetry.md` 4:
- Minidump to `Diagnostics/crash-<timestamp>.dmp`.
- Troubleshooting bundle captured at the last opportunity.
- On next launch (crash reporting opted in): prompt to upload.
- On next launch (opted out): a "Luthier crashed last session; the
  dump is at [path]" info banner.

## 13. Error log format

`~/Documents/Luthier/Diagnostics/errors-<yyyymm>.log`, one JSON
object per line:

```json
{ "ts": "2026-04-14T15:32:11.083Z",
  "severity": "warn",
  "module": "PresetSystem",
  "code": "MISSING_REFERENCE",
  "message": "Preset 'Modern Overdrive' referenced 'Cab-Match-A.wav', not found",
  "context": { "preset_path": "...", "expected_path": "..." } }
```

Severities: `debug`, `info`, `warn`, `error`. `debug` and `info`
only written when Diagnostics verbose is on (Options -> Diagnostics).

Rotates monthly. Old logs pruned by the 30-day sweep with the file
backups.

## 14. Banner priority

At most 3 banners visible at once; a fourth incoming banner replaces
the oldest. Priority:
1. Errors (warning colour).
2. Warnings (warning colour).
3. Info (accent colour).

Banners auto-dismiss after 5 s unless they contain a required
action.

## 15. Testing failure modes

Every failure mode listed above has a fixture and a test. Fixtures
live in `Tests/Fixtures/Errors/` grouped by section number. Every
new failure mode added must ship with a fixture and a test.

Bug bash (qa-polish.md 8) includes a "break the plugin" pass where
testers deliberately deliver corrupt files, network unplugs, audio
device removals, and MIDI floods; every symptom must map to one of
the responses above.

## 16. What is intentionally not caught

- Bugs in third-party plugins loaded by the host (out of scope).
- Bugs in the host itself (out of scope; captured in crash dump if
  they surface as a Luthier crash).
- User error that is not a plugin failure (e.g. "I saved to the wrong
  folder" is not a Luthier failure to handle).
