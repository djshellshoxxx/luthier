## rhythm-engine.md

The rhythm engine is functionally complete: chord detector (84 templates, 30 ms bursts, 0.6 confidence floor), rubric voicer (replaces ChordVoicer per DECISIONS.md), strum and fingerpick scheduling, humanisation, bypass, free-run, 28 genre kits, 7+ fingerpick patterns, the ADVANCED > RHYTHM tab and the Easy strip, and all six §10 tests exist. Gaps: `processBlock`/`scheduleFingerpick` copy the whole `RhythmPattern` (with its `juce::StringArray tags`) on the audio thread, which allocates (§0.2); humanise is the engine's own copy, not the instrument's humanise settings; rakes have no un-muted target; per-finger fingerpick excitation is on realism-b (OWNED); there is no UI for pattern length/subdivision or hand span; the MIDI-out RHYTHM source buffer is never filled; rhythm state is saved in session/snapshots but not in `.luthierpreset` files. Several UI editors have no test.

| Req | Summary | Engine location | GUI location | Test | Status |
|---|---|---|---|---|---|
| RE-1 (§0.1, §1, §9) | MIDI transformer between interpreter and technique layer | `Rhythm/RhythmEngine.cpp:processBlock`, `LuthierEngine.cpp` (rhythm.prepare/process) | n/a | `GenreKits::everyKitSoundsWhenApplied` | DONE |
| RE-2 (§0.2) | Never allocates in the callback — `const auto pattern = patterns[...]` (RhythmEngine.cpp:599, 702) copies a `RhythmPattern` incl. `StringArray tags` every block/step | `RhythmEngine::processBlock`, `scheduleFingerpick` | n/a | - | PARTIAL |
| RE-3 (§0.3) | Sample-accurate to host ppq | `RhythmEngine::processBlock` (RhythmTransport) | n/a | `RhythmPatterns::strumSchedulingIsSampleAccurate` | DONE |
| RE-4 (§0.4) | Silent when stopped unless Free-run | `RhythmEngine::setFreeRun` | ADVANCED > RHYTHM `RhythmPanel::freeRunButton` | `RhythmPatterns::silentWhenStoppedUnlessFreeRunning` | DONE |
| RE-5 (§0.5, §9) | Quantized on read, humanized on write from the existing HumanizeMatrix — engine owns its own `RhythmHumanise`; instrument `hum_*`/`macro_humanize` params never reach it | `RhythmEngine::setHumanise`, `Parameters.cpp:applyToEngine` (interp only) | RHYTHM feel sliders | `RhythmPatterns::humanisationIsDeterministic` | PARTIAL |
| RE-6 (§0.6) | Own bypass, reverts within one block, no artefact | `RhythmEngine::setEnabled`, `pendingRelease` | RHYTHM `enableToggle`; Easy `rhythmEnableButton` | `RhythmPatterns::bypassIsCleanAndImmediate` | DONE |
| RE-7 (§2.1-2) | ChordDetector: pitch classes, 84 templates | `Rhythm/ChordDetector.cpp` | n/a | `Rhythm::chordTemplatesAreUnique`, `Rhythm::chordDetectorRoundTripsEveryTemplateInEveryKey` | DONE |
| RE-8 (§2.3) | Lowest pitch as bass / slash | `ChordDetector::detect` | n/a | `Rhythm::slashChordsReportTheirBass`, `Rhythm::chordDetectorHandlesInversions` | DONE |
| RE-9 (§2.5) | Confidence < 0.6 -> Unknown, literal mapping | `ChordDetector::kConfidenceFloor` | n/a | `Rhythm::lowConfidenceIsReportedAsUnknown` | DONE |
| RE-10 (§2) | Detect once per 30 ms NoteOn burst | `ChordDetector::kBurstWindowSeconds` | n/a | `Rhythm::burstWindowGroupsASpreadChord` | DONE |
| RE-11 (§3) | Voicer constraints 1-5 and score (rubric voicer per DECISIONS) | `Model/Playing/RubricVoicer` | n/a | `RubricVoicer::everyConstraintOfFourOneRejects`, `RubricVoicer::eachScoreTermIsTheRubrics` | DONE |
| RE-12 (§3) | handSpanFrets default 5, user 3-7 — no user control; engine hard-sets 5/6 | `RhythmEngine.cpp:344` `setMaxFretSpan` | - | `RubricVoicer::everyConstraintOfFourOneRejects` | NO-GUI |
| RE-13 (§3) | 8 voicing styles + bias (+bass) | `VoicingStyle`, `RubricVoicer` style bias | RHYTHM `styleBox` | `RhythmPatterns::voicingStylesProduceDifferentVoicings`, `RubricVoicer::styleBiasesAreFourThrees` | DONE |
| RE-14 (§3) | voicing_density 0-100 caps notes — effect untested | `RhythmEngine::setVoicingDensity` -> voicer | RHYTHM `densitySlider` | - | NO-TEST |
| RE-15 (§3) | hand_position_hint follows previous chord | `RhythmEngine.cpp:371`, `RubricVoicer::setPreferredPosition` | RHYTHM `handPositionSlider` | `RubricVoicer::oneFourFiveOneTravelsThreeFretsAtMost` | DONE |
| RE-16 (§4) | Strum expands to per-string plucks spaced over duration, Down/Up order | `RhythmEngine::scheduleStrum`, `StrumGesture::plan` | STRUM group in RHYTHM | `StrumDynamics::rhythmEngineStrumsAreSpacedExactly` | DONE |
| RE-17 (§4) | DownMute/UpMute set palm-mute flag | `isMutedStrum`, `emitNote` (Technique::PalmMute) | strum grid cell types | `GenreKits::everyFactoryStrumPatternSounds` | DONE |
| RE-18 (§4) | Rake = 3-5 muted strings before the target — whole strum muted and slowed, no un-muted target | `scheduleStrum` (rake: sps/1.6, muted) | strum grid | - | PARTIAL |
| RE-19 (§4) | Rasgueado 4 strokes 10-20 ms apart | `scheduleStrum` strokes=4 | strum grid | `GenreKits::everyFactoryStrumPatternSounds` | DONE |
| RE-20 (§4) | Dynamic falloff by strum_evenness | `StrumGesture::plan` | kit-set; STRUM group | `StrumDynamics::evennessBoundsTheVariation` | DONE |
| RE-21 (§4) | string_mask per step | `StrumStep::stringMask` | strum grid right-click Mask | `GenreKits::factoryMasksSelectStringsASixStringHas` | DONE |
| RE-22 (§4) | 16/32 steps, triplet and dotted per pattern — no dotted subdivision; no UI to set length/subdivision | `Patterns.h:Subdivision`, `kMaxSteps=32` | - | `RhythmPatterns::patternsRoundTripThroughJson` | PARTIAL |
| RE-23 (§4) | Humanize timing/velocity/miss/ghost | `scheduleStrum`, `processBlock` ghost | RHYTHM timing/velocity/miss/ghost sliders | `RhythmPatterns::humanisationIsDeterministic`, `StrumDynamics::missesAreWeightedAndDeterministic` | DONE |
| RE-24 (§5) | Fingerpick p/i/m/a/e assignment, per-finger excitation profile — here `emitNote` gets no finger; on realism-b: `emitNote(..., finger)` sets `on.finger` (fingerstyle-attack 3) | `RhythmEngine::scheduleFingerpick`, `RhythmPattern::getStringForFinger` | FingerpickGrid | - | OWNED |
| RE-25 (§5) | 7 factory fingerpick patterns | `PatternLibrary` (Patterns.cpp:598-625) | pattern browser | `RhythmPatterns::factoryPatternsAreWellFormed` | DONE |
| RE-26 (§6) | `.luthierpattern` JSON format incl. mask, finger | `RhythmPattern::toVar/fromVar` | RHYTHM SAVE/EXPORT | `RhythmPatterns::patternsRoundTripThroughJson` | DONE |
| RE-27 (§6) | Factory patterns in Resources/Rhythm, user in ~/Documents/Luthier/Rhythm — factory built in code; no Resources/Rhythm | `PatternLibrary::addFactoryPatterns`, `Patterns.cpp:668` | - | - | PARTIAL |
| RE-28 (§7) | 28 listed GenreKits with style/density/patterns/humanise | `GenreKitLibrary` (GenreKit.cpp:194-366) | RHYTHM `genreBox`; Easy `rhythmGenreBox` | `GenreKits::factoryKitsAreWellFormed`, `GenreKits::applyingAKitInstallsItsPatternAndSettings` | DONE |
| RE-29 (§7) | Kit rig is a soft reference | `PluginProcessor.cpp:1655` | RHYTHM `rigHintLabel` | `GenreKits::applyingAKitDoesNotLoadItsRig` | DONE |
| RE-30 (§7) | Kits as JSON under Resources/Genres — built in code; user dir scanned | `GenreKitLibrary::getFactoryDirectory` | - | `GenreKits::kitsRoundTripThroughJson` | PARTIAL |
| RE-31 (§8.1) | Genre dropdown + randomize dice | `GenreKit::randomPattern` | RHYTHM `genreBox`, `diceButton` | `GenreKits::randomPatternStaysInsideTheKit` | DONE |
| RE-32 (§8.2) | Capo up/down | `RhythmEngine::setCapoFret` -> TuningEngine | RHYTHM `capoDown/capoUp` | `GenreKits::capoRemovesFretsBelowItAndMovesThePitch` | DONE |
| RE-33 (§8.3) | Strum grid 16/32 steps, right-click dynamic/mask/delete — no UI test | n/a | RHYTHM `StrumGrid::mouseDown` | - | NO-TEST |
| RE-34 (§8.4) | Fingerpick grid 5 rows — no UI test | n/a | RHYTHM `FingerpickGrid` | - | NO-TEST |
| RE-35 (§8.5) | Swing 0-100% — slider is 50-75%; swing effect on timing untested | `RhythmPattern::setSwing`, processBlock | RHYTHM `swingSlider` | - | NO-TEST |
| RE-36 (§8.6) | Pattern browser: tag filter, load, save, export — untested | `PatternLibrary::findByTag` | RHYTHM `tagFilterBox`, `patternList`, LOAD/SAVE/EXPORT | - | NO-TEST |
| RE-37 (§8.7) | Live indicators: chord, fretboard dots, next-strum light — untested | `getCurrentChord`, `getNextStrumType` | RHYTHM `RhythmIndicators` | - | NO-TEST |
| RE-38 (§8) | Easy strip: kit, feel, on/off, Mono hint — only feel knob tested | `EasyPanel::buildRhythmStrip` | Easy `rhythmGenreBox`, `rhythmFeelSlider`, `rhythmEnableButton`, `rhythmHintLabel` | `StrumDynamics::theEasyFeelKnobScalesTheStrum` | NO-TEST |
| RE-39 (§9) | Writes only NoteOn/NoteOff/palm-mute | `RhythmEngine::emitNote`, `releaseAll` | n/a | `GenreKits::everyKitSoundsWhenApplied` | DONE |
| RE-40 (§9) | Preset params `rhythm_engine.enabled` / `.state` — the whole `RhythmEngine::toVar` travels in the preset's `rhythm_engine` block; not automatable (no host parameter) | `Presets/PresetBlocks.cpp`, `RhythmEngine::toVar/fromVar` | - | `Presets::processorBlocksTravelInThePresetFile` | PARTIAL |
| RE-41 (§9) | MIDI-out captures transformed stream — `MidiOutRouter::getRhythmBuffer()` never filled by the processor; notes only reach out via the string-activity source | `Routing/MidiOutRouter.cpp:96` | ROUTING / MIDI OUT `RHYTHM` switch (inert) | `MidiExport::liveMidiOutKeepsTenThousandEventsOnTheirSample` (router fed by hand) | PARTIAL |
| RE-42 (§10) | Test: chord detector 1008 cases | - | n/a | `Rhythm::chordDetectorRoundTripsEveryTemplateInEveryKey`, `Rhythm::chordDetectorHandlesInversions` | DONE |
| RE-43 (§10) | Test: voicer every chord on every ship guitar | - | n/a | `RhythmPatterns::voicerHandlesEveryChordOnEveryGuitar`, `RubricVoicer::everyShipGuitarVoicesEveryTemplate` | DONE |
| RE-44 (§10) | Test: strum quantization +-1 sample | - | n/a | `RhythmPatterns::strumSchedulingIsSampleAccurate` | DONE |
| RE-45 (§10) | Test: humanization determinism | - | n/a | `RhythmPatterns::humanisationIsDeterministic` | DONE |
| RE-46 (§10) | Test: bypass no click | - | n/a | `RhythmPatterns::bypassIsCleanAndImmediate` | DONE |
| RE-47 (§10) | Test: free-run tempo | - | n/a | `RhythmPatterns::silentWhenStoppedUnlessFreeRunning` | DONE |

<!-- counts DONE=30 NO-GUI=1 NO-TEST=7 PARTIAL=8 MISSING=0 OWNED=1 -->
