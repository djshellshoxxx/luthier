# Luthier project status

Canonical status for the completion cycle that started on 2026-11-11. Updated after each
integration step. Companion documents: [FEATURE_INVENTORY.md](FEATURE_INVENTORY.md),
[BRANCH_PR_LEDGER.md](BRANCH_PR_LEDGER.md), [CONTINUATION.md](CONTINUATION.md).

Rule for this file: every figure names what was counted and where the evidence is. "Implemented"
means code exists. "Validated" means a named test or validator run proves it on a named platform.
Nothing here is an independent professional audit.

## Where things stand

| Metric | Value | Evidence |
|---|---|---|
| Integration branch | `integration/luthier-completion`, draft PR #89 into `master` | GitHub |
| Baseline | `ca14934e` (head of #85), 342 commits ahead of `master` | `git rev-list --count` |
| Unit tests | 2,923 tests in 367 suites | `LuthierTests --list` |
| Linux x64, baseline | 1 failing test per edition in CI (`RefPedalCe2.bbdOnOriginalThdGate`); reproduced locally with the same numbers | CI run 41796728348; local run on 2026-11-11 |
| Windows x64, baseline | 2 failing tests per edition | same CI run |
| macOS, baseline | not qualified: the Free unit run produced no summary line after 189 minutes; several named failures | same CI run |
| Platform cells qualified for release | 0 of 4 (Windows x64, macOS Intel, macOS ARM, Linux x64) | `docs/handoff/LUTHIER-CURRENT-COMPLETION-2026-10-27.md`; nothing newer contradicts it |
| Real hosts (DAWs) validated | 0 | `docs/beta/HOST_TEST_SHEET.md` is blank |
| Named pedalboard models registered | 0 of 6 | `Source/ReferencePedals/Models/Register*.cpp` (empty registration functions) |
| Pedalboard models available to users | 13 (10 generic, 3 prototypes) | `PedalRegistry` registrations |
| Owner-reported defects (`spec/issues.md`, 11 items) | 8 fixed with tests, 1 fixed without a test (illustration, subjective), 2 partly open (Reset & Stop control; MOD/RHYTHM tab-height tests missing) | `docs/status/FEATURE_INVENTORY.md`, section A |
| Security findings on untrusted input | 22 (2 high, 10 medium, 10 low); fixes in progress | independent read-only review, 2026-11-11 |
| Real-time safety findings | 9 (1 critical, 2 high, 4 medium, 2 low); fixes in progress | independent read-only review, 2026-11-11 |

## Completion, stated as counts

No single percentage is meaningful for this project, so none is given. The countable facts:

- **Specification rows** (`docs/spec-coverage.md`, audited 2026-09-23, known to under-report):
  673 of 1,989 verified (34 %); a further 757 implemented or partial; 502 pending; 28 blocked.
- **Tests**: 2,922 of 2,923 pass on Linux at the baseline.
- **Pedalboard release scope**: 0 of 6 named models registered; an earlier integrator estimated
  the six-pedal release at 55-65 % complete overall on 2026-10-27, with low-to-medium confidence
  and before the looper wrapper existed. That estimate was not re-derived here.
- **Platforms**: 0 of 4 cells qualified.

## Work in this cycle

| Workstream | Branch | State | Result |
|---|---|---|---|
| Documentation refresh | `agents/luthier/docs/refresh` | merged (PR #88) | 10 commits, documentation only |
| Chorus BBD distortion gate | `agents/luthier/dsp/ce2-bbd-thd` | in progress | - |
| Full-processor 30 s stress test | `agents/luthier/testing/processor-stress` | in progress | - |
| Legacy effects-chain use-after-free | `agents/luthier/audio/effects-chain-retire` | in progress | - |
| MIDI continuity in the audio callback | `agents/luthier/audio/midi-continuity` | in progress | - |
| Latency reporting off the audio thread; constant latency under board bypass | `agents/luthier/audio/latency-reporting` | in progress | - |
| Click-free cabinet and body impulse-response swap | `agents/luthier/audio/ir-swap-crossfade` | in progress | - |
| Session recorder sample rate and reset race | `agents/luthier/audio/session-recorder` | in progress | - |
| GUI: column scrolling, scrollbars, collapsible sections | `agents/luthier/gui/columns-scroll-collapse` | in progress | - |
| GUI: screen clamp, pedalboard at real sizes, layout integrity test | `agents/luthier/gui/sizing-board` | in progress | - |
| GUI: Reset & Stop, Workshop guitar types, focus ring, LIVE pads | `agents/luthier/gui/controls-focus-reset` | in progress | - |
| Security: JSON/XML depth and size guards | `agents/luthier/security/parse-depth-guards` | in progress | - |
| Security: path confinement, audio-file caps | `agents/luthier/security/file-asset-hardening` | in progress | - |
| Security: NaN, update check, crash handler | `agents/luthier/security/nan-update-crash` | in progress | - |
| Small fixes ported from unmerged branches | `agents/luthier/integration/cherry-picks` | in progress | - |
| Looper tap-tempo branch: trial merge and review | `agents/luthier/integration/rc5-tap` | in progress | - |
| Windows-only UI lifecycle failure | `agents/luthier/platform/windows-ui-lifecycle` | in progress | - |
| CI speed and safety | `agents/luthier/ci/speed` | in progress | - |

## Decisions that are the owner's to make

These block specific gates. They were found in the repository's own records; none was decided here.

1. **CPU reference machine** for the per-pedal CPU budgets (phaser, both choruses, delay,
   compressor fail them on a 2.1 GHz Xeon).
2. **Chorus BBD-on distortion threshold** if no physically defensible model meets it
   (see the chorus workstream's handoff when it lands).
3. **Chorus bypass raw-jump definition**, **two-mode chorus THD output and depth**, **compressor
   attack-time definition and maximum-sensitivity distortion**, **phaser 24-bit bypass click
   criterion**.
4. **Looper memory switching**: needs a message-thread restore hook on an interface the project
   treats as frozen.
5. **Licensing**: the Pro edition has no runtime licence check; the signed-licence code exists but
   is not wired in. Recorded as deferred by the owner on 2026-10-06.
6. **Pull requests #72 (trial lock), #86/#84 (repeat-runner), #87 (autonomous agent workflow)**:
   see the ledger.
7. **Pull request #77**: close unmerged, or remove the beta 2 publisher trigger, before any merge
   of this branch to `master`.
8. **Signing and notarisation** certificates; real-host and listening tests.

## What cannot be done from a Linux container

- Running or debugging on Windows or macOS (only GitHub Actions output is available).
- Audio-device tests, real DAW hosts, listening tests, hardware MIDI controllers.
- Signing, notarisation, and anything that needs credentials.
