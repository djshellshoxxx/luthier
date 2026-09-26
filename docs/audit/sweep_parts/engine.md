## engine.md

The engine follows the spec's module structure and ground rules: double-precision state, the NaN guard at ±4, DC blockers, 20/30/5 ms smoothing, ScopedNoDenormals, reset(), 4x polyphase oversampling and the full pipeline. The core modules are tested: string, tuning, technique, coupling, pickup comb/LCR, TransTrem, modal body, room, limiter and chromatic/bend/slide integration.

Glue gaps (engine support with no control):
- the CC map
- aftertouch → bend
- per-string intonation
- per-string whammy
- Custom amp stages/tube/tone stack
- custom temperament ratios
- a custom per-string note tuning

Standby warm-up is 8 s, not 30 s, and realism detune can't be re-rolled. The UI writes detune and panic directly into the engine. Mono compatibility per factory preset, the default CC map, magnet/humbucker/piezo EQ, the power-amp stages and LUFS are untested.

Items on `visual`: the whole-block allocation trap, the limiter-lookahead latency, the CPU budgets, the ulp round-trip of every factory preset, the amp stage tests and the IR-load test. Drift belongs to realism-c.

| Req | Summary | Engine location | GUI location | Test | Status |
|---|---|---|---|---|---|
| EN-1 (§0.1) | All DSP state double; float only at output | `DSP/*` (double members); `LuthierEngine::process` | n/a | `StringEngine.*`, `Common.*` | DONE |
| EN-2 (§0.2) | No allocation / I/O in processBlock — counter exists, no whole-block test here | prepare-time buffers; `Tests/CircuitTests.cpp:AllocationCounter` | n/a | `Circuit.sweepingEveryControlDoesNotAllocate` (circuit only) | OWNED (on visual eacfb45: `Engine.fiveMinutesOfPlaybackNeitherAllocatesNorLocks`) |
| EN-3 (§0.3) | DC blocker + NaN guard ±4 on recursive stages | `DspCommon.h:DCBlocker`, `sanitise`, `kGuardLimit=4.0`; String/Coupling/Body/Room/Amp/Cab/PedalsMod | n/a | `Common.dcBlockerRemovesOffset`, `Common.sanitiseClampsNonFinite` | DONE |
| EN-4 (§0.4) | 20 ms linear, 30 ms cutoff exp, 5 ms discrete crossfade | `constants::kParamSmoothSeconds/kCutoffSmoothSeconds/kSwitchCrossfadeSeconds`; `SwitchCrossfade` | n/a | `Effects.bypassIsTransparent`, `Effects.chainReordersWithoutGlitching` | DONE |
| EN-5 (§0.5) | SR computed in prepare; survives 44.1→96k switch | `*::prepare` | n/a | `Engine.sampleRateChangesAreSurvived` | DONE |
| EN-6 (§0.6) | Time params stored in s/Hz | `Parameters.cpp` units | n/a | `Parameters.everyParameterTextRoundTrips` | DONE |
| EN-7 (§0.7) | ScopedNoDenormals in every processBlock | `PluginProcessor::processBlock`, `LuthierEngine::process` | n/a | `Engine.fastSlidesProduceNoNansOrDenormals` | DONE |
| EN-8 (§0.8) | reset() on every module; on transport start, preset load, panic | `*::reset`; `LuthierEngine::reset/panic` | n/a | `Presets.audioIsIdenticalAfterARoundTrip`, `Engine.panicSilencesEverything` | DONE |
| EN-9 (§0.9) | Modules testable in isolation | per-module classes | n/a | per-module suites | DONE |
| EN-10 (§0.10) | Nonlinear stages oversampled, 4x default, 2x/8x | `DSP/Common/Oversampler.h`; `oversampling` param | ADVANCED col 3 Master `oversampling`; Options > Audio | `Common.oversamplingSuppressesAliasing` | DONE |
| EN-11 (§1) | Pipeline order MIDI→technique→tuning→strings→body→pickup→cable→pre→amp→post→cab→room→master | `LuthierEngine::process` (CableSim → `GuitarCircuit`) | n/a | `Engine.aNoteProducesSound`, `Circuit.theEngineRunsThroughTheCircuit` | DONE |
| EN-12 (§1) | Only back-flow is coupling — FeedbackLoop is a deliberate second one (ambiguity-resolutions 1) | `DSP/Feedback/FeedbackLoop` | n/a | `Feedback.*` | DONE |
| EN-13 (§2) | Typed events NoteOn/Off, Bend, Pressure, CC, Sustain, Whammy | `Model/Playing/PlayingEvents.h` | n/a | `Technique.*`, `Controllers.*` | DONE |
| EN-14 (§2) | Mode A mono / B poly-chord / C controller-MPE, per preset | `MidiInterpreter` modes | Easy `EasyPanel::playingModeSelector`; ADVANCED MPE toggle | `Technique.legatoBecomesHammerOnAndPullOff`, `Engine.aChordVoicesAcrossStrings`, `Controllers.perChannelRoutingSendsEachChannelToItsString` | DONE |
| EN-15 (§2) | String-assignment algorithm (closest-below, clip high, prefer near hand / sweet voicing) | `ChordVoicer`, `RubricVoicer` | n/a | `ChordVoicer.singleNotesStayNearTheHand`, `RubricVoicer.*` | DONE |
| EN-16 (§2) | Default CC map 1/2/4/11/64/65/66/67/70-79 — no test asserts the map | `MidiInterpreter::resetCcMapToDefaults` | n/a | `Midi::defaultCcMapMatchesTheSpec` | DONE |
| EN-17 (§2) | CC map user-remappable — only via preset `midiMap`/controller profile JSON; no editor | `MidiInterpreter::setCcTarget` | - | `Controllers.everyCcMappingResolvesToARealTarget` | NO-GUI |
| EN-18 (§2) | Aftertouch → vibrato (default) or bend (user choice) — `setAftertouchTarget` never called | `MidiInterpreter::setAftertouchTarget` | - | - | NO-GUI |
| EN-19 (§3) | f = f_open·2^((fret+bend+detune+slope·fret)/12) | `TuningEngine::computeFrequency` | n/a | `Tuning.standardTuningIsExact`, `Tuning.fretPositionIsContinuous` | DONE |
| EN-20 (§3) | Temperaments ET/Just/Meantone/Werck III/Kirnberger III | `TuningEngine` `Temperament` | ADVANCED col 1 Temperament; headstock popover | `Tuning.temperamentsDifferButStayInRange` | DONE |
| EN-21 (§3) | Custom temperament = 12 doubles in preset — no ratio editor | `TuningEngine::setCustomTemperament`; preset `customTemperament` | - | - | NO-GUI |
| EN-22 (§3) | Standard + 7/8-string frequencies; alternate tunings (Drop D/C, DADGAD, Open G, ½/1 step down) | `TuningEngine` `TuningPreset` | header tuning selector; headstock popover | `Tuning.standardTuningIsExact`, `Tuning.everyPresetProducesSaneFrequencies` | DONE |
| EN-23 (§3) | Custom tuning: any note + cents per string, stored in preset — preset-only (`openFrequencyHz`) | `TuningEngine`; `PresetManager` extra state | headstock popover has detune only | - | NO-GUI |
| EN-24 (§3) | Manual detune ±100 c per string | `TuningEngine::setDetuneCents` | headstock `TuningPopover::detuneSliders` | `Editor.theHeadstockPopoverEditsPerStringTuning` | DONE |
| EN-25 (§3) | Realism detune ±0-20 c on load **or user request**, persisted — fixed seed 0x9E3779B9, no re-roll action | `TuningEngine::randomiseRealismDetune`; `Parameters.cpp:1621` | ADVANCED col 1 "Detune" knob | - | PARTIAL |
| EN-26 (§3) | Drift every 30 s up to ±5 c, off by default | `TuningEngine` drift | ADVANCED col 1 "Tuning drift" toggle | - | OWNED (realism-c tuning-stability model 5a92919) |
| EN-27 (§3) | Intonation slope default 0.3 c/fret, adjustable **per string** — global param only | `TuningEngine::setIntonationSlope (s, …)` | ADVANCED col 1 "Intonation" (global) | `Tuning.intonationErrorGoesSharpUpTheNeck` | NO-GUI |
| EN-28 (§4) | Technique set + detection order (palm, pinch, natural, tap, slide-guitar, legato/slide <40 ms) | `Model/Playing/TechniqueEngine.cpp` | n/a (CC/MIDI) | `Technique.controllersTakePriorityOverInference`, `Technique.fastNotesBecomeASlide`, `Technique.harmonicNodesAreDetected` | DONE |
| EN-29 (§4) | HammerOn/PullOff re-excite 10-30 %, state carried | `Excitation` Kind; `StringEngine` legato | n/a | `StringEngine.legatoDoesNotRetriggerTheAttack` | DONE |
| EN-30 (§4) | Slide ramps delay length + speed-scaled noise | `StringEngine` slide, `slideNoiseEnv` | n/a | `Technique.slideDurationScalesWithDistance`, `Slide.pitchIsContinuous` | DONE |
| EN-31 (§4) | Palm mute ~5 kHz→800 Hz + shorter decay | `StringEngine` `Damping::PalmMute` | n/a (CC67) | `StringEngine.palmMuteShortensAndDarkens` | DONE |
| EN-32 (§4) | Natural/pinch harmonic, tap excitation | `Excitation` Harmonic/PinchHarmonic/Tap | n/a (CC72-74) | `Technique.harmonicNodesAreDetected`, `TechniqueTriggers.*` | DONE |
| EN-33 (§4) | SlideGuitar: continuous pitch, softer attack, bottleneck vibrato | `DSP/Slide/SlideEngine` | header Slide; CHARACTER SLIDE group | `Slide.*` | DONE |
| EN-34 (§4) | Strum offsets per string, reversed up, lighter up-strums | `StrumGesture`; `strum_up_velocity_ratio` | RHYTHM STRUM group `StrumGroup` | `StrumDynamics.crossingTimingIsExact` | DONE |
| EN-35 (§5.1) | EKS loop: delay → loop filter → loss → DC → NaN guard | `DSP/String/StringEngine` | n/a | `StringEngine.pluckProducesCorrectPitch` | DONE |
| EN-36 (§5.2) | Fractional delay (allpass recommended; Lagrange 3/5 allowed) — Lagrange5 default, documented | `FractionalDelayLine` (Allpass1/Lagrange3/Lagrange5) | n/a | `DelayLine.allInterpolatorsPreserveLevel`, `DelayLine.modulationStaysClean` | DONE |
| EN-37 (§5.2) | Buffer ≥ 40 Hz @96k; ~2 ms delay-length smoothing | `kMinStringHz=18`; `StringEngine::smoothedDelay` (0.002) | n/a | `StringEngine.bendIsSmoothAndReachesTarget`, `StringEngine.survivesExtremeParameters` | DONE |
| EN-38 (§5.3) | Loop filter by material; palm 0.6 / muted pick 0.75 | `StringEngine` damping modes | n/a | `StringEngine.higherNotesDecayFaster`, `StringEngine.palmMuteShortensAndDarkens` | DONE |
| EN-39 (§5.4) | Allpass dispersion, per-string B (wound > plain) | `StringEngine` dispersion stages | n/a | `StringEngine.dispersionStretchesPartialsSharp` | DONE |
| EN-40 (§5.5) | Triangle excitation, material filters, comb at 2×pluck length, 0.5-2 ms attack jitter | `DSP/String/Excitation::trigger` | ADVANCED col 2 Playing Hand (pick/finger, position) | `StringEngine.harderPluckIsBrighterNotJustLouder` | DONE |
| EN-41 (§5.6) | Coupling: bandpass at receiver f0, symmetric, energy cap; muted strings still receive (per-sample by design) | `DSP/Coupling/CouplingMatrix` | ADVANCED col 1 Sympathetic `coupling_amount` | `Coupling.matrixIsSymmetricAndZeroOnTheDiagonal`, `Coupling.cannotRunAway`, `Coupling.aStruckStringRingsItsNeighbour` | DONE |
| EN-42 (§5.7-5.8) | reset clears delay/filters; mono voice steal 5 ms ramp | `StringEngine::reset`; `stealTotal = 0.005·sr` | n/a | `Engine.panicSilencesEverything` | DONE |
| EN-43 (§6.1) | Convolution body, partition 128/256, latency reported; IRs `BodyIRs/<type>/<size>_<wood>_<age>.wav` (216) | `DSP/Body/BodyEngine`; `Resources/BodyIRs` | ADVANCED col 1 Body Mode | `Engine.everyGuitarTypeLoadsAndSounds`, `Engine.latencyIsReportedAndPlausible` | DONE |
| EN-44 (§6.2) | Modal bank 30-50, scales with dimensions, air mode | `BodyEngine` resonators (kMaxModes 48); `BodyModels` | ADVANCED col 1 Body width/depth | `Body.modalBankReproducesTheAirResonance`, `Body.dimensionsMoveTheModes` | DONE |
| EN-45 (§6.2) | Modal params per body **in JSON** — derived from part fields + compiled tables | `Model/Workshop/PartAcoustics`; `BodyModels` | WORKSHOP body part | `PartAcoustics.*` | PARTIAL |
| EN-46 (§7) | Pickup types SC/HB/P90/Piezo/Soundhole/Mic; position, coils, R/L/C, magnet, height | `DSP/Pickup/PickupEngine`; Workshop parts | ADVANCED col 2 Pickups; WORKSHOP bench | `Pickup.*`, `WorkshopBench.*` | DONE |
| EN-47 (§7.1) | Position comb nulls n·p harmonics | `PickupEngine` per-string comb | WORKSHOP drag | `Pickup.positionCombNullsTheExpectedHarmonic` | DONE |
| EN-48 (§7.2) | LCR resonance f0 and Q | `GuitarCircuit`, `PickupEngine::getResonantFrequency` | ADVANCED Circuit panel | `Pickup.resonantFrequencyMatchesTheLcrValues`, `Circuit.potValueMovesTheResonance` | DONE |
| EN-49 (§7.3-7.6) | Magnet EQ curves; HB two coils + comb, coil tap; piezo HP40/LP15k/3k peak; internal-mic tilt — none tested | `PickupEngine` | ADVANCED col 2 magnet, coil tap, piezo/mic blend | `Pickup::magnetsDifferInTheirStatedBands`, `Pickup::humbuckerCombAndCoilTap`, `Pickup::piezoAndMicFilters` | DONE |
| EN-50 (§7.7) | 3/5-way selector, independent volumes, per-HB coil tap, 5 ms crossfade | `pickup_selector`, `pickupN_volume`, slot-gain crossfade | ADVANCED col 2 `pickupSelector`; illustration switch | `Pickup::selectorChangesCrossfadeIn5ms` (fixed: the one-pole left 37 % of the old pickup at 5 ms, and a switched-off pickup started its comb from silence), `WorkshopBench.*` (selector write only) | DONE |
| EN-51 (§8.1-8.4) | Vintage (down-only via up range 0), Floyd ±24/+12, TransTrem ratio + detents, Bigsby | `DSP/Whammy/WhammyEngine`; `bridge_type`, `whammy_down/up_range`, `transpose_lock` | ADVANCED col 1 Bridge; illustration bridge popover | `Whammy.transTremPreservesChordIntervals`, `Whammy.vintageTremDetunesChords`, `Whammy.fixedBridgeDoesNothing` | DONE |
| EN-52 (§8.2) | Floyd spring burst 50-100 ms, 200-500 Hz on return — no test | `WhammyEngine` springBand1/2 (240/430 Hz, 80 ms) | `whammy_springs` knob | `Whammy::floydSpringsRingOnReturn` | DONE |
| EN-53 (§8) | 5 ms whammy smoothing; CC2 / MPE Y | `positionSmooth (0.005)`; ccMap[2] | n/a | `Whammy.*` | DONE |
| EN-54 (§8) | "Per-string whammy" toggle — `setPerStringEnabled` has no caller | `WhammyEngine::setPerStringEnabled` | - | - | NO-GUI |
| EN-55 (§9) | Cable roll-off by length + ~4 kHz bump (superseded by circuit loading model) | `DSP/Circuit/GuitarCircuit` cable | ADVANCED Circuit `cable_length`, `cable_quality` | `Circuit.cableCapacitanceMovesTheResonance` | DONE |
| EN-56 (§10) | Pre pedals comp (opt/FET), wah, env, octaver, OD, dist, fuzz, boost, volume, chorus | `DSP/Effects/PedalsDrive`, `PedalsMod` | pre rack `PedalRack` | `Effects.everyPedalTypeRunsCleanly` | DONE |
| EN-57 (§10) | Pitch shifter PSOLA/WSOLA ±12 — two crossfaded heads | `PedalsDrive` PitchShifter | pre rack | `ReviewRegression.thePitchShifterShiftsTheWayItSays` | PARTIAL |
| EN-58 (§10) | 8 slots, drag reorder, 10 ms bypass fade, 4x OS on drive | `EffectsChain::kNumSlots`; `Pedal::bypassFade (0.010)`; `PedalsDrive` Oversampler | racks, drag reorder `PedalRack::reorder` | `Effects.chainReordersWithoutGlitching`, `Effects.bypassIsTransparent` | DONE |
| EN-59 (§11.1) | Amp models incl. Custom (preamp count, power tube, tone-stack layout) — Custom has no controls | `AmpEngine::setCustomStages/PowerTube/ToneStackStyle` (no caller) | amp model choice only | - | NO-GUI |
| EN-60 (§11.2) | Preamp stages, asymmetric shaper, coupling HP, 4x OS | `DSP/Amp/AmpEngine` | amp face `AmpFacePanel` | `Amp.gainProducesHarmonicDistortion` | DONE |
| EN-61 (§11.3) | Passive Yeh tone stack; presence in NFB | `DSP/Amp/ToneStack` | amp face | `ReviewRegression.theToneStackIsPassiveAtEverySetting` | DONE |
| EN-62 (§11.4-11.6) | Phase inverter asymmetry, push-pull + sag, output transformer — no per-stage test | `AmpEngine` | amp face Master | - | OWNED (on visual: EffectsQaTests `Amp.gainSweepIsMonotonicAt1kHz`, `Amp.coldStartHasNoTransient`) |
| EN-63 (§11.7) | Standby mutes; **30 s** warm-up — 8 s (`warmupGain.prepare (sr, 8.0)`) | `AmpEngine` warmupGain | amp face Standby | `Amp.standbyIsSilentAndWarmsUp` | PARTIAL |
| EN-64 (§12) | Post fx chorus, phaser stages, flanger, tremolo, rotary, delay types/sync/ping-pong, reverbs, spring, EQ | `DSP/Effects/PedalsMod` | post rack | `Effects.everyPedalTypeRunsCleanly`, `ReviewRegression.theReverbPedalTailSurvivesRepeatedParameterSends` | DONE |
| EN-65 (§13.1) | Cab IR convolution, 200+ IRs (504) — no "matches offline convolution" test | `DSP/Amp/CabinetEngine`; `Resources/CabIRs` | ADVANCED col 3 Cabinet | `Cabinet.procedualFallbackRemovesTheFizz` | NO-TEST |
| EN-66 (§13.2-13.3) | Mic type/position/distance swap IR; dual-mic blend, opposite pan | `CabinetEngine`; `mic_*`, `dual_mic`, `mic_blend`, `mic_width` | ADVANCED col 3 Mic; Easy mic cards | `Engine.monoCompatibility` | DONE |
| EN-67 (§13.4) | Async IR load with zero-latency fallback | `CabinetEngine` fallback; `ConvolutionInstaller` | n/a | `Editor.aFailedPresetLoadAndAMissingIrEachRaiseABannerOnce` | DONE |
| EN-68 (§14) | ER taps + FDN; size/damping/mix | `DSP/Amp/RoomEngine` | ADVANCED col 3 Room | `Room.biggerRoomsRingLonger`, `ReviewRegression.theRoomTailSurvivesRepeatedDecaySends` | DONE |
| EN-69 (§15) | Master gain -60…+12, limiter -0.3 dBFS, DC 5 Hz | `DSP/Master/MasterBus` | ADVANCED Master; Easy output | `Master.limiterHoldsTheCeiling` | DONE |
| EN-70 (§15) | Metering peak + RMS + LUFS — LUFS untested (shown only in Debug) | `MasterBus::getLufs` | Easy `LevelMeter`; Debug overlay | `Master.meteringTracksTheSignal` (peak/RMS) | NO-TEST |
| EN-71 (§16) | Per-preset state (guitar, tuning, whammy, chains, amp, cab, room, humanise, MIDI maps); host state incl. UI state | `Presets/PresetManager`; `PluginProcessor::get/setStateInformation` | File menu | `Presets.stateRoundTripsExactly`, `StateModel.*`, `HostState.*` | DONE |
| EN-72 (§17) | Serial strings; workers for IR/preset; FIFO/atomics only — UI writes `TuningEngine::setDetuneCents` and calls `engine.panic()` from the message thread | `GuitarBodyComponent.cpp:478`; `PluginProcessor::panic` | n/a | - | PARTIAL |
| EN-73 (§18) | Latency = body + cab + oversampling + lookahead — limiter lookahead / oversampler real delay not reported here | `LuthierEngine::getLatencySamples` | n/a | `Engine.latencyIsReportedAndPlausible` | OWNED (on visual 3e5c493: `Latency.anImpulseArrivesWhenReported`, `Latency.oversamplerReportsItsGroupDelay`) |
| EN-74 (§19) | Unit: String pitch/decay/bend | `StringEngine` | n/a | `StringEngine.pluckProducesCorrectPitch`, `StringEngine.higherNotesDecayFaster`, `StringEngine.bendIsSmoothAndReachesTarget` | DONE |
| EN-75 (§19) | Unit: every tuning within 0.1 c — only standard tuning checked at 0.1 c | `TuningEngine` | n/a | `Tuning::everyPresetIsExactToATenthOfACent`, `Tuning.standardTuningIsExact`, `Tuning.everyPresetProducesSaneFrequencies` | DONE |
| EN-76 (§19) | Unit: each technique from MIDI | `TechniqueEngine` | n/a | `Technique.*`, `TechniqueTriggers.*` | DONE |
| EN-77 (§19) | Unit: pickup comb, resonance f0 **and Q** — Q unchecked | `PickupEngine` | n/a | `Pickup::resonantQMatchesTheLcrValues`, `Pickup.positionCombNullsTheExpectedHarmonic`, `Pickup.resonantFrequencyMatchesTheLcrValues` | DONE |
| EN-78 (§19) | Unit: TransTrem keeps intervals | `WhammyEngine` | n/a | `Whammy.transTremPreservesChordIntervals` | DONE |
| EN-79 (§19) | Unit: body IR loads correctly; modal frequencies | `BodyEngine` | n/a | `Body.modalBankReproducesTheAirResonance` (modal only) | OWNED (on visual 6b41cf2: `IrReload.theFirstNoteAfterALoadIsEveryNote`) |
| EN-80 (§19) | Unit: amp stage harmonic content | `AmpEngine` | n/a | `Amp.gainProducesHarmonicDistortion` | OWNED (on visual: EffectsQaTests Amp.*) |
| EN-81 (§19) | Unit: coupling no runaway | `CouplingMatrix` | n/a | `Coupling.cannotRunAway` | DONE |
| EN-82 (§19) | Integration: chromatic scale, bend A→B, fast slide no NaN/denormal | engine | n/a | `Engine.chromaticScalePlaysAtTheRightPitch`, `StringEngine.bendIsSmoothAndReachesTarget`, `Engine.fastSlidesProduceNoNansOrDenormals` | DONE |
| EN-83 (§19) | 30 s @96 kHz all fx < 15 % CPU — here only 3 s < 0.85× real time | engine | n/a | `Engine.cpuStaysWithinBudget` | OWNED (on visual 37e22ba: `PerfBudget.scenarioTotals`, `PerfBudget.sampleRateScaling`) |
| EN-84 (§19) | Round-trip every factory preset within 0.01 dB — here one preset | `PresetManager` | n/a | `Presets.audioIsIdenticalAfterARoundTrip` | OWNED (on visual 3b53f8b: `Presets.everyFactoryPresetRoundTripsToTheUlp`) |
| EN-85 (§19) | Pluginval level 10 in CI | `.github/workflows/build.yml` (nightly strictness 10) | n/a | CI pluginval job | DONE |
| EN-86 (§19) | Host matrix: 7 hosts × 30 min — no record | - | n/a | - | MISSING |
| EN-87 (§19) | MIDI controller matrix (keyboard, MPE, GK/TriplePlay) end-to-end — no record | `ControllerProfile` | CONTROLLERS page | `Controllers.*` (synthetic) | MISSING |
| EN-88 (§20.5) | Coupling per block, capped — runs per sample (documented: per-block ticks) | `CouplingMatrix.h` header note | n/a | `Coupling.cannotRunAway` | PARTIAL |
| EN-89 (§20.8) | No console logging on audio thread; lock-free queue to file | `Support/Diagnostics` ring buffer | n/a | `Diagnostics.ringBufferAndSelfTestWork` | DONE |
| EN-90 (§20.18) | Feedback capped at 0.998 (room FDN and the reverb pedal FDN) | `RoomEngine::kMaxFeedback`, `PedalsMod.cpp`; delays 0.95 | n/a | `Room::feedbackNeverExceedsTheCap`, `Effects.secretEffectIsStableAtMaximumRegeneration` | DONE |
| EN-91 (§20.19) | Every factory preset passes mono compatibility — test uses one synthetic rig | `MasterBus`/stereo fx | n/a | `Engine.monoCompatibility` | NO-TEST |
| EN-92 (§22) | < 8 % CPU @96k/128, 6 strings, all fx | engine | n/a | - | OWNED (on visual: `PerfBudget.everyModuleWithinBudget`, `PerfBudget.scenarioTotals`) |
| EN-93 (§22) | 16 instances @48k/256 no glitches | engine | n/a | - | OWNED (on visual: `Stress.thirtyTwoInstancesRenderInTurn`) |
| EN-94 (§22) | Preset load < 500 ms incl. async IR | `PresetManager::fromVar`; IR installers | n/a | - | NO-TEST |
| EN-95 (§22) | MIDI in → audio out < 2 ms (+ reported latency) | `MidiInterpreter` sample-accurate events; chord window | n/a | `Controllers.chordGroupsSoundOneWindowAfterTheyWerePlayed` | NO-TEST |

<!-- counts DONE=65 NO-GUI=7 NO-TEST=5 PARTIAL=6 MISSING=2 OWNED=10 -->
