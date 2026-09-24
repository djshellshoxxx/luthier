## input-routing.md

Only the outer shape of the MIDI chain is in place: MIDI-out pass-through is captured first, then MIDI Learn, then program change / CC0, then the interpreter, technique layer and rhythm engine inside `LuthierEngine`. MIDI Learn does not consume what it learns (`processMidi` takes a `const MidiBuffer&`) and learns CCs only; there is no controller-profile stage, practice tools are not fed MIDI, expression calibration is never applied, incoming Luthier SysEx and MIDI clock/Start/Stop/SPP are ignored, and there is no Diagnostics fixture injection or `Tests/InputRouting/` suite. The only root file-drop is `.mid`/`.midi` (plus the IR slot); an extension router exists on the visual branch (`FileOpenRouter`, standalone open) but is not wired to drops. Keyboard handling works: shortcuts go through the rebind registry with clash refusal, and JUCE gives focused text fields the keys.

| Req | Summary | Engine location | GUI location | Test | Status |
|---|---|---|---|---|---|
| IR-1 (§0.1, §1) | Fixed consumer order export -> learn -> profile -> interpreter -> technique -> rhythm -> tune -> practice -> strings — no profile stage; practice absent; tune chord channel merged after learn (DECISIONS "TUNE in the plugin") | `PluginProcessor::processSlice` (1074, 1149-1180), `LuthierEngine` | n/a | - | PARTIAL |
| IR-2 (§1.1) | MIDI-out pass-through first, unchanged | `MidiOutRouter::captureInput` | n/a | `Routing::midiOutPassThroughIsSampleExact` | DONE |
| IR-3 (§1.1) | Armed MIDI Learn consumes the first non-note event — sees it but does not remove it (const buffer), so it also acts downstream | `Support/MidiLearn.cpp:MidiLearnManager::processMidi` | header arm / right-click Learn | `MidiLearn::armingIsSeparateFromLearningUntilAControlClaimsIt` | PARTIAL |
| IR-4 (§1.1) | Learn accepts CC / PC / aftertouch / channel pressure; note learn opt-in in Options — CC only | `MidiLearnManager::processMidi` (`isController` only) | - | - | MISSING |
| IR-5 (§1 step 3) | Controller profile stage remaps channels + latency compensation — profile is applied as interpreter settings; no remap/latency stage (see controllers CT-4) | `ControllerProfileLibrary::apply` | CONTROLLERS tab | - | PARTIAL |
| IR-6 (§1.1) | RhythmEngine consumes the chord notes it uses | `RhythmEngine::handleMidi/processBlock` | n/a | `GenreKits::everyKitSoundsWhenApplied`, `TuneProcessor::theMelodySoundsWhileTheRhythmEngineStrums` | DONE |
| IR-7 (§1.1) | TechniqueEngine tags, does not consume — no routing test | `Model/Playing/TechniqueEngine` | n/a | - | NO-TEST |
| IR-8 (§1 step 7) | TuneBuilder ignores incoming notes for playback; takes record events when armed | `PluginProcessor.cpp:1120` record path | TUNE tab | `TuneProcessor::theMelodySoundsWhileTheRhythmEngineStrums` | DONE |
| IR-9 (§1 step 8) | Practice tools read MIDI when active, do not consume — nothing feeds them (see practice PT-34) | - | - | - | MISSING |
| IR-10 (§1.2) | Macro CC sources and learned mappings update and pass through | `PluginProcessor::feedModulationSources`, `MidiLearnManager::processMidi` | MOD / Learn | `MidiLearn::mapsAndUnmapsCleanly`, `ReviewRegression::midiLearnLearnsAppliesAndSurvivesAClear` | DONE |
| IR-11 (§1.2) | Expression calibration remaps CC to 0-1 before consumers — `ExpressionCalibrationSet::map` never called | `Live/LiveControls.cpp` | Options > EXPRESSION | - | MISSING |
| IR-12 (§1.2) | CCs never reach the string or rhythm engine directly (only via interpreter targets) — untested | `MidiInterpreter::handleController` | n/a | - | NO-TEST |
| IR-13 (§1.3) | PB / AT -> per-note bend / vibrato; pressure mapping target | `MidiInterpreter` pitch-wheel / pressure paths | n/a | `Controllers::pitchDeadZoneRejectsTrackingNoiseButNotRealBends` | DONE |
| IR-14 (§1.4) | PC -> snapshot; CC0 selects preset only when routing option "Bank + PC" — CC0 always selects; no option; PC consumed | `PluginProcessor::handleLiveMidi` | - | `LiveSnapshots::programChangeMapsAcrossAllOneTwentyEight` | PARTIAL |
| IR-15 (§1.5) | Incoming Luthier SysEx dispatched to engines (character, workshop, ranges); others ignored — no inbound dispatch | `Export/LuthierMidiEvents.cpp:decodeSysEx` (file import only) | n/a | `MidiExport::liveSysExIsDroppedByOtherHostsAndReadByLuthier` (decode only) | MISSING |
| IR-16 (§1.6) | MIDI clock / Start / Stop / Continue / SPP -> tap tempo and rhythm transport | - | - | - | MISSING |
| IR-17 (§2, §2.1) | Mouse z-order: overlays consume, scrim click dismisses; popovers eat the outside click — untested | `UI/Overlays.cpp:OverlayHost::mouseDown` | overlays | `Editor::everyOverlayShortcutOpensItsOwnOverlayAndEscapeClosesIt` (Escape only) | NO-TEST |
| IR-18 (§2.2) | Drag consumers: part cards -> illustration, pedals in rack, snapshots in strip, presets -> setlist, mod source -> control — pedal drag only; snapshot/preset drags absent; mod-source drag on visual | `PedalRack.cpp:PedalSlotComponent::mouseDrag` | FX rack | - | PARTIAL |
| IR-19 (§4) | Root file-drop by extension (preset, guitar, tune, part, set, loop, content, midprofile, mid, wav/aiff/flac, mp3) + unknown-type banner + batch — only `.mid/.midi` (editor) and IR slot; visual's `FileOpenRouter` routes 6 extensions for standalone open, not drops | `PluginEditor::isInterestedInFileDrag/filesDropped` | whole window (MIDI only) | `MidiImport` editor drop check (MidiImportTests.cpp:134) | PARTIAL |
| IR-20 (§3, §3.1) | Focused text field consumes keys; Escape passes; global shortcuts inactive — JUCE focus order; untested | `PluginEditor::keyPressed` (reached only if unhandled) | n/a | - | NO-TEST |
| IR-21 (§3) | Global shortcut table with rebinding | `AccessibilitySettings` | Options > ACCESSIBILITY Rebind | `Accessibility::shortcutsRebindAndRefuseClashes`, `Accessibility::shortcutDefaultsMatchTheCanonicalTable` | DONE |
| IR-22 (§3.2) | Rebind conflict detected at rebind time (inline refusal instead of modal) | `AccessibilitySettings::rebind`, `OptionsPages.cpp:1059` | Options | `Accessibility::shortcutsRebindAndRefuseClashes` | DONE |
| IR-23 (§3.3) | IME honoured; no shortcuts during composition — JUCE TextEditor; untested | JUCE `TextEditor` | text fields | - | NO-TEST |
| IR-24 (§5) | Host transport: rhythm start/stop/reposition, tune sync, tap defers, metronome grid, recorder regardless — metronome not transport-synced | `processSlice` playhead read, `RhythmTransport` | n/a | `RhythmPatterns::silentWhenStoppedUnlessFreeRunning`, `LiveTapTempo::respectsRangeSnapAndHostPriority` | PARTIAL |
| IR-25 (§6) | Sidechain consumers: followers, sidechain compressor, sidechain-to-amp, EQ/cab match — no sidechain compressor pedal | `ModEnvelopeFollower`, `engine.setSidechainToAmp`, ToneMatch capture | ROUTING, MOD | `Routing::sidechainToAmpReplacesTheInstrument` | PARTIAL |
| IR-26 (§6) | Sidechain never reaches main path unless consumed — untested | `processSlice` sidechainCopy | n/a | - | NO-TEST |
| IR-27 (§7) | Standalone audio input: sidechain, sung melody, trainer input — hum capture on tune-help; no trainer input | `humCapture` (tune-help) | - | - | PARTIAL |
| IR-28 (§8) | Options > Diagnostics "Inject fixture MIDI / audio" at chain front | - | - | - | MISSING |
| IR-29 (§9) | `Tests/InputRouting/` suite for every consumer / veto rule | - | n/a | - | MISSING |
| IR-30 (§9) | 60 s scripted integration session | - | n/a | - | MISSING |

<!-- counts DONE=9 NO-GUI=0 NO-TEST=7 PARTIAL=11 MISSING=9 OWNED=0 -->
