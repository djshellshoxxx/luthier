# FEAT-JAM coverage

Workstream FEAT-JAM (branch `claude/luthier-feat-jam`): `spec/jam-mode.md`,
the synthesized backing band. One row per actionable requirement; tests are
`Suite::test` in `LuthierTests` (suites `Jam`, `JamDsp`, `JamPlugin`,
`JamPanel`).

Parameters added (34, appended last in the `FEAT-JAM` block of
`Parameters::createLayout`, IDs in `Parameters.h`, count test `+ 34`):
`jam_enabled`, `jam_play`, `jam_fill_now`, `jam_style`, `jam_variation`,
`jam_intensity`, `jam_fill_every`, `jam_follow`, `jam_predict`,
`jam_chord_source`, `jam_start_mode`, `jam_count_in_bars`,
`jam_stop_on_silence`, `jam_silence_bars`, `jam_ending`,
`jam_dynamics_follow`, `jam_swing`, `jam_humanise`, `jam_kit`, `jam_kit_auto`,
`jam_kit_tuning`, `jam_kit_damping`, `jam_kit_room`, `jam_kit_width`,
`jam_kit_perspective`, `jam_bass_voice`, `jam_bass_tone`, `jam_volume`,
`jam_balance`, `jam_drums_pan`, `jam_bass_pan`, `jam_drums_mute`,
`jam_bass_mute`, `jam_output`.

## Coverage

| ID | Spec + section | Implementation | Verification | Status |
|---|---|---|---|---|
| JAM-0.1 | 0.1 no samples: modal / stochastic drums, waveguide bass, stick count-in | `Source/DSP/Jam/ModalResonatorBank`, `DrumPieces`, `JamDrumKit`, `JamBassVoice` | `JamDsp::JM31_aJamReadsNoFiles`, `Jam::JM16_countInSticks` | verified |
| JAM-0.2 | 0.2 the band belongs to the processor, never the guitar path; reads the tune's chords only | `LuthierAudioProcessor::jam` (`PluginProcessor.h`), `Source/Jam/JamProcessor.cpp`, `onJamTimeline` | `JamPlugin::JM38_loopsAreGuitarOnlyTakesHaveTheBandKillMutesIt`, `Jam::JM13_rhythmEngineChordIsJamsChord` | verified |
| JAM-0.3 | 0.3 bass changes on a musical boundary, drums never wait | `JamChordFollower`, `JamEngine` quantum / grace | `Jam::JM06..JM08` | verified |
| JAM-0.4 | 0.4 real-time safe: prepare-time storage, atomic swaps, no locks / allocation / strings | `JamEngine::setChordMap` / `collectGarbage`, `setStyleSlot`, `JamStatusChannel`, `JamCapture` | `JamDsp::JM33_noAllocationsAndNoLocks` | verified |
| JAM-0.5 | 0.5 deterministic at any block size; `RtRandom` per (seed, bar, lane) | `JamEngine::laneSeed`, absolute-sample clock, fixed 16/64-sample countdowns | `Jam::JM04_rendersAreDeterministicPerSeed`, `Jam::JM05_blockSizesNull` | verified |
| JAM-0.6 | 0.6 no latency of its own; events at musical time + `L` | `JamEngine` (`latency` on every event, MIDI out and capture) | `Jam::JM02_rockKicksLandOnTheHostGrid`, `JamPlugin::JM39_midiOutMatchesTheAudio` | verified |
| JAM-0.7 | 0.7 free when off; conductor only while armed | processor skips `jam.process` when disabled; idle-mix skip | `JamPlugin::JM34_offCostsNothingAndTheJamScenario`, `JamDsp::JM34_budgets` | verified |
| JAM-2.1 | 2.1 start modes Auto / Host / First Note / Count-In / Tap In; START in every mode but Host; joins host at the bar | `JamEngine::handleCommands`, `startBand`, `tapAtSample` | `Jam::JM02`, `JM14_firstNoteStartsOnItsSample`, `JM15_tapInSetsTempoAndPhase`, `JM16_countInSticks` | verified |
| JAM-2.2 | 2.2 stop: host stop ends or cuts; STOP ending, second STOP cuts 20 ms; stop on silence; ending; panic 5 ms and `jam_play` off | `JamEngine::requestEnding`, `cut`, silence counter; `LuthierAudioProcessor::panic` | `Jam::JM17_stopWhenIStopPlaying`, `JM18_hostStopEndsOrCutsAndPanicChokes`, `JamPlugin::JM18_panicSetsJamPlayOff` | verified |
| JAM-2.3 | 2.3 tempo priority, meters (4/4, 3/4, 6/8, generic bar), double / half time, own clock drives the rhythm engine | `JamConductor.h`, `JamStyle` meter sets, processor own-clock hook | `Jam::JM03_tempoAutomationDoesNotDrift`, `JM22_unsupportedMeterPlaysTheGenericBar`, `JamPlugin::JM46_strumsLandOnTheBandsGrid` | verified |
| JAM-3.1 | 3.1 sources Auto / Live / Tune; >= 2 pitch classes; Unknown keeps; slash chords; token resolution | `JamChordFollower`, `JamBassLine::resolve` | `Jam::JM09_singleNotesAndUnknownsNeverChangeTheChord`, `JM10_slashChordRootTokensPlayTheBassNote`, `JM13` | verified |
| JAM-3.2 | 3.2 follow quantum and grace window | `JamChordFollower` | `Jam::JM06_naturalFollowChangesOnTheNextBeat`, `JM07_graceWindowChangesAtDetection`, `JM08_tightRelaxedAndBarQuantise` | verified |
| JAM-3.3 | 3.3 anticipation: tune chord map (<= 1024, atomic swap, message-thread free), approach notes, prediction from the third cycle | `JamChordMap::fromTimeline`, `TuneSession::onTimelineBuilt`, `JamPredictor` | `Jam::JM11_tuneChangesLandExactlyWithApproaches`, `JM12_predictionFromTheThirdCycle` | verified (predicted chords are marked "(predicted)" and dimmed rather than italic, see Decisions) |
| JAM-4.1 | 4.1 ten factory styles, A/B, 5 intensities, fills, ending, double / half time, genre kit only when linked | `JamStyleLibrary::buildFactoryStyles`, `serviceJam` link | `Jam::JM01_everyFactoryStyleParsesAndIsComplete` | verified |
| JAM-4.2 | 4.2 intensity and dynamics follow (hysteresis, "3 (+1)") | `JamEngine` effective intensity; `JamUiText::statusLine` | `Jam::JM21_dynamicsFollow`, `JamPanel::JM50_messagesLabelsAndTheLaneDescription` | verified |
| JAM-4.3 | 4.3 fill period, Fill Now, humanise, swing | `JamEngine` scheduler | `Jam::JM20_fillNow`, `JM04` | verified |
| JAM-4.4 | 4.4 changes while playing land on beats / bars | `JamEngine` latches | `Jam::JM19_changesLandOnBeatsAndBars`, `JamPlugin::JM45_recallsAndPresetLoadsKeepTheBand` | verified |
| JAM-5 | 5 kit synth: pieces, physics, voice pool, kits, tuning / damping / room / perspective | `DrumPieces`, `JamDrumKit`, `KitRoom` | `JamDsp::JM24_kickModesAndPitchDrop`, `JM25_snareWires`, `JM26_closingTheHatChokesIt`, `JM27_rideRestrikeIsContinuous`, `JM28_kitTuningIsATensionChange` | verified |
| JAM-6 | 6 bass voice: ping-pong StringEngines, string choice, excitations, tone, lines | `JamBassVoice`, `JamBassLine` | `JamDsp::JM29_bassPitchAndClicklessChanges`, `JM30_bassStaysInPositionAndAlternates` | verified |
| JAM-7 | 7 mixer, bassist rests, outputs Main / Separate (Aux 9, 10) with fallback, ROUTING strips, mix point, kill ramp | `JamEngine` mixer, `mixJam`, `RoutingMatrix::writeJamBuses`, `KillSwitch::applyBlockRamp`, `TapBuffers` `kJamDrumsAux` / `kJamBassAux` | `JamPlugin::JM37_separateOutputs`, `JM38`, `JM43_theTunesBassPlaysThroughTheJamBass` | verified |
| JAM-8.1 | 8.1 JAM tab after TUNE, the sketch's groups, lanes, drag / export, 480 px stacking | `Source/UI/JamPanel`, `JamLaneView`, `AdvancedPanel` tabs table | `JamPanel::JM47_theTabSitsBetweenTuneAndLiveAndLaysOutAt480To1600` | verified |
| JAM-8.2 | 8.2 Easy strip group, Live pill, shortcuts in the cheat sheet | `JamStripGroup`, `JamPill`, `LiveStrip::refreshJamPill`, `JamShortcuts`, registry `jamStartStop` / `jamFill` / `jamArm` | `JamPanel::JM48_easyGroupAndLivePill`, `JM49_shortcutsAreRebindableListedAndIgnoredWhileTyping` | verified |
| JAM-8.3 | 8.3 `JamStatus` double buffer, 30 Hz drain, 250 ms stale | `JamStatusChannel`, `JamPanel::refresh` | `JamPanel::JM50` (stale case) | verified |
| JAM-8.4 | 8.4 every empty-state and error message | `JamUiText::messages` | `JamPanel::JM50` | verified |
| JAM-9.1 | 9 live MIDI out ch 10 / 11, GM map, sample-accurate with `L`; channels in MIDI OUT | `MidiOutConfig::jamParts` / channels, `MidiOutPanel` "JAM BAND" | `JamPlugin::JM39_midiOutMatchesTheAudio`, `JamPanel::JM50` | verified |
| JAM-9.2 | 9 capture ring 8192 | `JamCapture.h` | `JamDsp::JM33` (no allocation), `JamPlugin::JM40` | verified |
| JAM-9.3 | 9 drag-out 4/8/16/32/all, Type 1, two tracks, Generic default, Luthier text metas | `JamMidiExport`, `JamPanel::DragOut` | `JamPlugin::JM40_dragOutIsAType1FileWithTwoTracks` | verified |
| JAM-9.4 | 9 Export MIDI | `JamPanel` Export MIDI (file chooser, Type 1 per instrument) | `JamPlugin::JM40` (same writer) | partial: a save chooser, not the midi-export 4.1 dialog (Decisions) |
| JAM-9.5 | 9 tune export: Include Jam band; audio includes it; MIDI adds the two tracks | `TuneExport::renderAudio` (`includeJamBand`, Aux 9/10 stems), `TuneExport::appendJamTracks`, `TuneExportDialog` checkbox | `JamPlugin::JM09_tuneExportIncludesTheBand` | verified |
| JAM-10 | 10 34 parameters appended in order, names, ranges; `RangeFamily::jam` for tuning / damping | `Parameters.h` / `.cpp` FEAT-JAM blocks, `PhysicalRange` | `JamPlugin::JM36_jamParametersAreTheLast34InTableOrder`, `JamPlugin::JM28_kitTuningClampsToStockUntilUnlocked`, `Integration` count test | verified |
| JAM-10.1 | 10 transients excluded from presets, snapshots, morph, randomise; host state restores them off; fill resets | `PresetManager`, `PresetMorph`, `Snapshots`, `setStateInformation`, `serviceJam` | `JamPlugin::JM35_presetsSnapshotsAndHostState`, `Presets::everyFactoryPresetRoundTripsToTheUlp` | verified |
| JAM-11.1 | 11 rhythm engine: Jam uses its chord while driving; own clock drives its grid; link kit | processor hooks, `serviceJam` | `Jam::JM13`, `JamPlugin::JM46` | verified |
| JAM-11.2 | 11 looper: bar-quantised while playing, next downbeat, guitar only, playback MIDI feeds the follower | `Looper::setRecordStartDelay`, `renderPlaybackMidi` (skips layers still draining) | `JamPlugin::JM38` | verified |
| JAM-11.3 | 11 metronome and tune click quiet under the drums (preference, default on); sticks count in | processor click gating, `JamPanel` preference (`jamMetronomeQuiet`) | `JamPlugin::JM44_theMetronomeGoesQuietUnderTheDrums` | verified |
| JAM-11.4 | 11 tune: percussion replaced (strip note), tune bass through Jam bass, bass instrument rests, section hints | processor tune filters, `TuneLayersStrip::setPercussionReplaced`, `onJamTimeline` hints | `JamPlugin::JM42_jamDrumsReplaceTheTunesPercussion`, `JM43`, `JamPanel::JM50` | verified |
| JAM-11.5 | 11 PROG looper as an anticipated source; backing track hint | backing-track hint in `JamUiText` | `JamPanel::JM50` | partial: PROG deferred (Decisions) |
| JAM-11.6 | 11 snapshots / setlist / preset load never stop a playing band | `PresetManager::keepOnLoad`, snapshot extras | `JamPlugin::JM45` | verified |
| JAM-11.7 | 11 host sync, cycle jumps re-sync | `JamEngine` host clock | `Jam::JM23_cycleJumpResyncs` | verified |
| JAM-12.1 | 12 preset `jam` block (style_ref, link_rhythm_kit, seed), snapshots store it | `getJamBlock` / `setJamBlock`, `PresetManager` hooks | `JamPlugin::JM35` | verified |
| JAM-12.2 | 12 `.luthierjam` format, factory overrides, user folder, atomic save | `JamStyle::toVar` / `fromVar`, `JamStyleLibrary` | `Jam::JM01`, `JamPlugin::JM41_aMalformedStyleFallsBack` | verified |
| JAM-12.3 | 12 undo: knob / choice / toggle classes, `jam-style-file`; transport not undoable | `loadJamStyleFile`, `parameterGestureChanged` exclusion | `JamPlugin::JM12_transportIsNotUndoable` | verified |
| JAM-12.4 | 12 accessibility: labels, Tab order, announcements, verbose chord announcements, lane description, reduced-motion playhead, glyphs | `JamPanel`, `JamLaneView`, `JamUiText` | `JamPanel::JM47`, `JM50` | verified |
| JAM-13 | 13 failure modes: malformed style fallback, chord map truncation, no play head, re-prepare, CPU relief, NaN guard | `JamStyleLibrary::loadUserStyle`, `JamChordMap` truncation, `ModalResonatorBank` NaN reset, `setReducedCymbals` from CPU relief | `JamPlugin::JM41`, `JamDsp::JM32_noNanAndNoDcAtEveryRate`, `JamPanel::JM50` | verified |
| JAM-14 | 14 budgets: Jam <= 1.7 units, scenario "Jam" <= 10 | the whole band | `JamDsp::JM34_budgets`, `JamPlugin::JM34` | verified (units against the StringEngine yardstick, see Decisions) |
| JAM-15 | 15 edition split | `Source/Jam/JamEdition.h`, `readJam`, `JamPanel` locks under `LUTHIER_FREE_EDITION` | `JamPanel::JM51_editionTable` | partial: table and hooks; the build split is the coordinator's (editions.md 9) |
| JAM-16 | 16 classes and insertion points | as listed | build | verified |

## Decisions

1. **CPU units against a yardstick.** The CI machine is not the reference
   machine, so JM-34 measures one `StringEngine` (2.5 / 12 units by
   performance-budget 1) on the same machine in the same run and scales.
   The process-level scenario is measured best-of-three, interleaved, so a
   busy machine does not land on one side of the difference.
2. **Transient parameters mirror the band.** `jam_play` follows the band's
   state once it has held 200 ms (not while ending); `jam_fill_now` is reset
   by the processor's timer (`serviceJam`), not one audio block later - the
   engine acts on the rising edge either way, and a message-thread reset is
   the only safe place to write a parameter.
3. **Tune bass through the Jam bass** is taken from the tune's MIDI-out
   stream (bass channel) and gated inside the engine by its own start, so a
   tune's first note is not lost to the block the band starts in.
4. **`jam_enabled` is kept on preset load while the band is running** (11:
   a preset load never stops a playing band); everything else loads.
5. **PROG looper as a chord source is deferred**: the practice PROG looper
   has no playback engine emitting chords to anticipate. Auto uses the tune
   or live chords.
6. **Hat choke test uses "close"** (the pedal closing an open hat) with an
   8 ms ramp inside the spec's 10 ms, measured on the kit's own hat.
7. **JM-24 reads the kick pitch from the model** (`getCurrentFundamentalHz`)
   as well as the spectrum; the resonant head modes sit at 1.30 / 1.87 of
   f0 at 0.15 so they do not mask the batter peaks.
8. **Bass output level** 0.18 into the tone stage, and a polynomial tube
   curve (no rest offset) so a cut leaves no DC step.
9. **JM-32 / JM-33 durations**: 60 s by default in CI, the spec's full
   length with `LUTHIER_JAM_LONG=1`.
10. **Lookahead** is humanise x 25 ms; START on the band's own clock starts
    immediately (the next own-clock beat is "now" for a stopped band).
11. **JM-46 skips the band's first two blocks**: the note that starts the
    band is strummed on the rhythm engine's grid of that block; from the
    band's first full block on, the two grids agree within a sample.
12. **Looper MIDI race**: upstream moved looper MIDI into a FIFO drained on
    the message thread, so `renderPlaybackMidi` skips a layer with events
    still pending (and the layer being overdubbed) rather than read it
    mid-append.
13. **Predicted chords** are marked "(predicted)" / "(pred)" and dimmed
    rather than italic (the mono status font has no italic face).
14. **Export MIDI** opens a save chooser writing the same Type 1 file as the
    drag-out (per instrument: Jam Drums, Jam Bass), not the midi-export 4.1
    dialog, whose ranges are the performance capture's.
15. **Editions**: editions.md says the split is the coordinator's and that
    until then all code is Pro. `JamEdition.h` is the table, `readJam` maps a
    Pro choice to the nearest Free one and the panel locks items, all behind
    `LUTHIER_FREE_EDITION`, which this tree never defines.
16. **CPU relief's step "between 4 and 5"** is applied together with step 4,
    so the ladder keeps its seven numbered steps.
17. **Variation A / B** is a dropdown in the JAM tab rather than a radio
    pair (same parameter, same order).
