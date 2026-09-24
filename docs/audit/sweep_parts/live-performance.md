## live-performance.md

The live engines (snapshot bank with crossfade and morph, setlist with preload, tap tempo, kill switch, monitor mix, panic) are built and every §12 test exists; the LIVE workspace tab and the Live strip are reachable and the keyboard map is done. The gaps are on the input side and in persistence: live actions (next/prev/by-value snapshot, tap, kill, panic, setlist nav) cannot be assigned to a CC; the snapshot morph is not a parameter, so no pedal/LFO/mod-wheel can drive it; expression calibration is never fed incoming CCs (`observe`) nor applied to them (`map`); snapshots, live mode and MIDI-learn are saved only in the host session — `PresetManager::toVar` writes no `snapshots` block, contrary to §1/§11. Morph exclusions, Bezier points and monitor pan/EQ have no controls; Live Mode does not suppress tooltips.

| Req | Summary | Engine location | GUI location | Test | Status |
|---|---|---|---|---|---|
| LP-1 (§0.1) | Every live control reachable without a mouse — keys yes; CC only via PC/CC0; no CC for live actions | `PluginEditor::keyPressed`, `PluginProcessor::handleLiveMidi` | keys | `Accessibility::shortcutDefaultsMatchTheCanonicalTable` | PARTIAL |
| LP-2 (§0.2, §1) | Glitch-free switching; held notes not retriggered | `Live/Snapshots.cpp:SnapshotBank::recall`, `advance` | LIVE tab / Live strip | `LiveSnapshots::thousandRecallsNeverJumpAParameter`, `Combo::snapshotsAndPresetMorph` | DONE |
| LP-3 (§0.3, §5) | Tap tempo plugin-global, works with no host clock | `Live/TapTempo.cpp`, `PluginProcessor::tapTempoNow` | Live strip `TapPad`, key T | `LiveTapTempo::detectsTempoWithinHalfABpm` | DONE |
| LP-4 (§0.4, §6) | Kill within one block, 3 ms fades, DSP keeps running | `Live/LiveControls.cpp:KillSwitch` | Live strip `killButton`, key `\` | `LiveKillSwitch::reachesSilenceAndRecoversInsideFiveMilliseconds`, `LiveKillSwitch::fadesRatherThanJumping`, `LiveKillSwitch::doesNotDisturbTheSignalUnderneath` | DONE |
| LP-5 (§0.5) | No modal dialog on the live surface — rename uses CallOutBox; triptych "Open setlist" launches a FileChooser; untested | `LiveStrip` menus | Live strip | - | NO-TEST |
| LP-6 (§1) | 128 snapshots: params, mod matrix, bypasses, rhythm, 32-char label, 16 colours | `Snapshots.h:kMaxSnapshots/kMaxLabelLength/kNumColourTags`, `PluginProcessor::captureSnapshot` | LIVE `SnapshotGrid`; Live strip right-click Colour tag | `LiveSnapshots::captureAndRecallRoundTrip`, `Editor::theLiveTabEditsTheSnapshotBankAndTheSetlist` | DONE |
| LP-7 (§1) | Crossfade `snapshot_xfade_ms` 0-500, default 30 | `SnapshotBank::setCrossfadeMs` | LIVE `crossfade` slider | `LiveSnapshots::recallCrossfadesContinuousAndStepsDiscrete` | DONE |
| LP-8 (§1) | Bypass at crossfade midpoint; tails double-buffered; coupling matrix on worker — midpoint done; no tail double-buffer or coupling worker; untested | `SnapshotBank` `recallMidpointDone` | n/a | - | PARTIAL |
| LP-9 (§1, §11) | Snapshots stored in the `.luthierpreset` `snapshots` array; empty preset = implicit "Default" — saved only in host session state; `PresetManager::toVar` writes none; no implicit Default | `PluginProcessor.cpp:2215/2322`; `PresetManager.cpp:542` (known key only) | n/a | `LiveSnapshots::bankRoundTripsThroughJson` (bank only) | PARTIAL |
| LP-10 (§2) | PC = snapshot index; CC0 selects preset | `PluginProcessor::handleLiveMidi` | n/a | `LiveSnapshots::programChangeMapsAcrossAllOneTwentyEight` | DONE |
| LP-11 (§2) | CC assign: next / previous / snapshot-by-CC-value | - | - | - | MISSING |
| LP-12 (§2) | Keys `[` `]` step (Live Mode), 1-9, Shift+digit — dispatch untested | `PluginEditor::keyPressed` | keys | `Accessibility::shortcutDefaultsMatchTheCanonicalTable` (bindings only) | NO-TEST |
| LP-13 (§2, §10) | On-screen snapshot strip, bank of 8 + prev/next, active lit | `LiveStrip` `SnapshotStrip` | Live strip | `Editor::theModesThatChangeTheLayoutTakeEffectAndUndoThemselves` | DONE |
| LP-14 (§2) | Foot controller via MIDI Learn — parameters only; live actions not learnable | `Support/MidiLearn.cpp` | right-click Learn / header arm | `MidiLearn::mapsAndUnmapsCleanly` | PARTIAL |
| LP-15 (§3) | Morph A/B slots, knob, enable | `SnapshotBank::setMorphSlots/setMorphEnabled/setMorphPosition` | Live strip `slotAButton/slotBButton/morphSlider/morphEnable`; LIVE `morphEnabled` | `LiveSnapshots::morphFollowsItsCurve` | DONE |
| LP-16 (§3) | Morph driven by expression CC / LFO / mod wheel / sidechain env — snapshot morph is neither a parameter nor a mod destination | `SnapshotBank::setMorphPosition` (UI only) | - | - | MISSING |
| LP-17 (§3) | Continuous interpolate; discrete + bypass switch at 0.5 | `SnapshotBank::applyBlend`, `isDiscrete` | n/a | `LiveSnapshots::morphHonoursExclusionsAndDiscreteSwitching` | DONE |
| LP-18 (§3) | Per-parameter morph exclusion — engine only, no UI | `SnapshotBank::setParameterExcludedFromMorph` | - | `LiveSnapshots::morphHonoursExclusionsAndDiscreteSwitching` | NO-GUI |
| LP-19 (§3) | Curves linear/S/exp/4-point Bezier — Bezier control points have no UI | `MorphCurve`, `SnapshotBank::setBezierControlPoints` | LIVE `morphCurveBox` | `LiveSnapshots::morphFollowsItsCurve` | NO-GUI |
| LP-20 (§4) | Setlist `.luthierset` in ~/Documents/Luthier/Setlists with name/notes/bpm_default/entries | `Live/Setlist.cpp:toVar`, `getUserDirectory` | LIVE setlist list | `LiveSetlist::roundTripsThroughJson`, `LiveSetlist::reordersAndRemoves` | DONE |
| LP-21 (§4) | PageUp/PageDown navigation; user-assigned CCs — keys only | `PluginEditor::keyPressed` setlistPrevious/Next | keys | `Accessibility::shortcutDefaultsMatchTheCanonicalTable` | PARTIAL |
| LP-22 (§4) | Header triptych prev/current/next | `SetlistPlayer` | Live strip `SetlistTriptych` | `LiveSetlist::reportsPreviousCurrentAndNext` | DONE |
| LP-23 (§4) | Preload next entry, gap-free | `SetlistPlayer::preloadNext` | n/a | `LiveSetlist::walksForwardsAndBackwardsWithoutGrowing` | DONE |
| LP-24 (§5) | 4 taps / 3 s, median, outliers >30%, 20-300, snap 0.4 | `TapTempo` | TapPad | `LiveTapTempo::oneOutlierDoesNotMoveTheEstimate`, `LiveTapTempo::respectsRangeSnapAndHostPriority`, `LiveTapTempo::followsAGenuineTempoChange` | DONE |
| LP-25 (§5) | Tap drives rhythm (host stopped), synced delays, synced LFOs — wired via `blockTempo`; no test that a tap reaches delay/LFO/rhythm | `PluginProcessor.cpp:1093` `engine.setTempoBpm`, `context.bpm` | n/a | - | NO-TEST |
| LP-26 (§5) | Tap assignable to CC / footswitch — key only | - | key T | - | MISSING |
| LP-27 (§5) | Header LED blinks each beat in tap accent — the Live strip pad blinks; header LED does not | `LiveStrip` TapPad blink | Live strip | - | PARTIAL |
| LP-28 (§5) | Host tempo wins unless "internal tempo" forced | `TapTempo::getEffectiveBpm` | Options toggle | `LiveTapTempo::respectsRangeSnapAndHostPriority` | DONE |
| LP-29 (§6) | Kill momentary, key `\`, red pill; CC assignment — no CC route | `KillSwitch` | Live strip `killButton` | `LiveKillSwitch::reachesSilenceAndRecoversInsideFiveMilliseconds` | PARTIAL |
| LP-30 (§7) | Monitor: main + sidechain sum, level; to 2nd output pair; bypass when idle | `LiveControls.cpp:MonitorMix`, `RoutingMatrix::writeMonitorBus` | Live strip `monitorLevel` | `LiveMonitor::idleWhenNothingToMonitorAndSumsWhenThereIs` | DONE |
| LP-31 (§7) | Monitor pan + 3-band EQ — engine only | `MonitorMix::setPan/setEqLowDb/Mid/High` | - | - | NO-GUI |
| LP-32 (§7) | Click bus to monitor only (main optional) | `PluginProcessor::processSlice` click routing | Practice METRO "click to main" | `TuneProcessor::theMetronomeTabSendsTheClickToTheMainOut`, `TuneProcessor::theCountInIsHeardOnTheMainOutWhenSentThere` | DONE |
| LP-33 (§8) | Calibration wizard heel/toe, dead-zones, curve — UI exists but `ExpressionCalibrationSet::observe` is never fed incoming CCs, so the wizard never sees the pedal | `LiveControls.cpp:ExpressionCalibrationSet` | Options > EXPRESSION (`OptionsPages.cpp`) | `LiveExpression::wizardCapturesHeelAndToe` (direct calls) | PARTIAL |
| LP-34 (§8) | Calibration remaps incoming CC to 0-1 — `map()` never called on the MIDI path | `ExpressionCalibrationSet::map` | n/a | `LiveExpression::calibrationMapsRealTravelOntoFullRange` (unit only) | MISSING |
| LP-35 (§8, §11) | Per-CC, global in config/expression.json | `ExpressionCalibrationSet::getConfigFile/load` | n/a | `LiveExpression::calibrationSetRoundTrips` | DONE |
| LP-36 (§9) | Panic: notes off, tails, DC, coupling; keeps preset/snapshot/params | `LuthierAudioProcessor::panic` -> `LuthierEngine::panic` | header panic, key P | `Engine::panicSilencesEverything`, `Combo::unisonStringsNeverGrowAndPanicSilencesThem` | DONE |
| LP-37 (§9) | Panic by CC assignment | - | - | - | MISSING |
| LP-38 (§10) | Live strip: snapshots, triptych, tap, morph, kill, monitor | `UI/LiveStrip.cpp` | header LIVE -> Live strip | `Editor::theModesThatChangeTheLayoutTakeEffectAndUndoThemselves` | DONE |
| LP-39 (§10) | Live Mode: 44 px targets, lock Advanced, suppress tooltips — no tooltip suppression; 44 px untested | `LiveStrip::kTouchTargetHeight`, `PluginEditor::updateLiveStripVisibility` | header LIVE | `Editor::theModesThatChangeTheLayoutTakeEffectAndUndoThemselves` | PARTIAL |
| LP-40 (§11) | Live-mode on/off per preset — saved in session `uiState.liveMode`; a preset load keeps it | `PluginProcessor.cpp:2216/2326` | header LIVE | - | PARTIAL |
| LP-41 (§11) | MIDI-learn for live controls per-preset by default, "global" flag — mappings live in session state only; no flag | `MidiLearnManager` | - | - | MISSING |
| LP-42 (§12) | Test: 1000 recall fuzz, zero clicks | - | n/a | `LiveSnapshots::thousandRecallsNeverJumpAParameter` | DONE |
| LP-43 (§12) | Test: PC across 128 | - | n/a | `LiveSnapshots::programChangeMapsAcrossAllOneTwentyEight` | DONE |
| LP-44 (§12) | Test: morph curve shapes, 0.001 at 100 points | - | n/a | `LiveSnapshots::morphFollowsItsCurve` | DONE |
| LP-45 (§12) | Test: tap 10 sequences within 0.5 bpm | - | n/a | `LiveTapTempo::detectsTempoWithinHalfABpm` | DONE |
| LP-46 (§12) | Test: kill -80 dBFS in 5 ms, 1000 activations | - | n/a | `LiveKillSwitch::reachesSilenceAndRecoversInsideFiveMilliseconds` | DONE |
| LP-47 (§12) | Test: 50-preset setlist walk, no memory growth | - | n/a | `LiveSetlist::walksForwardsAndBackwardsWithoutGrowing` | DONE |

<!-- counts DONE=25 NO-GUI=3 NO-TEST=3 PARTIAL=10 MISSING=6 OWNED=0 -->
