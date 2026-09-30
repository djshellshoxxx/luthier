# Codex spec/doc audit coverage

Base: `claude/luthier-cloud-session-5lzlix` at `718f2b0e726b532317298e88b07d760c2129991b`.
Lane: specs/docs and focused tests. No DSP, UI wiring or parameter edits.
This is batch 1, not a completed whole-project audit. Existing reports are leads, not fresh verification.

## New coverage

| Requirement | Test |
|---|---|
| Exclude Combo consistently in execution/listing; preserve positive filters | `TestRunner` (4 tests) |
| Future schema payload survives; another preset clears it; rejection retains it | `PresetForwardCompatibility` (3 tests) |
| Learn and apply all 124 non-reserved CCs; reserved CCs leave learn active | `MidiLearnCoverage` (2 tests) |

## File inventory

Each partial row names the behavior actually inspected. Pending rows have not been behavior-verified. Administrative files are retained in the inventory so a later batch can classify them explicitly.

| File | Status |
|---|---|
| `docs/CHANGELOG.md` | Pending |
| `docs/COORDINATOR_PLAN.md` | Pending |
| `docs/GUITAR_PHYSICS.md` | Pending |
| `docs/HANDOFF.md` | Pending |
| `docs/KEYBOARD_SHORTCUTS.md` | Pending |
| `docs/KNOWN_ISSUES.md` | Pending |
| `docs/MIDI_EXPORT_LUTHIER_PROFILE.md` | Pending |
| `docs/PLAYING_TECHNIQUES.md` | Pending |
| `docs/PRESET_FORMAT.md` | Partial: backup path corrected; future payload tests added; F01-F03 remain. |
| `docs/PRODUCT_DESCRIPTION.md` | Partial: scale range corrected to 75–200%; remainder pending. |
| `docs/PRODUCT_OVERVIEW.md` | Partial: scale range corrected to 75–200%; remainder pending. |
| `docs/RELEASING.md` | Pending |
| `docs/THEME_AS_BUILT.md` | Pending |
| `docs/TROUBLESHOOTING.md` | Pending |
| `docs/USER_MANUAL.md` | Partial: MIDI Learn reserved CCs clarified; remainder pending. |
| `docs/spec-coverage.md` | Pending |
| `spec/CLAUDE_CODE_BRIEF.md` | Pending |
| `spec/DECISIONS.md` | Pending |
| `spec/GAPS.md` | Pending |
| `spec/INDEX.md` | Pending |
| `spec/JUCE_CLAUDE_GUIDELINES.md` | Pending |
| `spec/PROGRESS.md` | Pending |
| `spec/README.md` | Pending |
| `spec/REVIEW.md` | Pending |
| `spec/TODO.md` | Pending |
| `spec/accessibility.md` | Partial: scale range only; existing Accessibility::uiScaleStepsAndFontFloor. |
| `spec/action-and-undo.md` | Pending |
| `spec/advanced-ranges.md` | Pending |
| `spec/ambiguity-resolutions.md` | Pending |
| `spec/amp-cab-ir.md` | Pending |
| `spec/animated-strings.md` | Pending |
| `spec/auto-articulation.md` | Pending |
| `spec/bass-techniques.md` | Pending |
| `spec/body-coupling.md` | Pending |
| `spec/character-wear.md` | Pending |
| `spec/controllers.md` | Pending |
| `spec/cpu-quality-modes.md` | Partial: CQ-12 preset ordering fails; test/spec policy differs; F09-F10. |
| `spec/editions.md` | Pending |
| `spec/engine-technique-layer.md` | Pending |
| `spec/engine.md` | Pending |
| `spec/environment.md` | Pending |
| `spec/error-recovery.md` | Pending |
| `spec/factory-content.md` | Pending |
| `spec/file-formats.md` | Partial: preset schema, forward compatibility and backup paths; F01-F03 remain. |
| `spec/fingerstyle-attack.md` | Pending |
| `spec/fret-buzz.md` | Pending |
| `spec/global-search.md` | Pending |
| `spec/gui-engine-dataflow.md` | Pending |
| `spec/gui-integration.md` | Partial: existing GUI reachability test fails; F06; no wiring edits. |
| `spec/gui-techniques-updates.md` | Pending |
| `spec/guitar-illustration.md` | Pending |
| `spec/guitar-workshop.md` | Pending |
| `spec/harmonic-realism.md` | Pending |
| `spec/host-integration.md` | Partial: host restore/prepare test fails; F07; serialization policy pending. |
| `spec/include.md` | Pending |
| `spec/input-routing.md` | Pending |
| `spec/installer.md` | Partial: preset overwrite backup location only. |
| `spec/instruments/README.md` | Pending |
| `spec/instruments/acoustic-bass-guitar.md` | Pending |
| `spec/instruments/chapman-stick.md` | Pending |
| `spec/instruments/chitarra-sarda.md` | Pending |
| `spec/instruments/composite-neck.md` | Pending |
| `spec/instruments/extended-range-bass.md` | Pending |
| `spec/instruments/guitarron.md` | Pending |
| `spec/instruments/tenor-guitar.md` | Pending |
| `spec/issues.md` | Pending |
| `spec/jam-mode.md` | Pending |
| `spec/licensing.md` | Pending |
| `spec/live-performance.md` | Pending |
| `spec/mic-placement.md` | Pending |
| `spec/microtonal-bends.md` | Pending |
| `spec/midi-export.md` | Pending |
| `spec/midi-learn.md` | Partial: CC learn/application only; F04 remains; advanced learn requirements pending. |
| `spec/modulation-matrix.md` | Pending |
| `spec/muting-rhythm.md` | Pending |
| `spec/noise-floor.md` | Pending |
| `spec/notation-export.md` | Pending |
| `spec/onboarding.md` | Pending |
| `spec/output-normalization.md` | Partial: existing ON-02/ON-03 gates fail; F08; remaining requirements pending. |
| `spec/part-acoustics.md` | Pending |
| `spec/performance-budget.md` | Pending |
| `spec/piano-roll-chord-display.md` | Pending |
| `spec/pick-noise.md` | Pending |
| `spec/practice-tools.md` | Pending |
| `spec/preset-browser-previews.md` | Pending |
| `spec/proposals/visual-polish.md` | Pending |
| `spec/qa-polish.md` | Pending |
| `spec/randomize-and-ab.md` | Pending |
| `spec/rhythm-engine.md` | Pending |
| `spec/riff-library.md` | Pending |
| `spec/routing-io.md` | Pending |
| `spec/slide-guitar.md` | Pending |
| `spec/slide-technique-controls.md` | Pending |
| `spec/spec.md` | Pending |
| `spec/state-model.md` | Partial: host state restore/prepare test only; F07; remaining layering requirements pending. |
| `spec/string-aging.md` | Pending |
| `spec/string-interaction.md` | Pending |
| `spec/string-scraping.md` | Pending |
| `spec/string-slap-technique.md` | Pending |
| `spec/string-squeak.md` | Pending |
| `spec/strum-dynamics.md` | Pending |
| `spec/sustain-and-decay.md` | Pending |
| `spec/tab-export.md` | Pending |
| `spec/technique-cascade.md` | Pending |
| `spec/theme.md` | Pending |
| `spec/tone-match.md` | Pending |
| `spec/tune-builder.md` | Pending |
| `spec/tuner-and-tuning-reference.md` | Pending |
| `spec/tuning-stability.md` | Pending |
| `spec/two-hand-tapping.md` | Pending |
| `spec/ui-scaling.md` | Partial: six scale values only; window/storage requirements pending. |
| `spec/ui-wiring.md` | Partial: MIDI Learn CC test requirement only; F04 remains. |
| `spec/updates-telemetry.md` | Pending |
| `spec/volume-knob-interaction.md` | Pending |
| `spec/workshop-ui.md` | Pending |

## Validation

- `scripts/setup_linux.sh`: configured after installing missing CMake in the environment.
- `ninja -C build -j2 LuthierTests`, then incremental `-j4`: both exited 0; output redirected to logs.
- Focused runner: 9 tests, 662 checks, all passed (exit 0).
- `--list --skip=Combo`: 1,356 selected tests; no Combo entries.
- Full `--skip=Combo` run: exit 1; 7 of 1,356 tests failed, 160 of 801,364 checks, 1,980.4 s. F06-F09/F11-F12 record all seven failing test names and owner lanes.
- DSP, UI wiring and parameter files were not modified.
- Fresh-process reproduction (without running any new tests): GUI reachability, host restore, CQ-10 and CQ-22 all failed again (4/4, 37/122 checks). The expensive golden/performance checks were not repeated.
