## JUCE_CLAUDE_GUIDELINES.md

These are generic JUCE rules. Checked against the code, most of them hold. What does not hold:
- There are no warnings-as-errors flags, and MSVC warnings are suppressed.
- pluginval runs at strictness 5 on every PR (10 only nightly and at release).
- `dynamic_cast` still runs in `EffectsChain::setOversamplingFactor`, which the parameter bridge can reach.
- An IR path outside the library root falls back to an absolute path.
- Factory presets are compiled into code rather than shipped as BinaryData.
- `setBufferedToImage` is used only by `AnimationPolicy` at motion Off.
- Unused output channels are cleared per bus rather than at the top of `processBlock`.
- The tests use a custom harness, not `juce::UnitTest` (it is equivalent, with about 1611 tests).

The audio-thread allocation / lock rules now hold and are tested (`Engine::fiveMinutesOfPlaybackNeitherAllocatesNorLocks`; only try-locks remain). The context7 rule and the host test matrix are process items and cannot be checked in code. JUCE is pinned to tag 8.0.10 by script, not as a submodule.

| Req | Summary | Engine location | GUI location | Test | Status |
|---|---|---|---|---|---|
| JG-1 (§1) | Query context7 before writing or reviewing JUCE code — a workflow rule, not checkable in code | n/a | n/a | - | PARTIAL |
| JG-2 (§2) | No allocation, `std::function` assignment or container growth on the audio thread; pre-size in `prepareToPlay` - allocation trap covers the whole processor | `LuthierAudioProcessor::prepareToPlay`, `CMakeLists.txt` | n/a | `Engine::fiveMinutesOfPlaybackNeitherAllocatesNorLocks`, `Circuit::sweepingEveryControlDoesNotAllocate`, `Capture::capturingTenThousandNotesDoesNotAllocate`, `PracticeGaps::theSessionRecorderTakesMidiWithoutAllocating` | DONE |
| JG-3 (§2) | No locks on the audio thread - blocking locks trapped (pthread_mutex_lock interposed); only try-locks remain in `EffectsChain`, `BodyEngine`, `CabinetEngine`, `TunePlayer`, processor | `DSP/Effects/EffectsChain.cpp`, `DSP/Body/BodyEngine.cpp`, `ConvolutionInstaller.h`, `PluginProcessor.cpp` ScopedTryLock | n/a | `Engine::fiveMinutesOfPlaybackNeitherAllocatesNorLocks`, `ThreadProbe::theLockTrapSeesALock`, `StateModel::loadingAPresetWhileRenderingProducesNoGarbage` | DONE |
| JG-4 (§2) | `Pedal::setOversamplingFactor` is virtual (no-op default, DrivePedalBase overrides); `EffectsChain` no longer dynamic_casts | `EffectsChain.cpp:238` | n/a | `Effects::oversamplingChangeReachesEveryDrivePedal` | DONE |
| JG-5 (§2 / §5) | No GUI calls from the audio thread; `parameterChanged` marshals via `AsyncUpdater`; editor polls on timers | `ParameterBridge : AsyncUpdater` (`Parameters.h:430`) | n/a | CI pluginval "Open editor whilst processing"; `Editor::itLaysOutAndPaintsAcrossItsResizeRange` | DONE |
| JG-6 (§3) | DSP code free of GUI headers | `Source/DSP` (includes `juce_dsp` only, `DspCommon.h:12`) | n/a | builds headless (`LUTHIER_HEADLESS=1` render target) | DONE |
| JG-7 (§4) | JUCE pinned to a tag, never `develop` (spec says submodule; pinned by clone of tag 8.0.10) | `scripts/ci_build.sh:33`, `scripts/setup_linux.sh:12` | n/a | CI build.yml | DONE |
| JG-8 (§4) | `juce_add_plugin` with explicit FORMATS (VST3 Standalone, + AU on macOS); `COPY_PLUGIN_AFTER_BUILD` off; stable `PLUGIN_CODE` Lthr / `MANUFACTURER_CODE` Ltha; static MSVC runtime; C++17, no GNU extensions | `CMakeLists.txt:5-52` | n/a | CI build.yml (3 platforms) | DONE |
| JG-9 (§4 / §12) | Warnings as errors in CI (`-Wall -Wextra -Wpedantic -Werror`, `/W4 /WX`) — no warning flags anywhere; MSVC warnings suppressed (`/wd4244 /wd4267 /wd4305 /wd4996`) | `CMakeLists.txt:10` | n/a | - | MISSING |
| JG-10 (§5) | Single APVTS; IDs as constexpr in one header; cached raw pointers read on the audio side | `LuthierAudioProcessor::apvts`, `Parameters.h` `ParamIDs`, `ParameterBridge::cachePointers` | n/a | `Parameters::everyParameterHasAUniqueIdAndSaneDefault` | DONE |
| JG-11 (§5) | GUI controls through attachments, no hand-wired listeners — headstock detune and string mute bypass parameters | `LuthierKnob::attachTo`; `GuitarBodyComponent` detune, `FretboardComponent::setStringMuted` | headstock popover, fretboard | `GuiReach::operatingEachControlWritesItsParameter` | PARTIAL |
| JG-12 (§5 / §10) | State via copyState / replaceState, version-tagged (`pluginVersion`), backward compatible — custom JSON with `pluginVersion` and migrations (equivalent) | `PresetManager.cpp:358` | n/a | `HostState::theSameParametersGiveTheSameStateHoweverTheyArrived`, `Presets::stateRoundTripsExactly` | DONE |
| JG-13 (§6) | `prepare` and `reset` on every DSP object in `prepareToPlay` | `LuthierEngine::prepare` / `reset`, `Pedal::resetBase` | n/a | `MidiExport::luthierRoundTripNullsEveryFactoryPreset`, `ReviewRegression::prepareKeepsTheSequencerSteps` | DONE |
| JG-14 (§6) | Smoothing on continuous parameters (no zipper noise) | engine smoothers (`outputMixNow`, `widthNow`, pedal fades) | n/a | `PresetMorph::aFourSecondSweepDoesNotClick`, `Effects::chainReordersWithoutGlitching` | DONE |
| JG-15 (§6) | `ScopedNoDenormals` at the top of `processBlock` | `PluginProcessor.cpp:999`, `LuthierEngine.cpp:1832` | n/a | `Engine::fastSlidesProduceNoNansOrDenormals` | DONE |
| JG-16 (§6) | Variable block sizes, including blocks larger than prepared | `processSlice` | n/a | `Engine::blockSizeChangesAreSurvived`, `ReviewRegression::aBlockBiggerThanPreparedIsRenderedWhole` | DONE |
| JG-17 (§6) | Bus layouts validated explicitly in `isBusesLayoutSupported` | `PluginProcessor.cpp:284` | n/a | `PluginBuses::perStringLayoutPutsEachStringOnItsOwnBus` | DONE |
| JG-18 (§6 / §12) | Verified: `RoutingMatrix::distribute` writes or clears every enabled non-main bus each block (auditor missed the else/clear branches); test proves host garbage never survives | `Routing/RoutingMatrix.cpp:442` | n/a | `PluginBuses::anEnabledBusNobodyWritesIsSilent` | DONE |
| JG-19 (§6) | Oversampling factor is a parameter | `ParamIDs::oversample`, `LuthierEngine::setOversamplingFactor` | Options AUDIO; Advanced | `Combo::pairwiseAcrossMajorSettings` | DONE |
| JG-20 (§6) | MIDI processed at its sample position | `processSlice` slicing | n/a | `MidiExport::liveMidiOutKeepsTenThousandEventsOnTheirSample`, `Controllers::chordGroupsSoundOneWindowAfterTheyWerePlayed` | DONE |
| JG-21 (§7) | Background → audio by building off-thread and swapping atomically | `LuthierEngine::swapPartsAtBlockBoundary`, `ConvolutionInstaller.h` | n/a | `PartSwap::aSwapKeepingTheStructureIsTakenAtABlockBoundaryWithoutSilence`, `WorkshopSwap::noFileIsTouchedFromTheAudioThreadDuringASwap` | DONE |
| JG-22 (§8) | Cache expensive drawables; 30 Hz timers pull meter data - faces are cached; `setBufferedToImage` is used only by `AnimationPolicy` at motion Off | `UI/Faces` image cache; timers 4-30 Hz | all panels | `FacesIntegration::facesAreDrawnOnceAndAgainOnlyWhenTheyChange` | PARTIAL |
| JG-23 (§8) | DAW-driven resize handled cleanly; editor size restored from state | `PluginEditor.cpp:145,182` (`uiState.editorWidth`) | window | `Editor::theProcessorHandsOverAnEditorAtItsDocumentedSize`, `Editor::itLaysOutAndPaintsAcrossItsResizeRange` | DONE |
| JG-24 (§9) | `juce::UnitTest` suites for every DSP class — a custom `LUTHIER_TEST` harness is used instead (equivalent) | `Tests/TestFramework.h` | n/a | 832 registered | DONE |
| JG-25 (§9 / §12) | pluginval `--strictness-level 10` in CI on every PR - push / PR run strictness 5 (build.yml); 10 on manual dispatch, release and `scripts/pluginval.sh` | `.github/workflows/build.yml:75` | n/a | CI pluginval | PARTIAL |
| JG-26 (§9) | Test in Reaper, Ableton Live, FL Studio, Cubase, Logic; auval on macOS | - | - | - | MISSING |
| JG-27 (§10) | Never store absolute paths in state — `IrLibraryPaths::toPresetPath` falls back to the full path for an IR outside the library root | `ToneMatch` `toPresetPath` (`getFullPathName`) | n/a | `ToneMatch::*` (ToneMatchTests.cpp:471-494 toPresetPath) | PARTIAL |
| JG-28 (§10) | Factory presets shipped as BinaryData / ValueTree — compiled into `FactoryPresets.cpp` (still in the binary; no BinaryData) | `Presets/FactoryPresets.cpp` | preset browser | `Presets::everyFactoryPresetLoadsAndPlays` | DONE |
| JG-29 (§11) | VST3 parameter count and IDs locked once shipped; new params appended | `Parameters.cpp` | n/a | `Parameters::everyParameterHasAUniqueIdAndSaneDefault` (exact count 453) | DONE |
| JG-30 (§11) | AAX only with PACE — not built | `CMakeLists.txt:35` | n/a | n/a | DONE |

<!-- counts DONE=23 NO-GUI=0 NO-TEST=0 PARTIAL=5 MISSING=2 OWNED=0 -->
