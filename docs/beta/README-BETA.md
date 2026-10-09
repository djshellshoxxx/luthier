# Luthier beta — first play

**Version:** 1.0.0-beta1
**Editions:** Luthier Free and Luthier Pro
**Platforms:** Linux x86-64 (`.tar.gz`, `.deb`) and Windows x64 (installer `.exe` plus portable `.zip`). macOS packages follow in a later build.
**Archive SHA-256:** Check the adjacent `<archive>.sha256` sidecar, or the `SHA256SUMS.txt` published with the release. Do not install an archive whose checksum does not match.
**Status:** Testing. Every build passes the full automated unit-test suite plus pluginval (VST3) and clap-validator (CLAP) at strictness 10. Live DAW host testing is still under way, so treat host compatibility as unverified for now and please report anything that misbehaves.

## Before you start

Luthier is a physically modelled guitar and bass instrument. Play it from a MIDI keyboard or a MIDI track in a DAW, or run the standalone application on its own. This beta provides the VST3 and CLAP plug-in formats and a standalone app.

Compare the downloaded file's checksum with the release's `SHA256SUMS.txt` before installing. If it does not match, stop and report it.

### Known limitations in this beta

These are confirmed and are being addressed in the next update; workarounds are given where one exists.

- On a 1920x1080 display the window can open larger than the screen, and a few controls can be hard to reach. Workaround: lower the UI scale in Options, under Appearance.
- Switching straight from an electric to an acoustic guitar using the header dropdown can keep the previous amplifier, so the acoustic may sound electric or very quiet. Workaround: choose the acoustic through the Workshop family switch, or turn on auto-normalize in Options, under Audio.
- The single-coil mains hum has no dedicated on/off switch yet. To silence it, set its amount to zero in the Character tab's Noise Floor controls.
- The jam-band drums are fully synthesised and still rough; improved drum synthesis is in progress.
- The theme switcher and the output-normalize toggle both exist but are easy to miss — the theme chooser is in Options under Appearance, and the normalize switch is in Options under Audio.

## Install

### Linux

1. Extract `Luthier-Free-1.0.0-beta1-linux-x64.tar.gz` (or the Pro archive).
2. Open a terminal in the extracted folder.
3. Run `./install.sh` for a per-user install. A system-wide install is optional: `sudo ./install.sh --system`.
4. Rescan plug-ins in your DAW. A user install places the VST3 in `~/.vst3`, the CLAP in `~/.clap` when included, and the standalone in `~/.local/bin/luthier`.

A `.deb` package is also provided for Debian and Ubuntu systems: install it with `sudo apt install ./luthier-free_1.0.0-beta1_amd64.deb` (or the `luthier-pro` package). The two editions deliberately conflict, because they share the standalone command and content folder; install one at a time.

### Windows

The easiest path is the installer:

1. Run `Luthier-Free-1.0.0-beta1-Setup-win64.exe` (or the Pro installer) and follow the prompts. It places the VST3 and CLAP in the shared system plug-in folders, the standalone in Program Files, and the factory content in `C:\ProgramData\Luthier\Resources`.
2. Rescan plug-ins in your DAW.

If you prefer not to install, use the portable zip:

1. Extract `Luthier-Free-1.0.0-beta1-portable-win64.zip` to a folder you can write to, and keep the folder layout intact.
2. For the standalone, run the Luthier application from that folder (`Luthier Free.exe` or `Luthier Pro.exe`) and keep its adjacent `Resources` folder beside it.
3. For VST3, copy the complete edition bundle (`Luthier Free.vst3` or `Luthier Pro.vst3`) to `C:\Program Files\Common Files\VST3\`, and copy the archive's `Resources` folder to `C:\ProgramData\Luthier\Resources\`. If a `.clap` is included, copy it to `C:\Program Files\Common Files\CLAP\`. Rescan plug-ins in your DAW.

Free and Pro are separate products with their own identities, so you can install both side by side if you wish.

## Five-minute first play

1. Open your DAW and create a new project, or launch the standalone app and choose an audio output first.
2. Add Luthier as an instrument on a MIDI or instrument track.
3. Open the preset browser and select any factory preset.
4. Send a few MIDI notes. Confirm that you hear the instrument and that stopping playback silences it.
5. Change a clearly audible control such as Drive or Tone and listen for the change.
6. Save the project, close the host, reopen the project, and check that the preset and sound return.

If a step fails, stop and open an issue on the Luthier GitHub repository with these details:

- Luthier version and edition, the archive filename and its SHA-256, and the source commit if you have it
- OS version and architecture; the host and its exact version; the plug-in format
- Audio interface, driver, sample rate, buffer size, preset, project, and relevant control values
- Expected and observed result, how often it happens, and its impact
- Exact steps to reproduce, a relevant log excerpt, and any workaround
- Whether it also happens in a brand-new project

Share only material you are allowed to share. Remove account names, personal paths, licence keys, and unrelated project content before posting.

## Replace or remove

- **Linux:** run the installed `~/.local/share/luthier/uninstall.sh` for a user install, or `/usr/local/share/luthier/uninstall.sh` for a system install. Do not add `--purge` unless you intend to delete your user data. If you used the `.deb`, remove it with `sudo apt remove luthier-free` (or `luthier-pro`).
- **Windows:** close the DAW and the standalone first. If you used the installer, uninstall from Settings. If you used the portable zip, remove the copied edition VST3 bundle and the `C:\ProgramData\Luthier\Resources` folder only when no other Luthier edition still uses it, then delete the extracted portable folder. Removing Luthier never touches your own files under `Documents\Luthier`.

The release manifest records the gates run and the platform scope for this build.
