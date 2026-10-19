# Luthier Pro v0.1 Beta release handoff

Updated 2026-10-18. User authorizes consolidation, Windows/macOS/Linux builds and publication as v0.1-beta. This supersedes older coordinator-only merges and platform holds in docs/HANDOFF.md and docs/helpers/EXTERNAL_AGENT_BRIEF.md.

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
| Factory normalization lookup | ON03 keeps membership and measured-drift assertions. The release-version metadata and non-gesture guitar-selection defects were fixed; the complete revision-3 reference was regenerated natively. | Table generated; cross-platform gate pending |
| Early CI diagnosis | build.yml runs ON03/CQ23/PracticeTrack first, retaining complete suite and validators. | Pending CI |
| Version0.1.0-beta / tagv0.1-beta | CMake numeric0.1.0, beta+SHA build identifier, abbreviated tag guard and changelog prepared. | Implemented; no tag created |
| Platform release builds / checksums / public release | pro-beta-release.yml runs all3 Pro strictness10/LTO builds only after this exact PR branch merges to master. It refuses existing tag/release, checks actual payload executable architectures, attaches SHA256SUMS/provenance, creates complete draft then publishes and re-downloads/checksums assets. | Workflow implemented; native builds/publication pending |

## Exact next task

Run the targeted ON03/metadata/instrument-selection regressions against the committed revision-3 table on Windows, Linux and macOS. Then complete full Pro CI and the strictness-10/LTO release builds. Keep PR75 draft until all native builds, tests and validators pass. Do not weaken membership assertions or measured-drift bounds.

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

## CI checkpoint 2026-10-17 (head 7d5e82be, workflow 39028881759)

Native Pro build CI failed on all three platforms. Windows ran 2,361 tests; 1 test case failed (four missing ON03 factory-table entries). Linux/macOS early regression tests showed exactly the same four missing memberships. CodeQL 39028881768 passed. Previous diagnostic fix in NormalizationTestUtil.h corrected a thread-local name-lifetime bug, but **did not** restore these four memberships. Do not claim the calibration bug is fixed.

Commits 73642dc and 2ae39b2 add a conditional **Linux-only factory table regeneration diagnostic** to the reusable build workflow. On release-branch PR validation failure it executes the existing native LuthierRender calibration generator into a separate file, prints old/new key coverage, and uploads the generated JSON and log to a workflow artifact without changing checked-in Resources/NormalizationFactory.json. This is intentionally diagnostic, not a gate bypass. Inspect its generated hashes, verify expected catalogue coverage and measurements, then decide whether to update the committed reference table and run all platform tests. Keep PR75 draft until the real native release gates pass.

## Investigation 2026-10-18 (starting at aea12a77)

PR75 remains draft; master remains ea50a2b5. Baseline native run 39099537500 still fails ON03 on Linux/macOS. New macOS output now matches Linux for p41_g18 but differs for the other three; p30_g05 changed between native runs. This is substantial instrument-state variability, not floating-point hash formatting.

Root-cause candidate traced: `ParameterBridge::writtenSinceGuitarType` preserves writes within 250 ms before a non-gesture guitar selection as accompanying host automation. Both `normtest::loadCombo` and `calibrateFactory` load a preset, then select the requested guitar without a gesture. Depending on elapsed loading time, the previous preset's tuning, body and setup can therefore survive the guitar selection. Linux canonical p21_g20 carries six strings instead of a J-style bass's four; p30_g05 carries five instead of the Offset's six. Existing `HostState::aHostWritingAGuitarTypeWithItsPartsKeepsTheParts` and `WorkshopPresets::choosingAGuitarTypeFitsItsParts` establish the intended host-vs-player semantics. New focused instrument-count regression is being built before correcting the two callers; no runtime fix verification claimed yet. Production host-restoration behavior must remain unchanged.

Additional provenance diagnostics in NormalizationTestUtil report requested recipe, selected file/revision/SHA256, first semantic difference from current generated JSON, and effective guitar/string count. Trace values greater than 1 also print both preset JSON documents.

The initial suspected generator cache-seeding defect was ruled out: RenderCli main already redirects factory loading to an empty temporary file before regeneration. No extra cache reset is needed. The old documented name-lifetime explanation is also unproven: indexOfPreset(const String&) constructs an owning String before lazy refresh when passed a const char*.

Verified tooling repair: regeneration shell script now renders to a temporary sibling and only replaces the committed reference on success. Failed build/render leaves the previous table intact and removes the temporary output. `python3 -m unittest discover -s scripts -p 'test_*.py'`: 14 tests passed, including 3 new tests covering both renderer layouts (6 scenarios); shell syntax/whitespace checks passed. The regression author observed failed-render overwrite on the original script before applying the correction.

Local native bootstrap uses extracted Ubuntu packages under /tmp/luthier-native, environment /tmp/luthier-env.sh, GCC13 Release Pro with LTO off, and disposable focused CMake source filter /tmp/luthier-focused-tests.cmake. Test/renderer build log: /tmp/luthier-build.log. Xvfb uses TCP display 127.0.0.1:78 because Unix sockets are unavailable in this execution environment. Local focused builds are diagnostic evidence only, not native release binaries.

### Confirmed native reproductions

`/tmp/luthier-normalization-red.log`: the new instrument-selection test failed 3/12 checks (p21_g20 six vs four strings; p30_g05 five vs six; p68_g21 six vs four). The four original ON03 memberships also failed. All selected preset files reported `generatedMatch=yes`, ruling out stale factory recipe JSON in this local reproduction.

Independently, retaining every byte of each original canonical input except changing nested `meta.version_created` and `meta.version_modified` from `0.1.0` to `1.0.0` recovers these exact committed factory entries (Python SHA256 matches production's original logged key):

| Combination | Original runtime key (miss) | Version-only 1.0.0 key (factory hit) |
| --- | --- | --- |
| p41_g18 | a96b056b04f16579d7016c4c6789b66a8a4a72c019b136e304c25faa14099c33 | 84a680e34fd5c483af6e8d739a0fb555382c3ec691f82c898f29da0c6563082e |
| p68_g21 | adb6fe306328614d649099ee29690b651dbcf427e37fd99a9b932cfd6d0579dd | 090dd6bc57a9d1c9af832ce5b9c78569b8e4c9543bfaa42a049ccb253f32b30a |
| p21_g20 | 0d6f2770340c3e36f0b96f149f569f96955e3e9621970ca69e7a475a30d257a2 | fbd965fafa1e0997fc5db246b0622571d0a42d68bd8ac200e3fdead7bcbce2e0 |
| p30_g05 | a66babc860cf0630949200e1d25aae4ea151912507aee668261ec909fd221f46 | fa4d5de06e842b5880cbbfb30f073492f1f7a795914608e2a91ed45882201f15 |

`stripPresetIdentity` removes legacy top-level metadata but missed the newer nested `meta` object. PresetManager writes that object's versions from JucePlugin_VersionString; its parser consumes the object only as descriptive metadata. This is the direct cause of all four Linux/Windows table misses after the release version change. The independent 250 ms selection issue explains platform/run-dependent guitar state. Both require correction; regenerating the table alone would preserve these defects.

### Verified repair checkpoint (2026-10-18)

Revision-3 native focused run `/tmp/luthier-v3-focused.log` passed **5 tests / 233 checks**: ON26_RolesAndHash, FactoryGuitarOverrideUsesSelectedInstrument, CalibrationHashIgnoresPresetMetadata, aHostWritingAGuitarTypeWithItsPartsKeepsTheParts, choosingAGuitarTypeFitsItsParts. The metadata-invariance regression previously failed 1/3 checks on revision 2 (`/tmp/luthier-metadata-red.log`). These are real GCC13 Release Pro tests, not structural/parser substitutes.

Implemented: exclude nested preset meta from canonical hashing; bump calibration revision to 3; explicit guitar-selection gestures in shared normalization fixture and calibration CLI; refuse incomplete generator output and report successful/expected/unique/duplicate/failure counts; safe temporary publication by regen shell; provenance diagnostics; early focused tests on Windows as well as Unix; CI shell-publication regression and revision validation against the source constant. No production DSP, host-write timing, activation checks or parameter layout changed.

At this source checkpoint the committed factory resource is **still revision 2**, so ON03 remains expected red until a complete measured revision-3 table is reviewed and committed. Factory generation and corrected off-path goldens are the exact next actions. Full native platform verification, merge/tag/publication remain blocked; PR75 must stay draft. Do not reinterpret the five focused passes as release readiness.


## Calibration refresh checkpoint (2026-10-18)

The temporary Linux Pro calibration refresh workflow ran its native `LuthierRender` generator and completed **1,820/1,820 calibration cases**, producing **1,750 unique revision-3 keys with zero failures**. It confirmed the affected release-version cases were included in the generated table, then committed only `Resources/NormalizationFactory.json` as `eb134fcac5142292065b7ee1e62f313750f3b1f3`. The table currently reads revision 3, edition `standard`, 1,750 entries. The metadata and instrument-selection defect fixes have focused local regression evidence recorded above.

This refresh establishes that a complete measured revision-3 table is now present; it does **not** establish that ON03, full unit tests, plugin validators, or strictness-10/LTO release builds pass on every platform. The latest PR verification and CodeQL runs (39118037835 and 39118037785, head `eb134fc`) ended `action_required` with no jobs returned, so they produced no new native verification. Re-run/approve CI through repository Actions controls if necessary, then record the final SHA and per-platform results here. Keep the PR draft and do not publish until those gates pass.

The temporary `.github/workflows/refresh-pro-calibration.yml` did its one-time purpose and has been removed after committing the measured table. If another table change is required, use the checked-in safe regeneration script and review its complete output before committing.
