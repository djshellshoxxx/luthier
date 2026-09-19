# UPDATES, TELEMETRY, AND CRASH REPORTING SPEC

Fills the gap between include.md's on-device debug feature and actually
getting a fix to the user. Everything here is opt-in and privacy-first.

## 0. Ground rules

1. Nothing leaves the user's machine without an explicit opt-in. Default
   for every telemetry, crash-report, and update-check option is **off**.
2. No personally identifiable information is ever collected. Not license
   info, not filenames, not preset names, not IP addresses beyond what a
   normal HTTPS request requires.
3. Every outbound network call is logged locally in a rotating text file
   the user can inspect, with request destination, size, timestamp, and
   payload category.
4. Update checks run on a worker thread. Nothing blocks the audio thread
   for network reasons. Ever.
5. Update installation never happens automatically. The plugin only
   notifies; the user runs the installer.

## 1. Update check

- Opt-in in Options -> Updates.
- Frequency: on plugin load (throttled to once per 24 h), or manual "Check
  now" button.
- Endpoint: fetches a small JSON manifest from a versioned URL. Manifest
  lists latest stable, latest beta, minimum-supported, changelog URL,
  download URL per platform.
- Comparison: semantic version comparison against the running build.
- If a newer version is available, shows a non-modal banner in the header
  with "What's new" link and "Download" link. Never auto-installs.
- Beta channel: separate toggle. Beta users see beta releases as well.

Manifest format:
```json
{
  "schema": 1,
  "latest_stable": "1.4.2",
  "latest_beta": "1.5.0-beta3",
  "minimum_supported": "1.0.0",
  "changelog_url": "https://.../changelog",
  "downloads": {
    "windows_x64": "https://.../Luthier-1.4.2-win64.exe",
    "macos_universal": "https://.../Luthier-1.4.2-mac.pkg",
    "linux_x64": "https://.../Luthier-1.4.2-linux.tar.gz"
  }
}
```

## 2. Delta updates

- Where practical, the installer supports delta packages that patch an
  existing installation without re-downloading the full asset library.
- Deltas are optional; full installers are always available as fallback.
- Delta application uses `bsdiff`-style patches for binaries and rsync-style
  file-level diffs for the resource tree.

## 3. Telemetry

Two categories, both opt-in independently:

**A. Usage telemetry**
- Reports which panels the user opens, which presets they load, aggregate
  DSP CPU load, feature-flag adoption.
- No preset names, no user preset content, no filenames.
- Payload is a small JSON blob sent daily if telemetry is on and the user
  has been active in the plugin since the last send.

**B. Diagnostics telemetry**
- Reports plugin errors caught by the runtime (non-crash), diagnostic
  warnings, and non-identifying host information (host name, sample rate,
  buffer size, plugin format).

Both categories share the same on-disk log location:
`~/Documents/Luthier/Diagnostics/telemetry-<yyyymm>.log`. Contents are
plain JSON, one record per line, human-readable. The user can read exactly
what would be sent.

## 4. Crash reporting

- Opt-in in Options -> Diagnostics.
- On crash, a minidump is written to
  `~/Documents/Luthier/Diagnostics/crash-<yyyymmddhhmmss>.dmp`, along with
  the troubleshooting file (see include.md) captured at last opportunity
  before the crash.
- On next launch, if crash reporting is enabled, the plugin prompts the
  user to upload the crash dump with a diff-viewer showing what would be
  sent.
- Upload is a single HTTPS POST. No retries beyond three attempts. If it
  fails, the dump stays on disk.
- Crash dumps never contain audio or MIDI data.

## 5. License activation

- License activation is optional depending on the release channel (open
  source vs commercial).
- Commercial builds validate a license key via a single online activation
  and one revalidation per 30 days. Grace period of 14 days offline.
- Offline activation available for studios with air-gapped machines: a
  challenge string is displayed, user copies to a web form on another
  machine, receives a response string, pastes back.
- License data never leaves the machine after activation completes;
  revalidation sends only a signed proof of activation, not the license
  key.
- Removing a license (deactivation) is one click and immediate; it frees
  the seat.

## 6. Privacy dashboard

New tab in Options -> Privacy.
Contents:
- What each telemetry category collects, plain English.
- Toggle for each.
- "View last upload" opens the local log at the last uploaded record.
- "Clear all local logs" button.
- Endpoint URLs, editable for enterprise deployments that route through a
  local proxy.
- One-click "Turn everything off and delete all diagnostic files" for
  full paranoia mode.

## 7. Enterprise configuration

- A system-wide `luthier-policy.json` under a documented path allows IT
  admins to force telemetry off, force update-checks to a private mirror,
  and disable crash uploads by policy.
- The plugin displays a "Managed by policy" indicator when this file is
  present.

## 8. Tests

- No-network mode: disconnect network, verify plugin functions normally
  and no user-visible error appears from update or telemetry systems.
- Opt-in default: fresh install, verify all four toggles (usage,
  diagnostics, crash upload, update check) are off.
- Policy override: place a policy file, verify user cannot enable
  disallowed features.
- Crash dump privacy: verify a written dump contains no audio, MIDI, or
  preset data (grep fixtures).
- Delta apply: apply 5 sequential delta patches, verify final SHA matches
  a full installer of the same target version.
