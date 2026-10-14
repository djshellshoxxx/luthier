# Pedal models: integration map

Read-only scout; paths relative to `/home/user/luthier`.
## 0. Hazards

1. **Appending a `PedalType` rescales saved slot types.** `slotType` is an
   `AudioParameterChoice` over `Parameters::pedalTypeNames()` (Parameters.cpp:731,742).
   Presets/host/A-B/undo store the NORMALISED value (idx/(N-1); PresetManager.cpp:553-567)
   and restore it with `ExactRestore::applyNormalised` (:795-801). No remap exists.
   `Doubler`/`Gater` were appended (Pedal.h:33-37), same exposure. Any new enum entry
   shifts old files unless a schema-keyed migration rewrites `*_type`. Prefer variants
   inside an existing type, or new optional params/blocks.
2. Pedal state is **APVTS params**, not a pedal block: per slot `type, bypass, mix, p0..p9`
   as `preN_*`/`postN_*` (Parameters.cpp:46-53, 742-748). `Pedal::kMaxParams = 10`
   (Pedal.h:85); the rack needs a pedal to declare <= 9 (MIX takes the spare, PedalRack.cpp jassert).
3. Append new param IDs at the END of `createParameterLayout` (hosts automate by index; Parameters.cpp:752).
4. Float-only host: no double-precision override in Source/; `processBlock(AudioBuffer<float>&)` (PluginProcessor.h:80).

## 1. Audio path

`LuthierAudioProcessor::processBlock` (PluginProcessor.cpp:1418) -> `bridge.applyToEngine()`
(:1903) -> `engine.processBlock` (:1953) -> `LuthierEngine::processBlock` (LuthierEngine.cpp:2533)
-> `processSubBlock` (:2825; called :2583, :2635). Racks run there as
`EffectsChain::processStereo(double*, double*, int)` (EffectsChain.cpp:280), **double, in place**.

| Step | Where (LuthierEngine.cpp) | Notes |
|---|---|---|
| pre rack | :3441-3460 | mono `instrumentBuffer` copied to engine-owned `pedalScratchL/R`: **dual-mono stereo** |
| saturation | :3462-3464 | `Saturation`, bypass at 0 |
| EQ-match pos 0 | :3467-3478 | |
| amp | :3481-3502 | `(L+R)/2` -> `amp.processSample`, written to both channels |
| post rack | :3525 | stereo-capable; Aux 6 dry tap :3514-3524 |
| to float | :3527-3531 | `workBuffer` float 2ch from here |
| cab, room | :3547, :3573 | JUCE float buffers |
| secret fx, master | :3583-3606, after :3609 | freeze, width/mix, limiter |

Order: guitar -> pre rack -> saturation -> amp -> post rack -> cab -> room -> secret -> master.
A pedal sees true stereo only post-amp.

Mechanics (DSP/Effects/EffectsChain.*, Pedal.*):
- 8 slots (`kNumSlots`, EffectsChain.h:20); `Position` set LuthierEngine.cpp:129,132. `processStereo`
  try-locks `swapLock`; if a swap is in flight the block passes through unchanged (EffectsChain.cpp:285-288).
- Each slot runs `Pedal::processWithBypass` (Pedal.cpp:28): 10 ms bypass crossfade + smoothed
  wet/dry. Never call `process()` directly. Fully bypassed = no cost.
- Type change: `setSlotType` (EffectsChain.cpp:76) builds/prepares on the message thread,
  swaps under lock, retires the old pedal for message-thread free. Audio thread never allocates.
- Params: audio thread, `ParameterBridge::applyToEngine` (Parameters.cpp:1477; pedal loop
  :1889-1906) -> `fx.applySlotState` (EffectsChain.cpp:136) every block ->
  `setParameterNormalised` -> your `parameterChanged(i, realValue)`. **It runs on the audio
  thread: no allocation.** Structural: `applyStructural` (:2375; pedal types :2518-2560),
  `adoptPedalTypesFromParameters` (:2292, after preset load), `pushSlotParameters` (:2321);
  both apply `edition::effectivePedalIndex` (:2309, :2526).
- Tempo: `LuthierEngine::setTempoBpm` (:1260-1267) -> both racks; host side PluginProcessor.cpp:1656
  (`blockTempo`: host/tap/MIDI clock), :1731 tune, :1743 jam, :2893. Read `getTempoBpm()`
  (Pedal.h, clamp 20-300). New pedals get it at creation (EffectsChain.cpp:84).
- Expression: `midi.getExpression()` per block (LuthierEngine.cpp:3443-3444) -> `getExpression()` 0-1.
- Oversampling/quality: `applyOversamplingForQuality` (LuthierEngineQuality.cpp:22-34):
  `nominal = effectiveOversamplingFactor(user, sr)` (LuthierEngine.cpp:1246; /2 above 96k, /4 above
  176.4k); `driveFactor = capFactor(nominal, profile.driveOversamplingCap)`; sent to both racks as
  `setOversamplingFactor(effective, nominal, crossfade)` (EffectsChain.cpp:257; Pedal.h:186, default no-op).
  Reference: `DrivePedalBase::setOversamplingFactor` (PedalsDrive.cpp:611): runs at `effective`,
  `LatencyPad` pads to `Oversampler::latencyFor(nominal)`, 10 ms crossfade of old/new oversamplers.
- Latency: `Pedal::getLatencySamples()` (Pedal.h:180) summed by the chain (EffectsChain.cpp:268) and
  engine (LuthierEngine.cpp:3685-3697); `updateLatency` (PluginProcessor.cpp:1386) calls
  `setLatencySamples`; feedback loop uses pre + amp latency (:3104). **Report the NOMINAL factor's
  latency, constant across quality changes** (cpu-quality-modes 2.2).
- `reset()` must be allocation-free; `resetFromAudioThread()` (panic) try-locks and defers.

## 2. State

**a) `parameters` object** (preset, undo, A/B, host): `PresetManager::toVar` (PresetManager.cpp:437)
writes every APVTS param by ID, normalised (except morph position, jam transients; :540-565).
`fromVar` (:611) restores; a param ABSENT from a file is set to its default (PF-14, :802-815), so
old presets load any new param at default.

**b) Optional blocks beside `parameters`** (the pattern for a new pedal-model state block):
- Writer hooks (std::function on `PresetManager`): `captureJamBlock` (PresetManager.h:88),
  `captureTechniquesBlock` (:144), `capturePresetBlocks` (:108); wired PluginProcessor.cpp:137,146,203.
  Reader hooks `onJamBlockLoaded`, `onTechniquesBlockLoaded`, `onPresetBlocksLoaded`
  (PresetManager.cpp:1019-1027) get `obj->getProperty("x")`: a void `var` when absent = "defaults".
- Processor-owned blocks (modulation, snapshots, midi_mappings, rhythm_engine, routing, character,
  tone_match): Presets/PresetBlocks.cpp `writePresetBlocks` (:40) / `readPresetBlocks` (:92);
  `pick(key, legacyKey)` falls back to `defaultPresetBlocks` captured at construction (:154).
  **Recommended hook**: add a key to `PresetBlockKeys` (:28-39) plus one line in each function; the
  module's `fromVar(void)` must mean "as built". Write the block only when non-default (the
  midi_mappings rule, :53-55) to keep old saves byte-identical.
- **Known-key list**: new top-level preset keys must be added to `known` in `fromVar`
  (PresetManager.cpp:700-720) or they are treated as unknown, preserved and re-saved first (a
  missing comma there once fused two entries). Unknown keys: `unknownFields`, written FIRST by
  `toVar` (:447-450), known keys win. Keys nested in an existing block need nothing.
- Version: `kSchemaVersion = 1` (PresetManager.h:46) written as `schema` and `schemaVersion`
  (:452-453); newer refused (:675-690). Additive blocks need no bump. Migration examples:
  `doublerMigrated` B-07 (:453, :893), `backupMigratedOriginal` (:1391).

**c) Host blob**: `getStateInformation` (PluginProcessor.cpp:3687) / `setStateInformation` (:3887)
-> `restoreState` (:3892): JSON root with `preset` (same `toVar`), `midiLearn`, `ui`, `setlist`,
`lockedParameters`, `slotBActive`, ... HostStateFormat.cpp: `kCurrentStateFormatVersion = 1`
(PluginProcessor.h:127); `writeStateFormat` (:104) adds `formatVersion`/`savedBy` and re-emits
`unknownHostSections`; `readStateFormat` (:119) stores root keys not in `isKnownRootKey` (:36-46)
and backs up older blobs. A new session-level root key needs: write in `getStateInformation`,
`hasProperty` read in `restoreState`, entry in `isKnownRootKey`. Put pedal state inside `preset`
instead and none of that is needed.

Rule for a new component: absent block => reset to built default, touch nothing else, and the
default must be off/transparent so old presets sound unchanged.

## 3. Undo, MIDI learn, A/B, randomiser, editions

- **Undo** (Support/UndoHistory.*; PluginProcessor.cpp:3495-3600): an entry is the whole state blob
  captured BEFORE (`captureStateBlock` :3275 = `getStateInformation`), restored through
  `restoreState(..., RestoreScope::soundOnly)` (:3597); `ui`/`tune`/`metronome` are session layers
  kept across undo (:3600). Anything written by `getStateInformation` (APVTS + `preset`) is undoable
  for free. Multi-write gestures: `LuthierAudioProcessor::ScopedUndoAction(p, "text")`
  (PluginProcessor.h:691; used PedalRack.cpp:425, 534). State outside the blob:
  `pushUndoCallback(desc, class, target, undoFn, redoFn)` (:3529). Repeats of one class+target merge
  within 200 ms. Tests: UndoTests.cpp, UndoCoverageTests.cpp.
- **A/B**: `storeToSlot/recallSlot/copyAtoB` (PluginProcessor.cpp:3325-3360) = `captureStateBlock` +
  `restoreState(soundOnly)`; same coverage rule. A preset load clears both (PresetBlocks.cpp:162).
- **MIDI learn** (Support/MidiLearn.h:18): source key -> APVTS param ID. Pedal params are learnable
  when the control is a `LuthierKnob/Toggle/Choice/Slider` attached with
  `attachTo(processor, paramId, tooltip)` (they derive `LearnTarget`, Widgets.h:128, which also
  registers them for right-click menu, learn arm, global search). Non-APVTS state cannot be learned.
  A learn pushes its own undo entry (PluginProcessor.cpp:127). Mappings live in session `midiLearn`
  and preset `midi_mappings`.
- **Randomiser**: `PresetManager::randomise` (PresetManager.cpp:1807) resets all unlocked APVTS params
  to default, then randomises everything not excluded by `isRandomisable` (:1786; also `tune_*`, jam
  transients). Slot `_type` params are emptied 70% of the time (:1835-1841). Add new params to the
  exclusion list if random values sound broken. Locks are per param ID (`lockedParameters`). A state
  block outside the APVTS is neither randomised nor lockable unless you add that.
- **Search / path view**: Search/SearchProviders.cpp:58-175, 936 (uses `isPreAmpPedal`),
  Presets/Search/PresetFeatures.cpp, UI/AudioPathView.cpp read `slotType` and
  `ParameterLocations::parseSlotParameter`.
- **Editions** (Source/Edition.h, UI/EditionLocks.h, Support/Edition.h): `LUTHIER_PRO` is compile-time.
  `isFreePedalIndex` (Edition.h:102) is an explicit allow-list over enum order; **a new type index is
  Pro-only by default**. `effectivePedalIndex` (:162) maps a locked type to 0 (None) for playback
  only; the stored value is never changed (Pro presets round-trip through Free). UI:
  `EditionLocks::lockPedals(combo)` (EditionLocks.h:26; PedalRack.cpp:113). `Limits.pedalTypes` 15 Free;
  `rackSlotsPre/Post` (4 Free) is referenced only by EditionTests, so Free shows 8 slots today.
  Whole features: add to `Feature` enum (Edition.h:12), gate with `edition::has(Feature)`.
  EditionTests.cpp pins the Free list; update deliberately.
- Tests iterating `PedalType::NumTypes` (a new type is exercised automatically): EffectsQaTests,
  DocsSweepTests, QA_RtSafetyAlloc, EngineTests, GaterTests, FacesTests, FacesIntegrationTests.

## 4. UI hosting

- **Advanced**: AdvancedPanel.cpp:988-990 ("Pedalboard (before the amp)") and :1091-1093 ("Effects
  Loop (after the amp)"): `std::make_unique<PedalRack>(processor, post)` then
  `column.addControl(rack.get(), rack->getPreferredHeight())` into the scrolling `Column`; members
  AdvancedPanel.h:464. Helpers `column.addSection(title)`, `column.addGap(Metrics::grid)`.
- **Easy**: `CompactRack` (EasyPanel.h:48): 8 cards, click opens a popover containing the same
  `PedalSlotComponent` (`openSlot` :63); placed EasyPanel.cpp:717,727.
- **CircuitPanel hosts no pedals.**
- `PedalRack` (PedalRack.h:129) owns 8 `PedalSlotComponent` (:70, 10 Hz timer). `refresh()` rebuilds
  controls when the slot type changes; knobs are generated from `getParameterDescriptor`, so new
  descriptors need no UI code. Layout: `getPreferredHeightFor(width)`, `resized()` stacks slots with
  `Metrics::gridHalf` gaps (PedalRack.cpp:588-597). If a drawer changes the rack height, the owning
  panel must re-layout (`addControl` height is fixed at call time).
- **Face**: UI/Faces/PedalFace.h: `paintPedalFace`, `layoutPedalFace`, `knobCapFor(PedalType)`,
  `preferredHeightOnItsSide`, `pedalFaceTexts` (:41-77), `PedalFaceState::from(const Pedal&)`. The
  face is cached to a `juce::Image` keyed by type/bypass/enabled/knobs/size/scale/palette hash
  (PedalRack.h `CacheKey`); live knobs sit on `layoutPedalFace` rects under `FaceKnobLookAndFeel`
  (Faces/KnobCaps.h; `KnobCap` enum, `setCap`, `setFaceSwitch`). New look = new cases in PedalFace.cpp
  and `knobCapFor`; `faces::Materials::current()` (FaceMaterials.h) for palette-aware finishes.
- **Theme** (UI/Theme.h): `Palette::*` (:30-70), `Metrics::*` (:92-121: `grid 8`, `gridHalf 4`,
  `knobSmall 36`, `rowHeight 24`, `buttonHeight 28`, `panelCorner 4`, `animationMs 80`),
  `LuthierLookAndFeel` (:156). Palettes switch at runtime (`AccessibilitySettings::get().getColours()`):
  read `Palette::` at paint time, never cache colours, repaint in `lookAndFeelChanged()`.
- **Widgets** (UI/Widgets.h): `LuthierKnob{label, Size::Small}` (:189), `LuthierChoice` (:313),
  `LuthierToggle` (:350), `LuthierSlider` (:386), `SectionPanel` (:636), `InlineNotice` (:677).
  `attachTo` sets title/help for screen readers (`labelForScreenReaders`, Widgets.cpp:186).
- **Collapsible drawer model**: `PianoRollStrip` (UI/PianoRollStrip.h:91-108; .cpp:64-70, 145-165):
  `collapseButton` (glyph + `setTitle("Collapse ...")`, retitled by state), `isCollapsed()` from a
  persisted `uiState` field (`pianoRollExpanded`), `getPreferredHeight()` returns `kCollapsedHeight`
  when collapsed, `refreshButtons()` toggles child visibility then `resized()`. Persist open/closed in
  the host-state `ui` object (written in `getStateInformation`, read in `restoreState`
  PluginProcessor.cpp:3919+ with `hasProperty` default). Other drawers: `EasyPanel::setRiffDrawerOpen`
  (EasyPanel.h:152), `PracticePanel` (timer stopped while collapsed: closed = zero CPU).
- **Animation**: hold `AnimationPolicy::Registration motion {*this, AnimationPolicy::LiveReadout |
  Transition | Decorative, "Name"}` and `motion.startTimerHz(*this, hz)` (AnimationPolicy.h:117-123;
  `GateLed` PedalRack.h:47 is the model); call `AnimationPolicy::notePaint(*this)` in `paint`. Other
  timers fail the allow-list test (`getPollOnlyAllowList`, :197). Tween length:
  `AccessibilitySettings::get().getAnimationMs(ms)` (0 under reduced motion).
- **Accessibility** (Accessibility/Accessibility.h): `AccessibleSetup::configureSlider/Button/ComboBox/
  Meter/Descriptive`, `announceOverlayOpened`, `announce(text, Announcement)`. Every interactive
  control: `setTitle` + help/description; `setWantsKeyboardFocus(true)` for custom ones; containers
  return `AccessibilityHandler(*this, AccessibilityRole::group)` (PianoRollStrip.cpp:562). Never
  convey state by colour alone. Use `scaled()`/`scaledFont()` (font floor 10 px). Shortcuts are
  rebindable `ShortcutBinding` ids (`buildDefaultShortcuts`, `findAction`); document in
  docs/KEYBOARD_SHORTCUTS.md (pedal rack section: drag to reorder, right-click clears/resets).

## 5. Tests and trademark rule

- `Source/Tests/TestFramework.h`: `LUTHIER_TEST(Suite, name) { ... }` registers a static function through
  `TestRegistrar` into `TestRegistry::get()`; macros `CHECK`, `CHECK_MSG`, `CHECK_NEAR(a,e,tol)`,
  `CHECK_FINITE(buf,n)` use the implicit `ctx`. No central list to edit. Helpers: ComboHarness.h,
  QualityTestSupport.h, Tests/Fixtures, Tests/Golden. Model files: PresetPedalTests.cpp, GaterTests.cpp,
  FacesTests.cpp, EffectsQaTests.cpp.
- CMake: product sources `file(GLOB_RECURSE ... Source/*.cpp *.h)` minus `/Tests/` with
  `CONFIGURE_DEPENDS` (CMakeLists.txt:113-116); test sources `file(GLOB ... Source/Tests/*.cpp *.h)`
  (:329), **non-recursive: tests must sit directly in Source/Tests/**. Source/UI/ and `PluginEditor*`
  form the UI list (:281-294); `Source/WIP/` is excluded (:123).   Targets: `LuthierTests` (:337), `LuthierRender`; `add_test(LuthierUnitTests)` (:362).
- Run one suite: `cmake --build build-ci --target LuthierTests`, then
  `xvfb-run -a build-ci/LuthierTests_artefacts/Release/LuthierTests Gater` (CI: scripts/ci_build.sh:143).
  Args are case-insensitive SUBSTRINGS of suite or test name, OR-ed (TestSelection.h);
  `--list` dry-runs, `--skip=Suite` excludes an exact suite.
- **Trademark**: no brand in any string literal in Source/. `Tools/trademark_scan.py` scans every
  `"..."` in Source/**/*.cpp,*.h except `/Tests/`, skipping lines starting `//`, `*`, `/*` or containing
  `legacy name`; word-boundary match `(?<![A-Za-z])brand(?![a-z])`. Same list in
  Tests/TrademarkTests.cpp:12-26, which also checks `Parameters::pedalTypeNames()` and part names. Hits
  include Boss, MXR, Klon, Tube Screamer, Big Muff, Fuzz Face, Uni-Vibe, Leslie, Vox, Marshall, Orange,
  Deluxe, Champ, Jensen, Precision, so common words can trip it in UI text. Trailing `//` comments on a
  code line with a literal are not exempt. Real names only in docs/research (PROGRESS.md D1). Run
  `python3 Tools/trademark_scan.py` (exit 1 on any hit).

## 6. Reusable DSP

DSP/Common/DspCommon.h (namespace `luthier`):
- `double sanitise(double)` (:52; NaN->0, clamp +-4), `flushDenormal(double)` (:63), `dbToGain`,
  `gainToDb`, `centsToRatio`, `midiToHz(note, a4)`.
- `DCBlocker::prepare(sr, cutoffHz=7)`, `process(x)` (:87); `OnePoleLP::setCutoff(hz)|setPole|process`
  (:113); `OnePoleHP` (:151); `Allpass1` (:284).
- `Biquad::process(x)`; `setLowpass/Highpass/Bandpass/Notch/Allpass(sr,f,q)`, `setPeaking(sr,f,q,gainDb)`,
  `setLowShelf/HighShelf(sr,f,slope,gainDb)`, `setCoefficients(b0,b1,b2,a1,a2)` (:166-267).
- `TptSvf` (:586): `setBell/HighShelf/LowShelf(sr,hz,q,gainDb)`, `setLowpass/Highpass(sr,hz,q)`,
  `process(x)`, `processBlock(x,n)`, `rampTo(target,samples)`, static `processCascade(filters*,count,x,n)`.
- `ExpSmoother::prepare(sr,seconds)`, `setTarget`, `next()`, `snapToTarget`, `getCurrent` (:312);
  `LinSmoother` (:354); `SwitchCrossfade::prepare(sr,seconds)`, `trigger()`, `next()`, `isActive()` (:549).
- `EnvelopeFollower::prepare(sr)`, `setTimes(atk,rel)`, `process(x)` (:452); `Lfo` with
  `Shape{Sine,Triangle,Square,Saw,RandomSmooth,FingerVibrato}`, `prepare(sr)`, `next()` (:483);
  `RtRandom::setSeed(u64)`, `nextDouble()`, `nextGaussian()`, `nextInt(n)` (:399).
- Shapers: `softClip(double)` (:865; rational, +-1 beyond +-3), `tubeShape(double x, double bias)` (:873).
  Constants: `constants::kGuardLimit 4`, `kTwoPi`, `kPi`, `kDenormalFloor`.

`Oversampler` (DSP/Common/Oversampler.h; polyphase half-band, ~-70 dB): `prepare(sr, factor)`,
`setFactor(1|2|4|8)` (no-op if unchanged), `getFactor()`, `up(double x, double* dest)` (factor samples),
`down(const double* src)` (one sample), template `processSample(x, shaper)` (stateless shaper at the
high rate), `getLatencySamples()`, `static constexpr latencyFor(f)` = {1:0, 2:3, 4:5, 8:6} (:250),
`kMaxFactor 8`. One instance per channel (it has state). For quality-mode compliance derive from
`DrivePedalBase` (PedalsDrive.h:205-244: `LatencyPad`, `InputHistory`, old/new crossfade) and override
only `shape(double)` and `updateFilters()`.

`Saturation` (DSP/Effects/Saturation.h): `prepare(sr)`, `setAmount(0..1)`, `process(double*L,double*R,n)`;
smoothed tanh, NOT oversampled, true bypass at 0. It is the engine's closing stage, not a pedal block.

Pedal contract (Pedal.h): `getType`, `getName`, `prepare(sr,maxBlock)`, `reset`,
`process(double*,double*,int)`, `getNumParameters` (<=9), `getParameterDescriptor(i)` returning
`PedalParam{name, unit, min, max, default, skew, isDiscrete, numSteps, choices}` (:43), `parameterChanged`;
optional `getLatencySamples`, `setOversamplingFactor(eff,nom,xfade)`. New `PedalType` touch points: enum
(Pedal.h:21), `getTypeName`, `isPreAmpPedal`, `isPostAmpPedal`, `create` (Pedal.cpp:163-290), PedalFace.cpp
+ `knobCapFor`, Edition.h:102, hazard 0.1.
