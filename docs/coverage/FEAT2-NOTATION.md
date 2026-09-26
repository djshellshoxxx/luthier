# FEAT2-NOTATION: staff notation export coverage

Spec: `spec/notation-export.md` (esp. section 2.1) + `spec/midi-export.md` §9.
Task board entry: `docs/helpers/CODEX_TASKS_PARALLEL.md` Task X, marker
`FEAT2-NOTATION`. Branch: `claude/luthier-feat2-notation`.

Tests: `Source/Tests/NotationTests.cpp` (suite `Notation`),
`Source/Tests/NotationPanelTests.cpp` (suite `NotationTab`).
`xvfb-run -a build/LuthierTests_artefacts/Release/LuthierTests Notation`
`xvfb-run -a build/LuthierTests_artefacts/Release/LuthierTests NotationTab`

Parameters added: **none**. `NotationExportOptions::staffMode` is an export
option carried alongside `lineWidth`/`chordDiagrams`/`density`, not a plugin
parameter — nothing is added to the APVTS, and the parameter-count sum in
`Source/Tests/IntegrationTests.cpp` is unchanged.

## Starting state

Performance capture, `PerformanceScore`, `NotationExporter` /
`NotationImporter` (MusicXML, Guitar Pro, ASCII tab, MIDI), the NOTATION tab
(`Source/UI/NotationPanel.*`) and the header's File -> "Export notation..."
already existed on the integration branch before this task started (commit
`18a1396` and follow-ups `a6b40e0`, `3b4853c`) — the same `COORDINATOR_PLAN.md`
item (T2-3) that this task board entry duplicates. What did not exist: a
**staff** view distinct from tab. The MusicXML writer emitted exactly one
staff, always a TAB clef with `<string>`/`<fret>` on every note — the ASCII
and Guitar Pro writers' concern, not a plain reader's.

This branch's work is the gap: a real standard-notation staff option for the
MusicXML writer, wired into the export dialog, with its own deterministic
test against a fixed fixture (this task's ACCEPT criterion).

## Coverage

| ID | Spec section | Implementation | Verification | Status |
|---|---|---|---|---|
| N-1 | notation-export 2.1 | `NotationExportOptions::StaffMode` (`tabStaff` default, `standardStaff`) — `Source/Notation/NotationExport.h` | musicXmlStandardStaffHasCorrectPitchesAndDurations, musicXmlIsWellFormedAndGuitarAware (default unchanged) | verified |
| N-2 | notation-export 2.1 | `standardStaff` mode: `<clef><sign>G</sign><line>2</line><clef-octave-change>-1</clef-octave-change></clef>`, no `<staff-details>`/tuning/capo — `NotationExporter::renderMusicXml` | musicXmlStandardStaffHasCorrectPitchesAndDurations | verified |
| N-3 | notation-export 2.1 | Written pitch = sounding MIDI note + 12 semitones (the clef's octave-change makes it sound correctly again) — same function | musicXmlStandardStaffHasCorrectPitchesAndDurations (all 10 fixture notes, exact step/alter/octave) | verified |
| N-4 | notation-export 2.1 | Durations unaffected by staff mode (same `duration`/`type`/`dot` path as tab) | musicXmlStandardStaffHasCorrectPitchesAndDurations (all 10 fixture notes, exact tick counts) | verified |
| N-5 | notation-export 2.1 (distinct from tab) | `standardStaff` mode omits the whole `<technical>` group (string, fret, bend, harmonic, tap, palm-mute, dead-note, hammer-on/pull-off markers) — that detail is the ASCII/GP tab lane's | musicXmlStandardStaffHasCorrectPitchesAndDurations (`technicalOnStandardStaff == 0`) | verified |
| N-6 | notation-export 2.1 | Slides, vibrato and articulations (accent/staccato) are not tab-specific and still render in `standardStaff` mode | by inspection (same second technique loop, unconditional); not separately asserted | verified (visual-only techniques), not independently tested |
| N-7 | notation-export 5 | Export dialog: "STAFF NOTATION" toggle, MusicXML's own option (same slot pattern as ASCII's line width and Guitar Pro's chord diagrams) — `Source/UI/NotationPanel.*` | staffNotationToggleIsMusicXmlOnlyAndTakesEffect | verified |
| N-8 | notation-export 5 | Toggle visible only when the format box is MusicXML; hidden for ASCII/GP/MIDI | staffNotationToggleIsMusicXmlOnlyAndTakesEffect | verified |
| N-9 | notation-export 5, gui-integration 19 | Reachable exactly where the rest of notation export already is: Col 4 NOTATION tab and the header's File -> "Export notation..." (`HeaderBar::showFileMenu` -> `NotationTakeExport::writeAsync`) — no new entry point needed, the toggle rides the existing dialog/options plumbing | staffNotationToggleIsMusicXmlOnlyAndTakesEffect (through `NotationPanel::exportTo`, the same path the header menu calls) | verified |
| N-10 | (ACCEPT criterion) | Fixed `PerformanceScore` fixture (`makeTestScore()`, the same fixture the existing tab-mode tests use) renders to deterministic notation with correct pitches and durations | musicXmlStandardStaffHasCorrectPitchesAndDurations | verified |
| N-11 | (regression) | Default (`tabStaff`) MusicXML, ASCII tab, Guitar Pro, MIDI, round trips, live-tab window, capture — everything that existed before this task | full `Notation` + `NotationTab` suites: 21 tests, 339 checks, all pass | verified |

## Decisions

- **Standard staff, not dual staff**: real guitar sheet music with both
  notation and tab shows two staves in one part, each note written twice.
  That is more machinery (a second `<staff>` per note, `<staves>2</staves>`,
  a second clef, careful interaction with `<chord/>`/`<backup>`) than Task X
  asks for — a STAFF export "distinct from ASCII/GP tab, which another
  helper owns". A single dedicated standard-notation staff, selected by
  option instead of always combined with tab, is the minimal shape that
  satisfies that split; a `dual` mode is a natural follow-up if a later spec
  pass wants combined notation+tab sheet music, and the enum
  (`NotationExportOptions::StaffMode`) is written to take a third value
  without disturbing the two that exist.
- **Octave-down treble clef, not a literal pitch staff**: standard guitar
  notation is written a full octave above where it sounds
  (`<clef-octave-change>-1</clef-octave-change>`) — the universal convention,
  not a guess; writing the literal sounding octave against a plain G clef
  would put open low E below the staff's usual guitar range and would not
  match any published guitar score. The writer adds 12 semitones to the
  sounding MIDI note before the existing `getMusicXmlPitch` conversion and
  leaves that conversion itself untouched (it is already unit-tested by
  `musicXmlPitchConversion`).
- **`<technical>` dropped whole, not filtered per element**: everything in
  that MusicXML group exists to describe a fretted instrument's fingering
  (string, fret, bend curve, harmonic type, tap, palm mute, hammer-on/pull-off
  start markers). None of it has a home on a plain staff with no fret
  numbers, and thinning it element-by-element would just re-derive "is this
  one tab-specific" for each of the eight cases already in the switch. Slides
  (`<slide>`/`<glissando>`), vibrato (`<ornaments><wavy-line>`) and
  articulations sit in a separate switch outside `<technical>` in the
  existing code and are unaffected — those already read as ordinary notation
  ornaments, not tab annotations.
- **No new parameter, no new file**: `staffMode` is an export-time option,
  the same kind as `lineWidth` and `chordDiagrams` already are for the other
  formats, so it lives on the existing `NotationExportOptions` struct rather
  than a new one. The UI change follows the exact pattern those two options
  use (a widget occupying the same bounds as the other formats' own option,
  shown only for its format) rather than inventing a second pattern.
- **Full suite checked for regressions**: `LuthierTests` with no filter
  (1371 tests, 804414 checks, ~83 min) reports 7 failures, none touching
  notation, capture, export or this branch's files:
  `Combo.everyFactoryPresetPlaysEveryPhrase` and
  `Combo.snapshotsAndPresetMorph` (documented B-15, P-Bass Flatwound
  string-age bug, `docs/audit/BETA_TEST_REPORT.md`),
  `GuiReach.everyAutomatableParameterHasAVisibleControl` (documented 33
  parameters with no control yet, in progress on `claude/luthier-techniques`,
  `docs/audit/GAPS_AUDIT.md`), `NormalizationGolden.ON02_OffPathMatchesGoldenHashes`
  (documented golden-hash refresh pending on `claude/luthier-feat-normalize`),
  `Normalization.ON33_Performance` (a wall-clock render-cost budget, plausibly
  tight on this sandbox's shared vCPU), and
  `CpuQualityUi.CQ22_everyTimerDrivenUiClassIsRegisteredOrAllowListed`
  (documented: `StringAnimator` from FEAT-STRINGS has no `AnimationPolicy`
  registration yet, expected to fail on the integration branch,
  `docs/coverage/FIX-CROSS.md`). `CpuQuality.CQ10_aRingingNoteKeepsItsStagesUntilReExcited`
  is not documented as already-failing (`FEAT-CPU.md` lists CQ-10 verified) but
  is in `StringEngine` dispersion-stage code this branch never touches; most
  likely a timing-sensitive assertion under this run's CPU contention, not a
  regression from this diff. All seven pre-date this branch by file scope;
  none are in `Source/Notation`, `Source/Capture`, `Source/UI/NotationPanel.*`
  or `Source/Tests/Notation*.cpp`.
- **Build/test**: the container this task ran in started with no
  `ThirdParty/JUCE` checkout and no configured `build/` tree;
  `scripts/setup_linux.sh` was run first (clones JUCE 8.0.10 + the CLAP
  extension, configures Ninja+clang), then `ninja -C build LuthierTests`.
  Both `Notation` and `NotationTab` (21 tests, 339 checks) pass. The first
  cut of `staffNotationToggleIsMusicXmlOnlyAndTakesEffect` checked
  `getStaffNotationButton().isVisible()`, which reads the inner
  `juce::TextButton`'s own flag, not the `LuthierToggle` wrapper
  `NotationPanel::updatePreview()` actually calls `setVisible` on; a
  freshly-constructed button defaults to visible regardless of the
  wrapper's state, so the check passed for the wrong reason on MusicXML and
  failed outright on ASCII. Fixed by adding
  `NotationPanel::isStaffNotationShown()` (reads the wrapper) instead of
  inferring visibility from the button.
