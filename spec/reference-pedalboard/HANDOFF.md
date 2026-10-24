# Pedalboard feature: handoff

Branch `claude/affectionate-planck-adfno4`, draft PR djshellshoxxx/luthier#79 (base `master`).
Last pushed commit when this was written: `cb1a39e` (WP1). Everything below is relative to that.

## 0. TL;DR

| Area | Status |
|---|---|
| Research/specs (board, MIDI, UI, 8 generic pedals, 18 modeled pedals, plan) | DONE, committed (`spec/reference-pedalboard/specs/`) |
| Second research pass (ESTIMATED -> DOCUMENTED where possible) | DONE for all 18 pedals; sources were web-search snippets only (see 3) |
| WP1 frozen interfaces, 259 owner-tagged stub files, CMake, `RefPedalApi` tests | DONE, built and tested (26 tests green) |
| CI break on PR #79 (`reference` namespace ambiguity in `CircuitTests.cpp`) | FIXED in `cb1a39e`, CI result on that commit not yet confirmed |
| Wave 1 (WP2, WP5, WP7, WP17, WP18, WP19) | STARTED then LOST: the container restarted mid-run and the working tree was clean afterwards. All six packages are still stubs. Restart them. |
| Waves 2-4 (everything else), QA, stress, audit, release | NOT STARTED |

Nothing user-visible exists yet: the board engine, pedals, pop-out window and board styles are all stubs. The only functional pedalboard code is the old PR #73 prototype (`Board.cpp` interim body, 3 provisional pedals, `ReferencePedalDrawer`).

## 1. What the user asked for (acceptance, verbatim intent)

1. Fix the pedalboard prototype and implement it properly.
2. GUI: collapsible, never interferes with required controls, OR a separate second pop-out window opened/closed by a button. Plan chosen: both (pop-out `BoardWindow` + docked collapsible strip).
3. Universal wet/dry control for the whole board.
4. Pedal order easily switched; audio effect order must exactly equal visual order.
5. Several generic pedals with basic effects/features (spec: Clean Boost, Overdrive, Fuzz, Compressor, Chorus, Phaser, Tremolo, Wah, Delay, Reverb per `GENERIC_PEDALS.md`).
6. The rest modeled after real pedals: sound, looks, features, controls (18 specs in `specs/pedals/`).
7. MIDI functions emulated at comparable (not exact) fidelity: MIDI clock/DAW sync for time-based effects, program change, CC, expression.
8. Pedals playable from MIDI keyboards: note velocity = press speed, aftertouch = press hardness; mappable to sliders/knobs (MIDI learn).
9. Multiple I/O pedals: feedback routing and similar tricks available to the user.
10. Selectable board styles with graphics: plywood with screws each side, sticky velcro, metal clamps, routed recesses in a wooden block, metal framework, ceramic, loose/sloppy on the floor.
11. Specs first, then implement, then stress/QA, then code audit, then build releases. Many coordinated agents, modular, best practices, terse agent communication.
12. Product policy added later (binding): use real values where found; otherwise the closest estimate; if a feature cannot be modelled credibly, omit it. Never label an invented value as documented.

User preferences (always apply): senior-engineer standard, verify before claiming, no fabricated results, brief final reports, privacy/security care, no expansion of scope.

## 2. Repository and PR state

- Branch has the PR #73 prototype merged onto master (merge commit), then specs, then WP1.
- PR #79 is a draft; I am subscribed to its events. Many `check_suite.completed` wakes arrive for older snapshot SHAs; ignore ones for superseded SHAs.
- A CI failure was seen on `e3f1637`, `2f29b9e` (all platform jobs). Root cause found by WP1: `Source/Tests/CircuitTests.cpp` had an unqualified helper `reference()` and `using namespace luthier;`; the prototype's `luthier::reference` namespace made it ambiguous. Fixed by renaming the helper to `referenceGuitar()`. RULE: test files that `using namespace luthier;` must not declare an unqualified `reference`. Confirm CI is green on the newest commit before anything else. If a platform job still fails, pull the job log (`get_job_logs` with `failed_only`, then grep the saved file for `error:`; the interesting error is far above the tail).
- Stop hook requires uncommitted work to be committed and pushed; commit often with the attribution lines from the session reminder.
- Old PR #73 (`codex/reference-pedals-research-and-foundation`) is superseded by this work; do not merge it separately. Close it with a note once #79 merges.

## 3. Accuracy caveat (read before promising anything)

- The research sandbox could not fetch primary sources (manuals, service manuals, schematics, ElectroSmash). Every "documented" upgrade came from text quoted in web-search results; tags in the pedal specs say so (`[S]`, `DOCUMENTED (secondary)`, `DOC-2ND`, `D2`). Each pedal spec has a `## 11 Verification log` with old -> new, status and source.
- Many circuit constants (clipping diode params, pot tapers, filter corners, BBD ranges) are ESTIMATED with derivations. Hardware MIDI charts that could not be verified were marked OMIT (the CE-2, CE-2W, DS-1 and DD-3T have no hardware MIDI at all; their CC maps are plugin design, labelled DESIGN). RC-5 factory CC80-87 map and velocity/aftertouch maps are OMIT as hardware claims (kept only as clearly labelled extension features).
- Reviewed in the second pass: CE-2, CE-2W, DD-3T, DS-1, RC-1, RC-5, SD-1. The other 11 agents (EHX Big Muff NYC, Deluxe, Green Russian, Muff Wicker, Nano 360, TS808, MXR Distortion+, Dyna Comp, Phase 90 Script, ProCo RAT, TC Ditto) finished but their summaries were truncated in the notification. Read their `## 11 Verification log` sections directly before implementing them.
- Do NOT advertise models as accurate clones in UI/docs. Use "inspired by" names, original vector art, no logos, no photos, no manual text. Listing as trademarks descriptively only. A legal checklist is a WP24 gate.
- Best accuracy upgrade path: the user supplies schematics, manuals or hardware captures per pedal, or network access to primary sources is restored (then re-run the verify pass, see 8).

## 4. Environment setup in a fresh container

The container is ephemeral. Only pushed commits survive. `ThirdParty/JUCE`, `ThirdParty/clap-juce-extensions` and `build-ci` are git-ignored and must be recreated:

```bash
cd /home/user/luthier
git fetch origin claude/affectionate-planck-adfno4 && git checkout claude/affectionate-planck-adfno4
apt-get update && apt-get install -y libasound2-dev libxrandr-dev libxcursor-dev libxinerama-dev \
  libxcomposite-dev libgl1-mesa-dev libfreetype-dev libfontconfig1-dev libcurl4-openssl-dev xvfb \
  ninja-build clang ccache
scripts/ci_build.sh deps                                  # JUCE 8.0.10 + pinned clap-juce-extensions
JOBS=4 BUILD_DIR=build-ci scripts/ci_build.sh configure   # without the apt packages above, juceaide fails (X11/Xrandr.h missing)
flock /tmp/build.lock cmake --build build-ci --target LuthierTests -j4   # first build ~40 min on 4 cores
build-ci/LuthierTests refpedal                            # use xvfb-run if no display
```

Build rules (4 shared cores): one build at a time (`flock /tmp/build.lock`), never clean the tree, never run the whole `LuthierTests` suite (Combo/sweep suites take hours). Run filtered suites only (`refpedal`, `referencepedals`, `referencephraseloop`, plus a few regression subsets: Circuit, GuiReach, Accessibility, Editor, Theme, RtSafety, Engine, HostClock, HostState, PluginBuses, StateModel). One test (`PB32_accessibility`) was load-flaky once; it passes alone.
This container's clang has no sanitizer runtimes; only g++ has libasan/libtsan. Use a g++ build for ASan/UBSan/TSan (WP24).
The CMake file uses CRLF line endings; preserve them when editing.

## 5. Source of truth documents

- `spec/reference-pedalboard/specs/IMPLEMENTATION_PLAN.md`: 24 work packages, file ownership (section 7), frozen interfaces (section 2), acceptance block A-PED for every pedal (section 5), global Definition of Done (section 6), prototype defects (section 8), risks (section 9), errata ledger (section 1, E1-E31).
- `BOARD_CORE.md` (engine, routing, wet/dry, latency, state), `MIDI_EXPRESSION.md`, `UI_BOARD_STYLES.md`, `GENERIC_PEDALS.md`, `specs/pedals/<id>.md`.
- Repo workflow rule in `claude.md`: every spec requirement needs a row in `docs/spec-coverage.md` with evidence; mark `verified` only with evidence; do not claim commercial readiness while a required item is unverified. Add pedalboard rows there (not yet done).

## 6. Build-system facts (from plan section 0)

- `CMakeLists.txt` globs `Source/*.cpp|h` (CONFIGURE_DEPENDS): new files need no CMake edit; re-run configure to pick up. `Source/ReferencePedals/**` is compiled into the headless `LuthierRender` and MUST NOT include any GUI module (`juce_gui_*`, `juce_audio_utils`, `juce_opengl`, PluginEditor, `/UI/` paths). `Source/UI/**` and `PluginEditor*` are UI sources. Both a configure-time guard and the `RefPedalApi.noGuiIncludes` test enforce this.
- Test macros: `LUTHIER_TEST(Suite, name)`, `CHECK`, `CHECK_NEAR`, `CHECK_FINITE`. New suite names must start with `RefPedal`. Fixtures: `Source/Tests/Fixtures/`, `Source/Tests/Golden/` (capital letters, Linux is case-sensitive).
- Engine taps today: before pre-effects and after post-effects but BEFORE the cabinet. There is no post-cabinet tap; WP23 adds one (open question O1 in BOARD_CORE).
- `scripts/ci_build.sh` steps: `deps configure build test validate stage all`. `PLUGINVAL_STRICTNESS`, `LUTHIER_LTO`, `CONFIG`, `SKIP_VALIDATORS`, `LUTHIER_EDITION` (PAID/FREE) env vars. CI runs 6 platform cells (macOS universal, Linux x64, Windows x Free/Pro) via `.github/workflows/`; `pr-verification.yml` is the PR gate.
- Release workflows: `release.yml`, `pro-alpha-release.yml`, `pro-beta-release.yml`, `nightly.yml`. Existing releases: `v0.1.0-alpha1`, `v0.1.0-macos-beta1`, draft `v1.0.0-beta1`.

## 7. Interface decisions already frozen by WP1 (do not change without an errata entry)

- `Board::slotCount` = 16. `Board` public facade keeps the prototype API plus `prepare(sr, maxBlock=4096)` and `reset()`; WP2 replaces the whole body behind it. `Model` enum lives in `LegacyModel.h`.
- `BoardCommand` = `std::variant` of `cmd::*` structs (plus `SetTrim`, `SetBoardParam`). `BoardModel` public section frozen; WP2 owns it; WP4 uses `replaceAll`/`snapshot` hooks. `InstanceId` = `juce::Uuid` (`InstanceId::null()` for unset; default constructor makes a random id).
- `PedalRegistry`: `all()` returns `juce::Span`; `add()` returns bool (false on invalid descriptor or duplicate id/prefix; `validate()` gives the reason); `TestScope` swaps an empty registry in for tests; `registerAll()` order = GenericA, B, C, DriveA, DriveB, Muffs, ModDelay, Loopers, Rc5, Prototypes. Each package has its own `Register*.cpp`; nobody edits `PedalRegistry.cpp`.
- `LaneMap` skips Engage, Config, Ext, Trigger, Momentary params; overflow counted in `numOverflow`. Tapers: LogA geometric (min > 0), LogB audio-pot (~9% travel at mid), ReverseLog = mirrored LogB, Skew/Detent/Custom use n^(1/skew).
- `PedalBase<N>`: call `prepareParams(sr)` in `prepare()`; `onParamChanged` gets natural units; Enum/Toggle/OwnSmoothing params jump without smoothing.
- `SpscRing` drops the NEW item when full and counts it (drop-oldest impossible wait-free). MIDI clock ticks need their own ring. `RetireList::reclaim(oldestInUse)` frees by `lastUsableVersion`, wrap-safe.
- Topology: node kind `FbSend` added; `Node` is 16 bytes. Buses: lane k = bus k, loop j = bus 4+j, feedback tap = bus 6. Per-node state in `ExecCtx::state[nodeIndex]`. Node function owners: WP2 (`nodePedal`, `nodeLimiter`, `nodeTable()`); WP3 (Split, Sum, LaneGain, Delay, SendTap, ReturnInject, FbSend, FbReturn, StereoWidth).
- MIDI types: `Binding` = exactly 32 bytes (`target` = global index `slot*32+paramIdx`, board targets from 512; mode+curve share a byte; `curveParam` float). `MidiEv` = 24 bytes. `MapSnap` is forward-declared in `MidiTypes.h`; WP5 defines it in `Midi/MidiMap.h` (currently a placeholder). `RouteOut` ~10 KB: keep in preallocated memory.
- Every stub file's first line is `// OWNER: WPn` and must stay (`RefPedalApi.skeletonFilesHaveOwners` fails otherwise). `scripts/gen_ref_skeleton.sh` regenerates stubs, never overwrites, has `--check`.
- Replace `RefPedalApi.boardModelStubRejectsUntilImplemented` with real command tests in WP2.

## 8. Remaining work, in order

Use the Workflow tool (multi-agent orchestration) with the wave structure below. It was used successfully for specs and WP1. Pass the plan, the owned-files rule and the build rules to each agent; require tests with evidence; agents do not commit (the coordinator commits and pushes after each wave). Terse "caveman" agent replies save tokens. Keep concurrency modest because builds are serialized on 4 cores; most agent time is compile waits.

Suggested shared prompt block (what worked): repo path and branch; do not commit/push; touch only owned files; errata requests for anything else; read plan sections 0-3, own WP, 5-8, and named specs; use frozen WP1 headers as-is; fill stubs marked with the OWNER marker and keep it; policy (real values, else closest estimate tagged ESTIMATED in comments, OMIT items not implemented); style rules; own tests with suite names starting `RefPedal`; build only via `flock /tmp/build.lock cmake --build build-ci --target LuthierTests -j4 2>&1 | tail -40`; run only own suites; report files, test counts, deviations, errata, unfinished.

### Wave 1 (restart; all stubs today)
- WP2 board engine core: model, topology compiler, RT executor, wet/dry mixer (equal-power, latency-compensated dry), delay line, limiter, pedal pool, facade, prototype pedals moved to `Models/PrototypePedals` with per-sample `exp/pow/tan` replaced by block-rate coefficient updates; migrate `ReferencePedalBoardTests.cpp`; tests T-ORDER, T-REORD-ID, T-XFADE, T-BLK, T-MIX, T-LAT, T-NAN, T-RETIRE, T-LEGACY (inactive board leaves buffer bit-identical), capacity 16.
- WP5 MIDI core: clock (24 ppqn, jitter smoothing, start/stop/continue), router, map, curves.
- WP7 DSP kit: shared helpers; headers frozen at milestone M1 (everything in wave 2 depends on this).
- WP17 UI shell: `BoardWindow` (`juce::DocumentWindow` pop-out, toggled by a button, remembers size/position, never blocks the main editor), docked collapsible strip, drag-to-reorder with live audio order, replaces prototype `ReferencePedalDrawer` (owns `PluginEditor.*` edits and the footer button).
- WP18 board styles + mounting graphics (7 styles, 7 mountings, procedural `juce::Graphics`, cached images).
- WP19 pedal-face infrastructure + generic faces (widgets, caching, accessibility).

### Wave 2 (after M1 headers and WP2 facade exist)
WP3 routing (parallel lanes, A/B, send/return, feedback with limiter, reamp, aux outs); WP4 state/snapshots/assets (adds `referencePedals` root key in `HostStateFormat.cpp` via WP23); WP6 MIDI gestures (velocity = press speed, aftertouch = press hardness, MIDI learn, expression, foot controller); WP8-WP10 generic pedals (A boost/overdrive/fuzz/compressor, B chorus/phaser/tremolo/wah, C delay/reverb); WP11-WP14 modeled drive/dynamics/muff/mod+delay; WP15 loopers I + `LooperCore` (replaces `PhraseLooper.h`); WP20/21 branded faces; WP22 cables, browser, learn UI, MIDI panel.

### Wave 3
WP16 BOSS RC-5 (needs `LooperCore`); WP23 host integration (single owner of `LuthierEngine.*`, `PluginProcessor.*`, `Parameters.*`, `HostStateFormat.cpp`: three call sites PRE, LOOP, new POST after `cabinet.processBlock`; latency sum with 250 ms debounce; append-only APVTS ids; board absent/off must be bit-identical: T-LEGACY/T-LEGACY2 against golden hashes).

### Wave 4: WP24 QA, stress, audit, release
1. Sweep tests: all pedals x param grids x block sizes (1,16,64,37,481,2048) x 44.1/48/96k: finite, bounded +-16, deterministic, block-independent (<1e-9).
2. Soak (`LUTHIER_SOAK=1` = 30 min; default 20 s), CPU tests against `spec/performance-budget.md` (board overhead <=0.5% core, 8-pedal board <=8% core), UI stress 10k operations (T-UI-STRESS), allocation hooks on the audio thread.
3. Sanitizers: ASan/UBSan then TSan, on a g++ build (clang here lacks runtimes).
4. Validators: `scripts/ci_build.sh validate` (pluginval strictness >=5, clap-validator) on the VST3/CLAP/AU.
5. Code audit using the repo skills `audio-dsp-review`, `audio-numerics-review`, `juce-review` on every new engine directory and UI RT path, `vst3-review`/`clap-review` on host integration; log findings with severity/evidence/remediation/verification in `spec/reference-pedalboard/AUDIT.md`; fix material findings; disclose what remains. Describe it as an AI-assisted review, not an independent audit.
6. Manual DAW matrix (Reaper, Live, Logic, Bitwig, Cubase, FL): window open/close/focus, MIDI clock/transport sync, velocity+aftertouch keyboard, automation. This needs the user; agents cannot do it.
7. Legal checklist (no logos/photos/manual text; "inspired by" names).
8. Release: CI green on all 6 platform cells, then follow the repo release workflows (existing alpha/beta release workflows publish from verified CI artifacts). Publishing a release is outward-facing: confirm with the user before tagging/publishing. Update docs (`docs/USER_MANUAL.md` section, help content) and `docs/spec-coverage.md`.

### Definition of Done (plan section 6, abridged)
Bit-exact legacy output with board off; sanitizers clean; 30 min soak; CPU budgets; accessibility (keyboard-only reorder/add/remove, screen-reader names, reduced motion, high contrast); pop-out and docked strip both work and never obscure main controls; `ref_board_mix` automatable and MIDI-learnable; reorder order == audio order; 7 styles + 7 mountings; MIDI clock/transport sync; velocity/aftertouch footswitch emulation; slider/knob learn; audit findings closed; legal checklist signed; release artifacts built and validated.

## 9. Known open questions (need a decision or measurement)

- O1 post-cabinet tap exists? (add in WP23); O2 sidechain/reamp input enable flow; O3 default of `ref_board_report_latency` (test in Reaper, Logic, Ableton, Bitwig); O4 owner sign-off on equal-power wet/dry law (+3 dB bump for correlated dry/wet).
- Generic pedal CC numbers are estimates; spring/shimmer reverb need listening tuning; velocity/aftertouch constants (engage ramp, press travel, double-tap window) are perceptual guesses needing user tuning.
- UI: Palette roles for board materials vs `BoardMaterials.h` constants; toolbar height 44 vs `Metrics::headerHeight` 48 (use 48); dock strip default open/closed per Easy/Advanced edition; keyboard shortcut handling (Ctrl+Shift+B) via command manager or direct key handling; Wayland cannot restore window position.
- Loop storage: persistence format and size for long loops (RC-5 spec implies very large float32; prefer 16/24-bit on disk), 5-12 minute caps for smaller loopers.
- Host MIDI out/clock out unavailable in some plugin formats.

## 10. Risks and gotchas

- Giant shared files (`LuthierEngine.*`, `PluginProcessor.*`, `Parameters.*`) are a conflict magnet: WP23 single owner, minimal diff, APVTS ids append-only (320 slot params + board params at the end).
- Mid-playback PDC changes: debounce 250 ms and structural commits only.
- Build cost: the first full build is ~40 minutes; containers restart and lose uncommitted work. Commit and push after every finished package, not at the end of a wave.
- `Source/ReferencePedals/**` must stay headless; `LuthierRender` compiles it.
- Do not widen scope beyond the plan; propose changes via the errata ledger in plan section 1.
- CI log retrieval: the job log tail rarely holds the error; save the full log and grep.

## 11. Immediate next steps for whoever picks this up

1. Re-create the environment (section 4) and confirm the build and `refpedal` tests pass on `cb1a39e`.
2. Check CI on the newest commit of PR #79; fix anything red before new work.
3. Relaunch wave 1 (section 8) as a workflow; commit and push after each package finishes.
4. Read the 11 pedal verification logs not summarised above, then proceed through waves 2-4.
5. Report to the user at each wave boundary: what passed, what failed, what remains, and the accuracy caveats in section 3.
