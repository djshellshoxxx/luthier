# Branch ledger: 29 non-ancestor remote branches vs I

I = `origin/claude/determined-ptolemy-mlbclp` = `ca14934e` (PR #85 head). master = `0f62c3d6`. Checked on 2026-11-11, read-only.

## Method

- `git cherry I B` (patch-id) plus `git log I..B`.
- `git merge-tree --write-tree --name-only I B` for the merge result. **changes_vs_I** = number of paths where the merged tree differs from I. 0 means B adds nothing.
- Per "+" commit: `git apply --cached --check` (forward and `-R`) against a scratch index loaded from I's tree (`GIT_INDEX_FILE` in scratchpad; the repo index and work tree were not touched). R = the change is already in I. F = missing, applies cleanly. X = diverged.
- Commit subjects were matched against I's 342 subjects. Symbols, test names and file sizes were grepped in I.
- Evidence: I's own reconciliation records, `docs/handoff/PEDALBOARD-2026-10-25-BUILD6.md` ("Other author preservation") and `docs/coverage/PEDALBOARD-DIRECT-HOST-MIDI-20261025.md`.
- Scope: `docs/handoff/LUTHIER-CURRENT-COMPLETION-2026-10-27.md` and `PEDALBOARD-SIX-MODEL-SCENARIO-2026-10-26.md` (on I) define the first release as six models: Phase90, CE-2, CE-2W, DD-3T, Dyna Comp and RC-5. Twelve models are "deferred ... all source/specs/tests/provenance retained" on their source branches: DS-1, TS808, SD-1, Distortion+, RAT, NYC/Deluxe/Green Russian/Wicker Muff, Ditto, Nano360 and RC-1.

## Table

| branch | class | missing work (1 line) | merge result | value | recommendation |
|---|---|---|---|---|---|
| codex/luthier-soundgrind-ci-20261104 | UNIQUE | SoundGrind stress runner (PR #76 a2aa30d3), its repair (no zero-test false-green, no log overwrite), portable tests, 3-OS CI workflow, consolidation prompt doc | clean (6 paths) | med | owner decision; take this branch as one unit. It contains soundgrind-repair and stress-audio-gui |
| codex/setup-autonomous-agent-20261104 | UNIQUE | `openai-agents.yml`: `codex exec --full-auto` with `contents: write`, pushes branches and opens draft PRs; 2 prompt files | clean (3) | low | owner decision. Do not fold into integration (security-relevant workflow) |
| codex/luthier-rc5-tap-20261030 | UNIQUE + WIP-UNQUALIFIED | RC-5 TEMPO tap trigger `boss_rc5_tap`, tap to internal rhythm, `rc5TapTempoBpm` extra state, `extraOverridesInitialParam` hook (IPedal/PedalPool/TopologyCompiler), spec errata, evidence logs, 4 handoffs | clean (31; base a8f878d5) | high | owner decision. Merge only after review. The branch itself says "not ready to merge, register or release" |
| codex/luthier-pedalboard-release | UNIQUE | `pedalboard-release-qa.yml` (full native matrix, package, `secrets: inherit`), `pedalboard-focused.yml` with headless CMake harness, CE-2W original THD gate test, 3 RC-5 runtime acceptance tests (expected RED: RC-5 is unregistered) | conflict: `Source/Tests/ReferencePedals_boss_rc5Tests.cpp` | med | owner decision. Possibly cherry-pick dcfc780b (tests/harness) without the push-triggered workflows |
| codex/luthier-soundgrind-repair-20261030 | UNIQUE (subset) | Is an ancestor of soundgrind-ci | clean (4) | med | skip (take soundgrind-ci) |
| codex/luthier-ce2-evidence-20261028 | UNIQUE (docs only) | 3 handoffs: CE2-ENDPOINTS, RC5-TAP-INTERFACE, CODEX-RESTART | clean (3) | med | skip (is an ancestor of rc5-tap) or cherry-pick f769d2d6 209114b3 d0fa0df7 |
| codex/luthier-backend-release-retry | UNIQUE | MidiInterpreter: when the backend note-off queue is full, a rejected release no longer clears the held slot (fixes stuck notes); 2 regressions; doc | clean (3) | high | cherry-pick 9167c620 |
| codex/luthier-pedalboard-state | PARTIAL | Looper history/phase across bounded rebuilds (77f13e17); looper stress tests (b1b86117); sample-clock LooperFootGesture (b84a6fa0); native Ditto + LooperAssetConverter (ff289cac e6430df5 a0733cf3) | conflicted (34 paths, looper core, MIDI, IPedal…) | med (looper fixes) / low (Ditto deferred) | owner decision. Port 77f13e17 and b1b86117 by hand if still applicable to PagedLooperCore. Keep the Ditto work parked (deferred) |
| claude/peaceful-carson-vzl4e4 | STALE-SUPERSEDED (reviewed by I) | Remainder: duplicate cherry-picks of codex WIP, blanket ref_* visibility and CPU-ID exemptions (rejected in I), unbuilt Win/mac fixes fca497b2, 1c9ae70b, 075d69ab, 6d9048b5, docs/logs | conflicted (~70 paths) | low | skip. Optionally review 6d9048b5 (I still asserts `bpm == 133.0` at RefPedalApiTests.cpp:772) |
| codex/luthier-preview-ABI-unqualified | WIP-UNQUALIFIED | PreviewCommandQueue + PreviewMidiState + private PluginProcessor wiring (changes sizeof processor), preview tests, 4 coverage docs | clean (12) | med | skip the production wiring. Owner decision on dd36ba63/ba6a618a (queue + tests, unwired). Take the backend fix from 9167c620 instead |
| codex/luthier-pedalboard-dsp | UNIQUE, mostly WIP | Deferred named drives (TS808, DS1, SD1, Dist+, RAT, 4 Muffs) with failed gates; Generic A drive/comp WIP calibration; RBJ peak/shelf; PreparedDiodeRoot; paired biquad/FIR; LFO wrap; many evidence files | conflicted (25 paths) | low for initial release / high for deferred twelve | owner decision. Keep the branch as the source of the deferred models. Do not merge |
| codex/luthier-pedalboard-host-midi | PARTIAL | 5be9c0bc (ClockSync ignores tap under external master, 3 lines + test); 8419a705 (MidiRouter retries clock status after hidden-UI feedback overflow); 11b08f57 (isolated ParameterReadbackMailbox, unwired); dd36ba63 (preview queue); docs c121fa55 e421a9cb | conflicted (30 paths); the Source parts of 5be9c0bc, 8419a705, 11b08f57 and dd36ba63 each apply cleanly to I | med | cherry-pick 5be9c0bc 8419a705. Owner decision on 11b08f57, dd36ba63, c121fa55 |
| claude/intelligent-johnson-ruc6si | STALE-SUPERSEDED | I already has the file-first `lipo X -verify_arch`. Only unique content is 8 committed `__pycache__/*.pyc` | conflict: `scripts/test_native_diagnostics.py` | none | skip |
| codex/luthier-pedalboard-ui-full | STALE-SUPERSEDED | 8ecf6a5a 2dcf3adb 881aec2c in I by subject; 29cb9d1d already in I (R); 5e961be4 field `cpuGovernorFailed` in I; c8d3cc1d replaced by I's 5651d559 (PresetFolderTestSupport.h) | conflicted (16 paths) | none | skip |
| codex/luthier-pedalboard-timefx | PARTIAL | **ee3d7c0d DD-3T meter `energy` subnormal flush (missing, in-scope model)**; 8dc4ca07 exact LFO wrap (applies cleanly); 7e9e626f CE1 shared prefix and b9995a10 control-rate CPU work (conflict); paired biquad/FIR (I rejected paired-stage for Phase90); CQ03 proposal 10cd405a; packaging 82de765c and GUI-reach cbc19186 superseded in I | conflicted (31 paths) | high (ee3d7c0d) / med (CPU work) | cherry-pick ee3d7c0d (manual 1-line port to boss_dd3t.cpp:266 + test). Owner decision on 8dc4ca07, 7e9e626f, b9995a10 |
| codex/luthier-pedalboard-engine | STALE-SUPERSEDED | I has counterparts: 6863e0ef (prepared memory), 879be6be and 77a0e876 (retirement bound + control guard), c6f4cbd6 327fe2f0 0106640e 486d92d2 cb8130f7. Missing are docs only: b0c3d1c3 (LEGACY-DECAY-DIAGNOSIS, MEMORY-ADMISSION-PROPOSAL) and 6d39e58d (retirement-progress-source8 artifact) | conflicted (30 paths) | low | skip. Optionally cherry-pick b0c3d1c3 |
| codex/luthier-release-provenance | CONTAINED | 3 commits patch-equivalent in I (`-`) | merge = I | none | skip |
| codex/luthier-rc5-rhythm | CONTAINED | 3 commits patch-equivalent; I is newer (809f48bd DC removal) | add/add conflicts on Rc5Rhythm.{h,cpp} and its test because I is newer | none | skip |
| docs/readme-and-wiki | UNIQUE | README rewrite + 6 `docs/wiki/*.md` pages (PR #78) | conflict: `README.md` | low-med | cherry-pick `docs/wiki/*` from 9842fa61 only. Owner decision on README |
| codex/soundgrind-stress-audio-gui | UNIQUE (subset, buggy) | Original PR #76 runner with a zero-test false-green, fixed by the repair branch | clean (1) | none | skip |
| codex/release-fix-resonator-string-decay | CONTAINED | 8470a6e8 patch-equivalent in I | merge = I | none | skip |
| codex/pro-beta-resonator-decay-ci | STALE-SUPERSEDED | efa7d27d is an alternative SUS-08 isolation. I has the 8470a6e8 variant | clean (1) | none | skip |
| codex/luthier-ci-sharded-tests | UNIQUE | Opt-in deterministic test shards (TestSelection.h, tests, ci_build.sh). No "shard" in I | clean (3) | low | owner decision |
| claude/vigilant-babbage-2dwdec | STALE-SUPERSEDED (code) + UNIQUE docs | Old ReferenceBoard/ReferencePedalModel/CircuitPrimitives API (superseded by Board/BoardEngine) and LuthierEngine edits; ~40 `docs/research/pedals/*.md` + docs/pedals PLAN/PROGRESS | conflict: `Source/LuthierEngine.cpp`, `Source/LuthierEngine.h` | low | skip code. Optionally take `docs/research/pedals/` only |
| claude/luthier-cloud-session-5lzlix | UNIQUE | 60-day Pro trial + unlock code (PR #72): Edition, Telemetry, TrialLockTests, spec/trial-lock.md | conflict: `Source/PluginProcessor.cpp` | med (product) | owner decision (product + engineering review) |
| claude/luthier-micface-workshop-fix | STALE-SUPERSEDED | I's MicPlacementUi Rig pins the MOD tab (`TabPreference`, line ~95) | clean (1) | none | skip |
| claude/luthier-codex63-integ | CONTAINED | Merge commit only. Merged tree = I | merge = I | none | skip |
| codex/luthier-beta-looper-concurrency | CONTAINED | Merged tree = I (content present) | merge = I | none | skip |
| codex/luthier-licensing-core | CONTAINED | Merged tree = I | merge = I | none | skip |

Ancestry: soundgrind-repair is an ancestor of soundgrind-ci, stress-audio-gui is an ancestor of soundgrind-repair, and ce2-evidence is an ancestor of rc5-tap. backend-release-retry is NOT an ancestor of preview-ABI; preview-ABI carries an equivalent 8891712e and later edits.

## Per-branch notes (non-CONTAINED)

### codex/luthier-rc5-tap-20261030 (54ad2c72, 2026-10-31 06:02 UTC). UNIQUE, WIP. Clean merge.
- 15 commits on a8f878d5, which is PR #83 and I minus its last 3 commits. They include the 3 ce2-evidence documentation commits.
- Production: `IPedal.h`, `Models/boss_rc5.{h,cpp}`, `PedalPool.{h,cpp}`, `TopologyCompiler.cpp`, and spec `BOARD_CORE.md` + `pedals/boss-rc5.md`. I has none of `boss_rc5_tap`, `rc5TapTempoBpm` or `extraOverridesInitialParam`.
- Self-reported results (Linux Pro/Free/ASan; not re-run here): 28 tests / 26,023 checks; board regressions 141 tests / 111,253 checks.
- The handoff `RC5-TAP-2026-10-30-WIP.md` says: "The WIP hook, host-lane echo, preset/memory integration and native gates remain unaccepted." "Keep RC-5 unregistered and do not release it." "This is not ready to merge, register or release."
- It changes a shared interface (IPedal / PedalPool initial-serial replay), so it needs review before landing.

### codex/luthier-pedalboard-release (dcfc780b, 2026-10-30). UNIQUE. Conflict in `Source/Tests/ReferencePedals_boss_rc5Tests.cpp`.
- ddc7956d: new `pedalboard-release-qa.yml` calling `build.yml` with all 3 platforms, pro+free, strictness 10, `package: true`, version `0.1.0-pedalboard-qa`, `secrets: inherit`. It runs on push to this branch.
- dcfc780b:
  - adds `pedalboard-focused.yml` and `Tests/PedalboardFocused/CMakeLists.txt` + `scripts/pedalboard_focused_main.py` (headless six-model runner);
  - adds the CE-2W "original standard and CE-1 THD gates" test;
  - adds 3 `RefPedalBossRc5Runtime` tests that require a registered `boss-rc5`. The code comment says "Missing registration is a real, early failure", so these are RED by design on I.
- Packaging and secrets side effects need owner sign-off.

### codex/luthier-ce2-evidence-20261028 (d0fa0df7). UNIQUE docs. Clean. Ancestor of rc5-tap.
- Documentation only. No tests ran for these commits (the restart doc says so).

### codex/luthier-backend-release-retry (9167c620). UNIQUE. Clean. High value.
- Real defect: when the 192-entry PlayEventQueue note-off lane is full, MidiInterpreter cleared the held slot anyway, so the string kept sounding.
- Fix: check `addNoteOff` admission before clearing, then retry via the deferred path. "No new members, APIs, allocations, locks, or class layouts."
- The doc says: "Cherry-pick this isolated commit onto the coordinator's current source after review."
- Self-reported: 43 cases / 13,959 checks pass; headless ASan/UBSan clean; TSan not clean (convolution loader, not instrumented).

### codex/luthier-preview-ABI-unqualified (f29404c7). WIP-UNQUALIFIED. Clean.
- The doc says: "ABI-UNQUALIFIED private checkpoint ... not an integration-ready fix".
- "Appending previewState changes sizeof(LuthierAudioProcessor)". "No whole-host, sanitizer, safe lifetime, release qualification or root merge is claimed."
- It points coordinators to 9167c620 for the backend fix.

### codex/luthier-pedalboard-state. PARTIAL. 34 conflicted paths.
- Missing from I (file and test-name checks):
  - 77f13e17: tests `changingUndoDepthOneEightOnePreservesUndoneAudioAndNearestRedo` and `restoredPlayingTransportKeepsPublishedSamplePhase` are absent from I.
  - b1b86117: `thousandAcceptedOverdubsKeepAnalyticalSumAndUndoRedoBytes` and `seededRandomUndoRedoPreservesExactAcceptedLayerAudio` are absent.
  - b84a6fa0: new `Common/LooperFootGesture.h` and its test.
  - Ditto: `Models/tc_ditto.cpp` is 17,354 B on the branch vs a 195 B skeleton in I. `Common/LooperAssetConverter.{h,cpp}`, `RefPedalDittoTests.cpp`, `RefPedalLooperConversionTests.cpp` and the spec `LOOPER_ASSET_RATE_CONVERSION.md` are absent from I.
- Ditto is in the deferred twelve. The 77f13e17 and b1b86117 looper-core fixes touch PagedLooperCore and LooperStorage, which RC-5 uses. I rewrote those heavily afterwards (redo/reverse/cache work, 2026-10-27), so applicability is UNVERIFIED.
- 92888d2d and a1cf1000 are duplicates of prepared-memory and IPedal imports that are already in I.

### claude/peaceful-carson-vzl4e4 (1f646a65). STALE-SUPERSEDED.
- I's BUILD6 handoff records an explicit review: "Its authoritative handoff is `docs/handoff/INTEGRATION-HANDOFF-2026-10-25.md` on that ref ... Claude's blanket ref_* GUI/search and CPU-ID exemptions were rejected. Its unbuilt Windows/macOS hypotheses and unqualified model registration were not adopted."
- I reused b3ec7a41's three direct-host capacity fixture files.
- The branch's own handoff says: "The branch is built on a stale snapshot", "Nothing I wrote for Windows or macOS has been built on those platforms".
- Its handoff also lists unrelated non-Luthier scheduled tasks by name (personal data). Do not copy that doc into I.

### codex/luthier-pedalboard-dsp. UNIQUE, mostly WIP. 25 conflicted paths.
- Named drive and Muff models (DS1, SD1, TS808, Dist+, RAT, NYC/Deluxe/Green Russian/Wicker): 3–11 KB implementations on the branch vs about 200 B skeletons in I. Many commits are titled "WIP ... record failed gates" / "Keep all unaccepted named WP12 candidates out of the registry".
- Generic A Boost/Fuzz/Overdrive/Compressor are 1.5–2.5× larger on the branch (WIP calibration "with unchanged open alias and CPU gates").
- New helpers absent from I: CircuitPedal, DualGainLegStage, LinearGainStage, AsymmetricFeedbackDiodeStage, PreparedDiodeRoot, RBJ peak/shelf in Biquad, paired biquad/FIR.
- FeedbackDiodeStage and several helpers are byte-identical in I.

### codex/luthier-pedalboard-host-midi. PARTIAL.
- 063d4904, 6cc528ba and 5a716ec7 are in I by subject. f0647b8d is already in I (R).
- Missing: 5be9c0bc, 8419a705, 11b08f57, dd36ba63, and docs c121fa55 (HOST-RT-PUBLICATION-REVIEW.md, 116 lines) and e421a9cb.
- Test names absent from I: `tapCannotChangeTempoOrHistoryUnderExternalMaster`, `hiddenUiClockLossRetriesAfterRingDrain`, `hiddenUiTempoConvergesDuringContinuedActivityFlood`.
- The Source/scripts parts of each apply cleanly to I. Not compiled here.

### codex/luthier-pedalboard-timefx. PARTIAL.
- ee3d7c0d: in-scope DD-3T fix. I still has `energy+=(.5*wetEnergy-energy)*energyCoefficient;` at boss_dd3t.cpp:266. The branch wraps it in `common::safe(...)` and adds the test `silentTailAccumulatorDoesNotUnderflowWithFlushDisabled`. The patch context moved, so it needs a manual port.
- 8dc4ca07 (LFO exact bounded wrap) is absent from I and applies cleanly.
- 7e9e626f and b9995a10 (CE-1/CE-2/DD-3T control-rate CPU reductions) are absent from I and conflict.
- Already in I or superseded: 53707c7e, 9b6fdf08, 8977fa99 (by subject); 2f96a496 and 133ae0c5 (R); cbc19186 (I has cf2212fb); a21cb3d3 and bfa90f27 (I has native-diagnostics). 82de765c is likely superseded by f31e8f81 (UNVERIFIED).

### codex/luthier-pedalboard-engine. STALE-SUPERSEDED.
- Most "Import …" commits are already in I (R) or diverged in files that I later rewrote.
- 6d39e58d's own HANDOFF says it is a patch artifact ("Do not treat an artifact-only cherry-pick as deployment of the fix"). I contains the guard (`saveExtraOutsideControlGuard`, `controlGuard` in BoardEngine.cpp) via 77a0e876.
- Unique docs only: `docs/coverage/PEDALBOARD-LEGACY-DECAY-DIAGNOSIS.md`, `PEDALBOARD-MEMORY-ADMISSION-PROPOSAL.md`, `docs/coverage/retirement-progress-source8/*`.

### codex/luthier-soundgrind-ci-20261104 (f6bf946f, 2026-11-04). UNIQUE. Clean.
- Based on master, not on I. Touches only scripts, CI and docs.
- Repair doc: "has not been run through CI or independently reviewed"; "buffers complete child output"; random selection "is not a guarantee that every valid suite is executed".

### codex/setup-autonomous-agent-20261104. UNIQUE. Clean.
- The workflow is manual-dispatch. It installs `@openai/codex` unpinned, runs `codex exec --full-auto` with `OPENAI_API_KEY`, and has `contents: write` / `pull-requests: write`.
- Do not merge without explicit owner decision. Noted as a security-relevant change.

### docs/readme-and-wiki, claude/luthier-cloud-session-5lzlix, codex/luthier-ci-sharded-tests, claude/vigilant-babbage-2dwdec
- See the table.
- The consolidation prompt (on soundgrind branches) says: "PR #78 has potentially useful wiki content but conflicts with the newer README; reconcile selectively"; "PR #72 is an incomplete distinct trial-lock change that conflicts with the pedalboard tree and needs separate product and engineering review"; "PR #81's functional lipo corrections ... already exist".

### Superseded or junk: intelligent-johnson, pro-beta-resonator-decay-ci, micface-workshop-fix, ui-full, stress-audio-gui
- See the table. Nothing to recover.

## Handoff, ownership and restart docs NOT on I

Newest first:

1. **`docs/handoff/RC5-TAP-2026-10-30-WIP.md`** (rc5-tap, last edited 54ad2c72, 2026-10-31)
   - Test-first RC-5 tap checkpoint: RED, then tap→rhythm GREEN, then extra-state persistence, then board-restore fix through the `extraOverridesInitialParam` WIP hook. Pro/Free/ASan focused results recorded.
   - Ownership: "Owner confirmed Claude has stopped Luthier work. This Codex branch starts at `d0fa0df7…`".
   - Next step: "design and test safe synchronization into the board/host tempo lane and per-memory/preset state before registration". "Host-lane echo and 99-memory switching remain open." "This is not ready to merge, register or release."
2. **`docs/handoff/LUTHIER-CONSOLIDATION-PROMPT-2026-10-30.md`** (soundgrind-repair / soundgrind-ci, 17808fa3)
   - Restart prompt. It says to use "one canonical integrator" and to preserve every unique commit. Scope is the six models; the twelve deferred models are kept.
   - It names PR #83 (`a8f878d5`) as current lineage containing #73/#77/#79/#80/#82, and warns: "merging #77 to master can start its beta2 publisher".
   - Next step: "After ownership and branch reconciliation, continue the highest-value ready six-pedal implementation slice with focused RED/GREEN tests ... The RC-5 tap handoff proposes an event route but flags clock authority, persistence and host automation integration decisions".
3. **`docs/handoff/SOUNDGRIND-2026-10-30-REPAIR.md`** (soundgrind branches)
   - PR #76 runner repair: exclusive log creation, `--list` filter preflight, nonzero test summary required. 21 Python tests pass on Linux. Not CI'd or independently reviewed. "Do not merge or replace the separate pedalboard work."
4. **`docs/handoff/PEDALBOARD-2026-10-28-CODEX-RESTART.md`** (ce2-evidence d0fa0df7, "Record Luthier restart and ownership handoff"; also on rc5-tap)
   - Ownership quote: "The parent is checking current external Claude ownership. Native UI and paint diagnostics are externally owned; RC-5 source ownership is not yet confirmed. Do not edit those files or duplicate their work until reconciled. **Codex is the canonical source integrator.** No PR approval is authorized without specific user confirmation."
   - Next step: "1. Inspect current local status, worktrees, live refs/PRs and external owner's final handoff ... 2. Once RC-5 ownership is clear, use tap interface proposal for a test-first bounded integration ... 3. Retain all original sound, CPU, safety and realtime thresholds ... qualify native products on the resulting exact source".
   - Status: six models remain unregistered. Windows run 40678198637 on a8f878d5: Free 2/2905 and Pro 3/2922 failed (as reported by the parent; not independently read).
5. **`docs/handoff/PEDALBOARD-2026-10-28-RC5-TAP-INTERFACE.md`** (ce2-evidence 209114b3)
   - Read-only proposal: an appended `boss_rc5_tap` `ParamKind::Trigger` for the specified TEMPO button. The existing `Rc5TapTempo` estimator is not used by the `BossRc5` wrapper. The doc says not to repurpose FootEvent.
6. **`docs/handoff/PEDALBOARD-2026-10-28-CE2-ENDPOINTS.md`** (ce2-evidence f769d2d6)
   - 120-render private CE-2 transfer-endpoint experiment. No sampled weight satisfies both the low-level (0.5–3%) and high-level (2–6%) THD bounds. No DSP or threshold change. The gate stays RED.
7. `docs/coverage/retirement-progress-source8/HANDOFF.md` (engine 6d39e58d, 2026-10-25)
   - Patch-artifact checkpoint for reader retirement / control guard. I already contains this code.
8. `docs/handoff/INTEGRATION-HANDOFF-2026-10-25.md`, `docs/HANDOFF-pedalboard-release.md`, `docs/REPORT-pedalboard-status-2026-10-25.md` (peaceful-carson)
   - Claude preservation handoff, already reviewed by I (BUILD6).
   - Contains personal scheduled-task names unrelated to Luthier. Do not import.
9. `docs/coverage/PREVIEW-PROCESSOR-INTEGRATION.md`, `PREVIEW-COMMAND-TRANSPORT.md`, `HOST-RT-PUBLICATION-REVIEW.md` (preview-ABI / host-midi)
   - Preview transport contract and ABI caveat; whole-processor RT gate RED (20 trylocks per callback).

## Instructions found in branch text (noted, not followed)

- Several handoffs give resume commands, for example `git ls-remote` and `cmake --build ...`.
- The RC5-TAP doc says "do not launch a duplicate build".
- The autonomous-agent prompts direct an agent to choose and implement work.
- None of these were acted on. This task is read-only.

## UNVERIFIED

- None of the candidate cherry-picks were compiled or tested. "applies" means only `git apply --check` against I's tree.
- Whether 77f13e17 and b1b86117 looper fixes are still needed after I's 2026-10-27 looper rewrites.
- Whether I's `RefPedalApiTests.cpp:772` `bpm == 133.0` assertion currently fails (peaceful-carson 6d9048b5 says the glide makes it wrong).
- Whether 82de765c packaging is fully superseded by I's f31e8f81.
- CI status of any branch; GitHub PR state (no GitHub reads were made).
- All test counts quoted from branch docs are self-reported.
