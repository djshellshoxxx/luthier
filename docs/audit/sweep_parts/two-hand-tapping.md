## two-hand-tapping.md

No tapping exists on this checkout: no TapEngine, no `tap_*` parameters, no tap trigger. The only related code is the legato hammer-on/pull-off promotion in `Model/Playing/TechniqueEngine` (`legato_window` 40 ms, velocity threshold hard-coded 0.63), which is not the spec's 150 ms / user threshold. The techniques branch has the work: `DSP/Techniques/TapEngine`, `LuthierEngine::playTapEvent`, nine `tap_*` params, TAP page, pill, CHARACTER mirror, fretboard square markers, five presets and 10 `Tap.*` tests (spot-checked `TapTests.cpp` for the fret-12/fret-5, auto-pull-off-off and fret-12.3 cases, and the Eight-Finger preset). Owner gaps: MIDI export of taps deferred; Eight-Finger preset ships at 4 per string; the CHARACTER "Tapping" section sits at the foot of CHARACTER rather than inside the Right Hand group (which realism-b is adding).

| Req | Summary | Engine location | GUI location | Test | Status |
|---|---|---|---|---|---|
| TH-1 (§0, §1) | TapGesture (string, fret, strength, hand, pull-off-after, target, duration) | (branch) `DSP/Techniques/TapEngine.h:TapGesture` (fret continuous) | n/a | (branch) `Tap.fretSnapOffTapsBetweenTheFrets` | OWNED |
| TH-2 (§2) | Tap-on impulse + contact; tap-hold = movable capo; tap-off back to fretted pitch | (branch) `TapEngine`, `LuthierEngine::playTapEvent`, `techniqueFret` | n/a | (branch) `Tap.aTapIsAMovableCapoAndLiftsBackToTheFrettedNote` | OWNED |
| TH-3 (§2) | Pull-off lateral flick excitation on release | (branch) `TapEngine::tapOff` | n/a | (branch) `Tap.thePullOffFlickIsTheReleaseTransient` | OWNED |
| TH-4 (§2) | Multi-finger: concurrent taps same/different strings | (branch) `TapEngine::soundingFret` | n/a | (branch) `Tap.twoTapsOnAStringAreCaposInSeries` | OWNED |
| TH-5 (§3) | Trigger source MIDI ch 2 (default) / keyswitch / fretboard tap layer | (branch) `TapSettings::triggerConfig`, `TechniqueTriggers` KS 19 | (branch) TAP page; `TechniqueOverlay::handleMouseDown` | (branch) `Tap.theTriggersTakeTheirNotes`, `TechniquesUi.theFretboardTapsWhenTheTapLayerIsOn` | OWNED |
| TH-6 (§3) | Tap strength curve (default linear) | (branch) `TapSettings::strengthFor` (bipolar exponent) | (branch) TAP page | (branch) `Tap.theStrengthCurveShapesVelocity` | OWNED |
| TH-7 (§3) | Auto pull-off, default on | (branch) `TapEngine::tapOff`, `tap_auto_pull_off` | (branch) TAP page | (branch) `Tap.thePullOffFlickIsTheReleaseTransient` | OWNED |
| TH-8 (§3, §5) | LH hammer-on threshold (40) and 150 ms promotion window; pull-off on revealing note-off | (branch) `TechniqueEngine::setHammerOnWindowMs`, `tap_hammer_threshold` | (branch) TAP page | (branch) `Tap.softNotesCloseTogetherAreHammerOns` | OWNED |
| TH-9 (§3) | Lateral flick 0.5, duration 200 ms, max concurrent 2 (adv 8), fret snap on | (branch) `tap_flick/_duration/_max_concurrent/_fret_snap`; `PhysicalRange` row | (branch) TAP page | (branch) `Tap.everyControlRoundTrips`, `Tap.fretSnapOffTapsBetweenTheFrets` | OWNED |
| TH-10 (§4) | TapEngine trigger/release/processBlock/reset, inserted after TechniqueEngine before StringEngine | (branch) `TapEngine::requestGesture/requestRelease/processBlock/reset`; `LuthierEngineTechniques.cpp` | n/a | (branch) `Tap.*`, `TechniqueLayer.theModulesRunInTheDocumentedOrder` | OWNED |
| TH-11 (§6) | Techniques > Tapping sub-tab, all controls | (branch) - | (branch) `UI/Techniques/TechniquePages:TapPage` | (branch) `TechniquesUi.everySubTabRendersItsControls` | OWNED |
| TH-12 (§6) | Easy Playing strip Tap pill arms next note as tap | (branch) `tap_armed` | (branch) `TechniquePillRow` | (branch) `TechniquesUi.thePillsArmOnAClick` | OWNED |
| TH-13 (§6) | CHARACTER Right Hand group "Tapping" section (curve, flick, auto pull-off) — mirror sits at CHARACTER foot, not in a Right Hand group | (branch) - | (branch) `UI/Techniques/TechniqueMirrors` in `CharacterPanel` | (branch) `TechniquesUi.theCharacterMirrorsAttachTheSameParameters` | OWNED |
| TH-14 (§6) | Fretboard square tap markers, released fade 100 ms | (branch) - | (branch) `TechniqueOverlay` tapMarkers | (branch) `TechniquesUi.theOverlaysDrawInsideTheirBudget` | OWNED |
| TH-15 (§7) | Cascade: mute/bend compatible; slide conflicts on contacted strings; slap alternate; scrape conflicts | (branch) `CascadeResolver`, `playTapEvent` | n/a | (branch) `Tap.theSlideBarHoldsItsStrings`, `Cascade.aTapPreemptsAScrapeOnItsString` | OWNED |
| TH-16 (§8) | Five presets (Standard Two-Hand, Legato Runs, Eight-Finger, Microtonal, Percussive) — Eight-Finger at 4 not 8 | (branch) `Presets/TechniquePresets.cpp` | (branch) preset browser | (branch) `TechniquesUi.thePresetChipFilters` | OWNED |
| TH-17 (§9) | MIDI export of taps (SysEx hand/strength/pull-off; generic ch 2) — deferred on owner branch | - | n/a | - | OWNED |
| TH-T1 (§10) | Test: tap 12 over fret 5, returns with small transient | (branch) | n/a | (branch) `Tap.aTapIsAMovableCapoAndLiftsBackToTheFrettedNote` | OWNED |
| TH-T2 (§10) | Test: flick 1.0 transient; auto pull-off off none | (branch) | n/a | (branch) `Tap.thePullOffFlickIsTheReleaseTransient` | OWNED |
| TH-T3 (§10) | Test: hammer-on without pick transient | (branch) | n/a | (branch) `Tap.softNotesCloseTogetherAreHammerOns` | OWNED |
| TH-T4 (§10) | Test: two taps = capos in series | (branch) | n/a | (branch) `Tap.twoTapsOnAStringAreCaposInSeries` | OWNED |
| TH-T5 (§10) | Test: fret snap off 12.3 sharp | (branch) | n/a | (branch) `Tap.fretSnapOffTapsBetweenTheFrets` | OWNED |
| TH-T6 (§10) | Test: CPU idle < 0.05 %, busy < 0.8 % | (branch) | n/a | (branch) `Tap.cpuStaysInBudget` | OWNED |
| TH-T7 (§10) | Test: preset round-trips every field | (branch) | n/a | (branch) `Tap.everyControlRoundTrips` | OWNED |

<!-- counts DONE=0 NO-GUI=0 NO-TEST=0 PARTIAL=0 MISSING=0 OWNED=24 -->
