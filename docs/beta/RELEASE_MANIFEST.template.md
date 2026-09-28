# Luthier beta release manifest

> Release owner: complete from recorded build and host evidence. Leave unrun checks as **Not run**. Do not distribute an archive with placeholders or label it tested without evidence.

## Release identity

- Beta version:
- Release date:
- Integration branch and frozen source commit:
- Same source commit used for every platform? [yes / no — explain]
- Tag/release URL:
- Previous fallback archive:
- Known limitations:
- Beta contact / bug-report destination:

## Build environment

| Platform | OS image/version | Compiler/toolchain | CMake/Ninja | JUCE | CLAP dependency | Build command | Log locations |
|---|---|---|---|---|---|---|---|
| Linux x86-64 | | | | 8.0.10 | commit `55525c9858d4b25687be7759a5e0f70eccef218e` if CLAP built | `PLUGINVAL_STRICTNESS=10 scripts/ci_build.sh deps configure build test validate stage` | |
| Windows x64 | | Visual Studio 2022 / MSVC | | 8.0.10 | commit `55525c9858d4b25687be7759a5e0f70eccef218e` if CLAP built | `scripts/ci_build.ps1 -Step deps,configure,build,test,validate,stage -Jobs 2 -Strictness 10` | |

The build scripts default to `build-ci/`, stage products in `dist/linux` or `dist/windows`, and write logs under `build-ci/logs` unless configured otherwise. Record the actual paths if you override them.

## Artifacts

| Platform | Exact filename | Size | SHA-256 | Source commit | Contents inspected | Status |
|---|---|---:|---|---|---|---|
| Linux | `dist/installers/Luthier-[version]-linux-x64.tar.gz` | | | | | Not run |
| Linux, if produced | `dist/installers/luthier_[version]_amd64.deb` | | | | | Not run |
| Windows | `dist/installers/Luthier-[version]-portable-win64.zip` | | | | | Not run |

Linux packaging command: `scripts/package_linux.sh`. The tarball includes `install.sh` and `uninstall.sh`; the .deb is produced only when `dpkg-deb` is available.

Windows build/stage command: as above. Once PR #10 (`codex/luthier-portable-windows-beta`) is merged into the integration branch and its Windows packaging path has been tested on a Windows worker, run `scripts/package_windows.ps1 -PortableOnly -BetaReadme <completed-file>` and record the exact command and test evidence. Until both merge and Windows verification are complete, do not use or claim this installer-free path for the release; the current integration script still runs Inno Setup before creating the ZIP.

Checksum commands: Linux `sha256sum <archive>`; Windows PowerShell `Get-FileHash <archive> -Algorithm SHA256`.

## Automated checks

| Platform | Unit test command/result | VST3 pluginval (strictness 10) | CLAP validator (if included) | Logs |
|---|---|---|---|---|
| Linux | | | | |
| Windows | | | | |

A validator result is not a host-compatibility result. Attach or link the logs from this exact source commit.

## Host smoke-test evidence

| Platform | Host and version | Format | Install / scan | First sound | Preset change | Automation | Save/reopen | Offline render | Clean restart | Tester / date / evidence |
|---|---|---|---|---|---|---|---|---|---|---|
| Linux | | VST3 | | | | | | | | |
| Linux | | CLAP, if included | | | | | | | | |
| Windows | FL Studio | VST3 | | | | | | | | |
| Windows | [second VST3 host] | VST3 | | | | | | | | |
| Windows | | Standalone | N/A | | | N/A | | N/A | | |

Also record the Windows standalone audio/MIDI-device check and Linux standalone check here: [evidence links].

## Release decision

- No unresolved beta-blocking crash, audio-thread hang/dropout, stuck note, reset/stop, corrupt state, host automation, or data-loss issue: [yes / no / not assessed]
- Known limitations reviewed and written above: [yes / no]
- Exact downloadable archives match the checksums above: [yes / no]
- Decision: **Not assessed / Hold / Ready for invited beta**
- Decision owner and date:
- Evidence links:
