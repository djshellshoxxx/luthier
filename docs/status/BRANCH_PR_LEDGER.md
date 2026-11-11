# Branch and pull-request ledger

State of every open pull request and every remote branch that is not already contained in the
integration baseline, with the decision taken for each. Checked on 2026-11-11.

- Integration baseline: `integration/luthier-completion`, started at `ca14934e` (head of PR #85,
  342 commits ahead of `master` at `0f62c3d`).
- 174 remote branches existed. 145 are ancestors of the baseline (their work is contained).
  The 29 that are not are listed below.
- Method: `git cherry` (patch equivalence), `git merge-tree --write-tree` (trial merge), and a
  per-commit `git apply --check` against the baseline tree, plus symbol and test-name searches,
  because earlier integrators re-applied worker slices under new commit ids instead of merging.
- Nothing in this ledger was merged to `master`. No pull request was closed.

## Release hazard to resolve before any merge to master

`.github/workflows/pro-beta2-release.yml` publishes a beta when pull request #77
(`codex/preflight-build-hardening-2026-10-20`) is closed as merged into `master`. The integration
baseline contains that branch's commits (merged locally as `a63c6100`). GitHub marks a pull
request as merged when its commits reach the base branch by any route, so merging the integration
branch into `master` can fire that publisher even though #77 itself was never merged by hand.
Before merging anything that contains those commits: close #77 unmerged, or remove that trigger.
`pro-beta-release.yml` (head ref `codex/luthier-pro-v01-release`) and the alpha/macOS publishers
(PR #75) are scoped to other branches and are not affected.

## Open pull requests

| PR | Head branch | Base | Contained in baseline | Decision | Reason |
|---|---|---|---|---|---|
| #72 | `claude/luthier-cloud-session-5lzlix` | master | no (1 commit) | Owner decision | 60-day Pro trial and unlock code: a licensing/product change, stale since 2026-10-09, conflicts in `Source/PluginProcessor.cpp` |
| #73 | `codex/reference-pedals-research-and-foundation` | master | yes | Superseded by the integration PR | ancestor |
| #76 | `codex/soundgrind-stress-audio-gui` | master | no (1 commit) | Superseded by #84 | original repeat-runner had a zero-test false green that #84 fixes |
| #77 | `codex/preflight-build-hardening-2026-10-20` | master | yes | Do not merge; see hazard above | ancestor; merging it publishes beta 2 |
| #78 | `docs/readme-and-wiki` | master | no (1 commit) | Not merged; accurate parts reused in the docs refresh | README conflicts; content predates the pedalboard and has unverified counts |
| #79 | `claude/affectionate-planck-adfno4` | master | yes | Superseded by the integration PR | ancestor |
| #80 | `codex/luthier-pedalboard-completion` | master | yes | Superseded by the integration PR | ancestor |
| #81 | `claude/intelligent-johnson-ruc6si` | `codex/luthier-native-diagnostics` | no (1 commit) | Superseded | the fix is already in the baseline; the branch's only unique content is committed `.pyc` files |
| #82 | `claude/luthier-pedalboard-integration` | master | yes | Superseded by the integration PR | ancestor |
| #83 | `claude/quirky-darwin-eoesyi` | `claude/luthier-pedalboard-integration` | yes | Superseded by the integration PR | ancestor |
| #84 | `codex/luthier-soundgrind-repair-20261030` | master | no (4 commits) | Not taken in this cycle | a Python repeat-runner around the test binary; adds no stress coverage (its seed variable is read by no C++ code); contained in #86 |
| #85 | `claude/determined-ptolemy-mlbclp` | master | is the baseline | Superseded by the integration PR | the integration branch continues from its head |
| #86 | `codex/luthier-soundgrind-ci-20261104` | `codex/luthier-soundgrind-repair-20261030` | no (6 commits) | Owner decision | adds a three-OS workflow for the repeat-runner; merges cleanly; optional |
| #87 | `codex/setup-autonomous-agent-20261104` | master | no (3 commits) | Owner decision; not folded in | adds a workflow that runs an autonomous coding agent with `contents: write` and pull-request rights: security-relevant and costs API spend |

## Remote branches not contained in the baseline

| Branch | Class | What is unique | Trial merge | Decision |
|---|---|---|---|---|
| `codex/luthier-backend-release-retry` | unique | `9167c620`: a rejected note-off no longer clears the held slot when the backend queue is full (stuck-note fix) with two regressions | clean | **Take** (cherry-pick) |
| `codex/luthier-pedalboard-timefx` | partial | `ee3d7c0d`: delay meter subnormal flush; `8dc4ca07`: exact LFO phase wrap; CPU work that conflicts | 31 conflicted paths | **Take** `ee3d7c0d` (manual port); evaluate `8dc4ca07`; leave the rest |
| `codex/luthier-pedalboard-host-midi` | partial | `5be9c0bc`: tap ignored under an external clock master; `8419a705`: clock status retry after feedback overflow; unwired mailbox and preview queue | 30 conflicted paths; the two fixes apply cleanly | **Take** the two fixes; leave the rest |
| `codex/luthier-rc5-tap-20261030` | unique, self-declared work in progress | looper tap tempo (15 commits), a new hook on a frozen pedal interface, spec errata, evidence logs | clean (31 paths) | **Trial merge with review** (see PROJECT_STATUS.md for the outcome) |
| `codex/luthier-ce2-evidence-20261028` | unique, documents only | three handoffs; ancestor of the tap-tempo branch | clean | comes with the tap-tempo branch if that is taken |
| `codex/luthier-ci-sharded-tests` | unique | opt-in exact test shards for the runner and CI | clean (3 paths) | **Review for CI** (see CI work) |
| `codex/luthier-pedalboard-release` | unique | a release QA workflow with packaging and inherited secrets; tests that fail by design while the looper is unregistered | conflict in one test file | Owner decision; not taken |
| `codex/luthier-pedalboard-state` | partial | looper history fixes `77f13e17`, `b1b86117`; a deferred looper model | 34 conflicted paths | Not taken; the two looper fixes need a hand port against the rewritten paged core |
| `codex/luthier-pedalboard-dsp` | unique, work in progress | the deferred drive models with failing gates | 25 conflicted paths | Keep the branch as the source for the deferred twelve; do not merge |
| `codex/luthier-preview-ABI-unqualified` | work in progress | preview command queue wired into the processor; the branch says it is ABI-unqualified | clean (12 paths) | Not taken |
| `codex/luthier-soundgrind-ci-20261104`, `codex/luthier-soundgrind-repair-20261030`, `codex/soundgrind-stress-audio-gui` | unique | see PRs #86, #84, #76 | clean | Owner decision / superseded |
| `codex/setup-autonomous-agent-20261104` | unique | see PR #87 | clean | Owner decision; not folded in |
| `claude/luthier-cloud-session-5lzlix` | unique | see PR #72 | conflict in `Source/PluginProcessor.cpp` | Owner decision |
| `docs/readme-and-wiki` | unique | see PR #78 | conflict in `README.md` | accurate parts reused |
| `claude/vigilant-babbage-2dwdec` | superseded code, unique research notes | an older board API; about 40 research documents | conflict in `Source/LuthierEngine.*` | Skip the code; research notes optional |
| `claude/peaceful-carson-vzl4e4` | superseded | reviewed by an earlier integrator (`docs/handoff/PEDALBOARD-2026-10-25-BUILD6.md`); remainder is duplicates and rejected exemptions | about 70 conflicted paths | Skip |
| `codex/luthier-pedalboard-engine`, `codex/luthier-pedalboard-ui-full` | superseded | counterparts exist in the baseline; only documents are missing | conflicted | Skip |
| `claude/intelligent-johnson-ruc6si`, `claude/luthier-micface-workshop-fix`, `codex/pro-beta-resonator-decay-ci` | superseded | - | - | Skip |
| `codex/luthier-release-provenance`, `codex/luthier-rc5-rhythm`, `codex/release-fix-resonator-string-decay`, `claude/luthier-codex63-integ`, `codex/luthier-beta-looper-concurrency`, `codex/luthier-licensing-core` | contained by content | patch-equivalent commits are in the baseline | merge result equals the baseline | Skip |

## Ownership notes found on branches

- `docs/handoff/PEDALBOARD-2026-10-28-CODEX-RESTART.md` (on `codex/luthier-ce2-evidence-20261028`):
  "Codex is the canonical source integrator… RC-5 source ownership is not yet confirmed."
- `RC5-TAP-2026-10-30-WIP.md` (on `codex/luthier-rc5-tap-20261030`): "Owner confirmed Claude has
  stopped Luthier work"; the branch calls itself "not ready to merge, register or release".
- The last push by any worker before this cycle was on 2026-11-04. This cycle works only on new
  branches (`integration/luthier-completion`, `agents/luthier/**`) and pushes to no existing branch.
