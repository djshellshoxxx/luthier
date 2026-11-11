# Luthier project status: PAUSED

**Paused on 2026-11-11 by the owner.** The instruction ("stop working on all projects except
circula, classrupted, ghostpay, p2p offramp [and patchtrace]; have the workers finish the work they
are working on and then update to github and then stop") was given at 01:00 UTC in the owner's
project-prioritisation session and relayed to this cycle's coordinator. All workers were stopped,
everything was committed and pushed, and nothing further is running.

Nothing was merged to `master`. No release or tag was created. No test or threshold was weakened.

Companion documents: [FEATURE_INVENTORY.md](FEATURE_INVENTORY.md),
[BRANCH_PR_LEDGER.md](BRANCH_PR_LEDGER.md), [CONTINUATION.md](CONTINUATION.md),
the audit reports in [discovery-2026-11-11/](discovery-2026-11-11/), three worker write-ups in
[handoff-2026-11-11/](handoff-2026-11-11/), and six baseline screenshots in
[screenshots-2026-11-11/](screenshots-2026-11-11/).

Rule for this file: every figure names what was counted and where the evidence is. "Implemented"
means code exists. "Validated" means a named test or validator run proves it on a named platform.
Nothing here is an independent professional audit.

## What is on GitHub

| PR | Branch | State |
|---|---|---|
| #89 | `integration/luthier-completion` into `master` | Draft, **do not merge**. Baseline `ca14934e` (head of #85) plus the documentation refresh, these status documents and the worker scripts. No product code changed |
| #88 | `agents/luthier/docs/refresh` | **Merged** into the integration branch. Documentation only |
| #90 | `agents/luthier/integration/cherry-picks` | Parked. Stuck-note fix ported and verified (fails before, passes after); four other small ports not done |
| #91 | `agents/luthier/integration/rc5-tap` | Parked. Trial merge of the looper tap-tempo branch: builds, no new failures in the suites run; coordinator review and full suite not done |
| #92 | `agents/luthier/gui/columns-scroll-collapse` | Parked WIP. Tests and uncompiled drafts only |
| #93 | `agents/luthier/gui/sizing-board` | Parked WIP. Screenshot sweep test only |
| #94 | `agents/luthier/gui/controls-focus-reset` | Parked WIP. One unbuilt screenshot test; nothing implemented |
| #95 | `agents/luthier/testing/processor-stress` | Parked WIP. 30-second full-processor stress test written, never built |
| #96 | `agents/luthier/ci/speed` | Parked WIP. Workflow restructure validated locally (YAML, `needs`, shard arithmetic), never run on GitHub |
| #97 | `agents/luthier/audio/session-recorder` | Parked WIP. Unbuilt tests, no fix |
| #98 | `agents/luthier/audio/effects-chain-retire` | Parked WIP. Unbuilt stress tests for the rack-pedal use-after-free, no fix |
| #99 | `agents/luthier/audio/midi-continuity` | Parked WIP. Unbuilt test, no fix |
| #100 | `agents/luthier/dsp/ce2-bbd-thd` | Parked WIP. Root cause of the chorus distortion gate measured; model change written, never built |

Every parked branch ends in a commit that carries `[skip ci]`, so opening those pull requests
started no CI runs. Each pull-request description states exactly what was and was not verified.

**Not on GitHub, on purpose:** the security review of untrusted-input handling and three unfinished
security branches. This repository is public and they describe unfixed defects, so they were
delivered to the owner as files attached to the coordinating session instead (an audit report,
three patch files on base `ca14934e`, and a small archive of drafts, with a README).

## What the build and the discovery produced

- **Local Linux x64 build** of the test binary at `ca14934e` with clang 18 and JUCE 8.0.10:
  844 objects, about 15 minutes on 4 cores. `LuthierTests --list` reports 2,923 tests in 367 suites.
- **The one known Linux failure reproduces locally with the same numbers as CI**:
  `RefPedalCe2.bbdOnOriginalThdGate` (0.0017 % at 48 kHz against a required 0.5 to 3 %).
  `RefPedalCe2`: 1 of 13 tests failed, 5 of 637 checks.
- **Focused local runs that passed**: `OutputNormalization`, `Looper` and `Notation` together
  (125 tests, 46,264 checks); a worker's run of every `RefPedal` suite (526 of 527 passed, the one
  failure being the gate above) and of `HostState StateModel Parameters Trademarks` (73 of 73);
  the documentation-reading suites after the documentation merge.
- **Ten read-only audits**, all kept in `discovery-2026-11-11/` except the security one:
  open pull requests, unmerged branches, pedalboard inventory, core product inventory, GUI,
  CI, real-time safety, stress testing, documentation, and security (private).
- **Worker tooling** in `scripts/agents/`: a fresh worktree builds the test binary in about
  38 seconds with a 98.8 % compiler-cache hit rate.

## Started but not finished

See each pull request above. In order of value when work resumes:

1. **Rack-pedal use-after-free** (#98). Confirmed by reading the code: the message thread frees a
   rack pedal that four unlocked audio-thread paths may still call. Not yet reproduced or fixed.
2. **Chorus distortion gate** (#100). The gate is measured at the pedal output, where a comb notch
   next to the 1 kHz test tone makes output THD 3.5 to 4.3 times the wet-path THD. The gate therefore
   needs only 0.14 to 0.70 % in the wet path, not the figure an earlier review compared against.
   Whether a datasheet-typical curve passes depends on a supply-voltage scaling that is not verified.
3. **Stuck-note fix** (#90): done and verified in isolation; needs the full suite and a merge.
4. **MIDI continuity** (#99): rhythm MIDI output written twice and replayed after switch-off;
   MIDI dropped when the engine lock is contended. The write-up lists three further causes of stuck
   notes at the host that a simple fix would miss.
5. **Full-processor 30-second stress test** (#95): written, never built.
6. **CI restructure** (#96): one pull-request push costs about 263 minutes today.
7. **GUI** (#92, #93, #94) and **session recorder** (#97).
8. **Looper tap-tempo trial merge** (#91).
9. **Security hardening**: private patches, see above.

## Never started or never completed

- **A complete local run of the full test suite.** Two attempts were invalidated by set-up
  mistakes in this cycle (tests started from the wrong directory under the shared compiler cache;
  then a build lock held by a stray display-server process) and the third was stopped by the pause.
  The pitfalls are recorded in `scripts/agents/README.md`.
- Building the plugin formats (VST3, CLAP, Standalone) and the offline renderer locally, and running
  pluginval and clap-validator. Both validator downloads were confirmed reachable.
- Performance benchmarks. Any real-host test.
- Independent review of the worker branches.
- Two real-time review items whose workers were stopped before doing anything: moving
  `setLatencySamples` off the audio thread and keeping reported latency constant under pedalboard
  bypass; a click-free cabinet and body impulse-response swap. Both are described in
  `discovery-2026-11-11/rt-audit.md`.
- macOS failure triage. Tracking issues for the owner decisions. Publishing wiki pages (the wiki
  repository was not reachable from the build environment; drafts are in `docs/wiki-draft/`).

## Known failures and CI state

| Platform | State at `ca14934e` | Evidence |
|---|---|---|
| Linux x64 | 1 failing test per edition: `RefPedalCe2.bbdOnOriginalThdGate` | CI run 41796728348 (2026-11-04); reproduced locally |
| Windows x64 | 2 failing tests per edition: the same gate, and `RefPedalUiLifecycle.tenThousandNativeOperationsAndEditorDestruction` | same CI run |
| macOS universal | not qualified: the Free unit run ended with no summary line after 189 minutes; named failures in `HostState`, `Fuzz` convolution, `RefPedalControlRace` | same CI run |

- **CI on #89**: the runs started by opening the pull request sat in the queue and were cancelled
  when the project was paused, so there is no CI result for the integration head. Its code is the
  baseline's; only documents and scripts differ.
- **Windows-only failure, narrowed but not fixed**: a Linux run reproduces the same seeds, instance
  counts and rejection counts as the Windows log, which rules out library random-number differences
  and machine-dependent admission. The likely cause is that Ctrl+D posts the duplicate through an
  asynchronous call that the test's message drain did not run before checking the count on Windows.
  See `handoff-2026-11-11/windows-ui-lifecycle.md`.
- **Intermittent crash, pre-existing**: the seeded looper gesture stress test segfaulted in 2 of 5
  runs on the baseline binary (and 1 of 5 on the tap-tempo merge). Not investigated. It may be
  related to macOS unit runs that end without a summary line.
- **A test rewrites a tracked file**: running the suite modifies `docs/review/QA_ROBUSTNESS.md` in
  the source tree. Discard that change; do not commit it.

## Counts

No single completion percentage is meaningful for this project, so none is given.

| Metric | Value | Evidence |
|---|---|---|
| Specification rows verified | 673 of 1,989 (34 %); 757 more implemented or partial; 502 pending; 28 blocked | `docs/spec-coverage.md`, audited 2026-09-23 and known to under-report |
| Linux tests passing at the baseline | 2,922 of 2,923 | CI run 41796728348 |
| Named pedalboard models registered | 0 of 6 | empty registration functions in `Source/ReferencePedals/Models/Register*.cpp` |
| Pedalboard models available to users | 13 (10 generic, 3 prototypes) | registry |
| Platform cells qualified for release | 0 of 4 | `docs/handoff/LUTHIER-CURRENT-COMPLETION-2026-10-27.md`; nothing newer contradicts it |
| Real hosts validated | 0 | `docs/beta/HOST_TEST_SHEET.md` is blank |
| Owner-reported defects (`spec/issues.md`, 11) | 8 fixed with tests; 1 fixed without a test; 2 partly open | `FEATURE_INVENTORY.md` |
| Real-time review findings | 9 (1 critical, 2 high, 4 medium, 2 low); 0 fixed | `discovery-2026-11-11/rt-audit.md` |
| Security review findings | 22 (2 high, 10 medium, 10 low); 0 fixed | private report |

An earlier integrator estimated the six-pedal release at 55 to 65 % complete on 2026-10-27, with
low-to-medium confidence. That estimate was not re-derived here.

## Decisions that are the owner's

Found in the repository's own records; none was decided in this cycle.

1. **CPU reference machine** for the per-pedal CPU budgets.
2. **Chorus distortion gate**: accept a datasheet-based model with a supply-voltage scaling, or
   revise the estimated threshold (see #100).
3. Chorus bypass raw-jump definition; two-mode chorus THD output and depth; compressor attack-time
   definition and maximum-sensitivity distortion; phaser 24-bit bypass click criterion.
4. **Looper memory switching**: a message-thread restore hook on an interface treated as frozen.
5. **Licensing rollout**, deferred on 2026-10-06, and the related review findings in the private report.
6. Pull requests #72, #84/#86 and #87: see the ledger.
7. **Pull request #77**: close it unmerged, or remove the beta 2 publisher trigger, before any merge
   of the integration branch to `master` (see the ledger).
8. Whether to make the repository private or prioritise the security fixes before publishing more
   prerelease builds.
9. Signing and notarisation certificates; real-host and listening tests.

## What cannot be done from a Linux container

Running or debugging on Windows or macOS (only GitHub Actions output is available); audio-device
tests, real hosts, listening tests, hardware controllers; signing and notarisation.
