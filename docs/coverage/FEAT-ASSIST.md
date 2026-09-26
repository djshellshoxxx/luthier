# FEAT-ASSIST coverage: Performance Assist (auto articulation)

Workstream FEAT-ASSIST (branch `claude/luthier-feat-assist`) implements
`spec/auto-articulation.md` (AA-01 - AA-42). Tests are `Suite::test` in
`LuthierTests`:

- `Source/Tests/AutoArticulationTests.cpp`: suites `AutoArticulation`
  (interpreter-level rules) and `AutoArticulationEngine` (engine / processor).
- `Source/Tests/AutoArticulationUiTests.cpp`: suite `AutoArticulationUi` (xvfb).
- `Source/Tests/CombinationTests.cpp`: `Combo::performanceAssistAcrossContexts`.
- `scripts/assist_off_golden_check.sh`: AA-01 against the pre-feature build.

Parameters added (4, appended last in the `FEAT-ASSIST` block, count test
`... + 4 // FEAT-ASSIST`): `aa_enabled`, `aa_style`, `aa_amount`, `aa_rules`.

New files: `Source/Model/Playing/AutoArticulator.{h,cpp}`,
`AutoArticulationStyles.{h,cpp}`, `AutoArticulationFeed.h`,
`AssistDecisionLog.h`, `MidiInterpreterAssist.cpp`,
`Source/LuthierEngineAssist.cpp`, `Source/Support/Edition.{h,cpp}`,
`Source/UI/PerformanceAssistUi.{h,cpp}`, the three test files above, and the
two scripts.

## Coverage

| ID | Spec + section | Implementation | Verification | Status |
|---|---|---|---|---|
| AA-R0.1 | 0.1 Off is today: off / Amount 0 / rules 0 bit-identical; own hash, no `rng` draws | every hook behind `MidiInterpreter::isAssistEffective()`; `AutoArticulator::hash01` | `AutoArticulationEngine::offAmountZeroAndNoRulesAreTheSameAsNothing`; `AutoArticulation::assistDrawsNothingFromTheInterpretersRandom`; `scripts/assist_off_golden_check.sh` (36/36 factory presets byte-identical to ee09a86) | verified |
| AA-R0.2 | 0.2 / 5 Explicit wins | `TechniqueEngine::decide (..., explicitOut)`; `AutoArticulator::decorate`, `explicitOnString`; `MidiInterpreter::assistNoteIsExplicit` | `AutoArticulation::explicitTechniquesWin`, `slapZoneAndTapArmedAreNotDecorated` | verified |
| AA-R0.3 | 0.3 Zero added latency; `getLatencySamples` unchanged | no change to the chord window | `AutoArticulationEngine::latencyDoesNotDependOnAssist` | verified (see decision D1 for 3.3) |
| AA-R0.4 | 0.4 Deterministic, block-size independent | decisions from `NoteRecord` stamps; transport ppq from the block's start | `AutoArticulation::decisionsDoNotDependOnTheBlockSize`, `AutoArticulationEngine::theFeedDoesNotDependOnTheBlockSize`, `realtimeAndOfflineRendersAreIdentical` | verified |
| AA-R0.5 | 0.5 Never a harmonic / Tap / SlideGuitar / MutedPick | `decorate` only sets Pluck, HammerOn, PullOff, Slide, PalmMute | `AutoArticulation::neverAHarmonicTapSlideGuitarOrMutedPick` (10 000 phrases) | verified |
| AA-R0.6 | 0.6 No allocation / locks; SPSC POD feed | fixed arrays; `AutoArticulationFeed` | `AutoArticulationEngine::noAllocationInProcessBlockInAnyStyle` | verified |
| AA-R0.7 | 0.7 Visible and captured | feed (7.3), capture marks (9) | AA-32, AA-39 | verified |
| AA-S2 | 2 Style table, Amount scaling, chain caps | `AutoArticulationStyles.cpp` (`kStyles`); `windowScale`, `depthScale`, `probabilityScale` | AA-08, AA-11, AA-14, AA-17 pin the numbers | verified (scaling scope: decision D3) |
| AA-S2b | 2 Styles on any family: guitar style on a bass halves vibrato, strums together; bass style on a guitar no strums | `startVibratoCandidate` (bass depth 0.5), `planRollOrTogether` | `Combo::performanceAssistAcrossContexts` (5-string bass) | verified |
| AA-3.1 | 3.1 Position: cost terms, ties, hand box, chords via `setPreferredPosition (H)`, late join | `AutoArticulator::planSingle`, `setHandPositionFromVoicing`; `MidiInterpreter::flushChordGroup` hook, `assistFlushSingle` | AA-09, AA-10, AA-20 | verified |
| AA-3.2 | 3.2 Hammer-on / pull-off; hand-over; fretless -> slide; chain cap | `planSingle`, `resolveLegato`, `decorate`; `assistNoteOff` (hand-over) | AA-04, AA-06, AA-07, AA-08 | verified |
| AA-3.3 | 3.3 Slide by overlap; 4-7 st position shift | `planSingle` deferral + `assistResolvePending` | AA-05 | verified (decision D1) |
| AA-3.4 | 3.4 Delayed vibrato: lead only, not PM, no pitch gesture, 300 ms ramp, +-4 % rate, max with CC, 150 ms ramp-out | `autoVibratoDepth/Cents`, `setEffectiveNow`; `LuthierEngine::assistPerBlockCents` | AA-11, AA-12 | verified |
| AA-3.5 | 3.5 Attack: accent / soft on `Excitation::Params`, velocity unchanged, accent mark | `decorate`; `triggerNote` scales brightness / noise | AA-13, AA-32 (`<accent/>`) | verified |
| AA-3.6 | 3.6 Palm mute: register, chug, repeated pitch, Metal first note, per-note amount; mute lift at an absolute sample, 60 ms ramp | `decorate`; `LuthierEngine::assistNoteStarted` (schedules `kDampingLift`), `assistFireLift`, `assistPerBlockCents` | AA-14, AA-15 | verified |
| AA-3.7a | 3.7 Alternate picking: 16th grid running, toggle stopped, reset after 300 ms; up-stroke velocity / brightness / noise; bass fingers exempt | `decorate` | AA-16 | verified |
| AA-3.7b | 3.7 Strum: direction by grid (swing for Blues) or silence, sps x (0.75 + 0.5 v), up-stroke force scaling, missScale 0; Fingerstyle roll; Bass together; CC 76/77 win | `planStrum`, `planRollOrTogether`; `MidiInterpreter::assistPlanStrum`, `assistShapeStrikes` | AA-17, AA-18, AA-19 | verified |
| AA-3.8 | 3.8 Bend-into, slide-in, fall | `planSingle` (ornaments), `decorate` (pitch curve), `onNoteOff` (fall), `autoPitchCents` | AA-21, AA-22 | verified |
| AA-4.1 | 4.1 New files and the AutoArticulator API | as listed above | build | verified |
| AA-4.2a | 4.2 NoteOnEvent fields | `PlayingEvents.h` FEAT-ASSIST block (plus `muteLiftSamples`, `autoAccent`, `autoStrumMask`, `explicitArticulation`) | all rule tests | verified |
| AA-4.2b | 4.2 TechniqueEngine `setLegatoInferenceEnabled`, `decide (..., explicitOut)` | `TechniqueEngine.{h,cpp}` | AA-04, AA-24 | verified |
| AA-4.2c | 4.2 MidiInterpreter hooks (Mono, Poly single / late join, strum, decorate, note-off) | `MidiInterpreter.cpp` marked lines; `MidiInterpreterAssist.cpp` | rule tests | verified |
| AA-4.2d | 4.2 LuthierEngine: ExplicitContext, triggerNote fields, feed + capture, `ScheduledEvent` lift, per-block curves | `LuthierEngineAssist.cpp`; marked lines in `LuthierEngine.cpp` | AA-15, AA-28, AA-32 | verified |
| AA-4.2e | 4.2 ParameterBridge via fast table; Free resolved to effective values | `ParameterBridge::applyToEngine` FEAT-ASSIST block, `Parameters::effectiveAssistSettings` | AA-36 | verified (decision D5) |
| AA-5 | 5 Explicit table: GC / MPE bypass; rhythm driving; controller techniques; slap / scrape / Tap armed; mute grid; CC / AT vibrato and bends; strum CCs; Luthier-profile import | `isAssistEffective`, `AssistExplicitContext`, `assistSetContext` | AA-24 - AA-27, AA-33 | verified except Tap armed / mute grid sources: pending (decision D6) |
| AA-6 | 6 Four parameters, appended, defaults, automatable; morph: switches at 50 %, Amount interpolates | `Parameters.cpp` FEAT-ASSIST block; `AssistRulesParameter::isDiscrete` | AA-34, AA-35 | verified |
| AA-7.1 | 7.1 Easy AUTO pill (56 x 22, fill / outline, flash, tap, hold / Down popover with Amount, description, "More in RHYTHM tab") + style combo (70 px) | `AssistPill`, `AssistStyleBox`; `EasyPanel` mode column; `LuthierAudioProcessorEditor::openAssistInRhythmTab` | AA-37 | verified |
| AA-7.2 | 7.2 PLAYING group first in RHYTHM: mode mirror, switch, style, Amount, 9 rule switches, recent list (16, newest first), notice, collapsible, status line | `PerformanceAssistGroup`; `RhythmPanel` marked lines; `UiState::playingGroupCollapsed` | AA-38 | verified |
| AA-7.2b | 7.2 TECHNIQUES tab CASCADE read-only "Auto" row | - | - | deferred: the TECHNIQUES tab is not on the integration branch; `AssistUi::statusText` / `noticeText` are what it will show |
| AA-7.3 | 7.3 Labels on both fretboards, glyphs, 10 pt pill in string colour at 0.9, 600 ms fade, reduced motion, strum arrow, 30 Hz drain, 600 ms staleness | `AssistLabelOverlay`, `AssistDecisionLog`; one member each in `FretboardComponent`, `GuitarBodyComponent` | AA-39 | verified |
| AA-7.4 | 7.4 Options switch "Show Performance Assist labels" (UiPreferences, default on) | `AssistUi::showLabels`; `AppearancePage::assistLabelsToggle` | AA-39 | verified (decision D7) |
| AA-7.5 | 7.5 `A` shortcut (rebindable, in the overlay), help topic, empty list text, locked-style notice, no error states | `Accessibility.cpp` `toggleAssist`; `HelpContent` `performance-assist`; `AssistUi::kEmptyListText`, `showLockedStyleNotice` | AA-40, `HelpTab::*` | verified (upsell: decision D8) |
| AA-7.6 | 7.6 Feature-to-location row | RHYTHM -> PLAYING; Easy strip; `A` | AA-37, AA-38 | verified |
| AA-8a | 8 State: preset / snapshot / host state; old preset loads off | parameters; `PresetManager::fromVar` FEAT-ASSIST default fill | AA-35 | verified |
| AA-8b | 8 Undo wording and grouping | `AssistUi::setEnabled`, `setRule` (ScopedUndoAction); attachments' gestures for style / Amount | AA-41 | verified |
| AA-8c | 8 Accessibility: pill announcement, rule labels, list rows, tab order | `AssistPill` description, rule switch titles / descriptions, `getNameForRow`, explicit focus order | AA-40 | verified |
| AA-9a | 9 Capture marks: palm mute amount, vibrato rate / depth, accent, bend-into curve, slideOut, slideIn, pick strokes | `LuthierEngineAssist.cpp`; `PerformanceCapture::mark (.., second)`, `autoRules` record | AA-32 | verified |
| AA-9b | 9 `ScoreTechnique::pickStrokeUp/Down` -> MusicXML `<up-bow/>` / `<down-bow/>`, Guitar Pro pickstroke | `PerformanceScore.h`, `NotationExport.cpp`, `MidiPerformance.cpp` wire names | AA-32 | verified |
| AA-9c | 9 `CapturedNote::autoRules`; NOTE `aa=<hex>` written when nonzero and read back | `PerformanceCapture`, `ScoreNote::autoRules`, `MidiPerformance` | AA-32 | verified |
| AA-9d | 9 NOTATION tab / live TAB draw automatic techniques in the secondary accent | - | - | deferred: `ScoreNote::autoRules` carries the data; the NOTATION panel's drawing is outside this workstream's files (decision D9) |
| AA-10 | 10 Budget: +0 latency, O(strings) per block, < 3 us per note-on | as above | AA-03, AA-31 | verified (decision D4) |
| AA-11 | 11 Editions: Free styles, nearest-style mapping, aa_rules non-automatable " (Pro)", value written back | `Support/Edition.h`, `effectiveAssistSettings`, layout, `AssistStyleBox`, rule switches disabled in Free | AA-36 | verified |
| AA-12 | 12 Interactions: Humanize after Assist, strum dynamics untouched, rhythm engine, Tune direct notes assisted, Slide Mode | `assistSetContext (rhythm pass)`, direct pass; Slide Mode switches legato / ornaments off | AA-26, AA-42 | verified; piano roll ghost dots (PR-05) deferred with the VISUAL piano roll |
| AA-13 | 13 Failure modes | voicer fallback when unplayable; queue unchanged; contradictory context -> Pluck; lift; fall cancelled by a same-string note | AA-22, AA-23 | verified |
| AA-01 ... AA-42 | 14 Tests | see the rows above; every AA test is implemented | `AutoArticulation*`, `Combo::performanceAssistAcrossContexts` | verified |

## Decisions

- **D1 (3.3 slide vs zero latency).** "P still held past N's arrival for the
  minimum overlap" needs the future. A legato candidate whose source is still
  held waits for the source's release or the minimum overlap (25-50 ms),
  whichever comes first: release first is a hammer-on / pull-off at that
  sample, the deadline first a slide. The source keeps sounding meanwhile, so
  nothing goes silent; only these notes wait, only with Assist on, and the
  reported latency is unchanged. It is the only reading that meets AA-04 and
  AA-05 deterministically.
- **D2 (held state).** From the absolute stamps of a 32-entry note log, never
  from slot state (4.2).
- **D3 (Amount scaling scope).** "Time windows" are the legato max IOI and the
  chug IOI; the slide minimum overlap, vibrato delay and late-join window are
  thresholds and are not scaled (AA-11 pins the Blues delay at 220 ms at the
  default Amount).
- **D4 (AA-31 bound).** 0.02 performance units of a 4.5 s render is ~0.9 ms,
  below run-to-run noise on shared CI; the test bounds the whole render at
  +3 % and measures the per-note planning directly (< 3 us, 12 strings).
- **D5 (Free resolution).** Resolved in the bridge (a pure mapping of three
  ints, no allocation) against a relaxed-atomic Edition flag that defaults to
  Pro; the stored values are never rewritten. `aa_rules` automatability is
  fixed when the layout is built, as a host requires.
- **D6 (TECHNIQUES context).** `tapArmed` and `muteGridActive` exist in
  `AssistExplicitContext` and are honoured, but the TECHNIQUES engine layer is
  not on the integration branch; `LuthierEngine::assistSetContext` fills them
  false with a note for that merge.
- **D7 (Visual aids).** The shared "Visual aids" section (VISUAL branch) is not
  on the integration branch; the switch sits at the foot of Options ->
  Appearance and uses the `visualAids.assistLabels` UiPreferences key, so it
  moves with that section when it lands.
- **D8 (upsell).** The editions.md 4.2 upsell panel does not exist yet; a
  locked style shows a one-line notice naming what plays instead, and the
  combo marks Pro styles "(Pro)" rather than a padlock glyph the UI font may
  lack.
- **D9 (notation colour).** The data (`autoRules` per note) reaches the score;
  colouring it is the NOTATION panel's, left for that owner.
- **D10 (popover).** The Amount popover is a CallOutBox child of the editor, not
  a desktop window: hosts handle extra top-level windows badly and it closes
  with the editor.
- **D11 (explicitArticulation).** The riff library's `NoteOnEvent` field is added
  here with the same name and type; `decorate` records such notes unassisted.
- **D12 (Combo round-trip test).** Adding four parameters shifts
  `Combo::everyParameterSurvivesTheSessionStateRoundTrip`'s random sequence into
  B-07 (legacy `doubler_on` migrates on load); the test no longer randomises that
  hidden parameter, which its comparison already excluded.
- **D13 (Guitar Pro pick stroke).** Written as a note property beside the
  other note techniques rather than restructuring the beat writer.
