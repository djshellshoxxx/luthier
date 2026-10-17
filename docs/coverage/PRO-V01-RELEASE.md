# Luthier Pro v0.1 Beta release handoff

Updated 2026-10-17. User authorizes consolidation, Windows/macOS/Linux builds and publication as v0.1-beta. This supersedes older coordinator-only merges and platform holds in docs/HANDOFF.md and docs/helpers/EXTERNAL_AGENT_BRIEF.md.

## Authoritative state

`master` ea50a2b50b9901eed3d9914f03a804acb7ac2446 contains consolidated work through PR71. Work branch: `codex/luthier-pro-v01-release`. PR74 head a0afbf89 is included as an ancestor, preserving its edition-filter implementation.

No root AGENTS.md or uppercase CLAUDE.md exists; lowercase claude.md references absent specs/ (actual specifications are spec/). Read README, docs/RELEASING.md, docs/HANDOFF.md, external-agent brief and relevant specs before continuation.

## Branch disposition

All remote branches are ancestors of master except these eight (checked git for-each-ref --no-merged):

| Branch / PR | Disposition |
| --- | --- |
| claude/pensive-gauss-gqqh33 / PR74 | Relevant edition selection; included in release branch. Integration pending repaired macOS tests. |
| claude/vigilant-babbage-2dwdec / PR72 | Beta2 trial-lock feature and unfinished feedback batch; exclude from full Pro beta without expiry. Preserve branch. |
| codex/reference-pedals-research-and-foundation / PR73 | Explicit experimental draft with unverified real-time/state/model behavior; exclude. |
| claude/luthier-cloud-session-5lzlix | Contains beta2 trial-lock commit; exclude from this release. |
| claude/luthier-codex63-integ | Old merge-only integration commit; substantive source already consolidated. Do not remerge a historical tree. |
| claude/luthier-micface-workshop-fix | Historical mic layout test adjustment; current master supersedes its parent integration. Review focused delta if current tests fail. |
| codex/luthier-beta-looper-concurrency | One residual source-check test fix commit; substantive looper work consolidated in PR67. Review against current tests before applying. |
| codex/luthier-licensing-core | Residual SHA256 include fix commit; licensing core already consolidated and Windows/Linux Pro compile green. Verify current include before applying. |

No branches deleted or shared history rewritten. No reliable API exists to prove other agents idle; avoid their active PR72/73 branches.

## Checklist and evidence

| Requirement | Implementation / evidence | Status |
| --- | --- | --- |
| Pro edition configuration | cmake/Editions.cmake PAID sets LUTHIER_IS_PRO=1, Pro name and Lthr ID; Source/Edition.h enables feature set. Source/Support/Edition.cpp uses compile-time authority; no beta2 trial gate merged. | Inspected |
| Existing release ownership | Draft release408273616 is v1.0.0-beta1 with Windows/Linux assets, no macOS; do not overwrite or rename those binaries. No git tags currently listed. | Inspected |
| Previous platform verification | PR74 run38824640225: Windows/Linux Free+Pro builds, unit suites and validators pass; CodeQL38824640217 passes. macOS Free+Pro compile and validators pass; unit suites fail. | Recorded, not evidence for changed sources |
| Backing tempo regression | BackingTrackPlayer readiness API; tests wait for actual buffered input instead of400ms sleep. Actual-player probe24/24 duration/pitch cases passes (0.5x3.819s439.998Hz;1.5x1.301s439.980Hz). | Focused verified; full platform tests pending |
| Low-mode repaint accounting | Count fixed glow/fret/slide state changes separately from chord changes; per-component budgets, zero animation timer/readout limits retained; new idle-poll regression. | Implemented; runtime pending |
| Factory normalization lookup | ON03 uses same four failing Mac cases on all Pro platforms, logs canonical keys on miss (opt-in on hit). std::shuffle chose different samples per STL, so previous green Linux tested different states. Membership/drift assertions retained. | Root mismatch unresolved; release blocker |
| Early CI diagnosis | build.yml runs ON03/CQ23/PracticeTrack first, retaining complete suite and validators. | Pending CI |
| Version0.1.0-beta / tagv0.1-beta | CMake numeric0.1.0, beta+SHA build identifier, abbreviated tag guard and changelog prepared. | Implemented; no tag created |
| Platform release builds / checksums / public release | pro-beta-release.yml runs all3 Pro strictness10/LTO builds only after this exact PR branch merges to master. It refuses existing tag/release, checks actual payload executable architectures, attaches SHA256SUMS/provenance, creates complete draft then publishes and re-downloads/checksums assets. | Workflow implemented; native builds/publication pending |

## Exact next task

Run targeted early regression job with LUTHIER_NORMALIZATION_TRACE_KEYS=1 on Linux and macOS. Compare ON03 canonical inputs for p41_g18,p68_g21,p21_g20,p30_g05 to find factory cache mismatch. Do not weaken membership assertion or remove sound-affecting fields without evidence. Then full CI and strictness10 release builds. All changed-source platform verification remains pending.

Local bootstrap: standard apt install failed due sandbox setuid/cache permissions; use downloaded/extracted dependencies at /tmp/luthier-native and pip cmake/ninja under /root/.local/bin. JUCE8.0.10 cloned into ThirdParty/JUCE. Build configure log /tmp/luthier-configure.log. These are disposable intermediates, not release artifacts.

## Prepared publication and verification

PR75: https://github.com/djshellshoxxx/luthier/pull/75. Initial checkpoint commit8527de19237f811f3072512cce772730c0d50ae8 started CI39011054986 and CodeQL39011054988; later updates supersede that run. The release workflow is intentionally gated on merging this specific same-repository PR branch and successful native builds. Do not merge before the normalization blocker and all checks pass. Old PR74 is included in ancestry; close/classify it only once this branch integrates.

Source for residual branch dispositions: `PRO-V01-BRANCH-AUDIT.md` (exact equivalence evidence). Prepared Python release-asset gate has11 passing structural tests; actionlint passes the modified workflows. Those synthetic archive tests are parser tests, not native binary verification. Actual-player readiness probe24/24 passed. Complete native verification remains pending.

Local focused build compiles production engine/UI with TestMain, NormalizationTests, QualityModeUiTests and SweepDsp2Tests only, using a disposable deferred CMake source filter `/tmp/luthier-focused-tests.cmake`. It is for diagnosis; CI retains the complete suite. Local CMake cache was configured before the numeric version update; a future complete build must reconfigure without `CMAKE_PROJECT_Luthier_INCLUDE` and restore normal source set.

## CI verification checkpoint (2026-10-17, PR75 head baac04ab9b3dec1d077dbe69a2be14160e597b3b)

Workflow [39011447064](https://github.com/djshellshoxxx/luthier/actions/runs/39011447064) completed **failure** on all three native Pro platform jobs; CodeQL [39011447050](https://github.com/djshellshoxxx/luthier/actions/runs/39011447050) completed **success**. The Windows job 117334768774 compiled the three primary targets and executed its complete unit suite: 2,361 tests, 2,635,906 checks, one failed case `ON03_FactoryTableDoesNotDrift` (four failed assertions). Linux job 117334768797 and macOS job 117334768793 also compiled targets, then failed the targeted early regression stage with the same test case (4/613 failed checks). These are **not** release-ready passing builds.

The ON03 failure occurs at the factory-table membership assertion `CHECK_MSG(hit, c.key() + " is not in the factory table")` for the four explicit Pro combinations `p41_g18`, `p68_g21`, `p21_g20`, `p30_g05`. Canonical state and calibration hash are printed in native run logs when `LUTHIER_NORMALIZATION_TRACE_KEYS=1`. The likely class of defect is hash/key inconsistency between shipped `NormalizationFactory.json` and live `captureSoundState()`; this is **not yet proven** to be the exact cause. Compare factory entry hashes and canonical generation inputs before regenerating the table, changing the revision, or changing quantization. Preserve Pro membership assertions and audible-drift bounds.

Next: reproduce the four keys and inspect factory table coverage and key provenance; repair the root state/hash discrepancy; rerun targeted ON03 on all three platforms, followed by full unit/validator CI. Keep PR75 draft and do not tag/publish until green. No merge or public release was performed in this checkpoint.

### Follow-up normalization investigation (2026-10-17)

Read the committed `Resources/NormalizationFactory.json`: revision 2, edition `standard`, exactly 1,750 entries. Matched all four Linux runtime hashes from run 39011447064 against its entry keys; **none are present**. Linux and Windows produce identical hashes for all four cases; macOS produces differing hashes for three (p41_g18, p68_g21, p21_g20), while p30_g05 agrees across all three. Comparing full canonical inputs reveals substantial *preset-parameter and string-layout* differences on macOS (e.g. six strings versus four), not mere floating-point rounding. These findings warrant tracing preset resolution/loading and factory-table generation; do not treat them as a hash-algorithm rounding problem or regenerate the JSON without verifying correct inputs.

Committed focused ON03 test instrumentation: assert `loadCombo` succeeds and identify a failing factory preset load directly before classifying it as cache drift. This preserves the required factory-membership and measured-LU assertions for successfully loaded combinations; the test change is **not** a fix to the missing factory data. Next inspect CI diagnostics on the new commit, validate platform-specific preset loading, then repair the underlying load/generation discrepancy and run the full native jobs. No release publish.
