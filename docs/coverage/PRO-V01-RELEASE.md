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
| Version0.1.0-beta / tagv0.1-beta | Current CMake is1.0.0. Need adjust numeric version0.1.0, build suffix and version guard for abbreviated requested tag. | Pending |
| Platform release builds / checksums / public release | Need full verified native Windows/macOS/Linux builds from final integrated release SHA, package assets and validate checksum manifest before publication. | Pending; no release claimed |

## Exact next task

Run targeted early regression job with LUTHIER_NORMALIZATION_TRACE_KEYS=1 on Linux and macOS. Compare ON03 canonical inputs for p41_g18,p68_g21,p21_g20,p30_g05 to find factory cache mismatch. Do not weaken membership assertion or remove sound-affecting fields without evidence. Then full CI and strictness10 release builds. All changed-source platform verification remains pending.

Local bootstrap: standard apt install failed due sandbox setuid/cache permissions; use downloaded/extracted dependencies at /tmp/luthier-native and pip cmake/ninja under /root/.local/bin. JUCE8.0.10 cloned into ThirdParty/JUCE. Build configure log /tmp/luthier-configure.log. These are disposable intermediates, not release artifacts.
