## harmonic-realism.md

On this checkout harmonics are still the old model: the string is tuned to the touch fret (natural harmonics sound an octave high), `Excitation` band-isolates one partial, `StringEngine` keeps `t60Scale *= 0.55`, and the pinch partial comes from velocity; only the existing CC 72/73 and velocity triggers match the spec. The realism-b branch (last commit 2026-09-24 16:00, coverage doc complete) implements the contact comb, contacts API, all four harmonic kinds, analytic node search, sounding-pitch locator, 8 parameters, CC 103/104, the HARMONICS group and the fretboard contact ring, with `HarmonicRealism.HR01..HR19` (spot-checked tests, `HarmonicsGroup` in `CharacterPanel`, `RealismBUi.theFretboardDrawsTheTouch`). Owner gaps: gui-integration §19 row and the midi-export NOTE `touch_fret`/`partial` fields are deferred, and the HR-18 budget was relaxed from 0.05 to 1.0 units (amended in the spec's build notes).

| Req | Summary | Engine location | GUI location | Test | Status |
|---|---|---|---|---|---|
| HR-1 (§intro 1, 0.2, 4.1) | Stopped length sets pitch: natural = partial n of open string (fixes octave-high bug); `NoteOnEvent::touchFret` | (branch) `MidiInterpreter::emitVoicedNote` | n/a | (branch) `HarmonicRealism.HR01_naturalHarmonicPitch` | OWNED |
| HR-2 (§intro 2, 0.1, 3) | Harmonic kind renders as ordinary pluck; band isolation only as fallback; `t60Scale *= 0.55` removed | (branch) `Excitation` Harmonic kind, `Params::isolateHarmonic` | n/a | (branch) `HR02_HR03_fundamentalSuppressedAndNotASine` | OWNED |
| HR-3 (§1 eq 1-3) | n-tap node comb in the loop at the stretched partial | (branch) `StringEngine::applyContacts`, `getPartialFrequency` | n/a | (branch) `HR01`, `HR04_fret7`, `HR12_randomContactsAreStable` | OWNED |
| HR-4 (§0.3, 1) | Node efficiency e(d,w), n search 2..8 maximising e/sqrt(n), off-node < 0.05 (owner: pad profile, 0.1) | (branch) `DSP/String/Harmonics.h:findNode/nodeEfficiency` | n/a | (branch) `HR06_aMissedTouchIsADeadThud`, `HR07_fingerWidthIsTheTolerance` | OWNED |
| HR-5 (§0.5) | Just intonation kept | (branch) as HR-3 | n/a | (branch) `HR05_justIntonationIsKept` | OWNED |
| HR-6 (§2) | `Contact` struct, kMaxContacts 4, addContact/clearContact/clearAllContacts; idle path skipped | (branch) `StringEngine::addContact/...` | n/a | (branch) `HR12`, `HR16_contactFreeIsBitIdentical` | OWNED |
| HR-7 (§2) | g ramps over 1 ms | (branch) `contactRampStep` | n/a | (branch) `HR13_landingAndLiftingAreClickFree` | OWNED |
| HR-8 (§2) | M, n recomputed on needsLoopUpdate only | (branch) `updateContactSpacing` | n/a | (branch) `HR18_realtime` | OWNED |
| HR-9 (§2) | Fallback to band isolation when loop too short, counted by validator | (branch) `canRealiseContact`, `Validator::reportHarmonicFallback` | n/a | (branch) `HR15_fallbackIsCounted` | OWNED |
| HR-10 (§2) | `setHarmonicRestriction` kept as shim | (branch) `StringEngine::setHarmonicRestriction` | n/a | (branch) build (callers compile) | OWNED |
| HR-11 (§2) | couplingReceptivity 0.35 while touching | (branch) `StringEngine::updateReceptivity` | n/a | (branch) code review only | OWNED |
| HR-12 (§2) | `processSample(coupling, direct)` split; scrape/slap/tap use directInput | (branch) `StringEngine::processSample` | n/a | (branch) `HR16`, `Scrape.*` | OWNED |
| HR-13 (§3) | Natural / artificial harmonics via contact | (branch) `LuthierEngine::applyHarmonicContact` | n/a | (branch) `HR01`-`HR09` | OWNED |
| HR-14 (§3) | Plucking at a node kills the harmonic | (branch) `Params::exactPluckComb` | n/a | (branch) `HR08_pluckingTheNodeKillsIt` | OWNED |
| HR-15 (§3) | Pinch: node nearest pick + thumb offset, CC 70 sweeps; no velocity partial | (branch) `applyHarmonicContact`, `TechniqueEngine::decide` | n/a | (branch) `HR10_pinchHarmonic` | OWNED |
| HR-16 (§3) | Tapped harmonic: legato Kind::Tap impulse, no voice steal | (branch) `applyHarmonicContact` (tapped) | n/a | (branch) `HR11_tappedHarmonicConvertsTheRingingString` | OWNED |
| HR-17 (§3) | Artificial offsets map to partials (12->2, 7->3, 5->4, 4->5, 19->3, 24->4) | (branch) `harmonics::offsetFretsForChoice` | (branch) HARMONICS offset tooltips | (branch) `HR09_artificialHarmonics` | OWNED |
| HR-18 (§4.1) | Analytic node search replaces `harmonicPartialForFret` table (3.86, 8.84, 15.86 hit) | (branch) `harmonics::partialForFret` | n/a | (branch) `Technique.harmonicNodesAreDetected`, `HR05` | OWNED |
| HR-19 (§4.2) | Sounding-pitch mapping via `HarmonicLocator::find` | (branch) `harmonics::locate`, `MidiInterpreter::emitSoundingHarmonic` | (branch) HARMONICS note-mapping box | (branch) `HR14_soundingPitchLocator` | OWNED |
| HR-20 (§5) | 8 params (touch pressure, finger width, touch time, brief touch, thumb offset, artificial/tapped offset, note mapping); pick family rows | (branch) `Parameters.*` REALISM-B block, `PhysicalRange.cpp` | (branch) `UI/HarmonicsGroup` | (branch) `HR17_rangesAreRegistered` | OWNED |
| HR-21 (§6) | Existing CC 72 pinch / CC 73 natural / velocity trigger unchanged | `MidiInterpreter` CC map, `TechniqueEngine::setPinchHarmonicTrigger` | n/a (MIDI) | `Technique.controllersTakePriorityOverInference`, `Technique.harmonicNodesAreDetected` | DONE |
| HR-22 (§6) | CC 103 ArtificialHarmonic, CC 104 TappedHarmonic; decide() priority order | (branch) `MidiInterpreter::resetCcMapToDefaults`, `TechniqueEngine::decide` | n/a (MIDI) | (branch) `HR19_FA17_luthierExportRoundTrip`, `Technique.controllersTakePriorityOverInference` | OWNED |
| HR-23 (§7) | CHARACTER HARMONICS row in PICK with partial tooltips | (branch) n/a | (branch) `UI/HarmonicsGroup.*` after PICK in `CharacterPanel` | (branch) `RealismBUi.theCharacterTabCarriesTheThreeGroups` | OWNED |
| HR-24 (§7) | Fretboard hollow ring at contact, fading; dashed when missed | (branch) `getContactDisplay` | (branch) `UI/FretboardRealismB.cpp` | (branch) `RealismBUi.theFretboardDrawsTheTouch` | OWNED |
| HR-25 (§7) | gui-integration.md §19 row "Harmonic contact / offsets / mapping" — owner defers | n/a | n/a (doc) | - | MISSING |
| HR-26 (§7) | Preset: plain APVTS params | (branch) APVTS | n/a | (branch) `FingerstyleAttack.FA17_rangesRealtimeRoundTrip` (same mechanism) | OWNED |
| HR-27 (§7) | midi-export NOTE gains `touch_fret` + `partial` — owner defers | none | n/a | - | MISSING |
| HR-28 (§8) | No allocation, reset clears contacts, budget (spec 0.05, owner 1.0 units) | (branch) `StringEngine::reset`, `resetRealismB` | n/a | (branch) `HR18_realtime` | OWNED |
| HR-29 (§9 HR-01) | Natural harmonic pitch | (branch) | n/a | (branch) `HR01_naturalHarmonicPitch` | OWNED |
| HR-30 (§9 HR-02/03) | Fundamental suppressed; not a sine | (branch) | n/a | (branch) `HR02_HR03_fundamentalSuppressedAndNotASine` | OWNED |
| HR-31 (§9 HR-04) | Fret 7 | (branch) | n/a | (branch) `HR04_fret7` | OWNED |
| HR-32 (§9 HR-05) | Just intonation | (branch) | n/a | (branch) `HR05_justIntonationIsKept` | OWNED |
| HR-33 (§9 HR-06) | Missed touch | (branch) | n/a | (branch) `HR06_aMissedTouchIsADeadThud` | OWNED |
| HR-34 (§9 HR-07) | Finger width | (branch) | n/a | (branch) `HR07_fingerWidthIsTheTolerance` | OWNED |
| HR-35 (§9 HR-08) | Plucking the node | (branch) | n/a | (branch) `HR08_pluckingTheNodeKillsIt` | OWNED |
| HR-36 (§9 HR-09) | Artificial | (branch) | n/a | (branch) `HR09_artificialHarmonics` | OWNED |
| HR-37 (§9 HR-10) | Pinch | (branch) | n/a | (branch) `HR10_pinchHarmonic` | OWNED |
| HR-38 (§9 HR-11) | Tapped | (branch) | n/a | (branch) `HR11_tappedHarmonicConvertsTheRingingString` | OWNED |
| HR-39 (§9 HR-12) | Stability | (branch) | n/a | (branch) `HR12_randomContactsAreStable` | OWNED |
| HR-40 (§9 HR-13) | Click-free | (branch) | n/a | (branch) `HR13_landingAndLiftingAreClickFree` | OWNED |
| HR-41 (§9 HR-14) | Sounding-pitch mapping | (branch) | n/a | (branch) `HR14_soundingPitchLocator` | OWNED |
| HR-42 (§9 HR-15) | Fallback counted | (branch) | n/a | (branch) `HR15_fallbackIsCounted` | OWNED |
| HR-43 (§9 HR-16) | Contact-free bit-identical | (branch) | n/a | (branch) `HR16_contactFreeIsBitIdentical` | OWNED |
| HR-44 (§9 HR-17) | Ranges | (branch) | n/a | (branch) `HR17_rangesAreRegistered` | OWNED |
| HR-45 (§9 HR-18) | Realtime (budget relaxed to 1.0 units) | (branch) | n/a | (branch) `HR18_realtime` | OWNED |
| HR-46 (§9 HR-19) | Export round trip | (branch) | n/a | (branch) `HR19_FA17_luthierExportRoundTrip` | OWNED |
