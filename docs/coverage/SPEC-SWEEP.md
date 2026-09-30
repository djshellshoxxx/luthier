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

### Update 2026-09-26

REALISM-A, REALISM-B, REALISM-C and TUNE-HELP were merged into the integration
branch (commits 62a537c, bdd86cc, 8d23818, 4439443) and from there into this
branch. Their OWNED rows were re-verified on this checkout and set to DONE, or to
PARTIAL / NO-TEST / MISSING with a note; the requirements those workstreams left
open are the "OWNER-GAP (landed)" bullets in their `.fixes.md` files.
TECHNIQUES is NOT merged: commit 4439443 is titled "Merge TECHNIQUES and
TUNE-HELP-ONBOARDING" but its only second parent is the tune-help branch, so the
seven technique specs (and the cross-spec rows that point at the techniques
branch) stay OWNED. VISUAL is not merged either.

## Phase-2 organisation

Unowned non-DONE rows were split by area over six worktrees (`sweep/state`,
`sweep/rtmidi`, `sweep/dsp1`, `sweep/dsp2`, `sweep/ui`, `sweep/docs`), each
merged back here after its own suites passed. Per-worker decision notes are in
`docs/coverage/sweep-notes/`.

## Decisions

Each worker's decisions, one line per requirement with the reason, are in
`docs/coverage/sweep-notes/<worker>.md` (state, rtmidi, dsp1, dsp2, ui, docs).
Decisions taken while merging the workers:

- [ER-12 / C-20 / PR-31] A newer-schema preset is refused with the update message
  and leaves the current sound untouched (error-recovery 1 wins over the PROGRESS
  judgement call). The docs worker's test that expected a load was changed to
  assert the refusal.
- [ER-27 / HI-10] A mono main output is refused (host-integration 2, DECISIONS
  C-26); the state worker's layout test expected it accepted and was corrected.
- [UW-5 x MM-14/18/19/20/23/25] MOD source-card edits travel as one
  `ModSourceEdit` applied on the audio thread; the new dsp1 card controls (LFO
  phase, envelope curves / retrigger / loop, sequencer rate, follower string /
  log) were added as fields of that edit instead of calling the setters from the
  message thread.
- [LP-39 x UM-13] Tooltip delay is set in one place
  (`LuthierAudioProcessorEditor::applyTooltipPreference`) and is off in Live Mode.
- [SM-1] The session keeps character, modulation, snapshots, rhythm, routing,
  MIDI Learn and tone-match inside its preset block; `Environment::ENV11` builds
  its legacy session by moving the character block to the top level.
- [CW-21] The nut-wear test reads the note's starting sustain multiplier
  (`LuthierEngine::getNoteSustainScale`); the level-after-2 s version passed with
  the nut term removed.
- Worktree snapshots had committed the `ThirdParty/JUCE` and
  `clap-juce-extensions` symlinks; they are untracked again and `.gitignore` now
  matches them as files too.

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

## Merge of integration bdf9f1b

Merge commit 8ad54d0 brought in `origin/claude/luthier-cloud-session-5lzlix` at
bdf9f1b: VISUAL (workshop visuals, undo tiers, QA/perf/installer,
gui-integration, piano roll groundwork), FEAT-STRINGS, FEAT-JAM,
FEAT-NORMALIZE, FEAT-CPU, FIX-CROSS and the auditor's Combo fixes. There were
32 files with conflicts. How each was resolved, and which side won:

- `CMakeLists.txt`: both. `UiPreferences.cpp` stays in the engine sources
  (sweep, REALISM-C). The `PluginEditor*` filter covers PluginEditorTune
  (sweep) and PluginEditorOnboarding (FEAT-NORMALIZE).
- `Pedal.h` / `PedalsDrive.h` / `EffectsChain.cpp` (JG-4 against
  cpu-quality-modes 2.2): integration's `(effective, nominal, crossfade)`
  signature is the base. The sweep's virtual on `Pedal` now carries that
  signature, so the chain still makes a virtual call and needs no
  `dynamic_cast`.
- `MasterBus`: both. The GD-8 block counter ticks before the
  normalization branch, so the normalized path counts blocks too.
- `Snapshots.cpp`: both exclusions apply. `snapshot_morph` (LP-16) and the
  jam transients are never captured.
- `Parameters.cpp/.h`: the FEAT-JAM block goes in unchanged, and the
  SPEC-SWEEP block stays last in `createLayout` and `ParamIDs`. The sweep's
  `applyToEngine` block (a comment only) moved after REALISM-B, so it is last
  there as well. `IntegrationTests` counts `+ 34 FEAT-JAM + 1 SPEC-SWEEP`.
- Duplicate-parameter check: integration has no snapshot-morph parameter. Its
  "session morph position" is the B-10 fix that keeps `preset_morph_position`
  in the session. `snapshot_morph` stays, and LoudnessRoles classes it as
  Performance, like the preset morph.
- `PluginEditor`: the footer is integration's. The QualityBadge carries CPU,
  and the window paints `quality.badge.latency`. `getFooterText()` (UM-60 /
  TS-16) still returns the whole footer, now through the same string. The
  ER-19 save-error banner and the installer-8 migration banner are both
  posted. The sweep's second `UiPreferences.h` include was dropped as a
  duplicate.
- `PluginProcessor.h/.cpp`:
  - Includes and members come from both sides. OutputNormalization is still
    declared last.
  - Drawer-shut path: the Jam mix and the CT-11 wizard click can both run.
  - After `routing.distribute`: the TM-17 Cab Match signal is written, then
    the Jam aux buses.
  - `tapTempoAt` (the sweep's testable split) carries the Jam tap.
  - `recallSlot` uses integration's `restoreState (soundOnly)` and keeps
    UM-8's slot flag.
- `PresetManager.h/.cpp`:
  - Integration's `onPresetLoaded` is called at the same point as the sweep's
    `onPresetFileLoaded`, so the sweep's hook was dropped as a duplicate. The
    processor's `onPresetLoaded` runs SM-46's `presetFileLoaded()` and then
    the normalization notify.
  - Known keys: both lists.
  - Backup pruning: integration's three roots, each swept by the sweep's
    `pruneOldBackupsUnder` (PF-7 per-category folders).
  - PF-14's absent-means-default reset now respects FEAT-JAM's `keepOnLoad`
    and the jam transients.
- `HeaderBar.cpp`: integration's File menu is the base, with Undo history
  (item 15) and `pushUndoBoundary` on open/import. The sweep's UM-7 split
  (`buildFileMenu` / `handleFileMenuResult`) and GD-30 learn pulse are
  re-applied on top.
- `LiveStrip`: both widgets (LP-11 CC button, JAM pill). A snapshot pad keeps
  GI-72/GI-86 (click loads, Shift-click writes, an empty pad hints) through
  integration's undoable `...AsUserAction` calls. The morph-slider polling was
  dropped because LP-16 attaches the knob to `snapshot_morph`.
- Timers in `CircuitPanel`, `FretboardComponent`, `GuitarBodyComponent`,
  `RoutingPanel` and `OutputLed`: integration's AnimationPolicy registration
  starts them, at the sweep's GD-2/GD-8 `kRefreshHz` rates. The fretboard
  slide-bar ease follows the applied rate and integration's instant-at-Off
  rule, and keeps GD-14's stale timestamp. `OutputLed` keeps GD-8's stale
  dark and 400 ms red hold, plus integration's stepped-readout latch at Low.
- `GuitarBodyComponent` detune: integration's undo entry is pushed, then the
  value goes through the sweep's UW-5 command queue. Integration wrote the
  engine from the message thread there.
- `RhythmPanel` capo: the sweep's UW-2 (capo is the `capo_fret` parameter),
  plus integration's "Change rhythm capo" undo entry.
- `ModMatrixPanel`:
  - Integration's labelled 24 px rows and drag handle are the base. The
    sweep's cards controls (MM-14/18/19/20/23/25) use its `sliderRow`, and
    the phase and sequencer-rate sliders are named.
  - `preferredHeight` fits eight rows.
  - The route depth editor is the sweep's (MM-40, depth and offset), and
    `applyTypedValue` now pushes integration's 3.6 undo entry.
- `PracticePanel`: both. There are GD-31 LED helpers and integration's
  `editLayer`. The scale box is integration's undoable change with the sweep's
  PT-37 custom scale.
- `ToneMatchPanel`: the sweep's TM-5 worker thread is kept, and the "Load IR"
  undo entry moved into `finishAnalysis`. Integration ran the deconvolution on
  the message thread.
- `EasyPanel`, `AdvancedPanel`, `Widgets.h`, `LiveStrip.h`: both (GD-10
  arrow width counted in the rhythm row's budget, AR-15 MOD range tab).
- `Tests/IntegrationTests.cpp`: the count keeps every term.

Changes outside the conflict hunks, needed by the merge:

- `OutputNormalization::stripPresetIdentity` drops the processor blocks the
  sweep added to a preset (SM-1). Modulation, routing, character and
  tone-match are hashed from the live modules already. Snapshots, MIDI Learn
  and rhythm are performance, so capturing a snapshot does not trigger a
  recalibration.
- `Resources/NormalizationFactory.json` was regenerated
  (`scripts/regen_normalization_factory.sh`, 900 entries). Its hashes and
  levels move with the sweep's presets and audio.
- `Source/Tests/Golden/NormalizationOffHashes.json` was regenerated because
  the sweep changes audio. The same-build A/B half of ON-02 passes.
- The session writes `midiLearn` at its root again. The preset block carries
  mappings only when there are some (live-performance 11), so undo (3.12),
  A/B and the host session need the exact set.
- AnimationPolicy (CQ-22): `NextStrumArrow` registers itself. `LuthierKnob`
  (the shared ModArcHub), `LiveActionButton`, `MorphSetupPanel` and
  `MonitorSetupPanel` are allow-listed as poll-only.
- Tests adjusted:
  - `JM36` allows the SPEC-SWEEP block after the Jam block.
  - UndoCoverage settles one block before reading audio-thread state.
  - `cc11MovesTheMasterLevel` sets aside a CC 11 expression calibration from
    the user config.
  - `CQ11` does not count a level switch as a click when the signal has
    already rung out (second difference below 1e-4, about -80 dBFS). The one
    switch it flagged was at 10 s, where the value went from 1.9e-5 to 3.6e-5.

Tests after the merge. The targeted suites were run from `build/`, because
the Golden and CQ source scans resolve relative to the working directory.
These still fail, and each one fails the same way on a pure integration build
of bdf9f1b (`/home/user/wt/int`) or on the pre-merge sweep tip:

| Test | Also fails on |
|---|---|
| `Combo.snapshotsAndPresetMorph` | bdf9f1b (Combo, owned by the auditor) |
| `GuiReach.everyAutomatableParameterHasAVisibleControl`: `macro_assign_a/b`, `scrape_*`, `slap_*` | bdf9f1b, whose list also has `pickup_blend` |
| `CpuQuality.CQ10` (the latched dispersion stages) | bdf9f1b |
| `CpuQuality.CQ13` (a scenario ratio; this is timing) | bdf9f1b |
| `CpuQualityUi.CQ22`: NormalizationBadge, NormalizationOptionsGroup, NormalizationCaption, JamPill | bdf9f1b |
| `MidiExport.luthierRoundTripNullsEveryFactoryPreset`: the first render of "Clean Double-Cut Funk" does not repeat (-31.9 dB) | the pre-merge sweep tip 47c23c5, with the same message. It passes on 366b772, so this is a sweep regression that still needs fixing. |

`EBow.theHarmonicChoiceTakesTheString` failed once in the long run. It passes
when run alone, both here and on bdf9f1b.

## Merges of round 2 and integration 718f2b0; known failures on the sweep tip

Round-2 worker branches (ui, rtmidi, dsp1, dsp2) and integration 718f2b0 are
merged. Fret wear reached the buzz twice (dsp1 CW-12's per-note multiplier and
dsp2 FB-21's worn-crown geometry); the engine now feeds only the geometry, which
also lets the worn fret itself clear. `NormalizationFactory.json` and the golden
hashes were regenerated for the merged audio.

Failing on the sweep tip, and why:

| Test | Also fails on pure integration 718f2b0? | Note |
|---|---|---|
| `Combo.*` (auditor-owned) | yes | unchanged ownership |
| `GuiReach.everyAutomatableParameterHasAVisibleControl` | yes | `macro_assign_a/b`, `scrape_*`, `slap_*` (their controls are on the unmerged techniques branch); `pickup_blend` is fixed here |
| `HostState.aSessionSurvivesThePrepareThatFollowsIt` | yes | came with 718f2b0 (it dropped the fixed-point nudge; skewed parameters now move by one float step on a restore + prepare) |
| `CpuQualityUi.CQ10`, `CQ13`, `CQ22` | yes | CQ22 lists NormalizationBadge / NormalizationOptions / JamPill |
| `CpuQuality.CQ12_everyFactoryPresetAtEveryLevel` | yes | CPU-time ordering; a different preset set fails on each run under load |
| `EBow.theHarmonicChoiceTakesTheString` | intermittent | passes alone and in its suite; its own comment records it as a known intermittent |
| `Normalization.ON03_ON04_FactoryCombinationsLandOnTarget` | no (passes there) | `p34_g22` lands at -16.95 LUFS against -18 +-1. The render calibration (fresh processor, calibration mode, fixed play head) measures the phrase 1.05 dB quieter than the live instance plays it; on integration the same gap is 0.75 dB. Live and state-restored instances render identically (-21.39 both), so the gap is inside the calibration render, not state fidelity; the sweep's sound changes (strum direction, pickups, part acoustics) moved this combination 0.3 dB and exposed it. Left for FEAT-NORMALIZE: widen the stimulus/tolerance or find what the calibration render does differently. |
