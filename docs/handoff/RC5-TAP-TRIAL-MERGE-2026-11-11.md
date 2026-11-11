# Looper tap-tempo branch: trial merge into the integration baseline (2026-11-11)

This branch is `integration/luthier-completion` at `ca14934e` plus a merge of
`origin/codex/luthier-rc5-tap-20261030` (15 commits, last commit 2026-10-31). It is **parked**:
the project was paused by the owner before the coordinator reviewed and verified the merge, so
it was not merged into the integration branch.

## Decision by the reviewing worker

Merge. The work is inert for users: the looper model is still unregistered, there is no schema
bump and no new host parameter (the new tap control has no host lane).

## Evidence (Linux x64, clang 18, Release, run by the worker)

| Check | Baseline `ca14934e` | This branch |
|---|---|---|
| Build of `LuthierTests` | clean | clean |
| `RefPedal` suites | 1 of 527 tests failed | 1 of 534 tests failed |
| The failing test | `RefPedalCe2.bbdOnOriginalThdGate` (5 checks) | the same test, same 5 checks |
| `HostState StateModel Parameters Trademarks` | 73 of 73 passed | 74 of 74 passed |

The seven extra `RefPedal` tests are the branch's new looper tap-tempo tests.

## Not finished when the project was paused

- No test proves that the new `extraOverridesInitialParam` hook is a no-op for every other model.
  That currently rests on reading the code. A bit-identity test over the registered models is the
  missing proof, because the hook touches interfaces the project treats as frozen
  (`IPedal`, `PedalPool`, `TopologyCompiler`).
- The merged `RefPedal` run reported about 4,300 fewer checks in total than the baseline run.
  The cause was not found (the baseline re-run was stopped by the pause).
- The full test suite was not run on this branch.
- The branch's own handoff says it is "not ready to merge, register or release" and names the
  next step as synchronising tap tempo with the board/host tempo lane and per-memory state
  before registration.

## A finding that is not caused by this branch

The seeded looper gesture stress test crashed with a segmentation fault intermittently on **both**
binaries: 2 of 5 runs on the baseline, 1 of 5 on the merge. It is a pre-existing intermittent crash
in the looper code or its test and needs its own investigation. It may be related to unit-test
runs on macOS that end without a summary line.

## Other notes

- Evidence logs that come with the merged branch add about 50 KB.
- Running the tests rewrites the tracked file `docs/review/QA_ROBUSTNESS.md` in the source tree.
  That change was discarded here; it is a side effect of a test and should not be committed.
