# Luthier v0.1.0 Beta 2

The Free and Pro installers identify their edition in the filename and product
name. The release workflow freezes one source commit and builds every selected
platform/edition with LTO, complete unit suites and strictness-10 plugin
validation before creating a draft. BUILD-PROVENANCE.json records that commit
and the qualification run; SHA256SUMS.txt identifies the exact downloads.

## Changes since the published alpha

- Consolidated single pedalboard, saved-state and routing work.
- Correct guitar/parts override ordering for explicit player selections made
  during or just after a preset load.
- Preserve recorded loop audio during same-format host reinitialization; update
  recorder format after host sample-rate changes and protect concurrent exports.
- Preserve cathedral early reflections through correctly sized buffers.
- Repair rhythm MIDI timing, first-strum delivery, release retries and retained
  input during structural engine-lock contention.
- Retain pedalboard bypass delay and use one routing plan per audio block.
- Move host latency notifications off the audio callback.
- Crossfade prepared cabinet/body IR replacements and preserve cabinet bypass
  delay and raw IRs across host preparation.
- Correct CE-2 BBD response against the unchanged distortion acceptance gate.
- Repair queued asset destruction under libc++, exact host gain capture under
  fused arithmetic, and explicit tiny-buffer regression fixtures.

## Downloads and installation

- Windows x64: Setup installer or portable archive, with standalone, VST3,
  CLAP and factory Resources. Close the DAW before installing or replacing
  plugin files, then rescan the selected plugin format in the host.
- macOS Intel/Apple Silicon: universal installer with standalone, AU, VST3,
  CLAP and factory Resources. Close the host before installation, then rescan
  its plugins. The Pro beta also includes a universal bundle archive.
- Linux x64: standalone and plugin packages with factory Resources.

## Qualification and limitations

Automated qualification does not establish compatibility with every DAW,
interface or driver. Hardware listening, installation and saved-session
acceptance are separate from CI. Developer ID signing/notarization on macOS
and publisher signing on Windows depend on configured identities; use the
per-platform build logs to establish their actual status. Factory impulse
responses are synthesized rather than measured captures. The remaining GUI
fit and feature limitations are tracked in docs/KNOWN_ISSUES.md and the status
inventory; this beta does not claim every planned feature is complete.

Report crashes or sound regressions with OS, DAW, sample rate, buffer size,
plugin format, reproduction steps and the BUILD-PROVENANCE.json source SHA.
