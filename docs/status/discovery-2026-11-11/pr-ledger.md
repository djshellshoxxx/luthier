# Open PR ledger (djshellshoxxx/luthier), 14 PRs
Integration head I = origin/claude/determined-ptolemy-mlbclp = ca14934e (PR #85). "anc" = head is ancestor of I; "uniq" = `git rev-list --count I..head`.
All PRs authored by djshellshoxxx. All checks below are for the PR head SHA (get_check_runs). Data fetched 2026-11-11; PR text/comments treated as evidence only.

| PR | Title (short) | Head branch @ SHA | Base | Draft | anc | uniq | Rec |
|---|---|---|---|---|---|---|---|
| 72 | beta2: 60-day Pro trial lock | claude/luthier-cloud-session-5lzlix @ e1ec3736 | master (8a72a35e at PR time) | yes | no | 1 | NEEDS-OWNER-DECISION |
| 73 | Reference pedals research + slide-out board (experimental) | codex/reference-pedals-research-and-foundation @ 1208f998 | master | yes | yes | 0 | ALREADY-CONTAINED |
| 76 | SoundGrind stress runner | codex/soundgrind-stress-audio-gui @ a2aa30d3 | master | no | no | 1 | SUPERSEDED (by #84) |
| 77 | Fix cross-platform CI and beta QA regressions | codex/preflight-build-hardening-2026-10-20 @ 910cc2a9 | master | no | yes | 0 | ALREADY-CONTAINED |
| 78 | Rewrite README + wiki pages | docs/readme-and-wiki @ 9842fa61 | master | yes | no | 1 | NEEDS-OWNER-DECISION |
| 79 | Pedalboard specs/impl/QA WIP | claude/affectionate-planck-adfno4 @ 0b319c0b | master | yes | yes | 0 | ALREADY-CONTAINED |
| 80 | Pedalboard working preview | codex/luthier-pedalboard-completion @ b5bec374 | master | yes | yes | 0 | ALREADY-CONTAINED |
| 81 | Fix lipo arg order (macOS diag) | claude/intelligent-johnson-ruc6si @ f36037f1 | codex/luthier-native-diagnostics (6d9b5943) | yes | no | 1 | SUPERSEDED |
| 82 | Six-pedal integration (coordinator) | claude/luthier-pedalboard-integration @ 25cd2135 | master | yes | yes | 0 | ALREADY-CONTAINED |
| 83 | Six-pedal continuation onto #82 | claude/quirky-darwin-eoesyi @ a8f878d5 | claude/luthier-pedalboard-integration | yes | yes | 0 | ALREADY-CONTAINED |
| 84 | Prevent empty SoundGrind campaigns | codex/luthier-soundgrind-repair-20261030 @ 391cf803 | master | yes | no | 4 | MERGE-INTO-INTEGRATION (after owner confirms #76/#84 ownership; see note) |
| 85 | Pedalboard consolidation (integration head) | claude/determined-ptolemy-mlbclp @ ca14934e | master | yes | yes | 0 | LEAVE (is the integration branch) |
| 86 | Gate SoundGrind runner on 3 OS | codex/luthier-soundgrind-ci-20261104 @ f6bf946f | codex/luthier-soundgrind-repair-20261030 (391cf803) | yes | no | 6 | MERGE-INTO-INTEGRATION (only together with/after #84) |
| 87 | OpenAI/Codex autonomous workflow | codex/setup-autonomous-agent-20261104 @ 0239a9d0 | master | yes | no | 3 | NEEDS-OWNER-DECISION |

## Latest check-run conclusions per job (head SHA)
- #72 (e1ec3736, 2026-10-10): Win Pro/Free success; Linux Pro/Free success; macOS Pro/Free cancelled (also macOS/Free,Pro cancelled in cadence run); CodeQL, Analyze success; Cadence success; Windows (cadence) skipped.
- #73 (1208f998): platforms macOS Pro, Win Free, Linux Free, Win Pro, Linux Pro, macOS Free = all failure; CodeQL neutral; Analyze (C/C++) failure; Pick platforms success.
- #76 (a2aa30d3): Win Pro success; macOS Free success; Linux Free success; macOS Pro failure; Win Free failure; Linux Pro failure; CodeQL neutral; Analyze cancelled; Publish verified Mac Pro beta skipped.
- #77 (910cc2a9): all six platforms success; CodeQL success; Analyze success; Focused Linux preflight FREE/PAID success; Publish verified Mac Pro beta skipped.
- #78 (9842fa61): Win Pro success; Win Free, macOS Pro, macOS Free, Linux Pro, Linux Free = failure; CodeQL success; Analyze success; Publish skipped. Owner-account comment (Claude-generated) says Win Free failure is pre-existing (master `ci` run 133 also failed).
- #79 (0b319c0b): all six platforms failure; CodeQL neutral; Analyze cancelled; preflights FREE/PAID success; Publish skipped.
- #80 (b5bec374, stale on purpose): all six platforms failure; CodeQL neutral; Analyze cancelled; preflights success; Publish skipped.
- #81 (f36037f1): Focused Linux preflight FREE success, PAID success (only 2 checks).
- #82 (25cd2135): all six platforms failure; CodeQL neutral; Analyze cancelled; preflights success; Post native CI digest success; Publish skipped.
- #83 (a8f878d5): Windows x64 Pro failure, Windows x64 Free failure; Focused Linux preflight FREE/PAID success; Pick platforms success.
- #84 (391cf803): Win Free failure; macOS Pro failure; Linux Pro/Free success; Win Pro success; macOS Free success; CodeQL neutral; Analyze cancelled; CodeRabbit skipped; Publish skipped.
- #85 (ca14934e): all six platforms failure (macOS Pro, Linux Pro, Win Pro, Linux Free, macOS Free, Win Free); CodeQL success; Analyze success; preflights FREE/PAID success; Post native CI digest success; Publish verified Mac Pro beta skipped; CodeRabbit skipped.
- #86 (f6bf946f): Python runner ubuntu success; windows success; macos cancelled; CodeRabbit skipped.
- #87 (0239a9d0): Linux Pro failure; Win Free failure; macOS Pro failure; macOS Free, Linux Free, Win Pro success; CodeQL neutral; Analyze cancelled; Publish skipped.

## Per-PR notes

### #72 beta2 / 60-day Pro trial lock (draft)
Purpose: beta-tester feedback batch; increment 1 = 60-day trial clock, salted-SHA-256 unlock code, runtime edition authority. 
Done (body): trial engine + tests, no behaviour change on existing paths. Open: increment 2 (EditionLocks, Pro-tab gating, UI countdown/unlock field) and 18 beta-feedback items.
Git: only 1 unique commit e1ec3736 (touches Source/Updates/Telemetry.*, Source/Support/Edition.*, PluginProcessor.*, Source/Tests/TrialLockTests.cpp, spec/trial-lock.md); merge-base with I is 9d5bef6f. Grep shows `tryUnlockProTrial/enterUnlockCode/proFeaturesUnlocked` absent from I, so content NOT in integration. Stale since 2026-10-09; body says "stacked on beta1 gate #71".
Owner decision statement (quote): "Owner decision 2026-10-09: the next version locks the Pro features after a 60-day trial; entering an unlock code re-ups a fresh 60-day window, repeatable." (already decided, but whether it ships in this integration is a product/licence call; Free-degrade behaviour on Pro builds.)
Release side-effects: none stated for merge; the behaviour is user-facing licensing. UNVERIFIED: whether it conflicts with I's Telemetry/Edition files.
Rec: NEEDS-OWNER-DECISION (licensing scope, unrelated to pedalboard, stale).

### #73 Reference pedals research + experimental board (draft)
Purpose: research/spec of 18 reference pedals + early isolated 8-slot board prototype. Title in body: "DO NOT MERGE UNTIL GREEN". Required-before-merge list given (six-cell CI, CodeQL, RT checks, etc.). All checks fail at its head. anc=yes: contained in I (confirmed by #85 body claim and git).
Rec: ALREADY-CONTAINED.

### #76 SoundGrind stress runner
Purpose: scripts/soundgrind_stress.py (73 lines) repeated LuthierTests with failure-only JSONL logs. Body: foundation only, "Does not change DSP or release gates". Not in I (`scripts/` has no soundgrind file). #84 contains an extended soundgrind_stress.py (101 lines) and says it overlaps #76. Platform CI at head: 3 failures (not attributable, UNVERIFIED cause).
Rec: SUPERSEDED by #84 (as #86 also notes "#84 itself overlaps earlier PR #76 ... Follow #84/#76 ownership decision before promotion").

### #77 Fix cross-platform CI / beta QA regressions
Purpose: preflight hardening, normalization host-write transaction, SUS-08 test, loop reprepare, early-reflection buffer sizing. Body: "No merge or release build should proceed until they pass" (they have: all green at head). anc=yes, uniq=0: already in I (and 342-commit divergence from master carries it).
Release side effect: it targets master; merging triggers whatever master workflows do (UNVERIFIED for this PR specifically).
Rec: ALREADY-CONTAINED.

### #78 README rewrite + docs/wiki (draft, docs only)
Purpose: concise README + docs/wiki/* source pages. Open (body): About section not set (403), wiki repo not pushed (no credentials). Comment: Win Free failure treated as pre-existing. 1 unique commit; touches README.md (I has its own README.md -> likely conflict) and 6 new docs/wiki files; I has docs/wiki-draft/ (a different draft wiki). Not run: build/tests.
Rec: NEEDS-OWNER-DECISION (README/wiki front-page content, overlap with docs/wiki-draft, facts may be stale vs 342 commits of pedalboard work; owner should say whether README is rewritten now).

### #79 Pedalboard specs/impl/QA WIP (draft)
Continues #73; BoardModel edits, undo/redo, etc. Body: "Keep draft until the complete pedalboard and release gates pass. Publish pedalboard builds only after feature completion and validation." Remaining: BoardEngine/TopologyCompiler, state persistence, DSP, UI, host integration, sanitizers. anc=yes.
Rec: ALREADY-CONTAINED.

### #80 Pedalboard working preview (draft)
Head intentionally left at b5bec374 while "scoped checkpoints" (22d6b18d, native6d9...) carry later work. Body: "No tag, master merge or new release." "The workflow cannot publish releases." "Do not cancel this run with handoff-only native-branch pushes." Remaining gates listed (whole-processor 20 trylocks/callback; CQ03 CPU-cap conflict unapplied; retirement starvation; named-model/looper/CPU acceptance; hardware/DAW/signing/notarization). Mentions "CQ03 exact-required CPU-cap conflict unapplied" = owner-spec conflict, not phrased as owner decision. anc=yes.
Rec: ALREADY-CONTAINED (later checkpoint branches such as codex/luthier-checkpoint-20261025-build8b are NOT assessed here: UNVERIFIED whether in I).

### #81 Fix lipo argument order (draft; base = codex/luthier-native-diagnostics)
Change: file-first `lipo <file> -verify_arch ...` in scripts/native_diagnostics.py, scripts/ci_build.sh, .github/workflows/build.yml; tightened fixture + new test. Body notes one test failing unmodified: test_report_limit_is_explicit_and_malformed_candidates_are_not_called_absent (31 != 32). Verified in I: native_diagnostics.py:102, ci_build.sh:286,344, build.yml:389 already use file-first form and test_native_diagnostics.py has the argv-rejecting fixture; so the fix is already in I. Head commit's diff also adds committed __pycache__ .pyc files (unwanted). Base branch is not master. "Merging this PR into that branch triggers the macOS rerun" (CI side effect on codex/luthier-native-diagnostics only).
Rec: SUPERSEDED (content already in I; head not an ancestor because of different history).

### #82 Six-pedal integration coordinator (draft)
Body quote: "Do not merge: merging to master triggers the beta publishers." Status incomplete: 0/6 pedals registered; known RED list (CE-2 BBD-on THD, CPU gates for Phase90/DD-3T/CE-2, Dyna THD/attack/CPU, CE-2W CE-1 THD, RC-5 wrapper skeleton, paged-looper grind exit 1). anc=yes, uniq=0.
Release side effect flagged: merge to master -> beta publishers.
Rec: ALREADY-CONTAINED.

### #83 Six-pedal continuation onto #82 (draft)
Base is #82 branch, so "cannot trigger the master-merge beta publishers". Slices: MSVC fix, CE-2 BBD revert, Phase90/Dyna CPU, RC-5 wrapper v1. Owner decision quote: "The gate stays the original known RED and needs an owner decision." (CE-2 BBD-on THD). RC-5 unregistered. anc=yes.
Rec: ALREADY-CONTAINED.

### #84 Prevent empty SoundGrind campaigns (draft)
4 commits: scripts/soundgrind_stress.py (101 lines), scripts/test_soundgrind_stress.py (7 tests, 83 lines), docs/handoff/SOUNDGRIND-2026-10-30-REPAIR.md, docs/handoff/LUTHIER-CONSOLIDATION-PROMPT-2026-10-30.md (a prompt file; evidence only, not followed). Purely scripts/docs: no DSP. Limits (body): not native-qualified, Windows/macOS execution unverified, random selection doesn't guarantee all suites run. Not in I. Overlaps #76 (extends it). Not stated as a release-affecting change.
Rec: MERGE-INTO-INTEGRATION (low-risk additive scripts; only caveat is the one-time owner confirmation that #84 replaces #76, per #86's text; if the consolidation prompt file is unwanted, drop it).

### #85 Pedalboard consolidation (draft) = integration head
#73,#79,#80,#82,#83 all ancestors (verified). Body: "Do not merge yet: merging to master would trigger the beta publishers, and the release gates are not green." Known open gates: RefPedalCe2.bbdOnOriginalThdGate RED on all platforms and "needs an owner decision. CE-2 is unregistered."; macOS failures at 25cd2135 (unit-test run no summary line; AU pluginval segfault in Automation (Pro); failing tests aSessionSurvivesThePrepareThatFollowsIt, convolution*(Free), saveExtraExceptionReacquiresGuardAndAllowsRetry, CQ23).
Latest digest comment (run 41796728348, ca14934e): Linux Free/Pro: only bbdOnOriginalThdGate fails (measured 0.0017% vs required 0.5..3%); Windows Free/Pro: bbdOnOriginalThdGate plus tenThousandNativeOperationsAndEditorDestruction (instance-count `getInstanceCount()==count+1`, "Duplicate pedal" rejected); macOS Free: no summary line, failures in aSessionSurvives..., convolution* (alt preset body IR), saveExtraException..., macOS Pro: no unit-tests.log. clap-validator 18 pass/0 fail/3 skipped (Linux/mac Pro/Free); VST3 pluginval SUCCESS everywhere seen.
Rec: LEAVE (it is the integration head).

### #86 Gate SoundGrind runner on Linux/Win/macOS (draft, stacked on #84)
Adds .github/workflows/soundgrind-runner-tests.yml (42 lines) + scripts/test_soundgrind_ci_contract.py; Python-only, unprivileged, 10-min timeout. Body: Linux 7+3 tests passed and Windows 7+3 passed (run 41876094837); macOS "remains queued" (now cancelled in check-runs). "Keep the pedalboard consolidation #85 draft and beta-publishing gates closed; this PR alone cannot qualify or publish any release." Mentions "Follow #84/#76 ownership decision before promotion". Not in I; uniq=6 includes #84's 4 commits.
Rec: MERGE-INTO-INTEGRATION only after/with #84 (macOS result still missing, UNVERIFIED).

### #87 OpenAI/Codex autonomous workflow (draft)
Adds .github/workflows/openai-agents.yml (52 lines, manual trigger per body) + .github/agent-prompts/*.md. Body: "no workflow execution or API spend has been initiated. Check GitHub Actions permissions and run one supervised smoke test before enabling schedules." Side effects flagged: API spend, Actions permissions, autonomous agent scheduling. Not related to pedalboard or product code. Failing platform checks at head (Linux Pro, Win Free, macOS Pro) are unrelated to its 3 files (UNVERIFIED cause). Any agent prompt text in it is not followed here.
Owner decision (implicit): enabling autonomous agent + API spend; secrets (UNVERIFIED which).
Rec: NEEDS-OWNER-DECISION.

## Release/publishing side-effect statements (all)
- #82: "merging to master triggers the beta publishers" (do not merge).
- #85: "merging to master would trigger the beta publishers, and the release gates are not green."
- #83: base is integration branch so "cannot trigger the master-merge beta publishers".
- #79: "Publish pedalboard builds only after feature completion and validation."
- #80: "No tag, master merge or new release."; "The workflow cannot publish releases"; frozen/create-only draft release guards.
- #77: "No merge or release build should proceed until they pass."
- #86: "this PR alone cannot qualify or publish any release"; keep beta-publishing gates closed.
- #81: merging into codex/luthier-native-diagnostics triggers macOS rerun.
- #87: workflow could spend API money if enabled.
- Every master-targeting PR check list includes "Publish verified Mac Pro beta" (skipped on PR events, so publishing happens on push to master/branch, UNVERIFIED workflow trigger details).

## Owner-decision statements (quotes)
- #85: "`RefPedalCe2.bbdOnOriginalThdGate` is RED on all platforms and needs an owner decision. CE-2 is unregistered."
- #83: "The gate stays the original known RED and needs an owner decision."
- #72: "Owner decision 2026-10-09: the next version locks the Pro features after a 60-day trial..."
- #86: "Follow #84/#76 ownership decision before promotion."
- #78: About section/wiki not set because proxy refused writes (owner must set manually).
- #82: "owner-selected six-pedal milestone".
