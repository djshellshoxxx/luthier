# Luthier beta — first play

> Release owner: replace every bracketed field and remove this note before adding the file to a beta archive. Record only results backed by the release evidence.

**Version:** [beta version]  
**Platform/archive:** [Linux x86-64 .tar.gz / Windows x64 portable .zip]  
**Archive SHA-256:** [checksum]  
**Status:** [not tested / testing / verified for the listed hosts]

## Before you start

Luthier is a guitar and bass instrument. Play it from a MIDI keyboard or a MIDI track in a DAW. This beta supports only the formats and hosts listed below.

**Tested hosts:** [host and version for each platform]  
**Known limitations:** [list only confirmed limitations; write “None recorded” only after review]

Compare the downloaded file's checksum with the release manifest before installing. If it does not match, stop and contact [beta contact].

## Install

### Linux

1. Extract `Luthier-[version]-linux-x64.tar.gz`.
2. Open a terminal in the extracted `Luthier-[version]-linux-x64` folder.
3. Run `./install.sh` for your user. A system install is optional: `sudo ./install.sh --system`.
4. Rescan plugins in your DAW. The user install places VST3 in `~/.vst3`, CLAP in `~/.clap` when included, and the standalone in `~/.local/bin/luthier`.

### Windows

1. Extract `Luthier-[version]-portable-win64.zip` to a folder you can write to. Keep the folder layout intact.
2. For standalone, run `Luthier.exe` from that folder and keep its adjacent `Resources` folder.
3. For VST3, copy the complete `Luthier.vst3` bundle to `C:\Program Files\Common Files\VST3\`. Copy the archive's `Resources` folder to `C:\ProgramData\Luthier\Resources\`. Rescan plugins in your DAW.
4. [Add any release-specific, host-tested steps here.]

## Five-minute first play

1. Open one of the tested hosts above and create a new project.
2. Add Luthier as an instrument on a MIDI/instrument track. For standalone, choose an audio output first.
3. Select the factory preset **[preset name used in the release smoke test]**.
4. Send a few MIDI notes. Confirm that you hear the instrument and that stopping playback silences it.
5. Change **[main control tested]** and listen for the change.
6. Save the project, close the host, reopen the project, and check that the preset and sound return.

If a step fails, stop and use the [bug report template](BUG_REPORT_TEMPLATE.md). Include the host, OS, archive checksum, and exact steps. Do not attach private or copyrighted projects unless you have permission.

## Replace or remove

- **Linux:** run the installed `~/.local/share/luthier/uninstall.sh` for a user install, or `/usr/local/share/luthier/uninstall.sh` for a system install. Do not add `--purge` unless you intend to delete user data.
- **Windows:** close the DAW and standalone. Remove the copied `Luthier.vst3` bundle and the `C:\ProgramData\Luthier\Resources` folder only if no other Luthier version uses it; remove the extracted portable folder. This does not remove your files in Documents\Luthier.

Read the [Linux and Windows beta plan](../plans/BETA_LINUX_WINDOWS.md) for the release gates and platform scope.
