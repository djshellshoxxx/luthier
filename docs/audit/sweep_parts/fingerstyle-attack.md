## fingerstyle-attack.md

On this checkout the finger path is the old binary switch: `nail_vs_flesh` blends only the cutoff with a material step at 0.5, one tool for all strings, and `RhythmEngine::scheduleFingerpick` drops the pattern's finger; the only spec items already present are bass-techniques' `finger_alternation_variation` and `rest_stroke` parameters (bass-only, CHARACTER SLAP group), which this spec reuses. The realism-b branch (active today, coverage complete) implements contact profiles, stroke, per-string tools, finger carry-through, alternation, styles, CC 102/105, 14 parameters, the RIGHT HAND group, the Easy Tool selector and fretboard tool glyphs with `FingerstyleAttack.FA01..FA17`. Owner gaps: midi-export PICK `tool`/`finger`/`stroke` fields and the gui-integration §19 row are deferred; reset of rest neighbours/alternation is verified by code only.

| Req | Summary | Engine location | GUI location | Test | Status |
|---|---|---|---|---|---|
| FA-1 (§0.3, 1) | `Params::releaseSeconds`; lowpass 1/(2πτ) | (branch) `Excitation::Params::releaseSeconds` | n/a | (branch) `FingerstyleAttack.FA03_releaseTimeSetsTheCutoff` | OWNED |
| FA-2 (§1) | Full MaterialSpec blend by nail_vs_flesh (no step at 0.5) | (branch) blended `MaterialSpec` in `applyRightHand` | CHARACTER PICK `nail_vs_flesh` (existing) | (branch) `FA01_FA02_nailIsBrighterWithNoStep` | OWNED |
| FA-3 (§1) | Defaults reproduce 2.2/7.0/1.1 kHz | (branch) | n/a | (branch) `FA04_defaultsReproduceTheTable` | OWNED |
| FA-4 (§1) | Nail click at pick_click × 0.35 × b; fingertip noise unchanged | (branch) `applyRightHand` (Finger) | n/a | (branch) `FA10_perStringTools` | OWNED |
| FA-5 (§1) | Thumb position offset, clamp 0.02-0.5 | (branch) `applyRightHand` | (branch) RIGHT HAND thumb position | (branch) `FA14_thumbPosition` | OWNED |
| FA-6 (§0.2) | Pick path bit-identical | (branch) Global tool = old path | n/a | (branch) `FA05_thePickPathIsUntouched` | OWNED |
| FA-7 (§2) | Rest stroke terms (level ×1.26, length, brightness, bridge drive, sustain) | (branch) `applyRightHand`, `StringEngine::setCouplingSendScale` | (branch) RIGHT HAND stroke | (branch) `FA06_restStrokeLevelAndTone` | OWNED |
| FA-8 (§2) | Rest neighbour Chuck (finger s+1, thumb s-1) until next note-on | (branch) `applyRightHand` | n/a | (branch) `FA07_restDampsTheNeighbour` | OWNED |
| FA-9 (§2) | Auto stroke rule (single note within 30 ms, v >= 0.7) | (branch) `resolveRightHand` | (branch) RIGHT HAND stroke Auto | (branch) `FA08_autoStroke` | OWNED |
| FA-10 (§2, 6) | Reuse bass-techniques' `rest_stroke` / `finger_alternation_variation` IDs (not duplicated) | `Parameters.cpp` (bass-techniques IDs), `DSP/Slap/BassFingerstyle.h` | CHARACTER SLAP group (bass only), `SlapGroup::restStroke/alternation` | `BassTechniques.theRestStrokeDampsTheNextLowerString`, `BassTechniques.fingerAlternationVaries` | DONE |
| FA-11 (§2) | Bass `rest_stroke` on forces Rest for finger tools with this damping | (branch) bridge reads `rest_stroke` | (branch) RIGHT HAND (conditional) | (branch) `FA07` | OWNED |
| FA-12 (§3) | Per-string tools rh_string_tool_1..6; strings 7-12 follow course/lowest | (branch) `RightHand.h`, `resolveRightHand` | (branch) RIGHT HAND six-cell row | (branch) `FA10_perStringTools` | OWNED |
| FA-13 (§3) | Resolution order CC 102 > pattern finger > string > global; striker wins for strums | (branch) `resolveRightHand` | n/a | (branch) `FA09`, `FA15_ccTriggers` | OWNED |
| FA-14 (§3) | `NoteOnEvent::finger`; scheduleFingerpick passes step.finger | (branch) `RhythmEngine::emitNote` | n/a | (branch) `FA09_patternFingersReachTheString` | OWNED |
| FA-15 (§3) | i/m alternation (τ, position, +1.5v ms), deterministic, all families | (branch) `applyRightHand`, `Params::startDelaySamples` | (branch) RIGHT HAND alternation | (branch) `FA12_alternation` | OWNED |
| FA-16 (§4) | rh_style writes table on user change, one undo, never on preset load | (branch) `RightHandGroup::applyStyle` | (branch) RIGHT HAND style box | (branch) `FA16_styleWritesOnce` | OWNED |
| FA-17 (§4) | Travis thumb PalmMute at thumb_palm_mute | (branch) `applyRightHand` | (branch) RIGHT HAND Travis mute | (branch) `FA11_travisMute` | OWNED |
| FA-18 (§4) | Hybrid snap via SlapEngine pop collision | (branch) `applyRightHand`, `SlapEngine::makeContactBuzz` | (branch) RIGHT HAND hybrid snap | (branch) `FA13_slapAndPopTools` | OWNED |
| FA-19 (§4) | Slap / Pop tools classify via SlapEngine on any family, armed or not | (branch) `makeToolStrike` in `triggerNote` | (branch) tool cells | (branch) `FA13_slapAndPopTools` | OWNED |
| FA-20 (§5) | CC 102 RightHandTool (7 bands), CC 105 RestStroke | (branch) `MidiInterpreter` targets | n/a (MIDI) | (branch) `FA15_ccTriggers` | OWNED |
| FA-21 (§6) | 14 params (flesh/nail release, thumb position, rest damping, rh_stroke, rh_style, 6 tools, thumb_palm_mute, hybrid_snap); pick-family rows; choices append-only | (branch) `Parameters.*` REALISM-B block | (branch) RIGHT HAND group | (branch) `FA17_rangesRealtimeRoundTrip` | OWNED |
| FA-22 (§7) | CHARACTER RIGHT HAND group (mirrors use_fingers, nail_vs_flesh; kHz readouts) | (branch) n/a | (branch) `UI/RightHandGroup.*` in `CharacterPanel` | (branch) `RealismBUi.theCharacterTabCarriesTheThreeGroups` | OWNED |
| FA-23 (§7) | Easy Playing strip Tool selector (rh_style segmented, Custom = "Mixed") | (branch) n/a | (branch) `RightHandToolSelector` in `EasyPanel` | (branch) `RealismBUi.theEasyPlayingStripHasTheToolSelector` | OWNED |
| FA-24 (§7) | Illustration tool glyph per string at pluck point | (branch) n/a | (branch) fretboard REALISM-B layer | (branch) `RealismBUi.theFretboardDrawsTheTouch` | OWNED |
| FA-25 (§7) | gui-integration §19 row "Right-hand tools…" — not added | n/a | n/a (doc) | - | MISSING |
| FA-26 (§7) | midi-export PICK class gains tool / finger / stroke — owner defers | none | n/a | - | MISSING |
| FA-27 (§8) | Note-on only cost, budget 0.02, no alloc; reset clears rest neighbours and alternation phase | (branch) `resetRealismB` | n/a | (branch) `FA17_rangesRealtimeRoundTrip` (reset: code only) | OWNED |
| FA-28 (§9 FA-01/02) | Nail brighter; no step at 0.5 | (branch) | n/a | (branch) `FA01_FA02_nailIsBrighterWithNoStep` | OWNED |
| FA-29 (§9 FA-03) | Release time sets cutoff | (branch) | n/a | (branch) `FA03_releaseTimeSetsTheCutoff` | OWNED |
| FA-30 (§9 FA-04) | Defaults reproduce table | (branch) | n/a | (branch) `FA04_defaultsReproduceTheTable` | OWNED |
| FA-31 (§9 FA-05) | Pick path untouched | (branch) | n/a | (branch) `FA05_thePickPathIsUntouched` | OWNED |
| FA-32 (§9 FA-06) | Rest level and tone | (branch) | n/a | (branch) `FA06_restStrokeLevelAndTone` | OWNED |
| FA-33 (§9 FA-07) | Rest damps neighbour | (branch) | n/a | (branch) `FA07_restDampsTheNeighbour` | OWNED |
| FA-34 (§9 FA-08) | Auto stroke | (branch) | n/a | (branch) `FA08_autoStroke` | OWNED |
| FA-35 (§9 FA-09) | Pattern fingers reach the string | (branch) | n/a | (branch) `FA09_patternFingersReachTheString` | OWNED |
| FA-36 (§9 FA-10) | Per-string tools | (branch) | n/a | (branch) `FA10_perStringTools` | OWNED |
| FA-37 (§9 FA-11) | Travis mute | (branch) | n/a | (branch) `FA11_travisMute` | OWNED |
| FA-38 (§9 FA-12) | Alternation | (branch) | n/a | (branch) `FA12_alternation` | OWNED |
| FA-39 (§9 FA-13) | Slap and Pop tools | (branch) | n/a | (branch) `FA13_slapAndPopTools` | OWNED |
| FA-40 (§9 FA-14) | Thumb position | (branch) | n/a | (branch) `FA14_thumbPosition` | OWNED |
| FA-41 (§9 FA-15) | CC triggers | (branch) | n/a | (branch) `FA15_ccTriggers` | OWNED |
| FA-42 (§9 FA-16) | Style writes once | (branch) | n/a | (branch) `FA16_styleWritesOnce` | OWNED |
| FA-43 (§9 FA-17) | Ranges, realtime, export round trip | (branch) | n/a | (branch) `FA17_rangesRealtimeRoundTrip`, `HarmonicRealism.HR19_FA17_luthierExportRoundTrip` | OWNED |
