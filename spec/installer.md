# INSTALLER AND DISTRIBUTION SPEC

How Luthier gets from a CI build onto a user's machine and off again.
Also what an update, a clean uninstall, and an enterprise deployment
look like.

## 0. Ground rules

1. The installer never runs during the audio thread of a live host.
2. Every installer is code-signed (Windows) or notarized (macOS). Linux
   packages carry a detached PGP signature.
3. Uninstalling removes every file the installer wrote and preserves
   user data by default. Explicit "purge" removes user data too.
4. No installer requires elevated privileges beyond what is strictly
   necessary to write to destination paths.
5. Installers are deterministic: same source, same version, same
   platform produce byte-identical output.

## 1. Windows installer

Format: NSIS or Inno Setup, `.exe`.
Signing: EV code-signing certificate.
Naming: `Luthier-<version>-Setup-win64.exe`.

### 1.1 Install flow

1. Splash: Luthier logo, version, "Preparing installer".
2. Language: auto-detect from OS, offer override.
3. Licence: display, require accept.
4. Component picker (all selected by default):
   - VST3 plugin (mandatory).
   - Standalone application.
   - Factory content (presets, IRs, samples, parts library, guitars,
     tune templates, example tunes).
   - Documentation (PDF and HTML manual).
   - Uninstaller (mandatory).
5. Install locations:
   - VST3: `C:\Program Files\Common Files\VST3\` (fixed).
   - Standalone: `C:\Program Files\Luthier\` (editable).
   - Factory content: `C:\ProgramData\Luthier\` (editable).
   - Docs: alongside standalone.
6. Space check: refuse if less than required + 200 MB free.
7. Version check: same or newer installed = ask (upgrade, reinstall,
   cancel). Older = offer upgrade with note that old files will be
   removed.
8. Install progress: file-by-file with safe cancel / rollback.
9. Post-install:
   - Register uninstaller in Add/Remove Programs.
   - Register file associations: `.luthierpreset`, `.luthierguitar`,
     `.luthiertune`, `.luthierloop`, `.luthierset`, `.midprofile`.
   - Create Start menu entries.
   - Optionally launch standalone (default off).
   - Show "Install complete" with "What's new" link.

### 1.2 Silent install (enterprise)

- `/S` flag: fully silent, all defaults.
- `/D=<path>` overrides standalone install path.
- Exit codes documented.
- Registry keys under `HKLM\Software\Luthier` record install location
  and version for scripted management.

### 1.3 Uninstaller

- Standard Windows uninstaller.
- Removes every file it installed (tracked via install manifest).
- Preserves `C:\Users\<user>\Documents\Luthier\` by default.
- Explicit "Remove user data" checkbox (includes user parts, user
  guitars, user tunes, presets, loops, setlists, IRs).
- Refuses to run while any DAW that has loaded the plugin is running.

## 2. macOS installer

Format: `.pkg` inside a signed and notarized `.dmg`.
Signing: Developer ID Application + Installer certificate.
Notarization: required.
Naming: `Luthier-<version>-macOS.dmg`.

### 2.1 Install flow

1. Mount `.dmg`; shows `.pkg` and a "Read me first" link.
2. Standard macOS installer UI.
3. Licence acceptance.
4. Component picker: VST3, AU, Standalone, Content, Docs.
5. Install to standard paths:
   - VST3: `/Library/Audio/Plug-Ins/VST3/Luthier.vst3`.
   - AU: `/Library/Audio/Plug-Ins/Components/Luthier.component`.
   - Standalone: `/Applications/Luthier.app`.
   - Factory content: `/Library/Application Support/Luthier/`.
6. Register Launch Services file associations for
   `.luthierpreset`, `.luthierguitar`, `.luthiertune`,
   `.luthierloop`, `.luthierset`, `.midprofile`.
7. Admin password only if writing to system paths.
8. Post-install: "Open Luthier" button.

### 2.2 Universal binary

One `.pkg` supports Intel and Apple Silicon. Standalone is fat; plugins
are fat.

### 2.3 Uninstall

Bundled script: `/Applications/Luthier/Uninstall.command`.
Removes every installed file. Preserves `~/Documents/Luthier/` by
default; option to purge.

## 3. Linux packaging

Formats:
- `.tar.gz` for manual and non-Debian distros.
- `.deb` for Debian, Ubuntu, derivatives.
- `.rpm` for Fedora, openSUSE (best-effort).

Naming: `Luthier-<version>-linux-x64.tar.gz`,
`luthier_<version>_amd64.deb`.

### 3.1 File layout

- VST3: `/usr/lib/vst3/Luthier.vst3/` or `~/.vst3/Luthier.vst3/`.
- Standalone: `/usr/bin/luthier` or `~/.local/bin/luthier`.
- Factory content: `/usr/share/luthier/` or `~/.local/share/luthier/`.
- Desktop file: `/usr/share/applications/luthier.desktop`.
- MIME types: `/usr/share/mime/packages/luthier.xml` for the six file
  associations.
- Icon: standard icon-theme paths.

### 3.2 Install

`.deb`: standard `apt install ./luthier_<version>_amd64.deb`.
Post-install script updates icon cache, desktop database and MIME
database.

`.tar.gz`: extract, run `install.sh` which offers user or system
install. Uninstall via `uninstall.sh`.

### 3.3 Dependencies

Documented in the package: JACK or PipeWire, ALSA, X11 or Wayland.
No bundled shared libraries beyond JUCE's static-linked pieces.

## 4. Standalone-only distribution

Every platform ships a "Standalone only" bundle for users who do not
use a DAW.

## 5. Update delivery

Full installers always available. Delta packages where practical
(updates-telemetry.md 2).

### 5.1 Update check

Per updates-telemetry.md. On finding an update:
- Header banner shows "Version X.Y.Z available".
- Click opens release notes in browser.
- "Download" button fetches the platform-appropriate installer to
  Downloads.
- Plugin does not auto-launch installers.

### 5.2 Delta patches

- Under 50% of full installer size: offer both.
- One-way; cannot skip versions.
- SHA-256 verification against manifest.
- Failed apply rolls back and prompts full download.

## 6. First-run detection

After install, first plugin load creates:
- `~/Documents/Luthier/` folder tree per README.md.
- Subfolders: `Presets/User`, `Presets/Factory`, `Guitars/User`,
  `Guitars/Factory`, `Parts/User`, `Parts/Factory`, `Tunes/User`,
  `Tunes/Examples`, `IRs/`, `Loops/`, `Setlists/`, `Sessions/`,
  `Renders/`, `Captures/`, `Practice/`, `Diagnostics/`, `config/`,
  `ContentUpdates/`.
- User-global config: `~/Documents/Luthier/config/plugin.json`.
- Marker file: `~/Documents/Luthier/.installed_version`.

Absent marker = first run: trigger onboarding.md flow.
Marker version differs from running version = upgrade: show upgrade
banner and run migrations (section 8).

## 7. Enterprise deployment

- Managed installers accept command-line configuration.
- Policy file `luthier-policy.json` per updates-telemetry.md 7
  pre-placeable to enforce settings on first run.
- MSI wrapper available on request for Windows Group Policy.

## 8. Preset and guitar migrations

Between minor versions, formats are forward-compatible.

Between major versions, the plugin migrates on load:
- Old `.luthierpreset` without a `ranges` block: adds a stock-only
  ranges block on load, writes back on save, moves the original to
  `~/Documents/Luthier/Presets/Backup/<yyyy-mm-dd>/`.
- Old hard-coded guitar reference (pre-parts model): resolves through
  `Resources/Guitars/migration.json` to a shipped `.luthierguitar` per
  ambiguity-resolutions.md 7. Same backup rule.
- Old `.luthierloop` without event-class tags: loads with default tag
  values; new tags reconstructed on next record.
- Old `.mid` without the Luthier chunk: loads as Generic profile; user
  is not surprised.

User sees a subtle info banner on the first affected load.

## 9. Portable install (Windows only)

`.zip` that unpacks to any folder. No registry entries, no start menu,
no auto-update. Useful for USB drives and locked-down machines.

Portable install does not install VST3 to the system path; user copies
manually or points the DAW at the portable folder.

## 10. Verification

Every release publishes:
- SHA-256 checksums for every installer.
- PGP-signed manifest listing all installers and checksums.
- Canonical URL for automated verification.

## 11. Content updates without full install

Factory presets, guitars, tunes, parts, IR additions, translation
catalog updates, and the guitar-migration table can ship as a
`.luthiercontent` package smaller than a full installer:

- Signed `.luthiercontent` file.
- User drags onto the plugin or opens via Options -> Updates.
- Applied to `~/Documents/Luthier/ContentUpdates/<name>/`; plugin
  picks up on next load.

Content updates never modify code; only data.

## 12. Rollback plan

If a released version turns out broken:
1. Update manifest reverts to previous version within 30 minutes.
2. Installers for previous version remain at original URLs.
3. Users on the broken version downgrade via previous installer;
   installer detects newer install and prompts.
4. Rollback communicated via header banner in the running plugin.

## 13. Tests

- Install / uninstall cycle on every platform: installed files match
  manifest, uninstall removes exactly those.
- Upgrade: install A, install B, no A artefacts remain.
- Downgrade: install B, install A, clean downgrade.
- Enterprise silent install with policy: policy takes effect on first
  run.
- Signature verification: every published installer verifies against
  the PGP manifest.
- Portable Windows: unzip to arbitrary path, run, no writes outside
  the portable folder.
- File association: double-clicking `.luthierpreset`,
  `.luthierguitar`, `.luthiertune` and the others in the file manager
  launches Luthier and loads the file.
- Migration: 200 fixture presets from the pre-parts-model build load
  cleanly, back up correctly, and render within -60 dBFS RMS null of
  the golden.
