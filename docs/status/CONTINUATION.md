# Continuing this work

Everything needed to pick the cycle up from a fresh machine. Assume no memory of earlier sessions.

**State: paused by the owner on 2026-11-11.** See PROJECT_STATUS.md for what is parked where.
Eleven parked worker branches are draft pull requests #90 to #100 into the integration branch;
the unfinished security work is not in this repository (it was delivered to the owner as files).

## Read first

1. [PROJECT_STATUS.md](PROJECT_STATUS.md): where things stand and which decisions wait for the owner.
2. [FEATURE_INVENTORY.md](FEATURE_INVENTORY.md): implemented versus validated, per feature.
3. [BRANCH_PR_LEDGER.md](BRANCH_PR_LEDGER.md): what every open pull request and unmerged branch is, and what was decided.
4. `docs/handoff/PEDALBOARD-2026-10-28-CONTINUATION.md`: newest pedalboard handoff from the previous integrators.
5. `scripts/agents/README.md`: how to run parallel workers on one machine, with the pitfalls.
6. `docs/status/discovery-2026-11-11/`: the nine audit reports this cycle's decisions rest on.
7. `docs/status/handoff-2026-11-11/`: write-ups for the rack-pedal use-after-free, MIDI continuity
   and the Windows-only test failure.

## Branch model

- `master`: default branch. Nothing from this cycle is merged there.
- `integration/luthier-completion`: integration branch (draft PR #89). All work lands here first.
- `agents/luthier/<workstream>/<task>`: one branch per worker, each with a draft PR whose base is
  the integration branch. The coordinator reviews, merges locally with `--no-ff`, verifies, pushes.
- Never force-push a shared branch. Never push to a branch another worker owns.
- **Before any merge to `master`**, resolve the beta 2 publisher hazard described in the ledger.

## Set up a machine (Linux x64)

```bash
git clone https://github.com/djshellshoxxx/luthier && cd luthier
git checkout integration/luthier-completion
scripts/setup_linux.sh                      # packages, pinned JUCE 8.0.10 and clap-juce-extensions, configure build/
# development configure used in this cycle (link-time optimisation off):
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DLUTHIER_LTO=OFF \
      -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
      -DCMAKE_C_COMPILER_LAUNCHER=ccache -DCMAKE_CXX_COMPILER_LAUNCHER=ccache
ninja -C build -j4 LuthierTests LuthierRender      # about 15 minutes for the test binary on 4 cores
```

Then the compiler-cache settings in `scripts/agents/README.md` if several worktrees will be used.

## Run things

```bash
# One suite or test (substring match, case-insensitive):
scripts/agents/test.sh . RefPedalCe2
# Whole suite in three exact shards:
scripts/agents/shards.sh build/LuthierTests_artefacts/Release/LuthierTests 3 /tmp/full-run
# Validators (downloads pluginval and clap-validator; needs network):
scripts/ci_build.sh validate
```

Useful environment variables: `LUTHIER_SOAK=1` (30-minute board soak instead of 20-30 s),
`LUTHIER_PERF=1` (enable timing gates), `LUTHIER_SCREENSHOTS=<dir>` (write editor screenshots),
`LUTHIER_STRESS_SECONDS`, `LUTHIER_STRESS_JSON` (processor stress test, once merged).

## Resuming a parked branch

```bash
git fetch origin
scripts/agents/worktree.sh uaf-fix agents/luthier/audio/effects-chain-retire-2 origin/agents/luthier/audio/effects-chain-retire
```

Parked WIP commits say in their message whether the tree builds and what was verified. Most
parked tests were never compiled: build first, see the new test fail, then write the fix. The
last commit of each parked branch carries `[skip ci]`; the next ordinary commit starts CI again.

## Windows and macOS

There is no local machine for them. Evidence comes from GitHub Actions:

- A push to a pull request into `master` runs the PR verification matrix.
- To build a specific branch on chosen platforms without a pull request: run the `build` workflow
  by hand (Actions, "build", "Run workflow") with `platforms` set to `windows`, `macos` or both.
  Manually dispatched runs are in their own concurrency group and are not cancelled by later pushes.
- A digest of test failures is posted as a pull-request comment by the PR verification workflow.

## Rules that must not be broken

- Host parameters are append-only and position-indexed.
- No allocation, lock, logging or I/O on the audio thread.
- No third-party brand names in shipped strings or new identifiers.
- Never weaken a test or threshold to get a green run; a failing original gate stays visible.
- Run `scripts/fixeol.sh` before each commit (mixed line endings).
- Registration of a named pedal model happens only after its original gates pass.
