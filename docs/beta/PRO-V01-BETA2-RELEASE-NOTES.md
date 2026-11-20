# Luthier v0.1.0 Beta 2

The Free and Pro installers identify their edition in the filename and product
name. The release workflow freezes one source commit and builds every selected
platform/edition with LTO, complete unit suites and strictness-10 plugin
validation before creating a draft. BUILD-PROVENANCE.json records that commit
and the qualification run; SHA256SUMS.txt identifies the exact downloads. The
authorized beta2 candidate publishes only after the complete download set is
verified against those checksums and the frozen source.

## Pedalboard in this beta

The reference pedalboard offers one pedal: the **Four-Stage Phaser**, a
circuit-derived behavioural model of a classic four-stage script-logo phaser
(speed, sweep depth, wet trim, stage mismatch, tempo sync, stereo phase and
mono/dual-mono stereo). It was the first reference model developed, and it is
the only one the browser, the Add menu and drag-and-drop offer. The other
planned models (the two choruses, the delay, the compressor and the loopers)
are deferred to later updates. A session saved with a pedal from an earlier
build still loads that pedal.

Phase 90 limitations still open in this beta: its CPU target (under 0.4 % of
one core) is not yet met on every reference machine, the "under 1 LSB at 24
bits" bypass-click measurement has not been defined, and the hardware listening
comparison has not been run. The supplemental Linux component qualification
completed two 30-minute concurrent board soaks and a four-rate extended phaser
render. Those checks do not establish live DAW or audio-device behaviour. See
[the qualification report](../status/QUALIFICATION-2026-11-19.md) and
[known issues](../KNOWN_ISSUES.md).

## Changes since the published alpha

- Consolidated single pedalboard, saved-state and routing work.
- Keep shared factory content when uninstalling one edition while the other
  remains installed, and preserve administrator-managed machine policy.
- Correct guitar/parts override ordering for explicit player selections made
  during or just after a preset load.
- Preserve recorded loop audio during same-format host reinitialization; update
  recorder format after host sample-rate changes and protect concurrent exports.
- Preserve cathedral early reflections through correctly sized buffers.
- Wait for a complete disk-loop hash snapshot before reporting persistence
  completion.
- Repair rhythm MIDI timing, first-strum delivery, release retries and retained
  input during structural engine-lock contention.
- Retain pedalboard bypass delay and use one routing plan per audio block.
- Move host latency notifications off the audio callback.
- Crossfade prepared cabinet/body IR replacements and preserve cabinet bypass
  delay and raw IRs across host preparation.
- Build shorter quality IR banks only when the response needs them, avoiding
  unused loader threads during preparation and short-IR replacement. Repreparing
  with a different partition size now resets every quality bank's latency.
- Reserve MIDI output capacity for pedal releases and remember a pressed note
  only after its event is accepted, preventing stuck pedals during bursts.
- Bound content-package input, entry sizes and total expansion before extraction;
  verify exact decoded entry lengths while retaining signed-package rollback.
- Consolidate the newer external-text decoding and file-parser QA fixes, with
  their regression fixtures and an opt-in fuzz harness.
- Correct CE-2 BBD response against the unchanged distortion acceptance gate.
- Keep an external clock in control when a tap-tempo request arrives, and retry
  tempo/clock UI notices after an activity flood fills the feedback queue.
- Preserve the LFO's exact waveform and phase while simplifying its phase wrap;
  flush the digital delay's exhausted meter state before it becomes subnormal.
- Repair queued asset destruction under libc++, exact host gain capture under
  fused arithmetic, and explicit tiny-buffer regression fixtures.

## Downloads and installation

- Windows x64: Setup installer or portable archive, with standalone, VST3
  and factory Resources. Close the DAW before installing or replacing
  plugin files, then rescan the selected plugin format in the host.
- macOS Intel/Apple Silicon: universal installer with standalone, AU, VST3
  and factory Resources. Close the host before installation, then rescan
  its plugins. The Pro beta also includes a universal bundle archive.
- Linux and CLAP builds are not part of this beta. They are still built and
  tested in development CI; CLAP has open host-state round-trip defects found
  by clap-validator.

## Qualification and limitations

Automated qualification does not establish compatibility with every DAW,
interface or driver. Hardware listening, installation and saved-session
acceptance are separate from CI. Developer ID signing/notarization on macOS
and publisher signing on Windows were not configured for candidate
`51fd4de677fd0d4a056ded5ee2e8845b9e81584d` in
[release run 44662100996](https://github.com/djshellshoxxx/luthier/actions/runs/44662100996).
Its Windows packages are unsigned. Its macOS bundles are ad-hoc signed, its
package installers are unsigned, and no Apple notarization was performed.
Factory impulse
responses are synthesized rather than measured captures. The remaining GUI
fit and feature limitations are tracked in docs/KNOWN_ISSUES.md and the status
inventory; this beta does not claim every planned feature is complete.

Report crashes or sound regressions with OS, DAW, sample rate, buffer size,
plugin format, reproduction steps and the BUILD-PROVENANCE.json source SHA.
