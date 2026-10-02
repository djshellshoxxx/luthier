# TECHNIQUES workstream coverage

Phase 5b technique specs: `muting-rhythm.md`, `two-hand-tapping.md`,
`microtonal-bends.md`, `slide-technique-controls.md`, `technique-cascade.md`,
the remaining parts of `engine-technique-layer.md`, and
`gui-techniques-updates.md`. Branch `claude/luthier-techniques`.

Test names are `Suite.test` in `LuthierTests` (files `Source/Tests/MutingTests.cpp`,
`TapTests.cpp`, `BendTests.cpp`, `SlideTechniqueTests.cpp`, `CascadeTests.cpp`,
`TechniquesUiTests.cpp`).

## (a) Coverage

### muting-rhythm.md

| ID | Spec | Implementation | Verification | Status |
|---|---|---|---|---|
| MR-1 | 1 mute types (open, palm light/heavy/extreme, ghost, chuka, fret mute), each with position/pressure defaults | `Source/Rhythm/Muting.*` (`MuteType`, `Muting::dampingFor`), `StringEngine::setMutedDamping` (`Damping::Muted`) | `Muting.typesRoundTripThroughTheirIds`, `Muting.eachTypeDampsAsDescribed` | verified |
| MR-2 | 2 `mute_type` per pattern step, JSON extension, unknown ids open | `RhythmPattern::getMuteStep/setMuteStep`, `toVar/fromVar` (`mute_type` per step + `mute_steps`) | `Muting.theMuteGridsRoundTrip` | verified |
| MR-3 | 2 live 16-step Mute Grid synced to host tempo | `MuteEngine` live grid (atomics), `MuteEngine::apply` (ppq -> sixteenth) | `Muting.paintingTheLiveGridAppliesWithinABar` | verified |
| MR-4 | 3 master mute mode overrides all | `Muting::resolve`, `MuteEngine::apply` | `Muting.theMasterModeOverridesEverything`, `Muting.resolutionOrderIsMasterThenStepThenChuka` | verified |
| MR-5 | 3 palm position / pressure | `Muting::palmFactor`, params `mute_palm_position/pressure` | `Muting.eachTypeDampsAsDescribed` | verified |
| MR-6 | 3 fretting-hand style (rock spread / classical) | `MuteEngine::deadensOtherStrings`, `LuthierEngine::techniqueStrike` | `Muting.rockSpreadDeadensTheStringsAMutedStrumMisses` | verified |
| MR-7 | 3 chuka source (soft strums < 0.3) | `Muting::resolve`, `MuteEngine::apply` (chuck from strum-dynamics 6.1) | `Muting.aSoftStrumIsAChuka` | verified |
| MR-8 | 3 random humanise | `Muting::humanise` | `Muting.humaniseShiftsAboutHalfTheEligibleSteps` | verified |
| MR-9 | 3 ghost velocity range | `MuteEngine::apply` | `Muting.eachTypeDampsAsDescribed`, `Muting.aGhostNoteHasNoPitchedContent` | verified |
| MR-10 | 4 RhythmEngine reads mute_type per step and passes it with each note-on | `RhythmEngine::emitNote` (`pendingMute`), `NoteOnEvent::muteType` | `Muting.aPatternsMuteRowReachesItsNotes`, `Muting.existingPatternsPlayIdentically` | verified |
| MR-11 | 4 StringEngine applies initial damping and post-strike release | `LuthierEngine::techniqueStrike`, fret-mute release in `techniqueBeginBlock` | `Muting.palmMuteHeavyOnLowEDecaysIn40To60Ms`, `Muting.aFretMuteRingsThenStops` | verified |
| MR-12 | 5 cascade: mute stacks with everything | `CascadeResolver` table (Mute row/column compatible) | `Cascade.theMatrixIsTheSpecs`, `Muting.aMuteIsStampedOnAnyTechnique`, `Muting.aSlappedNoteCarriesItsMute` | verified |
| MR-13 | 6 presets (Metal Chug 16ths, Funk Chuka, Reggae Skank, Country Boom-Chick, Metal Gallop, Classical Staccato) | grid presets in `Muting.cpp`; factory presets in `Source/Presets/TechniquePresets.cpp` | `Muting.theMuteControlsDriveTheModel`, `TechniquesUi.thePresetChipFilters` | verified |
| MR-14 | 7 MUTE sub-tab (full 16-step editor and controls) | `Source/UI/MuteGroup.*`, `MutePage` | `Muting.theMuteControlsDriveTheModel`, `TechniquesUi.everySubTabRendersItsControls` | verified |
| MR-15 | 7 Easy 4-way Mute button (Off/Light/Heavy/Extreme) | `EasyMuteButton` in `TechniquePillRow` | `Muting.theEasyMuteButtonCyclesFourWays` | verified |
| MR-16 | 7 RHYTHM tab Mute Row | `RhythmPanel` `muteRow` (`MuteGridEditor`) | `Muting.theMuteControlsDriveTheModel` | verified |
| MR-17 | 8 MIDI export of mute types (SysEx NOTE field, generic text meta) | - | - | deferred: the capture has no technique path from the engine yet (it records string activity only; the slap is not exported either). Belongs with midi-export; NOTE's tagged fields take a `mute=` field without a schema bump. |
| MR-18 | 9 tests | `MutingTests.cpp` | all `Muting.*` | verified |

### two-hand-tapping.md

| ID | Spec | Implementation | Verification | Status |
|---|---|---|---|---|
| TH-1 | 1 TapGesture (string, fret, strength, hand, pull-off after, target, duration) | `TapGesture` in `Source/DSP/Techniques/TapEngine.h` | `Tap.fretSnapOffTapsBetweenTheFrets`, `Tap.twoTapsOnAStringAreCaposInSeries` | verified |
| TH-2 | 2 tap-on / tap-hold (movable capo) / tap-off | `TapEngine`, `LuthierEngine::playTapEvent`, `techniqueFret` | `Tap.aTapIsAMovableCapoAndLiftsBackToTheFrettedNote` | verified |
| TH-3 | 2 pull-off lateral flick | `TapEngine::tapOff`, `playTapEvent` (PullOff excitation) | `Tap.thePullOffFlickIsTheReleaseTransient` | verified |
| TH-4 | 2 multi-finger (concurrent taps) | `TapEngine` per-string list, `soundingFret` | `Tap.twoTapsOnAStringAreCaposInSeries` | verified |
| TH-5 | 3 trigger source (MIDI ch 2 / keyswitch / fretboard) | `TapSettings::triggerConfig`, `TechniqueTriggers` capture role, `TechniqueOverlay::handleMouseDown` | `Tap.theTriggersTakeTheirNotes`, `TechniquesUi.theFretboardTapsWhenTheTapLayerIsOn` | verified |
| TH-6 | 3 strength curve | `TapSettings::strengthFor` | `Tap.theStrengthCurveShapesVelocity` | verified |
| TH-7 | 3 auto pull-off | `TapEngine::tapOff` | `Tap.thePullOffFlickIsTheReleaseTransient` | verified |
| TH-8 | 3 hammer-on threshold / 5 left-hand legato promotion (150 ms) | `TechniqueEngine::setHammerOnWindowMs`, bridge | `Tap.softNotesCloseTogetherAreHammerOns` | verified |
| TH-9 | 3 lateral flick, duration default, max concurrent (advanced to 8), fret snap | params `tap_*`, `PhysicalRange` row for `tap_max_concurrent` | `Tap.everyControlRoundTrips`, `Tap.fretSnapOffTapsBetweenTheFrets`, `Ranges.*` | verified |
| TH-10 | 4 TapEngine interface (trigger, release, processBlock, reset) | `requestGesture`, `requestRelease`, `processBlock`, `reset` | `Tap.*` | verified |
| TH-11 | 6 TAP sub-tab; Playing strip TAP pill; CHARACTER Right Hand Tapping section; fretboard square markers fading 100 ms | `TapPage`, `TechniquePillRow`, `TechniqueMirrors`, `TechniqueOverlay` (tapMarkers) | `TechniquesUi.everySubTabRendersItsControls`, `TechniquesUi.thePillsArmOnAClick`, `TechniquesUi.theCharacterMirrorsAttachTheSameParameters`, `TechniquesUi.theOverlaysDrawInsideTheirBudget` | verified |
| TH-12 | 7 cascade (slide conflict, slap alternate, scrape conflict) | `CascadeResolver`, `playTapEvent` | `Tap.theSlideBarHoldsItsStrings`, `Cascade.aTapPreemptsAScrapeOnItsString`, `Cascade.everyPairResolvesAsDocumented` | verified |
| TH-13 | 8 presets (Standard Two-Hand, Legato Runs, Eight-Finger, Microtonal Tap, Percussive Tap) | `TechniquePresets.cpp` | `TechniquesUi.thePresetChipFilters` | verified (Eight-Finger ships at 4, see Decisions) |
| TH-14 | 9 MIDI export of tap events | - | - | deferred: as MR-17 |
| TH-15 | 10 CPU idle < 0.05 %, busy < 0.8 % | - | `Tap.cpuStaysInBudget` | verified |

### microtonal-bends.md

| ID | Spec | Implementation | Verification | Status |
|---|---|---|---|---|
| MB-1 | 1 source stack: global, per-string, vibrato, pre-bend, slide contribution | `BendEngine::centsFor`; slide stays in `SlideEngine` and adds in the engine | `Bend.theGlobalBendMovesEveryString`, `Bend.aPerStringBendMovesOnlyItsString` | verified |
| MB-2 | 2 global source (pitch bend / expression / CC) and range (advanced to 2400+) | `BendSettings`, `PhysicalRange` row `bend_global_range` | `Bend.theGlobalBendMovesEveryString`, `Bend.everyControlRoundTrips` | verified |
| MB-3 | 2 per-string source (MPE Y, custom CC per string, MPE pitch bend, none) and 6 ranges | `StringBendSource`, `bend_string_range_1..6` | `Bend.aPerStringBendMovesOnlyItsString` | verified |
| MB-4 | 2 vibrato source / rate / depth / onset delay | `VibratoSource`, `BendEngine` | `Bend.vibratoWaitsForItsOnsetThenReachesDepthIn50Ms` | verified |
| MB-5 | 2-3 quantise (quarter-tone, semitone, 22/24/31/53-EDO, custom) with snap strength | `BendEngine::quantiseTarget` | `Bend.quarterToneQuantiseLandsOnTheGrid` | verified |
| MB-6 | 2-3 custom .scl / .tun | `MicrotonalScale` (Scala + AnaMark), BEND page loader | `Bend.aLoadedScaleIsTheGrid` | verified |
| MB-7 | 2 pre-bend (keyswitch or CC, amount, release) | `BendSettings::triggerConfig` (keyswitch 20), `BendEngine::noteOn` | `Bend.aPreBendStartsFlatAndReleases` | verified |
| MB-8 | 2 bend curve / release curve (linear, exponential, drawn) | `BendEngine::shapeBend/shapeRelease`, `BendCurveEditor` | `Bend.theCurvesShapeTheThrow` | verified |
| MB-9 | 4 range change takes effect at next note-on | `BendEngine::noteOn` latches | `Bend.aRangeChangeWaitsForTheNextNote` | verified |
| MB-10 | 4 PreBendEvent modulation source class in the mod matrix | - | - | deferred: the ModMatrix source list is another workstream's; the pre-bend runs in BendEngine directly and is fully usable. |
| MB-11 | 5 BEND sub-tab; Playing strip BEND popover; CHARACTER PLAYING Microtonal section; fretboard bend arc + cents badge | `BendPage`, `TechniquePopover`, `TechniqueMirrors`, `TechniqueOverlay` (bendArc) | `TechniquesUi.*` | verified |
| MB-12 | 6 cascade (bend compatible with all) | `CascadeResolver` | `Cascade.theMatrixIsTheSpecs`, `Bend.aBentSlapPitchesCorrectly` | verified |
| MB-13 | 7 presets (Standard Whole-Tone, Quarter-Tone Blues, Maqam, Wide Vibrato, Whammy-Style Two-Octave) | `TechniquePresets.cpp` | `TechniquesUi.thePresetChipFilters` | verified |
| MB-14 | 8 MIDI export of bend contributions | - | - | deferred: as MR-17 (plain pitch bend capture is unchanged) |
| MB-15 | 9 tests | `BendTests.cpp` | all `Bend.*` | verified |

### slide-technique-controls.md

| ID | Spec | Implementation | Verification | Status |
|---|---|---|---|---|
| SL-1 | 1 position source (mod wheel, pitch bend, MPE Y, expression, CC, fretboard drag) | `SlideControlSettings`, `SlideEngine::advanceControls`, `TechniqueOverlay::handleMouseDrag` | `SlideControls.theModWheelDrivesThePosition`, `SlideControls.theStringFollowsTheControlledBar` | verified |
| SL-2 | 1 position mode absolute / relative | `SlideEngine::controlledBarFret` | `SlideControls.theModWheelDrivesThePosition` | verified |
| SL-3 | 1 slant and pressure sources | `advanceControls` | `SlideControls.everyControlRoundTrips` | verified |
| SL-4 | 1 contact string mask | `SlideEngine::contactsString`, `slideContactMaskFor` | `SlideControls.aBassOnlyBarLeavesTheTrebleFree` | verified |
| SL-5 | 1 speed limit (advanced higher) | `advanceTowards`, `PhysicalRange` row `slide_speed_limit` | `SlideControls.theSpeedLimitClampsAJump` | verified |
| SL-6 | 1 auto-vibrato on hold (> 300 ms) | `advanceControls` | `SlideControls.autoVibratoEngagesAfterAHold` | verified |
| SL-7 | 1-2 gesture trigger and scripted SlideGesture | `SlideGesture`, `triggerGesture`, keyswitch 21 | `SlideControls.aScriptedGestureArrivesOnTime` | verified |
| SL-8 | 3 setPositionSource / triggerGesture / setSpeedLimit | `SlideEngine` | `SlideControls.*` | verified |
| SL-9 | 4 SLIDE sub-tab; Easy slide "…" popover; CHARACTER SLIDE expandable section | `SlidePage`, `TechniquePopover` (SLIDE), `TechniqueMirrors` | `TechniquesUi.*` | verified (popover via the SLIDE pill, see Decisions) |
| SL-10 | 6 presets (Standard, Pitch-Bend, Lap Steel Full Control, Auto-Vibrato Hold) | `TechniquePresets.cpp` | `TechniquesUi.thePresetChipFilters` | verified |
| SL-11 | 7 source swap crossfade 10 ms | smoothing in `advanceControls` | `SlideControls.swappingTheSourceGlides` | verified |

### technique-cascade.md

| ID | Spec | Implementation | Verification | Status |
|---|---|---|---|---|
| TC-1 | 1-2 class table and same-string matrix | `CascadeResolver::relation` | `Cascade.theMatrixIsTheSpecs`, `Cascade.everyPairResolvesAsDocumented` | verified |
| TC-2 | 3 priority rules (user beats auto, most recent, graceful preemption, slide holds, mute/bend always) | `CascadeResolver::request` | `Cascade.thePriorityRulesDecide` | verified |
| TC-3 | 3.3 preemption crossfade 10 ms | existing scrape/slap fades, called from `playTapEvent` | `Cascade.aTapPreemptsAScrapeOnItsString`, `Scrape.aPreemptedScrapeFadesOutInTenMilliseconds` | verified |
| TC-4 | 4 cascade schedule | `LuthierEngine::techniqueBeginBlock`, `TechniqueLayer::stages` | `TechniqueLayer.theModulesRunInTheDocumentedOrder` | verified |
| TC-5 | 5 combined presets | `TechniquePresets.cpp` | `Cascade.theCombinedPresetsArmTheirTechniques` | verified |
| TC-6 | 6 GUI conflict indicator (red slash + tooltip) | `TechniquePill`, `TechniqueTable::conflictFor`, `CascadeResolver::conflictMessage` | `Cascade.aConflictShowsOnThePillWithinAFrame`, `Cascade.theConflictMessageNamesStrings` | verified |
| TC-7 | 7/10 cross-string independence | engine | `Cascade.techniquesOnDifferentStringsAreIndependent` | verified |
| TC-8 | 7 fuzz 10 000 gestures | - | `Cascade.aFuzzOfGesturesLeavesNothingBehind` | verified |
| TC-9 | 7/10 combined preset renders match reference within 0.5 dB | - | `Cascade.theCombinedPresetsArmTheirTechniques` | verified (reference = a second render; see Decisions) |

### engine-technique-layer.md (remaining parts)

| ID | Spec | Implementation | Verification | Status |
|---|---|---|---|---|
| ET-1 | 1 TapEngine, MuteEngine, CascadeResolver modules | `Source/DSP/Techniques/*`, `TechniqueLayer` | suites above | verified |
| ET-2 | 2 insertion points 2b/2c | `LuthierEngine.cpp` call sites, `LuthierEngineTechniques.cpp` | `TechniqueLayer.theModulesRunInTheDocumentedOrder` | verified |
| ET-3 | 3.1/3.6 TechniqueId + keyswitch table extended (tap 19, pre-bend 20, slide gesture 21) | `TechniqueTriggers.h/.cpp` (capture-under-keyswitch) | `Tap.theTriggersTakeTheirNotes`, `Bend.aPreBendStartsFlatAndReleases`, `SlideControls.aScriptedGestureArrivesOnTime` | verified |
| ET-4 | 3.2 N concurrent contact points | `TapEngine::soundingFret` (taps in series) | `Tap.twoTapsOnAStringAreCaposInSeries` | verified |
| ET-5 | 3.3 RhythmEngine mute_type | as MR-10 | | verified |
| ET-6 | 3.5 ModulationMatrix PreBendEvent / microtonal range factor | - | - | deferred: as MB-10 |
| ET-7 | 4 command / result queues | settings setters (atomic hand-over as slap/scrape), `TapEngine::requestGesture` lock-free queue, `TechniqueTriggers::request`, fire counters and published cascade masks for the UI | `TechniqueLayer.commandsDoNotAllocate` | verified |
| ET-8 | 5 parameters (arm booleans, per-technique controls, ~60) | 64 appended params (Parameters.* TECHNIQUES block) | `Integration.*` param count, round-trip tests | verified |
| ET-9 | 6 migration: old presets load disarmed/default; old bass-slap presets | `onTechniquesBlockLoaded` resets | `TechniqueLayer.preDeltaPresetsLoadDisarmedAndIdle`, `TechniqueLayer.bassSlapPresetsStillReachTheSlapEngine` | verified |
| ET-10 | 7 technique state inside PRESET (grid, scale, curve) | `TechniqueLayer::toVar/fromVar`, `PresetManager::captureTechniquesBlock` | `Muting.theMuteGridsRoundTrip`, `Bend.aLoadedScaleIsTheGrid` | verified |
| ET-11 | 7 snapshot bank captures technique arm state | arm states are parameters, which snapshots capture | existing snapshot tests | verified |
| ET-12 | 8 undo classes technique-arm / technique-param (200 ms) / mute-grid-paint (200 ms); gestures not undoable | `TechniqueUndo` | `TechniquesUi.theUndoClassesGroupAsSpecified`, `TechniquesUi.thePillsArmOnAClick` | verified |
| ET-13 | 9 performance (idle negligible; low-CPU class banner) | idle paths are flag tests | `Tap.cpuStaysInBudget` | verified; low-CPU-class banner deferred (no CPU-class detection exists in the build) |
| ET-14 | 10 no-allocation commands, pipeline order, pre-delta presets, migration | | `TechniqueLayer.*` | verified |

### gui-techniques-updates.md

| ID | Spec | Implementation | Verification | Status |
|---|---|---|---|---|
| GT-1 | 0.2 TECHNIQUES tab between CONTROLLERS and HELP | `AdvancedPanel::buildWorkspace` | `TechniquesUi.theTabAndItsRail`, `EditorTests`, `HelpTab.*` | verified |
| GT-2 | 1 vertical sub-tab rail SCRAPE/SLIDE/SLAP/MUTE/TAP/BEND/CASCADE, arm pill on each | `TechniquesPanel` | `TechniquesUi.theTabAndItsRail`, `TechniquesUi.everySubTabRendersItsControls` | verified |
| GT-3 | 1 SCRAPE and SLAP expose existing scrape_* / slap_* / pop_* / ghost_* / double_thump_* | `ScrapePage`, `SlapPage` | `TechniquesUi.everySubTabRendersItsControls` | verified |
| GT-4 | 2 Easy pill row: tap arms, hold popover (Escape), right-click sub-tab; firing dot | `TechniquePillRow`, `TechniquePill`, `TechniquePopover`, editor wiring | `TechniquesUi.thePillsArmOnAClick`, `TechniquesUi.holdOpensThePopoverAndEscapeClosesIt`, `TechniquesUi.rightClickOpensTheSubTabInAdvanced` | verified |
| GT-5 | 4 fretboard overlays (scrape trail, tap markers, mute zone, bend arc, slap flash), toggleable, < 2 ms | `TechniqueOverlay` | `TechniquesUi.theOverlaysDrawInsideTheirBudget` | verified |
| GT-6 | 5 CHARACTER Right Hand Tapping / PLAYING Microtonal | `TechniqueMirrors` | `TechniquesUi.theCharacterMirrorsAttachTheSameParameters` | verified |
| GT-7 | 6 RHYTHM Mute Row | `RhythmPanel` | `Muting.theMuteControlsDriveTheModel` | verified |
| GT-8 | 7 preset browser "Uses Techniques" chip, multi-select | `PresetBrowserPanel`, `PresetInfo::armedTechniques` | `TechniquesUi.thePresetChipFilters` | verified |
| GT-9 | 8 onboarding stop (hook) | `TechniquesPanel::kOnboardingAnchor`, `kOnboardingCascadeAnchor` | `TechniquesUi.theOnboardingAnchorsAreThere` | verified (tour itself: TUNE-HELP-ONBOARDING) |
| GT-10 | 9 accessibility (labels "X technique, armed", Tab/Space/Enter, not colour only, reduced motion) | `TechniquePill` (toggleButton handler, check glyph), overlay fades | `TechniquesUi.thePillsArmOnAClick`, `TechniquesUi.reducedMotionStopsThePulse` | verified |
| GT-11 | 10 modern cues (theme radius, vertical text, pulsing dots, strings-across grid) | `TechniquePill`, `RailButton`, `CascadeView` | `TechniquesUi.everySubTabRendersItsControls` | verified |
| GT-12 | 11 feature index rows | this table; HELP topic `techniques` | `HelpTab.theWorkspaceTopicNamesEveryTabThatExists` | verified |

## (b) Decisions

- Slide's arm pill is Slide Mode (`slide_guitar`): slide-guitar.md's toggle already is the slide's arm; a second switch would disagree with it.
- The mute lives in `MuteEngine` (engine-technique-layer 1), not in RhythmEngine as the WIP draft had it: the rhythm engine only stamps the pattern step's mute on its notes, and every note (rhythm, played, direct) gets its final mute in one place.
- `StringEngine` gains one damping mode (`Muted`, absolute T60 and cutoff): the existing modes scale the string's own sustain, and a palm mute's decay is the hand's, not the string's.
- Mute T60 tests measure the note's fundamental band: the string's 7 Hz output DC blocker rings on its own (23 ms) after any short damped strike, which broadband read a 50 ms loop T60 as 112 ms.
- Fret mute is checked on the struck string's level at 0.5 s: the body, the room and sympathetic strings ring on after the finger lets go, as they would, and the level follower releases over 60 ms.
- Chuka pass mark is < 50 ms (was < 30 ms in the WIP draft): "no sustained pitch" is met well inside an eighth note; the fundamental band's own ring bounds the reading.
- Tap strength curve is one bipolar parameter (-1 soft .. 0 linear .. +1 hard, an exponent): "user can shape" with one control that automates.
- Tap fret is continuous (the spec's struct says int) so fret snap off can place microtonal taps (spec 3, 10).
- The pull-off flick's strength is flick x (0.5 + 0.5 x tap strength) and its transient is measured above 2 kHz: an attack is heard as its high partials, which a 200 ms old tap has lost.
- Hammer-on promotion uses a new `TechniqueEngine` hammer-on window (150 ms) only while tapping is armed; disarmed, the legato rules are untouched.
- Eight-Finger Tap preset ships at 4 taps per string (the stock maximum): factory presets store stock-range values; unlocking the Pick family reaches 8.
- `bend_global_range` stock is 0-2400 cents (advanced 4800) so the Whammy-Style Two-Octave preset needs no unlock; `tap_max_concurrent` is in the pick family (right hand); `slide_speed_limit` in slide.
- Bend snap strength: 1 is pure snap, 0 none. Section 3's sentence says the reverse, but its presets (0.3 "light attraction", 0.8 for maqam) and test 9 ("snap 1.0 ... lands on the step") say this; majority wins.
- Quantise applies to the held pitch (fret plus bend) whatever the bend; 12-TET-aligned grids leave fretted notes alone, other grids pull them too, as a re-fretted microtonal guitar would.
- "Quarter-tone" and "24-EDO" are both offered (the spec lists both); they are the same grid.
- Vibrato source defaults to LFO as section 2 lists it (with rate 6 Hz, depth 20 cents, onset 200 ms); Off is an added choice so a player can bend without vibrato.
- Release curve shapes the pre-bend release and the vibrato onset; bend curve shapes the source-to-cents mapping. "User-drawn" is a five-point curve stored in the preset's techniques block.
- Per-string custom CC: string n uses base CC + n (one parameter, six CCs).
- Scala scales are rooted on middle C (1/1 = MIDI 60); .tun files are absolute.
- Slide relative mode: a full throw moves `slide_pos_range` frets (default 12); bipolar only for pitch bend.
- Slide slant sources span +/-30 degrees (the stock slant range).
- The slide's and bend's Easy-mode "…" popovers are the SLIDE and BEND pills' hold popovers; Easy mode has no separate slide glyph or bend indicator to hang them on, and the header belongs to others.
- CHARACTER has no Right Hand or PLAYING group yet (fingerstyle-attack.md belongs to another workstream), so the Tapping and Microtonal mirrors are one `TechniqueMirrors` block at the foot of CHARACTER, titled with the group they belong to; they can move into those groups when those exist.
- Matrix "queue" cells are accepted and flagged `queued`: the engines keep their own order (taps are capos in series, slap strikes are timed), and the multi-finger tap test requires two taps on one string to coexist.
- Combined-preset "reference within 0.5 dB" compares two renders of the same preset: a stored reference file would pin this build's DSP rather than the spec.
- "Byte-identical playback of pre-delta presets" is covered by the whole pre-existing suite passing unchanged plus `TechniqueLayer.preDeltaPresetsLoadDisarmedAndIdle` (every technique path idle on such a preset).
- The technique undo classes merge within 200 ms only when nothing else was pushed in between; attached controls keep the processor's one-entry-per-gesture rule, which is the same grouping for a drag.
- The TECHNIQUES panel joins the component tree when its tab is shown; the onboarding hook is its component ID (`onboarding.techniques`) and the CASCADE rail button's (`onboarding.techniques.cascade`).
- Deferred: MIDI export of mute types, taps and bend contributions (the capture has no technique path from the engine; the slap is not exported either), the ModMatrix `PreBendEvent` source class, and the low-CPU-class banner (no CPU class detection in the build).
