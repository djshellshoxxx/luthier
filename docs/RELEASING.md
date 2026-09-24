# Releasing Luthier

How a build becomes signed installers on a draft GitHub Release, and how
to run every step on your own machine.

## 1. What CI does

| Workflow | When | What |
|---|---|---|
| `.github/workflows/build.yml` | every push and pull request | Linux (clang), Windows (MSVC), macOS (universal arm64 + x86_64): build the VST3, CLAP, Standalone (and AU on macOS), `LuthierTests` and `LuthierRender`; run the unit tests (under xvfb on Linux); pluginval at **strictness 5** on the VST3 (and the AU, plus `auval`, on macOS); clap-validator on the CLAP; upload the plugins and the logs as artifacts |
| same | nightly 03:17 UTC, and **Run workflow** (workflow_dispatch) | Same, with LTO on and pluginval at **strictness 10** (dispatch lets you pick) |
| `.github/workflows/release.yml` | a tag `v*` pushed, or dispatch with a tag | Checks the tag matches `project(VERSION)` in `CMakeLists.txt`, runs `build.yml` at strictness 10 with LTO, builds the installers, writes `SHA256SUMS.txt` (signed if a GPG key is set) and creates a **draft** GitHub Release with everything attached. Nothing is published automatically |

Push and pull-request builds turn LTO off (it doubles the link time and
defeats the compiler cache); nightly and release builds turn it on, so
the validators always see a shipping-configuration build at least daily.

Compiler caches: ccache on Linux and macOS, sccache on Windows (through
`hendrikmuhs/ccache-action`), keyed per platform and LTO setting. JUCE and
clap-juce-extensions are cached by `actions/cache`, keyed on the CI
scripts, which hold the pinned versions.

Artifacts per run: `Luthier-<platform>-<sha>` (the staged plugins, app,
renderer and shared content) and `logs-<platform>-<sha>` (unit-test log,
pluginval logs, auval, clap-validator, CMake configure log). Kept 14 days.

## 2. Running CI locally

Every CI step is a script step; the workflows only call them.

Linux / macOS:

```bash
scripts/ci_build.sh                  # deps, configure, build, test, validate, stage
scripts/ci_build.sh build test       # just those steps
PLUGINVAL_STRICTNESS=10 LUTHIER_LTO=ON scripts/ci_build.sh configure build validate
SKIP_VALIDATORS=1 scripts/ci_build.sh    # offline
scripts/package_linux.sh             # after "stage": .tar.gz and .deb in dist/installers
scripts/package_macos.sh             # after "stage" on a Mac: .pkg in a .dmg
```

Windows, from a *Developer PowerShell for VS 2022*:

```powershell
scripts/ci_build.ps1                            # every step
scripts/ci_build.ps1 -Step configure,build -Jobs 2   # a 4 GB machine: 2 jobs
scripts/ci_build.ps1 -Step validate -Strictness 10
scripts/package_windows.ps1                     # after "stage": Setup .exe and portable zip
```

The build tree is `build-ci/` (so it never collides with the developer
tree `build/` from `scripts/setup_linux.sh` or `scripts/build.ps1`), the
staged products go to `dist/<platform>/`, installers to `dist/installers/`.

Pinned tool versions (change them in one place, the CI scripts):

| Tool | Version | Where |
|---|---|---|
| JUCE | 8.0.10 | `JUCE_TAG` in `scripts/ci_build.sh`, `$JuceTag` in `scripts/ci_build.ps1`, `scripts/setup_linux.sh` |
| clap-juce-extensions | commit `55525c98` | `CLAP_JUCE_EXT_COMMIT` / `$ClapJuceExtCommit` |
| pluginval | v1.0.4 | `PLUGINVAL_VERSION` |
| clap-validator | 0.3.2 | `CLAP_VALIDATOR_VERSION` |

## 3. What gets installed where

The plugin bundles no longer carry the 30 MB of factory content each:
one shared copy is installed, and `IrLibrary::searchForResources` looks
there. A developer build still copies the content into each bundle, so
nothing changes when running from the build tree.

| | Windows | macOS | Linux (per user / system / .deb) |
|---|---|---|---|
| VST3 | `C:\Program Files\Common Files\VST3\Luthier.vst3` | `/Library/Audio/Plug-Ins/VST3/Luthier.vst3` | `~/.vst3` / `/usr/local/lib/vst3` / `/usr/lib/vst3` |
| CLAP | `C:\Program Files\Common Files\CLAP\Luthier.clap` | `/Library/Audio/Plug-Ins/CLAP/Luthier.clap` | `~/.clap` / `/usr/lib/clap` / `/usr/lib/clap` |
| AU | | `/Library/Audio/Plug-Ins/Components/Luthier.component` | |
| Standalone | `C:\Program Files\Luthier\Luthier.exe` | `/Applications/Luthier.app` | `~/.local/bin/luthier` / `/usr/local/bin` / `/usr/bin` |
| Renderer | `...\Luthier\luthier-render.exe` | (not in the .pkg yet) | `luthier-render` beside the standalone |
| Content | `C:\ProgramData\Luthier\Resources` | `/Library/Application Support/Luthier/Resources` | `~/.local/share/luthier/Resources` / `/usr/local/share/...` / `/usr/share/...` |
| Uninstall | Add/Remove Programs | `/Applications/Luthier/Uninstall.command` | `<content dir>/uninstall.sh` / `apt remove luthier` |

User data (`~/Documents/Luthier`) is never touched by an installer; the
uninstallers remove it only when the user explicitly asks.

Outputs:

- `Luthier-<v>-Setup-win64.exe`, `Luthier-<v>-portable-win64.zip`
- `Luthier-<v>-macOS.dmg` (holding `Luthier-<v>.pkg`)
- `Luthier-<v>-linux-x64.tar.gz`, `luthier_<v>_amd64.deb` (+ `.asc` signatures)
- `SHA256SUMS.txt` (+ `SHA256SUMS.txt.asc`)

## 4. Signing secrets

Add them under *Settings -> Secrets and variables -> Actions*. Every one
is optional: without it the step builds unsigned output and prints a
warning, so test releases work on a fork with no secrets at all.

| Secret | Used for |
|---|---|
| `WINDOWS_CERT_PFX_BASE64`, `WINDOWS_CERT_PASSWORD` | Authenticode signing of the plug-ins, the app, the installer and its uninstaller (`signtool`, SHA-256, RFC 3161 timestamp) |
| `MACOS_CERT_P12_BASE64`, `MACOS_CERT_PASSWORD` | A .p12 holding both the *Developer ID Application* and *Developer ID Installer* identities |
| `MACOS_DEV_ID_APP` | e.g. `Developer ID Application: Luthier Audio (ABCDE12345)` |
| `MACOS_DEV_ID_INSTALLER` | e.g. `Developer ID Installer: Luthier Audio (ABCDE12345)` |
| `APPLE_ID`, `APPLE_APP_PASSWORD`, `APPLE_TEAM_ID` | `notarytool` (an app-specific password) |
| `GPG_PRIVATE_KEY`, `GPG_PASSPHRASE` | Detached signatures of the Linux packages and of `SHA256SUMS.txt` (`installer.md` 0.2, 10) |

Windows note: certificates issued since June 2023 live on a hardware
token or a cloud HSM and cannot be exported as a .pfx. With Azure
Trusted Signing, DigiCert KeyLocker or SSL.com eSigner, replace the
body of `Invoke-Sign` in `scripts/package_windows.ps1` and the
`/Ssigntool=` line with the provider's signing command; nothing else
changes.

macOS note: the bundles are signed with the hardened runtime
(`codesign --options runtime`); the standalone gets the audio-input
entitlement (`packaging/macos/entitlements.plist`) for the sidechain.

## 5. Cutting a release

1. Make sure the integration branch is green on the latest nightly
   (strictness 10).
2. Bump `project(Luthier VERSION x.y.z)` in `CMakeLists.txt`, update
   `docs/CHANGELOG.md`, commit.
3. Tag and push: `git tag -a v1.2.0 -m "Luthier 1.2.0" && git push origin v1.2.0`.
   A pre-release suffix is allowed (`v1.2.0-beta1`) and marks the draft
   as a pre-release.
4. Wait for **release** to finish. It fails fast if the tag and the CMake
   version disagree.
5. Open the draft release: check the six installers and the checksums,
   install each one on a clean machine (section 6), edit the notes,
   **Publish**.
6. Update the update-check manifest (`updates-telemetry.md` 1) to point
   at the new downloads.

To rebuild a draft for an existing tag (after fixing a signing secret,
say): *Actions -> release -> Run workflow*, enter the tag.

## 6. Manual checks before publishing (`installer.md` 13, `qa-polish.md`)

- Install, open the standalone, load the plugin in one DAW per format,
  play a factory preset: sound, and no "Resources not found" banner.
- Upgrade over the previous release: no old files left
  (`installer.md` 13).
- Uninstall: only the installed files go; `~/Documents/Luthier` stays.
- macOS: `spctl -a -vvv -t install Luthier-<v>.pkg` says *accepted,
  Notarized Developer ID*; `auval -v aumu Lthr Ltha` passes.
- Windows: the installer's signature shows the company name; SmartScreen
  behaviour noted.
- Linux: `sha256sum -c SHA256SUMS.txt`, `gpg --verify` on the `.asc` files.

## 7. Before the first public release

- Replace `packaging/common/EULA.txt` (a placeholder) with the real
  licence.
- Obtain the certificates of section 4 and the Apple Developer account.
- Decide the editions split (`spec/editions.md`) and licensing
  (`spec/licensing.md`); both add a CI matrix axis and installer names.
- Build the Linux release on the oldest distribution you support: the
  `.deb`'s libc6 requirement is computed from the binary, and a build on
  `ubuntu-latest` needs that distribution's glibc.

## 8. Known gaps

- Delta updates (`installer.md` 5.2), `.rpm` packages, the MSI wrapper
  (`installer.md` 7) and the "Refuses to run while a DAW has the plugin
  loaded" check beyond Inno Setup's Restart Manager are not built yet.
- Inno Setup's `/SILENT`, `/VERYSILENT` and `/DIR=` replace the
  NSIS-style `/S` and `/D=` named in `installer.md` 1.2.
- Determinism: the Linux archives are reproducible (sorted, fixed
  mtime and owner); the Windows and macOS installers embed signing
  timestamps and are not byte-identical between runs.
- clap-validator 0.3.2 skips 3 of its 21 tests: the three preset-discovery
  tests, because the plugin does not implement CLAP's (draft)
  preset-discovery factory. The other 18 pass.
