# Pedalboard integration: preservation and integration handoff

Prepared 2026-10-25 for the existing Luthier team. Read-only investigation plus
this documentation commit. No implementation, builds, CI reruns or merges were
done while preparing it. Sibling branches were fetched (remote-tracking refs
only) to compare them; nothing was merged or deleted.

Companion documents: `docs/HANDOFF-pedalboard-release.md` (operational notes),
`docs/REPORT-pedalboard-status-2026-10-25.md` (earlier status; **see the errata
at its top, it contains one wrong statement**), `docs/coverage/PEDALBOARD-*.md`
(the project's own ledgers). Raw material: `docs/handoff/imported-commits.tsv`
and `docs/handoff/logs/`.

## 0. Read this first

1. **The branch is built on a stale snapshot.** Every `codex/luthier-pedalboard-*`
   branch has moved on GitHub since I based on them, and there is a new branch,
   `codex/luthier-checkpoint-20261025-midi-host`. See section 9.
2. **No working release artifact exists** (section 8).
3. Several of my fixes duplicate fixes now made upstream, some with conflicting
   names (section 9). Reuse selectively.
4. One of my exemptions (`ref_*` parameters) is broader than its stated
   justification (section 4).
5. Nothing I wrote for Windows or macOS has been built on those platforms, and
   the macOS change has not even been compiled.

## 1. Repository state

| Item | Value |
|---|---|
| Current branch | `integ-try` (local; **no upstream configured**) |
| HEAD when investigated | `907dedb9a82883fd25d45d35896e675ce6e99ad6` (this handoff commit is on top; `git rev-parse HEAD` for the final value) |
| Remote `claude/peaceful-carson-vzl4e4` | `907dedb9a82883fd25d45d35896e675ce6e99ad6` (identical to HEAD at that time) |
| Local `claude/peaceful-carson-vzl4e4` | `b3ec7a41f1accbad83d2f25f74a131200b7b1ea5`, **stale**, 7 commits behind the remote. Nothing unique on it. Work was pushed with `git push origin integ-try:claude/peaceful-carson-vzl4e4`. |
| `master` | `0f62c3d67bdba055b41e66c7ab8ac9ab4b728662` (local = remote) |
| Unpushed commits | none |
| Uncommitted / untracked | none. Ignored, not in git: `ThirdParty/JUCE/`, `ThirdParty/clap-juce-extensions/`, `build-ci/` (reproducible with `scripts/ci_build.sh deps configure build`). |
| Stashes | none |
| Branches deleted | none |
| Worktrees | one (`/home/user/luthier`). I created a temporary `/home/user/lbase` worktree (detached at `master`, only an unfinished build tree, no commits) for a baseline build and removed it **before** this request when I was told to stop. |
| Base | `origin/codex/luthier-pedalboard-completion` at `e5d350e5994c96e02277f9c8a00606073df47213` |
| Commits since base | 72, no merges: 65 cherry-picks (64 from the six sibling branches, 1 from `pro-beta-resonator-decay-ci`) plus 7 of mine |

**Timer.** The Luthier continuation routine `trig_01CVERr6qMDvBr2AJw3ibuwg`
("Pedalboard release check-in") was disabled, then **deleted**; it is absent from
`list_triggers` (checked with `include_completed`). Ten other routines exist (p2p-offramp, replikrazy, GhostPay, a "Verify worker
checkpoints" reminder, and an old, already-fired "Check CI for 6ff0f63" from
2026-10-12). None was created by this effort, none is the Luthier continuation
timer, and none was touched.

## 2. Imports and skipped commits

### 2.1 Imported (65 commits)

Exact mapping, from the `cherry-pick -x` trailers on each commit, not
reconstructed: `docs/handoff/imported-commits.tsv` (columns: new SHA, original SHA,
original branch, subject). By source branch: dsp 13, engine 13, host-midi 13,
timefx 11, state 8, ui-full 6, plus `efa7d27d` from `codex/pro-beta-resonator-decay-ci`.
Method: `git cherry` against the base found 70 commits unique by patch-id
(`logs/unique-commits-by-patch-id.txt`), applied in commit-date order
(`logs/cherry-pick-order.txt`). A plain merge conflicted on 25 files because the
base had copied much of the siblings' work under different SHAs.

Several imported commits are the same change committed on more than one sibling
branch (for example send-return loops exists as engine `950dbf93`, host-midi
`5a716ec7`, ui-full `6fcd9b90`); only one copy applied.

About a dozen imported commits are explicitly WIP (unregistered circuit
candidates with recorded failed gates). See section 6.

### 2.2 Skipped (6 commits)

Commit messages have no body, so "intended behavior" below is from the subject
and the diff. "Identical" means the added/removed lines of the two patches are the
same (`diff` of the +/- lines = 0); context lines can differ.

| Full SHA | Branch | Files | Intended behavior | Why skipped | Equivalent in HEAD? |
|---|---|---|---|---|---|
| `11f3042451f66177fa21ad93cfb17be44149a03e` | `codex/luthier-pedalboard-engine` | `ReferencePedals/Midi/MidiRouter.cpp`, `.h` | E46: add `MidiRouter::setHostEnabled(bool, serial)` and track model/host enable so a live host enable reaches MIDI routing before the model echo | content conflict in both files | **Yes, in larger form.** Patch identical to `8ecf6a5a`; base already has `b7757366` (same subject, 38 lines vs 17). HEAD contains `setHostEnabled`, `hostEnabledSerial`; 12 of 13 added lines present verbatim, 1 present in a revised form (`modelEnabledRevision == 0 &&` added to the condition). |
| `5a716ec7d958998cabc26941d8bdebe345b303ae` | `codex/luthier-pedalboard-host-midi` | `Board.cpp/.h`, `BoardEngine.cpp/.h`, `BoardModel.cpp/.h`, `Routing/SendReturn.cpp/.h`, `TopologyCompiler.cpp`, `Tests/RefPedalBoardCoreTests.cpp`, `Tests/RefPedalRoutingTests.cpp`, `docs/coverage/PEDALBOARD-ENGINE.md` (12 files) | Execute send-return loops and preserve bounded pedal events | conflicts in `BoardEngine.cpp/.h`, `TopologyCompiler.cpp`, both tests, the ledger | **Yes.** HEAD has `e0e2709b` (import of engine `950dbf93`). Patches differ by 20 lines: an extra test `legacyMoveRetainsAllSixteenSlotsWithSparsePedals`, which **is already in HEAD** (from base commit `bc33a4b6`). |
| `8ecf6a5a188bd446684a6bb3d23f7714d60bbf32` | `codex/luthier-pedalboard-ui-full` | `MidiRouter.cpp`, `.h` | same as `11f30424` | conflict in both files | Yes, as `11f30424` (patches identical to each other). |
| `6fcd9b909078c5162e0047a519862feb0b2aaeb6` | `codex/luthier-pedalboard-ui-full` | same 12 files as `5a716ec7` | send-return loops | conflicts as `5a716ec7` | **Yes**, identical to the imported `e0e2709b` (0 differing lines). |
| `6d9ce297787d50a815639996c8df6a5920310f55` | `codex/luthier-pedalboard-host-midi` | `Board.cpp`, `BoardEngine.cpp/.h`, `BoardModel.cpp/.h`, `PedalPool.cpp/.h`, `TopologyCompiler.cpp`, `RefPedalBoardCoreTests.cpp`, ledger (10 files) | Capture published runtime extras without mutating compiler state | conflicts in `BoardEngine.cpp`, `PedalPool.cpp/.h`, test, ledger | **Yes**, identical to the imported `e240d107` (import of engine `63af5494`). |
| `c65a4a694fcac99e5da5b6ec04221b2f3a01eac1` | `codex/luthier-pedalboard-host-midi` | `BoardEngine.cpp/.h`, `BoardModel.cpp`, `BoardTypes.h`, `Routing/Feedback.cpp/.h`, `TopologyCompiler.cpp/.h`, `RefPedalRoutingTests.cpp`, ledger (10 files) | Bounded feedback routing with kill and latched safety guards | conflicts in `BoardEngine.cpp`, `TopologyCompiler.cpp`, `RefPedalRoutingTests.cpp`, ledger | **Yes**, identical to the imported `b4e9ab31` (import of engine `5536c968`). |

Conclusion: nothing in the six is known to be missing from HEAD. Conflict file
lists were recomputed read-only with `git merge-tree --write-tree --name-only`
against HEAD; the original conflicts were against the then-current tree.

## 3. My changes versus `codex/luthier-pedalboard-completion@e5d350e5`

Local test command used throughout (Linux, Pro edition only, `build-ci`):
`cmake --build build-ci --target LuthierTests -j4` then
`cd build-ci/LuthierTests_artefacts/Release && xvfb-run -a ./LuthierTests <suite-or-test-name-substring>`.
"CI" below means GitHub run #18 (`6d9048b5`) or #19 (`1c9ae70b`); see section 8.

| Commit | Files | What failed before | What passed after | Validation |
|---|---|---|---|---|
| `b3ec7a41` tests: align suites | `PluginBusTests.cpp`, `GuiReachabilityTests.cpp`, `JamProcessorTests.cpp`, `MidiOutPanelTests.cpp`, `ModelGapsUiTests.cpp`, `UI/Search/ParameterVisibility.h` | first local run (`logs/local-first-full-run-unit-tests.log.gz`): `PluginBuses.aux8NoiseIsDeclaredLast…`, `GuiReach.everyAutomatableParameterHasAVisibleControl`, `JamPlugin.JM39`, `JM43`, `MidiOutPanel.liveEventsAndWorkshopChanges…`, `ModelGaps.characterSeedAndEnvironment…` | local, exit 0: PluginBuses 10, MidiOutPanel 5, ModelGaps 12, JamPlugin 17, GuiReach 5, Search 48 passing, 0 failing | **local**. CI run #18 contains this commit; none of these tests is in its failure lists (Linux Free summary `5 of 2665 tests failed`, matching its five listed failures; Linux Pro lists six, no summary line seen). Consistent with passing, not proof for Pro. |
| `905fc097` (import of `efa7d27d`) | `CombinationTests.cpp` +3 lines | `Combo.releasedStringIsDampedQuickly`: P-Style Bass note 42 reads 34.7 dB vs 35 dB | **unchanged**, still 34.7 dB locally and in CI run #18 (Pro) | **no effect**; the hypothesis it would help was wrong |
| `6d9048b5` tests: facade tempo glide | `RefPedalApiTests.cpp` | `RefPedalApi.boardFacadeKeepsLegacyContract`: `bpm 120.000000` (expected 133) | `LuthierTests RefPedalApi`: 26 passing | **local**; absent from run #18 failure lists. The assertion was weakened (section 4). |
| `075d69ab` ci: error annotation | `scripts/ci_build.sh`, `scripts/ci_build.ps1` | compile/link errors not readable (logs blocked) | on a failed build the compiler errors appear as a `::error` annotation | **CI**: run #19 produced the annotations from both scripts (macOS = shell, Windows = PowerShell). The success path was not confirmed in CI (Linux jobs were cancelled during tests). Locally only `bash -n` and a fake-log test of the extraction pipeline. |
| `1c9ae70b` Linux CI suite | `ParamMeta.cpp/.h`, 8 `ReferencePedals/Models/*.cpp`, `QualityModeTests.cpp`, `RoutingSweepTests.cpp`, `TuneProcessorTests.cpp` | CI run #18, Linux: `CQ03_theLevelIsNeverInAPreset`, `everyParameterHasAUnitAndACategory`, `rhythmSourceCarriesTheStrum`, `sourceTreeHasNoUnmarkedBrandNames`, `midiOutCarriesTheTuneWhenAskedTo` (reproduced locally) | local, exit 0: each of those tests, plus suites Parameters 23, Trademarks 4, RefPedalApi 26 (`logs/local-verify-after-fixes.log`) | **local only**. CI run #19 compiled on Linux (both editions) but its tests were cancelled, so no CI confirmation. |
| `fca497b2` macOS/Windows | `RefPedalOversamplingOracleTests.cpp`, `CMakeLists.txt`, handoff doc | macOS and Windows compile/link failures (section 5) | **not verified. The Bessel edit was not compiled even on Linux.** | none |
| `e41494bc`, `907dedb9`, this commit | docs only | n/a | n/a | n/a |

Untested hypotheses, marked clearly:

- **Windows**: that the Pedalboard's `/GL` objects are what pushed the library over
  4 GiB, and that `/GL-` on `Source/ReferencePedals/**` and
  `Source/UI/ReferencePedals/**` (guarded by `MSVC AND LUTHIER_LTO`) will recover
  at least 1.55 MiB (Pro) / 4.45 MiB (Free). Not tested on MSVC. I only confirmed
  with a toy project (CMake 3.28.3, GNU-style flags) that per-source
  `COMPILE_OPTIONS` come after target options.
- **macOS**: that the `besselI0` series is the only missing library call. Only one
  error was printed, but link and signing steps were never reached. The failing file,
  `RefPedalOversamplingOracleTests.cpp`, exists **only** on the `dsp` branch (a WIP test
  added by `99234e0d`, imported here as `9e8a35d7`); `completion` and the other siblings
  do not have it. Not importing that WIP test would also remove the error.
- **Bass decay**: whether it is a regression from the integration. The `master`
  baseline that would answer it was about 30% built when stopped
  (`logs/master-baseline-build-INCOMPLETE.log.gz`).

First local full run, for the record: it **did not finish**. `unit-tests.log` ends
inside a performance table with no summary line (1,348 passing, 7 failing lines).
CI has 2,665 tests. Treat my "local full suite" results as partial.

## 4. Test exemptions, changed assertions, thresholds, selection

No test was skipped, deleted, or de-selected. No `--skip`, no threshold constant
(`floorDb`, tolerances) was changed. CI test selection is unchanged.

| Change | Where | What it does | Replacement coverage |
|---|---|---|---|
| **`ref_*` exempt from the reachability walk and from search** (`isPedalboardWindowParameter`: `startsWith("ref_")`, folded into `isIntentionallyHidden`) | `UI/Search/ParameterVisibility.h`; `GuiReachabilityTests.cpp` now calls `isIntentionallyHidden`; also removes `ref_*` from the search index (`SearchProviders.cpp:193,261`) and from `SearchEditorTests` (`:222`) | the `GuiReach` test reported 117 `ref_*` parameters with no control. Arithmetic: 3 board lanes + 64 slot lanes (enabled, in_db, out_db, pos × 16) + 50 extended lanes = 117 | **Partial. The exemption is broader than my justification ("controls live in the Pedalboard window").** UI attachments exist in code for: `ref_board_mix`, `ref_board_enabled`, `ref_board_tempo_bpm` (`BoardRoot.cpp:51-53`) and per-slot `enabled`, `in_db`, `out_db` and pedal lanes (`PedalView.cpp:146-152`), i.e. 51 of the 117. I found **no UI attachment** for the 16 `ref_sNN_pos` lanes or the 50 extended lanes (`ExtendedHostAutomation.h`). Tests that touch this: `RefPedalHost.appendOnlyLaneIdsAndDefaults` (lane existence/order), `RefPedalUiFaces.physicalLaneAttachmentsAndConfigurationUseRealTargets` (slot 1 only, on a synthetic parameter tree). **No test checks that every `ref_*` parameter has a control, and none checks search for them.** Also, the comment I wrote in `ParameterVisibility.h` overstates the case. |
| **CQ03: `cpu` guard skipped for `ref_` ids** | `QualityModeTests.cpp:233-234` | the test forbids any parameter id containing "cpu" (quality mode adds none); `ref_board_cpu_cap` is a Pedalboard host lane | The `quality…` prefix rule still applies to all ids. For `ref_board_cpu_cap` itself: `RefPedalBoardCoreTests` validates the `cpuCapPct` board setting (rejects 0), but **nothing guards that the parameter is independent of the quality controller**. |
| `PluginBuses` expected count `+3` | `PluginBusTests.cpp:78` | accepts Pedal B, Wet only, Dry only | `RefPedalHost.appendedAuxBusesPreserveLayoutAndRenderSlicedSends` checks the last three bus names, layout and rendering. |
| Facade tempo assertion weakened | `RefPedalApiTests.cpp` | first-block `bpm == 133` became `120 <= bpm <= 133`; added a block 4800 samples later that must equal 133 | the later-block check, same test. The 50 ms glide itself is covered by the clock tests in `RefPedalMidiCoreTests` (not re-audited). |
| MIDI-out tests give the processor a capacity-declared buffer | `JamProcessorTests`, `MidiOutPanelTests`, `ModelGapsUiTests`, `RoutingSweepTests`, `TuneProcessorTests` | assertions unchanged; the input buffer now calls `reserveMidiOutputBuffer` because the bounded path drops MIDI when capacity is unknown | unit level only: `RefPedalMidiCoreTests.cpp:326` checks `lowerBound(…, Unknown) == 0`. **No processor-level test that unknown capacity drops and counts events.** |
| Decay test isolation | `CombinationTests.cpp` (+`setBodyAmount(0.0)`) | changes test conditions, not the threshold | n/a (no effect on result) |

Production behavior changes that stand in for test changes: user-visible
"Inspired by" strings (section 7), and a new `ParamUnit::bpm`.

Imported commits also changed tests (45 test files, +3,321/−101 lines versus the
base, all sources combined); I did not audit those for weakened assertions.
Review with `git diff e5d350e5994c96e02277f9c8a00606073df47213 HEAD -- Source/Tests`.

## 5. Windows and macOS failures, and the Linux validator step

Run #18 = `40169506877` (commit `6d9048b5`); run #19 = `40186045042` (commit `1c9ae70b`).

| Platform / edition | Run #18 job ID | Run #19 job ID | Error |
|---|---|---|---|
| Windows Pro | `121097351118` | `121146981640` | `Luthier_artefacts\Release\Luthier Pro_SharedCode.lib : fatal error LNK1248: image size (10018DF91) exceeds maximum allowable size (FFFFFFFF)`; 4,296,597,393 B, over by 1,630,098 B (1.55 MiB) |
| Windows Free | `121097351171` | `121146981601` | same, `Luthier Free_SharedCode.lib`, `1004734D4`; 4,299,633,876 B, over by 4,666,581 B (4.45 MiB) |
| macOS Pro | `121097351172` | `121146981624` | `Source/Tests/RefPedalOversamplingOracleTests.cpp:20:37: error: no member named 'cyl_bessel_i' in namespace 'std'` |
| macOS Free | `121097351162` | `121146981616` | same |

Run #18 failed in "Configure and build" for these; the error text was first
readable in run #19, via the new annotations (logs are not downloadable).

**Fix status.** Windows: `CMakeLists.txt` `/GL-` change in `fca497b2`, **unverified**.
macOS: local `besselI0` in `fca497b2`, **unverified and uncompiled**. The file comes
from the `dsp` WIP import only (section 3, section 9).

**Linux validator step.** Step "pluginval + clap-validator (Unix)" failed in both
Linux jobs of run #18 (Pro job `121097351179`, Free job `121097351122`), the
step ran about 1m40s. The only annotation is "Process completed with exit
code 1."; no validator output was captured in any annotation. **The cause is not
known.** The command is in `scripts/ci_build.sh` `do_validate`: pluginval
(`--strictness-level $PLUGINVAL_STRICTNESS --timeout-ms 600000 --validate
<…/VST3/…vst3>` under `xvfb-run`) and then clap-validator on the `.clap`. I do not
know which of the two failed, or whether it failed because of the preceding
unit-test failures. Not rerun.

## 6. Pedal and model inventory

Registered and selectable (from `PedalRegistry::registerAll`):

- **Generic effects (10):** `generic.boost`, `generic.overdrive`, `generic.fuzz`,
  `generic.compressor`, `generic.chorus`, `generic.tremolo`, `generic.wah`,
  `generic.phaser`, `generic.delay`, `generic.reverb`.
- **Branded (1):** `mxr-dynacomp` (`RegisterDriveB.cpp`).
- **Compatibility prototypes (3):** `prototype.ts808`, `prototype.ds1`,
  `prototype.phase90` ("(prototype)" display names).

Empty registration skeletons (`void register…() {}`, 13 lines each):
`registerDriveA`, `registerLoopers`, `registerModDelay`, `registerMuffs`, `registerRc5`.

The 18 branded models specified in `spec/reference-pedalboard/specs/pedals/`:

| Model id | Runtime implementation | Registered | Status per project ledgers |
|---|---|---|---|
| `mxr-dynacomp` | yes (142 lines) | yes | `PEDALBOARD-DSP.md` "Dyna Comp candidate" (lines 39-45): implemented; attack/release acceptance, FRF/THD/IMD, alias and soak still open. (`PEDALBOARD-ACCEPTANCE.md` still lists WP12 as "skeleton", which is out of date.) |
| `boss-ds1`, `boss-sd1`, `ibanez-ts808` | yes (113 / 207 / 110 lines) | no | "unregistered, unaccepted candidates" (`PEDALBOARD-DSP.md`: 8 cases, 2,382,641 checks, 0 failures; no 30-minute soak, no hardware captures) |
| `mxr-distortion-plus`, `proco-rat` | yes (112 / 199 lines) | no | "candidate checkpoint… neither is registered", measured gaps recorded |
| `boss-ce2`, `boss-ce2w`, `boss-dd3t`, `mxr-phase90-script` | yes (95 / 75 / 206 / 122 lines) | no | "registration gated" until original acoustic/performance gates are met (`PEDALBOARD-TIMEFX.md`) |
| `ehx-big-muff-nyc`, `ehx-deluxe-muff`, `ehx-green-russian`, `ehx-muff-wicker` | **no: 7-line empty skeletons** (shared `ehx_muff_core.h` is also 7 lines) | no | not implemented |
| `boss-rc1`, `boss-rc5`, `ehx-nano-360`, `tc-ditto` | **no: 7-line empty skeletons** | no | named wrappers/registration not done; shared looper machinery exists (`Common/LooperCore`, `PagedLooperCore`, `LooperStorage`, `PhraseLooper.h`) and is tested without named models (`PEDALBOARD-LOOPERS.md`) |

Acceptance-failing items named in the ledgers: boost and fuzz fail their original
gates; alias, 0.5% CPU, 30-minute soak, FRF/THD/IMD checks and hardware captures
are open (`PEDALBOARD-DSP.md`); chorus pitch-depth and wet-chain calibration
unmeasured (`PEDALBOARD-TIMEFX.md`). Product-level failing test: bass release decay
(section 3).

**Correction to my earlier report.** `REPORT-pedalboard-status-2026-10-25.md` section 4.5
said model code exists for the Muffs, RC-1, RC-5, Nano 360 and Ditto. That is
wrong: those are empty skeletons. Ten of the 18 have runtime code (counting
Dyna Comp); eight do not.

## 7. Excluded, removed or renamed; compatibility

- **Nothing excluded or removed from the build.** The only build-system change is
  `CMakeLists.txt`: on MSVC with `LUTHIER_LTO`, `Source/ReferencePedals/**/*.cpp`
  and `Source/UI/ReferencePedals/**/*.cpp` get `/GL-`. They are still compiled.
- **User-visible strings renamed** (`1c9ae70b`, `Source/ReferencePedals/Models/`):

  | File | Field | Old | New |
  |---|---|---|---|
  | `boss_ce2.cpp:27` | displayName / inspiredBy | `BossCe2 (prototype)` / `BossCe2` | `CE-2 style Chorus (prototype)` / `CE-2 circuit` |
  | `boss_ce2w.cpp:22` | displayName / inspiredBy | `BossCe2w (prototype)` / `BossCe2w` | `CE-2W style Chorus (prototype)` / `CE-2W circuit` |
  | `boss_ds1.cpp:24` | inspiredBy | `Boss DS-1` | `DS-1 circuit` |
  | `boss_sd1.cpp:42` | inspiredBy | `Boss SD-1` | `SD-1 circuit` |
  | `ibanez_ts808.cpp:24` | inspiredBy | `Ibanez TS808` | `TS808 circuit` |
  | `mxr_distortion_plus.cpp:23` | inspiredBy | `MXR Distortion+` | `Distortion+ circuit` |
  | `mxr_dynacomp.cpp:31` | inspiredBy | `MXR Dyna Comp` | `Dyna Comp circuit` |
  | `mxr_phase90_script.cpp:38` | inspiredBy | `MXR Phase 90 Script` | `Phase 90 Script circuit` |

  The UI shows these as "Inspired by <x>; independent and unaffiliated."
  (`PedalView.cpp`, `PedalBrowser.cpp`). The originals are in git history
  (`git show 1c9ae70b`) and the branded names remain in the spec files.
  Rationale: `Trademarks.sourceTreeHasNoUnmarkedBrandNames` forbids brand names in
  shipped string literals. Whether to show brand attribution is a legal decision
  I did not make.
- **Identifiers unchanged:** model ids (`boss-ce2`, …), parameter prefixes, and all
  parameter ids. I did not touch `Parameters.cpp/.h`. `displayName`/`inspiredBy`
  are not in the state/preset serializers I searched (`BoardState.cpp`,
  `BoardModel.cpp`, `PresetManager.cpp`, `HostStateFormat.cpp`); a hit elsewhere
  was not ruled out. `ParamUnit::bpm` was appended at the end of the enum and is
  not persisted (only used by `ParamMeta` and tests).
- **Compatibility already in the imported work:** `ref_*` ids are append-only
  (`RefPedalHost.appendOnlyLaneIdsAndDefaults` checks positions relative to the
  end of the parameter list); extended lanes are in `ExtendedHostAutomation.h`.
- **Where original work is preserved:** all source branches are untouched on
  GitHub (`origin/codex/luthier-pedalboard-*`, `origin/codex/pro-beta-resonator-decay-ci`);
  each imported commit records its origin in its `(cherry picked from commit …)`
  trailer; the 6 skipped commits are in section 2.2 with full SHAs.

## 8. CI, release and artifact state

`release.yml` runs (this effort):

| Run | ID | Commit | Result |
|---|---|---|---|
| #17 | `40096845972` | `0f62c3d67bdba055b41e66c7ab8ac9ab4b728662` (pre-pedalboard `master`) | cancelled by me (it built the wrong code) |
| #18 | `40169506877` | `6d9048b5d350e9e50287cc93199203bbb0829595` | **failure**: Windows ×2 link, macOS ×2 compile, Linux ×2 test + validator failures |
| #19 | `40186045042` | `1c9ae70b644b9dd41ea3efb5c787f52d19498f6f` | cancelled by me mid-run. Windows ×2 and macOS ×2 had failed (same errors). Linux ×2 compiled; tests cancelled. |

Earlier runs, not mine: #16 `38849063270` (`master` `ea50a2b5`, failure), #15 `38824616005` (cancelled), #14 `38553461992` (`master`, success).

Artifacts (names; sha256 digests as reported by the GitHub API; I downloaded and
inspected none):

- Run #18, plugin bundles: `Luthier-PAID-linux-6d9048b5d350e9e50287cc93199203bbb0829595`
  (62,088,726 B, `sha256:5bb3af7624a765934ad8f9837eda0190dc54e428450b8c0fe8f6cd03dd8a3566`),
  `Luthier-FREE-linux-6d9048b5d350e9e50287cc93199203bbb0829595`
  (62,023,062 B, `sha256:c3923b0431505891d5928663e19e33c23a82c5cd0845ad01ef37729fee83ff03`).
  **These are staged outputs of a run whose unit tests and validators failed.**
  Not release artifacts. Retention is 14 days.
- Run #18 and #19 log artifacts: `logs-{PAID,FREE}-{linux,windows,macos}-<sha>` (small
  zips; digests via `gh api repos/djshellshoxxx/luthier/actions/runs/<id>/artifacts`).
  The storage host is blocked from the sandbox, so they were never read.
- Run #17: log artifacts only.

**Where no working release artifact exists.** For this effort there is none on
any platform: no installer, VST3, CLAP, AU or standalone passed its gates. Windows
and macOS produced no plugin artifact at all. The "Draft GitHub Release" job was
skipped in #18 and never reached in #19. No tag `v0.1.0` and no new release was
created; no PR was opened. Pre-existing and unrelated: draft `v1.0.0-beta1`
(13 assets, 2026-10-09) and prereleases `v0.1.0-alpha1`, `v0.1.0-macos-beta1`.

## 9. What the next integrator must know

Handoff path: `docs/handoff/INTEGRATION-HANDOFF-2026-10-25.md` (this file), with
`docs/HANDOFF-pedalboard-release.md` for operational detail.

1. **Stale base. The sibling branches moved after I based on them** (fetched
   2026-10-25 ~11:30 UTC):

   | Branch | My base | Now | New commits |
   |---|---|---|---|
   | `…-completion` | `e5d350e5994c96e02277f9c8a00606073df47213` | `b5bec374f01ede236350300770f5721709a39831` | 122 |
   | `…-dsp` | `e59496a7c7880688ab7fe2fcd264fb880231e5c3` | `933ceea89a413232d7fb526096f6553fecb9082c` | 29 |
   | `…-engine` | `e55902776b30e66d7efaf4abca99f294199755db` | `327fe2f03ba94c563cb7e336ffd5921d1bbbf017` | 33 |
   | `…-host-midi` | `6b0c4676ab1aeec97678825d5863b59cf7d63276` | `1f5177270d3f951acfa69ccc64cb0fca2e625be5` | 2 |
   | `…-state` | `59305029099b7eeb299e672bea467c8381ea41c1` | `739a79be78d54fc8a81a5268866385530044c7b5` | 14 |
   | `…-timefx` | `cb62daa833799270917647b182e9dec722e64ccd` | `cbc19186c873fb0857ebe0230198cca850604b94` | 27 |
   | `…-ui-full` | `881aec2c92377055b924a1bfd713dc483f14eb6c` | `cacda2c31b2a1da28fdb3b968f606421ddf937d9` | 15 |
   | new: `codex/luthier-checkpoint-20261025-midi-host` | n/a | `9773fca8205188df23f12ad3bc2fad1eaad4c500` | 125 ahead of my HEAD |

   Re-derive the integration from the new tips rather than extending my branch.
2. **Upstream already fixed some of what I fixed, differently** (checked at the new
   tips with `git grep`):
   - `std::cyl_bessel_i`: not fixed upstream. The failing test file
     (`RefPedalOversamplingOracleTests.cpp`) exists only on `dsp`, and the current `dsp`
     tip still calls `std::cyl_bessel_i`; the new `completion` and the other siblings do
     not contain the file. Either skip importing that WIP test or keep a local Bessel
     as in `fca497b2`. (The `besselI0` that `git grep` finds elsewhere is an unrelated
     lambda in `Presets/Preview/ToneFeatures.cpp`.)
   - BPM unit: the new `completion` has `ParamUnit::beatsPerMinute`. Mine is
     `ParamUnit::bpm`: **do not take both.**
   - Brand names in `Models/*.cpp` string literals: none remain at the new
     `completion` tip (done differently); `dsp` still has `"Boss DS-1"`.
   - GuiReach: the new `completion` walks real Pedalboard instances and expects
     `enabled, in_db, out_db, pos` and every pedal lane per slot. That supersedes my
     `ref_*` prefix exemption for slot lanes; prefer it.
   - MIDI-out test buffers: `checkpoint-20261025-midi-host` adds
     `reserveMidiOutputBuffer` in three test files.
   - Not found upstream: my `/GL-` Windows change (no 4 GiB handling in `CMakeLists.txt`),
     the CQ03 `ref_` allowance, the facade-glide test change, the CI error annotations.
     These may still be needed.
3. **Unverified items** (section 3 list) must not be treated as fixes. In particular
   `fca497b2`.
4. **The Windows library limit is a real blocker** unless upstream changed the
   build in a way I did not find; check the new tips for MSVC-specific code first.
5. **CI logs are unreadable from this sandbox**
   (`productionresultssa10.blob.core.windows.net` is denied). Annotations work:
   `gh api repos/djshellshoxxx/luthier/check-runs/<job-id>/annotations`.
6. **WIP candidates are compiled in but unregistered**; keep them out of any release
   configuration until their gates pass.
7. **Known wrong statements of mine, now corrected:** the REPORT's model-code claim
   (section 6) and the `ref_*`-controls-in-the-Pedalboard-window claim (section 4).
8. `docs/review/QA_ROBUSTNESS.md` is rewritten by every test run; revert it rather
   than commit it.

Preserved logs (`docs/handoff/logs/`): `local-first-full-run-*.gz` (incomplete
local run), `local-verify-after-fixes.log`, `master-baseline-build-INCOMPLETE.log.gz`,
`local-rerun-suites-filter-bug.log.gz` (output hidden by my own grep filter; low
value), and the cherry-pick lists. No secrets: scanned for keys/tokens; the only
keyword hit was a test name.
