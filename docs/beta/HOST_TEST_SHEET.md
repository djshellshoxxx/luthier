# Beta host test sheet

Use one copy per tester and release archive. Fill in observations; leave untested items as **Not run**. A validator pass does not replace a host test.

## Test setup

- Tester / date:
- Archive filename:
- Archive SHA-256:
- Luthier version and source commit (from manifest):
- OS edition/version/build and architecture:
- CPU / RAM:
- Audio interface and driver:
- Sample rate / buffer size:
- Host name, exact version, and plugin format:
- New install or upgrade:
- Preset tested:

## Install and first sound

Mark each **Pass / Fail / Blocked / Not run** and add a note for anything except Pass.

| Check | Result | Notes |
|---|---|---|
| Archive extracts with the expected VST3/standalone/Resources files | | |
| Host finds Luthier after a rescan | | |
| Luthier appears as an instrument and accepts MIDI | | |
| Factory preset loads and produces audible sound | | |
| Play then stop does not leave a stuck note or sound | | |
| Main sound control changes the sound and survives preset change | | |
| Editor opens, closes, and resizes without clipping or disappearing controls | | |

## Host and project checks

Make a separate row for every host and format tested. On Windows, include FL Studio and a second VST3 host. On Linux, include at least one DAW; test CLAP only if that format is present in the archive.

| OS | Host + version | Format | Scan | MIDI/audio | Preset while playing | Automation | Save/reopen | Offline render | Restart/shutdown |
|---|---|---|---|---|---|---|---|---|---|
| | | | | | | | | | |
| | | | | | | | | | |

For each host, verify: scan/discovery, MIDI sound, preset changes during playback, any advertised automation, project save/reopen with the same sound, and clean host shutdown/relaunch. Test offline render where the host supports it. For standalone, test audio-device selection and MIDI input separately.

## Replace/remove checks

| Check | Result | Notes |
|---|---|---|
| Replacing the plugin/archive follows the release instructions | | |
| Host rescans and finds only the intended Luthier version | | |
| Removing the beta leaves Documents/Luthier user files intact | | |
| A longer session of [duration] completes without crash, dropout, stuck note, or hang | | |

## Outcome

- Overall: **Pass / Fail / Blocked / Not run**
- Issues filed (IDs/links):
- Logs or short screen recording attached (if useful and safe to share):
- Notes for release owner:

Report a crash, audio dropout, stuck note, hang, broken project reopen, or missing user data with the [bug report template](BUG_REPORT_TEMPLATE.md). Avoid posting private project files or personal information.
