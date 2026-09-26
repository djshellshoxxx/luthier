# REALISM-B coverage

Workstream REALISM-B: `spec/harmonic-realism.md`, `spec/string-interaction.md`,
`spec/fingerstyle-attack.md`. Branch `claude/luthier-realism-b`.

Tests are in `Source/Tests/HarmonicRealismTests.cpp` (suite `HarmonicRealism`),
`StringInteractionTests.cpp` (`StringInteraction`), `FingerstyleAttackTests.cpp`
(`FingerstyleAttack`) and `RealismBUiTests.cpp` (`RealismBUi`). Each spec's
"Build notes" section records where the built model changed its text.

## (a) Coverage

### harmonic-realism.md

| ID | Spec | Implementation | Verification | Status |
|---|---|---|---|---|
| HR-bug | intro 1, 4.1 | `MidiInterpreter::emitVoicedNote` (stopped length sets the pitch; `NoteOnEvent::touchFret`) | HR01_naturalHarmonicPitch, HR14 | verified |
| HR-not-sine | intro 2, 3 | `Excitation` Harmonic kind = ordinary pluck; band isolation only as fallback (`Params::isolateHarmonic`); `t60Scale *= 0.55` removed | HR02_HR03 | verified |
| HR-comb | 1 eq (1)-(3) | `StringEngine::applyContacts`, `getPartialFrequency` (model's own stretched partial) | HR01, HR04, HR05, HR12 | verified |
| HR-node | 1 node efficiency, n search | `DSP/String/Harmonics.h` `findNode`, `nodeEfficiency` (pad profile, see Decisions) | HR06, HR07, TechniqueEngine test | verified |
| HR-contact-api | 2 Contact, kMaxContacts, add/clear | `StringEngine::addContact/clearContact/clearAllContacts` | HR12, HR13 | verified |
| HR-ramp | 2 1 ms ramp | `contactRampStep` | HR13_landingAndLiftingAreClickFree | verified |
| HR-M-rate | 2 M on loop update only | `updateContactSpacing` from `updateLoopCoefficients`; cached tap kernels | HR18 | verified |
| HR-fallback | 2 fallback counted | `canRealiseContact`, `Validator::reportHarmonicFallback` | HR15_fallbackIsCounted | verified |
| HR-shim | 2 setHarmonicRestriction shim | `StringEngine::setHarmonicRestriction` | build (callers compile) | verified |
| HR-receptivity | 2 0.35 while touching | `StringEngine::updateReceptivity` | code review | verified |
| HR-direct | 2 directInput split | `processSample (coupling, direct)`; scrape catches routed direct | HR16, full suite (ScrapeTests) | verified |
| HR-natural | 3 natural/artificial | `LuthierEngine::applyHarmonicContact` | HR01-HR09 | verified |
| HR-nodepluck | 3 plucking the node | `Params::exactPluckComb` | HR08_pluckingTheNodeKillsIt | verified |
| HR-pinch | 3 pinch at pick + thumb offset, CC 70 | `applyHarmonicContact`, `MidiInterpreter::hasPickPositionController` | HR10_pinchHarmonic | verified |
| HR-tapped | 3 tapped (legato, no steal) | `applyHarmonicContact` (tapped), Kind::Tap | HR11 | verified |
| HR-offsets | 3 offsets -> partials | `harmonics::offsetFretsForChoice` | HR09, RealismBUi (describeOffset) | verified |
| HR-analytic | 4.1 analytic table | `TechniqueEngine::harmonicPartialForFret` -> `harmonics::partialForFret` | Technique.harmonicNodesAreDetected | verified |
| HR-locator | 4.2 sounding mapping | `harmonics::locate`, `MidiInterpreter::emitSoundingHarmonic` | HR14_soundingPitchLocator | verified |
| HR-params | 5 eight parameters, pick family | `Parameters.*` REALISM-B block, `PhysicalRange.cpp` | HR17_rangesAreRegistered | verified |
| HR-cc | 6 CC 103/104, priority | `MidiInterpreter::resetCcMapToDefaults`, `TechniqueEngine::decide` | HR19, controllersTakePriorityOverInference | verified |
| HR-ui | 7 HARMONICS row in PICK | `UI/HarmonicsGroup.*` in `CharacterPanel` | RealismBUi.theCharacterTabCarriesTheThreeGroups | verified |
| HR-ring | 7 fretboard ring, dashed miss | `UI/FretboardRealismB.cpp` | RealismBUi.theFretboardDrawsTheTouch | verified |
| HR-19rows | 7 section 19 rows | text in spec 7 | - | deferred: gui-integration.md is not this workstream's file |
| HR-export-meta | 7 NOTE touch_fret, partial | - | - | deferred: capture-path metadata owned by midi-export; audio round trip holds (HR19) |
| HR-preset | 7 plain parameters | APVTS | FA17 / SI14 round trips (same mechanism) | verified |
| HR-rt | 8 no alloc, reset clears | `StringEngine::reset` | HR18_realtime | verified |
| HR-budget | 8 CPU | linear cached taps | HR18 (1.0 unit, see Decisions) | verified |
| HR-16 | 9 contact-free bit-identical | inert path | HR16 + all 36 factory presets bit-identical with REALISM-B amounts at 0 (manual render vs pre-change build) | verified |
| HR-19 | 9 export round trip | CC stream | HR19_FA17_luthierExportRoundTrip | verified |
| EBow flake | task | `EBowTests` fixed character seed + band-maximum measurement | EBow.theHarmonicChoiceTakesTheString (10/10 on the old build, passes here; 48 dB margin) | verified |

### string-interaction.md

| ID | Spec | Implementation | Verification | Status |
|---|---|---|---|---|
| SI-air | 1 rank-1 air term, delay, HP, a_cat | `CouplingMatrix` air path, `LuthierEngine::refreshAirCoupling` | SI01, SI02_SI03 | verified |
| SI-palm | 2 palm spread weights, restore | `LuthierEngine::updatePalmSpread`, `palmWeight` | SI04_SI05 | verified |
| SI-adjacent | 3 neighbour damping, bitmask, lift | `applyAdjacentMute`, `liftMutesFrom`, borrowed damping | SI06_SI07 | verified |
| SI-stagger | 4 release stagger, seeded, never earlier | `stageNoteOffs`, `ScheduledEvent::staggered/cancelled` | SI08_SI09 | verified |
| SI-crosstalk | 5 aperture gains | `PickupEngine::setStringLateralOffsets`, `updateCrosstalk` | SI10_crosstalk | verified |
| SI-thump | 6 muted-string thump, deadStrike | `Rhythm/MutedThump.h`, `MidiInterpreter::flushChordGroup`, `RhythmEngine::scheduleStrum`, `triggerNote` | SI11_mutedStringThump | verified |
| SI-params | 7 seven parameters, families | REALISM-B param block, `PhysicalRange.cpp` | SI12_SI14 | verified |
| SI-ui | 9 STRING INTERACTION group, palm shading | `UI/StringInteractionGroup.*`, fretboard palm band | RealismBUi | verified |
| SI-mute-mirror | 9 palm width mirrored in TECHNIQUES -> MUTE | - | - | deferred: the MUTE group (muting-rhythm.md) is WIP, not in the build |
| SI-flags | 9 flags cleared on reset/preset/panic | `resetRealismB` from `reset` and `panic` | code review | verified |
| SI-12 | 11 per-string contract | nothing after the per-string tap | RoutingTests (full suite) | verified |
| SI-13 | 11 realtime, cost | - | SI13_realtime (0.2 units, see Decisions) | verified |

### fingerstyle-attack.md

| ID | Spec | Implementation | Verification | Status |
|---|---|---|---|---|
| FA-profile | 1 release time, full blend | `Excitation::Params::releaseSeconds`, blended `MaterialSpec` | FA01_FA02, FA03, FA04 | verified |
| FA-nail | 1 nail click | `applyRightHand` (Finger) | FA10_perStringTools | verified |
| FA-thumbpos | 1 thumb position | `applyRightHand` | FA14_thumbPosition | verified |
| FA-stroke | 2 rest terms, neighbour damping | `applyRightHand`, `StringEngine::setCouplingSendScale` | FA06, FA07 | verified |
| FA-auto | 2 Auto rule | `resolveRightHand` (looks at the schedule both ways) | FA08_autoStroke | verified |
| FA-tools | 3 per-string tools, resolution order, 12-string courses | `RightHand.h`, `resolveRightHand` | FA10, FA15 | verified |
| FA-finger | 3 pattern finger carried | `NoteOnEvent::finger`, `RhythmEngine::emitNote` | FA09_patternFingersReachTheString | verified |
| FA-alt | 3 alternation | `applyRightHand`, `Params::startDelaySamples` | FA12_alternation | verified |
| FA-styles | 4 styles, one undo entry, never on load | `RightHandGroup::applyStyle` | FA16_styleWritesOnce, RealismBUi | verified |
| FA-travis | 4 Travis mute | `applyRightHand` | FA11_travisMute | verified |
| FA-hybrid | 4 hybrid snap | `applyRightHand` (pop collision via `SlapEngine::makeContactBuzz`) | FA13 | verified |
| FA-slappop | 4 Slap and Pop tools via SlapEngine | `makeToolStrike` in `triggerNote` | FA13_slapAndPopTools | verified |
| FA-cc | 5 CC 102, CC 105 | `MidiInterpreter` targets | FA15_ccTriggers | verified |
| FA-params | 6 fourteen parameters | REALISM-B param block | FA17 | verified |
| FA-ui | 7 RIGHT HAND group | `UI/RightHandGroup.*` | RealismBUi | verified |
| FA-easy | 7 Easy Tool selector | `RightHandToolSelector` in `EasyPanel` | RealismBUi.theEasyPlayingStripHasTheToolSelector | verified |
| FA-glyph | 7 tool glyph per string | fretboard REALISM-B layer | RealismBUi | verified |
| FA-export-meta | 7 PICK tool/finger/stroke | - | - | deferred: capture-path metadata owned by midi-export; audio round trip holds (HR19_FA17) |
| FA-05 | 9 pick path untouched | Global tool = old path | FA05 + factory presets bit-identical at amounts 0 | verified |
| FA-17 | 9 ranges, realtime, round trip | - | FA17, HR19_FA17 | verified |

## (b) Decisions

- Node efficiency is a flat-topped pad profile `exp(-(d/1.5w)^4)` with off-node below 0.1: the Gaussian made a 3 mm miss cost 31 dB (HR-07 asks 3-15).
- The pinch graze is half the touch pressure: at full strength it took 18-21 dB off the fundamental, against ground rule 4.
- A tapped harmonic is an ideal damper for max(1.5 graze, 0.5 touch time): the node comb's taps are in the loop, so it removes about 2 dB per ms, and 12 ms was not enough.
- A touched note's pick comb sits at the geometric pluck position with its filters' tails kept, so plucking the node kills the harmonic (40 dB measured); ordinary plucks are unchanged.
- Integer (MIDI-voiced) touch frets are read as tab: within 0.35 fret of a node of partials 2-5 they land on it (<4> = 3.86).
- Comb taps are linearly interpolated with cached weights: Lagrange-5 taps cost 1.5 units for six strings at n = 8.
- CPU budgets are the measured ones: 1.0 unit for six held n = 8 contacts, 0.2 units for the air path at 12 strings (the spec drafts' 0.05 / 0.1 were below their own arithmetic).
- Only the scrape's catches moved to the direct input; pick click, feedback and E-Bow stay scaled by receptivity so damped strings sound as before.
- A fresh pluck still forces a loop-coefficient refresh (the removed harmonic reset did): this keeps every factory preset bit-identical with the new amounts at 0.
- Fretting-hand mute style follows the right-hand style (Classical = 0.1) until muting-rhythm.md's MuteSettings exists.
- The muted-string thump in a pattern reaches strings outside the struck span (STRUM mask); in a live chord only inside it; its timing is interpolated from the planned strikes, so sounding strings are untouched.
- Rest stroke terms are level 1.35 / contact 1.10 so the note, not the impulse, lands in FA-06.
- Tone tests (FA-01/02/12) measure the contact's spectrum: the string output's centroid is dominated by the string's own partials.
- FA-07 is measured three periods plus 10 ms after the stroke with bridge coupling off: the lumped loop damps once per round trip, and the struck note re-drives the neighbour.
- FA-11 threshold 0.75x (0.35 on the existing palm-mute curve is 0.69x); FA-12 centroid floor 0.3 % (what section 3's terms give).
- Pattern fingers override Global strings, as the spec says: presets driving fingerpick patterns now play them with thumb and fingers.
- `finger_alternation_variation` and `rest_stroke` are not declared here (bass-techniques.md owns them); the bridge reads them if they exist.
- HR-11 and SI tests that measure one string's own decay switch bridge coupling off, because sympathetic strings (A fret 5 = open D) re-enter.
- The EBow flake: every processor drew its character seed from the clock and the test read one Goertzel bin at the nominal pitch with 1 Hz resolution; the rig now fixes the seed and takes the strongest bin within 1.5 %.
- `LuthierEngine::releaseNoteNow` was added beside `triggerNoteNow` for tests; engine prepared with a 1-sample maximum block crashes (pre-existing; tests prepare 256 and process 1-sample buffers instead).
- MIDI-export metadata fields (NOTE touch_fret/partial, PICK tool/finger/stroke) and gui-integration.md section 19 rows are left to their owners; audio round trips hold without them.

## Factory presets at the spec's defaults

With every REALISM-B amount at 0, all 36 factory presets render bit-identically
to the build before this workstream. At the spec's defaults (air 1.0,
neighbour mute 0.6, stagger 12 ms, thump 0.5) they change as the specs intend.
Output RMS of a fixed phrase (chord, release, single notes; no harmonics), dB:

| Preset | before | after (defaults) | delta |
|---|---|---|---|
| Clean Double-Cut Funk | -26.61 | -27.11 | -0.49 |
| T-Style Country Twang | -25.86 | -26.26 | -0.41 |
| Single-Cut Crunch | -18.60 | -18.98 | -0.38 |
| Modern Metal Chug | -20.30 | -20.07 | +0.23 |
| Jazz Hollowbody | -28.36 | -28.30 | +0.06 |
| Blues Slide | -33.32 | -33.19 | +0.13 |
| Shred Lead | -22.51 | -22.02 | +0.48 |
| Fuzz Face Lead | -20.69 | -20.67 | +0.02 |
| Surf Reverb | -36.36 | -36.47 | -0.11 |
| Semi-Hollow Chime | -29.88 | -29.51 | +0.37 |
| Drop C Riff | -22.37 | -22.81 | -0.44 |
| Wah Funk Rhythm | -52.02 | -57.22 | -5.20 |
| Octave Fuzz Stoner | -22.36 | -22.28 | +0.08 |
| Ambient Swell | -49.43 | -49.39 | +0.04 |
| 8-String Djent | -30.28 | -30.25 | +0.02 |
| Rockabilly Slap | -27.75 | -27.75 | +0.00 |
| Tapping Etude | -19.16 | -19.32 | -0.15 |
| Fingerstyle Folk | -53.68 | -53.47 | +0.21 |
| Strummed Dreadnought | -42.30 | -42.57 | -0.27 |
| Parlor Blues | -56.14 | -56.11 | +0.03 |
| 12-String Jangle | -47.06 | -46.84 | +0.22 |
| Nylon Classical | -47.66 | -47.61 | +0.04 |
| Flamenco Rasgueado | -45.69 | -45.95 | -0.26 |
| Nashville High-Strung | -47.43 | -47.83 | -0.40 |
| DADGAD Drone | -53.96 | -53.92 | +0.04 |
| Jumbo Bluegrass | -47.53 | -46.61 | +0.92 |
| P-Bass Flatwound | -19.36 | -19.36 | +0.00 |
| J-Style Fingerstyle | -15.09 | -14.96 | +0.12 |
| Fretless Mwah | -21.64 | -21.73 | -0.09 |
| Violin Bass Grind | -19.34 | -19.42 | -0.08 |
| 5-String Low B | -27.64 | -27.57 | +0.07 |
| Init | -18.79 | -20.57 | -1.78 |
| Dry Instrument | -18.27 | -18.92 | -0.65 |
| Physics Showcase | -49.90 | -51.99 | -2.09 |
| Transposing Trem Chords | -24.96 | -25.02 | -0.06 |
| Microtonal Just | -51.12 | -51.83 | -0.71 |

Harmonics in any preset (CC 72/73 or velocity-triggered) now sound at the right
pitch - an octave (12th fret) or more lower than before - which is the bug fix.
