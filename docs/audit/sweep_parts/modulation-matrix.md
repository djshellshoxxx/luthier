## modulation-matrix.md

The engine is complete: 8 LFOs, 4 DAHDSR envelopes, 2 step sequencers, 2 followers, note/CC/14-bit/random/macro sources, compiled lock-free routes (8 per destination), discrete destinations and all five spec tests. The MOD tab is thinner than the engine. It has no route Offset column. The LFO phase and breakpoint shape, the envelope retrigger, stage curves and loop mode, the sequencer's step grid and internal rate, and the follower's string index and log curve have no controls. There are no per-source colours. The matrix is not written to `.luthierpreset` files (session state and snapshots only), so preset load neither restores nor resets it. Per-string, rhythm, pan and snapshot-morph destinations do not exist as parameters. Drag-to-modulate is on the visual branch.

| Req | Summary | Engine location | GUI location | Test | Status |
|---|---|---|---|---|---|
| MM-1 (§0.1) | Control rate = block/32, min 128 samples, set in prepare | `Modulation/ModMatrix.cpp:ModMatrix::prepare` | n/a | `Modulation::controlRateIsABlockOver32FlooredAt128` | DONE |
| MM-2 (§0.2) | Linear interpolation between control ticks — implemented as a one-pole halving step per tick, read once per block by the bridge; not linear, untested | `ModMatrix::processBlock` (currentOffsets += (target-current)*0.5) | n/a | `Modulation::offsetsRampLinearlyBetweenTicks` (now a true linear ramp to each tick's target; was a one-pole that never arrived) | DONE |
| MM-3 (§0.3, §3) | Additive over base, (v*depth+offset)*range, clamped to range | `ModMatrix::apply` | n/a | `Modulation::routeModulatesItsDestination` | DONE |
| MM-4 (§0.4) | Up to 8 sources per destination | `ModMatrix::addRoute` (kMaxRoutesPerDestination) | Modulate menu disables when full | `Modulation::destinationAcceptsEightSourcesAndNoMore` | DONE |
| MM-5 (§0.4, §3) | Per-route depth and offset -100..+100% (engine) | `ModRoute::depth/offset` | see MM-40 | `Modulation::presetRoundTripIsExact` | DONE |
| MM-6 (§0.5) | Sources reset on preset load — every preset load goes through `readPresetBlocks` -> `ModMatrix::fromVar`, which asks for a source restart that the next block performs on the audio thread (it used to reset from the message thread, racing the tick) | `ModMatrix::fromVar`, `sourceResetPending`, `processBlock` | n/a | `Modulation::loadingAPresetResetsTheSources` | DONE |
| MM-7 (§0.5) | Sources reset on snapshot recall | `applySnapshotModules` -> `fromVar` -> deferred reset | n/a | `Modulation::recallingASnapshotResetsTheSources` | DONE |
| MM-8 (§0.5) | Sources reset on transport start: envelopes, followers, random and sequencers; LFOs per their retrigger mode (free-run exempt by 1.1) | `ModMatrix::updateSources` (`transportJustStarted`) | n/a | `Modulation::transportStartAndNotesRetriggerWhereTheySay` | DONE |
| MM-9 (§0.5, §1.1) | Free-running LFO as per-source option: ignores notes and transport | `ModLfo::Retrigger::freeRun` | ADVANCED > MOD, LFO card `retriggerBox` | `Modulation::transportStartAndNotesRetriggerWhereTheySay` | DONE |
| MM-10 (§0.6, §6) | Matrix stored under `modulation` in every preset (state worker's PresetBlocks) | `writePresetBlocks`/`readPresetBlocks` | n/a | `Presets::processorBlocksTravelInThePresetFile`, `Modulation::aPresetFileCarriesTheMatrixExactly` | DONE |
| MM-11 (§1.1) | LFO x8, 8 shapes incl S+H, random smooth, custom | `Modulation/ModSources.cpp:ModLfo` | MOD LFO card `shapeBox` | `Modulation::lfoFrequencyIsAccurate`, `Modulation::sampleAndHoldHoldsForAWholeCycle` | DONE |
| MM-12 (§1.1) | LFO custom 8-point breakpoint editor — component and edit kind done; placement on the LFO card waits on the ModMatrixPanel merge | `ModLfo::setBreakpoint`, `ModSourceEdit::Kind::lfoBreakpoint` | `UI/ModSourceEditors` `LfoBreakpointEditor` (not yet on the card) | `ModMatrixUi::draggingABreakpointWritesTheLfo` | PARTIAL |
| MM-13 (§1.1) | LFO rate 0.01-40 Hz or tempo-synced 1/32T..8 bars incl dotted/triplet | `ModLfo`, `ModSyncDivision` | MOD LFO card `rateSlider`, `syncButton`, `divisionBox` | `Modulation::syncedLfoFollowsTheHost`, `Modulation::lfoFrequencyIsAccurate` | DONE |
| MM-14 (§1.1) | LFO phase offset 0-360 | `ModLfo::setPhaseOffsetDegrees` | MOD LFO card `phaseSlider` | `ModMatrixUi::theSourceCardsWriteTheirNewControls` | DONE |
| MM-15 (§1.1) | LFO depth, symmetry, smoothing 0-500 ms, uni/bipolar — untested | `ModLfo::setDepth/setSymmetry/setSmoothingMs/setBipolar` | MOD LFO card sliders, `bipolarButton` | - | NO-TEST |
| MM-16 (§1.1) | LFO retrigger free/NoteOn/transport (sync-boundary uses the host position) | `ModLfo::Retrigger` | MOD LFO card `retriggerBox` | `Modulation::transportStartAndNotesRetriggerWhereTheySay` (free, note, transport) | DONE |
| MM-17 (§1.2) | Envelope x4 DAHDSR, 0-30 s stages, sustain 0-100% | `ModEnvelope` | MOD ENV card sliders | `Modulation::envelopeStageTimesAreAccurate` | DONE |
| MM-18 (§1.2) | Envelope curve per stage | `ModEnvelope::setStageCurve` | MOD ENV card attack/decay/release curve combos | `ModMatrixUi::theSourceCardsWriteTheirNewControls` | DONE |
| MM-19 (§1.2) | Envelope retrigger legato/always/one-shot | `ModEnvelope::setRetrigger` | MOD ENV card `envRetriggerBox` | `ModMatrixUi::theSourceCardsWriteTheirNewControls` | DONE |
| MM-20 (§1.2) | Envelope loop mode off/D-S/D-R | `ModEnvelope::setLoopMode` | MOD ENV card `loopModeBox` | `ModMatrixUi::theSourceCardsWriteTheirNewControls` | DONE |
| MM-21 (§1.3) | Step seq x2: length 4-64, grid, 5 directions, swing 0-75% | `ModStepSequencer` | MOD STEP card `lengthSlider`, `divisionBox`, `directionBox`, `swingSlider` | `Modulation::stepSequencerWalksItsSteps` | DONE |
| MM-22 (§1.3) | Per-step value/gate/slide/probability — step grid component and edit kind done; placement on the STEP card waits on the ModMatrixPanel merge | `ModStepSequencer::setStep`, `ModSourceEdit::Kind::seqStep` | `UI/ModSourceEditors` `StepGridEditor` (not yet on the card) | `ModMatrixUi::theStepGridWritesSteps`, `Modulation::stepSequencerWalksItsSteps` | PARTIAL |
| MM-23 (§1.3) | Seq sync to host / internal rate (now with a control, saved in the matrix); no tap-tempo option | `ModStepSequencer::setSynced/setInternalRateHz`, `ModMatrix::toVar` "rate" | MOD STEP card `syncButton` + `seqRateSlider` | `ModMatrixUi::theSourceCardsWriteTheirNewControls` | PARTIAL |
| MM-24 (§1.4) | Followers x2: source main/sidechain/per-string/pickup, attack, release, detection, threshold | `ModEnvelopeFollower` | MOD FOLLOW card `followerSourceBox`, `detectionBox`, sliders | `Modulation::envelopeFollowerTracksLevel` | DONE |
| MM-25 (§1.4) | Follower per-string index and log/linear output curve | `ModEnvelopeFollower::setStringIndex/setLogarithmic` | MOD FOLLOW card `followerStringBox` + `Log` toggle | `ModMatrixUi::theSourceCardsWriteTheirNewControls` | DONE |
| MM-26 (§1.5) | Note pitch/velocity/trigger/held/AT/poly-AT sources | `ModMatrix` note handling, `ModSourceSlots` 16-21 | MOD source selector / ADD ROUTE menu | `Modulation::noteSourcesFollowNotes` | DONE |
| MM-27 (§1.6) | Any CC, 14-bit pairs, PB, mod wheel, ch pressure | `ModSourceSlots::ccBase/cc14Base/pitchBend...` | MOD source selector | `Modulation::controllerSourcesFollowMidi` | DONE |
| MM-28 (§1.7) | 8 macros, named 32-char slots, host params "Macro 1..8" — 6 fixed-function macros + 2 assign, no naming | `Parameters.h:macro_*`, `ModSourceSlots::macroBase` | Easy/Advanced macro knobs | `GuiReach::everyAutomatableParameterHasAVisibleControl` | PARTIAL |
| MM-29 (§1.7) | Macros assignable to any control and themselves modulatable — untested as such | macro source slots; macro params are destinations | right-click Modulate on macro knobs | - | NO-TEST |
| MM-30 (§1.8) | Random per-note / per-bar / smooth, seeded | `ModRandomSource` | MOD source selector | `Modulation::randomSourcesAreDeterministic` | DONE |
| MM-31 (§2) | Every automatable parameter is a destination | `ModMatrix` destinations = parameter index | right-click Modulate on any `AttachedKnob` | `Modulation::routeModulatesItsDestination`, `ModelGapsUi::theSustainControlsAreModulationDestinations` | DONE |
| MM-32 (§2) | Per-string destinations (tuning cents, damping, pluck...) — per-string values are not parameters | - | - | - | MISSING |
| MM-33 (§2) | Rhythm-engine destinations (density, hand position, humanize) — only `hum_*` exist as params; density/hand position are not | `RhythmEngine::setVoicingDensity` (not a param) | - | - | PARTIAL |
| MM-34 (§2) | Master volume, output pan — `master_gain` yes, no pan parameter | `Parameters.h:master_gain`, `stereo_width` | - | - | PARTIAL |
| MM-35 (§2) | Snapshot morph position as destination — not a parameter (`preset_morph_position` is the preset morph) | `SnapshotBank::setMorphPosition` | - | - | MISSING |
| MM-36 (§2, §4) | Discrete destinations step at integer boundaries; bypass at 0.5 | `ModMatrix::apply` discrete branch | n/a | `Modulation::discreteDestinationsStepAtBoundaries`, `Modulation::aRouteAtZeroLeavesEveryChoiceWhereItIs` | DONE |
| MM-37 (§3) | Route record {source_id, channel, destination_id, depth, offset, curve, enabled}, interned ids | `ModRoute`, `modSourceSlotForId` | n/a | `Modulation::presetRoundTripIsExact`, `Modulation::sourceIdsResolveBothWays` | DONE |
| MM-38 (§3) | Curves Linear/Exp/Log/S | `ModCurve` | MOD route table curve cell (click cycles) | `Modulation::curvesPreserveSignAndFixedPoints` | DONE |
| MM-39 (§3) | Custom(name) curve — `ModCurve` has no custom entry | - | - | - | MISSING |
| MM-40 (§5) | Route table columns source/destination/depth/offset/curve/enabled/delete | `ModMatrix::setRouteDepth/setRouteOffset` | MOD `ModRouteTable` (Offset column, typed like depth) | `ModMatrixUi::theRouteTableEditsDepthAndOffset` | DONE |
| MM-41 (§3) | Enabled toggle per route | `ModRoute::enabled` | MOD `ModRouteTable` "On" column | `Modulation::routeModulatesItsDestination` (disabled route returns to base) | DONE |
| MM-42 (§5) | MOD tab in Advanced Column 4 | `UI/ModMatrixPanel.cpp` | ADVANCED > MOD tab | `Editor::everyWorkspaceTabSelectsAndPaints` | DONE |
| MM-43 (§5) | Left-third source pool / right two-thirds table — stacked vertically (documented in ModMatrixPanel.h) | n/a | MOD `ModMatrixPanel::resized` | - | PARTIAL |
| MM-44 (§5) | Add-route button — untested | `ModMatrix::addRoute` | MOD `ModMatrixPanel::addButton` | - | NO-TEST |
| MM-45 (§5) | Drag a source card onto a control creates a route — on visual: `ModSourceCard::mouseDrag`, `DragToModulate::aDroppedSourceRoutesAt25PercentAsOneEntry` | - | - | - | OWNED |
| MM-46 (§5) | Right-click any control -> Modulate submenu creates a route | `UI/Widgets.cpp` kModulateMenuBase | any `AttachedKnob` right-click | `Editor::rightClickOffersModulationAndBuildsTheRoute` | DONE |
| MM-47 (§5, §7) | Depth arc drawn around modulated controls — untested | `Widgets.cpp` "modulation arc" paint | every `AttachedKnob` | - | NO-TEST |
| MM-48 (§5) | Per-source user colour tag; routes and arcs inherit; defaults alternate accents — arc is always `Palette::secondary` | - | - | - | MISSING |
| MM-49 (§6) | Snapshot flag "includes modulation" vs "preset-level only" — snapshots always capture the matrix | `LuthierAudioProcessor::captureSnapshot` | - | - | MISSING |
| MM-50 (§6) | Import validates destination IDs and warns on unknown — list collected, no user-visible warning, and preset files carry no matrix | `ModMatrix::getUnknownDestinations` | - | `Modulation::unknownDestinationsAreReportedNotFatal` | PARTIAL |
| MM-51 (§7) | Automation moves base/control; modulation does not move the control; both stack — untested | `ParameterBridge` reads param then `ModMatrix::apply` | knob + arc | - | NO-TEST |
| MM-T1 (§8) | Test: each source's expected output (LFO freq, EG times, S+H hold) | n/a | n/a | `Modulation::lfoFrequencyIsAccurate`, `Modulation::envelopeStageTimesAreAccurate`, `Modulation::sampleAndHoldHoldsForAWholeCycle` | DONE |
| MM-T2 (§8) | Test: 1000-route stress under CPU budget | n/a | n/a | `Modulation::thousandRouteStressTest` | DONE |
| MM-T3 (§8) | Test: preset round trip byte-identical, through a saved preset file | n/a | n/a | `Modulation::aPresetFileCarriesTheMatrixExactly`, `presetRoundTripIsExact` | DONE |
| MM-T4 (§8) | Test: 5-option selector changes at 1/5..4/5 | n/a | n/a | `Modulation::discreteDestinationsStepAtBoundaries` | DONE |
| MM-T5 (§8) | Test: seeded random renders byte-identical | n/a | n/a | `Modulation::randomSourcesAreDeterministic` | DONE |

<!-- counts DONE=37 NO-GUI=0 NO-TEST=5 PARTIAL=8 MISSING=5 OWNED=1 -->
