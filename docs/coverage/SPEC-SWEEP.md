# SPEC-SWEEP: decisions and fix log

Branch `claude/luthier-spec-sweep`. The per-requirement audit is
`docs/audit/SPEC_SWEEP.md` (generated from `docs/audit/sweep_parts/` by
`scripts/assemble_spec_sweep.py`). This file records how the sweep was done,
the decisions taken while fixing, and the state of the owned workstreams.

## Method

- Every Markdown file under `spec/` (84 files including `proposals/visual-polish.md`
  and the two specs added during the sweep, `output-normalization.md` and
  `cpu-quality-modes.md`) and the feature-stating repo docs (`docs/USER_MANUAL.md`,
  `KEYBOARD_SHORTCUTS.md`, `PLAYING_TECHNIQUES.md`, `PRESET_FORMAT.md`,
  `MIDI_EXPORT_LUTHIER_PROFILE.md`, `TROUBLESHOOTING.md`) was read in full and
  split into actionable requirements: numbered items, MUST/SHALL statements,
  control/parameter/default table rows, UI placement statements and every
  Tests-section entry.
- Each requirement was checked against the code for an implementation, a
  reachable GUI control (a constructed, visible control attached to it) and a
  registered `LUTHIER_TEST`. The earlier auditor's `GAPS_AUDIT.md` (branch
  `claude/luthier-audit`, 43 commits older) was used as a checklist only; every
  row was re-verified.
- Status vocabulary: DONE, NO-GUI, NO-TEST, PARTIAL, MISSING, OWNED, DEFERRED.

## Ownership (checked 2026-09-24)

| Workstream | Specs | Branch / session | Evidence of work |
|---|---|---|---|
| REALISM-A | string-aging, environment, body-coupling | `claude/luthier-realism-a` | 48 files, coverage doc; session completed, not merged |
| REALISM-B | harmonic-realism, string-interaction, fingerstyle-attack | `claude/luthier-realism-b` | 52 files, coverage doc; session completed, not merged |
| REALISM-C | noise-floor, sustain-and-decay, tuning-stability | `claude/luthier-realism-c` | 53 files, coverage doc; session completed, not merged |
| TECHNIQUES | muting-rhythm, two-hand-tapping, microtonal-bends, slide-technique-controls, technique-cascade, engine-technique-layer, gui-techniques-updates | `claude/luthier-techniques` | 80 files, coverage doc; session completed, not merged |
| TUNE-HELP | tune-builder, onboarding, Help | `claude/luthier-tune-help` | 95 files, coverage doc; review-ready |
| VISUAL | guitar-workshop, workshop-ui, guitar-illustration, proposals/visual-polish, piano-roll-chord-display | `claude/luthier-visual` | 147 files; also covers many action-and-undo, qa-polish, performance-budget, installer and gui-integration rows (marked OWNED with evidence) |
| FEAT-SEARCH / STRINGS / ASSIST / RIFFS / JAM / BROWSER / MIC | global-search, animated-strings, auto-articulation, riff-library, jam-mode, preset-browser-previews, mic-placement | sessions running since 16:30, no branch pushed at audit time | treated as OWNED (active sessions) |
| FEAT-NORMALIZE / FEAT-CPU | output-normalization, cpu-quality-modes | specs landed during the sweep | OWNED |
| Release helper | editions, licensing | `claude/luthier-release` (no commits ahead) | deferred to the end by the coordinator |
| CLI easter egg | include.md "easter egg", gui-integration signature-notch pixel | none | deferred to last by the coordinator |

## Phase-2 organisation

Unowned non-DONE rows were split by area over six worktrees (`sweep/state`,
`sweep/rtmidi`, `sweep/dsp1`, `sweep/dsp2`, `sweep/ui`, `sweep/docs`), each
merged back here after its own suites passed. Per-worker decision notes are in
`docs/coverage/sweep-notes/`.

## Decisions

(Collected from `docs/coverage/sweep-notes/*.md` as workers land.)

## Test baseline (integration c29e228 + spec merge, before any fix)

Full suite: 11 of 831 tests fail. Ten are `Combo.*` (owned by the auditor
branch): pairwiseAcrossMajorSettings, everyGuitarTypePlaysEveryPhrase,
everyFactoryPresetPlaysEveryPhrase, presetSwitchUnderARingingNoteDoesNotClick,
modulationRoutesAtFullDepth, snapshotsAndPresetMorph,
advancedRangesUnlockedAtExtremes, sessionStateRoundTripReproducesAudio,
sustainFeaturesStayBounded, seededRandomConfigurations. The eleventh is
`GuiReach::everyAutomatableParameterHasAVisibleControl`: the 14 `scrape_*`
parameters (their controls are on the techniques branch) and `pickup_blend`
(fixed by this sweep) have no control.
