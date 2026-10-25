# Pedalboard v0.1.0: status report

Date: 2026-10-25. Branch `claude/peaceful-carson-vzl4e4`, head `fca497b2`.
Work was stopped at the user's request; nothing is running.

**How to read the percentages.** They are my estimates, not measurements. Where
a number comes from data (test counts, job results, file counts) it says so.
Where it is a judgment, it says "estimate" and gives the basis. Do not treat the
estimates as acceptance figures: the project's own ledgers say the pedalboard has
open acceptance gates.

## 1. Headline

| Area | Status | Basis |
|---|---|---|
| Pedalboard feature (whole) | **about 55% (estimate)** | per-area figures in section 4 |
| Branded pedal catalogue | **about 6% shipped**; about 60% has code | 1 of 18 specified branded pedals is registered; see 4.5 |
| Linux build | compiles 100%; **about 99.8% of tests pass** | CI: 5 of 2,665 tests failed (Free), 6 on Pro |
| Windows build | **0 of 4 jobs compile**; fix written, unverified | LNK1248 (library over 4 GiB) |
| macOS build | **0 of 4 jobs compile**; fix written, unverified | `std::cyl_bessel_i` missing in libc++ |
| Release artifacts (installers, CLAP etc.) | **0 produced** | no job reached packaging |
| Overall release readiness | **about 35% (estimate)** | 2 of 6 build jobs compiled; nothing packaged |

## 2. Where I am right now

- The integration branch exists, is pushed, and its working tree is clean.
- Every failure I could see in CI run #18 has a fix committed. The Linux
  fixes were verified locally. The Windows and macOS fixes were **not**
  built on those platforms.
- CI run #19 was cancelled mid-flight at your request. It had already shown
  the Windows and macOS errors (section 5), which is how the two fixes were
  written. It never reported the Linux results of the new code.
- The 5-hour routine is disabled. No background process is running.
- `docs/HANDOFF-pedalboard-release.md` holds the operational handoff.

## 3. What I was about to do next

In order:

1. Commit the Windows and macOS fixes (done) and **dispatch the release
   workflow again** to see whether both platforms now compile.
2. Read the Linux results of that run, including **why the
   "pluginval + clap-validator" step failed on both Linux jobs in run #18**.
   I had not investigated that: its annotation says only "exit code 1", and I
   do not know whether it is a real validator failure or a knock-on of the
   failed unit tests.
3. Settle the **bass-decay test** (section 5.3). A `master` baseline build was
   half done to decide whether it is a regression.
4. If Windows still hits LNK1248, take the fallbacks: drop the unregistered WIP
   models from the shared library, or build Windows with LTO off.
5. Once the run was green, leave the **draft** release alone (never publish).
6. Run the full local Pro test suite once (about 1.5 h). My one earlier full
   local run did not list failures that CI later found, so I do not trust it as
   a complete list.

## 4. The pedalboard: what is done and what is not

### 4.1 How the branch was assembled

Base: `origin/codex/luthier-pedalboard-completion` (77 commits ahead of the
earlier release branch). A plain merge of the six sibling branches conflicts on
25 files, because `completion` had copied much of their work in under different
commit IDs. So I cherry-picked the commits that were unique by patch (70 found).

- 64 applied cleanly. 6 conflicted and were skipped: `11f30424`, `5a716ec7`,
  `8ecf6a5a`, `6fcd9b90`, `6d9ce297`, `c65a4a69`. They look like duplicates of
  work that arrived through another branch (send-return loops, runtime extras,
  feedback routing), but I did not confirm each one. **Open question:**
  `11f30424` (E46 MIDI-enable sync) and `8ecf6a5a` (live host enable before
  model echo) could be real gaps.
- About a dozen applied commits are explicitly WIP (circuit candidates with
  recorded failed gates). They are compiled into the library but not registered.

### 4.2 Per-area estimates

| Area | Est. done | Evidence | Main gaps |
|---|---|---|---|
| Board model and persistence | 85% | `PEDALBOARD-STATE.md`; state suites pass | open items in the state ledger; a sparse legacy slot move failure was reported by the branch's own QA |
| Engine and routing (graph, send-return, feedback, aux buses) | 75% | `PEDALBOARD-ENGINE.md`: 43 tests, 56,350 checks, 0 failures in its focused harness | separate 10 ms read-tap crossfade pending; MIDI/foot/transport dispatch to pedals partly pending |
| Host automation and MIDI (maps, learn, clock, expression) | 70% | `PEDALBOARD-HOST-MIDI.md`: 206 cases / 5.6 M checks / 0 failures (native Linux Pro) | "does not claim full MIDI_EXPRESSION acceptance" |
| Time effects (chorus, phaser, delay, reverb) | 65% | `PEDALBOARD-TIMEFX.md`: about 934 k checks, 0 failures | chorus pitch-depth and wet-chain calibration unmeasured; delay mode-switch transient pending |
| Drive and dynamics DSP | 50% | `PEDALBOARD-DSP.md` | boost and fuzz fail original gates; alias, CPU (0.5%), 30-minute soak, static-curve and FRF/THD/IMD checks still open |
| Loopers | 70% | `PEDALBOARD-LOOPERS.md` | full crossfade not complete; asset-failure paths tested |
| UI (Pedalboard window, dock, cables) | 55% | `PEDALBOARD-UI.md`: "partial implementation, not a release sign-off"; 11 UI tests | much of the acceptance contract open; controls for `ref_*` parameters live only in the Pedalboard window |
| Acceptance and QA | 40% | `PEDALBOARD-ACCEPTANCE.md`, `PEDALBOARD-QA.md` | the branch's own notes say its runs "do not certify feature completeness" |

Rough weighting of the above gives about 55% overall. The honest range is
45 to 65%, because acceptance work is where the open gates are.

### 4.3 Integration fixes made in this session (all committed)

| Failure | Cause | Fix | Verified |
|---|---|---|---|
| `PluginBuses` bus count | three appended pedalboard buses are intended | test expects +3 | yes (local) |
| `GuiReach`, 117 `ref_*` params without a control | their controls are in the Pedalboard window, which the main-editor walk never opens | `ref_*` exempt (also from search) | yes (local) |
| JM39, JM43, MidiOutPanel, ModelGaps, rhythm source, tune MIDI-out tests | new bounded MIDI-out writes nothing without a declared output capacity; tests used a fresh unreserved buffer per block | tests reserve capacity (`reserveMidiOutputBuffer`) | yes (local) |
| `RefPedalApi` facade test | tempo now glides over 50 ms, first block reports the old tempo | test checks the glide | yes (local) |
| `CQ03` | `ref_board_cpu_cap` tripped a "no CPU parameter" guard meant for quality mode | `ref_` exempt there | yes (local) |
| unit/category test | no unit for BPM | new `ParamUnit::bpm` | yes (local) |
| trademark test | "Inspired by Boss DS-1" etc. in user-visible text | strings made generic | yes (local) |
| macOS compile | `std::cyl_bessel_i` | local `besselI0` series (I0(6.5) = 106.29 checked) | **no** |
| Windows link | `Luthier_SharedCode.lib` over 4 GiB | Pedalboard sources built `/GL-` on MSVC | **no** |

### 4.4 Decisions you may want to reverse

- Brand names were removed from the pedals' on-screen "Inspired by" text to keep
  the repo's trademark rule. If you want the attribution, the rule needs an
  exception that someone with the legal call decides.
- `ref_*` parameters are exempt from the reachability and search tests.
- Tag `v0.1.0` was chosen to match `project(VERSION 0.1.0)`.

### 4.5 The pedal catalogue (important)

Counts from the code:

- 18 branded pedals are specified (`spec/reference-pedalboard/specs/pedals/`).
- The registration functions `registerDriveA`, `registerDriveB` (except
  Dyna Comp), `registerMuffs`, `registerModDelay`, `registerLoopers` and
  `registerRc5` are **empty skeletons**. Registered today: **10 generic
  effects** (boost, overdrive, fuzz, compressor, chorus, delay, phaser, reverb,
  tremolo, wah and variants), **1 branded pedal (MXR Dyna Comp)**, and the
  prototype pedal.
- Model code exists for most of the other branded pedals (DS-1, SD-1, TS808,
  Distortion+, RAT, CE-2, CE-2W, Phase 90, DD-3T, RC-1, RC-5, Ditto, four Muffs),
  but they are **not selectable by a user** until registered, and several are
  WIP with failed acceptance gates.

So "pedalboard done" splits in two: the board itself is a substantial,
partly validated system; the branded pedal catalogue is mostly not shipped.
Registering the finished models is small work; making the WIP ones pass their
gates is not.

## 5. The build: what worked and what did not

### 5.1 Run #18 (commit `6d9048b5`), the first full release run

| Job | Compile | Unit tests | Validators | Packaging |
|---|---|---|---|---|
| Linux Pro | OK (25 min) | 6 failures | **failed** (cause unknown) | not reached |
| Linux Free | OK (50 min) | 5 failures (of 2,665) | **failed** (cause unknown) | not reached |
| Windows Pro | **FAILED** (LNK1248) | n/a | n/a | n/a |
| Windows Free | **FAILED** (LNK1248) | n/a | n/a | n/a |
| macOS Pro | **FAILED** (compile) | n/a | n/a | n/a |
| macOS Free | **FAILED** (compile) | n/a | n/a | n/a |

Early release regression tests passed on Linux Pro. The "Draft GitHub Release"
job was skipped. No installer, CLAP, VST3 or standalone artifact was produced for
any platform. Roughly 2 hours 20 minutes of CI wall time per full attempt.

### 5.2 Run #19 (commit `1c9ae70b`), cancelled

Windows and macOS failed again with the same two errors (so they are real, not
flukes), and the new error annotations made them readable:

- macOS: `RefPedalOversamplingOracleTests.cpp:20:37: no member named 'cyl_bessel_i' in namespace 'std'`
- Windows: `LNK1248: image size (0x10018DF91) exceeds maximum allowable size (0xFFFFFFFF)` on `Luthier Pro_SharedCode.lib` (Free: `0x1_004734D4`)

The Linux jobs were still running when I cancelled.

### 5.3 The one open Linux test: bass release decay

`Combo.releasedStringIsDampedQuickly` reads P-Style Bass note 42 as **34.7 dB**
down against a **35 dB** bar. It fails the same way locally and on CI, and an
extra isolation commit (`efa7d27d`) did not change it. The test's own comment
says this margin is marginal on some CPUs, but I do not know whether it
regressed with the integration. The deciding experiment (build `master`
`0f62c3d6` and run the same test) was about 30% built when stopped. My
proposed answer if `master` also reads about 34.7: a small, commented
tolerance, not skipping the test. If `master` passes: bisect the audio path.

### 5.4 Other build facts

- Pro and Free are both built by the release workflow; I built only Pro locally.
- A full local unit-test run takes about 1.5 h (the `Combo` suite alone is over
  30 min).
- Local Pro results before the fixes: 1,348 tests passed, 7 failed in the first
  full run; every one of those is fixed or explained. That list was incomplete
  (CI found five more), which is why a final full run is on the to-do list.
- Compile warnings (not errors) exist in several test files; I did not touch them.

### 5.5 What cannot be seen, and how I worked around it

The sandbox blocks `productionresultssa10.blob.core.windows.net`, so CI logs and
artifacts cannot be downloaded. I read results through check-run annotations and
changed the build scripts to print compiler errors as an annotation. To read raw
logs, allow that host under Allowed domains in the environment's network settings.

## 6. Percent remaining, by feature

| Item | Done | Remaining | What the remainder is |
|---|---|---|---|
| Board model, state, persistence | 85% | 15% | state-ledger open items; sparse legacy slot move |
| Engine and routing | 75% | 25% | read-tap crossfade; MIDI/foot/transport dispatch |
| Host MIDI and automation | 70% | 30% | MIDI_EXPRESSION acceptance; the 6 skipped commits to check |
| Time effects | 65% | 35% | calibration and transient measurements |
| Drive and dynamics DSP | 50% | 50% | alias, CPU, soak, FRF/THD/IMD gates; boost and fuzz fail original gates |
| Loopers | 70% | 30% | full crossfade |
| UI | 55% | 45% | acceptance contract |
| Branded pedal catalogue | 6% shipped | 94% | register finished models; finish WIP |
| Acceptance and QA | 40% | 60% | final native runs, soak, hardware/DAW checks |
| Linux build | about 90% | 10% | decay test; validator-step failure; re-verify the fixes in CI |
| Windows build | 0% compiled | 100% | verify the `/GL-` fix; then tests, validators, installer |
| macOS build | 0% compiled | 100% | verify the Bessel fix; then tests, validators, signing, installer |
| Release draft | 0% | 100% | needs a green run on all three platforms |

## 7. Risks and unknowns

1. The Windows fix is a guess at the cause. The library is about 1.6 MB over a
   4 GiB limit; I inferred that the Pedalboard's `/GL` code tipped it over. If
   the fix is not enough, LTO must come off for Windows or the library be split.
2. The validator-step failure on Linux is unexplained.
3. macOS showed one compile error, and the build keeps going after errors
   (`-k 0`), so it is probably the only compile error. But the targets that
   depend on the failed file never reached the link step, so link-time or
   signing errors could still be hiding behind it.
4. Free-only failures only appear in CI, since I built only Pro locally.
5. About a dozen WIP commits with failed gates are compiled in. They are not
   registered, so users cannot reach them, but they add size and risk.
6. Each full CI attempt costs about 2 h 20 min, so every wrong guess is expensive.

## 8. Files and where things are

- Handoff: `docs/HANDOFF-pedalboard-release.md`
- This report: `docs/REPORT-pedalboard-status-2026-10-25.md`
- Pedalboard ledgers: `docs/coverage/PEDALBOARD-*.md`
- Specs: `spec/reference-pedalboard/`
- Failing-run annotations: `gh api repos/djshellshoxxx/luthier/check-runs/<job id>/annotations`
- Release workflow: `.github/workflows/release.yml`
