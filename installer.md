# INSTALLER AND DISTRIBUTION SPEC

How Luthier gets from a CI build onto a user's machine and off again. Also
what an update, a clean uninstall, and an enterprise deployment look
like.

## 0. Ground rules

1. The installer never runs during the audio thread of a live host. It
   is a separate app, not a component of the plugin itself.
2. Every installer is code-signed (Windows) or notarized (macOS). Linux
   packages carry a detached PGP signature.
3. Uninstalling removes every file the installer wrote and preserves
   user data by default. Explicit "purge" removes user data too.
4. No installer requires elevated privileges beyond what is strictly
   necessary to write to the destination paths.
5. Installers are deterministic: same source, same version, same
   platform produce byte-identical output.

## 1. Windows installer

Format: NSIS or Inno Setup executable, `.exe`.
Signing: Extended Validation code-signing certificate.
Naming: `Luthier-<version>-Setup-win64.exe`.

### 1.1 Install flow

1. Splash: Luthier logo, version, "Preparing installer".
2. Language: auto-detect from OS, offer override.
3. Licence: display, require accept.
4. Component picker (default: all selected):
   - VST3 plugin (mandatory).
   - Standalone application.
   - Factory content (presets, IRs, samples).
   - Documentation (PDF and HTML manual).
   - Uninstaller (mandatory).
5. Install locations:
   - VST3: `C:\Program Files\Common Files\VST3\` (fixed).
   - Standalone: `C:\Program Files\Luthier\` (editable).
   - Factory content: `C:\ProgramData\Luthier\` (editable).
   - Docs: alongside standalone.
6. Space check: refuse if less than required + 200 MB free.
7. Version check: if a same or newer version is installed, ask (upgrade,
   reinstall, cancel). If older, offer upgrade with a note that old
   files will be removed.
8. Install progress: file-by-file with a cancel that safely rolls back.
9. Post-install:
   - Register uninstaller in Add/Remove Programs.
   - Create Start menu entries.
   - Optionally launch standalone (default off).
   - Show "Install complete" with a "What's new" link.

### 1.2 Silent install (enterprise)

- `/S` flag: fully silent install, all defaults.
- `/D=<path>` overrides standalone install path.
- Exit codes: 0 success, non-zero failure with codes documented.
- Registry keys under `HKLM\Software\Luthier` record install location
  and version for scripted management.

### 1.3 Uninstaller

- Standard Windows uninstaller.
- Removes every file it installed (tracked via install manifest).
- Preserves `C:\Users\<user>\Documents\Luthier\` by default.
- Explicit "Remove user data" checkbox.
- Refuses to run while any DAW is running that has loaded the plugin
  (detected by loaded DLL enumeration).

## 2. macOS installer

Format: `.pkg` inside a signed and notarized `.dmg`.
Signing: Developer ID Application + Installer certificate.
Notarization: required.
Naming: `Luthier-<version>-macOS.dmg`.

### 2.1 Install flow

1. Mount `.dmg` shows the `.pkg` and a "Read me first" link.
2. Standard macOS installer UI.
3. Licence acceptance.
4. Component picker: VST3, AU, Standalone, Content, Docs.
5. Install to standard paths:
   - VST3: `/Library/Audio/Plug-Ins/VST3/Luthier.vst3`.
   - AU: `/Library/Audio/Plug-Ins/Components/Luthier.component`.
   - Standalone: `/Applications/Luthier.app`.
   - Factory content: `/Library/Application Support/Luthier/`.
6. Admin password prompt only if writing to system paths.
7. Post-install: "Open Luthier" button.

### 2.2 Universal binary

- One `.pkg` supports Intel and Apple Silicon.
- Standalone is a fat binary; plugins are fat.

### 2.3 Uninstall

- Uninstall script bundled: `/Applications/Luthier/Uninstall.command`.
- Removes every file installed.
- Preserves `~/Documents/Luthier/` by default; option to purge.

## 3. Linux packaging

Formats:
- `.tar.gz` for manual install and non-Debian distros.
- `.deb` for Debian, Ubuntu, and derivatives.
- `.rpm` for Fedora, openSUSE (best-effort, not in every release).

Naming: `Luthier-<version>-linux-x64.tar.gz`, `luthier_<version>_amd64.deb`.

### 3.1 File layout

- VST3: `/usr/lib/vst3/Luthier.vst3/` or `~/.vst3/Luthier.vst3/`.
- Standalone: `/usr/bin/luthier` or `~/.local/bin/luthier`.
- Factory content: `/usr/share/luthier/` or `~/.local/share/luthier/`.
- Desktop file: `/usr/share/applications/luthier.desktop`.
- Icon: standard icon-theme paths.

### 3.2 Install

`.deb`: standard `apt install ./luthier_<version>_amd64.deb`.
Post-install script updates icon cache and desktop database.

`.tar.gz`: extract, run `install.sh` which offers user or system
install. Uninstall via `uninstall.sh`.

### 3.3 Dependencies

Documented in the package: JACK or PipeWire, ALSA, X11 or Wayland.
No bundled shared libraries beyond JUCE's static-linked pieces.

## 4. Standalone-only distribution

Every platform ships a "Standalone only" bundle for users who do not use
a DAW. Same installer with the plugin components deselected by default.

## 5. Update delivery

Full installers are always available. Delta packages available where
practical (updates-telemetry.md section 2).

### 5.1 Update check

Per updates-telemetry.md. On finding an update:
- Header banner shows "Version X.Y.Z available".
- Click opens release notes in browser.
- "Download" button fetches the platform-appropriate installer to the
  user's Downloads folder.
- Plugin does not auto-launch installers. User runs them manually.

### 5.2 Delta patches

- Where the delta is under 50% of the full installer size, offer both.
- Delta patches are one-way: cannot skip versions.
- Delta application verifies SHA-256 of the resulting installation
  against the manifest.
- Failed delta apply rolls back and prompts user to download the full
  installer.

## 6. First-run detection

After install, the plugin's first load creates:
- `~/Documents/Luthier/` folder tree per README.md.
- User-global config file: `~/Documents/Luthier/config/plugin.json`.
- Marker file: `~/Documents/Luthier/.installed_version` with version
  string.

Absence of the marker = first run: trigger onboarding.md flow.
Marker version differs from running version = upgrade: show upgrade
banner.

## 7. Enterprise deployment

- Managed installers accept command-line configuration.
- A policy file `luthier-policy.json` per updates-telemetry.md section 7
  can be pre-placed to enforce settings on first run.
- MSI wrapper available on request for Windows deployment via Group
  Policy.

## 8. Portable install (Windows only)

Advanced users: a `.zip` distribution that unpacks to any folder and
runs from there. No registry entries, no start menu, no auto-update.
Useful for USB-drive setups and locked-down machines.

Portable install does not install VST3 to the system path; user must
copy manually or point the DAW at the portable folder.

## 9. Verification

Every release publishes:
- SHA-256 checksums for every installer.
- PGP-signed manifest listing all installers and checksums.
- Published at a canonical URL for automated verification.

## 10. Rollback plan

If a released version turns out broken:
1. Update manifest is reverted to the previous version within 30
   minutes of decision.
2. Installers for the previous version remain available at their
   original URLs.
3. Users who already installed the broken version can downgrade via
   the previous installer, which detects the newer install and prompts
   to replace.
4. Rollback communicated via a header banner in the running plugin
   (via the same manifest that drives the update check).

## 11. Content updates without a full install

Factory presets, IR library additions, and translation catalog updates
can ship as a "content update" package smaller than a full installer:

- Signed `.luthiercontent` file.
- User drags onto the plugin window or opens via Options -> Updates.
- Applied to
  `~/Documents/Luthier/ContentUpdates/<name>/`, plugin picks up on
  next load.

Content updates never modify code; only data.

## 12. Tests

- Install / uninstall cycle on every platform: verify installed files
  match manifest, uninstall removes exactly those files.
- Upgrade: install version A, install version B, verify no A artefacts
  remain.
- Downgrade: install B, install A via A's installer, verify prompt and
  clean downgrade.
- Enterprise silent install with policy: verify policy takes effect on
  first run.
- Signature verification: every published installer verifies against
  the published PGP manifest.
- Portable Windows: unzip to arbitrary path, run, verify no writes
  outside the portable folder.
