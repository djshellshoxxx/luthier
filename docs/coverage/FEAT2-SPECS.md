# FEAT2-SPECS coverage

Workstream FEAT2-SPECS (branch `claude/luthier-feat2-specs`). Spec-only
task: no implementation, no parameters, no code. Two parts: six new specs
for genuinely missing GUI/UX features, and a confirm-status pass on six
features that were suspected gaps but needed checking against the current
spec set and `Source/` before writing anything.

## New specs written

| Spec | Closes |
|---|---|
| `spec/ui-scaling.md` | Persistence and cross-format (VST3/AU/CLAP/AAX/Standalone) contract for the 75-200% UI scale `accessibility.md` 4 and `gui-integration.md` 0.8 already require but never say how to remember, per machine. |
| `spec/tuner-and-tuning-reference.md` | A built-in tuner (Reference mode reading `TuningEngine`'s own targets, plus a Live mode pitch tracker against an external signal) and a global `tuning_reference_hz` (A4, 432-446 Hz, default 440) that scales every pitch `engine.md` 3 computes without touching any interval. Neither existed anywhere in the spec set. |
| `spec/midi-learn.md` | Extends `ui-wiring.md` 8's single-mapping wiring: a direct right-click entry point alongside the header-arm one, MPE-dimension and note-class sources (`controllers.md` 1), a mappings list/management popover, and the global-vs-preset resolution order. `ui-wiring.md` 8 and `gui-integration.md`'s MIDI Learn row are otherwise unchanged. |
| `spec/randomize-and-ab.md` | Both the header dice and the header A/B control had a one-line mention (`gui-integration.md` 2) and a keyboard binding but no behaviour spec. Now: Randomize's categories, musical (triangular, bias-toward-current) distribution and non-goals; A/B's two-RAM-buffer model, diff-only click-free flipping reusing each field's own existing crossfade, and its non-serializing, non-undoable nature (`action-and-undo.md` 7). |
| `spec/amp-cab-ir.md` | `engine.md` 11/13 spec the Amp/Cabinet DSP and `tone-match.md` fully specs user IR loading; neither gave either stage a whole-signal-path bypass switch. Adds `amp_bypass` / `cab_bypass`, a quick IR picker mirrored onto the Advanced Column 3 CAB panel (reading/writing `tone-match.md`'s existing slot state, not a second one), and states how `cpu-quality-modes.md` 2.1/2.3's existing truncation surfaces there. |
| `spec/tab-export.md` | `notation-export.md` already fully specs ASCII tab, MusicXML and Guitar Pro export (see confirm-status below - this was **not** a gap). What was missing: `tune-builder.md` 9.3/14 assert notation "falls out of" the Tune's `PerformanceScore` but nothing said how that struct gets built from a Tune's symbolic chord/melody data without a real-time render. Specs `TuneToScore`, the second producer, plus one additive format (a combined print page). Small additive notes added to `notation-export.md` (new 6.6) and `midi-export.md` (new bullet in section 9) naming this spec, per the task brief. |

## Confirm-status: six suspected gaps

Checked against the current spec set and `Source/` (not just against
`DECISIONS.md`/`GAPS.md`, which turned out to be stale on one point - see
the note below). No duplicate specs written for anything already covered.

| # | Feature | Status | Evidence |
|---|---|---|---|
| 1 | Doubler | **spec exists, built** | Full spec in `spec/ambiguity-resolutions.md` section 3 (defaults, 8 parameters, signal path), not just a decision note; `DECISIONS.md` 391-401 records the pedal-slot placement rule. Built: `Source/DSP/Effects/Pedal.h`/`.cpp` (`PedalType::Doubler`), `Source/DSP/Effects/PedalsMod.h`/`.cpp` (`DoublerPedal`), `Source/Parameters.cpp` (legacy-param migration), `Source/Presets/PresetManager.cpp` (migration on load), `Source/UI/Faces/PedalFace.cpp`. Tested: `Source/Tests/DoublerTests.cpp`. |
| 2 | Tone-match | **spec exists, built** | `spec/tone-match.md` (170 lines, complete). Built: `Source/ToneMatch/ToneMatch.h`/`.cpp` (`CabMatch`, `EqMatch`, `IrLibraryPaths`), `Source/UI/ToneMatchPanel.h`/`.cpp`, `Source/Support/IrLibrary.h`/`.cpp`. Tested: `Source/Tests/ToneMatchTests.cpp`. |
| 3 | Sympathetic resonance | **spec exists, built** | Core bridge coupling: `spec/spec.md` (34, 392, 454, 475, 507, 540) and `spec/engine.md` 5.6 (`CouplingMatrix`), always-on, never disabled. The extended air-path/body-mediated coupling this task's brief expected to be missing is **also on disk**: `spec/body-coupling.md` and `spec/string-interaction.md` both exist (see note below). Built: `Source/DSP/Coupling/CouplingMatrix.h`, `Source/DSP/Coupling/BodyCouplingBank.h`, `Source/DSP/String/StringEngine.{h,cpp}`, `Source/Parameters.cpp` (`couplingAmount`, default 0.85). Tested: `Source/Tests/BodyCouplingTests.cpp` (BC-07), `QualityModeRenderTests.cpp` (CQ-30), others. |
| 4 | Round-robin / humanized attack | **partial** | No "round-robin" concept exists anywhere (this is a physically-modelled synth, not sample-based, so round-robin sample variation doesn't apply - not a gap, a category error in the original ask). Its real analogue, **Humanize**, is fully specced (`spec.md` 318-320/366/410, `rhythm-engine.md` 19-21/134-138, `gui-integration.md` 167 Playing-strip macro) and fully built (`Source/Rhythm/RhythmEngine.{h,cpp}`, `GenreKit.cpp`, `StrumGesture.h`, `MidiInterpreter` CC79 wiring). `spec/fingerstyle-attack.md` and `spec/sustain-and-decay.md` **do exist on disk** now (contra the stale `DECISIONS.md` note - see below) but `fingerstyle-attack.md` explicitly self-describes as unfinished: per-string tool assignment and per-finger carry-through are not yet built ("every finger plays as the global tool" today). |
| 5 | In/out meters | **partial** | Spec calls for both in the header (`gui-integration.md` 2: "Meters \| 160 px \| Input meter, output meter, output LED"). Build has only the output LED in the header (`Source/UI/HeaderBar.h`); the real stereo `LevelMeter` component exists but is instantiated in `Source/UI/EasyPanel.h` (Easy Mode's meter column), not the header, and no input meter exists anywhere in `Source/`. `Source/UI/NormalizationBadge.h` (lines 3-10) documents this itself: it sits "beside the output meter (the header's output LED in this build)." Not tracked in `DECISIONS.md`/`GAPS.md` - a real, currently-unacknowledged gap, out of scope for this spec-only workstream to fix. |
| 6 | Tooltips / MIDI Learn | **spec exists, built** | Tooltips: `gui-integration.md` 99 (every header control), `theme.md` 62 (400 ms hover pill), `accessibility.md`, dozens of feature-specific requirements; used across 49 files in `Source/UI/`. MIDI Learn: `ui-wiring.md` section 8; built as `Support::MidiLearn` (`Source/Support/MidiLearn.{h,cpp}`), wired into `Source/UI/HeaderBar.{h,cpp}`, tested across `UndoTests.cpp`, `ReviewRegressionTests.cpp`, `StateModelTests.cpp`, `IntegrationTests.cpp`, `AudioThreadSafetyTests.cpp`. `spec/midi-learn.md` (this workstream) extends the spec on top of this working implementation; see above. |

### Note: a stale claim in `DECISIONS.md`/`GAPS.md`

This task's brief was written on the premise (from `DECISIONS.md` 136-139,
160-161, 218-220, dated 2026-09-24) that the nine phase-2b realism specs -
`string-aging.md`, `environment.md`, `body-coupling.md`,
`harmonic-realism.md`, `string-interaction.md`, `fingerstyle-attack.md`,
`noise-floor.md`, `sustain-and-decay.md`, `tuning-stability.md` - are "not
on disk" / blocked. `ls spec/` shows all nine exist (mtimes 2026-09-26,
two days after that note), and `spec/INDEX.md` 71/73 and `spec/editions.md`
109 already reference them. This affected items 3 and 4 above (both
looked like open phase-2b gaps and were not). `DECISIONS.md` itself is not
edited by this spec-only workstream; noted here so the next reader doesn't
repeat the check from the stale premise.
