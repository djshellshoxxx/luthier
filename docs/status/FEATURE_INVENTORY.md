# Luthier feature inventory

Canonical inventory of what the product is specified to do, what is implemented, and what has
been validated. It separates *implemented* (code exists) from *validated* (a test or a validator
run proves it on a named platform).

- Baseline assessed: `ca14934e` (the head of draft PR #85, start of `integration/luthier-completion`).
- Assessed on 2026-11-11 by read-only audits of the tree plus a local Linux x64 build (clang 18,
  JUCE 8.0.10). Rows marked *stale source* quote `docs/spec-coverage.md`, which was last audited
  on 2026-09-23 and is known to under-report (see "Evidence quality" below).
- Status of this session's work on each row is tracked in [PROJECT_STATUS.md](PROJECT_STATUS.md).

## What the product is

Luthier is a physically-modelled guitar **instrument** plugin (digital waveguide strings, body,
pickups, guitar circuit, amp, cabinet, room), not a stand-alone effects processor. It has two
effect subsystems:

1. **Legacy pedal racks**: 23 pedal types in two eight-slot racks, before and after the amp
   (`Source/DSP/Effects/Pedal.h:21`, `Source/UI/PedalRack.h`), shown in the Advanced view.
2. **Reference pedalboard** ("the pedalboard"): a newer, separate board with up to 16 slots,
   undo, snapshots, MIDI mapping and its own window or drawer (`Source/ReferencePedals/**`,
   `Source/UI/ReferencePedals/**`), opened with the header button or Ctrl+Shift+B.

Formats: VST3 and Standalone always; AU on macOS; CLAP when `ThirdParty/clap-juce-extensions`
is present. AAX is not built. Editions: Pro (default build) and Free.

## The "18 to 6" pedal decision

- **18** is the number of named, real-pedal-inspired models in the pedalboard specification
  (`spec/reference-pedalboard/specs/pedals/`, 18 files; 18 model sources in
  `Source/ReferencePedals/Models/`).
- **6** is the catalogue for the first pedalboard release: a script-era phaser, a bucket-brigade
  chorus, a digital delay with short loop, a two-mode chorus/vibrato, a compressor and an
  advanced looper (`docs/handoff/PEDALBOARD-SIX-MODEL-SCENARIO-2026-10-26.md`). The other twelve
  are deferred, with source, specs and tests kept.
- Neither number is a slot limit. The board holds up to 16 pedals
  (`Source/ReferencePedals/BoardTypes.h:22`) and 8 snapshots. No code constant encodes 6 or 18.
- **None of the six is registered**, so none is available to users. The board browser offers
  13 models today: 10 generic effects and 3 prototypes.

## Evidence quality

| Source | Date | Use |
|---|---|---|
| Local build and tests at `ca14934e` | 2026-11-11 | Primary. See PROJECT_STATUS.md for exact commands and counts. |
| GitHub Actions run 41796728348 (PR #85, `ca14934e`) | 2026-11-04 | Primary for Windows and macOS: no local machine exists for them. |
| `docs/handoff/PEDALBOARD-2026-10-28-CONTINUATION.md` | 2026-10-28 | Newest pedalboard handoff. Quoted numbers were not re-measured. |
| `docs/spec-coverage.md` | 2026-09-23 | Stale. 1,989 rows: 673 verified, 403 implemented, 354 partial, 502 pending, 28 blocked, 4 deferred, 25 n/a. Several "pending" rows have tests today (techniques, strum, onboarding, performance budget). |
| `docs/audit/BETA_TEST_REPORT.md` | 2026-09/10 | Beta findings B-01..B-23 with fix status. |

Percentages in this file are ratios of counted things and say what was counted. Where nothing
countable exists the cell says "not measurable".

## A. Instrument (non-pedalboard)

| Feature | Spec | Implemented | Validated (evidence) | Known defects | Remaining | Depends on | Effort (est.) | Owner (this cycle) | Completion (counted) | Acceptance tests |
|---|---|---|---|---|---|---|---|---|---|---|
| String engine and physics | `spec/engine.md`, advanced-ranges, pick-noise, fret-buzz, character-wear | Yes | Linux unit suites; 425 coverage rows: 227 verified, 124 implemented, 39 partial, 31 pending (stale source) | Idle CPU about equal to playing CPU at High quality (B-13); documented modelling approximations in `docs/KNOWN_ISSUES.md` | Suite-wide audio-thread allocation probe on the full processor; idle short-circuit at High quality | none | M | testing/processor-stress | 53 % of rows verified (227/425, stale) | `Engine*`, `Combo*`, `PerfBudget*`, new `ProcessorStress` |
| Instruments, Workshop, illustration | guitar-workshop, part-acoustics, workshop-ui, guitar-illustration | Yes: 25 guitar types (`Source/Model/Guitar/GuitarLibrary.h:24`), part library, family switch | `WorkshopPanel*`, `WorkshopPresets*`, `GuitarIllustration*`, `Combo.everyGuitarTypePlaysEveryPhrase` (24 types x 8 phrases) | Workshop offers only 5 families; per-model selection only in the header | Guitar-type selector inside the Workshop; illustration polish items in `spec/TODO.md` (V, G) | none | S-M | gui/controls-focus-reset | 49 % of rows verified (90/185, stale) | `WorkshopPanel*` plus new type-selector tests |
| Playing techniques | 12 technique specs | Yes | `ScrapeTests`, `SlapTests`, `TapTests`, `MutingTests`, `CascadeTests`, `BendTests`, `StrumGestureTests`, `SlideTechniqueTests`, `BassTechniqueTests` | B-11: some scrape and slap parameters had no visible control at beta time (current status not re-verified) | Re-audit the 256 coverage rows (200 still say pending although tests exist) | none | M (audit only) | unassigned | 9 % of rows verified (23/256, stale and known wrong) | the suites listed |
| Amp, cabinet, impulse responses | `spec/amp-cab-ir.md` (no coverage rows) | Yes: 13 amps, 10 cabinets, synthesised IR library | `Amp.*`, `Cabinet.*`, tiny-block convolution regressions (`Fuzz.convolution*`) | Heap corruption at host block sizes 1-4 was mitigated (`63178fcb`), byte-level root cause unproven; IR swap audible for one block | Root-cause proof under a memory checker; spec row audit | none | M | unassigned | not measurable (no rows) | `Fuzz.convolutionPreparesSafelyForTinyHostBlocks`, `...TinyHostRateAndCapacityBoundaries` |
| Legacy pedal racks (23 types) | `spec/spec.md`, `spec/README.md:102` | Yes | `Effects.*` incl. `Effects.aPedalSwitchedOnThroughItsParametersChangesTheOutput`, `EffectsQaTests` | Owner report "none of the effects work" is fixed at parameter/engine level with a test; no end-to-end UI click test | UI-level proof that a rack pedal switched on in the editor changes the output | none | S | unassigned | 2 of 2 owner reports fixed with engine-level tests | `Effects.*` |
| Rhythm, live, practice, tune builder, modulation | rhythm-engine, live-performance, practice-tools, tune-builder, modulation-matrix | Yes | 301 rows: 159 verified (stale source); `ResetStop*`, `Rhythm*`, `Live*`, `Practice*` | No visible "Reset & Stop" control (function exists, reachable by menu and shortcut only); LIVE pads about 40x10 px | Visible Reset & Stop; LIVE legibility and tooltips | none | S-M | gui/controls-focus-reset | 53 % of rows verified (159/301, stale) | `ResetStop*` plus new UI tests |
| GUI shell, theme, onboarding | gui-integration, ui-wiring, theme, onboarding | Yes: Easy and Advanced modes, workspace tabs, resizable 5:3 editor (940x560 to 2400x1440, default 1200x720) | `Accessibility*`, `EasyLayout*`, `GuiReach*`, `Screenshots`, `ScrollWheel*` | Wheel over a combo box in a scrolling column changes its value; 8 px scrollbars; no collapsible Advanced sections; no visible focus ring; restored size not clamped to the screen; no overlap/clipping test | See GUI rows in PROJECT_STATUS.md | none | M-L | gui/columns-scroll-collapse, gui/sizing-board, gui/controls-focus-reset | 19 % of rows verified (47/254, stale) | new layout-integrity suite; screenshots at 940x564, 1200x720, 1666x1000 |
| Presets, state, undo, file formats | state-model, file-formats, action-and-undo, error-recovery | Yes | `HostState*`, `StateModel*`, `Presets*`, forward-compat tests; clap-validator state tests | Hostile-input findings from the 2026-11-11 review (details delivered to the owner privately); non-atomic saves for some file kinds (ER-24) | Hostile-input hardening; atomic saves; migration fixtures | none | M | security work, parked (private patches) | 11 % of rows verified (14/132, stale) | new hostile-input suites; `HostState*` |
| MIDI in, controllers, MIDI learn | controllers, midi-learn | Yes | `MidiLearn*`, `Controllers*` | A hostile-input finding from the 2026-11-11 review (details private); stuck note when the backend note-off queue is full (fix ported and verified on `agents/luthier/integration/cherry-picks`, not merged) | Hostile-input hardening; merge the stuck-note fix | none | S | integration/cherry-picks (parked) | 52 % of rows verified (15/29, stale) | `MidiLearn*`, `MidiInterpreter*` |
| Routing and I/O | routing-io, input-routing | Yes: multi-out buses, sidechain, MIDI out | `PluginBus*`, `InputRouting*`; pluginval bus tests (older build) | none recorded | none recorded | none | S | unassigned | 39 % of rows verified (21/54, stale) | `PluginBus*`, pluginval |
| Exports and importers | midi-export, notation-export, tab import/export | Yes: MIDI, ASCII tab, Guitar Pro 3-5 and 7/8 readers, MusicXML | `Notation*`, `AsciiDialectCorpus*`, GP fixtures | Hostile-input findings from the 2026-11-11 review (details private) | Hostile-input hardening | none | M | security work, parked (private patches) | 57 % of rows verified (39/69, stale) | `Notation*`, new hostile-input suites |
| Offline renderer (CLI) | `spec/cli-tools.md` | Yes: `luthier-render` (`Tools/RenderCli.cpp`) | `CliConvertTests`, ctest `LuthierRenderCli` | 13 flags parsed but undocumented | Document the flags | none | S | docs/refresh | not measurable | `LuthierRenderCli` smoke test |
| Editions and licensing | editions, licensing | Partly: compile-time Pro/Free split; signed-licence code exists but is not wired in | `Edition*`, `Licence*` unit tests | Licensing rollout is deferred by the owner (recorded 2026-10-06); review findings on the legacy activation path are in the private report | Owner decision on licensing rollout | owner | owner decision | - | not measurable | `Edition*`, `Licence*` |
| Installers and updates | installer, updates-telemetry | Scripts for Linux `.deb`/`.tar.gz`, Windows Inno Setup, macOS pkg | `InstallCycleLinux`, `PackagingPaths` ctest | Review findings on the update path (details private); no signed or notarised installers | Hardening; signing needs certificates | owner (certificates) | M + owner | security work, parked (private patches) | 24 % of rows verified (13/54, stale) | install-cycle tests; manual signing checks |

## B. Reference pedalboard

| Feature | Spec | Implemented | Validated (evidence) | Known defects | Remaining | Depends on | Effort (est.) | Owner (this cycle) | Completion (counted) | Acceptance tests |
|---|---|---|---|---|---|---|---|---|---|---|
| Board core: add, remove, move, duplicate, bypass | `spec/reference-pedalboard/specs/BOARD_CORE.md` | Yes | `RefPedalBoardCore` (59 tests), `RefPedalRouting` (19) on Linux | Windows: `RefPedalUiLifecycle.tenThousandNativeOperationsAndEditorDestruction` fails (a duplicate is rejected) | Root cause and portable fix | none | S-M | platform/windows-ui-lifecycle | Linux: suites green; Windows: 1 failing test | the suites listed |
| Undo/redo, transactions | same | Yes (100 steps, drag coalescing) | `RefPedalBoardCore`, `RefPedalHistoryFailure` (fault-injected allocation) | none open | none | none | - | - | suites green on Linux | same |
| Drag and drop, keyboard editing | `UI_BOARD_STYLES.md` | Yes | `RefPedalUi`, `RefPedalUiFaces` | Complex cabling UI incomplete | Cabling UI; screen-reader manual checks | none | M | unassigned | not measurable | same |
| Dock/pop-out window | `Source/PluginEditor.cpp:1734` | Yes; docks only when the editor is at least 840 px tall | `RefPedalUi.compactDockRetainsRejectionThroughPresentationUpdates` | At the default 1200x720 the board never docks; default drawer leaves about 162 px for a canvas that needs 300 | Usable board at default size and at 1080p | none | M | gui/sizing-board | not measurable | new six-pedal visibility tests; screenshots |
| Snapshots and scenes | `BoardSnapshots.h` | Engine and state only (8 snapshots); a source search found no UI control or MIDI route that calls `ApplySnapshot` or `SetScene` outside tests, so users cannot reach them yet (not verified at runtime) | `RefPedalState` (23), `RefPedalControlRace` | macOS: `saveExtraExceptionReacquiresGuardAndAllowsRetry` failed at an older commit | Snapshot/scene UI and MIDI recall; macOS triage | macOS CI | M | unassigned | engine tests green on Linux; 0 user-facing controls | same |
| Board presets | - | Board state is saved in host state; no separate board-preset library was found | `RefPedalBoardCore.stateSnapshotRoundTrip` | - | Decide whether a board-preset library is in scope | owner | owner decision | - | not measurable | - |
| MIDI map, learn, expression, clock | `MIDI_EXPRESSION.md` | Partly: bindings, learn, footswitch notes, expression, clock in | `RefPedalMidiCore` (34), `RefPedalMidiGesture`, `RefPedalMidiPublication` | Two small fixes exist on an unmerged branch (tap under external clock; clock status retry) | Clock out, jitter handling, scene/bank workflows | none | M-L | integration/cherry-picks (the two fixes) | not measurable | same |
| Generic effects (10) | `spec/reference-pedalboard/specs/` | Yes, registered: boost, overdrive, fuzz, compressor, chorus, tremolo, wah, phaser, delay, reverb | `RefPedalGenericA/B/C` | Original acoustic/alias/CPU grids not complete | Finish the grids | none | M-L | unassigned | 10 of 10 registered; grid completion not counted | `RefPedalGeneric*` |
| Named model: phaser | pedals spec | Prototype, unregistered | `RefPedalPhase90` (13), memory tests | CPU budget 0.4 % not met on the reference machine; 24-bit bypass click criterion unresolved | Owner rulings (CPU reference machine, click criterion); registration | owner | S + owner | - | 0 of 1 registered | same |
| Named model: bucket-brigade chorus | pedals spec section 9 | Prototype, unregistered | `RefPedalCe2` (6), node bypass, memory | `bbdOnOriginalThdGate` fails on every platform: 0.0003-0.0097 % measured, 0.5-3 % required; bypass raw-jump probe; CPU | Physically grounded BBD non-linearity or an owner ruling | owner (if no defensible model) | S-M | dsp/ce2-bbd-thd | 0 of 1 registered | `RefPedalCe2.*` |
| Named model: digital delay | pedals spec | Prototype, unregistered | `RefPedalDd3t` (14), memory, node | CPU budget 0.5 % open; meter state can go subnormal (fix on an unmerged branch) | Port the flush; CPU; registration | owner (CPU reference) | S + owner | integration/cherry-picks | 0 of 1 registered | same |
| Named model: two-mode chorus | pedals spec | Prototype, unregistered | `RefPedalCe2w` (6) | THD criterion ambiguous (which output, which depth); CPU | Owner ruling | owner | owner decision | - | 0 of 1 registered | same |
| Named model: compressor | pedals spec | Prototype, unregistered | `RefPedalDynaComp`, boundaries, node bypass | Max-sensitivity THD, fast attack time and CPU all outside the original gates | DSP work per owner ruling on definitions | owner | M + owner | - | 0 of 1 registered | same |
| Named model: advanced looper | pedals spec | Wrapper v1, unregistered; rhythm engine (57 patterns x 2, 7 kits), paged storage, state codec | `RefPedalBossRc5` (14), memory bank (13), rhythm (11), tap tempo (10), paged looper (13) | 99-memory audio switching, MIDI program/control changes, quantise, half-speed, live reverse and clock out are not done | The largest remaining implementation item; tap-tempo work sits on an unmerged branch | owner (restore hook on a frozen interface) | L | integration/rc5-tap (trial merge) | 0 of 1 registered | same plus 30-minute soak |
| Real-time safety of the board | `spec/engine.md` ground rules | Lock-free publication, retire lists, allocation-free processing (by test) | `RefPedalSoak` (30 s; 30 min with `LUTHIER_SOAK=1`): concurrent edits, zero allocations, zero locks, finite output; strict sanitizer runs on focused suites | Whole-target ThreadSanitizer cannot link (test allocator override) | Whole-processor stress; call-graph audit | none | M | testing/processor-stress; RT audit | not measurable | `RefPedalSoak`, new `ProcessorStress` |

## C. Cross-cutting

| Feature | Implemented | Validated (evidence) | Known defects | Remaining | Owner (this cycle) | Completion (counted) |
|---|---|---|---|---|---|---|
| Linux x64 build and unit tests | Yes | CI run 41796728348: one failing test in each edition; local run: see PROJECT_STATUS.md | the chorus THD gate | get to zero failures | dsp/ce2-bbd-thd | 2,922 of 2,923 tests pass in CI (Pro) |
| Windows x64 | Builds in CI | Same run: two failing tests per edition | chorus gate; UI lifecycle duplicate | fix, then confirm in CI | platform/windows-ui-lifecycle | 0 of 1 platform cells qualified |
| macOS universal | Builds in CI | Same run: Free unit run produced no summary after 189 min; several named failures; AU pluginval segfault in automation at an older commit | see PROJECT_STATUS.md | triage needs macOS CI output | unassigned | 0 of 2 architecture cells qualified |
| Plugin validators | pluginval (VST3, AU), clap-validator (CLAP) wired in `scripts/ci_build.sh validate` | CI: VST3 pluginval success where it ran; CLAP 18 pass, 0 fail, 3 skipped | AU segfault (macOS, older commit) | re-run on final source | unassigned | not measurable |
| Real DAW hosts | - | None. `docs/beta/HOST_TEST_SHEET.md` is blank | - | manual host matrix | owner (manual) | 0 hosts validated |
| 30-second stress test | Board-only soak exists | `RefPedalSoak` | No full-processor stress with timing, click detection or a JSON report | implement and run | testing/processor-stress | board only |
| CI | 12 workflows | - | 263 minutes per PR push; test binary compiled 5 times; CodeQL cancelled by its own timeout in 5 of 7 runs; three third-party actions | restructure | ci/speed | not measurable |
| Security of untrusted input | Hostile-input tests for GP3-5, ASCII tab, tunes, board state, looper | - | 22 findings (2 high, 10 medium, 10 low) in an independent review on 2026-11-11. The report and the unfinished fixes were delivered to the owner privately and are not in this public repository | fix and add tests | parked (private patches) | 0 of 22 fixed |
| Documentation | Extensive | - | KNOWN_ISSUES contradicts the build; changelog stops at 2026-10-16; no pedalboard guide | refresh | docs/refresh | not measurable |
