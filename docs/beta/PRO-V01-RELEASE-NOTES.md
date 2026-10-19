# Luthier Pro v0.1 Beta

This release provides the Pro edition of Luthier, a physically modelled guitar and bass instrument, for Windows x64, Linux x86-64 and macOS on Intel and Apple Silicon.

The release is built from the exact merge commit recorded in `BUILD-PROVENANCE.json`. Publication requires the full automated test suite and the existing VST3/CLAP validators at strictness 10 to pass on all three platforms. Archive checks additionally inspect executable formats and architecture, Pro product names, factory content and platform payloads. `SHA256SUMS` contains the SHA-256 of every downloadable archive and the provenance file.

## Downloads and installation

| Archive | Contents and installation |
| --- | --- |
| `Luthier-Pro-v0.1-beta-Windows-x64.zip` | Extract the complete archive. Run `Luthier Pro.exe` with `Resources` beside it. Copy the complete `Luthier Pro.vst3` bundle to `C:\Program Files\Common Files\VST3` for a DAW, and factory `Resources` to `C:\ProgramData\Luthier\Resources`. The CLAP file belongs in `C:\Program Files\Common Files\CLAP`. |
| `Luthier-Pro-v0.1-beta-Linux-x64.tar.gz` | Extract, enter the extracted folder, and run `./install.sh` for a per-user installation. The archive includes standalone, VST3, CLAP and factory content. The standalone needs ALSA and an X11 or XWayland desktop; your distribution must satisfy the executable's library requirements. |
| `Luthier-Pro-v0.1-beta-macOS-universal.zip` | Extract and open the `macos` folder. Copy `Luthier Pro.app` to Applications, VST3 to `~/Library/Audio/Plug-Ins/VST3`, AU to `~/Library/Audio/Plug-Ins/Components`, and CLAP to `~/Library/Audio/Plug-Ins/CLAP`. Copy `Resources` to `~/Library/Application Support/Luthier/Resources`. Each executable contains both arm64 and x86_64 code. |

The macOS ZIP bundles are ad hoc signed and are not Apple notarized. Developer ID signing and notarization are not prerequisites for these beta archives. Windows signing depends on the repository's configured signing credentials; this release does not promise a publisher certificate.

Close the DAW before replacing plug-ins, then rescan its plug-in folders. Keep the complete VST3 bundle intact. The existing activation architecture is retained, and no license key is embedded in these packages. The separate beta2 60-day trial changes are not included in this release. This release adds no artificial beta expiry.

## Known beta issues

These issues remain documented from the earlier beta and are outside the release fixes in this build.

- At 1920 × 1080, the window can open larger than the screen and some controls can be difficult to reach. Lower the UI scale in Options under Appearance.
- Switching directly from electric to acoustic through the header dropdown can retain the previous amplifier, making the acoustic sound electric or very quiet. Choose the acoustic through the Workshop family switch, or enable auto-normalize in Options under Audio.
- The single-coil mains hum has no dedicated on/off switch. Set its amount to zero in Character under Noise Floor to silence it.
- The jam-band drums are synthesised and remain rough.
- The theme chooser in Options under Appearance and the normalize switch in Options under Audio can be easy to miss.

## First play and feedback

Launch the standalone and select your audio output, or add Luthier Pro to a MIDI instrument track in a DAW. Load a factory preset and send MIDI notes. Confirm that stopping playback silences the instrument, then save and reopen the session to check that its state returns.

This is a testing release. Automated validation does not establish compatibility with every DAW, audio driver or hardware configuration. Live host compatibility and production use remain subject to testing. Report reproducible problems through the repository's Issues page, including the OS, host version, plug-in format, sample rate, buffer size, preset, steps to reproduce, archive checksum and source commit from the provenance file.

The previous `v1.0.0-beta1` draft remains separate and is not replaced by this release.
