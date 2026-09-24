## README.md

spec/README.md is the product front page: build instructions, content counts, architecture, DSP rules and deliberate deviations. Most counts and build claims hold on this checkout (AU is now added on macOS, CI runs pluginval, the allocation counter is compiled into the tests). Still false: the pedal count (22, not 21), the stale `DSP/Cable/` layout, "all three interpolators selectable", the "scrolling data stream unchanged" claim, the missing `THIRD_PARTY_LICENCES.txt` (both on visual), and the "byte-identical" IR regeneration (`make_irs.py` seeds from Python's randomised `hash()`).

| Req | Summary | Engine location | GUI location | Test | Status |
|---|---|---|---|---|---|
| RM-1 (Build/Req) | CMake 3.22, JUCE 8.0.10 fetched into ThirdParty/JUCE | `CMakeLists.txt`, `scripts/ci_build.sh:JUCE_TAG` | n/a | CI `.github/workflows/build.yml` | DONE |
| RM-2 (Targets) | VST3, Standalone, AU (macOS), LuthierTests, luthier-render; `-DLUTHIER_BUILD_TESTS/CLI=OFF` | `CMakeLists.txt:15-16,35-38` | n/a | CI build job | DONE |
| RM-3 (Install) | Build copies Resources/ beside every artefact | `CMakeLists.txt:luthier_copy_resources` | n/a | CI build + `Presets::everyFactoryPresetLoadsAndPlays` (reads Resources) | DONE |
| RM-4 (Box) | 25 instruments, 17 tunings + custom, 7 temperaments | `Model/Guitar/GuitarLibrary.h:GuitarType`, `TuningEngine.h:TuningPreset/Temperament` | Header selectors; col 1 Temperament | `Engine::everyGuitarTypeLoadsAndSounds`, `Engine::everyTuningLoadsAndSounds` | DONE |
| RM-5 (Box) | 12 string materials, 11 gauge sets | `StringMaterials.h:StringMaterial/StringGauge` | col 1 String Set | `StringPhysics::everyFactoryGuitarIsStringedPlausibly` | DONE |
| RM-6 (Box) | "22 pedals (21 effects plus the Doubler)" in two 8-slot reorderable racks — README corrected | `Effects/Pedal.h:PedalType`, `EffectsChain::kNumSlots=8` | col 2/3 PedalRack | `GuiReach::everySlotTypeBypassAndMixHasAControl`, `Doubler::isAPostAmpRackPedal` | DONE |
| RM-7 (Box) | 13 amps, 10 cabinets, 8 speakers, 7 mics, dual-mic + ToF align | `AmpEngine.h:AmpModel`, `CabinetEngine.h` | col 3 Amplifier / Cabinet and Mic | `FacesIntegration::theAdvancedAmpSectionHasItsControlsOnTheFace` | DONE |
| RM-8 (Box) | 720 IRs (216 body + 504 cab) | `Resources/BodyIRs`, cab IRs; `Support/IrLibrary` | n/a | `Cabinet::procedualFallbackRemovesTheFizz` (fallback only) | NO-TEST |
| RM-9 (Box) | 36 factory presets across Electric/Acoustic/Classical/Bass/Utility | `Presets/FactoryPresets.cpp:buildBank` (36 `make`) | Preset browser, Easy Style | `Presets::everyFactoryPresetLoadsAndPlays` (asserts >=20 only) | DONE |
| RM-10 (Arch) | Signal chain MidiInterpreter→Technique→Tuning→String×N+Coupling→Body→Pickup→Circuit→FX→Amp→Cab→Room→Master | `LuthierEngine.cpp:process` | n/a | `Engine::aNoteProducesSound`, `Circuit::theEngineRunsThroughTheCircuit` | DONE |
| RM-11 (Layout) | Source layout: `DSP/Cable/` dropped; Circuit/Feedback/Noise/Slap/Slide and the top-level feature dirs listed; diagram says GuitarCircuit | `Source/DSP/Circuit`, spec/README.md | n/a | - | DONE |
| RM-12 (Rule 1) | All internal DSP is double | `Source/DSP/*` | n/a | `StringEngine::survivesExtremeParameters` | DONE |
| RM-13 (Rule 2) | No alloc/lock/file I/O in processBlock — only module-level alloc checks; try-locks remain | `PluginProcessor::processBlock` | n/a | `Circuit::sweepingEveryControlDoesNotAllocate`, `TunePlayer::rendersWithoutAllocating` | OWNED |
| RM-14 (Rules 3,7) | DC blocker + NaN guard per recursive stage; ScopedNoDenormals + flushing | `DspCommon.h`, `PluginProcessor.cpp:999`, `LuthierEngine.cpp:1832` | n/a | `Common::dcBlockerRemovesOffset`, `Engine::fastSlidesProduceNoNansOrDenormals` | DONE |
| RM-15 (Rules 4,5,6) | Smoothed params, 5 ms discrete crossfade, SR recomputed in prepare, times in s/Hz | `DspCommon.h:ExpSmoother/LinSmoother`, `prepareToPlay` | n/a | `Engine::sampleRateChangesAreSurvived`, `PresetMorph::aFourSecondSweepDoesNotClick` | DONE |
| RM-16 (Rules 8-10) | reset() on every module, isolation, nonlinear stages oversampled 4x default | `Parameters.cpp:661 oversample default 2 (=4x)`, `Oversampler.h` | col 3 Master Oversampling; Options AUDIO | `Common::oversamplingSuppressesAliasing`, `Combo::renderIsDeterministicAfterReset` | DONE |
| RM-17 (CLI) | luthier-render `--midi --preset --out --audition --guitar --verbose --list-presets --help` | `Tools/RenderCli.cpp:168-184` | n/a | - | NO-TEST |
| RM-18 (Tests) | Test runner filter by name and `--list`, non-zero exit on failure | `Tests/TestMain.cpp` | n/a | CI unit-test step | DONE |
| RM-19 (Tests) | 10,000-state fuzz, audio-compared preset round trip, mono check | n/a | n/a | `Parameters::fuzzAcrossTenThousandStates`, `Presets::audioIsIdenticalAfterARoundTrip`, `Engine::monoCompatibility` | DONE |
| RM-20 (Tests) | pluginval claim now matches CI: v1.0.4, strictness 5 on push/PR, 10 nightly | `scripts/ci_build.sh:fetch_pluginval` | n/a | CI validate step | DONE |
| RM-21 (IRs) | `make_irs.py` seeds its scatter with `stable_seed` (zlib.crc32 of the IR name), `--out DIR` option; two runs verified byte-identical (PYTHONHASHSEED 1 vs 7). Shipped IRs not regenerated (README says so) | `scripts/make_irs.py:stable_seed` | n/a | manual: two `--out` runs diff clean | DONE |
| RM-22 (Runtime) | User presets / Renders / Diagnostics under Documents/Luthier; factory in bundle or Documents fallback | `PresetManager::getUserPresetFolder/getRenderFolder/getFactoryPresetFolder`, `Diagnostics::getDiagnosticsFolder` | Options > FILE LOCATIONS | - | NO-TEST |
| RM-23 (Runtime) | Options has a button for each folder | `OptionsPages.cpp:FileLocationsPage` | Options > FILE LOCATIONS `openUserFolder/openRenderFolder/openFactoryFolder/openDiagnosticsFolder` | `Editor::everyOptionsPageSelectsAndPaints` (paint only) | NO-TEST |
| RM-24 (Docs) | Documentation table: every listed doc exists | `docs/*.md` | n/a | - | DONE |
| RM-25 (Dev) | Synthesised IRs; any WAV loads | `ToneMatch/*`, `IrLibrary` | TONE MATCH tab | `ToneMatch::sweepDeconvolutionRecoversTheSourceIr` | DONE |
| RM-26 (Dev) | Guitar drawn from live GuitarSpec | `UI/Guitar/*` renderer | Easy top band, Advanced strip, Workshop | `GuitarIllustration::everyFactoryGuitarRendersWithoutClipping` | DONE |
| RM-27 (Dev) | Lagrange5 used; README now says all three are implemented and tested, not selectable | `FractionalDelayLine.h:Interpolation` | n/a (developer choice) | `DelayLine::allInterpolatorsPreserveLevel` | DONE |
| RM-28 (Dev) | Damping as cutoff; per-string positional comb; per-sample coupling; core-diameter inharmonicity | `StringEngine::setDamping`, `PickupEngine`, `CouplingMatrix` | n/a | `StringEngine::palmMuteShortensAndDarkens`, `Pickup::positionCombNullsTheExpectedHarmonic`, `Coupling::aStruckStringRingsItsNeighbour`, `StringPhysics::woundStringsAreLessStiffThanTheirDiameterSuggests` | DONE |
| RM-29 (Dev) | Cutaway drawn not clipped; signature notch; output LED | `PluginEditor.cpp:345 drawSignatureNotch`, `HeaderBar::led` | window | `Theme::controlsRenderInEveryPaletteAndRepeatExactly` | DONE |
| RM-30 (Dev) | Theme structure unchanged "including the scrolling data stream" — `DataStreamDisplay` built but never shown | `UI/Widgets.cpp:DataStreamDisplay` | none | - | OWNED |
| RM-31 (Licence) | `THIRD_PARTY_LICENCES.txt` beside the plugin — file absent; HelpContent.cpp:449 cites it | - | Help > About | - | OWNED |
| RM-32 (Licence) | Help > About shows licence text | `UI/HelpContent.cpp` | HELP tab | `HelpTab::theContentCoversWhatIncludeMdAsksFor` | DONE |

Notes: RM-13 on visual: "Audio thread: no allocation, no blocking lock" and "QA: allocation trap live". RM-30 on visual: `PluginEditor.h:145 DataStreamDisplay dataStream` + Options > Appearance switch, `AppearanceTests`. RM-31 on visual: `THIRD_PARTY_LICENCES.txt` and `Resources/THIRD_PARTY_LICENCES.txt` (287 lines).

<!-- counts DONE=25 NO-GUI=0 NO-TEST=4 PARTIAL=0 MISSING=0 OWNED=3 -->
