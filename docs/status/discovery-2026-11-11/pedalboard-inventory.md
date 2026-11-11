# Pedalboard feature inventory — branch I (`integration/luthier-completion` @ `ca14934e`)

Read-only discovery, 2026-11-11. No files in the repo were modified; no build/test was run.
Everything below is from source reads, `git log`, and repo docs. Items not checked are marked **UNVERIFIED**.

## 0. Which documents are newest (precedence)

| Rank | Document / evidence | Date | Why |
|---|---|---|---|
| 1 | Commit bodies `15e5ce38`, `8f52d04f`, `ca14934e` (2026-11-04) + PR #85 body (REST `gh api repos/djshellshoxxx/luthier/pulls/85`) | 2026-11-04 | Newest; only these postdate the handoffs. Test/CI-only changes. |
| 2 | `docs/handoff/PEDALBOARD-2026-10-28-CONTINUATION.md` | last touched by `a8f878d5`/`c7a78559` 2026-10-28 | Newest handoff; states six-model status table, owner decisions, Windows evidence. |
| 3 | `docs/handoff/SIX-PEDAL-COORDINATOR-MAP-2026-10-27.md` | 2026-10-27/28 | Live map the continuation adds to. |
| 4 | `docs/handoff/LUTHIER-CURRENT-COMPLETION-2026-10-27.md` | 2026-10-27, source `4ceb1b5d` | Supersedes LUTHIER-FEATURE-INCOMPLETENESS-2026-10-26 (it says so, line 26 of the latter). **Stale on RC-5**: it calls the RC-5 wrapper a skeleton; superseded by (2) Slice 4 (`66d3e65a`). |
| 5 | `docs/handoff/PEDALBOARD-SIX-MODEL-SCENARIO-2026-10-26.md` | 2026-10-26 | Scope decision (authoritative for the "6"). Its per-model gate text is older than (2). |
| 6 | QA-CHECKPOINT / QUALIFICATION-REVIEW / BUFFER-SIZE-1-4 (2026-10-26) | 2026-10-26 | Older. Used only for the heap-corruption item. |

Where docs conflict, (2) wins over (4)/(5). Example: (4) says "RC-5 model wrapper is still a skeleton"; (2) Slice 4 says wrapper v1 exists, and `Source/ReferencePedals/Models/boss_rc5.cpp` (289 lines) confirms it.

## 1. Scope decision: "18 → 6"

**Authoritative statement:** `docs/handoff/PEDALBOARD-SIX-MODEL-SCENARIO-2026-10-26.md`, heading "Six named pedals first — confirmed initial scope, RC-5 retained" and "Confirmed initial selection and remaining work":
- "The initial six are now **Phase90, CE-2, DD-3T, CE-2W, Dyna Comp and RC-5**; provisional Distortion+ moves to updates." The owner asked for an advanced looper, so RC-5 replaced Distortion+.
- "This means six of the eighteen **named** models. The existing ten generic effects are not counted as named models or silently removed. The three explicitly provisional compatibility processors do not satisfy named acceptance."
- The twelve deferred models are DS-1, TS808, SD-1, Distortion+, RAT, NYC Muff, Deluxe Muff, Green Russian, Wicker Muff, Ditto, Nano360 and RC-1. Their source, specs and tests are kept.
- Also: `docs/coverage/PEDALBOARD-ACCEPTANCE.md:356` (R1: "six selected named models at original gates; twelve other named models explicitly deferred, no registered placeholders"); `LUTHIER-CURRENT-COMPLETION-2026-10-27.md:99`.
- Commits: `7a79c0cc` "docs: correct Phase90 status and compare six-model release scenario"; `ae67e429` "docs: retain RC-5 in initial six…".

**What 18 and 6 mean:**
- **18** = the number of *named, real-pedal-modelled model types* in the reference-pedalboard spec. There are exactly 18 files in `spec/reference-pedalboard/specs/pedals/`, and 18 model header/source pairs in `Source/ReferencePedals/Models/`.
- **6** = the named model types in the *initial release catalogue*. It is **not** a slot count, a per-board instance cap or a default board.
- No code constant encodes "6" or "18".

**What the code enforces today:**
- Board capacity: `kMaxSlots = 16` (`Source/ReferencePedals/BoardTypes.h:22`, comment "UI shows 8 by default").
  - Enforced at `TopologyCompiler.cpp:34` ("Board exceeds 16 pedal slots"), `BoardModel.cpp:356,380,746`, `BoardState.cpp:420` and `PedalBrowser.cpp:24` (Add disabled at 16, tooltip "Board capacity is 16 pedals").
- Host automation block: `3 + kMaxSlots*20` params (`BoardTypes.h:111`).
- Snapshots: at most 8 (`BoardState.cpp:327`).
- Catalogue offered = whatever is registered, through `PedalBrowser::refresh()` iterating `PedalRegistry::all()` (`Source/UI/ReferencePedals/PedalBrowser.cpp:16`). Registered today:
  - 10 generic models: Boost, Overdrive, Fuzz, Compressor (`Generic/RegisterGenericA.cpp:9-14`); Chorus, Tremolo, Wah, Phaser (`RegisterGenericB.cpp:10-15`); Delay, Reverb (`RegisterGenericC.cpp:8-11`).
  - 3 prototypes: ts808/ds1/phase90 prototypes (`Models/RegisterPrototypes.cpp:11-16`).
  - **Total 13. All 18 named models are unregistered.** Every named register function is an empty body: `RegisterModDelay.cpp:11`, `RegisterDriveA.cpp:11`, `RegisterDriveB.cpp:11`, `RegisterMuffs.cpp:11`, `RegisterLoopers.cpp:11`, `RegisterRc5.cpp:11`. They are called from `PedalRegistry.cpp:240-252`.
- Edition: no Free/Pro gating found in `Source/ReferencePedals` or `Source/UI/ReferencePedals` (a grep for `Edition::`/`LUTHIER_FREE` returned nothing). The board appears to have the same capacity and catalogue in Free (**UNVERIFIED at runtime**). Legacy-rack Free gating lives in `Source/Edition.h:102-162` (`isFreePedalIndex`, `effectivePedalIndex`).

## 2. The six named models

Common facts:
- None is registered. The relevant empty functions are `registerModDelay()` (Phase90/CE-2/CE-2W/DD-3T, `RegisterModDelay.cpp:11`), `registerDriveB()` (Dyna Comp, `RegisterDriveB.cpp:11`) and `registerRc5()` (`RegisterRc5.cpp:11`).
- Ledger policy: registration only after the original gates pass (`PEDALBOARD-2026-10-28-CONTINUATION.md` "Six-model status": "0/6 registered").
- Descriptors carry `inspiredBy=nullptr` and display names ending "(prototype)".

| Model | Source | Descriptor (path:line) | Params | Test suites (count of LUTHIER_TEST) | Open gates (newest handoff) | Remaining work |
|---|---|---|---|---|---|---|
| Phase 90 (Script/Block) | `Models/mxr_phase90_script.{h,cpp}` (158 lines) | `mxr_phase90_script.cpp:38` id `mxr-phase90-script`, 13 params, BypassMode::True, EqualPower fade | speed, bypass, variant, depth_trim, mix_trim, stage_mismatch, sync, sync_div, stereo_phase, ext_speed, reset_on_stomp, noise, stereo_mode | RefPedalPhase90 (13, `ReferencePedals_mxr_phase90_scriptTests.cpp`), RefPedalPhase90Memory (4) | CPU <0.4 % **RED** (0.419–0.78 % on 2.1 GHz Xeon after exact-output optimisation in `4dd4adc2`); 24-bit click criterion "peak delta <1 LSB 24b" unresolved since 2026-10-26 (`PEDALBOARD-2026-10-26-PHASE90-BYPASS.md:36-49`). Both are probes, not committed tests (coordinator map, "Gates that are not committed tests") | Owner rulings on CPU reference machine and click criterion; encode CPU gate as a test; registration; host/state/automation end-to-end; native qualification |
| CE-2 | `Models/boss_ce2.{h,cpp}` (111 lines), `Common/BbdCore.*` | `boss_ce2.cpp:27` id `boss-ce2`, 14 params, Buffered, EqualPower | rate, depth, bypass, variant, bbd_mode, noise, sync, sync_div, sync_reset, tap_enable, at_depth, lfo_phase_offset, mono_out_dup, stereo_mode | RefPedalCe2 (6), RefPedalCe2NodeBypass (1), RefPedalBbdCore (3), RefPedalChorusMemory (4, shared with CE-2W) | `RefPedalCe2.bbdOnOriginalThdGate` **RED** on all platforms (see §6); raw on/off jump RED 12/60 cells, max 0.0014990 > 0.001 (probe; conflicts with the specified 5 ms equal-power law); CPU ~0.66 % contended vs <0.4 % | Owner decisions #2/#3; CPU; registration |
| DD-3T | `Models/boss_dd3t.{h,cpp}` (270 lines) | `boss_dd3t.cpp:35` id `boss-dd3t`, 19 params, Buffered, has tails flag | elevel, feedback, time, mode, active, loop_hold, tap, tap_func, trails, time_law, bits, conv_sim, sync, sync_div, at_fb, mono_out_dup, stereo_split, variant, stereo_mode | RefPedalDd3t (14), RefPedalDd3tMemory (5), RefPedalDd3tNode (3) | CPU <0.5 % open (~0.9 % contended) | CPU; remaining acoustic/mode-transition gates; registration and host/MIDI/UI flow |
| CE-2W | `Models/boss_ce2w.{h,cpp}` (101 lines) | `boss_ce2w.cpp:22` id `boss-ce2w`, 9 params, MonoInStereoOut | rate, depth, mode, bypass, out_mode, sync, sync_div, noise, hq | RefPedalCe2w (6), RefPedalChorusMemory | CE-1 THD 1–3 % criterion ambiguous: measured A 0.42 %, B 1.05–1.13 %, vibrato 0.81 %. Owner must say which output and depth. CPU ~1.5 % contended vs 1 % | Owner decision #4; CPU; registration |
| Dyna Comp | `Models/mxr_dynacomp.{h,cpp}` (177 lines) | `mxr_dynacomp.cpp:39-56` id `mxr-dynacomp`, 10 params, True bypass, latency 23, tails | sensitivity, output, attack, bypass, variant, noise, quality, expr_target, mix, stereo_mode (`mxr_dynacomp.cpp:28-38`) | RefPedalDynaComp (4), RefPedalDynaBoundaries (3), RefPedalDynaNodeBypass (1) | CPU RED (0.93–1.78 % vs ≤0.5 % on a 3 GHz core); THD at max SENS 0.017 % vs 3–5 % RED; FAST attack 1.22–1.29 ms vs 5 ms ±50 % RED; THD at default SENS passes (0.183 %); release passes (1.247 s); "90 cells" of original THD/attack failures retained | Owner decision #5 (detector RC vs GR 63 % time; topology for max-SENS THD); CPU; registration |
| RC-5 | `Models/boss_rc5.{h,cpp}` (289 lines, wrapper v1 `66d3e65a`), `rc5_rhythm.*`, `rc5_storage.*`, `rc5_patterns.inc`, `Common/{PagedLooperCore,LooperStorage,LooperStateCodec,Rc5*}` | `boss_rc5.cpp:44-45` id `boss-rc5`, 23 params, Stereo, Buffered | loop_level, rhythm_level, rhythm_on, tempo, pattern, variation, kit, memory, reverse, pedal1-3, sync_clock, quantize, exp_target, exp, hold_ms, dtap_ms, lofi, ram_cap_mb, dry_mute, half_speed, clock_out | RefPedalBossRc5 (14), RefPedalRc5Double (2), RefPedalRc5MemoryBank (13), RefPedalRc5Rhythm (11), RefPedalRc5TapTempo (10), RefPedalPagedLooper (13), RefPedalPagedReverse, RefPedalLooperCore (7), RefPedalLooperState, RefPedalLooperStorage* | Not done: 99-memory audio switching (needs a message-thread `restore` hook, owner decision #7 / WP1 frozen-interface erratum); MIDI PC/CC80-87 (`wantsRawMidi` false); quantize/auto length; tap-tempo button; half-speed (param has no effect); live reverse toggle; MIDI clock out; 30-min soak; 90-min spill; native host validation. TSan whole-target cannot link (focused harness only) | Largest remaining item (see tasks 2-4) |

Sanitizer evidence (newest): ASan+UBSan on the six-model cohort at `0b5af0c3` passed 129 tests / 199,110 checks. The only failure was the known CE-2 RED. TSan was not run (CONTINUATION "Sanitizer evidence").

## 3. Legacy (v1) pedals vs the reference board

- **Legacy racks still exist and are untouched.**
  - `Source/DSP/Effects/Pedal.h:21-40` `enum class PedalType` has 23 types: 21 effects plus Doubler and Gater. This matches `spec/README.md:102` "23 pedals … across two eight-slot, reorderable racks".
  - `EffectsChain.h:22` `kNumSlots = 8` per rack.
  - The UI is `Source/UI/PedalRack.h:173`, instantiated as `preRack`/`postRack` in `Source/UI/AdvancedPanel.cpp:989,1092`.
  - Free gating: `Edition.h:102-162`. There is a spec inconsistency: "15 of 22" vs 23 types (`spec/design/v2/pedalboard/05-edition-gating.md:28`).
- **The reference board ("RefPedal*", `Source/ReferencePedals/**`, UI `Source/UI/ReferencePedals/**`) is a separate, parallel subsystem.**
  - `PluginEditor.cpp:74`: "Independent experimental reference pedalboard; legacy racks are untouched." `Board.h:6` says the same of EffectsChain/PedalType.
  - Audio: `LuthierEngine.cpp:3485` (`Placement::beforeExistingPre`) and `:3553` (`afterExistingPost`). The board processes before the legacy pre chain and/or after the legacy post chain, depending on each pedal's placement.
  - State: saved/restored in host state at `PluginProcessor.cpp:3791/4031`.
  - The legacy `Model` enum (`LegacyModel.h:13`: empty, ts808Prototype, ds1Prototype, phase90Prototype) is a compatibility shim for the old prototype board (`Board.h:33` "prototype had 8").
- **What the user sees: both.**
  - The Advanced panel shows the two legacy 8-slot racks.
  - The header button `ref_board_toggle` ("Show/hide pedalboard", Ctrl+Shift+B, `PluginEditor.cpp:75-85,813`) opens the reference board. It docks as a drawer if the editor is ≥840 px tall, otherwise it pops out (`PluginEditor.cpp:1734-1766`).
  - The board browser lists only registered models: 10 generic + 3 prototypes, with categories "Generic"/"Reference" (`PedalBrowser.cpp:10-24`).
  - None of the six named models is visible to users.

## 4. Board features

| Feature | Spec / handoff ref | Implemented? | Validated? + evidence | Defects / failures | Remaining | Acceptance test |
|---|---|---|---|---|---|---|
| Add / remove | `spec/reference-pedalboard/specs/BOARD_CORE.md`; `BoardCommand.h:17,25` | Yes (`cmd::Add`, `cmd::Remove`) | RefPedalBoardCore (59 tests), e.g. `instanceMoveRemoveAndUndo`; RefPedalUi | none recorded | — | RefPedalBoardCore.instanceMoveRemoveAndUndo |
| Reorder / move | `BoardCommand.h:27` | Yes | `poolKeepsPedalIdentityAcrossReorderAndControls`, `outputIsIndependentOfHostBlockPartitionIncludingReorder`, `legacyMoveRetainsAllSixteenSlotsWithSparsePedals`, `ReferencePedals.routingOrderIsAtomicAndStable` | — | — | same |
| Duplicate (Ctrl+D) | `BoardCommand.h:26` | Yes | `RefPedalBoardCore.duplicatePreservesControlsAndHasIndependentIdentity` | Windows native: `RefPedalUiLifecycle.tenThousandNativeOperationsAndEditorDestruction` Ctrl+D added nothing (Pro, line 197, run 40649780764). Root cause found as resize/aspect (5:3 constrainer → 768 px < 840 px dock floor); test fixed `15e5ce38`. Re-run result **UNVERIFIED** | Confirm green on Windows CI | RefPedalUiLifecycle.tenThousandNativeOperationsAndEditorDestruction |
| Bypass (per pedal, board) | `BoardCommand.h:36,46`; spec §bypass per model | Yes; equal-power 5 ms fade owned by board for buffered models | RefPedalRouting (19 tests) e.g. `slotBypassAndBoardDryHaveIdenticalLatency`, `bypassModesPreserveColourLatencyAndDecayingTailsWithTrimCorrection`; RefPedalBufferedTimeFx.`boardOwnsFadeAndBypassHistoryIsIndependent`; RefPedalCe2NodeBypass, RefPedalDynaNodeBypass | CE-2 raw-jump probe RED (definition conflict); Phase90 24-bit click criterion open | Owner rulings | as listed |
| Undo / redo | `BoardModel.h:63-70` (100 steps, drags coalesce, transactions) | Yes | RefPedalBoardCore (`transactionIsOneUndoStep…`, `emptyAndRejectedTransactionsDoNotConsumeUndo`, `undoDuringOpenTransactionWaitsUntilCommit`); RefPedalHistoryFailure (7, fault-injected allocation) | MSVC terminate in HistoryFailure fixed `c7a78559` (Board.cpp change-key; JUCE double formatting allocates under MSVC STL inside `noexcept`) | — | RefPedalHistoryFailure.* |
| Drag-drop | `UI/ReferencePedals/DragController.*` | Yes | RefPedalUi.`dragDwellThrottleFinalCommitAndUndo`, `cancelledDragPreservesExistingRedoAndApplyOnDrop`, `canvasGhostCancellationAndPlacementDropUseOneTransaction`; RefPedalUiFaces.`liveDragRecordsOnlyAcceptedFinalPositionAndBalancesEveryExit` | — | Complex cabling UI incomplete (CURRENT-COMPLETION row "Board UI") | — |
| Keyboard | `UI_BOARD_STYLES.md` (keyboard + screen reader); `docs/KEYBOARD_SHORTCUTS.md` | Yes (reorder, knob fine/coarse, Ctrl+Shift+B, Escape) | RefPedalUi.`keyboardReorderUsesOneUndoStep`; RefPedalUiFaces.`keyboardKnobUsesFineAndCoarseSteps`, `keyboardReorderRecordsAcceptedHostPositionInsideGesture` | — | Screen-reader/a11y manual checks (O) | — |
| Presets | snapshots use "same schema as a board preset" (`BoardSnapshots.h:7`) | Board state persists in host state (`PluginProcessor.cpp:3791/4031`); a separate board-preset library/browser was **not found** (**UNVERIFIED**) | RefPedalState (23) e.g. `eightSnapshotsSurvivePersistence`; RefPedalBoardCore.`stateSnapshotRoundTrip` | — | Decide whether board presets are in scope | — |
| MIDI map / learn | `spec/reference-pedalboard/specs/MIDI_EXPRESSION.md`; `UI/ReferencePedals/BoardMidiPanel.*`, `MidiLearnOverlay.*`; `cmd::SetMidiBinding` | Partial: bindings, learn, footswitch notes, expression | RefPedalMidiCore (34) e.g. `learnNeedsDistinctValuesAndSuppresses`, `heldNotesReleaseOriginalSlotAcrossReorder`; RefPedalMidiGesture (3), RefPedalMidiPublication (2) | — | Full PLL/jitter/output clock, learn conflicts, macro/NRPN/scene/bank workflows (CURRENT-COMPLETION row "Host automation/MIDI/clock/expression", 30–50 % remaining) | — |
| Scenes | `cmd::SetScene` `BoardCommand.h:55` | Yes | RefPedalBoardCore.`sceneCommandsPreserveBankAndMakeRecallOneUndoableIntent` | — | Scene/bank MIDI workflows | — |
| Snapshots | `cmd::ApplySnapshot`; max 8 `BoardState.cpp:327` | Yes | RefPedalState.`eightSnapshotsSurvivePersistence`; RefPedalBoardCore snapshot tests (4); RefPedalControlRace.`snapshotRestoreAllowsSaveExtraToReclaimEngine` | macOS: `RefPedalControlRace.saveExtraExceptionReacquiresGuardAndAllowsRetry` failed at `25cd2135` (see §8) | Named-model extras lifecycle | — |
| Looper | RC-5 / DD-3T Short Loop / `PhraseLooper.h` | Board-level loopers: RC-5 wrapper v1 (unregistered); DD-3T Short Loop (unregistered); `PhraseLooper` with `ReferencePhraseLooperTests.cpp` | See §2 RC-5 suites; paged looper origin-underrun defect fixed (`bdfef8d4`, test `RefPedalPagedLooper.realtimeRecordThenImmediatePlay…`) | Paged-looper grind readiness "exit 1, 81 observations" (coordinator map) | See RC-5 | — |
| Tempo sync | `cmd::SetBoardParam`, extended tempo source | Yes (host clock, MIDI clock in, tap per model) | RefPedalBoardCore.`extendedTempoSourceUsesCanonicalPriorityAndExplicitModelIntent`; RefPedalHost.`extendedTempoSourceAndBpmWithoutMessagePump`; RefPedalMidiCore.`clockTempoIndependentOfRateAndBlockPartition`, `tempoControlIntentAndCanonicalGlide`; RefPedalGenericB.`syncClockConverges`; RefPedalGenericC.`delaySyncAndTapPersistence` | — | MIDI clock out, PLL/jitter | — |
| Paint perf (16 pedals) | `UI_BOARD_STYLES.md:201` T-UI-PERF | Yes | RefPedalUi.`cachedSixteenPedalPaintBudget` | Failed Windows p99 (run 40678198637); best-of-3 batches `8f52d04f`; re-run **UNVERIFIED** | — | same |
| Dock / popout | `PluginEditor.cpp:1734-1766` | Yes; dock refuses below 840 px by design | RefPedalUi.`compactDockRetainsRejectionThroughPresentationUpdates`; RefPedalUiLifecycle | Free Windows: "root not in a Drawer after onDock()" (line 155); same 5:3 aspect root cause, fixed by `15e5ce38` | Confirm CI | — |

## 5. Open release gates (newest first)

From `PEDALBOARD-2026-10-28-CONTINUATION.md`, the 2026-11-04 commits and PR #85:

1. Owner decisions blocking the original gates (CONTINUATION "Owner decisions needed"):
   1. CPU reference machine, for Phase90 / CE-2 / DD-3T / Dyna (all RED on the 2.1 GHz Xeon).
   2. CE-2 BBD-on THD.
   3. CE-2 raw jump definition.
   4. CE-2W CE-1 THD output/depth.
   5. Dyna attack definition and max-SENS THD.
   6. Phase90 24-bit click.
   7. RC-5 memory switching interface hook.
   8. Optional: nominative "Inspired by" provenance text.
2. 0/6 named models registered (R1 pending, `PEDALBOARD-ACCEPTANCE.md:356`).
3. RC-5 remaining features (§2).
4. Windows CI at `c7a78559`:
   - Pro 2,922 tests with 2 failures: CE-2 RED and RefPedalUiLifecycle.
   - Free 2,905 tests with 2 failures: the same two.
   - Later Windows run 40678198637 also failed `cachedSixteenPedalPaintBudget`.
   - Fixes `15e5ce38` and `8f52d04f` are not yet confirmed by CI (**UNVERIFIED**).
5. macOS at `25cd2135` (PR #82 digest, quoted in PR #85):
   - the unit-test run ended with no summary line;
   - AU pluginval segfaulted in Automation (Pro);
   - four tests failed: `aSessionSurvivesThePrepareThatFollowsIt`, `convolution*` (Free), `saveExtraExceptionReacquiresGuardAndAllowsRetry`, `CQ23`.
   - PR #85 says "This run gives fresh evidence". The run result is **UNVERIFIED** (not read).
6. Ledger R2–R9 all "pending" (`PEDALBOARD-ACCEPTANCE.md:356-364`): numerics, RT/concurrency (TSan whole-target cannot link because the test harness overrides global operator new), seeded-1000 state fuzz, 30-min soak, native validators per platform, manual DAW/listening, packaging.
7. Native qualification: 0/4 platform cells qualified (`LUTHIER-CURRENT-COMPLETION-2026-10-27.md:22-23`). The release order given by the owner is: Windows standalone → macOS Intel+ARM → VST3 → CLAP → Linux (coordinator map "Release order").
8. Estimate (low confidence): 55–65 % overall / 65–75 % implementation for the six-pedal release (CURRENT-COMPLETION). This predates RC-5 wrapper v1.

## 6. `RefPedalCe2.bbdOnOriginalThdGate`

- **Location:** `Source/Tests/ReferencePedals_boss_ce2Tests.cpp:65-95`.
- **What it measures:**
  - Fixture: `BossCe2` with depth (param1)=0, BBD sim (param4)=1, noise (param5)=0.
  - Input: a 1 kHz sine at −12 dBFS, 2 s at 1-sample blocks, at 48 / 44.1 / 88.2 / 96 / 192 kHz.
  - Output: coherent DFT over the last 1 s; THD = sqrt(Σ harmonics 2..9) / fundamental.
- **Threshold:** 0.005 ≤ THD ≤ 0.03 (0.5–3 %). This is spec §9, tagged [E] (estimated).
- **Measured:** 0.0003–0.0097 % at all five rates (coordinator map baseline; CONTINUATION slice 2). It fails 5 checks.
- **Root cause (measured):**
  - At −12 dBFS the input shaper is linear (onset 0.5).
  - The ~150 kHz BBD clock zero-order hold adds no harmonics.
  - The model has no BBD transfer nonlinearity.
- **History:**
  - `4dd4adc2` added a fitted even/odd term (`x += even*(x²−⟨x²⟩) − odd*x³`, even .02 / odd .03). It measured 0.90–1.08 %.
  - `6e2cb7f7` reverted it. A prior DSP review (`PEDALBOARD-2026-10-27-REVERSE-DSP-GATES.md`) had ruled curve-fitting unsupported. The fit needed about 9× the datasheet-typical distortion: the forum-quoted MN3007 figure is 0.5 % typ at 0.78 Vrms, which gives about 0.1 % at 0.18 Vrms.
  - The gate stays honestly RED. Source is bit-identical to `97d3b013`.
- **Owner decision requested — options:**
  - (a) revise the [E] 0.5–3 % threshold;
  - (b) supply level/transfer data (or evidence that the CE-2 drives the BBD well above unity);
  - (c) approve an explicit estimated behavioural rationale, which would allow re-applying a nonlinearity like `4dd4adc2`.
- **Companion test:** `RefPedalCe2.bbdOnHeadroomGateKeepsThirdHarmonicDominant` (−3 dBFS, 2–6 %, 3rd harmonic dominant). It passes on the unmodified model at 2.02–2.42 %. The 192 kHz margin is thin (2.015 %).

## 7. Buffer-size 1–4 heap corruption

- **Report:** `docs/handoff/BUFFER-SIZE-1-4-HEAP-CORRUPTION-2026-10-26.md`.
  - Trigger: `prepareToPlay` with max block 1–4 plus a preset that changes cabinet/body IR (`Clean Double-Cut Funk` at 8 kHz).
  - Symptom: glibc heap aborts.
- **Status:** mitigated, root cause not proven.
  - Fix commit `63178fcb` (2026-10-26), "fix: preserve preview fidelity and tiny-block convolution safety". Cabinet, body and IR-variant `juce::dsp::Convolution` are prepared with an internal minimum block capacity of 8, while the host's real 1–4-sample callbacks are still processed.
- **Regression tests:**
  - `Fuzz.convolutionPreparesSafelyForTinyHostBlocks` (`Source/Tests/QA_Robustness.cpp:399`): 1–4 samples at 8 kHz, 20,000 blocks per preset.
  - `Fuzz.convolutionTinyHostRateAndCapacityBoundaries` (`QA_Robustness.cpp:410`): {8, 44.1, 192 kHz} × {1, 2, 3, 4, 5, 7, 8} (`d5b64ab6`).
  - Strict ASan/LSan/UBSan: 2 tests / 11,029 checks / exit 0.
- **Still open:**
  - The handoff says the byte-level root cause is "unproven"; Valgrind was unavailable.
  - The broad `Fuzz.presetLoadAtTrulyExtremeBufferSizes` still excludes 1–4 (`safeBlockSizes` 16+).
- **Related Free RED:** the Linux Free RED on these two tests (QA_Robustness.cpp:332 "alternate preset did not reload the body convolution") was a fixture issue. Free maps Strat→Tele, so the alternate preset loaded the same body IR. It was fixed in `77ade0bc` (fixture uses distinct shipped IRs; `PEDALBOARD-2026-10-26-TINY-BOUNDARY-FREE.md` "Confirmed fixture cause").

## 8. macOS failures in PR #85 (at `25cd2135`)

`77ade0bc` (the Free convolution fixture fix) **is** an ancestor of `25cd2135` (checked with `git merge-base --is-ancestor`). So the macOS Free `convolution*` failure is not the already-fixed Linux fixture failure.

| Test | Location | What it checks | Root-cause hypothesis in repo docs |
|---|---|---|---|
| `HostState.aSessionSurvivesThePrepareThatFollowsIt` | `Source/Tests/HostStateTests.cpp:204` | Restores a session then calls `prepareToPlay(44100,512)`; LFO breakpoints and sequencer step survive (1e-9); re-saved state exactly equals saved JSON (`sameState`) | Only an older audit, `docs/audit/CODEX_AUDIT_FINDINGS.md:15` (F07): a full non-Combo run failed it because the saved JSON differed in 14 normalised parameter floats (e.g. `bend_range` 0.108979657292366 vs 0.108979664742947, float vs double round trip). Not linked to macOS by any handoff. **No macOS-specific hypothesis.** |
| `Fuzz.convolution*` (Free) — `convolutionPreparesSafelyForTinyHostBlocks` / `convolutionTinyHostRateAndCapacityBoundaries` (possibly also `Cabinet.convolutionMatchesOfflineConvolution`, `EngineSweepTests.cpp:468`; the PR wildcard is ambiguous) | `QA_Robustness.cpp:399,410` | Tiny-block convolution install/reload, distinct IRs, finite nonzero output, destruction/churn | None for macOS. The Linux Free fixture cause is already fixed (above). **UNVERIFIED** which assertion failed on macOS. |
| `RefPedalControlRace.saveExtraExceptionReacquiresGuardAndAllowsRetry` | `Source/Tests/RefPedalControlRaceTests.cpp:113` | A pedal whose `saveExtra` throws makes `SetParam` fail (`!apply`); the control guard is re-acquired (`BoardEngine.cpp:117-123` `saveExtraOutsideControlGuard` try/catch/relock/rethrow); a retry succeeds; retired plans ≤8 | None in docs. The analogous Windows terminate (exception through a `noexcept` frame, `c7a78559`) postdates `25cd2135`; whether libc++ shows the same pattern is **UNVERIFIED** (my hypothesis only). |
| `CpuQualityUi.CQ23_staticOverlayCountsStateChangesOnly`, `CQ23_lowMeansNoAnimationRepaints` | `Source/Tests/QualityModeUiTests.cpp:499,539` | At Quality Low, static overlay changes only count state changes; no decorative animation repaints | The test comment (`QualityModeUiTests.cpp:519-521`, added 2026-10-19) blames timer-phase misses on a "busy macOS CI message thread" and added a 1.5 s deadline. No newer hypothesis. |
| AU pluginval segfault in Automation (Pro) | CI (`.github/workflows`, `scripts/ci_failure_digest.py`) | pluginval Automation test on the AU build | None. `ca14934e` adds lldb re-run and crash frames to capture a backtrace. Root cause **UNVERIFIED**. |
| macOS unit run had no summary line | — | — | Suggests the test binary crashed or hit a timeout. `ca14934e` now includes the log tail. **UNVERIFIED** |

## (a) Consolidated inventory table

| Feature | Spec/handoff ref | Implemented? | Validated? + evidence | Defects | Remaining | Acceptance test |
|---|---|---|---|---|---|---|
| Board core (add/remove/move/dup/undo/transactions) | BOARD_CORE.md; CURRENT-COMPLETION row 1 | Yes | Linux full RefPedal 527 tests / 12.4 M checks, only CE-2 RED (CONTINUATION slice 5/6) | Windows Ctrl+D lifecycle (fixed in test, unconfirmed) | Final topology workflow | RefPedalBoardCore.*, RefPedalUiLifecycle |
| Routing/bypass/latency | Routing specs | Yes | RefPedalRouting 19 | — | — | RefPedalRouting.* |
| State/snapshots/scenes | BoardState; ACCEPTANCE R5 | Yes (8 snapshots) | RefPedalState 23, RefPedalExtraState 3 | macOS saveExtra exception test | Seeded 1000 malformed-state fuzz (R5) | RefPedalState.* |
| MIDI/automation/clock | MIDI_EXPRESSION.md | Partial | RefPedalMidiCore 34, RefPedalHost 19 | — | Clock out/PLL, NRPN, macro, bank workflows | RefPedalMidi*.* |
| UI (faces, drag, keyboard, dock, styles) | UI_BOARD_STYLES.md | Yes (generic + named faces) | RefPedalUi 17, RefPedalUiFaces*, RefPedalUiStyles 3 | Windows perf/lifecycle (fixed in test, unconfirmed) | Cabling, RC-5 LCD/ring, a11y/DPI | RefPedalUi.* |
| Ten generic pedals | Generic specs | Yes, registered | RefPedalGenericA/B/C | — | Original acoustic/alias/CPU grids (20–35 %) | RefPedalGeneric*.* |
| Phase90 | mxr-phase90-script.md | DSP yes; unregistered | 17 tests | CPU RED; click criterion | Owner + CPU + register | probe → test |
| CE-2 | boss-ce2.md | DSP yes; unregistered | 10+ tests | THD RED; jump RED; CPU | Owner + CPU + register | bbdOnOriginalThdGate |
| DD-3T | boss-dd3t.md | DSP yes; unregistered | 22 tests | CPU | CPU + register | probe → test |
| CE-2W | boss-ce2w.md | DSP yes; unregistered | 6+4 tests | THD criterion ambiguous; CPU | Owner + register | probe → test |
| Dyna Comp | mxr-dynacomp.md | DSP yes; unregistered | 8 tests | CPU, THD-max, attack RED | Owner + DSP + register | probe → test |
| RC-5 | boss-rc5.md | Wrapper v1; unregistered | ~63 RC-5/looper tests | Grind readiness exit 1 | Memories, MIDI, quantize, half-speed, reverse, clock out, soak | RefPedalBossRc5.* |
| Legacy racks (23 types) | spec/README.md:102 | Yes, untouched | existing suites | Free type-count spec inconsistency | Owner decision | — |
| Tiny-buffer safety | BUFFER-SIZE-1-4 | Mitigated (`63178fcb`) | Fuzz.convolution* strict SAN | macOS Free failure | Root cause unproven | Fuzz.convolution* |
| Native qualification | ACCEPTANCE R7 | — | 0/4 cells | macOS AU segfault; Windows 2 fails | All | pluginval/clap-validator |

## (b) Top 15 engineering tasks toward release (ranked)

| # | Task | Files | Size | Depends on | Linux-only doable+verifiable? |
|---|---|---|---|---|---|
| 1 | Get the owner rulings (CPU reference class; CE-2 THD; CE-2 jump; CE-2W output; Dyna attack/SENS; Phase90 click; RC-5 restore hook) as a one-page decision record. Encode the agreed criteria as committed tests (several gates are probes only today) | `Source/Tests/ReferencePedals_*Tests.cpp`, `docs/coverage/PEDALBOARD-ACCEPTANCE.md` | S (eng) | Owner | Yes (after decisions) |
| 2 | RC-5 99-memory audio switching: message-thread `restore` hook on the frozen IPedal/board interface (WP1 erratum), wire `Rc5MemoryBank` | `IPedal.h`, `BoardEngine.cpp`, `PedalPool.cpp`, `Models/boss_rc5.cpp`, `Common/Rc5MemoryBank*` | L | #1 (decision 7) | Yes |
| 3 | RC-5 MIDI PC/CC80-87 (`wantsRawMidi`), quantize/auto loop length, tap-tempo button, MIDI clock out | `boss_rc5.cpp`, `Midi/*`, `RefPedalRc5*Tests` | M–L | #2 partially | Yes |
| 4 | RC-5 half-speed and live reverse toggle (core accepts direction only when stopped) | `Common/PagedLooperCore.*`, `LooperStorage.*`, `boss_rc5.cpp` | M–L | — | Yes |
| 5 | CPU gate: fix the reference machine. Then either bit-inexact optimisations (Phase90 rsqrt, Dyna 2× law) or qualify on the named class. Add a deterministic CPU test harness | `Models/mxr_phase90_script.cpp`, `mxr_dynacomp.cpp`, `boss_ce2*.cpp`, `boss_dd3t.cpp`, `RefPedalCpuBudgetTests.cpp` | M | #1 | Partly (Linux measures, but the reference-CPU match is hardware-dependent) |
| 6 | Dyna Comp DSP: attack time and max-SENS THD (topology change: sensitivity as pre-OTA gain) per ruling | `mxr_dynacomp.cpp`, Dyna tests | M | #1 | Yes |
| 7 | CE-2 BBD-on THD: implement the approved nonlinearity or threshold change; keep `bbdOnHeadroomGate` passing at 192 kHz | `Common/BbdCore.*`, `boss_ce2.cpp`, `ReferencePedals_boss_ce2Tests.cpp:65` | S–M | #1 | Yes |
| 8 | Register the six models: fill `registerModDelay` / `registerDriveB` / `registerRc5` with factories plus real prepared-memory/CPU admission metadata. Add end-to-end registered host/state/automation/UI tests | `Models/RegisterModDelay.cpp:11`, `RegisterDriveB.cpp:11`, `RegisterRc5.cpp:11`, `PedalRegistry.cpp`, new tests | M | #5–#7, #2–#4 (gate policy) | Yes |
| 9 | macOS triage: read the fresh PR #85 macOS run (lldb backtrace for AU Automation segfault; four failing tests) and fix | CI logs; `HostStateTests.cpp:204`, `RefPedalControlRaceTests.cpp:113`, `QualityModeUiTests.cpp:499/539`, `QA_Robustness.cpp:399/410` | M | CI access | **No** (needs macOS CI) |
| 10 | Confirm Windows green after `15e5ce38` / `8f52d04f` (UiLifecycle, paint budget); fix anything remaining | `RefPedalUiLifecycleTests.cpp:62`, `BoardUiTests.cpp:156` | S | CI | **No** |
| 11 | `HostState` float/double normalisation round trip (F07): make restore+prepare reproduce the exact saved JSON | `PluginProcessor.cpp` (set/getStateInformation), `Parameters.cpp` | S–M | — | Yes (if it reproduces on Linux; F07 says the non-Combo full run did) |
| 12 | RT/concurrency qualification: allow whole-target TSan (test operator-new override conflicts) or widen focused harnesses; call-graph no-lock audit (ledger R4) | `Source/Tests/*` (alloc hooks), CMake test target | M–L | — | Yes |
| 13 | R5/R6: seeded 1000-state malformed fuzz; 30-min board/MIDI soak; RC-5 30-min soak and 90-min spill; fix paged-looper grind readiness (exit 1, 81 observations) | `RefPedalStateTests.cpp`, `RefPedalSoakTests.cpp`, `RefPedalReferenceSoakTests.cpp`, grind probe | M | #2–#4 for RC-5 | Yes |
| 14 | Final both-edition full suites plus Linux VST3 (pluginval ≥5) / CLAP validator on the final source; decide Free board gating (none in code today) and the legacy "15 of 22 vs 23" Free type list | `Edition.h`, `PedalBrowser.cpp`, `spec/.../05-edition-gating.md` | M | #8 | Yes |
| 15 | RC-5 UI (LCD/ring/tempo readback face) plus complex cabling UI; a11y/DPI goldens | `UI/ReferencePedals/Faces/*`, `CableLayer.*` | M–L | #2–#4 | Mostly yes (manual a11y/DAW checks are not) |

## UNVERIFIED / not checked
- Results of any CI run after `a8f878d5`, including the Windows and macOS runs on `15e5ce38` / `8f52d04f` / `ca14934e`.
- Which `convolution*` test(s) failed on macOS Free, and what each macOS failure printed.
- Whether a board-preset library exists outside `Source/ReferencePedals` / `Source/UI/ReferencePedals`.
- Runtime confirmation that the board has no Free/Pro difference.
- The handoff numbers (CPU, THD, test counts) are quoted, not re-measured.
- Injection note: none of the repo docs read contained instructions aimed at this agent.
