# Luthier v0.1.0 Beta 3

The Free and Pro installers identify their edition in the filename and product
name. The release workflow freezes one source commit and builds every selected
platform/edition with LTO, complete unit suites and strictness-10 plugin
validation before creating a draft. BUILD-PROVENANCE.json records that commit
and the qualification run; SHA256SUMS.txt identifies the exact downloads. The
authorized beta3 candidate publishes only after the complete download set is
verified against those checksums and the frozen source.

## What beta 3 adds

Beta 3 adds Linux x64 and the CLAP plug-in. All three platforms ship VST3, CLAP
and the standalone application, plus AU on macOS. The release workflow builds
the CLAP for both editions on Linux, Windows and macOS, validates it with
clap-validator 0.4.1 alongside pluginval on the VST3 (and the AU on macOS), and
ships it in every package that carries the VST3. Beta 2 shipped Windows and
macOS without CLAP.

## Pedalboard in this beta

Unchanged from beta 2. The reference pedalboard offers one pedal: the
**Four-Stage Phaser**, a circuit-derived behavioural model of a classic
four-stage script-logo phaser (speed, sweep depth, wet trim, stage mismatch,
tempo sync, stereo phase and mono/dual-mono stereo). It was the first reference
model developed, and it is the only one the browser, the Add menu and
drag-and-drop offer. The other planned models (the two choruses, the delay, the
compressor and the loopers) are deferred to later updates. A session saved with
a pedal from an earlier build still loads that pedal.

Phaser limitations still open in this beta: its CPU target (under 0.4 % of one
core) is not yet met on every reference machine, the "under 1 LSB at 24 bits"
bypass-click wording has not been ruled on, and the 30-minute soak and hardware
listening comparison have not been run. See docs/KNOWN_ISSUES.md.

## Changes since beta 2

- Linux x64 packages for both editions.
- The CLAP plug-in on Linux, Windows and macOS.
- The CLAP wrapper, clap-juce-extensions, is re-pinned to upstream commit
  6bde1083: parameter values are rescanned after a state load (clap-validator's
  state-reproducibility tests failed on the previous pin), latency is reported
  during activate, transport events are delivered in split blocks, and GUI
  resizing is guarded. The existing host-write intent patch is kept.
- A fresh CLAP instance reports its startup guitar's values as the defaults of
  the guitar-owned parameters (clap-validator's param-default-values test).
  VST3, AU and Standalone defaults, presets, sessions and the loudness table
  are unchanged.
- Preparing no longer waits for a parked or fading audio callback
  (re-activation took about 0.9 s; clap-validator parameter fuzzing timed out).

## Downloads and installation

- Windows x64: Setup installer or portable archive, with standalone, VST3,
  CLAP and factory Resources. Close the DAW before installing or replacing
  plugin files, then rescan the selected plugin format in the host. The
  portable archive's README.txt says where each plugin file belongs.
- macOS Intel/Apple Silicon: universal installer with standalone, AU, VST3,
  CLAP and factory Resources, each format a separate installer choice. Close
  the host before installation, then rescan its plugins. The Pro beta also
  includes a universal bundle archive.
- Linux x64: for each edition, an archive whose `install.sh` installs the
  standalone, VST3, CLAP and factory Resources for the current user (or for
  all users with `--system`), a standalone-only archive without the plugins,
  and a `.deb` package (`luthier-pro` or `luthier-free`). On Linux both
  editions use the same `luthier` command, desktop entry and content folder,
  so installing one edition replaces the other. The standalone needs ALSA
  (JACK or PipeWire for low-latency audio) and X11 or XWayland. No `.rpm` is
  provided while the version carries a pre-release suffix such as `-beta3`:
  RPM does not accept `-` in a package Version. On RPM-based distributions,
  use the archive and its `install.sh` instead.

## Qualification and limitations

Automated qualification does not establish compatibility with every DAW,
interface or driver. Linux and CLAP pass the same automated release gates as
the other platforms and formats: complete unit suites, plugin validation
(pluginval at strictness 10, clap-validator for the CLAP) and packaging.
Neither has been tested host by host, and this beta makes no DAW-by-DAW
compatibility claim for any format. Hardware listening, installation and
saved-session acceptance are separate from CI.
Developer ID signing/notarization on macOS, publisher signing on Windows and
GPG signatures for the Linux packages and the checksum manifest depend on
configured identities; use the per-platform build logs and the release assets
to establish their actual status. The Linux binaries are built on GitHub's
current Ubuntu runner; the `.deb` states the minimum glibc they need, and
older distributions may not run them. Factory impulse responses are
synthesized rather than measured captures. The remaining GUI fit and feature
limitations are tracked in docs/KNOWN_ISSUES.md and the status inventory; this
beta does not claim every planned feature is complete.

Report crashes or sound regressions with OS, DAW, sample rate, buffer size,
plugin format, reproduction steps and the BUILD-PROVENANCE.json source SHA. On
Linux, also name the audio backend (ALSA, JACK or PipeWire).
