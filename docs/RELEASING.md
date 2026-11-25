# Releasing Luthier

How a build becomes signed installers on a draft GitHub Release, and how
to run every step on your own machine.

## 1. What CI does

| Workflow | When | What |
|---|---|---|
| `.github/workflows/build.yml` | every push and pull request | Linux (clang), Windows (MSVC), macOS (universal arm64 + x86_64): build the VST3, CLAP, Standalone (and AU on macOS), `LuthierTests` and `LuthierRender`; run the unit tests (under xvfb on Linux); pluginval at **strictness 5** on the VST3 (and the AU, plus `auval`, on macOS); clap-validator on the CLAP; upload the plugins and the logs as artifacts |
| same | nightly 03:17 UTC, and **Run workflow** (workflow_dispatch) | Same, with LTO on and pluginval at **strictness 10** (dispatch lets you pick) |
| `.github/workflows/release.yml` | a tag `v*` or candidate branch `release/v*` pushed, or dispatch with a tag | Freezes one source SHA, checks the tag matches `project(VERSION)` in `CMakeLists.txt`, and builds/tests/packages with LTO and strictness 10. The beta defaults to Linux/Windows/macOS, both editions, four unit-test shards per build, and the CLAP plug-in built, validated and packaged on every platform. Only a fully successful matrix creates the source tag and a **draft** GitHub Release with checksums and provenance. Only the authorized `release/v0.1.0-beta2` push and the authorized beta3 dispatch (on `claude/determined-ptolemy-mlbclp`, tag `v0.1.0-beta3`) additionally verify and publish their own prerelease; only beta2 then consolidates its source into unchanged `master`. Other invocations, including a `release/v0.1.0-beta3` push, retain draft review |

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

Both editions build from the same tree. `LUTHIER_EDITION=PAID` (Luthier Pro, the
default) or `FREE` (Luthier Free) selects the edition for `ci_build.sh` **and** the
packaging scripts, which must be run with the same value as `stage`: they find the
staged files by the product name ("Luthier Pro", "Luthier Free") and name their
outputs after the edition. CI sets it per matrix job.

Windows, from a *Developer PowerShell for VS 2022*:

```powershell
scripts/ci_build.ps1                            # every step
scripts/ci_build.ps1 -Step configure,build -Jobs 2   # a 4 GB machine: 2 jobs
scripts/ci_build.ps1 -Step validate -Strictness 10
scripts/package_windows.ps1                     # after "stage": Setup .exe and portable zip
scripts/ci_build.ps1 -Edition FREE ...          # LUTHIER_EDITION / -Edition PAID|FREE, as above
```

The build tree is `build-ci/` (so it never collides with the developer
tree `build/` from `scripts/setup_linux.sh` or `scripts/build.ps1`), the
staged products go to `dist/<platform>/`, installers to `dist/installers/`.

Pinned tool versions (change them in one place, the CI scripts):

| Tool | Version | Where |
|---|---|---|
| JUCE | 8.0.10 | `JUCE_TAG` in `scripts/ci_build.sh`, `$JuceTag` in `scripts/ci_build.ps1`, `scripts/setup_linux.sh` |
| clap-juce-extensions | commit `6bde1083` | `CLAP_JUCE_EXT_COMMIT` / `$ClapJuceExtCommit` |
| pluginval | v1.0.4 | `PLUGINVAL_VERSION` |
| clap-validator | 0.4.1 (`0.4.1-127-g152b982`) | `CLAP_VALIDATOR_VERSION` / pinned binary build in CI scripts, each download verified against its SHA-256 (`CLAP_VALIDATOR_SHA256_LINUX`/`_MACOS`, `$ClapValidatorWindowsSha256`) before it is unpacked; a new build needs new digests. Development builds and the beta3 release qualification |

## 3. What gets installed where

The plugin bundles no longer carry the 30 MB of factory content each:
one shared copy is installed, and `IrLibrary::searchForResources` looks
there. A developer build still copies the content into each bundle, so
nothing changes when running from the build tree.

| | Windows | macOS | Linux (per user / system / .deb) |
|---|---|---|---|
| VST3 | `C:\Program Files\Common Files\VST3\Luthier Pro.vst3` | `/Library/Audio/Plug-Ins/VST3/Luthier Pro.vst3` | `~/.vst3` / `/usr/local/lib/vst3` / `/usr/lib/vst3` |
| CLAP | `C:\Program Files\Common Files\CLAP\Luthier Pro.clap` | `/Library/Audio/Plug-Ins/CLAP/Luthier Pro.clap` | `~/.clap` / `/usr/lib/clap` / `/usr/lib/clap` |
| AU | | `/Library/Audio/Plug-Ins/Components/Luthier Pro.component` | |
| Standalone | `C:\Program Files\Luthier Pro\Luthier Pro.exe` | `/Applications/Luthier Pro.app` | `~/.local/bin/luthier` / `/usr/local/bin` / `/usr/bin` |
| Renderer | `...\Luthier\luthier-render.exe` | (not in the .pkg yet) | `luthier-render` beside the standalone |
| Content | `C:\ProgramData\Luthier\Resources` | `/Library/Application Support/Luthier/Resources` | `~/.local/share/luthier/Resources` / `/usr/local/share/...` / `/usr/share/...` |
| Uninstall | Add/Remove Programs | `/Applications/Luthier Pro/Uninstall.command` | `<content dir>/uninstall.sh` / `apt remove luthier-pro` |

The table shows Luthier Pro. Luthier Free installs the same layout with
`Luthier Free` in place of `Luthier Pro` (and `luthier-free` for the packages),
with its own Windows AppId and macOS package IDs. The two editions share the
factory content folder (`IrLibrary` does not search an edition-specific one yet)
and, on Linux, the `luthier` binary and desktop entry, so on Linux the two
editions replace each other rather than sit side by side (the packages declare
`Conflicts:`).

User data (`~/Documents/Luthier`) is never touched by an installer; the
uninstallers remove it only when the user explicitly asks.

Outputs:

Each edition produces its own set, named with `<Edition>` = `Pro` or `Free`, so
one tag's draft release carries both without a collision:

- `Luthier-<Edition>-<v>-Setup-win64.exe`, `Luthier-<Edition>-<v>-portable-win64.zip`
- `Luthier-<Edition>-<v>-macOS.dmg` (holding `Luthier-<Edition>-<v>.pkg`)
- `Luthier-<Edition>-<v>-linux-x64.tar.gz`, `Luthier-<Edition>-<v>-standalone-linux-x64.tar.gz`,
  `luthier-pro_<v>_amd64.deb` / `luthier-free_<v>_amd64.deb`, and the best-effort
  `luthier-pro-<v>-1.x86_64.rpm` / `luthier-free-<v>-1.x86_64.rpm` (+ `.asc` signatures).
  No `.rpm` is built while `<v>` carries a pre-release suffix such as `-beta3`:
  RPM rejects `-` in Version.
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

To build a draft for an existing tag that has no release yet, use *Actions ->
release -> Run workflow* and enter the tag. Existing drafts and releases are
never overwritten; a correction after drafting requires a new version.

### Creating a new candidate without a manual dispatch

Push the qualified source commit to a new branch named `release/v<version>`,
for example `release/v0.1.0-beta2`. This starts the same release workflow.
The workflow freezes that commit, verifies the version, builds all selected
editions/platforms with LTO and strictness 10, runs the full tests and validators,
and packages the installers. Only after all jobs pass does it create the new
tag and draft release. Ordinary development branches do not trigger releases.

Existing releases and drafts are rejected before building and checked again
before drafting. Existing tags must identify exactly the tested source; they
are never moved. A corrected candidate needs a new version once its tag/release
exists. General releases still need the manual acceptance checks below before
public publication. The limited beta2 and beta3 exceptions are described next.

### Authorized beta publication

Only two successful runs publish automatically, each for its own tag:

- **beta2**: a push of `refs/heads/release/v0.1.0-beta2` for tag `v0.1.0-beta2`
  (qualified on Windows and macOS, without CLAP).
- **beta3**: a dispatch of `release.yml` on `claude/determined-ptolemy-mlbclp`
  with tag `v0.1.0-beta3` and ref `claude/determined-ptolemy-mlbclp` (qualified
  on Linux, Windows and macOS, with CLAP):

  ```bash
  gh workflow run release.yml --ref claude/determined-ptolemy-mlbclp \
      -f tag=v0.1.0-beta3 -f ref=claude/determined-ptolemy-mlbclp
  ```

  Leave the `platforms` and `editions` inputs at their defaults: provenance must
  list `linux,windows,macos` and `pro,free`. The version job refuses the run
  unless its checkout is the dispatched commit (`github.sha`), and the publisher
  requires the branch and the new tag to identify that commit before the
  download, before publishing and afterwards. Do not push
  `claude/determined-ptolemy-mlbclp` until the run has finished: a moved branch
  stops publication and leaves the draft.

Each retains its complete Free/Pro qualification: Windows installer coexistence,
all unit-test shards, plugin validators at strictness 10, LTO, and packaging.
Everything else stops at a draft: a push of `release/v0.1.0-beta3`, tag pushes,
dispatches from another branch or with another `tag` or `ref` input, and every
other candidate version.

After creating its new draft, `scripts/publish_qualified_beta.py` verifies the
candidate and release tag still identify the frozen source. It requires exactly
the asset set the candidate's platforms package: for Windows, the two Setup
installers and two portable ZIPs with their checksum sidecars; for macOS, the two
DMGs and the Pro universal ZIP; for Linux (beta3), each edition's
`Luthier-<Edition>-<v>-linux-x64.tar.gz`, `Luthier-<Edition>-<v>-standalone-linux-x64.tar.gz`
and `<pkg>_<v>_amd64.deb`; and provenance and the checksum manifest. The only
optional extras are each edition's best-effort `<pkg>-<v>-1.x86_64.rpm` (never
built for a pre-release version such as `0.1.0-beta3`) and one
complete set of detached signatures: when `GPG_PRIVATE_KEY` is configured,
`SHA256SUMS.txt.asc` and a `.asc` for every Linux package, otherwise none.
Provenance must name this source, version, CI run, both editions and exactly the
candidate's platforms in the order `release.yml` records them. Local checksums,
uploaded sizes and SHA-256 digests, and freshly downloaded bytes must all match.
It checks the draft and asset IDs again immediately before publishing that exact
release ID as a prerelease, then verifies the published state, assets and source.
No assets, tags, existing releases or notes are replaced.

A failed publication check stops the job without automatic retry or rollback.
Inspect its draft/release state before choosing the next action. A failure during
the final API request or post-publication verification can occur after the
prerelease became public; it must not be treated as permission to recreate it.
Hardware, DAW, listening and signing acceptance remain explicitly limited in the
beta2 and beta3 release notes and are not claimed by this automation.

After verified beta2 publication, the same candidate run fast-forwards `master` to
that exact source. `scripts/consolidate_qualified_beta.py` (beta2 only) requires the original
observed master (`0f62c3d67bdba055b41e66c7ab8ac9ab4b728662`), verifies ancestry, and
uses an atomic expected-head, non-forced ref update. An intervening master change stops consolidation
for review while retaining the published beta. Branch protection is never changed.

Beta3 does not consolidate `master`. Its dispatch publishes the verified
prerelease and leaves `master` unchanged; bringing that source into `master` is
a separate, reviewed change.

## 6. Manual checks before publishing (`installer.md` 13, `qa-polish.md`)

- Install, open the standalone, load the plugin in one DAW per format,
  play a factory preset: sound, and no "Resources not found" banner.
- Upgrade over the previous release: no old files left
  (`installer.md` 13).
- Uninstall: only the installed files go; `~/Documents/Luthier` stays.
- macOS: `spctl -a -vvv -t install Luthier-<Edition>-<v>.pkg` says *accepted,
  Notarized Developer ID*; `auval -v aumu Lthr Ltha` (Free: `Lthf`) passes.
- Windows: the installer's signature shows the company name; SmartScreen
  behaviour noted.
- Linux: `scripts/verify_release.sh <dir> [public-key.asc]` (checksums, every
  installer listed, `gpg --verify` on the `.asc` files).
- Linux install cycle: `scripts/test_install_linux.sh Luthier-<Edition>-<v>-linux-x64.tar.gz`
  (install, upgrade, downgrade, uninstall, purge, all in a throw-away HOME;
  with no argument it tests synthetic packages built from `packaging/linux`).

## 7. Before the first public release

- Replace `packaging/common/EULA.txt` (a placeholder) with the real
  licence.
- Obtain the certificates of section 4 and the Apple Developer account.
- Decide the editions split (`spec/editions.md`) and licensing
  (`spec/licensing.md`); both add a CI matrix axis and installer names.
- Build the Linux release on the oldest distribution you support: the
  `.deb`'s libc6 requirement is computed from the binary, and a build on
  `ubuntu-latest` needs that distribution's glibc.

## 8. Rollback and hotfix runbook (`installer.md` 12, `qa-polish.md` 13)

Target: a bad release is withdrawn in 30 minutes, a fixed one is out in 24 hours.

1. **Stop the spread (5 min).** Edit the update manifest so `latest` points at
   the previous good version (`scripts/release_manifest.sh` regenerates it from
   a release tag); running plug-ins then stop offering the bad build. Mark the
   GitHub release as *pre-release* or delete the draft. Old installers stay
   published: never delete a release people may need to go back to.
2. **Tell people (10 min).** Pin an issue with the symptom, the affected
   versions and the workaround. Users go back by installing the previous
   installer over the top: the Windows installer asks before replacing a newer
   version, the macOS `.pkg` and Linux `install.sh` replace it silently (Linux
   removes the old manifest first, so no file from the newer build survives).
   User data in `~/Documents/Luthier` is untouched by any of this.
3. **Fix (up to 24 h).** Branch from the last good tag, cherry-pick only the
   fix, bump the patch version, run the section 6 checks, tag, let `release.yml`
   build. Point the manifest at the hotfix once the checks pass.
4. **Monitor (72 h).** Watch the issue tracker and support inbox; a second
   report of the same crash reopens step 1.

## 9. Why the installers ask for what they ask for

- **Windows needs administrator rights.** The VST3 and CLAP folders live under
  `C:\Program Files\Common Files`, and the factory content under
  `C:\ProgramData\Luthier`, which every plug-in format finds there. The
  portable zip needs no rights but is copied by hand.
- **macOS needs an administrator password** for `/Library/Audio/Plug-Ins` and
  `/Library/Application Support/Luthier`.
- **Linux needs none by default:** `install.sh` installs for the current user
  (`~/.vst3`, `~/.clap`, `~/.local`); `--system` (root) installs under
  `/usr/local`. The `.deb` and `.rpm` install under `/usr`.
- **Cancel and rollback** during an install are Inno Setup's and Installer.app's
  own: cancelling removes what was copied so far.
- **Inno exit codes** (`Setup.exe /VERYSILENT`): 0 success; 1 Setup failed to
  initialise; 2 the user cancelled before installing; 3 a fatal error while
  preparing the install; 4 a fatal error during the install; 5 the user cancelled
  (or chose Abort) during the install; 6 Setup was force-terminated; 7 the
  *Preparing to Install* stage decided Setup cannot proceed; 8 the same, and a
  restart is needed first. The Inno Setup documentation has the current list.
- **Managed installs** can pre-place the policy file
  (`luthier-policy.json`, the path `Policy::getPolicyFile` reads):
  Windows `Luthier-<Edition>-<v>-Setup-win64.exe /VERYSILENT /POLICY=C:\path\luthier-policy.json`
  copies it to `C:\ProgramData\Luthier\`; on macOS put it at
  `/Library/Application Support/Luthier/luthier-policy.json`, on Linux at
  `/etc/luthier/luthier-policy.json`. A policy can only remove permissions
  (force telemetry off, name a private update mirror).
- **The content location is fixed by design** so every format finds one shared
  copy (`IrLibrary::getCandidateFolders`): `C:\ProgramData\Luthier\Resources`,
  `/Library/Application Support/Luthier/Resources`,
  `$XDG_DATA_HOME|~/.local/share/luthier/Resources`, `/usr/local/share/luthier`,
  `/usr/share/luthier`.
- **Standalone only:** the Linux `Luthier-<Edition>-<v>-standalone-linux-x64.tar.gz`
  installs the app and content without the plug-ins; on Windows and macOS untick
  the plug-in components in the installer.

## 10. Known gaps

- Delta updates (`installer.md` 5.2), the MSI wrapper
  (`installer.md` 7) and the "Refuses to run while a DAW has the plugin
  loaded" check beyond Inno Setup's Restart Manager are not built yet.
- Inno Setup's `/SILENT`, `/VERYSILENT` and `/DIR=` replace the
  NSIS-style `/S` and `/D=` named in `installer.md` 1.2.
- Determinism: the Linux archives are reproducible (sorted, fixed
  mtime and owner); the Windows and macOS installers embed signing
  timestamps and are not byte-identical between runs.
- Beta2 excluded CLAP because native validator runs exposed state round-trip
  defects. Beta3 builds, validates (clap-validator 0.4.1) and packages CLAP on
  every platform; older 0.3.2 pass/skip counts are historical and do not
  qualify a release.

## 11. Elevation, silent installs and enterprise policy

Why each installer asks for what it asks for (`installer.md` 0.4):

- **Windows** runs elevated (`PrivilegesRequired=admin` in `Luthier.iss`)
  because VST3 plug-ins must go to `C:\Program Files\Common Files\VST3`
  and shared content to `C:\ProgramData\Luthier`. Nothing else needs it.
- **macOS** asks for an administrator only for the system paths
  (`/Library/Audio/Plug-Ins`, `/Library/Application Support/Luthier`).
- **Linux** installs per user by default (`install.sh`); `--system` and the
  `.deb` need root.

Silent installs (`installer.md` 1.2) use Inno Setup's own switches, the
documented deviation from the NSIS names:

| Switch | Meaning |
|---|---|
| `/VERYSILENT /SUPPRESSMSGBOXES` | no UI, default answers |
| `/DIR="C:\Path"` | standalone application folder |
| `/COMPONENTS="vst3,clap,standalone"` | component subset |
| `/LOG="file.txt"` | write a setup log |

Exit codes are Inno Setup's: `0` success, `1` setup failed to initialise,
`2` cancelled before install began, `3` fatal error while preparing, `4` fatal
error during install (rolled back), `5` cancelled during install (rolled
back), `6` killed by the system, `7` a preparation step refused (for example
a file in use), `8` a restart is needed first. Inno's progress page offers
Cancel at every step and rolls a partial install back (`installer.md` 1.1.8).

**Policy file** (`updates-telemetry.md` 7, `installer.md` 7). An
administrator pre-places `luthier-policy.json` at the path the plugin reads
(`Policy::getPolicyFile`): `%ProgramData%\Luthier\luthier-policy.json` on
Windows, `/Library/Application Support/Luthier/luthier-policy.json` on macOS,
`/etc/luthier/luthier-policy.json` on Linux. A policy can only switch things
off:

```json
{ "allow_usage_telemetry": false, "allow_diagnostics_telemetry": false,
  "allow_crash_upload": false, "allow_update_check": false,
  "update_manifest_url": "https://mirror.example/manifest.json" }
```

Deploy it with the software-distribution tool alongside a silent install;
the installers do not take it as a switch yet.

## 12. Rollback and hotfix, second part (`qa-polish.md` 13, `installer.md` 12; continues section 8)

A release that has to come back:

1. **Revert the manifest first** (target: within 30 minutes of the call). Put
   the previous `latest_stable` back in the update manifest; the update check
   stops offering the bad build on its next daily check.
2. **Keep every published installer.** Old assets are never deleted from
   GitHub Releases, so the previous version is always downloadable; mark
   the bad release as a pre-release instead of deleting it.
3. **Tell installed users** through the release notes linked from the
   manifest's `changelog_url`. (An in-plugin rollback banner driven by a
   manifest field is not built yet.)
4. **Downgrading** is supported: the Windows installer detects the newer
   installed version and asks before replacing it; the macOS and Linux
   packages install over it. User data in `Documents/Luthier` is never
   touched by an install or uninstall.
5. **Hotfix path:** branch from the release tag, fix, bump the patch
   version, tag, and run `release.yml`; the same CI gates apply. The target
   is a hotfix within 24 hours of a confirmed ship-blocker.
6. **Monitoring** for 72 hours after a release (crash reports opted into,
   support inbox, community channels) is a process step owned by the
   release lead.

## 13. Release hazards

**The obsolete beta2 publisher is retired in the current release chain.**
`.github/workflows/pro-beta2-release.yml` now has only a manual dispatch trigger and
an unconditionally disabled preflight. It cannot publish the old candidate when an
ancestor PR becomes merged. This retirement was added by the November 16 release
continuation and is present in PR #105's source.

Older `master`/integration snapshots predate that change. Consolidate a source that
includes the retirement, and inspect the effective base-branch workflows before
merging. The current `release.yml` creates a draft only after the selected native
test and validator matrix succeeds. The authorized beta2 candidate push and beta3
dispatch have the additional verified-publication step described above; other drafts
require a separate release action. Source tags and existing release assets must not be moved or replaced.

Other things to keep in mind: the release workflows refuse to overwrite an existing
release (see `release.yml`), and publication of a public release
needs the checks in section 6 and the ledger in `docs/coverage/PRO-V01-RELEASE.md`.
