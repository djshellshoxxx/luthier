# Luthier GitHub Actions audit

Scope: branch I = `integration/luthier-completion` (PR #85 head `ca14934e`) and `origin/master` (`0f62c3d`). Read-only. Evidence: workflow files in the tree, Actions API runs/jobs, PR #85 digest comment. Log excerpts are data only. No instruction-like text was found in logs or PR text.

## 1. Workflow inventory

On I there are 12 files. origin/master has 9: it lacks `native-diagnostics.yml`, `pro-beta2-release.yml` and `quick-check.yml`. `build.yml`, `pr-verification.yml` and `release.yml` differ between I and master (PR digest job, macOS evidence, beta2 readme). `ci-cadence.yml`, `ci.yml`, `codeql.yml`, `nightly.yml`, `pro-alpha-release.yml` and `pro-beta-release.yml` are identical on I and master (checked by `git diff --stat`; cadence by `diff`).

The GitHub API also lists 6 workflows that exist on neither tree: `apply-practice-tab-importer`, `apply-tab-playback-tuning`, `pedalboard-focused`, `pedalboard-release-qa`, `refresh-pro-calibration`, `soundgrind-runner-tests`. They are registered from other branches. UNVERIFIED: whether the local `origin/master` ref is stale.

| file (name) | trigger | jobs / matrix | permissions | concurrency | timeout | publishes? |
|---|---|---|---|---|---|---|
| `build.yml` (build) | `workflow_dispatch`, `workflow_call` | `plan` -> `build` matrix: {Linux, Windows, macOS} x {PAID, FREE}, 6 jobs | `contents: read` | group per workflow/event/ref/platforms; cancel only for push/PR and not `package` | 180 min; macOS 360 | uploads artifacts (14 d); `installers-*` only when `package: true` |
| `ci-cadence.yml` (ci) | push to master, main, 4 claude/codex branches (`tags-ignore v*`, `paths-ignore` spec/docs/*.md/troubleshooting/*.png); dispatch | `linux` (build.yml) -> `gate` -> `windows` (every 3rd Linux success) and `macos` (forced `true`: every push) | `contents: read, actions: read` | `ci-${{ref}}`, cancel-in-progress | inherits build.yml | artifacts only (`ci-linux-success` marker, 90 d) |
| `pr-verification.yml` | pull_request to master/main (opened, synchronize, reopened, ready_for_review) | `platforms` (build.yml, linux,windows,macos, 6 jobs, strictness 5, LTO off) + `digest` (PR comment) + `macos-beta-release` | `contents: read`; digest adds `pull-requests: write`; beta job `contents: write` | per PR number, cancel-in-progress | digest 15, beta 45 | `macos-beta-release` creates a non-draft prerelease `v0.1.0-macos-beta1`, gated on PR number 75 and head ref `codex/luthier-pro-v01-release`. Not reachable from #85 |
| `quick-check.yml` (I only) | pull_request with paths filter (Source, Tests, CMakeLists, cmake, scripts, workflows); dispatch | `focused-linux` x {PAID, FREE}; builds the whole `LuthierTests` target, runs smoke tests | `contents: read` | per ref, cancel-in-progress | 45 min | no |
| `codeql.yml` | push and PR to master and `codex/luthier-beta`; cron `27 4 * * 1` | `analyze`: setup_linux.sh, `ninja -j2 LuthierTests`, CodeQL | `contents: read, security-events: write` | per ref, cancel-in-progress | 120 min | no (code-scanning upload) |
| `release.yml` | push tag `v*`; dispatch (tag, ref, platforms, editions) | `version` -> `build` (build.yml, `package: true`, LTO, strictness 10) -> `draft` | `contents: read`; `draft` `contents: write` | per tag, no cancel | build.yml limits | creates the tag via API if missing and a DRAFT GitHub Release with installers and SHA256SUMS. A human publishes it |
| `pro-alpha-release.yml` | `workflow_run` of PR verification on branch `codex/luthier-pro-v01-release`; dispatch | `preflight`, `publish`, `screenshots` (macos-15) | top-level `contents: write, actions: read` for all jobs | `luthier-pro-alpha-publish`, no cancel | 60 / 40 / 70 | publishes `v0.1.0-alpha1` as a non-draft prerelease (draft -> verify -> `--draft=false`). `screenshots` runs `git push origin HEAD:master` |
| `pro-beta-release.yml` | `pull_request` closed to master | gated on merged PR from head ref `codex/luthier-pro-v01-release`; runs build.yml with `package` | `contents: read`; publish `contents: write` | no cancel | n/a | creates and un-drafts `v0.1-beta`. Refuses if tag or release exists |
| `pro-beta2-release.yml` (I only) | `pull_request` closed to master | gated on PR number 77 and head ref `codex/preflight-build-hardening-2026-10-20` | same pattern | no cancel | n/a | creates and un-drafts `v0.1.0-beta2` |
| `native-diagnostics.yml` (I only) | push to `codex/luthier-native-diagnostics` or `codex/luthier-macos-diagnostics`; dispatch | windows + macos Pro, no LTO | `contents: read` | per ref, cancel | 105 | no (artifacts 7 d, "never a release") |
| `ci.yml` (manual Linux package) | `workflow_dispatch` only | one Linux job: build, test, pluginval 10, cpack | none declared | `ci-${{ref}}`, cancel-in-progress | 120 | artifact only |
| `nightly.yml` | `workflow_dispatch` only (schedule removed 2026-09-26) | `perf` (240 min), `asan` (120 min) | none declared | none | 240 / 120 | artifact only |

### What fires on a push to master

A push to master is a merge of a PR. Both a PR merge and a direct push give the same push event.

- **`ci-cadence.yml` fires.** It runs Linux Pro and Free through build.yml. On a push to master STRICTNESS is 10 (the HI-43 rule) and LTO is OFF. Then `gate` runs. macOS Pro and Free always run (forced `macos=true`). Windows runs on every 3rd successful Linux build.
  - Cost per push: about 2 Linux + 2 macOS (+ 2 Windows) jobs.
  - Published: nothing. Only `upload-artifact` (14 d logs and plugins, a 90 d marker).
  - A change that touches only `docs/**`, `*.md`, `spec/**`, `troubleshooting/**` or root `*.png` is skipped by `paths-ignore`.
- **`codeql.yml` fires.** It is a push trigger on master, about 2 h of runner time. It uploads SARIF, not a release.
- **`pro-beta-release.yml` and `pro-beta2-release.yml`** trigger on the PR-closed event, not on the push. Both are gated on head ref or PR number. A merge of PR #85 (head `claude/determined-ptolemy-mlbclp`) fires neither. If the PR were #77 or the `codex/luthier-pro-v01-release` branch, they would create real public prereleases.
- **`pro-alpha-release.yml`** fires from the `workflow_run` of PR verification only for branch `codex/luthier-pro-v01-release`. Not reachable from #85.
- **`release.yml`** fires only on a `v*` tag push. It produces a DRAFT release.
- **Master pushes by automation.** The `screenshots` job of pro-alpha does `git push origin HEAD:master`. That push touches README.md and docs/screenshots only, so `paths-ignore` filters it. Release publishing and master writes need `contents: write`.

Summary: a master push publishes no release. The only published releases come from the gated alpha, beta, beta2 and macos-beta flows, which need specific branches, PR numbers or tags. All of them are non-draft prereleases published automatically after their checks, not drafts.

## 2. Recent runs

### Per-job wall time, PR #85 run 41796728348 (PR verification, run 125, 2026-11-04, total 263 min)

Jobs were created 08:22 but only started 08:32 to 09:11 (queue 10 to 49 min).

| job | start to end | build step | unit tests step | other | conclusion |
|---|---|---|---|---|---|
| Linux Pro | 09:11 -> 10:58 (107 m) | 26 m | 79 m (fail) | early regressions 0.7 m, validators 0.8 m | failure |
| Linux Free | 08:58 -> 11:19 (141 m) | 50.5 m | 89 m (fail) | factory riffs 0.6 m | failure |
| Windows Pro | 08:32 -> 10:49 (137 m) | 42 m | 92 m (fail) | early regressions 0.7 m | failure |
| Windows Free | 08:39 -> 11:16 (157 m) | 59 m | 97 m (fail) | n/a | failure |
| macOS Pro | 08:37 -> 09:39 (62 m) | 55 m | skipped | early regressions 1.8 m (FAIL), pluginval 4.5 m | failure |
| macOS Free | 08:22 -> 12:26 (244 m) | 51 m | 189 m (fail, no summary line) | pluginval 3.3 m (fail) | failure |
| digest | 12:27 (12 s) | n/a | n/a | n/a | success |

The slowest steps are the unit tests (79 to 189 min). They are single-threaded and the full suite of 2,900+ tests runs on every PR push. The second slowest is the build, which runs without a shared artifact. quick-check on the same SHA took 32 min per job (compile about 30 min, smoke 40 s).

The 2026-11-04 CodeQL run 41796727995 on #85 was a success: 105 min total (build 73 min, analyze 14 min).

### Run list (about 15 completed runs)

| run id | workflow | event / branch | conclusion | duration |
|---|---|---|---|---|
| 42622272751 | CodeQL | schedule / master (11-09) | cancelled | 120 m |
| 41915106091 | PR verification | PR #87 | failure | 301 m |
| 41915106090 | CodeQL | PR #87 | cancelled | 121 m |
| 41796728348 | PR verification | PR #85 | failure | 263 m |
| 41796728281 | quick-check | PR #85 | success | 33 m of job time |
| 41796727995 | CodeQL | PR #85 | success | 105 m |
| 41446504434 | CodeQL | schedule / master (11-02) | cancelled | 122 m |
| 41036633882 | PR verification | PR #84 | failure | 294 m |
| 41036633817 | CodeQL | PR #84 | cancelled | 120 m |
| 40678207140 | quick-check | PR #83 | success | 2 m |
| 40649786319 | quick-check | PR #83 | success | 2 m |
| 40603478203 | PR verification | PR #82 | failure | 327 m |
| 40603478168 | CodeQL | PR #82 | cancelled | 121 m |
| 40603478163 | quick-check | PR #82 | success | 27 m |
| 40306349092 | CodeQL | schedule / master (10-26) | success | 91 m |

Of the last 15 completed CodeQL / PR verification / quick-check / build runs listed above, every PR verification failed (4 of 4), 5 of 7 CodeQL runs were cancelled, and quick-check passed whenever it ran to the end. All 4 `build` workflow_dispatch runs on `claude/quirky-darwin-eoesyi` failed after 100 to 120 min (not inspected). The last `ci` (cadence) runs on master are from 2026-10-19/20 (runs 39331151677 to 39359397237: 3 cancelled, 2 failures). No master push has run since.

Of PR #85's checks only quick-check and CodeQL are green. Run time was 263 min wall.

### Why CodeQL runs end "cancelled" after about 2 h

They hit `timeout-minutes: 120`, not a human cancel and not concurrency.

- Evidence: run 42622272751 started 13:04:36. Step "Build LuthierTests" ran 13:05:52 to 15:01:14 (115.4 min). Step "Perform CodeQL analysis" started 15:01:14 and was cancelled at 15:04:36. The job ended 15:04:51, exactly 120 min after start.
- The same job on a lucky runner took 73 min to build (run 41796727995, 105 min total) and 91 min for the success on 10-26. The build has no compiler cache (`CCACHE_DISABLE: '1'`) and uses `ninja -j2`. It builds the full `LuthierTests` unity, so run time varies by about 1.5x depending on the runner and sits within 15 to 30 min of the cap. The analyze step needs another 14 min.
- The scheduled runs on master (11-02 and 11-09) are not superseded by a newer push, so concurrency is not the cause there. The PR runs 41915106090, 41036633817 and 40603478168 show the same 120 to 121 min figure.
- Fix direction: `-j4` (the runner has 4 vCPU), a bigger timeout or reduced scope, and an explicit CodeQL build of only the source library target.

### Why PR verification #85 (run 41796728348) failed

First error per job. Sources: the PR #85 digest comment (github-actions[bot], comment 6630575628) and the failed-step logs for 3 jobs.

| run | job | first error |
|---|---|---|
| 41796728348 | Linux x64 / Pro | `[FAIL] bbdOnOriginalThdGate` (RefPedal CE-2): "BBD-on THD rate 48000.0 Hz: measured ratio 0.000017 (0.0017%), required 0.005..0.03". "1 of 2923 tests failed". This is the only failing test. pluginval strictness 5 SUCCESS, clap-validator 18/21 passed |
| 41796728348 | Linux x64 / Free | same: `bbdOnOriginalThdGate`, "1 of 2906 tests failed" |
| 41796728348 | Windows x64 / Pro | `[FAIL] tenThousandNativeOperationsAndEditorDestruction` at line 220, `expected model.getInstanceCount()==count+1`, last model rejection "Duplicate pedal" with 13 instances; plus `bbdOnOriginalThdGate`. "2 of 2922 tests failed" |
| 41796728348 | Windows x64 / Free | same two tests ("15 pedals"), "2 of 2905" |
| 41796728348 | macOS universal / Pro | step "Early release regression tests (Unix Pro)" failed 09:32 to 09:34 (1.8 min). The test runner log is cut off by the log tail, so the failing test name is not visible. UNVERIFIED. The digest says "no unit-tests.log" because the unit-test step was skipped |
| 41796728348 | macOS universal / Free | "Unit tests (Unix)" failed after 189 min with "no summary line (crash or timeout?)". Digest excerpts: `[FAIL] aSessionSurvivesThePrepareThatFollowsIt` (line 230, `expected sameState(... "luthier-restored")`); `convolutionPreparesSafelyForTinyHostBlocks` and `convolutionTinyHostRateAndCapacityBoundaries` ("alternate preset must select a distinct supported body IR"); `saveExtraExceptionReacquiresGuardAndAllowsRetry` (line 121). pluginval step also failed (exit 1) after "21 tests run, 18 passed, 0 failed, 3 skipped" (clap) |

Common failure: `bbdOnOriginalThdGate` fails on every platform and both editions (CE-2 BBD THD is measured at 0.0003 to 0.0097%, spec wants 0.5 to 3%). It is a known RED (an earlier commit message calls it "the known CE-2 RED"). It alone fails Linux, so Linux is red only because of this single test. Windows adds a native UI stress failure (Duplicate pedal rejected). macOS has distinct failures that need a log (names above from the digest).

Billing note on the same run: `get_workflow_run_usage` returns `billable` total_ms 0 for UBUNTU, WINDOWS and MACOS. That is consistent with a public repo (free runner minutes) and with the comment in `ci-cadence.yml` ("public repo: macOS runners are free").

## 3. Rule comparison (violations with patches)

| # | rule | status | violation | patch |
|---|---|---|---|---|
| 1 | concurrency + cancel-in-progress on push/PR, never on release | mostly OK | `ci-cadence.yml`, `pr-verification.yml`, `quick-check.yml`, `codeql.yml`, `native-diagnostics.yml` cancel. release / pro-* / build `package` do not (correct). `ci.yml` cancels but is manual. `nightly.yml` has none. Problem: cancelled runs still leave 2 PR-triggered workflows (PR verification + CodeQL) with different groups | no change for the cancel flags; add `concurrency: {group: nightly, cancel-in-progress: false}` to `nightly.yml`. |
| 2 | `paths-ignore` docs / markdown / images | violation | `pr-verification.yml` and `codeql.yml` have no path filters: a docs-only PR triggers 6 build jobs and a 2 h CodeQL. `ci-cadence` ignores `*.png` at root only. `quick-check` uses an allow list (OK) | add `paths-ignore: ['docs/**','**/*.md','spec/**','troubleshooting/**','**/*.png','**/*.jpg','LICENSE']` to `pr-verification.yml` and `codeql.yml` (push + pull_request). Careful: required checks stay pending when a workflow is skipped by path filters, so use a path gate job if branch protection requires the check |
| 3 | least-privilege `permissions:` | partial | `ci.yml`, `nightly.yml`, `native-diagnostics.yml`(has contents: read) , `ci.yml` and `nightly.yml` have no `permissions:` block (default token scope depends on repo setting, UNVERIFIED). `pro-alpha-release.yml` grants `contents: write` at workflow level to `preflight` and `publish`, which only need read. `ci-cadence.yml` grants `actions: read` to all | add `permissions: {contents: read}` to `ci.yml`, `nightly.yml`; in `pro-alpha-release.yml` set top-level `contents: read, actions: read` and `contents: write` only on `publish` and `screenshots` |
| 4 | `fetch-depth: 1` | OK | `actions/checkout@v4` default depth is 1 everywhere. Exception: `pro-alpha-release.yml` `screenshots` sets `fetch-depth: 0` (needs `git fetch origin master`; not needed) | drop `fetch-depth: 0` there |
| 5 | explicit `cmake --build --parallel N` ~1 job per 2 GB | partial | `ci_build.sh` passes `--parallel $JOBS`: Linux default `JOBS=8` set by build.yml on a 4-vCPU / 16 GB runner (over-subscribed vs cores, within RAM); macOS 4, Windows 4 (OK). `codeql.yml` uses `ninja -j2` (slow: 73 to 115 min). `ci.yml` and `nightly.yml` use `ninja -j4`. `quick-check.yml` `JOBS: '4'` OK. Runner core count is UNVERIFIED (public-repo runners are documented at 4 vCPU / 16 GB for Linux and Windows) | build.yml: `JOBS: ${{ runner.os == 'Linux' && '4' || '4' }}` -> just `'4'`; `codeql.yml`: `-j4` |
| 6 | build each artifact once, reuse via upload/download | big violation | `LuthierTests` is compiled 5 times on every PR push of the same SHA: quick-check x2 (PAID, FREE), build.yml Linux x2, CodeQL x1, plus Windows x2 and macOS x2. quick-check (32 min) and the Linux build (26 to 50 min) rebuild what build.yml builds. Windows and macOS are separate machines (must build natively) | make quick-check the first job of `pr-verification`: have it upload the Linux `LuthierTests` binary (`upload-artifact`) and let later Linux test jobs `download-artifact` it, or delete quick-check and keep one Linux gate. Share the Linux build between `build.yml` and `codeql.yml` is NOT possible (CodeQL must observe the compile) |
| 7 | build only needed targets | partial | build.yml `ci_build.sh configure build` builds plugin targets + `LuthierTests` + `LuthierRender` on every job (per its header comment). quick-check and CodeQL build `LuthierTests` only (OK). | add a `targets` input and build only `LuthierTests` for the per-push Linux gate; plugin formats only in the packaging/validate job |
| 8 | expensive macOS / Windows only on main, nightly, dispatch; one fast Linux gate per push | violation | `pr-verification.yml` runs all 6 jobs (Linux, Windows, macOS x2 editions) on every PR push (about 4 h wall). `ci-cadence.yml` builds macOS on every push to master (forced true) with Windows every 3rd. `ci-cadence.yml` header says macOS every 15th, comment says "public repo: macOS runners are free" | change `pr-verification.yml` `platforms:` to `linux` (Linux Pro + Free) and add Windows and macOS only for `workflow_dispatch`, labels (`if: contains(github.event.pull_request.labels.*.name, 'native')`) or on push to master. Reinstate the cadence for master |
| 9 | cache fetched dependencies keyed on the pin | partial | JUCE and clap-juce-extensions are cached (`actions/cache@v4`, key `thirdparty-${{runner.os}}-${{hashFiles(scripts/ci_build.sh, ci_build.ps1)}}`). No build trees are cached (OK). ccache/sccache (`hendrikmuhs/ccache-action`) caches compiler output for builds, not build trees (acceptable). Key hashes whole scripts, so any script edit re-clones JUCE; `clap-juce-extensions` is cloned without a tag or SHA (see 11) | derive the key from the pins: `hashFiles` of a small `scripts/deps.pin` file, or `key: juce-8.0.10-clap-<sha>` |
| 10 | first-party actions only | violation | third-party actions: `seanmiddleditch/gha-setup-ninja@v5` (4 uses), `hendrikmuhs/ccache-action@v1.2` (4), `ilammy/msvc-dev-cmd@v1` (2). All the rest are `actions/*` or `github/*` (`actions/checkout@v4` x19, `upload-artifact@v4` x12, `download-artifact@v4` x9, `github-script@v7` x8, `cache@v4` x4, `setup-python@v5` x2, `github/codeql-action/{init,analyze}@v3`) | ninja: `pip install ninja` or `choco install ninja`/`brew install ninja` (Linux already uses apt); msvc-dev-cmd: run `vcvarsall.bat` through `cmd`/`pwsh` (VS is preinstalled on windows-latest); ccache-action: `actions/cache@v4` over `~/.ccache` with `CCACHE_DIR`. If third-party actions stay, pin them to full commit SHAs. Tags like `@v1` are mutable and these actions run with the repo's secrets inherited in release builds (`secrets: inherit`) |
| 11 | pinned tool versions | partial | JUCE `8.0.10`, pluginval `v1.0.4`, clap-validator `0.3.2` are pinned. NOT pinned: `clap-juce-extensions` is a shallow-less `git clone` of the default branch (no tag or SHA); runner images `ubuntu-latest`, `windows-latest`, `macos-latest` (macOS 15 is explicit in 3 jobs); `ubuntu-24.04` is pinned in codeql, ci, nightly. apt packages unversioned. Action tags (see 10) | pin `clap-juce-extensions` to a SHA in both scripts; use `ubuntu-24.04`, `windows-2025` (or `windows-2022`), `macos-15` in build.yml; pin actions by SHA |

Other findings:
- **Duplicate PR workloads.** CodeQL, PR verification and quick-check all trigger on the same PR push. CodeQL (2 h, often timing out) and PR verification (4 to 5 h) are the two big ones.
- **Early-test duplication.** build.yml runs a subset of tests as "Early release regression tests", then the full suite. quick-check runs another subset. That is three test passes over the same names.
- **Logs.** `Upload logs` and `Upload plugins` per job are fine at 14 d retention.
- **Publishing workflows on `pull_request: closed`** (`pro-beta*`) run with `contents: write` from the PR event. They are gated on head repo equal to this repo and a fixed branch or PR number, which is good. A hard-coded PR number (77) is fragile but safe.
- **`pr-verification.yml` `digest` job** posts a PR comment on every run, including same-repo PRs from bots. It uses `github.token` with `pull-requests: write`. Safe: the checkout uses `persist-credentials: false`.
- **Interpolated expressions.** `${{ github.event.pull_request.head.sha }}` is used inside a shell script in `digest`. It is a hex SHA, so not injectable. `inputs.tag` in release.yml goes through `env`, which is the right pattern.

## 4. Billing / minutes

Not a blocker now.

- The old handoff said jobs died in about 5 s. Recent evidence disagrees: the most recent runs are 2 min (quick-check), 33 min, 105 min, 244 min (macOS Free) and 301 min jobs. All of them start, run to a real result, and finish. The only 5-minute run in the recent list is `Pedalboard focused acceptance` run 41166786730 (2026-10-31, failure, 5 min, on another branch and another workflow). It is a real failure, not an immediate billing death. UNVERIFIED: its cause.
- `get_workflow_run_usage` for run 41796728348 reports 0 billable ms on all three OS types, as expected for a public repo.
- The real limit is time and queueing, not minutes. Jobs queued 10 to 49 min before starting. macOS jobs are slow (244 min, with a 360 min cap). Free-plan concurrency (about 20 jobs, 5 macOS) is UNVERIFIED but fits the queueing.
- CodeQL on a timeout wastes 2 h per run.

## 5. Prioritised fix list

| pri | file | change | expected saving / risk removed |
|---|---|---|---|
| P0 | `.github/workflows/codeql.yml` | `ninja -j4`; `timeout-minutes: 180`; add `paths-ignore` for docs; trigger on `schedule` + `push` to master only (drop `pull_request`, or keep it with paths filter) | stops the recurring `cancelled` (5 of 7 recent runs); saves 120 min per PR push |
| P0 | `.github/workflows/pr-verification.yml` | `platforms: linux` per PR push (Pro + Free); Windows and macOS only via `workflow_dispatch`, a label, or on push to master | 6 jobs -> 2: PR wall time 263 min -> about 110 min; removes 4 h of macOS and Windows per push |
| P0 | `.github/workflows/quick-check.yml` and `build.yml` | stop compiling `LuthierTests` twice per edition: keep quick-check as the fast gate and make `build.yml` Linux reuse its artifact (`upload-artifact` / `download-artifact`), or drop quick-check | saves about 30 to 50 min of Linux compile per PR push |
| P1 | `.github/workflows/ci-cadence.yml` | restore `[ $((n % 15)) -eq 0 ] && macos=true` (or restrict macOS to main + dispatch), keep Windows every 3rd | removes about 2 macOS jobs (about 4 h) per master push |
| P1 | `pr-verification.yml`, `codeql.yml` | add `paths-ignore` for docs, markdown, images | docs-only PRs cost 0 build minutes |
| P1 | `build.yml` | `JOBS: '4'` on Linux; add `targets` input so the per-push gate builds only `LuthierTests` | avoids CPU oversubscription (8 jobs on 4 vCPU) and about 5 to 10 min of plugin/LuthierRender link time per job (UNVERIFIED size) |
| P1 | `pro-alpha-release.yml` | move `contents: write` from workflow level to the `publish` and `screenshots` jobs; drop `fetch-depth: 0` | removes write token from `preflight` |
| P2 | `build.yml`, `quick-check.yml`, `native-diagnostics.yml` | replace `seanmiddleditch/gha-setup-ninja`, `ilammy/msvc-dev-cmd`, `hendrikmuhs/ccache-action` with first-party equivalents or pin to a commit SHA | removes supply-chain risk in jobs that inherit release secrets |
| P2 | `scripts/ci_build.sh`, `scripts/ci_build.ps1` | pin `clap-juce-extensions` to a commit; key the dependency cache on the pins only | reproducible builds; avoids unrelated cache busts |
| P2 | `ci.yml`, `nightly.yml` | add `permissions: contents: read`; nightly add concurrency | least privilege |
| P2 | `build.yml` | pin runner images (`ubuntu-24.04`, `windows-2025` or `windows-2022`, `macos-15`) | image drift cannot change results |
| P3 | `build.yml` | drop the duplicate "Early release regression tests" step once a Linux gate runs the same names | saves about 1 to 2 min per job |

Release risk: no workflow on master publishes automatically after a plain merge of #85. If PR #85 is merged, `pro-beta2-release.yml` arrives on master gated on PR number 77. Check that PR #77 is closed before relying on it. UNVERIFIED: whether PR #77 and PR #75 are merged and whether `v0.1.0-beta2` already exists.

## 6. UNVERIFIED

- The first error of the macOS Pro job (step "Early release regression tests (Unix Pro)"). Job logs are served as 5,000-line tails only, and the signed blob URL was refused by the agent proxy.
- Whether the local `origin/master` ref is stale versus the real master (6 extra workflows appear in the API list).
- The repo's default `GITHUB_TOKEN` permission setting.
- Runner core counts (the report assumes 4 vCPU / 16 GB for public Linux and Windows runners, per ci_build.ps1 comment).
- Why the 4 `build` workflow_dispatch runs on `claude/quirky-darwin-eoesyi` failed, and why `Pedalboard focused acceptance` run 41166786730 failed after 5 min. Not inspected.
- Whether PR #75, #77 and the beta tags are merged or exist.
