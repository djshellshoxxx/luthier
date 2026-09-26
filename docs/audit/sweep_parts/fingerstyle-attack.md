## fingerstyle-attack.md

REALISM-B has landed on this checkout: contact profiles, rest/free stroke, per-string tools, the pattern's finger carried to the string, i/m alternation, styles, CC 102/105, the 14 parameters, the CHARACTER RIGHT HAND group, the Easy Tool selector and fretboard tool glyphs, with `FingerstyleAttack.FA01..FA17` registered. Still open: the §19 row (FA-25), the midi-export PICK tool/finger/stroke fields (FA-26) and a test that reset clears rest neighbours and the alternation phase (FA-27).

| Req | Summary | Engine location | GUI location | Test | Status |
|---|---|---|---|---|---|
| FA-1 (§0.3, 1) | `Params::releaseSeconds`; lowpass 1/(2πτ) | `Excitation::Params::releaseSeconds` | n/a | `FingerstyleAttack.FA03_releaseTimeSetsTheCutoff` | DONE |
| FA-2 (§1) | Full MaterialSpec blend by nail_vs_flesh (no step at 0.5) | blended `MaterialSpec` in `applyRightHand` | CHARACTER PICK `nail_vs_flesh` (existing) | `FingerstyleAttack.FA01_FA02_nailIsBrighterWithNoStep` | DONE |
| FA-3 (§1) | Defaults reproduce 2.2/7.0/1.1 kHz | - | n/a | `FingerstyleAttack.FA04_defaultsReproduceTheTable` | DONE |
| FA-4 (§1) | Nail click at pick_click × 0.35 × b; fingertip noise unchanged | `applyRightHand` (Finger) | n/a | `FingerstyleAttack.FA10_perStringTools` | DONE |
| FA-5 (§1) | Thumb position offset, clamp 0.02-0.5 | `applyRightHand` | RIGHT HAND thumb position | `FingerstyleAttack.FA14_thumbPosition` | DONE |
| FA-6 (§0.2) | Pick path bit-identical | Global tool = old path | n/a | `FingerstyleAttack.FA05_thePickPathIsUntouched` | DONE |
| FA-7 (§2) | Rest stroke terms (level ×1.26, length, brightness, bridge drive, sustain) | `applyRightHand`, `StringEngine::setCouplingSendScale` | RIGHT HAND stroke | `FingerstyleAttack.FA06_restStrokeLevelAndTone` | DONE |
| FA-8 (§2) | Rest neighbour Chuck (finger s+1, thumb s-1) until next note-on | `applyRightHand` | n/a | `FingerstyleAttack.FA07_restDampsTheNeighbour` | DONE |
| FA-9 (§2) | Auto stroke rule (single note within 30 ms, v >= 0.7) | `resolveRightHand` | RIGHT HAND stroke Auto | `FingerstyleAttack.FA08_autoStroke` | DONE |
| FA-10 (§2, 6) | Reuse bass-techniques' `rest_stroke` / `finger_alternation_variation` IDs (not duplicated) | `Parameters.cpp` (bass-techniques IDs), `DSP/Slap/BassFingerstyle.h` | CHARACTER SLAP group (bass only), `SlapGroup::restStroke/alternation` | `BassTechniques.theRestStrokeDampsTheNextLowerString`, `BassTechniques.fingerAlternationVaries` | DONE |
| FA-11 (§2) | Bass `rest_stroke` on forces Rest for finger tools with this damping | bridge reads `rest_stroke` | RIGHT HAND (conditional) | `FingerstyleAttack.FA07_restDampsTheNeighbour` | DONE |
| FA-12 (§3) | Per-string tools rh_string_tool_1..6; strings 7-12 follow course/lowest | `RightHand.h`, `resolveRightHand` | RIGHT HAND six-cell row | `FingerstyleAttack.FA10_perStringTools` | DONE |
| FA-13 (§3) | Resolution order CC 102 > pattern finger > string > global; striker wins for strums | `resolveRightHand` | n/a | `FingerstyleAttack.FA09_patternFingersReachTheString`, `FingerstyleAttack.FA15_ccTriggers` | DONE |
| FA-14 (§3) | `NoteOnEvent::finger`; scheduleFingerpick passes step.finger | `RhythmEngine::emitNote` | n/a | `FingerstyleAttack.FA09_patternFingersReachTheString` | DONE |
| FA-15 (§3) | i/m alternation (τ, position, +1.5v ms), deterministic, all families | `applyRightHand`, `Params::startDelaySamples` | RIGHT HAND alternation | `FingerstyleAttack.FA12_alternation` | DONE |
| FA-16 (§4) | rh_style writes table on user change, one undo, never on preset load | `RightHandGroup::applyStyle` | RIGHT HAND style box | `FingerstyleAttack.FA16_styleWritesOnce` | DONE |
| FA-17 (§4) | Travis thumb PalmMute at thumb_palm_mute | `applyRightHand` | RIGHT HAND Travis mute | `FingerstyleAttack.FA11_travisMute` | DONE |
| FA-18 (§4) | Hybrid snap via SlapEngine pop collision | `applyRightHand`, `SlapEngine::makeContactBuzz` | RIGHT HAND hybrid snap | `FingerstyleAttack.FA13_slapAndPopTools` | DONE |
| FA-19 (§4) | Slap / Pop tools classify via SlapEngine on any family, armed or not | `makeToolStrike` in `triggerNote` | tool cells | `FingerstyleAttack.FA13_slapAndPopTools` | DONE |
| FA-20 (§5) | CC 102 RightHandTool (7 bands), CC 105 RestStroke | `MidiInterpreter` targets | n/a (MIDI) | `FingerstyleAttack.FA15_ccTriggers` | DONE |
| FA-21 (§6) | 14 params (flesh/nail release, thumb position, rest damping, rh_stroke, rh_style, 6 tools, thumb_palm_mute, hybrid_snap); pick-family rows; choices append-only | `Parameters.*` REALISM-B block | RIGHT HAND group | `FingerstyleAttack.FA17_rangesRealtimeRoundTrip` | DONE |
| FA-22 (§7) | CHARACTER RIGHT HAND group (mirrors use_fingers, nail_vs_flesh; kHz readouts) | n/a | `UI/RightHandGroup.*` in `CharacterPanel` | `RealismBUi.theCharacterTabCarriesTheThreeGroups` | DONE |
| FA-23 (§7) | Easy Playing strip Tool selector (rh_style segmented, Custom = "Mixed") | n/a | `RightHandToolSelector` in `EasyPanel` | `RealismBUi.theEasyPlayingStripHasTheToolSelector` | DONE |
| FA-24 (§7) | Illustration tool glyph per string at pluck point | n/a | fretboard REALISM-B layer | `RealismBUi.theFretboardDrawsTheTouch` | DONE |
| FA-25 (§7) | gui-integration §19 row "Right-hand tools…" — not added | n/a | n/a (doc) | - | MISSING |
| FA-26 (§7) | midi-export PICK class gains tool / finger / stroke — owner defers | none | n/a | - | MISSING |
| FA-27 (§8) | Note-on only cost, budget 0.02, no alloc; reset clears rest neighbours and alternation phase — reset clearing rest neighbours / alternation phase has no test | `resetRealismB` | n/a | `FingerstyleAttack.FA17_rangesRealtimeRoundTrip` | PARTIAL |
| FA-28 (§9 FA-01/02) | Nail brighter; no step at 0.5 | - | n/a | `FingerstyleAttack.FA01_FA02_nailIsBrighterWithNoStep` | DONE |
| FA-29 (§9 FA-03) | Release time sets cutoff | - | n/a | `FingerstyleAttack.FA03_releaseTimeSetsTheCutoff` | DONE |
| FA-30 (§9 FA-04) | Defaults reproduce table | - | n/a | `FingerstyleAttack.FA04_defaultsReproduceTheTable` | DONE |
| FA-31 (§9 FA-05) | Pick path untouched | - | n/a | `FingerstyleAttack.FA05_thePickPathIsUntouched` | DONE |
| FA-32 (§9 FA-06) | Rest level and tone | - | n/a | `FingerstyleAttack.FA06_restStrokeLevelAndTone` | DONE |
| FA-33 (§9 FA-07) | Rest damps neighbour | - | n/a | `FingerstyleAttack.FA07_restDampsTheNeighbour` | DONE |
| FA-34 (§9 FA-08) | Auto stroke | - | n/a | `FingerstyleAttack.FA08_autoStroke` | DONE |
| FA-35 (§9 FA-09) | Pattern fingers reach the string | - | n/a | `FingerstyleAttack.FA09_patternFingersReachTheString` | DONE |
| FA-36 (§9 FA-10) | Per-string tools | - | n/a | `FingerstyleAttack.FA10_perStringTools` | DONE |
| FA-37 (§9 FA-11) | Travis mute | - | n/a | `FingerstyleAttack.FA11_travisMute` | DONE |
| FA-38 (§9 FA-12) | Alternation | - | n/a | `FingerstyleAttack.FA12_alternation` | DONE |
| FA-39 (§9 FA-13) | Slap and Pop tools | - | n/a | `FingerstyleAttack.FA13_slapAndPopTools` | DONE |
| FA-40 (§9 FA-14) | Thumb position | - | n/a | `FingerstyleAttack.FA14_thumbPosition` | DONE |
| FA-41 (§9 FA-15) | CC triggers | - | n/a | `FingerstyleAttack.FA15_ccTriggers` | DONE |
| FA-42 (§9 FA-16) | Style writes once | - | n/a | `FingerstyleAttack.FA16_styleWritesOnce` | DONE |
| FA-43 (§9 FA-17) | Ranges, realtime, export round trip | - | n/a | `FingerstyleAttack.FA17_rangesRealtimeRoundTrip`, `HarmonicRealism.HR19_FA17_luthierExportRoundTrip` | DONE |
