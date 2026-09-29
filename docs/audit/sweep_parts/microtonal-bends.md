## microtonal-bends.md

This checkout has only the old global bend model: `bend_range` in semitones (1-48, default 2), MPE per-note pitch bend (`mpe_enabled`) and `vibrato_rate/depth/shape` in Advanced PERFORMANCE — no bend source stack, per-string ranges, quantise, scale loading, pre-bend or curves. The techniques branch has the work: `DSP/Techniques/BendEngine` + `MicrotonalScale` (Scala/AnaMark), ~20 `bend_*` params, BEND page, pill popover, CHARACTER mirror, fretboard bend arc + cents badge + vertical-drag bend, five presets and 11 `Bend.*` tests (spot-checked `BendEngine.h` enums/sources and `TechniqueOverlay` layers). Owner gaps: ModMatrix `PreBendEvent` source deferred, MIDI export deferred, per-source latency compensation (§1) absent, and the branch adds a second vibrato system beside the existing `vibrato_*` params.

| Req | Summary | Engine location | GUI location | Test | Status |
|---|---|---|---|---|---|
| MB-1 (§0.1, §0.3, §1) | Continuous per-string pitch; ordered source stack global + per-string + vibrato + pre-bend + slide, adding | (branch) `DSP/Techniques/BendEngine::centsFor`; slide adds in engine | n/a | (branch) `Bend.theGlobalBendMovesEveryString`, `Bend.aPerStringBendMovesOnlyItsString` | OWNED |
| MB-2 (§1) | Per-source scale factor + optional latency compensation — no latency compensation on branch | (branch) ranges only | n/a | - | OWNED |
| MB-3 (§0.2, §2) | Global bend source PB / expression / CC; fretboard drag | (branch) `BendSettings::globalSource`; `TechniqueOverlay::handleMouseDrag` | (branch) TECHNIQUES > BEND; fretboard | (branch) `Bend.theGlobalBendMovesEveryString` | OWNED |
| MB-4 (§2) | Global range cents default 200, advanced 2400 (branch: stock 2400, adv 4800) | (branch) `bend_global_range`, `PhysicalRange` row | (branch) BEND page + Easy BEND popover | (branch) `Bend.everyControlRoundTrips` | OWNED |
| MB-5 (§2, §0.4) | Per-string source MPE Y / CC per string / none; 6 per-string ranges 200 c | (branch) `StringBendSource`, `bend_string_range_1..6` | (branch) BEND page | (branch) `Bend.aPerStringBendMovesOnlyItsString` | OWNED |
| MB-6 (§2) | Vibrato source LFO/AT/MPE Z, rate 3-10 (6), depth 5-50 (20), onset 200 ms | (branch) `VibratoSource`, `BendEngine` | (branch) BEND page + popover | (branch) `Bend.vibratoWaitsForItsOnsetThenReachesDepthIn50Ms` | OWNED |
| MB-7 (§2-3) | Quantise none/quarter/semitone/24/22/31/53-EDO/custom with snap strength | (branch) `BendEngine::quantiseTarget`, `bend_quantise/_snap` | (branch) BEND page | (branch) `Bend.quarterToneQuantiseLandsOnTheGrid` | OWNED |
| MB-8 (§2-3) | Custom .scl / .tun loading | (branch) `DSP/Techniques/MicrotonalScale` | (branch) BEND page loader | (branch) `Bend.aLoadedScaleIsTheGrid` | OWNED |
| MB-9 (§2) | Pre-bend via keyswitch/CC, amount -200 c, releases | (branch) `BendEngine::noteOn`, KS 20 | (branch) BEND page | (branch) `Bend.aPreBendStartsFlatAndReleases` | OWNED |
| MB-10 (§2) | Bend curve / release curve linear / exponential / drawn | (branch) `BendEngine::shapeBend/shapeRelease` | (branch) `BendCurveEditor` | (branch) `Bend.theCurvesShapeTheThrow` | OWNED |
| MB-11 (§4) | setBendSourceStack / setBendQuantise; range change at next note-on | (branch) `BendEngine` settings, latched at `noteOn` | n/a | (branch) `Bend.aRangeChangeWaitsForTheNextNote` | OWNED |
| MB-12 (§4) | ModMatrix `PreBendEvent` source class — deferred on owner branch | - | n/a | - | OWNED |
| MB-13 (§5) | Techniques > Microtonal Bends sub-tab | (branch) - | (branch) `TechniquePages:BendPage` | (branch) `TechniquesUi.everySubTabRendersItsControls` | OWNED |
| MB-14 (§5) | Easy bend/vibrato "…" popover (range + vibrato) | (branch) - | (branch) BEND pill hold `TechniquePopover` | (branch) `TechniquesUi.holdOpensThePopoverAndEscapeClosesIt` | OWNED |
| MB-15 (§5) | CHARACTER PLAYING group "Microtonal" section — mirror at CHARACTER foot | (branch) - | (branch) `TechniqueMirrors` | (branch) `TechniquesUi.theCharacterMirrorsAttachTheSameParameters` | OWNED |
| MB-16 (§5) | Fretboard cents badge; heavy bend pushes dot along fret gap | (branch) - | (branch) `TechniqueOverlay` bendArc | (branch) `TechniquesUi.theOverlaysDrawInsideTheirBudget` | OWNED |
| MB-17 (§6) | Cascade: bend compatible with all; bent slap | (branch) `CascadeResolver` | n/a | (branch) `Bend.aBentSlapPitchesCorrectly`, `Cascade.theMatrixIsTheSpecs` | OWNED |
| MB-18 (§7) | Five presets (Whole-Tone, Quarter-Tone Blues, Maqam, Wide Vibrato, Whammy Two-Octave) | (branch) `Presets/TechniquePresets.cpp` | (branch) browser | (branch) `TechniquesUi.thePresetChipFilters` | OWNED |
| MB-19 (§8) | MIDI export per-source SysEx; generic PB split per channel — deferred on owner branch | - | n/a | - | OWNED |
| MB-T1 (§9) | Test: global +100 c on every note | (branch) | n/a | (branch) `Bend.theGlobalBendMovesEveryString` | OWNED |
| MB-T2 (§9) | Test: per-string bend only string 3 | (branch) | n/a | (branch) `Bend.aPerStringBendMovesOnlyItsString` | OWNED |
| MB-T3 (§9) | Test: vibrato onset delay; depth within 50 ms | (branch) | n/a | (branch) `Bend.vibratoWaitsForItsOnsetThenReachesDepthIn50Ms` | OWNED |
| MB-T4 (§9) | Test: quarter-tone snap lands on 50 c | (branch) | n/a | (branch) `Bend.quarterToneQuantiseLandsOnTheGrid` | OWNED |
| MB-T5 (§9) | Test: .scl within 0.5 c | (branch) | n/a | (branch) `Bend.aLoadedScaleIsTheGrid` | OWNED |
| MB-T6 (§9) | Test: pre-bend -200 c releases | (branch) | n/a | (branch) `Bend.aPreBendStartsFlatAndReleases` | OWNED |
| MB-T7 (§9) | Test: range change mid-note no jump | (branch) | n/a | (branch) `Bend.aRangeChangeWaitsForTheNextNote` | OWNED |
| MB-T8 (§9) | Test: bend + slap on low E | (branch) | n/a | (branch) `Bend.aBentSlapPitchesCorrectly` | OWNED |
| MB-T9 (§9) | Test: preset round-trips every field | (branch) | n/a | (branch) `Bend.everyControlRoundTrips` | OWNED |

<!-- counts DONE=0 NO-GUI=0 NO-TEST=0 PARTIAL=0 MISSING=0 OWNED=28 -->
