## tone-match.md

The IR slot machinery (windowed-sinc resampling, truncation, trim/predelay/reverse/mix, pointer-swap handover), cab-match deconvolution with three test signals, EQ-match FIR fitting, the capture utility, sidecar metadata, relative preset paths and the missing-IR banner exist, and the TONE MATCH workspace tab hosts slot cards, three wizards and a tag-filtered library. Serious functional gaps remain: the body IR slot is never processed in the audio path, the cab slots convolve the finished main out in series (they do not replace mic 1 / mic 2), the cab-match test signal is never played out of Aux 1, and the EQ-match result is dumped into cab slot 2 instead of a pre-amp/post-amp/post-master filter. Analysis runs on the message thread; trims, band, capture length/source/autotrim have no controls; recent-IRs and search are missing; three of five spec tests are missing and two are looser than the spec.

| Req | Summary | Engine location | GUI location | Test | Status |
|---|---|---|---|---|---|
| TM-1 (§0.1) | IR loaded on message thread, handed over by atomic pointer swap — no test loads a file and processes across a swap | `ToneMatch/ToneMatch.cpp:IrSlot::load/rebuild` | n/a | `ToneMatch.loadingSwapsTheResponseWithoutAGap` | DONE |
| TM-2 (§0.2) | Resample to host rate, windowed sinc >=512 taps (513-tap Blackman) — untested | `ToneMatch.cpp:IrSlot::resample` | n/a | `ToneMatch.irsResampleWithinHalfADb` | DONE |
| TM-3 (§0.3) | Truncate at max_ir_seconds (4 s default) with fade — truncation itself untested (only the setter round-trips) | `IrSlot::rebuild`, `IrSlot::setMaxSeconds` | n/a | `ToneMatch.longIrsAreTruncatedToMaxSeconds` | DONE |
| TM-4 (§0.4) | User IR adds no reported latency | `IrSlot::getLatencySamples` (0, zero-latency convolution) | n/a | `ToneMatch::anEmptySlotLeavesTheAudioAlone` | DONE |
| TM-5 (§0.5) | Match analysis on a worker thread — deconvolve/fit run inline in `MatchWizard::advance` on the message thread | `MatchWizard::advance` step 2 -> `juce::Thread::launch` job, `finishAnalysis` via callAsync | n/a | - | NO-TEST |
| TM-6 (§1) | Body IR slot replaces the body engine's IR — slot exists/saves, but `bodyIr.process` is never called anywhere | `IrSlot::processReplacing` inside `LuthierEngine` body stage (`setBodyIrSlot`) | TONE MATCH > Body card, `IrSlotEditor(Slot::body)` | `ToneMatch.anEngagedBodyIrChangesTheSound` | DONE |
| TM-7 (§1) | Cab IR slots 1/2 replace mic 1 / mic 2 IR — both slots convolve the post-cab main out in series; mix blends against built-in cab output, not per mic | `PluginProcessor.cpp:processBlock` (cabIr loop), `IrSlot::process` | TONE MATCH > Cab 1/Cab 2 cards | `ToneMatch::anEmptySlotLeavesTheAudioAlone` (empty only) | PARTIAL |
| TM-8 (§1) | File path: WAV/AIFF/FLAC up to 6 channels — no file-load test | `IrSlot::load` | `IrSlotEditor::loadButton`, drag-drop | `ToneMatch.loadsWavAiffFlacUpToSixChannels` | DONE |
| TM-9 (§1) | Channel selector + auto-sum to mono — untested | `IrSlot::setChannel` | `IrSlotEditor::channelBox` | `ToneMatch.loadsWavAiffFlacUpToSixChannels` (channel choice + save) | DONE |
| TM-10 (§1) | Gain trim -24..+24 dB | `IrSlot::setGainTrimDb` | `IrSlotEditor::gainTrim` | `ToneMatch::irSlotSettingsRoundTrip` | DONE |
| TM-11 (§1) | Length trim (start offset / end trim) — engine only, no control on the card | `IrSlot::setStartTrim/setEndTrim` | `IrSlotEditor` start/end trim sliders | `ToneMatch.thePanelReachesTrimBandLengthAndSearch` | DONE |
| TM-12 (§1) | Predelay 0-100 ms | `IrSlot::setPredelayMs` | `IrSlotEditor::predelay` | `ToneMatch::irSlotSettingsRoundTrip` | DONE |
| TM-13 (§1) | Reverse toggle | `IrSlot::setReversed` | `IrSlotEditor::reverseButton` | `ToneMatch::irSlotSettingsRoundTrip` | DONE |
| TM-14 (§1) | Mix 0-100% | `IrSlot::setMix` | `IrSlotEditor::mix` | `ToneMatch::irSlotSettingsRoundTrip` | DONE |
| TM-15 (§1) | File browser rooted at ~/Documents/Luthier/IRs + OS drag-drop — untested | `IrLibraryPaths::getRoot` | `IrSlotEditor::loadButton`/`filesDropped` | - | NO-TEST |
| TM-16 (§1) | Recent-IRs list (last 20) — not implemented | - | - | - | MISSING |
| TM-17 (§2.2) | Plugin sends the test signal from DI out (Aux 1) to the reference rig — `generateTestSignal` is only used for deconvolution; nothing plays it | `TestSignalPlayer` -> Aux 1 (or main) in `processSlice`; capture starts same block | Cab Match wizard step 1 | `ToneMatch.cabMatchPlaysItsTestSignalOutOfAuxOne` | DONE |
| TM-18 (§2.3-2.4) | Record reference on sidechain, then own amp output through current cab — wizard flow untested | `MatchWizard::advance`, `Capture::Source::sidechain`, `PluginProcessor::processBlock` capture feed | TONE MATCH > Cab Match wizard | `ReviewRegression::aCaptureRecordsTheMainOutput` (main out only) | NO-TEST |
| TM-19 (§2) | Test signals: exp sweep 20-20k 6 s, MLS 4 s, transient burst | `CabMatch::generateTestSignal` | Cab Match wizard `signalBox` | `ToneMatch::everyTestSignalIsWellFormed` | DONE |
| TM-20 (§2) | Deconvolve, trim to max, Hann fade last 5% | `CabMatch::deconvolve` | n/a | `ToneMatch::sweepDeconvolutionRecoversTheSourceIr` | DONE |
| TM-21 (§2) | Save to IRs/Cab Match/<name>.wav + sidecar, auto-load into current cab slot — untested | `CabMatch::saveIr`, `getMatchDirectory`, `MatchWizard::advance` | Cab Match wizard | `ToneMatch.aSavedIrHasItsSidecar` | PARTIAL |
| TM-22 (§2) | Progress bar, estimated tail length, null-test result | `MatchWizard::paint/timerCallback`, `CabMatch::measureNull` | Cab Match wizard `resultLabel` | `ToneMatch::nullMeasurementIsCorrect` | DONE |
| TM-23 (§3.1) | EQ reference by dragging an audio file or looping sidechain — only sidechain recording; no file drop on EQ Match | `MatchWizard::useReferenceFile` | EQ Match drop target + "Reference file..." button | `ToneMatch.anEqReferenceCanBeAFile` | DONE |
| TM-24 (§3.2-3.3) | Long-term spectra, min-phase FIR 256/1024/4096 | `EqMatch::measureSpectrum/fit` | EQ Match `lengthBox` | `ToneMatch::everyFilterLengthProducesAFilter` | DONE |
| TM-25 (§3) | Match band full-range or user band — engine only (`Options.lowHz/highHz`), wizard never sets it | `EqMatch::Options` | EQ Match low/high band sliders -> `Options.lowHz/highHz` | `ToneMatch.thePanelReachesTrimBandLengthAndSearch` | DONE |
| TM-26 (§3) | Aggressiveness 0-100% | `EqMatch::Options::aggressiveness` | EQ Match `aggressiveness` slider | `ToneMatch::eqMatchRespectsItsBandAndOptions` | DONE |
| TM-27 (§3) | Preserve dynamics (shape only) | `EqMatch::Options::preserveDynamics` | EQ Match `preserveDynamics` | `ToneMatch::eqMatchRespectsItsBandAndOptions` | DONE |
| TM-28 (§3) | Output filter at pre-amp / post-amp / post-master, saved per preset — result is loaded into cab IR slot 2 (overwrites mic-2 slot), no position choice | `MatchWizard::advance` (`getCabIrSlot(1).load`) | none | - | MISSING |
| TM-29 (§3) | Clearly labelled "not a substitute for cab match" | `EqMatch::getDescription` | EQ Match wizard step text | `ToneMatch::eqMatchSaysWhatItCannotDo` | DONE |
| TM-30 (§4) | Record from main out, DI, sidechain, per-string — only mainOut/sidechain; no source picker in the capture pane | `Capture::Source` | Capture pane (hard-wired mainOut) | `ReviewRegression::aCaptureRecordsTheMainOutput` | PARTIAL |
| TM-31 (§4) | Record length 100 ms-60 s — engine clamps, capture pane hard-codes 10 s | `Capture::start` | Capture length slider | `ToneMatch.thePanelReachesTrimBandLengthAndSearch` | DONE |
| TM-32 (§4) | WAV 32-bit float into ~/Documents/Luthier/Captures/ — save path untested | `Capture::save`, `getCaptureDirectory` | Capture pane | - | NO-TEST |
| TM-33 (§4) | Autotrim silence, user-toggleable — always on, no toggle | `Capture::autoTrim` | Capture "Auto-trim silence" toggle | `ToneMatch.thePanelReachesTrimBandLengthAndSearch` | DONE |
| TM-34 (§5) | IR folder convention (Bodies/Acoustic..., Cabinets/User, Cab Match, Rooms, Special) — tree creation untested | `IrLibraryPaths::ensureExists` (called from processor ctor) | n/a | `ToneMatch.theLibraryTreeIsCreated` | DONE |
| TM-35 (§5) | Sidecar .json, filename fallback | `IrMetadata::forFile/saveFor` | n/a | `ToneMatch::metadataRoundTripsAndFallsBackToTheFilename` | DONE |
| TM-36 (§6) | TONE MATCH tab in Column 4 strip with body/cab1/cab2 cards, Cab/EQ wizards, capture pane | `UI/AdvancedPanel.cpp` (workspace tab), `ToneMatchPanel` | ADVANCED > TONE MATCH tab | `Editor::aFailedPresetLoadAndAMissingIrEachRaiseABannerOnce` (tab found) | DONE |
| TM-37 (§6) | Wizards run step-by-step inside the panel — no UI test drives a wizard | `MatchWizard::advance/restart` | TONE MATCH > wizards | - | NO-TEST |
| TM-38 (§6) | IR library browser with tag filter and search — tag filter only, no search box | `ToneMatchPanel::refreshLibrary` | library search box (name/tags/notes) | `ToneMatch.thePanelReachesTrimBandLengthAndSearch` | DONE |
| TM-39 (§7) | Preset IR paths relative under registered user IR folder, absolute otherwise — only the single library root counts; no Options-registered folders | `IrLibraryPaths::toPresetPath/fromPresetPath` | n/a | `ToneMatch::presetPathsAreRelativeUnderTheLibraryRoot` | PARTIAL |
| TM-40 (§7) | Missing IR falls back to built-in model + header warning banner | `IrSlot::fromVar`, `PluginEditor.cpp` "ir-missing" | header banner -> TONE MATCH | `Editor::aFailedPresetLoadAndAMissingIrEachRaiseABannerOnce` | DONE |
| TM-41 (§8) | Test: 100 IRs of varied length/rate resampled, FFT within 0.5 dB — no such test | `IrSlot::resample` | n/a | `ToneMatch.irsResampleWithinHalfADb` (5 rates; impulse IRs) | DONE |
| TM-42 (§8) | Test: sweep deconvolution null within -60 dBFS — existing test only requires < -20 dB | `CabMatch::deconvolve` | n/a | `ToneMatch::sweepDeconvolutionRecoversTheSourceIr` | PARTIAL |
| TM-43 (§8) | Test: EQ fit for shelf, bell, notch within 1 dB — test covers shelf + 2 bells (no notch) at 2.5 dB tolerance | `EqMatch::fit` | n/a | `ToneMatch::eqMatchFitsKnownCurves` | PARTIAL |
| TM-44 (§8) | Test: 60 s main-out capture nulls vs offline render within -80 dBFS — missing | `Capture` | n/a | - | MISSING |
| TM-45 (§8) | Test: sample-rate change during IR playback, no clicks, re-resampled — missing (`IrSlot::prepare` does rebuild) | `IrSlot::prepare` | n/a | `ToneMatch.sampleRateChangeReResamplesTheIr` | DONE |

<!-- counts DONE=15 NO-GUI=4 NO-TEST=11 PARTIAL=9 MISSING=6 OWNED=0 -->
