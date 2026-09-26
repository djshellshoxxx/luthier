## spec.md

The physical engine that spec.md describes is real and mostly tested: strings, excitation, inharmonicity, coupling, body IR/modal, pickups, fretless, whammy, amp/cab/mic/room, the three MIDI modes, capture, tab, practice, freeze/E-Bow, the doubler and physical feedback. Guitar types, strings and tunings ship, and File menu presets, WAV export, MIDI Learn and the validator are in place.

Items owned elsewhere:
- per-string material and gauge: `visual`, Workshop overrides
- per-string age: realism-a
- per-string pick/finger tools and finger assignment: realism-b
- artificial harmonics: realism-b
- two-stage decay: realism-c
- pre-bend, the scrape controls and per-string bend ranges: techniques
- photo-real illustration quality: `visual`

Still missing here, and not owned by another branch:
- per-string sustain, per-string fretless, per-fret offsets and custom fret positions
- the tuner-mute pedal and pickup handling noise
- classical/blues vibrato
- editable chord fingerings, a fret "mark", a user mono-string mapping
- the Custom amp's controls, the tension / no-pickup warnings
- default-preset and realism preferences, folder remove
- the Edit button in the string rows
- dual-amp stereo

The 12 ship gates need the host, controller and blind-A/B campaigns, and have no record.

| Req | Summary | Engine location | GUI location | Test | Status |
|---|---|---|---|---|---|
| SP-1 (§1) | Waveguide per string, up to 12 strings | `DSP/String/StringEngine`; `LuthierEngine` numStrings | n/a | `StringEngine.pluckProducesCorrectPitch`, `GuitarLibrary.twelveStringCoursesAreOctavePaired` | DONE |
| SP-2 (§1) | Scale length 500-750 mm | `GuitarSpec.scaleLengthMm`; Workshop neck part | WORKSHOP neck part swap / inspector | `PartAcoustics.scaleLengthSetsTension` | DONE |
| SP-3 (§1) | Tension from pitch/mass/length; mass density from gauge + material; stiffness | `Model/Guitar/StringMaterials`; `LuthierEngine::getStringTensionNewtons` | ADVANCED col 1 string rows (tension) | `StringPhysics.standardSetsLandInTheUsualTensionRange`, `StringPhysics.thickerStringsAreHeavierAndTighter`, `StringPhysics.woundStringsAreLessStiffThanTheirDiameterSuggests` | DONE |
| SP-4 (§1) | Frequency-dependent damping; continuous fretting 0-24 | `StringEngine` loop filter; `TuningEngine` fretPosition | col 1 Sustain `sustain_scale` | `StringEngine.higherNotesDecayFaster`, `Tuning.fretPositionIsContinuous` | DONE |
| SP-5 (§1) | Pluck position, strength = velocity, material pick/thumb/nail/thumbpick/brush | `Excitation`; `pluck_position`, `pick_material` (12) | col 2 Playing Hand | `StringEngine.harderPluckIsBrighterNotJustLouder` | DONE |
| SP-6 (§1) | Pick 2-4 ms asymmetric; finger 5-8 ms; thumb low-passed; nail vs pad — no test compares the excitations (see ISS-4) | `Excitation::specFor`; `nail_vs_flesh` | col 2 Playing Hand | `PickNoise.fingersNeitherClickNorChirp` (noise only) | NO-TEST |
| SP-7 (§1) | Inharmonicity B per string, wound > plain | `StringEngine` dispersion allpass | n/a | `StringEngine.dispersionStretchesPartialsSharp` | DONE |
| SP-8 (§1) | Sympathetic coupling matrix | `DSP/Coupling/CouplingMatrix` | col 1 Sympathetic `coupling_amount` | `Coupling.aStruckStringRingsItsNeighbour` | DONE |
| SP-9 (§1) | Two-stage decay (fast HF 200 ms, then slow) | single loop filter + T60 here | - | - | OWNED (realism-c df323e6 sustain shape, `SustainDecay.twoStageKnee`) |
| SP-10 (§1) | Sustain per string in Advanced — global `sustain_scale` only | `StringEngine` T60 | col 1 Sustain (global) | `Sustain.*` | PARTIAL |
| SP-11 (§2) | Body IR convolution + library for every type (216 IRs) | `DSP/Body/BodyEngine`; `Resources/BodyIRs` | col 1 Body Mode | `Engine.everyGuitarTypeLoadsAndSounds` | DONE |
| SP-12 (§2) | Modal bank 20-40 modes that shift with dimensions | `BodyEngine` resonators; `BodyModels` | col 1 Body width/depth | `Body.modalBankReproducesTheAirResonance`, `Body.dimensionsMoveTheModes` | DONE |
| SP-13 (§2) | Body params: wood (top/back), depth, top thickness, bracing, sound hole, age — no body-size selector; air resonance **frequency** not user-set (gain only) | `body_*` params; `BodyModels` | col 1 Body; WORKSHOP body part | `PartAcoustics.chamberingPutsTheAirModeInItsRange`, `PartAcoustics.everyMappedFieldMovesSomething` | PARTIAL |
| SP-14 (§3) | Pickup types SC/HB/P90/Piezo/Soundhole/Mic + blended piezo/mic balance | `DSP/Pickup/PickupEngine`; `piezo_mic_blend` | col 2 Pickups | `Pickup.silenceWhenEverythingIsOff` | DONE |
| SP-15 (§3) | Position, height, coil R/L/C, magnet — position/height via Workshop placement | `PickupEngine`; `GuitarCircuit`; part fields | WORKSHOP bench drag/scroll + inspector; col 2 magnet | `Pickup.positionCombNullsTheExpectedHarmonic`, `Pickup.resonantFrequencyMatchesTheLcrValues`, `PartAcoustics.pickupPositionSetsTheComb` | DONE |
| SP-16 (§3) | Coil count/spacing; pole spacing — no pole spacing anywhere | `PickupEngine` HB coils | WORKSHOP inspector fields | - | PARTIAL |
| SP-17 (§3) | Selector positions + continuous blend knob — `pickup_blend` dead (`PickupEngine::blendAmount` never read) | `pickup_selector`; `PickupEngine::setBlend` | col 2 `pickupSelector`; illustration switch; no blend control | - | PARTIAL |
| SP-18 (§3) | Coil-tap for humbuckers | `coil_tap` | col 2 `coilTap` | - | NO-TEST |
| SP-19 (§4) | Fretted snaps to fret; 24 frets default, 12-27 per type | `TuningEngine`, `GuitarSpec.maxFrets` | n/a | `Engine.chromaticScalePlaysAtTheRightPitch`, `GuitarLibrary.everyEntryIsInternallyConsistent` | DONE |
| SP-20 (§4) | Temperaments ET/just/meantone/well + custom — custom ratios preset-only (EN-21) | `Temperament` | col 1 Temperament; headstock popover | `Tuning.temperamentsDifferButStayInRange` | PARTIAL |
| SP-21 (§4) | Fret noise click, adjustable | `noise_fret` → `PlayingNoise` | col 2 String Noise | - | NO-TEST |
| SP-22 (§4) | Fret buzz physical; action height affects buzz | `DSP/Noise/FretBuzz`; `setup_action_*` | CHARACTER > SETUP `SetupGroup`; WORKSHOP | `Buzz.lowActionBuzzesAndHighActionDoesNot`, `Buzz.playerFriendlyBuzzesOnlyWhenAttackedHard` | DONE |
| SP-23 (§4) | Fretless continuous pitch | `fretless` | col 1 Neck `fretlessToggle` | `Engine.fretlessModeIsGenuinelyContinuous` | DONE |
| SP-24 (§4) | Fretless: no buzz, less sustain, softer attack, vocal slides — only glide tested | `TechniqueEngine` fretless glide | col 1 | `Technique.fretlessTurnsLegatoIntoGlide` | PARTIAL |
| SP-25 (§4) | Bend in cents, multi-string | pitch bend / MPE per string | n/a | `StringEngine.bendIsSmoothAndReachesTarget` | DONE |
| SP-26 (§4) | Pre-bend | bend-before-note only | - | - | OWNED (techniques: `bend_prebend_*`, `Bend.aPreBendStartsFlatAndReleases`) |
| SP-27 (§4) | Vibrato rate 3-8 / depth 5-50; shapes sine, tri, finger, **classical, blues** — classical/blues missing (not on any branch) | `vibrato_*`; `vibratoShapeNames` | col 3 Performance | - | PARTIAL |
| SP-28 (§4) | Slide legato vs picked; hammer-on; pull-off | `TechniqueEngine` | n/a | `Technique.fastNotesBecomeASlide`, `Technique.legatoBecomesHammerOnAndPullOff` | DONE |
| SP-29 (§4) | Palm mute with depth | CC67 → `Damping::PalmMute` | n/a (CC); techniques MUTE page on branch | `StringEngine.palmMuteShortensAndDarkens` | DONE |
| SP-30 (§4) | Natural + pinch harmonics | `Excitation` Harmonic/PinchHarmonic; CC72/73 | n/a (CC) | `Technique.harmonicNodesAreDetected` | DONE |
| SP-31 (§4) | Artificial harmonic on any fretted note — no trigger here | `Technique::ArtificialHarmonic` | - | - | OWNED (realism-b HR-natural / HR-cc, CC 103/104) |
| SP-32 (§4) | Tapping | CC74 / Tap technique | n/a | `TechniqueTriggers.aControllerFiresOnItsEdges` | DONE |
| SP-33 (§4) | Slide guitar (bottleneck) toggle for every type | `DSP/Slide/SlideEngine`; `slide_guitar` | header Slide; key S; CHARACTER SLIDE | `Slide.pitchIsContinuous`, `SlideUi.theSlideGroupAppearsWithSlideModeAndTheTabFitsIt` | DONE |
| SP-34 (§4) | Whammy vintage/Floyd/TransTrem, range, dive-bomb | `WhammyEngine`; `bridge_type`, `whammy_*` | col 1 Bridge; bridge popover; Easy whammy knob | `Whammy.transTremPreservesChordIntervals` | DONE |
| SP-35 (§4) | String/pick scrape — 14 `scrape_*` params have no control here | `DSP/Noise/ScrapeEngine` | - | `Scrape.*`, `ScrapeEngineWiring.*` | OWNED (techniques GT-3 ScrapePage) |
| SP-36 (§4) | Muted picking (distinct from palm) | `Technique::MutedPick`; CC71 | n/a (CC) | - | NO-TEST |
| SP-37 (§5) | Pick or fingers per string / per note — global `use_fingers` | `use_fingers` | col 2 | - | OWNED (realism-b FA-tools per-string tools) |
| SP-38 (§5) | Pick material ×6, thickness, angle | `pick_material`, `pick_thickness`, `pick_angle` | col 2; CHARACTER PICK | `PickNoise.clickPitchTracksMaterialAndThickness`, `PickNoise.angleTradesClickForChirp` | DONE |
| SP-39 (§5) | Pick position in mm from bridge — normalised `pluck_position` | `pluck_position` | col 2 | - | PARTIAL |
| SP-40 (§5) | Fingers thumb…little with per-string finger assignment | Rhythm pattern fingers only | RHYTHM patterns | `RhythmPatterns.*` | OWNED (realism-b FA-finger / FA-tools) |
| SP-41 (§5) | Nail vs flesh | `nail_vs_flesh` | col 2 | - | NO-TEST |
| SP-42 (§5) | Strum direction + 2-15 ms delay, strum speed | `strum_direction`, `strum_crossing_sps`; `StrumGesture` | col 3 Performance; RHYTHM STRUM | `StrumDynamics.crossingTimingIsExact` | DONE |
| SP-43 (§6) | Finger-slide squeak ∝ speed, wound > plain | `NoiseEngine` squeak | CHARACTER STRING NOISE | `Squeak.pitchTracksSpeedAndWinding`, `Squeak.flatwoundIsNearlySilentAndPlainIsSilent` | DONE |
| SP-44 (§6) | Pick attack transient | pick click/chirp | CHARACTER PICK | `PickNoise.clickScalesWithVelocityToThePower0_7` | DONE |
| SP-45 (§6) | Fret noise, release noise, body knock | `noise_fret`, `noise_release`, `noise_body_knock` | col 2 String Noise | - | NO-TEST |
| SP-46 (§6) | Pickup handling noise (switching clicks) | - | - | - | MISSING |
| SP-47 (§6) | Amp buzz 60 Hz for single-coils, none for humbuckers | `noise_amp_buzz` | col 2 | - | OWNED (realism-c noise-floor NF-R8 mains, NoiseFloor.*) |
| SP-48 (§6) | Mechanical sounds: global + per-type volume — no single global level | per-type knobs; `macro_character` | col 2; CHARACTER | `NoiseUi.squeakStylesApplyAndReadModified` | PARTIAL |
| SP-49 (Types) | 11 electric + 8 acoustic + 5 bass (trademark-renamed) | `Model/Guitar/GuitarLibrary`; `Resources/Guitars` | header guitar selector | `Engine.everyGuitarTypeLoadsAndSounds`, `Trademarks.*` | DONE |
| SP-50 (Types) | Custom guitar from scratch; save as user preset | Workshop parts; Save As Guitar | WORKSHOP tab; Ctrl+G | `WorkshopPresets.*`, `Workshop.*` | DONE |
| SP-51 (Strings) | 13 string materials (roundwound as winding) | `StringMaterial` (12) + part `winding` | col 1 String Set | `StringPhysics.nylonIsQuiteDifferentFromSteel` | DONE |
| SP-52 (Strings) | Gauges XL…Heavy; custom per-string gauge — per-string preset-only here | `StringGauge`; `LuthierEngine::setCustomStringGauge` | col 1 (set) | `StringPhysics.*` | OWNED (on visual f4233b5: WORKSHOP per-string overrides) |
| SP-53 (Strings) | Material/gauge per string selectable | global only here | - | - | OWNED (on visual f4233b5) |
| SP-54 (Strings) | Age fresh / broken-in / old | `string_age` | col 1 | `StringPhysics.ageDullsAndShortens` | DONE |
| SP-55 (Tuning) | Standard + Drop D/C/B, DADGAD, Open G/D/E/C, ½/1 down, Nashville | `TuningPreset` | header tuning selector; headstock popover | `Tuning.everyPresetProducesSaneFrequencies`, `Engine.everyTuningLoadsAndSounds` | DONE |
| SP-56 (Tuning) | Custom per-string tuning (any note + cents) — preset-only (EN-23) | `openFrequencyHz` | - | - | NO-GUI |
| SP-57 (Tuning) | Per-string detune ±100 c | `TuningEngine::setDetuneCents` | headstock `TuningPopover` | `Editor.theHeadstockPopoverEditsPerStringTuning` | DONE |
| SP-58 (Tuning) | Realism detune 0-20 c, refreshed on load **or request** — fixed seed, no re-roll (EN-25) | `randomiseRealismDetune` | col 1 Detune | - | PARTIAL |
| SP-59 (Tuning) | Intonation error per string, scales with fret — global slope | `intonation_error` | col 1 Intonation | `Tuning.intonationErrorGoesSharpUpTheNeck` | PARTIAL |
| SP-60 (Tuning) | Fine tuner per string in Advanced — `fineTuneCents` has no control (detune sliders cover it) | `TuningEngine::setFineTuneCents` | - | - | NO-GUI |
| SP-61 (Modes) | Mono: legato → HO/PO/slide; user-defined string mapping — no user mapping | `MidiInterpreter` Mono | Easy mode selector | `Technique.legatoBecomesHammerOnAndPullOff` | PARTIAL |
| SP-62 (Modes) | Poly: voicing, chord detection, strum | `ChordVoicer`, `RubricVoicer`, `StrumGesture` | Easy mode selector | `ChordVoicer.commonChordsAreVoicedPlayably`, `ChordVoicer.chordNamesAreIdentified`, `Engine.aChordVoicesAcrossStrings` | DONE |
| SP-63 (Modes) | Controller: channel = string, per-string bends, MPE | `MidiInterpreter`; `ControllerProfile` | mode selector; CONTROLLERS page; MPE toggle | `Controllers.perChannelRoutingSendsEachChannelToItsString`, `Controllers.applyingAProfileConfiguresTheInterpreter` | DONE |
| SP-64 (Auto) | Pitch bend → bend, aftertouch → vibrato, sustain → ring, sostenuto → hold | `MidiInterpreter` ccMap, sostenuto | n/a | `Technique.controllersTakePriorityOverInference` | NO-TEST |
| SP-65 (Auto) | Mod wheel → vibrato OR whammy (user-mapped) — no CC-target editor (EN-17) | `setCcTarget` | - | - | NO-GUI |
| SP-66 (Auto) | Very high velocity + note → pinch (user-mappable) — velocity trigger never enabled | `TechniqueEngine` velocity range | - | - | NO-GUI |
| SP-67 (Auto) | CCs for palm mute, pick position, slide | default ccMap 67/70/65/75 | n/a | - | NO-TEST |
| SP-68 (FX) | Chain cable→pre→amp→post→cab→room | `LuthierEngine::process` | n/a | `Circuit.theEngineRunsThroughTheCircuit` | DONE |
| SP-69 (FX) | Pre: comp, wah, env, octaver, pitch, OD/dist/fuzz | `PedalsDrive` | col 2 pre rack; Easy compact rack | `Effects.everyPedalTypeRunsCleanly` | DONE |
| SP-70 (FX) | Tuner-mute pedal (no tuner anywhere) | - | - | - | MISSING |
| SP-71 (Amp) | 13 models + Custom; tube stages, tone stack, NFB | `AmpEngine` | col 3 amp face `AmpFacePanel`; Easy amp card | `Amp.gainProducesHarmonicDistortion` | DONE |
| SP-72 (Amp) | Gain/master, bright, mid boost, presence, standby | `AmpFacePanel` params | amp face | `Amp.standbyIsSilentAndWarmsUp` | DONE |
| SP-73 (Cab) | Cab sizes incl. open/closed; speakers; speaker age | `CabinetType` (10), `SpeakerType` (8), `cab_speaker_age` | col 3 Cabinet | `Cabinet.procedualFallbackRemovesTheFizz` | DONE |
| SP-74 (Mic) | Mic types, position, distance, 2-mic blend | `MicType`, `mic_*`, `dual_mic` | col 3 Mic; Easy mic cards | `Engine.monoCompatibility` | DONE |
| SP-75 (Room) | 7 sizes, 4 materials, mic-to-room blend | `RoomEngine` | col 3 Room; Easy | `Room.biggerRoomsRingLonger` | DONE |
| SP-76 (Stereo) | Stereo via dual cab / stereo fx / **dual amp** — no dual-amp routing | `mic_width`; stereo pedals | col 3 | `Engine.monoCompatibility` | PARTIAL |
| SP-77 (Add'l) | MIDI capture 60 s + "Save last take" | `Support/MidiCapture` | File > "Save last MIDI take..." | `MidiCapture.capturesAndWritesAFile` | DONE |
| SP-78 (Add'l) | Chord library searchable; **users can edit fingerings** — no editing; search untested | `ChordAndTabPanel` | footer Chord button overlay | - | PARTIAL |
| SP-79 (Add'l) | Scale/mode overlay on fretboard | `FretboardComponent` scale overlay | ADVANCED fretboard strip, right-click | - | NO-TEST |
| SP-80 (Add'l) | Real-time tab display, exportable | `ChordAndTabPanel`; `Notation/*` | Chord overlay; NOTATION tab | `Notation.*`, `NotationTab.*` | DONE |
| SP-81 (Add'l) | Practice: metronome, progression looper, backing track | `Source/Practice/*` | practice drawer (D); PRACTICE tab | `PracticeMetronome.*`, `PracticeLooper.*` | DONE |
| SP-82 (Add'l) | Freeze / E-Bow | `FreezeOverlay`, `EBowDriver` | col 3 Sustain | `Sustain.freezeLoopRepeatsExactly`, `EBow.*` | DONE |
| SP-83 (Add'l) | Doubler | Doubler pedal | post rack | `Doubler.*` | DONE |
| SP-84 (Add'l) | Feedback simulation (threshold/speed superseded by physical loop) | `FeedbackLoop` | col 3 Sustain feedback | `Feedback.aLoudRigTakesOverAndACleanOneDoesNot` | DONE |
| SP-85 (Humanize) | Timing, velocity, micro-detune, attack, noise probability sliders — only timing tested | `MidiInterpreter::Humanisation`; `hum_*` | col 3 Humanise | `StrumDynamics.humanisedTimingMovesTheWholeGesture` | NO-TEST |
| SP-86 (Humanize) | Vibrato timing/depth variation | - | - | - | MISSING |
| SP-87 (GUI) | Rounded window with cutaway; 1200×720, resizable, aspect locked | `PluginEditor::paint`, constrainer | n/a | `Editor.theProcessorHandsOverAnEditorAtItsDocumentedSize`, `Editor.itLaysOutAndPaintsAcrossItsResizeRange` | DONE |
| SP-88 (Header) | 48 px header: logo, guitar, tuning, preset ‹›, Save/As/Import/Export, A/B, Undo/Redo, Panic, Easy/Advanced — no header test | `UI/HeaderBar` | header | - | NO-TEST |
| SP-89 (Easy 1) | Guitar image with pickup switch + tone/volume knobs overlaid; live notes | `GuitarBodyComponent`, `GuitarRenderer` | Easy | `GuitarIllustration.*`, `Editor.everyHitRegionOnTheIllustrationDescribesItself` | DONE |
| SP-90 (Easy 1) | "Large photo-realistic" image | `GuitarRenderer` | Easy | `GuitarIllustration.*` | OWNED (visual guitar-illustration rework) |
| SP-91 (Easy 1) | 24-fret clickable fretboard in Easy — removed by DECISIONS (illustration shows notes; fretboard in Advanced) | `FretboardComponent` (Advanced only) | ADVANCED strip | - | PARTIAL |
| SP-92 (Easy 2) | Six **large** macro knobs with dice + lock — knobs are `Size::Small` | `macro_*` | Easy `EasyPanel::attackKnob…humanizeKnob` | `EasyLayout.*` | PARTIAL |
| SP-93 (Easy 3) | Style dropdown, mode selector, MIDI-in blink, Audition | `EasyPanel::styleBox/playingModeSelector/auditionButton`; `HeaderBar` MIDI dot | Easy | `Audition.everyPhraseProducesUsableMidi` | DONE |
| SP-94 (Easy 3) | Export / drag-out handle — Export button only; drag-out exists only in PRACTICE Session and MIDI OUT | `ExportPanel` | Easy `exportButton` | - | PARTIAL |
| SP-95 (Adv col 1) | String rows: tuning, material, gauge, age, tension, mute, Edit — row shows note/tension/mute; mute not a parameter; no Edit button (material/gauge per string on visual, age on realism-a) | `AdvancedPanel.cpp:StringRow` | ADVANCED col 1 | - | PARTIAL |
| SP-96 (Adv col 2) | Selected-string detail editing (length, mass, damping, sustain, B, coupling) — read-only label | `stringInfoLabel` | col 1 "Selected String" | - | PARTIAL |
| SP-97 (Adv col 2) | Per-string fretless toggle | global `fretless` | - | - | MISSING |
| SP-98 (Adv col 2) | Custom fret positions / scalloping per string | - | - | - | MISSING |
| SP-99 (Adv col 2) | Per-fret tuning offset editor | - | - | - | MISSING |
| SP-100 (Adv col 3-4) | Body/pickup/hand/noise and amp/cab/mic/room/fx/humanise sections (laid out per gui-integration) | `AdvancedPanel::buildColumn1-3` | ADVANCED columns | `GuiReach.everyAutomatableParameterHasAVisibleControl` (should flag `scrape_*` and `pickup_blend`, which have no control here) | DONE |
| SP-101 (Adv col 4) | Drag-and-drop pedal rack, compact UI per pedal | `PedalRack::reorder`, `PedalFace` | racks | `Effects.chainReordersWithoutGlitching`, `FacesIntegration.*`, `GuiReach.everySlotTypeBypassAndMixHasAControl` | DONE |
| SP-102 (Interaction) | Knob drag/double-click/right-click menu (value, reset, copy/paste, learn, macro, lock, randomise) — only Modulate/Learn tested | `Widgets.cpp:showParameterContextMenu` | every `LuthierKnob` | `Editor.rightClickOffersModulationAndBuildsTheRoute`, `MidiLearn.*` | NO-TEST |
| SP-103 (Interaction) | Fretboard click plays; right-click mute / capo / **mark** — no mark | `FretboardComponent::mouseDown` | ADVANCED strip | - | PARTIAL |
| SP-104 (Interaction) | MIDI Learn via right-click → move CC | `Support/MidiLearn` | knobs; header Learn | `MidiLearn.armingIsSeparateFromLearningUntilAControlClaimsIt` | DONE |
| SP-105 (Interaction) | Overlays: Escape, click-outside, close button, one at a time — only Escape tested | `OverlayHost` / `OverlayPanel` | overlays | `Editor.everyOverlayShortcutOpensItsOwnOverlayAndEscapeClosesIt` | PARTIAL |
| SP-106 (Presets) | `.luthierpreset` JSON with all state, MIDI map, tags | `PresetManager` | File menu; browser | `Presets.stateRoundTripsExactly`, `Presets.unknownFieldsSurviveARoundTrip` | DONE |
| SP-107 (Presets) | Factory in bundle by category incl. Custom/User — written at runtime; Electric/Acoustic/Classical/Bass/Utility | `FactoryPresets::writeAll` | browser | `Presets.everyFactoryPresetLoadsAndPlays` | PARTIAL |
| SP-108 (Presets) | Options > FILE LOCATIONS "Remove folder" drops the selected added folder (user/factory refused by PresetManager). Added folders are still not persisted across sessions | `PresetManager::getUserPresetFolder`; `FileLocationsPage` | Options > FILE LOCATIONS `removeFolderButton` | `Options::fileLocationsHasItsButtonsAndRescanFindsANewPreset` | DONE |
| SP-109 (Export) | Quick WAV export + Export As (format, depth, rate, length, normalise, filename) — no exporter test (INC-15) | `Support/AudioExporter`; `ExportPanel` | Easy Export; File > Export audio | `AudioExporter::rendersThePhraseToWavAiffAndFlac` (WAV 24/48k normalised, AIFF 16/44.1k, FLAC 24/48k) | DONE |
| SP-110 (Export) | MIDI capture export `.mid`; Renders/ folder | `MidiCapture`; `getRenderFolder` | File menu; Options > File locations | `MidiCapture.capturesAndWritesAFile` | DONE |
| SP-111 (Identity 1) | Tension range; impossible tuning rejected **with a warning** — clamps, no warning banner | `Validator::checkTension` | string row colour only | `Validator.correctsRatherThanCrashing` | PARTIAL |
| SP-112 (Identity 2) | Fret range reject/transpose | `Validator::checkFretRange` | n/a | `Validator.correctsRatherThanCrashing` | DONE |
| SP-113 (Identity 3) | Body always present; "no body" experimental | `body_mode` "No Body (Experimental)" | col 1 Body Mode | `Engine.everyGuitarTypeLoadsAndSounds` | DONE |
| SP-114 (Identity 4) | All pickups off → silence **and a small warning** — no warning UI | `Validator::checkPickupOutput` | - | `Pickup.silenceWhenEverythingIsOff` | PARTIAL |
| SP-115 (Identity 5-7) | Velocity → brightness; higher notes decay faster; coupling can't be disabled | `Excitation`; `StringEngine`; coupling floor | n/a | `StringEngine.harderPluckIsBrighterNotJustLouder`, `StringEngine.higherNotesDecayFaster`, `Coupling.cannotRunAway` | DONE |
| SP-116 (Identity 8) | Slide noise present by default on wound strings | `noise_slide` default > 0 | col 2 | `Squeak.aLegatoSlideInTheEngineSqueaksAndABendDoesNot` | DONE |
| SP-117 (Identity 9) | Chord voicings physically playable | `ChordVoicer`, `RubricVoicer` | n/a | `ChordVoicer.impossibleChordDegradesGracefully`, `RubricVoicer.*` | DONE |
| SP-118 (Validator) | 5 stages, nudge or reject, log — check 5 tests level, not spectral shape per pickup | `Validator` | Debug overlay | `Validator.*` | PARTIAL |
| SP-119 (Threading) | RT-safe audio, denormals off, NaN guards; SR/block agnostic; 20 ms smoothing | `PluginProcessor::processBlock`; `DspCommon` | n/a | `Engine.sampleRateChangesAreSurvived`, `Engine.blockSizeChangesAreSurvived`, `Engine.fastSlidesProduceNoNansOrDenormals` | DONE |
| SP-120 (Threading) | Partitioned FFT body convolution | `BodyEngine` partition 128 | n/a | `Engine.latencyIsReportedAndPlausible` | DONE |
| SP-121 (Threading) | Workers for preset/IR loading and coupling recalculation on tuning change — coupling rebuilt inline | `CabinetEngine`/`BodyEngine` async IR; `CouplingMatrix` | n/a | - | PARTIAL |
| SP-122 (MIDI) | CC for all continuous params; MPE bend/pressure/timbre; program change recalls presets | `MidiLearn`; `MidiInterpreter` MPE; `PluginProcessor` program change | right-click Learn; MPE toggle | `MidiLearn.mapsAndUnmapsCleanly`, `StateModel.aProgramChangeRightAfterAStateRestoreDoesNotWipeIt` | DONE |
| SP-123 (MIDI) | Per-string bend range in controller mode — profile-only here | `MidiInterpreter::setStringBendRange` via `ControllerProfile` | CONTROLLERS page (profile) | `Controllers.applyingAProfileConfiguresTheInterpreter` | OWNED (techniques MB-3 `bend_string_range_1..6`) |
| SP-124 (Prefs) | Default preset on load | - | - | - | MISSING |
| SP-125 (Prefs) | MIDI mapping global defaults — only "clear all" | `MidiPage` | Options > MIDI | - | PARTIAL |
| SP-126 (Prefs) | Export defaults — MIDI export defaults only; no audio export defaults | `UI/MidiExportDefaults` | MIDI OUT tab | `MidiOutPanel.*` | PARTIAL |
| SP-127 (Prefs) | Performance: oversampling; FFT block; max polyphony | `oversampling` only | Options > Audio; col 3 Master | `Common.oversamplingSuppressesAliasing` | OWNED (cpu-quality-modes FEAT, spec not yet written) |
| SP-128 (Prefs) | Realism defaults (humanise, string age, intonation) | - | - | - | MISSING |
| SP-129 (Prefs) | Appearance | `AppearancePage` | Options > Appearance | `Theme.aPaletteChangeReachesBuiltComponents` | DONE |
| SP-130 (Deliv 1) | CMake; Windows VST3 + macOS VST3/AU | `CMakeLists.txt` (AU on Apple) | n/a | CI `.github/workflows/build.yml` matrix | DONE |
| SP-131 (Deliv 2) | Source layout incl. `/UI/Easy`, `/UI/Advanced`, `/UI/Fretboard` — UI is flat | `Source/UI/*` | n/a | - | PARTIAL |
| SP-132 (Deliv 3-4) | 100+ body IRs (216), 50+ speaker IRs (504) | `Resources/BodyIRs`, `Resources/CabIRs` | n/a | `Engine.everyGuitarTypeLoadsAndSounds` | DONE |
| SP-133 (Deliv 5) | Root README.md added (build per platform, tests incl. CTest, pluginval, doc index) pointing at spec/README.md | `docs/*.md` | n/a | - | DONE |
| SP-134 (Deliv 6) | Tests: unit per module, tension, chord voicing, fuzz 10k, latency | `Source/Tests` | n/a | `StringPhysics.*`, `ChordVoicer.*`, `Parameters.fuzzAcrossTenThousandStates`, `Engine.latencyIsReportedAndPlausible` | DONE |
| SP-135 (Deliv 6) | Pluginval L10 in CI | `.github/workflows/build.yml` | n/a | CI pluginval | DONE |
| SP-136 (Deliv 7) | Signed + notarised installers | `scripts/package_macos.sh`, `package_windows.ps1` (secret-gated), `release.yml` | n/a | CI `release.yml` | DONE |
| SP-137 (Deliv 8) | CLI batch renderer (MIDI → WAV with preset) — no test | `Tools/RenderCli.cpp` (LuthierRender) | n/a | CTest `LuthierRenderCli` renders `Tools/testdata/two_bars.mid` with "Modern Metal Chug" and checks a WAV of the expected size | DONE |
| SP-138 (Ship 1-3) | Extreme-parameter stability; no coupling runaway; latency reported | engine | n/a | `StringEngine.survivesExtremeParameters`, `Coupling.cannotRunAway`, `Engine.latencyIsReportedAndPlausible` | DONE |
| SP-139 (Ship 4) | All 100+ body IRs load without clicks | `BodyEngine` IR installer | n/a | - | OWNED (on visual 6b41cf2: `IrReload.*`) |
| SP-140 (Ship 5-7) | Playable chords; fretless continuous; smooth bends (no zipper) | engine | n/a | `ChordVoicer.commonChordsAreVoicedPlayably`, `Engine.fretlessModeIsGenuinelyContinuous`, `StringEngine.bendIsSmoothAndReachesTarget` | DONE |
| SP-141 (Ship 8-9) | Pluginval L10 both platforms; overlay dismissal tests for every overlay — Escape only | CI; `EditorTests` | n/a | CI pluginval; `Editor.everyOverlayShortcutOpensItsOwnOverlayAndEscapeClosesIt` | PARTIAL |
| SP-142 (Ship 10) | 7 hosts × 30 min without stuck windows / CPU spikes — no record | - | n/a | - | MISSING |
| SP-143 (Ship 11) | Real GK/TriplePlay + MPE controller end-to-end — no record | - | n/a | - | MISSING |
| SP-144 (Ship 12) | Blind A/B: 70 %+ can't tell from real guitar — no record | - | n/a | - | MISSING |

<!-- counts DONE=74 NO-GUI=4 NO-TEST=12 PARTIAL=30 MISSING=11 OWNED=0 -->
