# Handoff: Pedalboard v0.1.0 release build

Written 2026-10-25. Branch `claude/peaceful-carson-vzl4e4`, head `1c9ae70b`.

## Goal

A finished pedalboard version with every build in one **draft** GitHub Release:
CLAP, VST3 and Standalone on Linux, Windows and macOS (AU on macOS), Pro and
Free editions, built by `.github/workflows/release.yml`.

Standing instructions from the user were: work silently, no notifications,
never publish the draft, be quick and efficient.

**Work was stopped by the user on 2026-10-25.** The 5-hour routine
(`trig_01CVERr6qMDvBr2AJw3ibuwg`) is now **disabled** (re-enable with
`update_trigger enabled=true`), and release run #19 (`40186045042`) was
cancelled. Nothing is running in the background.

## What the branch is

`origin/codex/luthier-pedalboard-completion` (the most complete pedalboard
branch) plus 64 cherry-picked commits that were unique to the six sibling
branches (`state`, `ui-full`, `engine`, `dsp`, `timefx`, `host-midi`). A plain
merge conflicts on 25 files because `completion` copied much of that work in
under different SHAs, so the cherry-pick was done by `git cherry` patch-id, in
commit-date order.

Six commits conflicted and were **skipped** (mostly duplicates of work that
arrived through another branch; not individually confirmed):
`11f30424`, `5a716ec7`, `8ecf6a5a`, `6fcd9b90`, `6d9ce297`, `c65a4a69`.

Also applied: `efa7d27d` (extra isolation in the SUS-08 decay test).

The local branch is `integ-try`. Push with
`git push origin integ-try:claude/peaceful-carson-vzl4e4`.

About a dozen of the cherry-picked commits are explicitly WIP (unregistered
TS808 / DS-1 / SD-1 / RAT / Distortion Plus circuit candidates, with recorded
failed acceptance gates). They are not registered in `PedalRegistry`, but their
sources are compiled into the shared library. See "Windows" below.

## Fixes made on top of the cherry-picks

| Area | Change |
|---|---|
| `PluginBusTests` | expects the 3 appended pedalboard buses (Pedal B, Wet only, Dry only) |
| `ParameterVisibility.h` | `ref_*` lanes are exempt from the main-editor reachability walk and from search; their controls are in the Pedalboard window |
| Jam / MidiOutPanel / ModelGaps / RoutingSweep / TuneProcessor test harnesses | process through one `MidiBuffer` reserved with `reserveMidiOutputBuffer()`; the bounded MIDI-out path writes nothing when the capacity is unknown |
| `RefPedalApiTests` facade test | expects the 50 ms tempo glide instead of an instant jump |
| `ParamMeta` | new `ParamUnit::bpm` |
| `QualityModeTests` CQ03 | `ref_board_cpu_cap` is the Pedalboard's own cap |
| `Source/ReferencePedals/Models/*.cpp` | user-visible "Inspired by" strings no longer contain brand names (Boss, MXR, Ibanez); model ids unchanged |
| `scripts/ci_build.sh`, `ci_build.ps1` | on a failed build, print compiler/linker errors as one `::error` annotation |

## CI status: run #19 (`40186045042`), commit `1c9ae70b`

Linux jobs were still running at handoff. Windows and macOS had already failed
in "Configure and build":

- **macOS (Pro and Free)** *(fix written, not yet built on macOS)*:
  `Source/Tests/RefPedalOversamplingOracleTests.cpp:20:37: error: no member named 'cyl_bessel_i' in namespace 'std'`.
  libc++ has no `std::cyl_bessel_i`. Done: the test now uses a local
  power-series `besselI0` (checked: I0(6.5) = 106.29). That was the only use of
  a missing special function in `Source`.
- **Windows (Pro and Free)** *(fix written, not yet built on Windows)*:
  `LNK1248: image size (0x10018DF91) exceeds maximum allowable size (0xFFFFFFFF)`
  on `Luthier Pro_SharedCode.lib` (Free: `0x1_004734D4`). The static library
  is about 1.6 MB over 4 GiB, with LTO on (`/GL` objects are large). Options,
  cheapest first:
  1. Stop compiling the unregistered WIP candidate models into the shared
     library (they are unregistered, so nothing uses them). Candidates:
     `ibanez_ts808.cpp`, `mxr_distortion_plus.cpp`, and any other model
     under `Source/ReferencePedals/Models/` that `PedalRegistry::registerAll()`
     does not reference. Check how the sources are gathered (the `Luthier`
     target's source list around `CMakeLists.txt:398`) before removing.
  2. Build the Windows release with `LUTHIER_LTO=OFF` (loses optimisation).
  3. Split the library (larger change).
  The margin is small, so option 1 may need to be repeated for a couple of
  more files, or combined with 2.
  What was actually done (a different, simpler variant): `CMakeLists.txt` now
  compiles `Source/ReferencePedals/**` and `Source/UI/ReferencePedals/**` with
  `/GL-` on MSVC when LTO is on, so the Pedalboard (the only code added since
  `master`, which linked) is ordinary machine code instead of bulky `/GL`
  intermediate code. CMake puts per-source options after target options
  (verified), so `/GL-` overrides `/GL`. **Unverified on MSVC**: if LNK1248
  persists, fall back to option 1 or 2 above.
- **Linux:** the earlier run #18 showed 5 failures on both editions (now fixed
  and verified locally) plus one on Pro only:
  `Combo.releasedStringIsDampedQuickly` reads 34.7 dB against a 35 dB bar for
  P-Style Bass note 42. It still fails after `efa7d27d`. Whether this is a real
  regression from the integration or a marginal reading is **not yet known**:
  a `master` (`0f62c3d6`) baseline build is at `/home/user/lbase`, build
  script `.../scratchpad/basebuild.sh`, output `.../scratchpad/base.log`
  (it was at step ~170 of 586). If master reads >= 35 dB, bisect the audio
  path; if master also reads about 34.7, the test margin (CI allows
  `slowCiHost` -31 dB on macOS, -35 dB elsewhere) is the issue and a small,
  commented tolerance is reasonable. Do not skip the test.

## Next steps

1. Read the Linux results of run #19 (annotations, below), plus Windows/macOS
   if re-run.
2. Fix the macOS Bessel call and the Windows 4 GiB library; keep the Linux
   decay decision based on the master baseline.
3. Push, dispatch `release.yml` again (tag `v0.1.0`, ref = the branch,
   platforms `linux,windows,macos`, editions `pro,free`).
4. Repeat until the run is green. Then leave the **draft** release alone; do
   not publish it.
5. Before declaring done, run the full local Pro suite once
   (`LuthierTests` with no filter, about 1.5 h); the one full local run done
   earlier did not list every failure that CI later found.

## Decisions the user should know about

- Brand names in the pedals' "Inspired by" text were made generic to satisfy
  `Trademarks.sourceTreeHasNoUnmarkedBrandNames`. If brand attribution is
  wanted, the trademark rule needs a deliberate exception.
- `ref_*` parameters are exempt from the main-editor reachability and search
  tests; they are reachable only in the Pedalboard window.
- The tag is `v0.1.0` (matches `project(Luthier VERSION 0.1.0)`). Existing tags
  are `v0.1.0-alpha1` and `v0.1.0-macos-beta1`.
- No PR has been opened. GitHub refused one at first because the branch had no
  commits ahead of `master` (`0f62c3d6`); it is now well ahead, so a draft PR
  can be opened if wanted.

## How to read CI (important)

The sandbox network policy blocks
`productionresultssa10.blob.core.windows.net`, so CI **logs and artifacts
cannot be downloaded**. Check-run annotations are readable:

```
gh api repos/djshellshoxxx/luthier/actions/runs/<RUN>/jobs --jq '.jobs[] | "\(.id) \(.name) \(.conclusion)"'
gh api repos/djshellshoxxx/luthier/check-runs/<JOB_ID>/annotations --jq '.[] | .message'
```

Failed builds now print their compiler errors as an annotation (see the
`scripts/ci_build.*` change). Failed unit tests show up as annotations with the
surrounding log lines. To read the raw logs instead, the user can add the host
under Allowed domains in the environment's network settings.

## Local build and test

Toolchain is installed (clang, ninja, cmake, xvfb, X11/ALSA dev packages).
JUCE and clap-juce-extensions are in `ThirdParty/` (fetched by
`scripts/ci_build.sh deps`). Build tree: `build-ci/` (Release, LTO off, Pro).

```
cmake --build build-ci --target LuthierTests -j4
cd build-ci/LuthierTests_artefacts/Release
xvfb-run -a ./LuthierTests <suite-or-test-name-substring>   # matches suite OR test name
```

A full no-filter run takes about 1.5 h (the `Combo` suite alone exceeds 30 min).
Only the Pro edition has been built locally; Free-only failures show up in CI.

`docs/review/QA_ROBUSTNESS.md` is rewritten by every test run. It is generated;
revert it with `git checkout docs/review/QA_ROBUSTNESS.md` rather than commit it
(the stop hook complains about uncommitted changes otherwise).

Background scripts and logs live in the session scratchpad
`/tmp/claude-0/-home-user-luthier/ee86c3f8-549c-521b-b917-bc8d93d2de59/scratchpad/`
(`native.log` first full run, `verify.log`, `base.log`). The scratch worktree
`/home/user/lbase` can be removed with `git worktree remove --force /home/user/lbase`.
