## harmonic-realism.md

REALISM-B has landed on this checkout: the contact comb and contacts API, all four harmonic kinds, the analytic node search, the sounding-pitch locator, the 8 parameters, CC 103/104, the CHARACTER HARMONICS group and the fretboard contact ring, with `HarmonicRealism.HR01..HR19` registered. Still open: a test for the 0.35 receptivity while touching (HR-11), the gui-integration §19 row (HR-25) and the gui-integration row (HR-25); the midi-export `touch_fret`/`partial` fields (HR-27) are DEFERRED by the spec.

| Req | Summary | Engine location | GUI location | Test | Status |
|---|---|---|---|---|---|
| HR-1 (§intro 1, 0.2, 4.1) | Stopped length sets pitch: natural = partial n of open string (fixes octave-high bug); `NoteOnEvent::touchFret` | `MidiInterpreter::emitVoicedNote` | n/a | `HarmonicRealism.HR01_naturalHarmonicPitch` | DONE |
| HR-2 (§intro 2, 0.1, 3) | Harmonic kind renders as ordinary pluck; band isolation only as fallback; `t60Scale *= 0.55` removed | `Excitation` Harmonic kind, `Params::isolateHarmonic` | n/a | `HarmonicRealism.HR02_HR03_fundamentalSuppressedAndNotASine` | DONE |
| HR-3 (§1 eq 1-3) | n-tap node comb in the loop at the stretched partial | `StringEngine::applyContacts`, `getPartialFrequency` | n/a | `HarmonicRealism.HR01_naturalHarmonicPitch`, `HarmonicRealism.HR04_fret7`, `HarmonicRealism.HR12_randomContactsAreStable` | DONE |
| HR-4 (§0.3, 1) | Node efficiency e(d,w), n search 2..8 maximising e/sqrt(n), off-node < 0.05 (owner: pad profile, 0.1) | `DSP/String/Harmonics.h:findNode/nodeEfficiency` | n/a | `HarmonicRealism.HR06_aMissedTouchIsADeadThud`, `HarmonicRealism.HR07_fingerWidthIsTheTolerance` | DONE |
| HR-5 (§0.5) | Just intonation kept | as HR-3 | n/a | `HarmonicRealism.HR05_justIntonationIsKept` | DONE |
| HR-6 (§2) | `Contact` struct, kMaxContacts 4, addContact/clearContact/clearAllContacts; idle path skipped | `StringEngine::addContact/...` | n/a | `HarmonicRealism.HR12_randomContactsAreStable`, `HarmonicRealism.HR16_contactFreeIsBitIdentical` | DONE |
| HR-7 (§2) | g ramps over 1 ms | `contactRampStep` | n/a | `HarmonicRealism.HR13_landingAndLiftingAreClickFree` | DONE |
| HR-8 (§2) | M, n recomputed on needsLoopUpdate only | `updateContactSpacing` | n/a | `HarmonicRealism.HR18_realtime` | DONE |
| HR-9 (§2) | Fallback to band isolation when loop too short, counted by validator | `canRealiseContact`, `Validator::reportHarmonicFallback` | n/a | `HarmonicRealism.HR15_fallbackIsCounted` | DONE |
| HR-10 (§2) | `setHarmonicRestriction` kept as shim | `StringEngine::setHarmonicRestriction` | n/a | n/a (callers compile) | DONE |
| HR-11 (§2) | couplingReceptivity 0.35 while touching — implemented; no test | `StringEngine::updateReceptivity` | n/a | - | NO-TEST |
| HR-12 (§2) | `processSample(coupling, direct)` split; scrape/slap/tap use directInput | `StringEngine::processSample` | n/a | `HarmonicRealism.HR16_contactFreeIsBitIdentical`, `Scrape.*` | DONE |
| HR-13 (§3) | Natural / artificial harmonics via contact | `LuthierEngine::applyHarmonicContact` | n/a | `HarmonicRealism.HR01_naturalHarmonicPitch`-`HarmonicRealism.HR09_artificialHarmonics` | DONE |
| HR-14 (§3) | Plucking at a node kills the harmonic | `Params::exactPluckComb` | n/a | `HarmonicRealism.HR08_pluckingTheNodeKillsIt` | DONE |
| HR-15 (§3) | Pinch: node nearest pick + thumb offset, CC 70 sweeps; no velocity partial | `applyHarmonicContact`, `TechniqueEngine::decide` | n/a | `HarmonicRealism.HR10_pinchHarmonic` | DONE |
| HR-16 (§3) | Tapped harmonic: legato Kind::Tap impulse, no voice steal | `applyHarmonicContact` (tapped) | n/a | `HarmonicRealism.HR11_tappedHarmonicConvertsTheRingingString` | DONE |
| HR-17 (§3) | Artificial offsets map to partials (12->2, 7->3, 5->4, 4->5, 19->3, 24->4) | `harmonics::offsetFretsForChoice` | HARMONICS offset tooltips | `HarmonicRealism.HR09_artificialHarmonics` | DONE |
| HR-18 (§4.1) | Analytic node search replaces `harmonicPartialForFret` table (3.86, 8.84, 15.86 hit) | `harmonics::partialForFret` | n/a | `Technique.harmonicNodesAreDetected`, `HarmonicRealism.HR05_justIntonationIsKept` | DONE |
| HR-19 (§4.2) | Sounding-pitch mapping via `HarmonicLocator::find` | `harmonics::locate`, `MidiInterpreter::emitSoundingHarmonic` | HARMONICS note-mapping box | `HarmonicRealism.HR14_soundingPitchLocator` | DONE |
| HR-20 (§5) | 8 params (touch pressure, finger width, touch time, brief touch, thumb offset, artificial/tapped offset, note mapping); pick family rows | `Parameters.*` REALISM-B block, `PhysicalRange.cpp` | `UI/HarmonicsGroup.*` | `HarmonicRealism.HR17_rangesAreRegistered` | DONE |
| HR-21 (§6) | Existing CC 72 pinch / CC 73 natural / velocity trigger unchanged | `MidiInterpreter` CC map, `TechniqueEngine::setPinchHarmonicTrigger` | n/a (MIDI) | `Technique.controllersTakePriorityOverInference`, `Technique.harmonicNodesAreDetected` | DONE |
| HR-22 (§6) | CC 103 ArtificialHarmonic, CC 104 TappedHarmonic; decide() priority order | `MidiInterpreter::resetCcMapToDefaults`, `TechniqueEngine::decide` | n/a (MIDI) | `HarmonicRealism.HR19_FA17_luthierExportRoundTrip`, `Technique.controllersTakePriorityOverInference` | DONE |
| HR-23 (§7) | CHARACTER HARMONICS row in PICK with partial tooltips | n/a | `UI/HarmonicsGroup.*` after PICK in `CharacterPanel` | `RealismBUi.theCharacterTabCarriesTheThreeGroups` | DONE |
| HR-24 (§7) | Fretboard hollow ring at contact, fading; dashed when missed | `getContactDisplay` | `UI/FretboardRealismB.cpp` | `RealismBUi.theFretboardDrawsTheTouch` | DONE |
| HR-25 (§7) | gui-integration.md §19 row "Harmonic contact / offsets / mapping" - no such row in spec/gui-integration.md (Feature-to-location index) | n/a | n/a (doc) | - | MISSING |
| HR-26 (§7) | Preset: plain APVTS params | APVTS | n/a | `FingerstyleAttack.FA17_rangesRealtimeRoundTrip` (same mechanism) | DONE |
| HR-27 (§7) | midi-export NOTE gains `touch_fret` + `partial` - deferred by spec/harmonic-realism.md (item 10, midi-export owns the capture path); not implemented | none | n/a | - | DEFERRED |
| HR-28 (§8) | No allocation, reset clears contacts, budget (spec 0.05, owner 1.0 units) | `StringEngine::reset`, `resetRealismB` | n/a | `HarmonicRealism.HR18_realtime` | DONE |
| HR-29 (§9 HR-01) | Natural harmonic pitch | - | n/a | `HarmonicRealism.HR01_naturalHarmonicPitch` | DONE |
| HR-30 (§9 HR-02/03) | Fundamental suppressed; not a sine | - | n/a | `HarmonicRealism.HR02_HR03_fundamentalSuppressedAndNotASine` | DONE |
| HR-31 (§9 HR-04) | Fret 7 | - | n/a | `HarmonicRealism.HR04_fret7` | DONE |
| HR-32 (§9 HR-05) | Just intonation | - | n/a | `HarmonicRealism.HR05_justIntonationIsKept` | DONE |
| HR-33 (§9 HR-06) | Missed touch | - | n/a | `HarmonicRealism.HR06_aMissedTouchIsADeadThud` | DONE |
| HR-34 (§9 HR-07) | Finger width | - | n/a | `HarmonicRealism.HR07_fingerWidthIsTheTolerance` | DONE |
| HR-35 (§9 HR-08) | Plucking the node | - | n/a | `HarmonicRealism.HR08_pluckingTheNodeKillsIt` | DONE |
| HR-36 (§9 HR-09) | Artificial | - | n/a | `HarmonicRealism.HR09_artificialHarmonics` | DONE |
| HR-37 (§9 HR-10) | Pinch | - | n/a | `HarmonicRealism.HR10_pinchHarmonic` | DONE |
| HR-38 (§9 HR-11) | Tapped | - | n/a | `HarmonicRealism.HR11_tappedHarmonicConvertsTheRingingString` | DONE |
| HR-39 (§9 HR-12) | Stability | - | n/a | `HarmonicRealism.HR12_randomContactsAreStable` | DONE |
| HR-40 (§9 HR-13) | Click-free | - | n/a | `HarmonicRealism.HR13_landingAndLiftingAreClickFree` | DONE |
| HR-41 (§9 HR-14) | Sounding-pitch mapping | - | n/a | `HarmonicRealism.HR14_soundingPitchLocator` | DONE |
| HR-42 (§9 HR-15) | Fallback counted | - | n/a | `HarmonicRealism.HR15_fallbackIsCounted` | DONE |
| HR-43 (§9 HR-16) | Contact-free bit-identical | - | n/a | `HarmonicRealism.HR16_contactFreeIsBitIdentical` | DONE |
| HR-44 (§9 HR-17) | Ranges | - | n/a | `HarmonicRealism.HR17_rangesAreRegistered` | DONE |
| HR-45 (§9 HR-18) | Realtime (budget relaxed to 1.0 units) | - | n/a | `HarmonicRealism.HR18_realtime` | DONE |
| HR-46 (§9 HR-19) | Export round trip | - | n/a | `HarmonicRealism.HR19_FA17_luthierExportRoundTrip` | DONE |
