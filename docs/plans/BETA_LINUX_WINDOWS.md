# Luthier Linux and Windows Beta Plan

**Planning baseline:** 2026-09-28, integration branch `claude/luthier-cloud-session-5lzlix`. This is a beta of the existing full instrument, before Free/Pro editions, licensing, new instrument expansion, and macOS. The exact commit shipped must be recorded at release time.

## Beta promise

A tester can install the instrument, load a guitar or bass preset, play it in a supported host or standalone, change the main sound controls, save a project, reopen it with the same sound, and uninstall or replace the beta without losing user data. All visible beta features must have a working path; hide or label unfinished extras.

**Targets:** Linux x86-64 VST3, CLAP, standalone; Windows x64 VST3 and standalone. Windows CLAP is optional if its validator and host smoke tests pass. No installer is required for this beta. Ship versioned archives with checksums and exact copy instructions. Do not call an untested archive a beta build.

## Critical path

### 1. Freeze a coherent beta scope

- [ ] Choose a tested integration commit and create a beta release branch. Keep feature expansion on other branches. Include only features whose UI, state and audio path work end to end.
- [ ] Review the current beta report and open draft PRs. Merge only required fixes after Linux build and focused tests; do not make ASCII TAB, new instruments, licensing, or edition splitting prerequisites.
- [ ] Resolve the visible first-hour gaps: accessible preset selection, audible factory sounds, no dead controls, clear first-run flow, a short Help page, and known limitations. If a feature cannot meet that bar, remove its beta entry point or label it explicitly experimental.
- [ ] Assign a stable beta version and preserve plugin identity and parameter IDs so projects made in this beta can reopen in the next one.

**Exit:** The feature list is frozen, and one person unfamiliar with Luthier can play and save a useful sound without developer guidance.

### 2. Remove beta blockers

- [ ] Fix and reproduce the two callback-lock paths in `docs/review/CODEX_RTSAFETY.md`: live body part swaps and active rhythm humanization. Address first-use scratch allocation and dense MIDI growth where exercised in beta. Do not treat a static review as a passing runtime test.
- [ ] Triage the remaining open `B-*` findings in `docs/audit/BETA_TEST_REPORT.md`. Block beta on crashes, hangs, audio dropouts, stuck notes, broken reset/stop, corrupt state, lost host automation, missing primary controls, or clearly wrong factory sounds. Document low-impact limitations for testers.
- [ ] Compile and run the draft coverage and robustness tests in PRs #4 and #5; repair actual failures without weakening their oracles. Review the amp layout in PR #8 at 1200 x 720 in a real editor window.
- [ ] Restore a functioning build route. GitHub Actions is reported blocked by the account spending limit; use an authorized local Linux and Windows build if that is not resolved. Record logs and the commit SHA for both.

**Exit:** No known P0/P1 defect in the beta path; focused regression tests and the full Linux suite pass on the release candidate.

### 3. Produce and validate Linux artifacts

Use the repository scripts on a clean supported Linux x86-64 build machine:

```bash
scripts/ci_build.sh deps configure build test validate stage
scripts/package_linux.sh
```

Set `PLUGINVAL_STRICTNESS=10` for the release candidate. Keep the full logs and check the exact output paths. The existing packaging script produces a versioned `.tar.gz` with `install.sh`/`uninstall.sh` and a `.deb`; the tarball is enough for the fastest manual beta, while the `.deb` can follow if its clean install/uninstall test passes.

- [ ] Run the complete suite, focused new QA suites, pluginval on VST3 and clap-validator on CLAP. A validator pass is necessary but does not replace an actual host test.
- [ ] On a clean Linux machine, install from the archive, scan in at least one Linux DAW, play MIDI, save/reopen a session, use standalone, and remove/upgrade without deleting user content.
- [ ] Confirm the VST3 bundle and factory resources are present and discoverable after installation. Check the minimum supported glibc and linked libraries on the binary that will be distributed.

**Exit:** One immutable Linux archive and logs tied to its SHA-256 checksum pass all checks.

### 4. Produce and validate Windows artifacts

Use Developer PowerShell for VS 2022 on a Windows x64 build machine. The repository's existing script already has the required stages:

```powershell
scripts/ci_build.ps1 -Step deps,configure,build,test,validate,stage -Jobs 2 -Strictness 10
```

The script builds VST3, standalone, tests, the render CLI, and CLAP if its pinned dependency is present. If a larger build runner is used, its default job count may be appropriate; on a 4 GB machine keep `-Jobs 2` or lower. Capture test and validator logs. Do not rely on Linux results to claim Windows works.

- [ ] Inspect `dist/windows` after staging. Package the entire `Luthier.vst3` directory, `Luthier.exe`, and the required `Resources` tree into a versioned ZIP. Preserve their relative layout. The current `package_windows.ps1` makes an installer before its portable ZIP; add and test a `-PortableOnly` path, or use an equivalent small staging script that does not invoke Inno Setup.
- [ ] On a clean Windows test machine, copy the complete `Luthier.vst3` folder to `C:\Program Files\Common Files\VST3\` (or a tested per-user VST3 path). Keep the standalone EXE with its adjacent Resources. Rescan plugins in FL Studio.
- [ ] Test FL Studio and a second VST3 host: discovery as an instrument, MIDI sound, GUI resize, preset changes while playing, automation, project save/reopen, offline render, and host shutdown/relaunch. Test standalone audio/MIDI selection separately.
- [ ] Replace the ZIP with a newer beta on a clean and an existing installation. Confirm no duplicate plugin ID, missing content, or lost user presets. Record the actual host/version/Windows version and results.

**Exit:** The exact Windows ZIP passes unit tests, pluginval, and clean-machine FL Studio plus second-host smoke tests.

### 5. Publish to a small community group

- [ ] Freeze the release commit and build Linux and Windows from that same commit. Record compiler, JUCE and dependency versions, checksums, test logs, validator logs and host matrix in a beta manifest.
- [ ] Put `README-BETA.md` in each archive: supported OS/hosts, install and removal steps, version, a five-minute first-play walkthrough, known limitations, where user data is stored, and a bug-report link/template. Provide a fallback previous archive.
- [ ] Start with a small invited group; ask each tester to try a fresh install, first sound, one preset edit, save/reopen, and one longer session. Collect reproducible issues with host/version, OS, audio interface, steps, logs and affected project/preset if shareable.
- [ ] Widen access only after the first group reports no release-blocking failures.

**Release decision:** both platform archives pass the same core behavior, no known crash/audio-thread stall or state-loss defect remains in the advertised path, and the exact downloadable files have clean install and host evidence.

## Windows DLL question

The VST3 binary on Windows is DLL code **inside** a `.vst3` bundle. Steinberg's current bundle structure is `Luthier.vst3/Contents/x86_64-win/Luthier.vst3` plus `Contents/Resources`. Its older single-file `.vst3` DLL layout is deprecated. A bare `.dll` would normally mean building a separate VST2 plugin, which is not this project's current target. A ZIP of the complete VST3 bundle needs no installer, though a tester may need permission to copy it into the system VST3 folder. The standalone `Luthier.exe` can be portable with its resources next to it.

## Repo references

- `docs/HANDOFF.md`, `docs/COORDINATOR_PLAN.md`, `docs/audit/BETA_TEST_REPORT.md`, `docs/review/CODEX_RTSAFETY.md`
- `scripts/ci_build.sh`, `scripts/ci_build.ps1`, `scripts/package_linux.sh`, `scripts/package_windows.ps1`, `.github/workflows/build.yml`
- Steinberg VST3 format: https://steinbergmedia.github.io/vst3_dev_portal/pages/Technical+Documentation/Locations+Format/Plugin+Format.html
- Steinberg Windows location: https://helpcenter.steinberg.de/hc/en-us/articles/115000177084-VST-plug-in-locations-on-Windows
- Image-Line plugin scanning: https://www.image-line.com/fl-studio-learning/fl-studio-online-manual/html/basics_externalplugins.htm
