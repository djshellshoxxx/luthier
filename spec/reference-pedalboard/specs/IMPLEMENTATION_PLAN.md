# IMPLEMENTATION_PLAN: pedalboard feature (work packages, frozen interfaces, defects)

Ver 1.0 | 2026-10-21 | Status PLAN. Audience: parallel implementation agents. Sibling specs: BOARD_CORE.md, GENERIC_PEDALS.md, MIDI_EXPRESSION.md, UI_BOARD_STYLES.md, pedals/*.md, ../ARCHITECTURE.md (compat/legal gates).
Tags: [DOC]=verified in repo at this commit, [EST]=chosen. Rule: where specs conflict, SEC 1 of this file wins; the specs were edited to match (their headers say so).

## 0. Repo facts that shape the plan [DOC]
- `CMakeLists.txt:113` `file(GLOB_RECURSE ... CONFIGURE_DEPENDS Source/*.cpp Source/*.h)` minus `/Tests/`, minus `Source/WIP/`. `LUTHIER_ENGINE_SOURCES` (renderer + tests) = all minus `Source/UI/` (+UiPreferences.cpp) minus `PluginEditor*`; `LUTHIER_UI_SOURCES` (tests + plugin) = `Source/UI/` + `PluginEditor*`. Consequence: **no CMake edit is needed to add files**; `Source/ReferencePedals/**` MUST NOT include `juce_gui_basics` (it is compiled into `LuthierRender`, headless); `Source/UI/ReferencePedals/**` may.
- Tests: `Source/Tests/*.cpp` globbed; `LUTHIER_TEST(Suite, name)`, `CHECK/CHECK_NEAR/CHECK_FINITE`; runner filter = substring on suite/test name (`LuthierTests refpedal`), `--list`, `--skip=suite`. Fixtures via `juce::File(__FILE__).getSiblingFile("Fixtures")`; existing dirs are `Source/Tests/Fixtures/` and `Source/Tests/Golden/` (capital F/G; Linux is case-sensitive). `LUTHIER_ALLOCATION_COUNTER=1` is defined for LuthierTests; heap hook pattern in `Source/Tests/QA_RtSafetyAlloc.cpp`, `CodexQA_RtSafety.cpp`.
- Engine taps today: `LuthierEngine.cpp:3461` (before `preEffects`), `:3529` (after `postEffects`, BEFORE `cabinet.processBlock`). No post-cabinet tap exists. Host state: `Source/HostStateFormat.cpp isKnownRootKey` (add `referencePedals`), `kCurrentStateFormatVersion = 1` (not bumped). Block entry: `PluginProcessor.cpp processSlice` (has `hostClock.read(getPlayHead())` and the slice `midiMessages`). Latency: `updateLatency()` -> `engine.getLatencySamples()`.
- `Source/Support/CommandQueue.h` push() takes a SpinLock (message->audio only). `Source/Support/MidiLearn.*` is the legacy engine-parameter learn. `Source/DSP/Common/Oversampler.h`, `Source/Support/CpuLoadMonitor.h`, `Source/Support/HostClockGuard.h`, `Source/UI/{Theme,PaintCache,AnimationPolicy}.h` exist.
- Prototype never persists its state: no `referencePedals` in PluginProcessor/HostStateFormat; no APVTS params.
- Editor owns `reference::Drawer referencePedalDrawer` + footer `referencePedalsButton` (`PluginEditor.h:214,252`; `PluginEditor.cpp:75-80,600,625`).

## 1. Normative errata (cross-spec reconciliation ledger)
| # | Conflict found | Resolution (binding) | Edited |
|---|---|---|---|
| E1 | Board param ids: `ref_*` (BOARD_CORE) vs `board_*` (MIDI, UI) vs bare shorthand | ALL board-scope ids `ref_board_<name>` (e.g. `ref_board_mix`, `ref_board_enabled`, `ref_board_mix_law`, `ref_board_mode`, `ref_board_tempo_bpm`, `ref_board_tempo_source`, `ref_board_midi_chan`, `ref_board_fb_gain`...); slot-scope `ref_s{nn}_*`. `board_wetdry` == `ref_board_mix`; `board_enable(d)` == `ref_board_enabled`. Prose shorthand in BOARD_CORE (`fb_lp`, `loop_mix`, `ab_select`, `reamp_enable`, `parallel_norm`, `lane{k}_phase`) means `ref_board_<shorthand>` | all specs (perl) |
| E2 | Host lanes `p01..p12` (BOARD_CORE) vs `kMaxParams=16` and pedals with 13-23 params | descriptor capacity `kMaxParams=32`, host lanes `kMaxLaneParams=16`: `ref_s{nn}_p01..p16`; 16*(1+16+2+1)=320 APVTS params; lane assignment by `PedalRegistry::laneMap` skipping Engage/Config/Ext/Trigger/Momentary params; overflow params are state-only but MIDI-learnable | BOARD_CORE 9.1/9.2/11 |
| E3 | Footswitch param spelled `_on`,`_bypass`,`_active`,`_enabled`; polarity "on=0" in Green Russian | `PF_Engage` flag; aliases slot `enabled` (1 = engaged); removed from lane space; polarity normalised | BOARD_CORE 11; every pedal spec banner |
| E4 | Per-pedal `*_input_trim_db/_output_trim_db/_in_trim` duplicate slot trims | removed; use `ref_s{nn}_in_db/out_db` (BOARD_CORE 5.4) | pedal banners |
| E5 | Param prefixes inconsistent (`ce2_`, `mxrdp_`, `phase90_`, `ehx360_`, unprefixed Green Russian, `ref_vpk`) vs rule "prefix = model id with - -> _" | `PedalDesc.paramPrefix` is mandatory & registry-validated. Renamed: `boss_ce2_`, `boss_ce2w_`, `mxr_distortion_plus_`, `mxr_phase90_script_`, `ehx_nano_360_`; Green Russian rows prefixed `ehx_green_russian_`; `ehx_big_muff_nyc_ref_vpk`. Generic keeps `gen_<short>_` | pedal specs |
| E6 | Base API: `ReferencePedalProcessor{processStereo,setControlSnapshot}` (ARCHITECTURE) vs `IPedal{process,setParam}` (BOARD_CORE) vs `processStereo`/`ControlSnapshot` (MIDI) | `IPedal` (BOARD_CORE 11, frozen I-1) is the only pedal API. `setControlSnapshot` removed. Extra hooks added: `ParamKind::Trigger`, `onTransport`, `drainMidiOut`, `PedalServices`, `wantsRawMidi`. `onMidiClockTick` removed (use `TimeInfo.ppq`) | BOARD_CORE 9.5/11; ARCHITECTURE |
| E7 | Event types: `MidiEvent` vs `MidiEv` vs `FootEvent` vs `FsPress`/`FootswitchEvent` | `MidiEv` (router-internal, 24 B), `ParamWrite` (router->pedal `setParam`), `MidiEvent` (raw, only `wantsRawMidi` pedals), `FootEvent{press,speed,force,offset}` (pedal-facing). `FsPress`/`FootswitchEvent` are router-internal names for the same data | MIDI 1, BOARD_CORE 9.5 |
| E8 | Clock: `BoardClock`+`TimeInfo` vs `ClockSync::snapshot()`; tempo priority differs | one implementation `Midi/ClockSync` (reuses `HostClockGuard`), pedals only see `TimeInfo{bpm,ppq,playing,clockLocked,valid,sample}`; priority = MIDI_EXPRESSION 3.1 (AUTO: HOST playing > MIDI_CLOCK > INTERNAL, hysteresis 2 beats / 1 s) | BOARD_CORE 9.5 |
| E9 | MIDI CC maps: ~28 pedal specs each claim CC14-19/80-87/102-119 etc., generic "CC80+k", MIDI doc "direct map" 9.2 ranges, Focus bank CC70-77 | no global map. Pedal-spec CC tables = pedal-LOCAL profiles (active only for a slot with explicit `midi_chan` or MIDI focus + opt-in toggle). Default routing = Focus mode (CC70-77 bank, CC11, CC64). `ref_board_direct_map_enable` retired. RC-5 CC80-87 (spec wins), DD-3T/Ditto/Nano per their specs. Reserved CCs 0,32,120-127 never in profiles | MIDI 9, GENERIC 0.3 |
| E10 | Footswitch mode params (`*_stomp_mode`,`*_footsw_mode`,`*_fs_mode`,`*_momentary`) vs FootController mode; `*_midi_chan`,`*_soft_press`,`*_midi_enable` | owned by slot FootController / `midi.slotChan[]` (state), removed from ParamDesc | MIDI 9.2; pedal banners |
| E11 | Looper default note 60 vs `ref_board_kbd_fs_base_note` 36+i | kbd scheme (36+chain position) is the default; 60 is the LOCAL-profile note | MIDI 9.1 |
| E12 | `Placement` enum with redundant `preAmpInput(3)`; names PRE/LOOP/POST in UI | `{beforeExistingPre=0 (PRE), afterExistingPost=1 (LOOP), postCab=2 (POST)}`; value 3 dropped; postCab tap site verified feasible (between `cabinet.processBlock` and `room.processBlock`, float->double scratch) | BOARD_CORE 6.5 |
| E13 | State schema: BOARD_CORE schema 1; MIDI "bump to 2" | nothing shipped => schema 1 includes `midi`; no migration yet; fixtures `Source/Tests/Fixtures/ReferencePedals/ref_board_v1.json`; add `referencePedals` to `isKnownRootKey`; `kCurrentStateFormatVersion` unchanged | BOARD_CORE 10.2; MIDI 11 |
| E14 | `Registry` w/ `REGISTER_PEDAL` static init | `PedalRegistry::registerAll()` explicit list (linker-safe, 3 targets); per-package `register*()` functions in package-owned files | BOARD_CORE 11 |
| E15 | "SPSC CommandQueue" for audio->UI | new `Rt/SpscRing.h` (wait-free); `CommandQueue.h` not used | BOARD_CORE 12 |
| E16 | Style lists differ (BOARD_CORE 14 vs UI 5) | styles `plywood|velcro|clamps|routed|rail|ceramic|loose`; mounts `screws|velcro|clamps|routed|rail|ceramic|none` | BOARD_CORE 14 |
| E17 | Pedal sizes: GENERIC "66x112 units", UI "120x200 px", toolbar 44 vs 48 | units are mm; `px = mm*1.6` @zoom 1 (cellH 240 fits 150 mm wah); toolbar = `Metrics::headerHeight` 48 | GENERIC 0.4; UI 4, 4.1 |
| E18 | Face assets: `Faces/sd1_face.svg` vs "no bitmaps/assets" | procedural `Faces/<snake_id>_face.cpp`; golden PNGs are test-only | SD-1 spec; pedal banners |
| E19 | Paths: `Tests/golden`, `Tests/fixtures`, `Models/` namespace `ReferencePedals::Models`, `BoardClock.*`, `Registry.cpp`, new `MidiLearn.*` clashing with `Support/MidiLearn.*` | `Tests/Golden/ReferencePedals/`, `Tests/Fixtures/ReferencePedals/`; namespace `luthier::reference::models`; `ClockSync`; `PedalRegistry`; board learn = `BoardMidiLearn.*` | all |
| E20 | RC-5 sync ids `boss_rc5_sync_mode`/`loop_sync` not in RC-5 spec | `boss_rc5_sync_clock` {INTERNAL,AUTO} + EXT `boss_rc5_clock_out`, `boss_rc5_rx_map`, `boss_rc5_quantize` | MIDI 9.3; RC-5 spec |
| E21 | `Mono` pedal channel rule: BOARD_CORE sums L+R, muff specs default dual_mono | Config `stereo_mode` {dual_mono default, mono_sum}; dual-mono input (L==R) processed once and copied (bit-exact) | BOARD_CORE 6.8 |
| E22 | `boss_dd3t_tempo_bpm` local tempo | read-only mirror of last tap tempo (`saveExtra`), authoritative tempo = `ref_board_tempo_bpm`/`TimeInfo` | this file |
| E23 | `BoardCommand` list incomplete (UI needs SetStyle etc.) | list extended (BOARD_CORE 10.6) | BOARD_CORE |
| E24 | Suite naming for filtered runs | suites start with `RefPedal` (`LuthierTests refpedal`); prototype suites `ReferencePedals`, `ReferencePhraseLoop` kept until migrated | this file |

## 2. Interfaces to freeze FIRST (WP1 output; changes only via errata PR to WP1 owner)
All headers below are created by WP1 as compilable, documented, test-instrumented skeletons.
- **I-1 `IPedal`, `PedalDesc`, `ParamDesc`, `ParamFlag`, `TimeInfo`, `MidiEvent`, `FootEvent`, `TransportEvent`, `PedalServices`, `PedalBase<N>`** - verbatim BOARD_CORE sec 11.
- **I-2 `PedalRegistry`** - BOARD_CORE sec 11 (`registerAll/add/make/find/all/laneMap`). `Registration.h` declares one function per package: `registerGenericA/B/C()`, `registerDriveA()`, `registerDriveB()`, `registerMuffs()`, `registerModDelay()`, `registerLoopers()`, `registerRc5()`, `registerPrototypes()`. `PedalRegistry.cpp` (WP1) calls all; each package owns its `Register*.cpp` (WP1 ships empty bodies).
- **I-3 `Board` facade** (`Source/ReferencePedals/Board.h`, replaces prototype class, same name/namespace/member `LuthierEngine::referencePedals`):
```cpp
namespace luthier::reference {
constexpr int kMaxSlots = 16, kMaxBranches = 4, kMaxLoops = 2;
enum class Placement : int { beforeExistingPre = 0, afterExistingPost = 1, postCab = 2 };
struct HostTransport { bool valid=false, playing=false; double bpm=120, ppq=0; int tsNum=4, tsDen=4; };
struct BoardMeters { float inPeakDb, outPeakDb, cpuPct; int latencySamples; bool fbTripped, fbFault, clockLocked, clockLost; uint32_t midiDropped; };
class Board final {
public:
  static constexpr int slotCount = kMaxSlots;           // legacy name (prototype 8)
  Board(); ~Board(); Board(const Board&)=delete; Board& operator=(const Board&)=delete;
  // ---- message thread
  void prepare (double sampleRate, int maxBlock);        // allocs pool, delay lines, scratch; idempotent
  void releaseResources();
  BoardModel& model() noexcept;                          // single truth; edits via model().apply(BoardCommand)
  midi::MidiRouter& midi() noexcept;
  int reclaim() noexcept;                                // called by 30 Hz UI timer / after prepare: frees retired topologies/DSP
  BoardMeters readMeters() const noexcept;               // atomics
  // ---- audio thread, per processSlice, in this order
  void beginBlock (const juce::MidiBuffer& midi, const HostTransport& host, int numSamples) noexcept;
  void processStereo (double* L, double* R, int n, Placement p) noexcept;   // up to 3x per block; early-out BEFORE touching buffers when !isActive()
  void endBlock (juce::MidiBuffer& midiOut) noexcept;    // meters, SPSC drain, RC-5 clock out
  int  getLatencySamples() const noexcept;               // board latency L_board (0 when inactive)
  bool isActive() const noexcept;                        // enabled && >=1 non-empty pedal
  // ---- legacy shims (prototype API, forward to model commands; deleted after test migration)
  void setBoardEnabled(bool); bool isBoardEnabled() const; void setModel(int slot, Model); Model getModel(int) const;
  void setSlotEnabled(int,bool); bool isSlotEnabled(int) const; void setPlacement(int,Placement); Placement getPlacement(int) const;
  void setKnob(int slot,int knob,double); double getKnob(int,int) const; void move(int from,int to); int getIdAtPosition(int) const;
  static const char* modelName(Model);
};}
```
  Contract: `Board.h` includes only light headers (forward-declares `BoardModel`, `MidiRouter`) so `LuthierEngine.h` stays cheap; all heap state lives behind a pimpl.
- **I-4 `BoardModel` + `BoardCommand` + `BoardModel::Listener`** (UI/state contract): `bool apply(const BoardCommand&)` (message thread; compiles+publishes topology; returns false with reason on reject), `getOrder()` (instanceIds in audio order), `getInstance(id)`, `getTopologyInfo()`, `getBoardSettings()`, `undo()/redo()/beginTransaction()`, `addListener(Listener*)`; `Listener{ orderChanged(); instanceChanged(id); settingsChanged(); styleChanged(); rejected(reason); }`. Commands: Add, Remove, Duplicate, Move, SetModel, SetParam, SetEnabled, SetPlacement, SetMode, SetLane, SetLoop, SetFeedback, SetStyle, SetMounting, SetMix, SetMixLaw, SetBoardEnabled, SetMidiBinding, SetSlotChannel, SetScene, ApplySnapshot (BOARD_CORE 10.6). `Topology`, node kinds and `ExecCtx` in `Topology.h`.
- **I-5 `MidiRouter`** (`Midi/MidiRouter.h`): 
```cpp
namespace luthier::reference::midi {
struct ParamWrite { uint8_t slot; uint16_t paramIdx; float norm; uint16_t smoothMs; int offset; };
struct RouteOut  { ParamWrite writes[512]; int nWrites; FootEvent foot[64]; uint8_t footSlot[64]; int nFoot; TransportEvent tr[8]; int trOff[8]; int nTr; TimeInfo time; };
class MidiRouter { public:
  void prepare (double sr, int maxBlock);
  void publishMap (std::unique_ptr<MapSnap>) ;                          // message thread; RCU publish (BOARD_CORE 4.1)
  void pushUiEvent (const MidiEv&) noexcept;                            // device/UI thread -> uiInRing
  void process (const juce::MidiBuffer&, const HostTransport&, int64_t blockStart, int n, RouteOut& out) noexcept; // audio thread, no alloc
  void armLearn (const LearnTarget&); bool pollLearn (LearnCapture&);   // message thread
  int  drainUi (UiEv* out, int cap) noexcept;                           // message thread
};}
```
  `BoardEngine` consumes `RouteOut`, splits sub-blocks at event offsets (min 16 samples), calls `setParam/onFoot/onTransport` in BOARD_CORE 11 dispatch order.
- **I-6 `BoardWindow` + `PedalFace`** (`Source/UI/ReferencePedals/BoardWindow.h`, `PedalFace.h`): `BoardWindow(BoardModel&, LuthierAudioProcessor&, juce::Component* anchor)`; `void showWindow(); void hideWindow(); bool isShowing(); void forceHeadlessContent(); std::function<void(bool)> onVisibilityChanged;`. `class PedalFace { virtual juce::Point<float> designSizeMm() const; virtual void paintFace(juce::Graphics&, const FaceState&) ; virtual void layout(FaceLayout&) ; }` (`FaceState`: params norm[], enabled, tailActive, theme flags; `FaceLayout`: control rects, jack positions, footswitch rect, LED rect). `PedalFaceFactory::make(modelId)`.
- **I-7 `Topology.h`** node kinds `Pedal, Split, Sum, LaneGain, Delay, SendTap, ReturnInject, FbReturn, Limiter, StereoWidth`, `ExecCtx{double* scratch[..]; int n; ...}`, `NodeFn` signature and the routing function table `Routing/RoutingNodes.h` (impl WP3, table `NodeTable.cpp` WP2).
- **I-8 `Rt/SpscRing.h`, `Rt/RetireList.h`, `IAssetStore`** (`Services/IAssetStore.h`: `putBlob(hash,span)`, `getBlobPath(hash)`, async API; impl WP4).
- **I-9 DSP kit headers (`Common/*.h`) are frozen by WP7 at its milestone M1** (API + doc + stub bodies pushed within the first day; pedal packages start after M1).

Skeleton rule S-1: WP1 creates every file listed in sec 4 as a compilable stub (header guard + empty namespace / trivial body) with an `// OWNER: WPn` first line, so globbed CMake, includes and the registry compile from day 1 and no two agents ever create the same path. Ownership then transfers; no one edits another package's file (send an errata request instead).

## 3. Dependency graph / waves
```
Wave0: WP1
Wave1 (parallel): WP2  WP5  WP7  WP17  WP18  WP19(infra part)
Wave2 (parallel): WP3  WP4  WP6  WP8 WP9 WP10 WP11 WP12 WP13 WP14 WP15  WP20 WP21 WP22
Wave3: WP16 (needs WP15 LooperCore)   WP23 (needs WP2 WP4 WP5 WP6 + registry)
Wave4: WP24 (QA/stress/audit/release; needs all)
```
Merge order inside a wave is free (disjoint files). Every WP leaves `scripts/ci_build.sh configure build test` green on its own branch.

## 4. Work packages
Notation: **Own** = exclusive files (create/edit). **Test** = test files owned. **Acc** = acceptance. All pedal DSP packages share the common acceptance block A-PED (sec 5).

### WP1 Foundation, frozen interfaces, skeleton, CMake
- Depends: none.
- Own: `Source/ReferencePedals/{IPedal.h,PedalDesc.h,PedalBase.h,PedalRegistry.h,PedalRegistry.cpp,Registration.h,Topology.h,BoardCommand.h,BoardTypes.h,Board.h,LegacyModel.h}`, `Rt/{SpscRing.h,RetireList.h}`, `Services/IAssetStore.h`, `Routing/RoutingNodes.h`, `Midi/{MidiTypes.h,MidiRouter.h}`, `Source/UI/ReferencePedals/{BoardWindow.h,PedalFace.h,PedalFaceFactory.h}`, ALL stub files of sec 4, `CMakeLists.txt` (only package that edits it), `scripts/gen_ref_skeleton.sh`.
- CMake: (1) none for sources (glob). (2) add `option(LUTHIER_SANITIZE "address|undefined|thread" "")` -> `-fsanitize=` on `LuthierTests` (non-MSVC), plus `LUTHIER_SOAK` runtime env documented; (3) `add_test(NAME RefPedalSuite COMMAND LuthierTests refpedal)` and `add_test(NAME RefPedalLegacy COMMAND LuthierTests referencepedals referencephraseloop)`; (4) assert at configure that no file under `Source/ReferencePedals` includes `juce_gui` (CMake `file(STRINGS)` check or a unit test `RefPedalApi.noGuiIncludes`).
- Test: `Source/Tests/RefPedalApiTests.cpp` (T-REG scaffolding: registry validation rules, laneMap rules, static_asserts on POD sizes `MidiEv==24`, `sizeof(Binding)==32`, `kMaxLaneParams`, header self-containment, no-GUI-include scan, SpscRing wait-free stress).
- Acc: all stubs compile in LuthierTests, LuthierRender, plugin targets; `registerAll()` idempotent; registry rejects duplicate id / bad prefix; SpscRing 10^8 ops TSan clean; interfaces reviewed by all wave-1 owners.
- Risks: header churn after freeze -> errata PR only; skeleton file count (~220) -> generated by script, reviewed once.

### WP2 Board engine core (model, compiler, RT executor, mix, latency, limiter, facade)
- Depends: WP1.
- Own: `BoardModel.{h,cpp}`, `TopologyCompiler.{h,cpp}`, `BoardEngine.{h,cpp}`, `NodeTable.cpp`, `Board.cpp`, `WetDryMixer.h`, `DelayLine.h`, `BoardLimiter.h`, `PedalPool.{h,cpp}`, `Models/PrototypePedals.{h,cpp}` + `Models/RegisterPrototypes.cpp` (ts808/ds1/phase90 prototype math moved here as `prototype.ts808|ds1|phase90`, family "Prototype", hidden from browser once real models exist, per-sample `exp/pow/tan` replaced by block-rate coefficient updates), `Source/Tests/ReferencePedalBoardTests.cpp` (migrated to facade shims, old assertions kept).
- Test: `Source/Tests/RefPedalBoardCoreTests.cpp` - T-ORDER, T-REORD-ID, T-XFADE, T-BLK, T-MIX, T-LAT, T-LAT2, T-NAN, T-RETIRE, T-LEGACY(unit part: inactive board leaves buffer bit-identical), series transition (dip-through-dry) at block-split points, undo/redo coalescing, capacity 16, kMaxLatencySamp reject, CPU cap reject.
- Acc: BOARD_CORE sec 15 rows above pass; `processStereo` allocation count 0 over 10 min random edits at 100 Hz (allocator hook); TSan clean; wet/dry equal-power law exact (m=0 -> dryD, m=1 -> wet bit-exact, m=0.5 correlated +3.01 dB); dry path latency matched.
- Risks: reorder click (mitigation 6 ms dip, tested); retired-topology UAF (epoch rule + ASan T-RETIRE); shim drift (shims are thin forwards).

### WP3 Advanced routing: parallel lanes, A/B, send/return loops, feedback, reamp, aux outputs
- Depends: WP1, WP2 (for integration tests; node functions unit-testable standalone).
- Own: `Routing/{Lanes,AbSwitch,SendReturn,Feedback,Reamp,StereoWidth}.cpp` (+ private `.h`), `Routing/LatencyEqualiser.{h,cpp}`.
- Test: `Source/Tests/RefPedalRoutingTests.cpp` - T-PAR, T-AB, T-LOOP, T-FB (g=0.98, runaway 1.05 w/ and w/o `ref_board_fb_runaway_ok`, NaN injection, watchdog trip, kill), compiler rejects s<r feedback, lane latency equalisation, polarity invert.
- Acc: BOARD_CORE 6.2-6.6, 6.9 behaviours; feedback bounded `|y|<=ceil*1.01`; trips within 200 ms; unit-delay >= 64 samples documented; ping/AB equal-power.
- Risks: feedback instability -> 7 safety stages mandatory; lane latency mismatch -> cross-correlation test.

### WP4 State, snapshots, assets
- Depends: WP1, WP2 (model types).
- Own: `BoardState.{h,cpp}` (JSON <-> BoardModel, `migrate_N_to_N+1`, unknown-field preservation, MissingPedal, size cap 256 KB, partial-failure keep-previous), `BoardSnapshots.{h,cpp}` (8 snapshots/scenes), `Services/AssetStore.{h,cpp}` (content-addressed, temp+atomic rename, path redaction, missing-asset report), `Source/Tests/Fixtures/ReferencePedals/*.json`.
- Test: `Source/Tests/RefPedalStateTests.cpp` - T-STATE (1000-seed fuzz), T-MIGR (v1, schema+1, unknown keys byte-wise, missing model), asset atomic-write failure leaves old file, concurrent save during edit, 256 KB cap.
- Acc: round-trip JSON equal; audio equal after restore (bit-exact for deterministic models); unknown root keys and nested unknown fields preserved; newer schema => board disabled + block preserved verbatim.
- Risks: host blob growth -> loop audio never inline (asset refs only).

### WP5 MIDI core: clock, router, map, curves
- Depends: WP1.
- Own: `Midi/{Curves.h,MidiMap.{h,cpp},ClockSync.{h,cpp},MidiRouter.cpp}`.
- Test: `Source/Tests/RefPedalMidiCoreTests.cpp` - MIDI_EXPRESSION T-01..T-08 (clock 120 BPM +-0.1, jitter, loss, FSM table, host transport, divisions, LFO phase lock, tap), T-12 (offset 100 ramp start; split vs unsplit <=1 LSB), T-13, T-14, T-16 (priority/hold), T-20..T-22, S-01 flood (100k ev/s, <3% core), S-02 alloc, S-04 block sizes, S-05 sample rates.
- Acc: lock <= 1 beat; clockLost within 500 ms; router never allocates; drop counters exposed; soft-takeover hooks; map RCU publish TSan clean.
- Risks: JUCE `MidiBuffer` iteration on RT (read-only OK); event flood (ring overflow policy tested).

### WP6 MIDI gestures, learn, expression, foot controller
- Depends: WP1, WP5 (types), WP2 (for slot targets, mock allowed).
- Own: `Midi/{GestureEngine,BoardMidiLearn,ExprEngine,MidiState}.{h,cpp}`, `FootController.h`, `Common/GestureDecoder.{h,cpp}` (shared looper tap/double/hold decoder, per-pedal configurable thresholds).
- Test: `Source/Tests/RefPedalMidiGestureTests.cpp` - T-09, T-10, T-11, T-15, T-17, T-18, T-19, S-06 (30 min fuzz, 20 s default, `LUTHIER_SOAK=1` full), S-07; velocity->xfade lerp(12,2), aftertouch modes, keyboard base-note scheme, macro fan-out.
- Acc: velocity/aftertouch emulate stomp speed/force as MIDI_EXPRESSION 5; tap accuracy +-1.5 BPM @ +-5 ms jitter; learn FSM legal transitions; bindings keyed by instanceId survive reorder; reserved-CC guard.
- Risks: gesture latency (fast-tap optimisation tested).

### WP7 DSP kit (shared helpers) - milestone M1 = headers frozen
- Depends: WP1.
- Own: `Common/{Biquad,OnePole,DcBlock,TptSvf,Lfo,TableSin,EnvelopeFollower,SoftClip,FractionalDelay,TempoSync,Smoothers,NewtonClipper,OversamplerAdapter,NoiseGen}.h` (+ `.cpp` where needed). `OversamplerAdapter` wraps `Source/DSP/Common/Oversampler.h` (counts latency, policy Eco/Normal/High), `NoiseGen` deterministic seeded.
- Test: `Source/Tests/RefPedalDspKitTests.cpp` (coefficient accuracy vs analytic, block independence, denormal flush, Newton solver convergence <=8 iterations to 1e-9, alias floor, LFO phase continuity).
- Acc: every helper header-only unit-tested; no alloc after prepare; double precision; `-ffast-math`-free determinism.
- Risks: API churn blocks 9 packages -> M1 gate.

### WP8 Generic pedals A: boost, overdrive, fuzz, compressor
- Depends: WP1, WP7. Own: `Generic/{Boost,Overdrive,Fuzz,Compressor}.{h,cpp}`, `Generic/RegisterGenericA.cpp`. Test: `RefPedalGenericATests.cpp`. Acc: GENERIC_PEDALS sec 1-4 tests + A-PED. Risks: oversampling latency vs bypass match (PDC test), fuzz gate chatter.

### WP9 Generic pedals B: chorus, phaser, tremolo, wah
- Depends: WP1, WP7. Own: `Generic/{Chorus,Phaser,Tremolo,Wah}.{h,cpp}`, `Generic/RegisterGenericB.cpp`. Test: `RefPedalGenericBTests.cpp`. Acc: GENERIC sec 5,8,9,10; tempo sync via `TimeInfo` (lock <= 2 beats); wah treadle via expression/`setExpression` + toe switch hysteresis. Risks: LFO phase glitch on sync toggle.

### WP10 Generic pedals C: delay, reverb
- Depends: WP1, WP7. Own: `Generic/{Delay,Reverb,ReverbFdn,ReverbPlate,ReverbSpring}.{h,cpp}`, `Generic/RegisterGenericC.cpp`. Test: `RefPedalGenericCTests.cpp`. Acc: GENERIC sec 6,7 (RT60 within 10%, tail <-90 dBFS, freeze +-1 dB 10 s, tap/sync accuracy, trails, `gen_delay_tap` Trigger). Risks: CPU (reverb budget 3%), denormals in FDN (flush + dither).

### WP11 Reference models: drive I (BOSS DS-1, Ibanez TS808, BOSS SD-1)
- Depends: WP1, WP7. Own: `Models/{boss_ds1,ibanez_ts808,boss_sd1,boss_sd1_params}.{h,cpp}`, `Models/RegisterDriveA.cpp`. Test: `ReferencePedals_boss_ds1Tests.cpp`, `ReferencePedals_ibanez_ts808Tests.cpp`, `ReferencePedals_boss_sd1Tests.cpp`. Acc: per-spec test lists + A-PED; values tagged MEASURED/DOCUMENTED/ESTIMATED kept in code comments. Risks: all circuit values ESTIMATED until bench capture - UI shows evidence grade; no "accurate" claim.

### WP12 Reference models: drive II + dynamics (MXR Distortion+, Pro Co RAT, MXR Dyna Comp)
- Depends: WP1, WP7. Own: `Models/{mxr_distortion_plus,proco_rat,mxr_dynacomp}.{h,cpp}`, `Models/RegisterDriveB.cpp`. Test: `ReferencePedals_mxr_distortion_plusTests.cpp`, `..._proco_ratTests.cpp`, `..._mxr_dynacompTests.cpp`. Acc: per-spec + A-PED. Risks: LM308 slew/GBW model cost; OTA compressor attack/release calibrations ESTIMATED.

### WP13 Reference models: Big Muff family (NYC, Tone Wicker, Green Russian, Deluxe)
- Depends: WP1, WP7. Own: `Models/{ehx_muff_core.h,ehx_big_muff_nyc,ehx_muff_wicker,ehx_green_russian,ehx_deluxe_muff}.{h,cpp}`, `Models/RegisterMuffs.cpp`. Test: four `ReferencePedals_<id>Tests.cpp`. Acc: shared core tests (Newton clip, tone stack), per-variant deltas, Deluxe gate/mids/EXP jack (expression via `setExpression`). Risks: 4 clipping stages x 4-8x oversampling CPU -> Eco fallback; Wicker circuit unpublished (ESTIMATED).

### WP14 Reference models: modulation + delay (BOSS CE-2, CE-2W, MXR Phase 90 Script, BOSS DD-3T)
- Depends: WP1, WP7. Own: `Common/BbdCore.{h,cpp}`, `Models/{boss_ce2,boss_ce2w,mxr_phase90_script,boss_dd3t}.{h,cpp}`, `Models/RegisterModDelay.cpp`. Test: four `ReferencePedals_<id>Tests.cpp` + `RefPedalBbdCoreTests.cpp`. Acc: per-spec + A-PED + MIDI-clock sync (LFO phase lock, DD-3T tap subdivisions Q/D8/QT, 12.5-800 ms halving rule, hold short loop). Risks: BBD clock-aliasing noise; CE-2W three-mode switching clicks.

### WP15 Reference models: loopers I (TC Ditto, EHX Nano Looper 360, BOSS RC-1) + LooperCore
- Depends: WP1, WP6 (GestureDecoder), WP4 (AssetStore). Own: `Common/LooperCore.{h,cpp}` (states Empty/Recording/Playing/Overdubbing/Stopped/Undo/Redo, layered undo, seam crossfade, timestamped command FIFO, bounded preallocated RAM, worker spill via IAssetStore), `Models/{tc_ditto,ehx_nano_360,boss_rc1}.{h,cpp}`, `Models/RegisterLoopers.cpp`, `Source/Tests/ReferencePhraseLooperTests.cpp` (migrated), removal of `ReferencePedals/PhraseLooper.h` at WP end. Test: `ReferencePedals_tc_dittoTests.cpp`, `..._ehx_nano_360Tests.cpp`, `..._boss_rc1Tests.cpp`, `RefPedalLooperCoreTests.cpp`. Acc: per-spec; dry-through never muted; undo/redo bit-exact null < -120 dB; no seam click; clear O(1); memory cap respected. Risks: destructive overdub prototype behaviour must not leak; persistence of audio only via assets.

### WP16 Reference model: BOSS RC-5 (looper + rhythm + storage + MIDI clock master)
- Depends: WP15, WP5/WP6, WP4. Own: `Models/{boss_rc5,rc5_rhythm,rc5_storage,rc5_patterns.inc}.{h,cpp}`, `Models/RegisterRc5.cpp`. Test: `ReferencePedals_boss_rc5Tests.cpp`. Acc: RC-5 spec s9 tests; 99 memories; PC 0..98; CC80-87 RX map; clock slave/master via `drainMidiOut`; original (non-Roland) synthesized kits and 57x2 original patterns; storage spill never blocks audio (UNDERRUN flag). Risks: patterns authoring volume, RAM cap, disk worker failure (auto-PLAY, error LED).

### WP17 UI shell: pop-out window, dock strip, layout, drag-reorder
- Depends: WP1 (I-6), WP2 (BoardModel), WP5 (MIDI panel hook optional).
- Own: `Source/UI/ReferencePedals/{BoardWindow,BoardRoot,Toolbar,BoardViewport,BoardCanvas,PedalView,DragController,StatusBar,BoardUiState}.{h,cpp}`, `Source/UI/ReferencePedalDrawer.{h,cpp}` (rewritten: docked shell hosting the single `BoardView`, replaces overlay behaviour), `Source/PluginEditor.{h,cpp}` (ONLY: own `std::unique_ptr<BoardWindow>`, PEDALS toggle in header/footer + Ctrl+Shift+B, dock reserved height in layout instead of overlay, editor dtor closes window), `Source/UI/AnimationPolicy.cpp` (allow-list entry `BoardStatusPoll`).
- Test: `Source/Tests/BoardUiTests.cpp` (suite `RefPedalUi`): T-UI-WINDOW, FALLBACK, DOCK, NONBLOCK, REORDER, ORDER-AUDIO, MIX, STATE, STRESS, PERF.
- Acc: UI A1-A4, A8-A10; main editor never covered or resized by the pedalboard; toggle button and X agree; geometry validated against displays; visual order == `BoardModel::getOrder()` after every commit; one undo step per drag; idle repaints == 0.
- Risks: DocumentWindow unsupported in some hosts (fallback to dock tested); focus/keyboard in host windows; Wayland position restore (ignore position).

### WP18 UI board styles + mounting graphics
- Depends: WP1. Own: `Source/UI/ReferencePedals/{BoardSurfacePainter,MountPainter,MountView,BoardMaterials}.{h,cpp}`, `styles/{Plywood,Velcro,Clamp,Routed,Rail,Ceramic,Loose}.cpp`, `mounts/{Screws,Velcro,Clamps,Routed,Rail,Ceramic,None}.cpp`, `Source/Tests/Golden/ReferencePedals/*.png`. Test: `RefPedalUiStylesTests.cpp` (T-UI-SNAP-STYLE x7x2, MOUNT, SEED, DPI seams). Acc: 7 styles + 7 mountings per UI_BOARD_STYLES sec 5/8/9 (plywood screws each side, velcro strips, metal clamps, routed pockets, rail frame, ceramic tile, loose floor); procedural only; LOD0/1/2; tile cache <= 96 MB; high-contrast flat mode. Risks: paint cost (PaintCache keys), golden drift across OS fonts (mask text).

### WP19 UI pedal face infrastructure + generic faces
- Depends: WP1. Own: `PedalFaceFactory.cpp`, `GenericFace.{h,cpp}`, `PedalFaceCache.{h,cpp}`, `FootswitchButton.{h,cpp}`, `KnobControl.{h,cpp}` (value text with units, double-click reset, shift fine, wheel 1%, keyboard, a11y names), `Faces/generic_*_face.cpp` x10. Test: `RefPedalUiFacesTests.cpp`. Acc: knob/toggle/LED/footswitch behaviour per GENERIC 0.4; a11y names `"<pedal> <label>, <value>"`; face cache shared by model/rev/size/scale.

### WP20 UI branded faces I (drive, muff, dynamics: 10 pedals)
- Depends: WP19. Own: `Faces/{boss_ds1,ibanez_ts808,boss_sd1,mxr_distortion_plus,proco_rat,mxr_dynacomp,ehx_big_muff_nyc,ehx_muff_wicker,ehx_green_russian,ehx_deluxe_muff}_face.cpp`. Test: `RefPedalUiFacesDriveTests.cpp`. Acc: original vector art per model spec s7 (no logos/photos), mm size honoured, labels from spec, evidence-grade chip.

### WP21 UI branded faces II (mod/delay/looper: 8 pedals)
- Depends: WP19. Own: `Faces/{boss_ce2,boss_ce2w,mxr_phase90_script,boss_dd3t,tc_ditto,ehx_nano_360,boss_rc1,boss_rc5}_face.cpp`. Test: `RefPedalUiFacesModTests.cpp`. Acc: as WP20 plus looper LED ring / LCD progress, tempo LED, 7-seg BPM.

### WP22 UI cables, browser, MIDI learn, MIDI panel
- Depends: WP1, WP2, WP3 (feedback model), WP6 (learn). Own: `CableLayer.{h,cpp}`, `PedalBrowser.{h,cpp}`, `MidiLearnOverlay.{h,cpp}`, `MappingCallout.{h,cpp}`, `BoardMidiPanel.{h,cpp}`. Test: `RefPedalUiCablesTests.cpp` (T-UI-CABLE, FB, LEARN, A11Y). Acc: cable endpoints == jack positions +-0.5 px; chevrons; feedback arc states; colour never sole cue; learn flow for CC/AT/velocity/PB; per-slot MIDI channel + "Use pedal CC profile" toggle; browser Add/Remove/drag, CPU-cap message.

### WP23 Host integration
- Depends: WP2, WP4, WP5, WP6, WP1 registry (pedals optional, prototypes suffice).
- Own: `Source/LuthierEngine.{h,cpp}` (3 call sites: PRE, LOOP, NEW POST after `cabinet.processBlock`; `prepare/reset`; latency sum), `Source/PluginProcessor.{h,cpp}` (`beginBlock/endBlock` in `processSlice`, get/setStateInformation hook, `updateLatency`, 30 Hz reclaim, device MIDI -> `pushUiEvent`, optional aux buses appended AFTER existing ones, default disabled, `isBusesLayoutSupported` unchanged for old layouts), `Source/Parameters.{h,cpp}` (320 slot params + board params appended at END, `getParameterName` relabel), `Source/HostStateFormat.cpp` (`referencePedals` in `isKnownRootKey`).
- Test: `Source/Tests/RefPedalHostTests.cpp` - T-LEGACY (10 s corpus, factory presets, 44.1/48/96k, hash == `Golden/NormalizationOffHashes.json`-style golden, board absent/off => bit-identical), T-LEGACY2, T-AUTO (1k events/s), latency report debounce, state through real processor, old-project load (no key), newer-schema load.
- Acc: with board off/absent output bit-identical; APVTS ids stable & append-only; no allocation in `processSlice` (existing RT tests stay green); `LuthierRender` links.
- Risks: giant shared files (conflict magnet) - single owner, minimal diff; APVTS size (320) vs host param limits (VST3 ok); PDC changes mid-play (debounce 250 ms, structural-commit only).

### WP24 QA, stress, audit, release
- Depends: all.
- Own: `Source/Tests/RefPedalIntegrationTests.cpp`, `RefPedalSweepTests.cpp` (all 28 pedals x param grids 0/25/50/75/100% x levels x block sizes 1,16,64,37,481,2048 x 44.1/48/96k: finite, bounded +-16, deterministic, block-independent), `RefPedalSoakTests.cpp` (T-SOAK, `LUTHIER_SOAK=1` => 30 min, default 20 s), `RefPedalCpuTests.cpp` (T-CPU vs `spec/performance-budget.md`), edits to `Source/Tests/{ThemeSweepTests,GuiReachabilityTests,AccessibilitySweepTests}.cpp` (registration lines only), `scripts/ci_build.sh` (SANITIZE env + soak flag), `spec/performance-budget.md` (reference-board rows), `docs/` user guide section, `spec/reference-pedalboard/AUDIT.md` (audit findings log).
- Steps: (1) full unit + stress under ASan/UBSan, then TSan; (2) pluginval strictness >= 5 + clap-validator via `ci_build.sh validate`; (3) code audit: skills `audio-dsp-review`, `audio-numerics-review`, `juce-review` on every new engine dir + UI RT paths, `vst3-review`/`clap-review` on host integration; findings fixed by owning WP; (4) manual DAW matrix (Reaper, Live, Logic, Bitwig, Cubase, FL): window open/close/focus, MIDI clock/transport sync, velocity+aftertouch keyboard, automation; (5) legal review (no logos/photos/manual text; "inspired by" names); (6) release: `scripts/ci_build.sh all` on Linux/macOS/Windows, stage + package (Pro and Free editions).
- Acc: sec 6 global DoD.

## 5. Common acceptance block A-PED (every pedal)
Registers & constructs via registry; descriptor valid (prefix, lanes <=16, Engage alias); `process` no alloc/lock (hook); output finite & within +-16 for Inf/NaN/denormal/+-1e6 input and param fuzz; bit-identical across two runs; block-size independent <1e-9 (1,16,64,37,481,2048); sample rates 44.1/48/88.2/96/192k; enable toggle xfade 5 ms +-0.5 ms, no step > 1.5x natural; reported latency == oversampler delay and bypass path latency-matched; param round trip norm->value->norm +-1e-6; `saveExtra/loadExtra` round trip; CPU within `cpuUnits` [EST]; per-spec numeric tests (MEASURED/DOCUMENTED/ESTIMATED tags carried into comments); face present (WP19-21); MIDI local profile + FootEvent speed->xfade lerp(12,2,speed) implemented via board (not per pedal).

## 6. Global Definition of Done (release gate)
1. T-LEGACY/T-LEGACY2 bit-exact; all `refpedal*` suites + prior suites green on 3 OS.
2. ASan/UBSan and TSan clean (board, MIDI, UI stress T-UI-STRESS 10k ops).
3. 30 min soak: no xrun in offline sim, memory flat.
4. CPU: board overhead <=0.5% core, 8-pedal board <=8% [MEAS replaces EST in performance-budget.md].
5. Accessibility: keyboard-only reorder/add/remove, screen-reader names, reduced motion, high contrast.
6. Pop-out window and docked strip both work; main editor controls never obscured (user requirement).
7. Universal wet/dry knob (`ref_board_mix`) automatable + MIDI-learnable; reorder order == audio order (T-ORDER, T-UI-ORDER-AUDIO).
8. 7 board styles + 7 mountings; MIDI clock/transport sync; velocity/aftertouch footswitch emulation; slider/knob learn.
9. Audit findings closed; legal checklist signed; release artifacts built and validated.

## 7. Disjointness / shared-file rules
- Single-owner shared files: `CMakeLists.txt` WP1; `LuthierEngine.*`, `PluginProcessor.*`, `Parameters.*`, `HostStateFormat.cpp` WP23; `PluginEditor.*`, `AnimationPolicy.cpp`, `ReferencePedalDrawer.*` WP17; `scripts/ci_build.sh`, `ThemeSweepTests/GuiReachabilityTests/AccessibilitySweepTests` WP24; `ReferencePedalBoardTests.cpp` WP2; `ReferencePhraseLooperTests.cpp` + `PhraseLooper.h` removal WP15; specs: WP1 owner applies errata.
- Registration is per package (`Register*.cpp`); no package edits `PedalRegistry.cpp`.
- Test files are per package (names above); fixtures per package subfolder.
- Any agent needing a change in a file it does not own files an errata request to the owner; no cross-edits.

## 8. Prototype defects (must be fixed; owner WP in brackets)
Board.h
1. [WP2] No click-free reorder: `move()` swaps `packedOrder` instantly; pedal inputs change discontinuously => clicks. Needs dip-through-dry transition (BOARD_CORE 4.2).
2. [WP2] No click-free board enable/disable: `processStereo` early-returns on `!isBoardEnabled()` leaving per-slot `mix`/filter state stale; re-enable pops; tails cut.
3. [WP2] Only 8 slots (4-bit nibble pack in uint32), 3 knobs, 3 hard-coded models via `switch`; no registry, no per-model params, no descriptors.
4. [WP2] No universal wet/dry, no latency compensation, no oversampling (tanh/clamp aliasing), no stereo/dual-mono shortcut (L and R both fully processed even when identical).
5. [WP2] RT cost: per-sample `std::exp`/`std::pow`/`std::tan` in `lp()` and Phase 90 branch; coefficients must be block-rate/table.
6. [WP2] No parameter smoothing: knobs read once per block and applied raw (zipper); `step` recomputed per call.
7. [WP2] Disabled slot (`enabled=false`, mix ramped to 0) still runs `processOne` for the whole block (wasted CPU; no tail-silence skip).
8. [WP2] `setPlacement` accepts any int cast to `Placement` (unvalidated); `Channel::hpPrevious` misnamed (is an LP memory); `phase90` per-channel phase duplicated.
9. [WP2/WP23] `prepare()`/`reset()` mutate DSP state from the message thread with no audio-thread hand-off; no retire/RCU for model swaps.
10. [WP4/WP23] State never persisted (not in preset/host state) and no APVTS/automation/MIDI-learn: board vanishes on reload.
11. [WP23] Taps only PRE and LOOP; no post-cabinet tap although UI/spec require it.
PhraseLooper.h
12. [WP15] Single-slot mailbox `request()` overwrites commands; applied at block start, not sample-accurate.
13. [WP15] `prepare()/reset()` write `length/head/dataL/dataR` (plain ints/vectors) from the message thread while the audio thread may run: data race; `getCapacity()` reads `dataL.size()` unsynchronised.
14. [WP15] Auto-switch to PLAY at capacity without seam crossfade (click at loop point); no crossfade at REC->PLAY close, stop, wrap.
15. [WP15] Destructive overdub (no layers, no undo/redo); `record` while OVERDUBBING/RECORDING ignored (no single-button cycle); no gesture decode (tap/double/hold).
16. [WP15] Capacity clamp 60 s vs Nano 360 s total / RC-5 1.5 h per track; no paged/spilled storage; float32 stereo vectors allocated in full.
17. [WP15] Looper audio has no persistence path (must go through AssetStore, never inline in host blob).
ReferencePedalDrawer
18. [WP17] `Card::later.onClick` does `getParentComponent()->getParentComponent()` then `dynamic_cast<Drawer*>`: Card's parent is `contents`, whose parent is the Viewport's internal holder, so the cast yields nullptr and the UI is never refreshed after "move later" (works only for "earlier", which uses `onReordered`).
19. [WP17] Drawer is added with `addChildComponent` and laid out as a bottom overlay (`PluginEditor.cpp:625`): when open it COVERS the lower 232 px of the main controls instead of reserving space or floating (violates "must not interfere with required controls").
20. [WP17] Cards are bound to chain POSITION not instance id: UI state (focus, scroll, labels) jumps on reorder; no drag handle/drag-reorder; arrows only; no keyboard reorder.
21. [WP17] Fixed 3 knobs/labels hard-coded per `Model` via `labels[(int)model][3]` (out-of-range if enum grows); no per-model faces, value text/units, tooltips are static strings; no accessibility names/handlers; no focus order; no reduced-motion/high-contrast handling.
22. [WP17] No pop-out window, no collapse summary chip, no style/mounting, no cables/routing view, no MIDI learn/wet-dry/undo; `Drawer::resized` re-sizes `contents` every layout.
23. [WP17] Prototype test coupling: `ReferencePedalBoardTests.cpp` asserts `Board::slotCount`, `getIdAtPosition`, `Model::*Prototype` - migrate via shims (WP2), do not delete assertions.

## 9. Cross-cutting risks
| Risk | Mitigation |
|---|---|
| 28 pedals x ESTIMATED circuit values, no hardware captures | evidence grade chip in UI; no "accurate" claim; MEASURED fields filled only after captures (ARCHITECTURE gate) |
| CPU: 16 pedals with 4-8x oversampling | per-pedal `cpuUnits`, board CPU cap, Eco auto-fallback via structural transition, tail-silence skip |
| Merge conflicts on giant files | single-owner list sec 7, skeleton S-1 |
| Linker drops static registrars; ODR/duplicate symbols across 3 targets | explicit `registerAll`; `Source/ReferencePedals` has no GUI includes |
| Case-sensitive paths (`Golden/`, `Fixtures/`) | errata E19, CI on Linux |
| Latency changes during playback in hosts | debounce 250 ms, structural commits only, `ref_board_report_latency` |
| Window creation refused by host | automatic dock fallback (tested) |
| Feedback runaway / NaN | 7-stage safety, watchdog, board limiter, NaN guard (T-FB, T-NAN) |
| Host blob growth / looper audio | external content-addressed assets only; 256 KB cap |
| Legal (trademarks, art) | "inspired by" text only, original vector art, no manual text; WP24 legal gate |
