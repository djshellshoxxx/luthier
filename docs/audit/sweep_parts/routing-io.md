## routing-io.md

The routing engine is complete: all four layouts are advertised (plus the Aux 8 noise bus of DECISIONS.md), the aux taps, per-string buses, mute/solo/trim, sidechain-to-amp, the four MIDI-out sources and the ROUTING workspace tab all exist, and five of the six spec tests are present. Gaps: there is no sidechain-compressor pedal; the layout "selector" is a read-only label of the negotiated layout; routing state is saved only in the host session, not in `.luthierpreset` files (`PresetManager::toVar` writes no `routing` block), so §9 "per-preset" is not met. The latency impulse test, the limiter-lookahead latency term and the panel-vs-host latency check are now merged (`Latency::*`).

| Req | Summary | Engine location | GUI location | Test | Status |
|---|---|---|---|---|---|
| RIO-1 (§0.1, §1) | Layouts A/B/C/D advertised via BusesProperties; host picks | `PluginProcessor.cpp:buildBusesProperties`, `isBusesLayoutSupported` | n/a | `Routing::everyLayoutRendersCleanly`, `PluginBuses::perStringLayoutPutsEachStringOnItsOwnBus` | DONE |
| RIO-2 (§0.2) | Runs correctly at every advertised layout incl. stereo-only | `PluginProcessor::processSlice`, `RoutingMatrix::distribute` | n/a | `Routing::everyLayoutRendersCleanly` | DONE |
| RIO-3 (§0.3) | Extra outs post-limiter unless tap points; taps documented | `Routing/TapBuffers.h`, `RoutingMatrix::distribute` | n/a | `Routing::diTapNullsAgainstReappliedAmp` | DONE |
| RIO-4 (§0.4, §4) | Sidechain optional; makes follower source `sidechain` live — no test drives a follower from the sidechain bus | `ModEnvelopeFollower::Source::sidechain`, `ModBlockContext` sidechain peak | ADVANCED > MOD, follower card `followerSourceBox` | - | NO-TEST |
| RIO-5 (§0.5, §6) | MIDI out sample-accurate for pass-through and generated events | `Routing/MidiOutRouter.cpp` | n/a | `Routing::midiOutPassThroughIsSampleExact`, `MidiExport::liveMidiOutKeepsTenThousandEventsOnTheirSample` | DONE |
| RIO-6 (§2) | Aux 1-7 tap assignments (DI, pre-cab, mic1, mic2, room, wet, monitor) — only Aux1/Aux6 (and Aux 8) are asserted by tests | `TapBuffers`, `RoutingMatrix::distribute/writeMonitorBus` | n/a | `Routing::diTapNullsAgainstReappliedAmp` (Aux1 only) | NO-TEST |
| RIO-7 (§2) | Per-aux gain trim | `RoutingMatrix::setAuxGainDb` | ADVANCED > ROUTING, `AuxStrip::gain` | `PluginBuses::aux8CarriesThePlayingNoiseAndObeysItsStrip`, `Routing::stateRoundTrips` | DONE |
| RIO-8 (§2) | Muted aux bus skips its tap render | `RoutingMatrix::updateWantedTaps` | ROUTING `AuxStrip` mute | `Routing::muteAndSoloResolveTogether` | DONE |
| RIO-9 (§3) | 12 mono per-string buses, post-body pre-pickup, silence when unused | `RoutingMatrix::distribute` per-string | ROUTING `PerStringStrip` | `Routing::perStringOutputsSumToPreBody`, `PluginBuses::perStringLayoutPutsEachStringOnItsOwnBus` | DONE |
| RIO-10 (§3, §7) | Per-string latency = engine-only latency | `LuthierEngine::getPerStringLatencySamples` | ROUTING `latencyLabel` | `Routing::perOutputLatencyIsConsistent` | DONE |
| RIO-11 (§4) | Sidechain ducks a "sidechain compressor" pedal — no such pedal | `DSP/Effects/Pedal.h:PedalType` (absent) | - | - | MISSING |
| RIO-12 (§4) | Sidechain as monitor input for backing-track summing | `Live/LiveControls.cpp:MonitorMix` | Live Strip `monitorLevel` | `LiveMonitor::idleWhenNothingToMonitorAndSumsWhenThereIs` | DONE |
| RIO-13 (§4) | Sidechain never sums into the guitar output unless consumed — no test asserts main out unchanged by a sidechain with toggle off | `PluginProcessor::processSlice` (sidechainCopy to consumers only) | n/a | - | NO-TEST |
| RIO-14 (§4, §8) | Sidechain meter in routing panel — untested | `RoutingMatrix::meterSidechain` | ROUTING `RoutingPanel::sidechainMeterBounds` | - | NO-TEST |
| RIO-15 (§5A) | External re-amp: Aux 1 DI render (pre/post-circuit option) | `LuthierEngine` DI tap, `aux1_pre_circuit` | ROUTING `RoutingPanel::aux1PreCircuit` | `ModelGapsUi::auxOneTapsBeforeOrAfterTheCircuit` | DONE |
| RIO-16 (§5B) | Internal re-amp "Sidechain to amp" toggle, off by default, replaces string engine | `RoutingMatrix::setSidechainToAmp`, `LuthierEngine::setSidechainToAmp` | ROUTING `RoutingPanel::sidechainToAmp` | `Routing::sidechainToAmpReplacesTheInstrument` | DONE |
| RIO-17 (§6) | MIDI out: note pass-through, same timestamps | `MidiOutRouter::captureInput` | ROUTING `midiPassThrough` | `Routing::midiOutPassThroughIsSampleExact` | DONE |
| RIO-18 (§6) | MIDI out: rhythm-engine stream | `MidiOutRouter` rhythm source | ROUTING `midiRhythm` | `MidiExport::liveMidiOutKeepsTenThousandEventsOnTheirSample` | DONE |
| RIO-19 (§6) | MIDI out: per-string activity NoteOn/Off | `StringActivityQueue`, `MidiOutRouter` | ROUTING `midiStringActivity` | `Routing::stringActivityIsSampleAccurate` | DONE |
| RIO-20 (§6) | MIDI out: macro CC broadcast, CC# assignable | `MidiOutConfig::macroCc` | ROUTING `macroCc[]` combos | `MidiExport::liveMidiOutKeepsTenThousandEventsOnTheirSample`, `MidiOutPanel::liveSwitchesAreTheRoutingPanelsSwitches` | DONE |
| RIO-21 (§6) | Silently no-ops on hosts without MIDI out — host behaviour, untested | JUCE `MidiBuffer` | n/a | - | NO-TEST |
| RIO-22 (§7) | Main-out latency = OS group delay + convolution + limiter lookahead (master.getLatencySamples adds the lookahead) | `LuthierEngine::getLatencySamples` (body+cabinet+effects+amp+midi+master), `PluginProcessor::updateRoutingLatencyReport` | n/a | `Latency::anImpulseArrivesWhenReported`, `Latency::oversamplerReportsItsGroupDelay`, `Routing::perOutputLatencyIsConsistent` | DONE |
| RIO-23 (§7) | Per-output latency (DI, pre-cab, per-string); single-value hosts get main | `LuthierEngine::getLatencySamples(AuxBus)`, `setLatencySamples` | n/a | `Routing::perOutputLatencyIsConsistent` | DONE |
| RIO-24 (§8) | ROUTING panel in Advanced Column 4 tab strip | `UI/AdvancedPanel.cpp:buildWorkspace` | ADVANCED > ROUTING tab | `Editor::everyWorkspaceTabSelectsAndPaints` | DONE |
| RIO-25 (§8) | Layout selector listing host-supported layouts — only a read-only label of the negotiated layout | `RoutingMatrix::getActiveLayout` | ROUTING `RoutingPanel::layoutLabel` | - | PARTIAL |
| RIO-26 (§8) | Aux strips: mute, solo, gain, meter (8 strips incl. Aux 8) | `RoutingMatrix` mute/solo/gain/meter | ROUTING `AuxStrip` | `Routing::muteAndSoloResolveTogether` | DONE |
| RIO-27 (§8) | Compact per-string strip shown only for Layout C/D — visibility untested | `RoutingMatrix::setPerStringMuted/GainDb` | ROUTING `PerStringStrip` | - | NO-TEST |
| RIO-28 (§8) | MIDI out enable + source checkboxes + CC-mapping table | `MidiOutConfig` | ROUTING `midiOutEnable`, `midiPassThrough`..., `macroCc[]` | `MidiOutPanel::liveSwitchesAreTheRoutingPanelsSwitches` | DONE |
| RIO-29 (§8) | Latency readout per active output; panel value checked against the host-reported value | `RoutingMatrix::getLatencyReport` | ROUTING `RoutingPanel::latencyLabel` | `Latency::dspLatencyIsWithinBudget` (host == panel mainOut, per-output budgets) | DONE |
| RIO-30 (§9) | Layout not stored; aux mute/solo/gain restored from the preset — `routing` block in `.luthierpreset` (absent = defaults) | `RoutingMatrix::toVar/fromVar` via `Presets/PresetBlocks.cpp` | n/a | `Presets::processorBlocksTravelInThePresetFile`, `Routing::stateRoundTrips`, `Routing::loadingClearsPreviousState` | DONE |
| RIO-31 (§9) | MIDI-out assignments per-preset | same as RIO-30 | n/a | `Presets::processorBlocksTravelInThePresetFile` | DONE |
| RIO-32 (§9) | Sidechain-to-amp per-preset | same as RIO-30 | n/a | `Presets::processorBlocksTravelInThePresetFile` | DONE |
| RIO-T1 (§10) | Test: each layout instantiates, expected sample count per output | n/a | n/a | `Routing::everyLayoutRendersCleanly` | DONE |
| RIO-T2 (§10) | Test: Aux1 DI vs main null within -60 dBFS after re-applied amp | n/a | n/a | `Routing::diTapNullsAgainstReappliedAmp` | DONE |
| RIO-T3 (§10) | Test: sum of per-string outs = main pre-body within -80 dBFS | n/a | n/a | `Routing::perStringOutputsSumToPreBody` | DONE |
| RIO-T4 (§10) | Test: 10 000-event MIDI pass-through fuzz, 0-sample error | n/a | n/a | `Routing::midiOutPassThroughIsSampleExact` | DONE |
| RIO-T5 (§10) | Test: sidechain-to-amp routes correctly | n/a | n/a | `Routing::sidechainToAmpReplacesTheInstrument` | DONE |
| RIO-T6 (§10) | Test: reported main latency = measured impulse latency within 1 sample (amp path, cabinet/room off) | n/a | n/a | `Latency::anImpulseArrivesWhenReported`, `Routing::perOutputLatencyIsConsistent` | DONE |

<!-- counts DONE=30 NO-GUI=0 NO-TEST=6 PARTIAL=1 MISSING=1 DEFERRED=0 -->
