# Spec coverage

Audit of every Markdown file under `spec/` against the build, as
`CLAUDE.md` requires before and during implementation.

- **Audited:** 2026-09-23, starting at commit `173a292` (branch `master`).
  **Refreshed** for `a406915` (guitar renderer, guitar-shop theme, queued
  MIDI while parked), `e60d708` (family switching, string counts per type)
  and `0d225f0` (Workshop bench model, spectrum delta, IR caching); then for
  `d45fcd6` / `bc25f89` (Workshop bench UI, family category, field editing),
  `0052205` (Easy mode strips; `input_gain`, `output_mix`, `stereo_width`,
  `macro_character`), `25e6139` (trademark sweep), `d284547` (Tune Builder
  model) and the integrated MIDI export model (`Source/Export/*`); then for
  `8fb9ec9` (renders repeat after a reset), `a357142` (MIDI OUT tab, live
  Luthier SysEx, Poly chord timing), `a901c72` (Aux 8, per-string buses by
  name) and the physical feedback loop; then for `14864ca` (feedback loop
  committed), `9c672aa` (E-Bow), `f904956` (Doubler pedal), `5ccf238`
  (guitar migration table, trademark scan in the suite), `18a1396`
  (performance capture wired, NOTATION tab), `980b07e` (practice routines
  model) and `52cb9d7` (amp and pedal faces, standalone). Rows cite what is
  committed at `52cb9d7`; the assistants' uncommitted work in the working
  tree at this refresh (preset morph, rubric voicer, tune player and TUNE
  tab, PRACTICE setup panel, faces on the AMP section and racks) is not
  counted.
- **Spec set:** `spec/` (CLAUDE.md says `specs/`; the folder on disk is
  `spec/`). Inventory command: `rg --files spec -g '*.md'` - 63 files.
- **Precedence:** `spec/CLAUDE_CODE_BRIEF.md` "Handling conflicts between
  docs" (22 levels), plus the user decision of 2026-09-23 that
  `spec/proposals/visual-polish.md` section 6 overrides `theme.md` for
  this plugin.
- **Test evidence:** the commit messages record 531 tests passing at
  `a357142` (including every `MidiExport` test, the factory-preset null
  among them, after the reset-determinism fix) and 534 at `a901c72`
  (adding `PluginBuses`), then 540 at `14864ca` (`Feedback`), 544 at
  `9c672aa` (`EBow`), 549 at `f904956` (`Doubler`), 553 at `5ccf238`
  (`GuitarMigration`, `Trademarks::sourceTreeHasNoUnmarkedBrandNames`), 568
  at `18a1396` (`Capture`, `NotationTab`), 586 at `980b07e`
  (`PracticeRoutine`) and 595 at `52cb9d7` (`Faces`). No log was available
  to this audit and no build or test was run for it. One caveat:
  `LUTHIER_ALLOCATION_COUNTER` is not defined anywhere in the build, so the
  no-allocation assertion in `Capture::capturingTenThousandNotesDoesNotAllocate`
  compiles out (NOT-7.1-02).

## How to read this file

Each spec file has a section with one row per actionable requirement: a
concrete behaviour, value, UI element, file-schema field, budget, or test the
spec demands. Trivially related lines are collapsed into one row; nothing is
dropped. Each spec's own "Tests" section becomes rows too.

`ID` is `<file prefix>-<section>-<counter>`, for example `GI-11.2-03`.
`Implementation location` cites `path:line` or `path` + symbol.
`Verification` names the test (`File.cpp` `Suite::name`) whose assertions
check the requirement, or says what kind of evidence is missing.

### Status legend

| Status | Meaning |
|---|---|
| `verified` | Evidence exists: a named test in `Source/Tests/*.cpp` whose assertions check this requirement, or explicit evidence recorded in the row. A GUI control, a class, or a passing build alone does not qualify. |
| `implemented` | Code exists at the cited place; no test proves the requirement. |
| `partial` | Some of the requirement is built; the row says what is missing. |
| `pending` | Not implemented. |
| `deferred` | Deliberately postponed; the row gives the reason. |
| `blocked` | Cannot proceed; the row gives the reason. |
| `n/a` | Not a product requirement (process guidance, superseded, informational). |

# Release blockers (summary)

**Not shippable.** None of `qa-polish.md` section 0's five gates is green,
and the BRIEF's definition of done (every GI 19 row in the UI, every spec
test passing, human check, READY TO SHIP marker) is unmet.

| | Rows | Share |
|---|---|---|
| verified | 661 | 32% |
| implemented (no proving test) | 447 | 21% |
| partial | 383 | 18% |
| pending | 533 | 26% |
| blocked | 28 | 1% |
| deferred | 4 | <1% |
| n/a | 25 | 1% |
| **Total** | **2081** | |

1,391 required rows (everything except `verified`, `deferred`, `n/a`) lack
evidence. Phase 1 and the built realism specs (advanced ranges, circuit,
pick/squeak/buzz noise, slide, parts model, part acoustics) are in good
shape; everything after them is largely unbuilt.

Biggest unimplemented areas, roughly in order of size and ship impact:

1. **Phase 2b specs missing** (9 files; Conflict C-02, kept blocked by
   DECISIONS). They block the reading order and five phase-5b dependencies
   (Body Tap, multi-source excitation, fretting-hand mutes, fingerstyle
   tools, nut/tuner physics).
2. **Phase 5b technique layer absent** (~170 rows): ScrapeEngine,
   SlapEngine, TapEngine, MuteEngine, CascadeResolver, slide technique
   controls, microtonal bends, TECHNIQUES tab and Playing-strip pills.
3. **Workshop bench remainder** (`workshop-ui.md`, 3 pending, 12 partial):
   the bench is built and tested (`d45fcd6`, `bc25f89`); live overlays,
   fret-wear / pick / slide / capo drags, the 700 px drawer dropdown, the
   30 ms audition crossfade, the audio-thread check and A/B persistence
   remain.
4. **Guitar illustration remainder** (`guitar-illustration.md`, 10
   pending, 12 partial): the renderer, family switching and 11 tests landed
   (`a406915`, `e60d708`); zoom and pan, heatmap / pick / pickup-pulse
   overlays, browser thumbnails and Workshop drag targets remain.
5. **Tune Builder UI and playback** (14 pending, 13 partial): the model,
   file format and ten templates are built and tested (`d284547`); the TUNE
   tab, transport, engine playback, audio export, MIDI-in recording, Sing,
   and the Luthier-profile MIDI path (C-53) are not.
6. **MIDI export / import remainder**: the model, the MIDI OUT tab and the
   EVENTS / WORKSHOP SysEx sources are built and tested (`a357142`); import
   targets, tune-builder and File-menu entry points and `.midprofile`
   installer registration are pending, and the plugin still declares
   `NEEDS_MIDI_OUTPUT FALSE` (`CMakeLists.txt:29`) while `producesMidi()`
   returns true, so a VST3 host gets no MIDI out bus.
7. **Performance capture remainder**: the capture is wired and the NOTATION
   tab is built and tested (`18a1396`), but only string activity reaches the
   capture: no techniques, chord symbols, bass techniques or slide-bar
   events, so the chord history is empty in use; no fretboard tab dots, Mono
   chord extraction, marked-region range or worker-thread export; the
   capture's no-allocation check compiles out (`LUTHIER_ALLOCATION_COUNTER`
   undefined).
8. **Strum dynamics and bass techniques** (51 pending): crossing velocity,
   strikers, chucks, slap/pop/ghost, bass defaults.
9. **ui-wiring threading contract**: no command/result queue, no display
   FIFO; guitar swaps still park the audio thread (notes are now queued,
   not dropped; the off-thread part swap DECISIONS C-09 promises is
   pending).
10. **Easy Mode layout and column-4 tabs** (GI 3, 4.4, 19): no rig strip,
    playing or tone strip; 6 of 13 (or 14) tabs missing; GI section 22 tests
    absent.
11. **Ambiguity resolutions not built**: preset morph (uncommitted),
    chord-voicer `Bass` style, transition scoring and 4.7 tests, crossing
    velocity, Aux 1 pre/post toggle. Built and tested: the feedback loop,
    E-Bow, Doubler pedal and guitar migration table. The PRACTICE tab and
    its processor hooks are also open (model built, `980b07e`).
12. **Performance and QA infrastructure**: no per-module CPU/memory
    measurement, no CI, no allocation or lock trap, no CPU relief ladder,
    pluginval last run 2026-09-19; host matrix, MIDI-guitar/MPE hardware and
    blind listening tests never performed.
13. **Onboarding and installer**: no welcome banner, tour, first-run tree
    or reset-to-first-run; no installers, signing, notarisation, portable
    or content-update packages; AU target not declared; macOS and Linux
    blocked on this Windows-only machine.
14. **Legal and content**: trademark sweep done (`25e6139`, `Trademarks::*`) but the
    legal review is open (C-17); no `THIRD_PARTY_LICENCES.txt`;
    placeholder homepage, support email and service URLs; factory presets
    do not match `factory-content.md` 1; the ten tune templates ship, but
    no example tunes, setlists, backing tracks or example MIDI; no EULA.
15. **Localisation and accessibility**: English-only catalog (242 keys),
    14 ship locales untranslated, many hard-coded strings; no fretboard or
    meter accessibility children; no screen-reader, Tab-walk or reflow
    tests.
16. **Undo and error handling**: no state boundaries or 200 ms grouping,
    most structural edits push no undo entry; no crash-dump writer; banner
    policy, MIDI-flood, device-loss and corrupt-config responses missing.
17. **Visual polish** (user priority, TODO V): the guitar-shop theme and
    guitar lighting landed (`a406915`); amp and pedal faces with model knob
    caps are built and tested as painters (`52cb9d7`) but not shown on the
    AMP section or racks; live tube glow, VU, room light and the accent
    picker are pending.

Also open from TODO: pick-scrape trigger (3f),
slide technique controls (5b), audit items 14/14b/14c. Not in TODO at this
refresh: chambering's feedback coupling (PA-2.1-02), chord symbols and
slide / bass events into the capture (NOT-4-01, NOT-6-02), the capture's
allocation counter (NOT-7.1-02), the feedback LED's 30 Hz rate (GED-22-01),
and a migration-time preset backup (FF-2-02).

# Inventory

All 63 files from `rg --files spec -g '*.md'` on 2026-09-23, plus the nine
phase-2b files INDEX names that are not on disk. Counts are coverage rows
(conflict rows excluded).

| File | Phase / INDEX # | Rows | verified | implemented | partial | pending | deferred | blocked | n/a |
|---|---|---|---|---|---|---|---|---|---|
| `routing-io.md` | 1 (phase 1) | 33 | 20 | 9 | 2 | 2 | 0 | 0 | 0 |
| `modulation-matrix.md` | 2 (phase 1) | 38 | 23 | 9 | 4 | 2 | 0 | 0 | 0 |
| `rhythm-engine.md` | 3 (phase 1) | 48 | 25 | 17 | 6 | 0 | 0 | 0 | 0 |
| `live-performance.md` | 4 (phase 1) | 48 | 28 | 14 | 3 | 3 | 0 | 0 | 0 |
| `controllers.md` | 5 (phase 1) | 29 | 15 | 9 | 3 | 2 | 0 | 0 | 0 |
| `practice-tools.md` | 6 (phase 1) | 64 | 17 | 20 | 19 | 7 | 0 | 1 | 0 |
| `tone-match.md` | 7 (phase 1) | 29 | 13 | 10 | 3 | 3 | 0 | 0 | 0 |
| `notation-export.md` | 8 (phase 1) | 39 | 22 | 5 | 8 | 4 | 0 | 0 | 0 |
| `character-wear.md` | 9 (phase 1) | 31 | 19 | 10 | 0 | 2 | 0 | 0 | 0 |
| `accessibility.md` | 10 (phase 1) | 39 | 8 | 6 | 12 | 13 | 0 | 0 | 0 |
| `updates-telemetry.md` | 11 (phase 1) | 33 | 13 | 13 | 0 | 5 | 0 | 2 | 0 |
| `advanced-ranges.md` | 12 (phase 2) | 42 | 28 | 8 | 2 | 4 | 0 | 0 | 0 |
| `volume-knob-interaction.md` | 13 (phase 2) | 32 | 25 | 5 | 0 | 2 | 0 | 0 | 0 |
| `pick-noise.md` | 14 (phase 2) | 32 | 18 | 8 | 4 | 2 | 0 | 0 | 0 |
| `string-squeak.md` | 15 (phase 2) | 37 | 23 | 5 | 7 | 2 | 0 | 0 | 0 |
| `fret-buzz.md` | 16 (phase 2) | 30 | 20 | 5 | 0 | 5 | 0 | 0 | 0 |
| `slide-guitar.md` | 17 (phase 2) | 36 | 23 | 9 | 1 | 3 | 0 | 0 | 0 |
| `guitar-workshop.md` | 18 (phase 2) | 35 | 27 | 2 | 5 | 1 | 0 | 0 | 0 |
| `part-acoustics.md` | 19 (phase 2) | 37 | 24 | 5 | 6 | 1 | 0 | 1 | 0 |
| `workshop-ui.md` | 20 (phase 2) | 34 | 17 | 2 | 12 | 3 | 0 | 0 | 0 |
| `strum-dynamics.md` | 21 (phase 2) | 26 | 0 | 0 | 3 | 23 | 0 | 0 | 0 |
| `bass-techniques.md` | 22 (phase 2) | 28 | 0 | 0 | 0 | 28 | 0 | 0 | 0 |
| `midi-export.md` | 23 (phase 2) | 30 | 17 | 2 | 11 | 0 | 0 | 0 | 0 |
| `9 phase-2b files (missing)` | 23a-23i (phase 2b) | 9 | 0 | 0 | 0 | 0 | 0 | 9 | 0 |
| `tune-builder.md` | 24 (phase 3) | 43 | 15 | 1 | 13 | 14 | 0 | 0 | 0 |
| `ambiguity-resolutions.md` | 25 (phase 4) | 29 | 14 | 6 | 3 | 6 | 0 | 0 | 0 |
| `gui-integration.md` | 26 (phase 4) | 184 | 44 | 51 | 56 | 32 | 0 | 0 | 1 |
| `ui-wiring.md` | 27 (phase 4) | 45 | 5 | 4 | 22 | 14 | 0 | 0 | 0 |
| `onboarding.md` | 28 (phase 4) | 30 | 5 | 3 | 5 | 17 | 0 | 0 | 0 |
| `performance-budget.md` | 29 (phase 4) | 26 | 0 | 1 | 4 | 21 | 0 | 0 | 0 |
| `qa-polish.md` | 30 (phase 4) | 50 | 5 | 1 | 11 | 30 | 0 | 3 | 0 |
| `installer.md` | 31 (phase 4) | 21 | 0 | 0 | 3 | 14 | 1 | 3 | 0 |
| `CLAUDE_CODE_BRIEF.md` | 32 (phase 4) | 16 | 0 | 1 | 4 | 8 | 0 | 1 | 2 |
| `file-formats.md` | 33 (phase 5) | 28 | 4 | 3 | 15 | 5 | 0 | 0 | 1 |
| `factory-content.md` | 34 (phase 5) | 25 | 4 | 2 | 5 | 13 | 1 | 0 | 0 |
| `error-recovery.md` | 35 (phase 5) | 38 | 7 | 6 | 13 | 12 | 0 | 0 | 0 |
| `state-model.md` | 36 (phase 5) | 26 | 2 | 3 | 11 | 10 | 0 | 0 | 0 |
| `gui-engine-dataflow.md` | 37 (phase 5) | 34 | 0 | 15 | 11 | 8 | 0 | 0 | 0 |
| `guitar-illustration.md` | 38 (phase 5) | 54 | 18 | 14 | 12 | 10 | 0 | 0 | 0 |
| `input-routing.md` | 39 (phase 5) | 21 | 1 | 7 | 8 | 5 | 0 | 0 | 0 |
| `host-integration.md` | 40 (phase 5) | 31 | 2 | 8 | 14 | 6 | 1 | 0 | 0 |
| `action-and-undo.md` | 41 (phase 5) | 40 | 1 | 8 | 9 | 22 | 0 | 0 | 0 |
| `string-scraping.md` | 42 (phase 5b) | 13 | 0 | 0 | 0 | 13 | 0 | 0 | 0 |
| `slide-technique-controls.md` | 43 (phase 5b) | 20 | 0 | 0 | 1 | 19 | 0 | 0 | 0 |
| `string-slap-technique.md` | 44 (phase 5b) | 17 | 0 | 0 | 0 | 14 | 0 | 3 | 0 |
| `muting-rhythm.md` | 45 (phase 5b) | 19 | 0 | 0 | 1 | 17 | 0 | 1 | 0 |
| `two-hand-tapping.md` | 46 (phase 5b) | 18 | 0 | 0 | 2 | 15 | 0 | 1 | 0 |
| `microtonal-bends.md` | 47 (phase 5b) | 25 | 0 | 1 | 5 | 19 | 0 | 0 | 0 |
| `technique-cascade.md` | 48 (phase 5b) | 15 | 0 | 0 | 0 | 15 | 0 | 0 | 0 |
| `gui-techniques-updates.md` | 49 (phase 5b) | 19 | 0 | 0 | 0 | 17 | 0 | 0 | 2 |
| `engine-technique-layer.md` | 50 (phase 5b) | 20 | 0 | 0 | 0 | 17 | 0 | 2 | 1 |
| `spec.md` | companion | 127 | 54 | 43 | 19 | 9 | 0 | 0 | 2 |
| `engine.md` | companion | 94 | 40 | 40 | 7 | 5 | 0 | 0 | 2 |
| `theme.md` | companion (overridden by VP 6) | 21 | 0 | 11 | 2 | 0 | 1 | 0 | 7 |
| `include.md` | companion | 23 | 2 | 16 | 4 | 0 | 0 | 0 | 1 |
| `proposals/visual-polish.md` | approved proposal | 34 | 9 | 11 | 5 | 7 | 0 | 0 | 2 |
| `README.md` | project readme | 10 | 0 | 7 | 2 | 0 | 0 | 0 | 1 |
| `JUCE_CLAUDE_GUIDELINES.md` | dev guidelines | 15 | 3 | 8 | 2 | 1 | 0 | 0 | 1 |
| `INDEX.md` | index | 11 | 1 | 3 | 3 | 1 | 0 | 1 | 2 |
| `TODO.md` | process doc, no requirements | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| `DECISIONS.md` | process doc, no requirements | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| `GAPS.md` | process doc, no requirements | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| `PROGRESS.md` | process doc, no requirements | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| `REVIEW.md` | process doc, no requirements | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| **Total** | 63 files on disk + 9 missing | **2081** | **661** | **447** | **383** | **533** | **4** | **28** | **25** |

Every file was read in full. Granularity note: related lines are collapsed
into one row where they would share a location and a status (for example a
spec's full parameter table, or guitar-illustration's per-body dimensions);
nothing was dropped.

# Conflicts

Every place two specs (or a spec and a recorded decision) disagree. The
precedence list is `CLAUDE_CODE_BRIEF.md` "Handling conflicts between docs"
(rank 1 = strongest; rank 22 = later INDEX number wins). "Build" says
whether the code follows the resolution. D-refs point at `spec/DECISIONS.md`
entries by their bold lead-in.

## Decided by the lead (formerly unresolved)

The first audit flagged five conflicts that precedence could not settle.
`spec/DECISIONS.md` "Conflicts from the coverage audit" (2026-09-23,
committed in `a406915`) decides all five. No conflict is currently
unresolved.

| ID | Conflict | Decision (DECISIONS.md) | Build |
|---|---|---|---|
| C-02 | Nine phase-2b specs named by INDEX and the brief are not on disk; five phase-5b specs depend on them. | Stays **blocked**: writing nine physics specs from one-line summaries would invent features. Phase-5b work is built without those modules and the gaps are noted. | Blocked rows stay `blocked` |
| C-09 | ui-wiring 6 wants audio-thread atomic swaps with a 5 ms crossfade; the build parks the audio thread and dropped notes. | Keep the park for whole-guitar loads (a string-count change cannot crossfade in one engine) but **queue** notes that arrive while parked; same-string-count part swaps move to ui-wiring 6's off-thread build and block-boundary swap with the bench. | Queueing done (`a406915`, `WorkshopSwap::aNotePlayedWhileParkedIsKeptNotDropped`); off-thread part swap pending |
| C-16 | Onboarding says 12 tune templates; tune-builder lists 10. | Ship tune-builder's **ten**; onboarding's 12 is a miscount. | Follows: ten templates ship (`d284547`, `TuneBuilder::theTenTemplatesLoadInOrderAndAreValid`) |
| C-19 | Genre kits: `.luthierkit` (factory-content) vs "JSON under Resources/Genres" (rhythm-engine) vs no kit type in file-formats. | `.luthierkit` files, magic `luthier.kit`, factory-content 6's fields; added to file-formats when kits are next touched. | Kits still compiled in |
| C-32 | TECHNIQUES tab makes column 4 a 14-tab strip vs the brief's fixed 13. | **14 tabs**, TECHNIQUES before HELP. | TECHNIQUES tab not built |

## Resolved by precedence or recorded decision

| ID | Conflict | Rule applied | Resolution | Build |
|---|---|---|---|---|
| C-01 | `CLAUDE.md` says specs live in `specs/`; they are in `spec/`. | Lead instruction (this audit) | Treat `spec/` as the spec set. Consider fixing CLAUDE.md. | n/a |
| C-03 | engine.md 5.2 / 20.11 recommend allpass fractional delay; `spec/README.md` deviations make 5th-order Lagrange the default (allpass and linear selectable). | Rank 10 engine.md (DSP rules); README is unranked | engine.md wins as written. The deviation is documented and not the forbidden linear case; needs a proposal amending engine.md or a default switch. | Deviates |
| C-04 | engine.md 5.3 (higher damping coefficient = brighter/longer) vs engine.md 4 (palm mute lowers cutoff 5 kHz -> 800 Hz). | Internal to one spec | README deviation: model damping as a cutoff (the physical reading); both statements then agree in effect. | Follows the chosen reading |
| C-05 | engine.md 20.5 says update coupling per block; README deviations update per sample (per-block steps tick). | Rank 10 engine.md | engine.md wins as written; the deviation is documented with a CPU argument. Needs a proposal or a change. | Deviates |
| C-06 | engine.md 1 puts the pickup after the string sum; README applies the comb per string. | engine.md 7.1 itself uses each string's delay | Per-string comb is what 7.1's formula requires; electrical stage on the sum. | Follows |
| C-07 | routing-io 2 has 7 aux buses; pick-noise 1.3, host-integration 2 and gui-integration 4.4/19 have Aux 8 noise. | Rank 2 (GI) and rank 22 (later specs) | Aux 8 exists, appended after the 12 per-string buses so indices do not shift (TODO 2f). | Follows (`a901c72`) |
| C-08 | volume-knob 1.1 asks for H(s) as a biquad pair recomputed on the message thread; ui-wiring 15 asks for a coefficient FIFO. Build: nodal (MNA) solver with trapezoidal companions recomputed on the audio thread between blocks. | Rank 9 volume-knob; D: "GuitarCircuit is a nodal (MNA) solver", "Recomputed on the audio thread" | Accepted by D: trapezoidal companions are the bilinear transform of the whole network; allocation-free; ground rule 0.5's intent (no audio-thread allocation) holds. | Deviates as recorded |
| C-10 | spec.md header 48 px with guitar/tuning selectors; theme.md 32 px; gui-integration 1-2 32 px with a fixed region list; footer 16 px. | Rank 2 GI | 32 px header per GI 2; guitar/tuning selectors allowed only as mirrors (GI 0.1). Footer 16 px. | Deviates (48 px header, 18 px footer) |
| C-11 | gui-integration 4.4 puts expression calibration on the LIVE tab; GI 5 and GI 19 put it in Options EXPRESSION (GAPS A2 left this open). | GI 19 is the canonical map; GI 0.1 allows mirrors | Primary: Options EXPRESSION. LIVE may mirror it, sharing the single `ExpressionCalibrationSet` so there is one writer. | Options only (mirror missing) |
| C-12 | theme.md "all value changes animate over 80 ms" vs visual-polish 0.4 "No motion. Animated transitions were considered and declined" (user-approved). | User decision 2026-09-23 | No new value-change animation; reduced-motion rules still apply to live elements. Confirm with the user that 0.4 covers value easing. | Follows (none built) |
| C-13 | gui-integration 11.2 / ui-wiring 12 give drag-to-modulate a 25% default depth; the build's right-click route uses 0.33. | Not a conflict | 25% applies to drag only; right-click depth is unspecified. | n/a |
| C-14 | tune-builder 2 binds `Ctrl+S` to save the tune; gui-integration 17 binds `Ctrl+S` to save preset and makes `Ctrl+E` context-aware. Space: GI 17 tune transport vs build audition (GAPS A5). | Rank 2 GI | `Ctrl+S` saves the preset; `Ctrl+E` is context-aware (tune when TUNE is active). Space moves to the tune transport when the Tune Builder lands. | Not built |
| C-15 | Factory guitar count: onboarding 6 says 12; factory-content 2 says 15 (claims the correction was propagated, it was not); guitar-workshop 0.6 ships every enum entry as a file. | Rank 8 guitar-workshop | Ship every enum entry plus factory-content's named 15 (D: "Factory guitars: ... (27 files)"). Onboarding 6 should read 27. | Follows |
| C-17 | spec.md names instruments, amps, speakers and mics by brand (Stratocaster, Les Paul, Fender, Marshall, Celestion, SM57 ...); factory-content 0.1 and qa-polish 11 forbid trademarks. | Rank 1 qa-polish | Reference-style names only; legal review. | Follows (`25e6139`): reference-style display names, old names load through `PartLibrary::renamedFactoryPart` / `renamedFactoryGuitar` and `Resources/Guitars/migration.json` (`5ccf238`); the source scan runs in the suite (`Trademarks::sourceTreeHasNoUnmarkedBrandNames`); final legal review still open |
| C-18 | include.md defers Linux and CLAP to "future versions"; qa-polish 1 tests Ubuntu and installer 3 packages Linux; host-integration 1 schedules CLAP for v1.1. | Rank 1 qa-polish for ship gates | Linux is a ship gate for v1.0; CLAP is not (host-integration). | Linux blocked on environment |
| C-20 | error-recovery 1 refuses a newer-schema file with a banner; the build loads it and logs `NEWER_SCHEMA` (PROGRESS judgement citing 0.4 partial success). | Specific row beats the general rule 0.4 | Refuse with the documented banner. | Deviates |
| C-21 | guitar-workshop 0.4/5 (and GAPS B0 decision 2): incompatible parts fit with a warning; error-recovery 5: not offered for the slot, refused via API with a banner. | Rank 8 guitar-workshop over rank 12 | Advisory: warn and fit. error-recovery 5's refusal row does not apply. | Follows |
| C-22 | error-recovery 14: up to 3 banners visible, a 4th replaces the oldest, priority errors > warnings > info. Build: one banner visible, rest queued with "+N" (PROGRESS A6). GI 15 is silent on count. | Rank 12 error-recovery (failure responses) | Show up to three with priority ordering. | Deviates |
| C-23 | Noise-event strip: string-squeak 9.1 (last 8 s, 30 Hz, grey after 2 s) vs gui-engine-dataflow 10 (15 Hz, events fall off after 1 s, reduced-motion static count at 5 Hz) and ui-wiring 4.2 (15 Hz). | Rank 14 gui-engine-dataflow (drain rates, staleness) | 15 Hz drain, 1 s per-event staleness; reduced-motion per-class count. | Deviates (follows string-squeak) |
| C-24 | guitar-illustration 3 adds an `extended` family; guitar-workshop 5 lists electric, acoustic, bass, classical, resonator, any. | Rank 8 guitar-workshop (data model); D: "Family templates are the factory guitars 12.2 names" | No `extended` family value; the 7-String Modern is an electric, not a family template. | Follows (`e60d708`) |
| C-25 | guitar-illustration 0.2 (flat language, no faux metal beyond one highlight) vs visual-polish 1 (baked lighting, lacquer sheen, metal reflections, drop shadows). | User approval; D: "Guitar illustration materials" | visual-polish 1 wins where they differ; High contrast renders flat. | Follows (`a406915`) |
| C-26 | host-integration 2 rejects a mono main output; `JUCE_CLAUDE_GUIDELINES.md` 6 says support mono-in/mono-out. | host-integration rank 17; guidelines unranked | Reject mono main. | Deviates (accepts mono) |
| C-27 | action-and-undo 0.3/4 (200 ms same-class same-target grouping, stack 200) vs gui-integration 18 (200 ms, stack 64). Build: one entry per host gesture, stack 200. | Rank 18 action-and-undo on undo grouping | 200 ms grouping, stack of 200. | Stack follows; grouping deviates |
| C-28 | Undo across a boundary: GI 18 "holding Shift"; action-and-undo 0.2/5 "Shift-Ctrl-Z"; action-and-undo 9 "Ctrl-Shift-Z = redo, Ctrl-Alt-Z = undo across boundary"; GI 17 "Redo = Ctrl+Shift+Z". | Only reading without a key collision | Redo = `Ctrl+Shift+Z` (and `Ctrl+Y` on Windows); cross-boundary undo = `Ctrl+Alt+Z` with a confirmation banner. | Not built |
| C-29 | pick-noise 5 scrape (noise generators, 8-pool, `pick_scrape_amount`) vs string-scraping 1-3 (`ScrapeEngine`, per-winding impulses into the string). | Rank 19 engine-technique-layer (technique modules) | `ScrapeEngine` is the scrape model. Proposal: keep `pick_scrape_amount` as its level trim so the parameter count does not change. | Not built |
| C-30 | muting-rhythm 4 "No new engine module" vs engine-technique-layer 1 adds `MuteEngine`. | Rank 19 engine-technique-layer | `MuteEngine` exists and feeds per-beat damping. | Not built |
| C-31 | two-hand-tapping 6, microtonal-bends 5, slide-technique-controls 4 and gui-techniques-updates 5 place controls in "Advanced Mode Col 3 CHARACTER" / "Col 3 SLIDE group"; gui-integration 4.4 makes CHARACTER (with its SLIDE group) a column-4 tab. | Rank 2 GI | Put those mirrors in the column-4 CHARACTER tab. "Right Hand group" is a missing `fingerstyle-attack.md` item (C-02). | Not built |
| C-33 | gui-integration 16 offers "Restrict to stock range" when "unlocked at preset level"; advanced-ranges 4 offers it only for controls in `per_control_unlocks`. | Rank 7 advanced-ranges on range semantics; D: "Right-click 'Restrict to stock range' follows advanced-ranges.md 4" | advanced-ranges 4 wins. | Follows |
| C-34 | gui-integration 15 "advanced-range clamped on preset save" banner and advanced-ranges 1.3 banner on narrowing vs D: "No separate 'advanced-range clamped' banner" (a save cannot clamp; narrowing reports in place). | D records the call; rank 2 GI would otherwise require a banner | Accepted per D: the save trigger cannot occur; narrowing reports by confirmation count or bubble. Confirm with the user. | Follows D |
| C-35 | engine.md 9 / 21 step 10 `CableSim` and engine.md hard-coded guitars. | Ranks 9 and 8 | CableSim removed; guitars become part files; the enum survives as a browser shortcut. | Follows |
| C-36 | spec.md Easy three bands and Advanced column contents vs gui-integration 3-4. | Rank 2 GI | GI layout. | Advanced follows; Easy deviates (TODO 2e) |
| C-37 | engine.md 22 / 19 CPU targets (< 8% at 96 kHz / 128; < 15% integration) vs performance-budget 1 (units at 48 kHz / 128, totals by scenario). | Rank 5 performance-budget | performance-budget numbers govern. | Not measured |
| C-38 | engine.md 18 "typical 320 samples" latency vs performance-budget 4 (main <= 128 samples excluding oversampling). | Rank 5 performance-budget | <= 128 samples plus reported oversampling delay. | Not measured |
| C-39 | routing-io 2 Aux 1 "post-cable-sim" vs gui-integration 19 / performance-budget 4 Aux 1 pre/post-circuit toggle. | Rank 2 GI | Aux 1 with a pre/post-circuit toggle. | Toggle not built |
| C-40 | onboarding 1 default "Factory / Rock / Modern Overdrive" on "Les Paul Standard" vs factory-content 1 ("Modern Overdrive", Electric / Overdrive, Vintage Single-Cut) and the trademark rule. | Rank 1 qa-polish (trademarks); factory-content later | Default preset Modern Overdrive on Vintage Single-Cut (D maps "Les Paul" to Vintage Single-Cut). | Deviates (no such preset yet) |
| C-41 | ambiguity-resolutions 3 makes the doubler a post-amp pre-cab rack pedal with eight parameters; spec.md / build have an engine-level doubler with two. | Rank 4 ambiguity-resolutions | Doubler pedal per 3. | Follows (`f904956`, `PedalType::Doubler`; old params kept inert for automation) |
| C-42 | error-recovery 3 "if all five stages engage" vs performance-budget 8's seven relief stages. | Rank 5 performance-budget | Seven stages; the "CPU limit" banner accompanies stage 7. | Not built |
| C-43 | engine.md 4 legato inference (slide if < 40 ms; hammer/pull if velocity < 80) vs two-hand-tapping 5 (hammer-on within 150 ms, velocity < 40). | Rank 19 engine-technique-layer defers to the technique specs; rank 22 later spec | Proposal: < 40 ms stays a slide (engine.md); 40-150 ms below the tapping threshold is a hammer-on. | Follows engine.md only |
| C-44 | gui-integration 0.8 minimum 1280x800 vs GI 4.5 / 13 reflow below 1280 and Advanced unavailable below 1000. | Internal to GI | 1280x800 is the design size; smaller windows reflow per 13 (window minimum 940x560 is compatible). | Follows |
| C-45 | modulation-matrix 1.7 and GI 19 "Macros 1..8" host names vs the build's six named macros plus two assign slots (`Parameters.h:33-41`). | Rank 2 GI (UI) / modulation-matrix | Eight macros, user-nameable. The build's reuse of six named macros is a deviation without a DECISIONS entry. | Deviates |
| C-46 | workshop-ui 4 pickup height range 0.5-6.0 mm vs guitar-illustration 19 "not below 0.8 mm without advanced ranges". | Rank 15 guitar-illustration (visible guitar); D: "Pickup heights clamp at 0.8 - 6 mm" | 0.8-6 mm until pickup height joins an advanced-range family. | Follows (`0d225f0`) |
| C-47 | guitar-illustration 5 layer order puts strings (14) under neck, fretboard and frets (15-17), which would hide them along the neck. | Internal to one spec; D: "Strings draw over the neck" | Neck, fretboard, frets, nut and headstock first, then strings, then tuner posts. | Follows |
| C-48 | guitar-illustration 6 draws four bolt dots on a bolt-on neck; they are on the back of a real guitar. | D: "No neck-plate bolts on the top view" | Top view shows the pocket seam; set neck a rounded heel; through-neck laminate lines. | Follows |
| C-49 | guitar-illustration 11.2 draws bursts as one radial gradient, which cannot darken along a single-cut's edge. | D: "A burst follows the outline" | Radial centre plus stacked edge strokes clipped to the body. | Follows |
| C-51 | gui-integration 3.4: wet/dry mixes "post-master". | D: "Wet/dry mixes the DI against the rig before the master limiter" | Before the limiter, so the dry signal cannot push the output over the ceiling; width is mid/side at the same point. | Follows (`0052205`) |
| C-52 | gui-integration 3.3 places Humanize and Character in the Playing strip and does not mention spec.md's five Easy macros; 3.1 shows notes on the illustration's fretboard. | D: "Easy mode keeps its five older macro knobs", "Easy mode's separate fretboard is gone" | The five macros stay beside Humanize and Character; Easy loses its separate fretboard, Advanced keeps it. | Follows (`0052205`) |
| C-53 | tune-builder 9.2 exports a tune's MIDI "via midi-export profiles"; the Tune model writes its own `.mid` (`buildTuneMidiFile`) with no Luthier profile. | Rank 8 midi-export owns the MIDI format | A tune's MIDI should go through `MidiProfiles` (e.g. `buildTuneScore` -> `MidiPerformance::fromScore`, or a Tune -> `MidiPerformance` adapter). | **Deviates** - integration gap |
| C-50 | string-squeak 11: Generic drops squeak events. midi-export 3, 4.1 and 8: Generic writes realism as `LUTHIER:` text metas when "include realism" is on. | Judgement (coverage assistant), for the lead to confirm | Both hold: realism is off by default, so squeaks are dropped; turned on, they are text for a person to read, which no DAW plays and which leaves the file readable elsewhere. | Follows (model) |
| C-54 | ambiguity-resolutions 2.2: the E-Bow uses "the feedback path" and its intensity "maps to feedback_amount"; 2.4 wants it steady within 500 ms at 50% whatever the rig. | D: "The E-Bow drives each string from itself, through section 1's per-string narrowband injection" | The E-Bow reuses 1's per-string narrowband injection but is driven by the string itself, not the amp output, and has its own `ebow_intensity`, so it holds through any amp and does not switch on amp feedback. | Deviates as recorded (`9c672aa`) |

Editorial slips noted while reading (no behaviour decision needed):
gui-integration 3.2 and 4.2 cite "volume-knob-interaction.md 10" (the UI is
section 5); workshop-ui 3.3 and 4 cite "guitar-illustration.md 608 / 792"
(line numbers, not sections); ui-wiring 15 cites "volume-knob-interaction.md
13" (tests are section 6); tune-builder 0.7 says five melody methods while
section 2 lists four (Sing is section 13) and its step list skips 4;
factory-content 2's header says 12 guitars and its body says 15.


# Coverage by spec file

Sections follow `spec/INDEX.md` build order (1-50, including the missing
phase 2b files), then the companion and remaining files.

## 1. routing-io.md (phase 1)

Built as milestone M31. Aux 8 (noise bus) is added by `pick-noise.md` 1.3 and
tracked there (Conflict C-07).

| ID | Requirement (short) | Source (file §section) | Implementation location | Verification | Status |
|---|---|---|---|---|---|
| RIO-0-01 | Layouts negotiated via BusesProperties; host picks | routing-io §0.1 | `PluginProcessor::buildBusesProperties` `Source/PluginProcessor.cpp:32` | `RoutingTests.cpp` `Routing::everyLayoutRendersCleanly` | `verified` |
| RIO-0-02 | Runs correctly at every advertised layout incl. minimal stereo | routing-io §0.2 | `isBusesLayoutSupported` `PluginProcessor.cpp:212` | `RoutingTests.cpp` `Routing::everyLayoutRendersCleanly` | `verified` |
| RIO-0-03 | Extra outputs post-limiter unless labelled tap points | routing-io §0.3 | `Source/Routing/TapBuffers.h` | none | `implemented` |
| RIO-0-04 | Sidechain optional, gain-neutral; makes sidechain follower live | routing-io §0.4 | sidechain input bus (optional); `ModMatrix.cpp:710` | none | `implemented` |
| RIO-0-05 | MIDI out sample-accurate to input / internal stream | routing-io §0.5 | `Source/Routing/MidiOutRouter.cpp` | `RoutingTests.cpp` `Routing::midiOutPassThroughIsSampleExact`, `Routing::stringActivityIsSampleAccurate` | `verified` |
| RIO-1-01 | Layouts A (stereo), B (+7 aux stereo), C (+12 mono), D (+7 aux + 12 mono) | routing-io §1 | `enum class BusLayout` `Source/Routing/RoutingMatrix.h:31` | `RoutingTests.cpp` `Routing::everyLayoutRendersCleanly` | `verified` |
| RIO-2-01 | Aux 1 DI, 2 amp pre-cab, 3 mic 1, 4 mic 2, 5 room, 6 wet FX, 7 monitor | routing-io §2 | `TapBuffers.h`, `getAuxBusName` | `RoutingTests.cpp` `Routing::diTapNullsAgainstReappliedAmp` (Aux 1) | `implemented` - only Aux 1 content asserted |
| RIO-2-02 | Per-aux gain trim; muted aux skips its render | routing-io §2 | `RoutingMatrix.cpp:66,97-152` | `RoutingTests.cpp` `Routing::muteAndSoloResolveTogether` | `partial` - render skip on mute not asserted |
| RIO-3-01 | 12 mono per-string buses post-body pre-pickup; unused strings silent | routing-io §3 | `PluginProcessor.cpp:42` | `RoutingTests.cpp` `Routing::perStringOutputsSumToPreBody`; `PluginBusTests.cpp` `PluginBuses::perStringLayoutPutsEachStringOnItsOwnBus` (real processor, layout C) | `verified` |
| RIO-3-02 | Per-string bus latency = engine latency | routing-io §3 | latency reporting | `RoutingTests.cpp` `Routing::perOutputLatencyIsConsistent` | `verified` |
| RIO-4-01 | Sidechain as SidechainEnvFollower mod source | routing-io §4 | `ModSources.h:350`, `ModMatrix.cpp:224` | none | `implemented` |
| RIO-4-02 | Sidechain as ducking source for a sidechain compressor pedal | routing-io §4 | no sidechain compressor pedal (rg `sidechain` in `Source/DSP/Effects`) | none | `pending` |
| RIO-4-03 | Sidechain as monitor input for backing-track summing | routing-io §4 | not found in `Source/Practice` | none | `pending` |
| RIO-4-04 | Sidechain never sums into output unless a module mixes it | routing-io §4 | - | `RoutingTests.cpp` `Routing::sidechainToAmpReplacesTheInstrument` | `implemented` |
| RIO-4-05 | Sidechain meter in routing panel | routing-io §4 | `RoutingPanel.h:118` | none | `implemented` |
| RIO-5-01 | External re-amp: render Aux 1 DI | routing-io §5 | Aux 1 tap | `RoutingTests.cpp` `Routing::diTapNullsAgainstReappliedAmp` | `verified` |
| RIO-5-02 | Internal re-amp: Sidechain-to-amp toggle replaces string engine at amp input; off by default | routing-io §5 | `RoutingPanel.h:117` | `RoutingTests.cpp` `Routing::sidechainToAmpReplacesTheInstrument` | `verified` |
| RIO-6-01 | MIDI out: pass-through with same timestamps | routing-io §6 | `MidiOutRouter` | `RoutingTests.cpp` `Routing::midiOutPassThroughIsSampleExact` | `verified` |
| RIO-6-02 | MIDI out: rhythm engine output stream | routing-io §6 | `MidiOutConfig::rhythmEngine` `RoutingMatrix.h:49` | none | `implemented` |
| RIO-6-03 | MIDI out: per-string activity note on/off | routing-io §6 | `MidiOutConfig::stringActivity` | `RoutingTests.cpp` `Routing::stringActivityIsSampleAccurate` | `verified` |
| RIO-6-04 | MIDI out: CC broadcast of macros with user CC numbers | routing-io §6 | `MidiOutConfig::macroCc` | none | `implemented` |
| RIO-6-05 | Uses host MidiBuffer; no-ops silently where unsupported | routing-io §6 | `producesMidi() = true` `PluginProcessor.h:59`, but `CMakeLists.txt:29` `NEEDS_MIDI_OUTPUT FALSE` | none | `partial` - VST3 wrapper derives the MIDI-out bus from `NEEDS_MIDI_OUTPUT`; check a host actually receives it |
| RIO-6-06 | Disabled MIDI out emits nothing | routing-io §6 | `MidiOutConfig::enabled` | `RoutingTests.cpp` `Routing::midiOutDisabledEmitsNothing` | `verified` |
| RIO-7-01 | Per-output latency: main, Aux 1, Aux 2, per-string; single value = main | routing-io §7 | `PluginProcessor.cpp:759` | `RoutingTests.cpp` `Routing::perOutputLatencyIsConsistent` | `verified` |
| RIO-8-01 | ROUTING tab in Advanced column 4 | routing-io §8 | `Source/UI/RoutingPanel.cpp`; `AdvancedPanel::buildWorkspace` | `EditorTests.cpp` `Editor::everyWorkspaceTabSelectsAndPaints` | `verified` |
| RIO-8-02 | Panel: layout readout, 7 aux strips (mute, solo, gain, meter), per-string strip for C/D, sidechain meter + toggle, MIDI out enable/sources/CC table, latency readout | routing-io §8 | `RoutingPanel.h:29-128` | `EditorTests.cpp` `Editor::everyWorkspaceTabSelectsAndPaints` (paint only) | `implemented` |
| RIO-9-01 | Layout not in preset; aux mute/solo/gain, MIDI out assignments, sidechain-to-amp per preset | routing-io §9 | `RoutingMatrix` toVar/fromVar | `RoutingTests.cpp` `Routing::stateRoundTrips`, `Routing::loadingClearsPreviousState` | `verified` |
| RIO-10-01 | Test: each layout renders expected samples | routing-io §10 | - | `RoutingTests.cpp` `Routing::everyLayoutRendersCleanly` | `verified` |
| RIO-10-02 | Test: DI vs main null after reapplied amp within -60 dBFS | routing-io §10 | - | `RoutingTests.cpp` `Routing::diTapNullsAgainstReappliedAmp` | `verified` |
| RIO-10-03 | Test: per-string sum equals pre-body within -80 dBFS | routing-io §10 | - | `RoutingTests.cpp` `Routing::perStringOutputsSumToPreBody` | `verified` |
| RIO-10-04 | Test: 10 000-event MIDI fuzz pass-through within 0 samples | routing-io §10 | - | `RoutingTests.cpp` `Routing::midiOutPassThroughIsSampleExact` | `verified` |
| RIO-10-05 | Test: sidechain-to-amp routes; string-activity aux silent | routing-io §10 | - | `RoutingTests.cpp` `Routing::sidechainToAmpReplacesTheInstrument` | `verified` |
| RIO-10-06 | Test: reported main latency matches measured within 1 sample | routing-io §10 | - | `RoutingTests.cpp` `Routing::perOutputLatencyIsConsistent`; `IntegrationTests.cpp` `Engine::latencyIsReportedAndPlausible` | `verified` |

## 2. modulation-matrix.md (phase 1)

Built as milestone M32 (`Source/Modulation/ModMatrix.*`, `ModSources.*`,
`Source/UI/ModMatrixPanel.*`).

| ID | Requirement (short) | Source (file §section) | Implementation location | Verification | Status |
|---|---|---|---|---|---|
| MOD-0-01 | Control rate = block/32, min 128 samples, set in prepare | modulation-matrix §0.1 | `ModMatrix.cpp:198-200` | none | `implemented` |
| MOD-0-02 | Linear interpolation between control ticks | modulation-matrix §0.2 | `ParameterBridge::value` + smoothing | none | `implemented` |
| MOD-0-03 | Additive over base value, clamped to range | modulation-matrix §0.3 | `ParameterBridge` (`Parameters.h:362-367`) | `ModulationTests.cpp` `Modulation::routeModulatesItsDestination` | `verified` |
| MOD-0-04 | Up to 8 sources per destination; depth and offset -100..+100% | modulation-matrix §0.4 | `kMaxRoutesPerDestination = 8` `ModMatrix.h:126` | `ModulationTests.cpp` `Modulation::destinationAcceptsEightSourcesAndNoMore` | `verified` |
| MOD-0-05 | Sources reset on preset load, snapshot recall, transport start; free-run option | modulation-matrix §0.5 | `ModMatrix::reset`; LFO `Retrigger::freeRun` | none | `implemented` |
| MOD-0-06 | Serialized as compact route array | modulation-matrix §0.6 | `ModMatrix::toVar` | `ModulationTests.cpp` `Modulation::presetRoundTripIsExact` | `verified` |
| MOD-1.1-01 | 8 LFOs: 8 shapes incl. S+H, random smooth, custom 8-point | modulation-matrix §1.1 | `ModSources.h:86-97` | `ModulationTests.cpp` `Modulation::sampleAndHoldHoldsForAWholeCycle`, `Modulation::curvesPreserveSignAndFixedPoints` | `verified` |
| MOD-1.1-02 | LFO rate 0.01-40 Hz or tempo-synced 1/32T..8 bars, dotted/triplet | modulation-matrix §1.1 | `ModSyncDivision` `ModSources.h:67` | `ModulationTests.cpp` `Modulation::lfoFrequencyIsAccurate`, `Modulation::syncedLfoFollowsTheHost` | `verified` |
| MOD-1.1-03 | Phase, depth, symmetry, retrigger modes (4), smoothing 0-500 ms, uni/bipolar | modulation-matrix §1.1 | `ModSources.h:95` and LFO members | none | `implemented` |
| MOD-1.2-01 | 4 DAHDSR EGs, 0-30 s log stages, sustain, per-stage curve, retrigger (legato/always/one-shot), loop modes | modulation-matrix §1.2 | `ModSources.h:202-204` | `ModulationTests.cpp` `Modulation::envelopeStageTimesAreAccurate` | `verified` |
| MOD-1.3-01 | 2 step sequencers: 4-64 steps, grids incl. dotted/triplet, value/gate/slide/probability, sync host/tap/internal, 5 directions, swing 0-75% | modulation-matrix §1.3 | `ModSources.h:277-301` | `ModulationTests.cpp` `Modulation::stepSequencerWalksItsSteps` | `verified` |
| MOD-1.4-01 | 2 envelope followers: sources main/sidechain/per-string/pickup; attack/release ranges; peak/RMS/true-peak; threshold/gate; log/lin | modulation-matrix §1.4 | `ModSources.h:349-350` | `ModulationTests.cpp` `Modulation::envelopeFollowerTracksLevel` | `verified` |
| MOD-1.5-01 | Note sources: pitch, velocity, note-on trigger, notes-held, aftertouch, poly AT | modulation-matrix §1.5 | `ModMatrix` performance sources | `ModulationTests.cpp` `Modulation::sourceIdsResolveBothWays` | `implemented` - outputs not individually asserted |
| MOD-1.6-01 | MIDI CC sources incl. 14-bit pairs; pitch bend, mod wheel, channel pressure; program change not a source | modulation-matrix §1.6 | `ModMatrix` `ccN` ids | `ModulationTests.cpp` `Modulation::sourceIdsResolveBothWays` | `implemented` - 14-bit pairing not asserted |
| MOD-1.7-01 | 8 macros, named 32-char slots, modulatable, host-automatable Macro 1..8 | modulation-matrix §1.7 | six named macros + `macro_assign_a/b` `Parameters.h:26-41` | none | `partial` - host names are the six fixed macros + two assign slots, not "Macro 1..8"; user naming not found |
| MOD-1.8-01 | Random: per note, per bar, smoothed continuous; seeded | modulation-matrix §1.8 | `randomSource` `ModMatrix.cpp:217` | `ModulationTests.cpp` `Modulation::randomSourcesAreDeterministic` | `verified` |
| MOD-2-01 | Every automatable parameter is a destination | modulation-matrix §2 | `ParameterBridge::value` | `ModulationTests.cpp` `Modulation::routeModulatesItsDestination` | `implemented` |
| MOD-2-02 | Per-string destinations (tuning, damping, sustain, etc.), rhythm-engine params, output pan, snapshot morph position | modulation-matrix §2 | per-string and morph values are not parameters | none | `partial` - destinations limited to APVTS params |
| MOD-2-03 | Discrete destinations change only on integer boundary with module crossfade | modulation-matrix §2, §4 | `ModMatrix` discrete handling | `ModulationTests.cpp` `Modulation::discreteDestinationsStepAtBoundaries` | `verified` |
| MOD-3-01 | Route record `{source, channel, destination, depth, offset, curve, enabled}`; interned ids like `lfo1`, `cc74` | modulation-matrix §3 | `ModMatrix.h` Route | `ModulationTests.cpp` `Modulation::sourceIdsResolveBothWays` | `verified` |
| MOD-3-02 | Curves Linear, Exp, Log, S, Custom(name) | modulation-matrix §3 | `enum class ModCurve` `ModSources.h:33` | `ModulationTests.cpp` `Modulation::curvesPreserveSignAndFixedPoints` | `verified` |
| MOD-3-03 | Sum-and-clamp; host automation applies to base | modulation-matrix §3, §7 | `ParameterBridge` | `ModulationTests.cpp` `Modulation::routeModulatesItsDestination` | `verified` |
| MOD-4-01 | Selector index = floor(pos*count+0.5); bypass flips at 0.5 | modulation-matrix §4 | `ModMatrix` | `ModulationTests.cpp` `Modulation::discreteDestinationsStepAtBoundaries` | `verified` |
| MOD-5-01 | MOD tab in Advanced column 4 | modulation-matrix §5 | `Source/UI/ModMatrixPanel.cpp` | `EditorTests.cpp` `Editor::everyWorkspaceTabSelectsAndPaints` | `verified` |
| MOD-5-02 | Source pool cards with compact editors; routing table with source/dest/depth/offset/curve/enabled/delete; add-route | modulation-matrix §5 | `ModMatrixPanel.cpp` | `EditorTests.cpp` `Editor::everyWorkspaceTabSelectsAndPaints` (paint only) | `implemented` |
| MOD-5-03 | Drag a source card onto a control to create a route | modulation-matrix §5 | absent (GAPS A4: no DragAndDropContainer) | none | `pending` |
| MOD-5-04 | Right-click any control: Modulate submenu creates a route | modulation-matrix §5 | `buildParameterContextMenu` / `applyParameterMenuResult` | `EditorTests.cpp` `Editor::rightClickOffersModulationAndBuildsTheRoute` | `verified` |
| MOD-5-05 | Modulation depth arc on modulated controls in the source's colour | modulation-matrix §5, §7 | `Widgets.cpp:562-600` (single secondary colour) | none | `partial` - arc exists; per-source colour not |
| MOD-5-06 | User-assignable per-source colour from theme palette; routes inherit | modulation-matrix §5 | not found (`ModMatrixPanel.cpp` uses `Palette::secondary`) | none | `pending` |
| MOD-6-01 | Modulation stored in every preset under `modulation` | modulation-matrix §6 | `PresetManager` | `ModulationTests.cpp` `Modulation::presetRoundTripIsExact` | `verified` |
| MOD-6-02 | Snapshot flag: includes modulation vs preset-level only | modulation-matrix §6 | snapshots always carry matrix blob (`Snapshots.h:166-190`); no flag found | none | `partial` |
| MOD-6-03 | Import validates destination ids, warns on unknown | modulation-matrix §6 | `ModMatrix.cpp:430-551` `unknownDestinations` | `ModulationTests.cpp` `Modulation::unknownDestinationsAreReportedNotFatal` | `verified` |
| MOD-7-01 | Automation moves the control; modulation does not (arc only) | modulation-matrix §7 | `ParameterBridge::baseValue` | none | `implemented` |
| MOD-8-01 | Test: source outputs (LFO freq, EG times, S+H hold) | modulation-matrix §8 | - | `ModulationTests.cpp` `Modulation::lfoFrequencyIsAccurate`, `envelopeStageTimesAreAccurate`, `sampleAndHoldHoldsForAWholeCycle` | `verified` |
| MOD-8-02 | Test: 1000-route stress under 1% CPU at 128-sample blocks | modulation-matrix §8 | - | `ModulationTests.cpp` `Modulation::thousandRouteStressTest` | `verified` |
| MOD-8-03 | Test: preset round trip byte-identical | modulation-matrix §8 | - | `ModulationTests.cpp` `Modulation::presetRoundTripIsExact` | `verified` |
| MOD-8-04 | Test: 5-option selector changes at 1/5..4/5 | modulation-matrix §8 | - | `ModulationTests.cpp` `Modulation::discreteDestinationsStepAtBoundaries` | `verified` |
| MOD-8-05 | Test: seeded random renders byte-identical | modulation-matrix §8 | - | `ModulationTests.cpp` `Modulation::randomSourcesAreDeterministic` | `verified` |

## 3. rhythm-engine.md (phase 1)

Built as milestone M33 (`Source/Rhythm/*`, `Source/UI/RhythmPanel.*`). The
RHYTHM tab's STRUM group belongs to `strum-dynamics.md`; the mute grid to
`muting-rhythm.md`.

| ID | Requirement (short) | Source (file §section) | Implementation location | Verification | Status |
|---|---|---|---|---|---|
| RHY-0-01 | MIDI transformer between interpreter and technique engine; never touches audio | rhythm-engine §0.1, §9 | `Source/Rhythm/RhythmEngine.cpp` | none | `implemented` |
| RHY-0-02 | No allocation in callback; pattern buffers pre-sized | rhythm-engine §0.2 | fixed arrays `Patterns.h` (`kMaxSteps`) | none | `implemented` |
| RHY-0-03 | Sample-accurate to host transport via playhead | rhythm-engine §0.3 | `RhythmEngine` | `RhythmSchedulerTests.cpp` `RhythmPatterns::strumSchedulingIsSampleAccurate` | `verified` |
| RHY-0-04 | Silent when host stopped unless Free-run | rhythm-engine §0.4 | `RhythmEngine.h:89` `setFreeRun` | `RhythmSchedulerTests.cpp` `RhythmPatterns::silentWhenStoppedUnlessFreeRunning` | `verified` |
| RHY-0-05 | Quantized on read, humanized on write from shared humanize settings | rhythm-engine §0.5, §9 | `RhythmEngine` humanize struct `RhythmEngine.h:54-57` | `RhythmSchedulerTests.cpp` `RhythmPatterns::humanisationIsDeterministic` | `partial` - keeps its own humanize block rather than reading `hum_*` (spec: "without owning its own copy") |
| RHY-0-06 | Own bypass; reverts within one block without artefacts | rhythm-engine §0.6 | `RhythmEngine` bypass | `RhythmSchedulerTests.cpp` `RhythmPatterns::bypassIsCleanAndImmediate` | `verified` |
| RHY-1-01 | Sub-modules ChordDetector, ChordVoicer, StrumScheduler, FingerpickScheduler, PatternLibrary, GenreKit | rhythm-engine §1 | `ChordDetector.cpp`, `ChordVoicer.cpp`, `Patterns.cpp`, `GenreKit.cpp` | - | `implemented` |
| RHY-2-01 | 84 templates (maj..6/9 + slash inversions); pitch-class matching | rhythm-engine §2 | `Source/Rhythm/ChordDetector.cpp` | `RhythmTests.cpp` `Rhythm::chordTemplatesAreUnique`, `Rhythm::chordDetectorRoundTripsEveryTemplateInEveryKey` | `verified` |
| RHY-2-02 | Bass = lowest note, slash if not root | rhythm-engine §2 | `ChordDetector` | `RhythmTests.cpp` `Rhythm::slashChordsReportTheirBass`, `Rhythm::chordDetectorHandlesInversions` | `verified` |
| RHY-2-03 | Confidence = matched/held; < 0.6 -> Unknown -> literal mapping | rhythm-engine §2 | `ChordDetector` | `RhythmTests.cpp` `Rhythm::lowConfidenceIsReportedAsUnknown` | `verified` |
| RHY-2-04 | Runs once per NoteOn burst (30 ms window) | rhythm-engine §2 | `ChordDetector` burst window | `RhythmTests.cpp` `Rhythm::burstWindowGroupsASpreadChord` | `verified` |
| RHY-3-01 | Voicer input incl. capo, style, density, hand-position hint; output fret map, mute mask, barre fret | rhythm-engine §3 | `ChordVoicer.h`, `RhythmEngine` | `RhythmSchedulerTests.cpp` `RhythmPatterns::voicerHandlesEveryChordOnEveryGuitar` | `verified` |
| RHY-3-02 | Constraints: fret range; hand span default 5 (user 3-7); root/bass lowest; no >4 st downward skip; barre needs 3 strings | rhythm-engine §3 | `ChordVoicer.h:65-67` (default 4, range 2-7) | `ModelTests.cpp` `ChordVoicer::commonChordsAreVoicedPlayably`, `ChordVoicer::pitchOrderFollowsStringOrder` | `partial` - span default 4 and range 2-7 differ from 5 and 3-7 |
| RHY-3-03 | Score: open bonus 3, hint proximity, barre penalty 2, mute penalty 4, style bias | rhythm-engine §3 | `ChordVoicer.cpp` | `RhythmSchedulerTests.cpp` `RhythmPatterns::voicingStylesProduceDifferentVoicings` | `implemented` - weights not asserted |
| RHY-3-04 | Styles Open, Barre, Triad, Shell, Drop2, Drop3, Power, Rootless, Wide | rhythm-engine §3 | `enum class VoicingStyle` `RhythmEngine.h:61` | `RhythmSchedulerTests.cpp` `RhythmPatterns::voicingStylesProduceDifferentVoicings` | `verified` |
| RHY-3-05 | Density 0-100 caps voiced notes | rhythm-engine §3 | `RhythmEngine` | none | `implemented` |
| RHY-3-06 | Hand-position hint follows previous chord | rhythm-engine §3 | `RhythmEngine` | none | `implemented` |
| RHY-4-01 | Strum spacing `duration/(n-1)`; Down, Up, DownMute, UpMute, Rake, Rasgueado | rhythm-engine §4 | `enum class StrumType` `Patterns.h:22` | `RhythmSchedulerTests.cpp` `RhythmPatterns::strumSchedulingIsSampleAccurate` | `verified` |
| RHY-4-02 | Dynamic fall-off 100% -> 85-95% by `strum_evenness` | rhythm-engine §4 | `RhythmEngine.h:128` | none | `implemented` |
| RHY-4-03 | String mask per strum | rhythm-engine §4 | pattern step mask | `GenreKitTests.cpp` `GenreKits::factoryMasksSelectStringsASixStringHas` | `verified` |
| RHY-4-04 | 16 steps default, 32 max; 16th quantize; triplet/dotted per pattern | rhythm-engine §4 | `enum class Subdivision` `Patterns.h:59` | `RhythmSchedulerTests.cpp` `RhythmPatterns::factoryPatternsAreWellFormed` | `verified` |
| RHY-4-05 | Humanize timing (gaussian), velocity, miss %, ghost % | rhythm-engine §4 | `RhythmEngine.h:54-55` | `RhythmSchedulerTests.cpp` `RhythmPatterns::humanisationIsDeterministic` | `verified` |
| RHY-5-01 | Fingerpick: finger assignment p/i/m/a/e per string; per-finger excitation | rhythm-engine §5 | `enum class Finger` `Patterns.h:52` | none | `partial` - per-finger excitation profiles not found (see SPEC-5-03) |
| RHY-5-02 | Factory fingerpick patterns: Travis, p-i-m-a asc, p-i-m-a-m-i, Boom-chick, Piedmont, Modern folk, 5-3-4-2-3-4 | rhythm-engine §5 | `Patterns.cpp` `makeFingerpick` (11 incl. all 7) | `RhythmSchedulerTests.cpp` `RhythmPatterns::factoryPatternsAreWellFormed` | `verified` |
| RHY-5-03 | Patterns user-editable | rhythm-engine §5 | `RhythmPanel` editors | none | `implemented` |
| RHY-6-01 | `.luthierpattern` JSON schema (name, type, length_steps, subdivision, swing, steps{step,event,dynamic,mask}, tags) | rhythm-engine §6 | `Patterns.cpp:167-290` | `RhythmSchedulerTests.cpp` `RhythmPatterns::patternsRoundTripThroughJson` | `verified` |
| RHY-6-02 | Factory patterns under `Resources/Rhythm/`; user under `~/Documents/Luthier/Rhythm/` | rhythm-engine §6 | factory compiled in `Patterns.cpp`; export via file dialog `RhythmPanel.cpp:906`; no `Resources/Rhythm` | none | `partial` - no on-disk factory folder or user library scan |
| RHY-7-01 | GenreKit bundles style, density, pattern list, humanize defaults, soft rig reference | rhythm-engine §7 | `Source/Rhythm/GenreKit.cpp` | `GenreKitTests.cpp` `GenreKits::applyingAKitInstallsItsPatternAndSettings`, `GenreKits::applyingAKitDoesNotLoadItsRig` | `verified` |
| RHY-7-02 | Ship the 28 named kits | rhythm-engine §7 | `GenreKit.cpp` (all 28 names present) | `GenreKitTests.cpp` `GenreKits::factoryKitsAreWellFormed`, `GenreKits::everyKitSoundsWhenApplied` | `verified` |
| RHY-7-03 | Kits as JSON files under `Resources/Genres/` | rhythm-engine §7 | compiled in; JSON round trip exists | `GenreKitTests.cpp` `GenreKits::kitsRoundTripThroughJson` | `partial` - not shipped as files |
| RHY-8-01 | RHYTHM tab in column 4 | rhythm-engine §8 | `Source/UI/RhythmPanel.cpp` | `EditorTests.cpp` `Editor::everyWorkspaceTabSelectsAndPaints` | `verified` |
| RHY-8-02 | Genre kit dropdown + randomise-within-style dice | rhythm-engine §8 | `RhythmPanel.cpp` | `GenreKitTests.cpp` `GenreKits::randomPatternStaysInsideTheKit` | `verified` |
| RHY-8-03 | Voicing controls: style, density, hand position, capo up/down | rhythm-engine §8 | `RhythmPanel.cpp` (capo delegates to `TuningEngine`) | none | `implemented` |
| RHY-8-04 | Strum grid 16/32 steps; right-click cell for dynamic/mask/delete | rhythm-engine §8 | `RhythmPanel.cpp` | none | `implemented` |
| RHY-8-05 | Fingerpick grid 16 steps x 5 fingers | rhythm-engine §8 | `RhythmPanel.cpp` | none | `implemented` |
| RHY-8-06 | Feel: swing, timing jitter, velocity jitter, miss %, ghost % | rhythm-engine §8 | `RhythmPanel.cpp` | none | `implemented` |
| RHY-8-07 | Pattern browser: list, tag filter, load, save, export | rhythm-engine §8 | `RhythmPanel.cpp:813` export | none | `partial` - tag filter / user library not verified |
| RHY-8-08 | Live indicators: chord symbol, voicing dots, next-strum lamp on the beat | rhythm-engine §8 | `RhythmPanel.cpp` | none | `implemented` |
| RHY-8-09 | Easy Mode strip: kit, feel, on/off; Mono-mode hint | rhythm-engine §8 | `EasyPanel.cpp` rhythm strip | none | `implemented` - gui-integration 3 adds dice + readouts (TODO 2e) |
| RHY-9-01 | Writes only NoteOn/Off and palm-mute CC | rhythm-engine §9 | `RhythmEngine` | none | `implemented` |
| RHY-9-02 | Preset carries rhythm enabled + state blob | rhythm-engine §9 | `PluginProcessor.cpp:1748,1832` (`rhythm` block) | `RhythmSchedulerTests.cpp` `RhythmPatterns::patternsRoundTripThroughJson` | `implemented` |
| RHY-9-03 | MIDI out captures transformed stream | rhythm-engine §9 | `MidiOutConfig::rhythmEngine` | none | `implemented` |
| RHY-10-01 | Test: 84 templates x 12 keys x inversions (1008 cases) | rhythm-engine §10 | - | `RhythmTests.cpp` `Rhythm::chordDetectorRoundTripsEveryTemplateInEveryKey`, `Rhythm::chordDetectorHandlesInversions` | `verified` |
| RHY-10-02 | Test: voicer valid or "unplayable" for 25 guitars x 84 x 12 | rhythm-engine §10 | - | `RhythmSchedulerTests.cpp` `RhythmPatterns::voicerHandlesEveryChordOnEveryGuitar` | `verified` |
| RHY-10-03 | Test: 120 bpm 16th grid within ±1 sample at 48 kHz | rhythm-engine §10 | - | `RhythmSchedulerTests.cpp` `RhythmPatterns::strumSchedulingIsSampleAccurate` | `verified` |
| RHY-10-04 | Test: humanisation deterministic with seed | rhythm-engine §10 | - | `RhythmSchedulerTests.cpp` `RhythmPatterns::humanisationIsDeterministic` | `verified` |
| RHY-10-05 | Test: bypass during playback, no click | rhythm-engine §10 | - | `RhythmSchedulerTests.cpp` `RhythmPatterns::bypassIsCleanAndImmediate` | `verified` |
| RHY-10-06 | Test: free-run advances at tempo when stopped | rhythm-engine §10 | - | `RhythmSchedulerTests.cpp` `RhythmPatterns::silentWhenStoppedUnlessFreeRunning` | `verified` |

## 4. live-performance.md (phase 1)

Built as milestone M34 (`Source/Live/*`, `Source/UI/LiveStrip.*`,
`Source/UI/LivePanel.*`).

| ID | Requirement (short) | Source (file §section) | Implementation location | Verification | Status |
|---|---|---|---|---|---|
| LIVE-0-01 | Every live control reachable without a mouse (key, CC, PC, controller) | live-performance §0.1 | shortcut registry `Accessibility.cpp:502-515`; MIDI Learn | `AccessibilityTests.cpp` `Accessibility::shortcutDefaultsMatchTheCanonicalTable` | `implemented` |
| LIVE-0-02 | Snapshot switching glitch-free; held notes continue | live-performance §0.2 | `Source/Live/Snapshots.cpp` | `LiveTests.cpp` `LiveSnapshots::thousandRecallsNeverJumpAParameter` | `verified` |
| LIVE-0-03 | Tap tempo plugin-global, works without host clock | live-performance §0.3 | `Source/Live/TapTempo.cpp` | `LiveTests.cpp` `LiveTapTempo::detectsTempoWithinHalfABpm` | `verified` |
| LIVE-0-04 | Kill switch acts within one block of press/release | live-performance §0.4 | `KillSwitch` `LiveControls.h:31` | `LiveTests.cpp` `LiveKillSwitch::reachesSilenceAndRecoversInsideFiveMilliseconds` | `verified` |
| LIVE-0-05 | Nothing on the live surface opens a modal dialog | live-performance §0.5 | `LiveStrip.h:12` note | none | `implemented` |
| LIVE-1-01 | Up to 128 snapshots per preset: params, mod routing, bypasses, rhythm state, 32-char label, 16 colour tags | live-performance §1 | `Snapshots.h:59-66`, `Snapshots.cpp:95-118` | `LiveTests.cpp` `LiveSnapshots::captureAndRecallRoundTrip`, `LiveSnapshots::bankRoundTripsThroughJson` | `verified` |
| LIVE-1-02 | Recall: no retrigger; amp crossfade `snapshot_xfade_ms` default 30, 0-500 | live-performance §1 | `SnapshotBank` crossfade (clamp 500 ms) | `LiveTests.cpp` `LiveSnapshots::recallCrossfadesContinuousAndStepsDiscrete` | `verified` |
| LIVE-1-03 | Bypass changes at crossfade midpoint | live-performance §1 | `Snapshots.h:166-175` | `LiveTests.cpp` `LiveSnapshots::recallCrossfadesContinuousAndStepsDiscrete` | `verified` |
| LIVE-1-04 | Delay/reverb tails preserved (double-buffered tail modules) | live-performance §1 | not found (rg `double.buffer` in `Source/Live`) | none | `partial` - tails continue because FX state is not reset; no double buffering |
| LIVE-1-05 | Coupling matrix recomputed on a worker; old held one block if late | live-performance §1 | not found | none | `pending` |
| LIVE-1-06 | Snapshots in preset `snapshots` array; implicit "Default" when none | live-performance §1 | `PresetManager` snapshots block | `LiveTests.cpp` `LiveSnapshots::bankRoundTripsThroughJson` | `implemented` - implicit Default not asserted |
| LIVE-2-01 | Program Change = snapshot index | live-performance §2 | `PluginProcessor.cpp:1252` | `LiveTests.cpp` `LiveSnapshots::programChangeMapsAcrossAllOneTwentyEight` | `verified` |
| LIVE-2-02 | Bank Select (CC 0) selects the preset | live-performance §2 | `LuthierAudioProcessor::handleLiveMidi` `PluginProcessor.cpp:1260` (`pendingPresetSelect`, applied on the editor timer) | none | `implemented` |
| LIVE-2-03 | CC for next / previous / snapshot-by-value | live-performance §2 | MIDI Learn targets | none | `implemented` |
| LIVE-2-04 | Keys: `[` `]` step, digits 1-9, Shift+digit 10-18 | live-performance §2 | `Accessibility.cpp:506-507`; digits positional (GAPS A5) | `EditorTests.cpp` `Editor::theModesThatChangeTheLayoutTakeEffectAndUndoThemselves` (does not cover digits) | `implemented` |
| LIVE-2-05 | Snapshot strip: bank of 8, active lit | live-performance §2, §10 | `Source/UI/LiveStrip.cpp` | none | `implemented` |
| LIVE-3-01 | Morph between two snapshots from pedal CC, LFO, mod wheel, sidechain | live-performance §3 | `SnapshotBank` morph | `LiveTests.cpp` `LiveSnapshots::morphFollowsItsCurve` | `verified` |
| LIVE-3-02 | Discrete params switch at 0.5; per-parameter exclusion takes A | live-performance §3 | `Snapshots.h:160-190` | `LiveTests.cpp` `LiveSnapshots::morphHonoursExclusionsAndDiscreteSwitching` | `verified` |
| LIVE-3-03 | Curves linear, S, exp, custom 4-point Bezier | live-performance §3 | `Snapshots.h:53,221` | `LiveTests.cpp` `LiveSnapshots::morphFollowsItsCurve` | `verified` |
| LIVE-3-04 | Morph control: A/B slots, knob or pedal, enable | live-performance §3, §10 | `LiveStrip.h:140-149` | none | `implemented` |
| LIVE-4-01 | Setlist `.luthierset` in `~/Documents/Luthier/Setlists/` with name, notes, bpm_default, entries {preset_path, snapshot_index, notes} | live-performance §4 | `Source/Live/Setlist.cpp:6,112,174` | `LiveTests.cpp` `LiveSetlist::roundTripsThroughJson` | `verified` |
| LIVE-4-02 | PageUp/PageDown or CCs navigate | live-performance §4 | `Accessibility.cpp:515` | `LiveTests.cpp` `LiveSetlist::walksForwardsAndBackwardsWithoutGrowing` | `verified` |
| LIVE-4-03 | Header triptych prev/current/next | live-performance §4 | `LiveStrip.cpp` | `LiveTests.cpp` `LiveSetlist::reportsPreviousCurrentAndNext` | `verified` |
| LIVE-4-04 | Pre-load next entry for gap-free switch | live-performance §4 | `Setlist.cpp:200` `preloadedIndex` | none | `implemented` |
| LIVE-5-01 | Tap drives rhythm (host stopped), synced delays, synced LFOs | live-performance §5 | `TapTempo` consumers | none | `implemented` |
| LIVE-5-02 | Last 4 taps in 3 s; median; outliers > 30% discarded; 20-300 bpm; snap within 0.4 | live-performance §5 | `TapTempo.cpp:72-114` | `LiveTests.cpp` `LiveTapTempo::oneOutlierDoesNotMoveTheEstimate`, `LiveTapTempo::respectsRangeSnapAndHostPriority` | `verified` |
| LIVE-5-03 | Tap assignable to CC/footswitch/key (`T`); LED blinks on beat | live-performance §5 | `Accessibility.cpp:503` | none | `implemented` - beat blink not verified |
| LIVE-5-04 | Host tempo wins unless "internal tempo" forced in Options | live-performance §5 | `TapTempo.h:65-97` | `LiveTests.cpp` `LiveTapTempo::respectsRangeSnapAndHostPriority` | `verified` |
| LIVE-6-01 | Kill switch: 3 ms fade in/out; DSP keeps running; default key `\`; red pill | live-performance §6 | `LiveControls.h:31-64`; `Accessibility.cpp:504` | `LiveTests.cpp` `LiveKillSwitch::fadesRatherThanJumping`, `LiveKillSwitch::doesNotDisturbTheSignalUnderneath` | `verified` |
| LIVE-7-01 | Monitor mix: main + sidechain; level, pan, 3-band EQ; click bus; second stereo pair (Aux 7) | live-performance §7 | `MonitorMix` `LiveControls.h:77-119` | `LiveTests.cpp` `LiveMonitor::idleWhenNothingToMonitorAndSumsWhenThereIs` | `verified` |
| LIVE-7-02 | Monitor path bypassed, no CPU when idle | live-performance §7 | `LiveControls.h:75-116` | `LiveTests.cpp` `LiveMonitor::idleWhenNothingToMonitorAndSumsWhenThereIs` | `verified` |
| LIVE-8-01 | Options -> Expression -> Calibrate heel/toe wizard | live-performance §8 | Options EXPRESSION page (`OptionsPages.cpp`) | `LiveTests.cpp` `LiveExpression::wizardCapturesHeelAndToe` | `verified` |
| LIVE-8-02 | Min/max map to 0-1; dead zones 3% heel, 5% toe; curves linear/log/exp/S | live-performance §8 | `ExpressionCalibrationSet` | `LiveTests.cpp` `LiveExpression::calibrationMapsRealTravelOntoFullRange`, `LiveExpression::everyCurveIsMonotonicAndPinnedAtBothEnds` | `verified` |
| LIVE-8-03 | Per-CC calibrations, user-global | live-performance §8, §11 | `~/Documents/Luthier/config/expression.json` | `LiveTests.cpp` `LiveExpression::calibrationSetRoundTrips` | `verified` |
| LIVE-9-01 | Panic: notes off, FX tails, amp DC/feedback, coupling; keeps snapshot/preset and params; key `P` | live-performance §9 | `Accessibility.cpp:502`; `LuthierEngine::panic` | `IntegrationTests.cpp` `Engine::panicSilencesEverything` | `partial` - "does not change params/snapshot" not asserted |
| LIVE-10-01 | Live strip: snapshots, setlist triptych, tap pad, morph, kill pill, monitor level | live-performance §10 | `Source/UI/LiveStrip.cpp` | `EditorTests.cpp` `Editor::theModesThatChangeTheLayoutTakeEffectAndUndoThemselves` | `implemented` |
| LIVE-10-02 | Live Mode: 44 px minimum hit targets | live-performance §10 | not found | none | `pending` |
| LIVE-10-03 | Live Mode: suppress non-critical tooltips | live-performance §10 | not found | none | `pending` |
| LIVE-10-04 | Live Mode locks the Advanced toggle | live-performance §10 | `HeaderBar.cpp:229` | none | `implemented` (PROGRESS: fixed for keyboard route) |
| LIVE-11-01 | Live-mode on/off per preset | live-performance §11 | `uiState` travels with preset | `StateModelTests.cpp` `StateModel::loadingAPresetLeavesTheLayersAboveItAlone` | `implemented` |
| LIVE-11-02 | Live-control MIDI Learn per preset, optionally "global" | live-performance §11 | global flag not found (`MidiLearn.h`) | none | `partial` |
| LIVE-12-01 | Test: 1000 random recalls, zero clicks | live-performance §12 | - | `LiveTests.cpp` `LiveSnapshots::thousandRecallsNeverJumpAParameter` | `verified` |
| LIVE-12-02 | Test: PC across 128 values | live-performance §12 | - | `LiveTests.cpp` `LiveSnapshots::programChangeMapsAcrossAllOneTwentyEight` | `verified` |
| LIVE-12-03 | Test: morph curves within 0.001 at 100 points | live-performance §12 | - | `LiveTests.cpp` `LiveSnapshots::morphFollowsItsCurve` | `verified` |
| LIVE-12-04 | Test: 10 tap sequences within 0.5 bpm | live-performance §12 | - | `LiveTests.cpp` `LiveTapTempo::detectsTempoWithinHalfABpm` | `verified` |
| LIVE-12-05 | Test: kill < -80 dBFS within 5 ms, recover within 5 ms, 1000 times | live-performance §12 | - | `LiveTests.cpp` `LiveKillSwitch::reachesSilenceAndRecoversInsideFiveMilliseconds` | `verified` |
| LIVE-12-06 | Test: 50-preset setlist walk, no memory growth | live-performance §12 | - | `LiveTests.cpp` `LiveSetlist::walksForwardsAndBackwardsWithoutGrowing` | `verified` |
| LIVE-LT-01 | LIVE tab in column 4 (setup surface) | gui-integration 4.4 via live-performance | `Source/UI/LivePanel.cpp` | `EditorTests.cpp` `Editor::theLiveTabEditsTheSnapshotBankAndTheSetlist` | `verified` |

## 5. controllers.md (phase 1)

Built as milestone M35 (`Source/Controllers/ControllerProfile.*`;
CONTROLLERS page moved to Advanced column 4, GAPS A2).

| ID | Requirement (short) | Source (file §section) | Implementation location | Verification | Status |
|---|---|---|---|---|---|
| CTL-0-01 | Named profiles loaded from `Resources/Controllers/*.json`; default Generic MIDI | controllers §0.1 | profiles compiled in `ControllerProfile.cpp:286-429`; no `Resources/Controllers` folder | `ControllerTests.cpp` `Controllers::everyShipProfileIsWellFormed` | `partial` - ship profiles are code, not JSON files |
| CTL-0-02 | Profile declares mode, bend range, latency budget, CC map | controllers §0.2 | `ControllerProfile.h` | `ControllerTests.cpp` `Controllers::profilesRoundTripThroughJson` | `verified` |
| CTL-0-03 | Latency compensation on MIDI input path; forward-quantize late MIDI | controllers §0.3 | `MidiInterpreter` | `ControllerTests.cpp` `Controllers::measuredLatencyOverridesTheBudget` | `verified` |
| CTL-0-04 | User overrides in `~/Documents/Luthier/Controllers/` win | controllers §0.4 | `ControllerProfile.cpp:449-456` | `ControllerTests.cpp` `Controllers::savingAProfileReplacesTheOneOfTheSameId` | `verified` |
| CTL-1-01 | Generic MIDI: ch 1, bend 2, no per-string | controllers §1 | `ControllerProfile.cpp:350` | `ControllerTests.cpp` `Controllers::everyShipProfileIsWellFormed` | `implemented` - profile presence and sanity verified; its specific values are not asserted |
| CTL-1-02 | Generic MPE lower zone: master 1, members 2-16, bend 48/2, CC 74 timbre | controllers §1 | `makeMpeProfile` `ControllerProfile.cpp:310` | `ControllerTests.cpp` `Controllers::everyShipProfileIsWellFormed` | `implemented` - profile presence and sanity verified; its specific values are not asserted |
| CTL-1-03 | ROLI Seaboard: CC 74 -> whammy, aftertouch -> vibrato | controllers §1 | ship profile | `ControllerTests.cpp` `Controllers::everyCcMappingResolvesToARealTarget` | `implemented` - profile presence and sanity verified; its specific values are not asserted |
| CTL-1-04 | LinnStrument: optional rows-as-strings "Guitar mode" | controllers §1 | `rowsAsStrings` `ControllerProfile.h:102` | none | `implemented` |
| CTL-1-05 | Osmose: non-linear pitch LUT | controllers §1 | ship profile | `ControllerTests.cpp` `Controllers::osmosePitchCurveIsMonotonicAndPinned` | `verified` |
| CTL-1-06 | Roland GK: ch 11-16, bend 24 per string, 3 ms | controllers §1 | `makeHexProfile` | `ControllerTests.cpp` `Controllers::parsesTheSpecExampleProfile` | `implemented` - profile presence and sanity verified; its specific values are not asserted |
| CTL-1-07 | Fishman TriplePlay: ch 1-6 configurable, bend 24, 5 ms | controllers §1 | ship profile | `ControllerTests.cpp` `Controllers::everyShipProfileIsWellFormed` | `implemented` - profile presence and sanity verified; its specific values are not asserted |
| CTL-1-08 | Jamstik: ch 1-6, bend 12, 8 ms | controllers §1 | ship profile | `ControllerTests.cpp` `Controllers::everyShipProfileIsWellFormed` | `implemented` - profile presence and sanity verified; its specific values are not asserted |
| CTL-1-09 | Yamaha EZ-EG class: ch 1, no per-string, sensible defaults | controllers §1 | `ControllerProfile.cpp:428` | `ControllerTests.cpp` `Controllers::everyShipProfileIsWellFormed` | `implemented` - profile presence and sanity verified; its specific values are not asserted |
| CTL-2-01 | JSON schema: id, display_name, mode, per_string{channel, pitch_bend_semis}, cc_map, latency_ms_default, notes | controllers §2 | `ControllerProfile.cpp:115-245` | `ControllerTests.cpp` `Controllers::parsesTheSpecExampleProfile`, `Controllers::profilesRoundTripThroughJson` | `verified` |
| CTL-3-01 | Latency wizard in Options -> Controllers (now CONTROLLERS tab): click, play along, measure | controllers §3 | `ControllersPage` | `ControllerTests.cpp` `Controllers::latencyWizardIsStableAndReportsItsScatter` | `verified` |
| CTL-3-02 | Store `latency_ms_measured`; shift events earlier | controllers §3 | `ControllerProfile` | `ControllerTests.cpp` `Controllers::measuredLatencyOverridesTheBudget` | `verified` |
| CTL-3-03 | Interpolate beyond one block; warn beyond two blocks and suggest smaller buffer | controllers §3 | warning not found | none | `partial` |
| CTL-4-01 | Per-channel: channel = string; unreachable pitch dropped with diagnostic; per-string bend and pressure | controllers §4 | `MidiInterpreter` | `ControllerTests.cpp` `Controllers::perChannelRoutingSendsEachChannelToItsString` | `verified` |
| CTL-4-02 | MPE: master notes ignored; one note per member; sticky string per member channel | controllers §4 | `MidiInterpreter` | none | `implemented` - stickiness untested |
| CTL-5-01 | Calibrate bend range by confirming target pitch | controllers §5 | not found as a flow | none | `pending` |
| CTL-5-02 | Minimum note duration for lazy note-offs | controllers §5 | `MidiInterpreter.cpp:661` | `ControllerTests.cpp` `Controllers::minimumNoteDurationSurvivesAnEarlyNoteOff` | `verified` |
| CTL-5-03 | Pitch-tracking dead zone (default 5 cents) | controllers §5 | `ControllerProfile` | `ControllerTests.cpp` `Controllers::pitchDeadZoneRejectsTrackingNoiseButNotRealBends` | `verified` |
| CTL-6-01 | Multi-controller merge: per-source tagging and profiles; most recent wins per string; contention diagnostic | controllers §6 | `MidiInterpreter` | `ControllerTests.cpp` `Controllers::multiControllerMergeNeverLosesAString`, `Controllers::oneControllerPlayingNormallyReportsNoContention` | `verified` |
| CTL-7-01 | Test: every profile loads; cc_map resolves | controllers §7 | - | `ControllerTests.cpp` `Controllers::everyShipProfileIsWellFormed`, `Controllers::everyCcMappingResolvesToARealTarget` | `verified` |
| CTL-7-02 | Test: wizard stable within 0.5 ms sigma over 10 runs | controllers §7 | - | `ControllerTests.cpp` `Controllers::latencyWizardIsStableAndReportsItsScatter` | `verified` |
| CTL-7-03 | Test: 10 000-event per-channel fuzz | controllers §7 | - | `ControllerTests.cpp` `Controllers::perChannelRoutingSendsEachChannelToItsString` | `partial` - the test fuzzes 600 events per profile, not 10 000 |
| CTL-7-04 | Test: MPE stickiness on member channel | controllers §7 | - | none found | `pending` |
| CTL-7-05 | Test: dual-source stress, no dropped strings or coupling corruption | controllers §7 | - | `ControllerTests.cpp` `Controllers::multiControllerMergeNeverLosesAString` | `verified` |
| CTL-UI-01 | CONTROLLERS tab in column 4 (per gui-integration 19) | controllers §0.1 (Options) / GI 19 | `ControllersPage` in `AdvancedPanel::buildWorkspace` | `EditorTests.cpp` `Editor::everyWorkspaceTabSelectsAndPaints` | `verified` |

## 6. practice-tools.md (phase 1)

Built as milestone M36 (`Source/Practice/*`, `Source/UI/PracticePanel.*`).
Section 11's model is in the build since `980b07e`
(`Source/Practice/PracticeRoutine*`: routines and runner, progress stats,
per-tool defaults, library, session-recorder setup, count-ins, loop regions,
speed trainer) with 18 `PracticeRoutine` tests against the real tools. The
PRACTICE tab itself and the processor hooks (activity tracking, the runner
driving the drawer) are not committed (TODO 11).

| ID | Requirement (short) | Source (file §section) | Implementation location | Verification | Status |
|---|---|---|---|---|---|
| PRA-0-01 | Closed practice panel consumes no CPU | practice-tools §0.1 | `PracticePanel` / practice tools idle paths | `PracticeTests.cpp` `PracticeMetronome::silentWhenDisabled` | `partial` - CPU when closed not measured |
| PRA-0-02 | Click to monitor bus by default, optionally main | practice-tools §0.2 | `MonitorMix` click bus; `setClickToMain` / Metronome tab CLICK TO MAIN; main out when the layout has no monitor bus (DECISIONS "Where the click goes") | `TuneProcessor::theCountInIsHeardOnTheMainOutWhenSentThere`, `TuneProcessor::theMetronomeTabSendsTheClickToTheMainOut` | `verified` |
| PRA-0-03 | Loops record plugin audio and MIDI; re-render through new tone | practice-tools §0.3 | `Source/Practice/Looper.cpp` | `PracticeTests.cpp` `PracticeLooper::recordsClosesAndOverdubs` | `partial` - re-render on tone change not asserted |
| PRA-0-04 | Backing tracks stream from disk | practice-tools §0.4 | `BackingTrack.h:10-47` (BufferingAudioSource) | none | `implemented` |
| PRA-0-05 | Practice audio summed at master, own gain; never in DSP path | practice-tools §0.5 | practice sum in processor | none | `implemented` |
| PRA-1-01 | Time signatures incl. custom 1-32 / 2,4,8,16; tempo 20-300 following tap/host | practice-tools §1 | `Source/Practice/Metronome.h` | `PracticeTests.cpp` `PracticeMetronome::staysAccurateAtAnAwkwardTempo` | `implemented` |
| PRA-1-02 | Per-beat accent map, three levels | practice-tools §1 | `Metronome` | `PracticeTests.cpp` `PracticeMetronome::accentPatternIsObeyed` | `verified` |
| PRA-1-03 | Subdivisions incl. dotted, each with own sample and gain | practice-tools §1 | `enum class ClickSubdivision` `Metronome.h:50` | none | `implemented` |
| PRA-1-04 | Six click sounds; ship 6 accent+normal pairs as 24-bit/48k WAVs (16 files) in `Resources/Practice/Clicks/` | practice-tools §1, §10 | `enum class ClickSound` `Metronome.h:40` - synthesised, no files | `PracticeTests.cpp` `PracticeMetronome::everyClickSoundIsAudibleAndFinite` | `partial` - no sample files shipped |
| PRA-1-05 | Silent bars every Nth (1-16) | practice-tools §1 | `Metronome` | `PracticeTests.cpp` `PracticeMetronome::silentBarsMuteTheClickWithoutStoppingTheCount` | `verified` |
| PRA-1-06 | Progressive tempo A->B over N bars | practice-tools §1 | `Metronome` | `PracticeTests.cpp` `PracticeMetronome::progressiveTempoRampsAndStops` | `verified` |
| PRA-1-07 | 4-dot pulsing indicator | practice-tools §1 | `PracticePanel.cpp` | none | `implemented` |
| PRA-2-01 | Loop 1-240 s, bar-quantized with metronome | practice-tools §2 | `Looper.h:156` | `PracticeTests.cpp` `PracticeLooper::loopLengthQuantisesToBars` | `verified` |
| PRA-2-02 | Up to 8 layers with MIDI + audio snapshot | practice-tools §2 | `Looper.h:155` | `PracticeTests.cpp` `PracticeLooper::recordsClosesAndOverdubs` | `verified` |
| PRA-2-03 | Per-layer undo/redo | practice-tools §2 | `Looper` | `PracticeTests.cpp` `PracticeLooper::layerUndoAndRedo` | `verified` |
| PRA-2-04 | Reverse and half-speed per layer, audio only | practice-tools §2 | `Looper.h:69-75` | `PracticeTests.cpp` `PracticeLooper::reverseAndHalfSpeedDoNotAlterTheRecording` | `verified` |
| PRA-2-05 | Overdub / replace / play-once modes | practice-tools §2 | `Looper.h:32-34` | `PracticeTests.cpp` `PracticeLooper::recordsClosesAndOverdubs` | `implemented` - replace/play-once not asserted |
| PRA-2-06 | Per-layer volume, pan, low-cut, high-cut | practice-tools §2 | `Looper.h:77-136` | `PracticeTests.cpp` `PracticeLooper::mutedLayersAreSilent` | `implemented` |
| PRA-2-07 | Export mix WAV or stems | practice-tools §2 | `Looper.cpp:660` | none | `implemented` |
| PRA-2-08 | Save `.luthierloop` with MIDI, audio, settings; temp folder until saved | practice-tools §2 | saved as a folder with `loop.json` + WAVs (`Looper.cpp:671-757`) | none | `partial` - folder format, not a `.luthierloop` file (see `file-formats.md`) |
| PRA-3-01 | Formats WAV, AIFF, FLAC, MP3 (dr_mp3) | practice-tools §3 | chooser offers mp3/ogg (`PracticePanel.cpp:653`); no MP3 decoder configured | none | `partial` - MP3 decoding not enabled |
| PRA-3-02 | 4-second ring per file | practice-tools §3 | `BackingTrack.h:47` | none | `implemented` |
| PRA-3-03 | Volume, pan, mono/stereo, low/high cut | practice-tools §3 | `BackingTrack` | none | `implemented` |
| PRA-3-04 | Loop points snapped to zero crossings | practice-tools §3 | `BackingTrack.h:104` | none | `implemented` |
| PRA-3-05 | Pitch shift ±12 st, tempo 25-200%, independent | practice-tools §3 | `BackingTrack.h:108-112` | none | `implemented` |
| PRA-3-06 | Section markers with hotkeys | practice-tools §3 | `TrackMarker` `BackingTrack.h:33` | none | `implemented` - hotkeys not verified |
| PRA-3-07 | Tempo auto-detect on load | practice-tools §3 | `BackingTrack.h:14` | none | `implemented` |
| PRA-3-08 | Gapless playlist | practice-tools §3 | not found | none | `pending` |
| PRA-4-01 | Scale trainer Explore / Quiz / Interval / Chord-tone modes | practice-tools §4 | `Source/Practice/Trainers.cpp` | `PracticeTests.cpp` `PracticeTrainers::scaleQuizScoresAnswers` | `partial` - chord-tone trainer not found |
| PRA-4-02 | All diatonic modes, harmonic/melodic minor, pentatonic, blues in every key; custom scales | practice-tools §4 | `Trainers` | `PracticeTests.cpp` `PracticeTrainers::scaleTrainerKnowsItsScales` | `verified` |
| PRA-5-01 | Ear training: intervals (asc/desc/harmonic), 11 chord qualities, 15 progressions | practice-tools §5 | `Trainers.h:128-137` (14 progressions) | `PracticeTests.cpp` `PracticeTrainers::earTrainerPosesAnswerableQuestions` | `partial` - 14 progressions vs 15 |
| PRA-5-02 | Difficulty adapts | practice-tools §5 | `Trainers` | `PracticeTests.cpp` `PracticeTrainers::earTrainerDifficultyAdapts` | `verified` |
| PRA-5-03 | Stats in `~/Documents/Luthier/Practice/stats.json` | practice-tools §5, §10 | `Trainers.cpp:612` | `PracticeTests.cpp` `PracticeTrainers::earTrainerStatsRoundTrip` | `verified` |
| PRA-6-01 | Tab reader: GP5, GP6+, ASCII, MusicXML, PowerTab | practice-tools §6 | `NotationExport.cpp:1208-1226` refuses `.gp5/.gp/.ptb` with a reason | `NotationTests.cpp` `Notation::importerIsHonestAboutWhatItReads` | `partial` - binary formats unsupported |
| PRA-6-02 | Scrolling tab with cursor, tempo, section loop, count-in; fretboard highlight | practice-tools §6 | drawer TAB tab `PracticePanel.cpp` | none | `implemented` |
| PRA-6-03 | Speed trainer mode | practice-tools §6 | `Source/Practice/PracticeRoutineTempo.*` speed trainer (`980b07e`); not offered on the drawer TAB tab | `PracticeRoutineTests.cpp` `PracticeRoutine::theSpeedTrainerClimbsUntilAPassMissesNotes` | `partial` - model only; no drawer control |
| PRA-6-04 | Play-along scoring via MIDI in | practice-tools §6 | not found | none | `pending` |
| PRA-7-01 | Progression looper: parse `Am - F - C - G x4`; voice/strum/loop; tempo, feel, kit | practice-tools §7 | `Trainers.h:3` progression looper | `PracticeTests.cpp` `PracticeTrainers::progressionParsing` | `verified` |
| PRA-8-01 | Session recorder ring (default 60 min), save WAV + MIDI by timestamp | practice-tools §8 | `SessionRecorder` `Looper.cpp:966-995` | `PracticeTests.cpp` `PracticeSession::ringBufferNeverGrows` | `verified` |
| PRA-8-02 | Ring in `Sessions/tmp/`; 24 h cleanup | practice-tools §8 | `Looper.cpp:1009-1035` | none | `implemented` |
| PRA-8-03 | Disabled by default; enabled in Options | practice-tools §8 | `SessionRecorder` | `PracticeTests.cpp` `PracticeSession::disabledByDefault` | `verified` |
| PRA-9-01 | Drawer 32-360 px resizable; collapsed strip shows bpm, loop LED, track title | practice-tools §9 | `PracticePanel.cpp` | `EditorTests.cpp` `Editor::theModesThatChangeTheLayoutTakeEffectAndUndoThemselves` | `implemented` |
| PRA-9-02 | Tabs METRO, LOOP, TRACK, SCALE, EAR, TAB, PROG, SESSION | practice-tools §9 | `PracticePanel.cpp` (eight tabs, PROGRESS M36) | none | `implemented` |
| PRA-9-03 | Drawer globals: practice volume, tap tempo mirror, practice panic | practice-tools §9 | `PracticePanel.cpp` | none | `implemented` |
| PRA-10-01 | Data locations table (Clicks, Loops, stats, Sessions/tmp, Sessions) | practice-tools §10 | paths in `Looper.cpp`, `Trainers.cpp` | none | `partial` - no Clicks folder |
| PRA-11-01 | PRACTICE tab in column 4 (setup surface) | practice-tools §11 | `Source/UI/PracticeSetupPanel.*`, between CHARACTER and NOTATION | `EditorTests` `everyWorkspaceTabSelectsAndPaints`; `PracticeSetupPanel::renders` | `verified` |
| PRA-11-02 | Progress: 90-day time, per-tool breakdown, trainer accuracy, tempo progress, streak; CSV export; confirmed clear | practice-tools §11.2 | `PracticeRoutineProgress.*` (`PracticeStats`, `toCsv`, clear keeps loops/sessions) | `PracticeSetupPanel::progressReadsStatsJsonBackAndSaysSoWhenEmpty`, `clearHistoryEmptiesStatsButKeepsLoopsAndSessions`, `PracticeDrawer::theDrawerFollowsTheRoutineAndCountsItsMinutes` | `verified` |
| PRA-11-03 | Routines: ordered entries driving the drawer; three factory routines; saved to `Practice/Routines/*.json` | practice-tools §11.2 | `PracticeRoutine.*` routines, runner, library under `Practice/Routines` | `PracticeSetupPanel::aRoutineStartsInTheDrawerFromTheTab`, `routineEditsGoThroughTheModelAndRoundTrip`, `PracticeDrawer::theDrawerFollowsTheRoutineAndCountsItsMinutes`, `aRoutineStartAsksForTheDrawerOnce` | `verified` |
| PRA-11-04 | Defaults per tool (metronome, looper, trainers, backing) | practice-tools §11.2 | `PracticeRoutineSetup.*` defaults | `PracticeRoutineTests.cpp` `PracticeRoutine::defaultsSurviveAReopenAndStartTheMetronomeAtThem` | `partial` - model only; no tab to edit them |
| PRA-11-05 | Library: loops, sessions, backing folder, recent tabs, open-folder buttons | practice-tools §11.2 | `PracticeRoutineSetup.*` library listing | `PracticeSetupPanel::libraryListsLoadsAndDeletesAndStatesItsEmptyLists` | `partial` - the tab reader records recent tabs (`TabReaderTab`) but no test opens a file through it |
| PRA-11-06 | Session recorder setup: ring length, audio/MIDI, auto-save, 1.4 GB warning | practice-tools §11.2 | `PracticeRoutineSetup.*` session setup | `PracticeSetupPanel::sessionSetupWritesTheModelAndStatesTheSizeInPlainWords` | `partial` - the drawer applies the stored setup on enable (DECISIONS) without a test; record audio/MIDI and auto-save are stored but SessionRecorder ignores them |
| PRA-11-07 | No transport, trainers or live TAB on the tab | practice-tools §11.3 | `PracticeSetupPanel` (START IN DRAWER is the one exception, DECISIONS) | `PracticeSetupPanel::theTabExposesNoTransport` | `verified` |
| PRA-11-08 | Empty-state strings | practice-tools §11.4 | strings in `PracticeRoutineSetup.*` | `PracticeSetupPanel::progressReadsStatsJsonBackAndSaysSoWhenEmpty`, `libraryListsLoadsAndDeletesAndStatesItsEmptyLists` | `verified` |
| PRA-12-01 | Test: metronome ±0.5 ms at 120 bpm for 60 s | practice-tools §12 | - | `PracticeTests.cpp` `PracticeMetronome::interClickIntervalIsWithinHalfAMillisecond` | `verified` |
| PRA-12-02 | Test: looper snapshot vs fresh render within -80 dBFS | practice-tools §12 | - | none found | `pending` |
| PRA-12-03 | Test: 60-min backing stream, no memory growth | practice-tools §12 | - | none found | `pending` |
| PRA-12-04 | Test: 50 Guitar Pro fixtures parse, note counts match | practice-tools §12 | - | none (GP import unsupported) | `blocked` - needs GP parser and fixture set |
| PRA-12-05 | Test: session ring never allocates on audio thread | practice-tools §12 | - | `PracticeTests.cpp` `PracticeSession::ringBufferNeverGrows` | `verified` |
| PRA-12.1-01 | Test: stats accumulate 60 s and tab reads back | practice-tools §12.1 | - | `PracticeDrawer::theDrawerFollowsTheRoutineAndCountsItsMinutes`, `PracticeSetupPanel::progressReadsStatsJsonBackAndSaysSoWhenEmpty` | `verified` |
| PRA-12.1-02 | Test: routine drives the drawer | practice-tools §12.1 | - | `PracticeDrawer::theDrawerFollowsTheRoutineAndCountsItsMinutes` (the plugin's drawer) | `verified` |
| PRA-12.1-03 | Test: routine round trip (5 entries) | practice-tools §12.1 | - | `PracticeRoutineTests.cpp` `PracticeRoutine::aFiveEntryRoutineRoundTripsIdentically` (suite green at `980b07e`) | `verified` |
| PRA-12.1-04 | Test: defaults apply after reopen | practice-tools §12.1 | - | `PracticeRoutineTests.cpp` `PracticeRoutine::defaultsSurviveAReopenAndStartTheMetronomeAtThem` (real `Metronome`) | `verified` |
| PRA-12.1-05 | Test: no transport control on the tab | practice-tools §12.1 | - | `PracticeSetupPanel::theTabExposesNoTransport` | `verified` |
| PRA-12.1-06 | Test: clear history confirms and keeps loops/sessions | practice-tools §12.1 | - | `PracticeSetupPanel::clearHistoryEmptiesStatsButKeepsLoopsAndSessions` (confirmed path) | `verified` |

## 7. tone-match.md (phase 1)

Built as milestone M37 (`Source/ToneMatch/ToneMatch.*`,
`Source/UI/ToneMatchPanel.*`).

| ID | Requirement (short) | Source (file §section) | Implementation location | Verification | Status |
|---|---|---|---|---|---|
| TM-0-01 | IRs loaded on message thread; pointer swap to audio thread | tone-match §0.1 | `IrSlot` `ToneMatch.h:6-15` | `ToneMatchTests.cpp` `ToneMatch::anEmptySlotLeavesTheAudioAlone` | `implemented` |
| TM-0-02 | Windowed-sinc resample (>= 512 taps) to host rate before swap | tone-match §0.2 | `IrSlot::resample` `ToneMatch.h:160` | none | `implemented` - no resampling accuracy test |
| TM-0-03 | Truncate at `max_ir_seconds` (default 4 s) | tone-match §0.3 | `kMaxSecondsDefault = 4.0` `ToneMatch.h:41` | none | `implemented` |
| TM-0-04 | User IR latency equals built-in cab latency | tone-match §0.4 | convolution setup | none | `implemented` |
| TM-0-05 | Match analysis on a worker thread only | tone-match §0.5 | `ToneMatch.h:19` | none | `implemented` |
| TM-1-01 | Slots: body IR, cab IR 1, cab IR 2 | tone-match §1 | `processor.getCabIrSlot(n)`, body slot | `ToneMatchTests.cpp` `ToneMatch::irSlotSettingsRoundTrip` | `verified` |
| TM-1-02 | Slot params: file (WAV/AIFF/FLAC, <= 6 ch), channel/sum, gain ±24 dB, start/end trim, predelay 0-100 ms, reverse, mix | tone-match §1 | `ToneMatch.h:116-189`, `ToneMatch.cpp:265,422` | `ToneMatchTests.cpp` `ToneMatch::irSlotSettingsRoundTrip` | `verified` |
| TM-1-03 | Browser under `~/Documents/Luthier/IRs/` with OS drag and drop | tone-match §1 | `IrLibraryPaths::getRoot` `ToneMatch.cpp:1368`; `ToneMatchPanel::filesDropped` | none | `implemented` |
| TM-1-04 | Recent-IRs list of last 20 | tone-match §1 | not found (rg `recent` in `ToneMatchPanel.h`) | none | `pending` |
| TM-2-01 | Cab Match: DI test signal out, reference into sidechain, internal amp recorded, IR fitted | tone-match §2 | `CabMatch` `ToneMatch.h:267-279`; wizard `ToneMatchPanel.cpp` | `ToneMatchTests.cpp` `ToneMatch::sweepDeconvolutionRecoversTheSourceIr` | `verified` |
| TM-2-02 | Test signals: 6 s ESS 20 Hz-20 kHz, 4 s MLS, transient burst library | tone-match §2 | `enum class TestSignal` `ToneMatch.h:267` | `ToneMatchTests.cpp` `ToneMatch::everyTestSignalIsWellFormed` | `verified` |
| TM-2-03 | Trim to max, Hann fade on last 5%; save to `IRs/Cab Match/<name>.wav`; auto-load into cab slot | tone-match §2 | `CabMatch::saveIr` | none | `implemented` |
| TM-2-04 | Progress bar, tail estimate, null-test result | tone-match §2 | wizard UI | `ToneMatchTests.cpp` `ToneMatch::nullMeasurementIsCorrect` | `implemented` |
| TM-2-05 | Entry point "Cab Match" in the routing panel | tone-match §2 | wizard is on TONE MATCH tab | none | `partial` - not reachable from ROUTING |
| TM-3-01 | EQ Match: reference file or sidechain loop vs Luthier passage; min-phase FIR 256/1024/4096 | tone-match §3 | `EqMatch` `ToneMatch.h:315` | `ToneMatchTests.cpp` `ToneMatch::eqMatchFitsKnownCurves`, `ToneMatch::everyFilterLengthProducesAFilter` | `verified` |
| TM-3-02 | Options: match band, aggressiveness, preserve dynamics | tone-match §3 | `ToneMatch.cpp:332-335` | `ToneMatchTests.cpp` `ToneMatch::eqMatchRespectsItsBandAndOptions` | `verified` |
| TM-3-03 | Filter placed pre-amp, post-amp or post-master; saved per preset | tone-match §3 | loaded into cab slot 2 (`ToneMatchPanel.cpp:545-557`) | none | `partial` - placement choice absent |
| TM-3-04 | Labelled as not a substitute for cab match | tone-match §3 | `EqMatch::getDescription` | `ToneMatchTests.cpp` `ToneMatch::eqMatchSaysWhatItCannotDo` | `verified` |
| TM-4-01 | Capture from main, DI, sidechain, per-string; 100 ms-60 s; 32-bit float WAV to `Captures/`; autotrim toggle | tone-match §4 | `Capture` `ToneMatch.cpp:723` | `ToneMatchTests.cpp` `ToneMatch::captureRecordsAndStops`, `ToneMatch::captureAutoTrimsSilence` | `verified` |
| TM-5-01 | Library folders Bodies/Acoustic, Bodies/Electric, Cabinets/User, Cabinets/Cab Match, Rooms, Special | tone-match §5 | `IrLibraryPaths` `ToneMatch.cpp:1368+` | none | `implemented` |
| TM-5-02 | Optional `.json` sidecar (name, type, sample_rate, length_ms, author, tags, notes); filename fallback | tone-match §5 | `IrMetadata` | `ToneMatchTests.cpp` `ToneMatch::metadataRoundTripsAndFallsBackToTheFilename` | `verified` |
| TM-6-01 | TONE MATCH tab: slot cards, cab/EQ wizards, capture pane, browser with tag filter and search | tone-match §6 | `ToneMatchPanel.h:81-147` | `EditorTests.cpp` `Editor::everyWorkspaceTabSelectsAndPaints` | `implemented` |
| TM-7-01 | Presets store IR relative path under registered IR folder, else absolute | tone-match §7 | `IrSlot::toVar` | `ToneMatchTests.cpp` `ToneMatch::presetPathsAreRelativeUnderTheLibraryRoot` | `verified` |
| TM-7-02 | Missing IR falls back to built-in with header banner | tone-match §7 | `IrSlot::fromVar`; `NotificationCentre` | `EditorTests.cpp` `Editor::aFailedPresetLoadAndAMissingIrEachRaiseABannerOnce` | `verified` |
| TM-8-01 | Test: 100 IRs of varied length/rate, resampled within 0.5 dB | tone-match §8 | - | none found | `pending` |
| TM-8-02 | Test: sweep deconvolution within -60 dBFS | tone-match §8 | - | `ToneMatchTests.cpp` `ToneMatch::sweepDeconvolutionRecoversTheSourceIr` | `verified` |
| TM-8-03 | Test: EQ fit within 1 dB for shelf, bell, notch | tone-match §8 | - | `ToneMatchTests.cpp` `ToneMatch::eqMatchFitsKnownCurves` | `verified` |
| TM-8-04 | Test: 60 s capture vs offline render within -80 dBFS | tone-match §8 | - | `ToneMatchTests.cpp` `ToneMatch::captureRecordsAndStops` (not a 60 s null) | `partial` |
| TM-8-05 | Test: SR change during IR playback, no clicks, re-resampled | tone-match §8 | - | none found | `pending` |

## 8. notation-export.md (phase 1)

Exporters and importers built as milestone M38 (`Source/Notation/*`).
Section 6 `PerformanceCapture` (`Source/Capture/*`) is in the build and wired
since `18a1396`: clocked from the host play-head each block, fed from the
engine's string activity (string and fret as voiced, no technique), drained
at 10 Hz by the processor's timer. The NOTATION tab (`NotationPanel`) shows
capture state, the live tab, chord history and exports all four formats;
the header menu has "Export notation...". Nothing reports chord changes,
techniques, bass techniques or slide-bar events to the capture yet
(`chordSymbol`, `bassTechnique`, `slideBar` have no caller outside tests),
so the chord history stays empty in use.

| ID | Requirement (short) | Source (file §section) | Implementation location | Verification | Status |
|---|---|---|---|---|---|
| NOT-0-01 | Offline export on a worker thread from captured stream | notation-export §0.1 | `NotationTakeExport::write` exports the capture, called on the message thread (`NotationPanel`, `HeaderBar`) | `NotationPanelTests.cpp` `NotationTab::exportsEveryFormat` | `partial` - exports the captured stream, but not on a worker thread |
| NOT-0-02 | Guitar-aware: string and fret, not derived from pitch | notation-export §0.2 | `PerformanceScore` | `NotationTests.cpp` `Notation::stringAndFretAreNotDerivedFromPitch` | `verified` |
| NOT-0-03 | Technique metadata preserved (bend, slide, H/P, PM, harmonics, tap, whammy) | notation-export §0.3 | `ScoreTechnique` `PerformanceScore.h:30-34` | `NotationTests.cpp` `Notation::musicXmlIsWellFormedAndGuitarAware` | `verified` |
| NOT-0-04 | Round trips with documented per-format losses | notation-export §0.4 | `getNotationFormatLoss` | `NotationTests.cpp` `Notation::formatsDeclareTheirLosses`, `Notation::musicXmlRoundTrips`, `Notation::asciiTabRoundTrips` | `verified` |
| NOT-1-01 | PerformanceScore model (meta, tracks, capo, measures, voices, ScoreNote, techniques) | notation-export §1 | `Source/Notation/PerformanceScore.h` | `NotationTests.cpp` `Notation::captureBuildsMeasures` | `verified` |
| NOT-1-02 | Built in real time from engine activity; capture last N min (default 10) | notation-export §1 | `PerformanceCapture` rolling window, default 10 min, fed by `LuthierAudioProcessor::processBlock` (`captureStringActivity`) and `drainPerformanceCapture` | `CaptureTests.cpp` `Capture::rollingKeepsTheLastMinutes`; `NotationPanelTests.cpp` `NotationTab::thePluginCapturesWhatTheEnginePlayed` | `verified` |
| NOT-2.1-01 | MusicXML 4.0 with `<technical>` elements, string/fret | notation-export §2.1 | `NotationExport.cpp` `renderMusicXml` | `NotationTests.cpp` `Notation::musicXmlIsWellFormedAndGuitarAware`, `Notation::musicXmlPitchConversion` | `verified` |
| NOT-2.1-02 | Chord symbols (Poly); grace notes for H/P; multi-voice; whammy as text | notation-export §2.1 | `NotationExport.cpp:323` `<harmony>` | `NotationTests.cpp` `Notation::musicXmlRoundTrips` | `implemented` - grace notes and multi-voice not asserted |
| NOT-2.2-01 | Guitar Pro 8 `.gp` with full techniques and whammy bar events | notation-export §2.2 | `NotationExport.cpp:1172` zipped bundle | `NotationTests.cpp` `Notation::guitarProBundleIsAValidZip` | `partial` - validity only; GP fidelity untested |
| NOT-2.2-02 | Chord diagrams at first occurrence | notation-export §2.2 | `NotationExport.cpp:991-1015` | none | `implemented` |
| NOT-2.3-01 | ASCII tab: six lines low-bottom, beat ruler, symbols b r h p / \ ~ PM <12> [12], width 80, section headings | notation-export §2.3 | `NotationExport.cpp` ASCII writer | `NotationTests.cpp` `Notation::asciiTabColumnsAlign`, `Notation::asciiTabUsesTheSpecifiedSymbols` | `verified` |
| NOT-2.4-01 | MIDI: per-string tracks (16 max), GP RPN string/fret, bends as pitch bend, CC 68 legato | notation-export §2.4 | `NotationExport.cpp:839-919` | `NotationTests.cpp` `Notation::midiExportIsPerString` | `verified` |
| NOT-3-01 | Live TAB view: last N beats in practice panel, updated as played | notation-export §3 | NOTATION tab live tab (GI 4.4 / 19 place it there); the drawer TAB view still shows an imported score only | `CaptureTests.cpp` `Capture::theLiveTabShowsWhatWasPlayed`; `NotationPanelTests.cpp` `NotationTab::stateButtonsLiveTabAndPreview` | `verified` - on the NOTATION tab; drawer TAB not fed |
| NOT-3-02 | Current bar as tab dots on fretboard | notation-export §3 | not found (TODO 9 remaining) | none | `pending` |
| NOT-3-03 | Controls: show/hide, scroll speed (slow/med/fast/freeze), 1-8 bars, symbol density | notation-export §3 | `NotationPanel` SHOW TAB toggle, bars, density and speed boxes | none (the tab test reads the tab text, not the controls) | `implemented` |
| NOT-4-01 | Poly: chord symbols written at change beats | notation-export §4 | `PerformanceCapture::chordSymbol`, changes only, into the score; NOTATION tab chord history; no caller in the processor, so the Poly detector never reaches it | `CaptureTests.cpp` `Capture::techniquesBendsChordsAndMetersReachTheScore` (model) | `partial` - not wired to the chord detector |
| NOT-4-02 | Mono: offline chord extraction by template | notation-export §4 | not found | none | `pending` |
| NOT-5-01 | File menu Export -> Notation dialog: format, range, per-format options, destination, preview | notation-export §5 | NOTATION tab export: format, range (entire / last N s), quantise, per-format options, first-bar preview, file chooser; header "Export notation..." picks the format by extension with the tab's options | `NotationPanelTests.cpp` `NotationTab::exportsEveryFormat`, `NotationTab::stateButtonsLiveTabAndPreview` | `partial` - no marked-region range; the header entry has no dialog of its own |
| NOT-5-02 | "Export to Notation" beside MIDI capture "Save last take" | notation-export §5 | header menu "Export notation..." next to "Save last MIDI take..." (`HeaderBar.cpp`) | none | `implemented` |
| NOT-6-01 | PerformanceCapture records voiced notes (string, fret, time, duration, velocity, technique flags) | notation-export §6.1 | `PerformanceCapture` fed by `captureStringActivity` from the processor (DECISIONS "fed from the engine's string activity"); the `noteOn` / `noteOff` engine hook that carries techniques is not wired | `CaptureTests.cpp` `Capture::recordsVoicedNotesNotMidi`; `NotationPanelTests.cpp` `NotationTab::thePluginCapturesWhatTheEnginePlayed` | `partial` - no technique flags in use (TODO 9) |
| NOT-6-02 | Chord, tempo, time-signature, BASS_TECH and slide tracks | notation-export §6.1 | meter records from `beginBlock` (wired); `chordSymbol`, `bassTechnique`, `slideBar` -> `CapturedEvent` have no producer | `CaptureTests.cpp` `Capture::techniquesBendsChordsAndMetersReachTheScore`, `Capture::bassAndSlideEventsBecomeLuthierEvents` (model) | `partial` - only tempo / meter reach the capture in use |
| NOT-6-03 | Lock-free ring 8192 records, pre-allocated; 10 Hz drain; drop oldest + counter | notation-export §6.2 | `Source/Capture/CaptureRing.h`; drained every third tick of the processor's 30 Hz timer (`drainPerformanceCapture`) | `CaptureTests.cpp` `Capture::ringOverflowDropsTheOldestAndCountsExactly` | `verified` - drain rate by construction |
| NOT-6-04 | States off / rolling (default) / armed | notation-export §6.3 | `CaptureState`; NOTATION tab state buttons | `CaptureTests.cpp` `Capture::armedStartsCleanFromTheNextNote`, `Capture::offWritesNothingAndLeavesTheAudioAlone`; `NotationPanelTests.cpp` `NotationTab::thePluginCapturesWhatTheEnginePlayed` (rolling default), `NotationTab::stateButtonsLiveTabAndPreview` | `verified` |
| NOT-6-05 | Times in quarter notes against transport, seconds when stopped; no input quantise | notation-export §6.4-6.5 | `CaptureClock` per block from the host play-head; quantise only in `CaptureScoreOptions` | `CaptureTests.cpp` `Capture::transportTimingIsInQuarterNotes`, `Capture::freePlayIsInSecondsAndQuantisesAfterwards` | `verified` |
| NOT-7-01 | Test: MusicXML round trip via MuseScore fixture | notation-export §7 | - | `NotationTests.cpp` `Notation::musicXmlRoundTrips` (self round trip, no MuseScore fixture) | `partial` |
| NOT-7-02 | Test: Guitar Pro round trip via fixture parser | notation-export §7 | - | none | `pending` |
| NOT-7-03 | Test: ASCII column alignment at 4/4 | notation-export §7 | - | `NotationTests.cpp` `Notation::asciiTabColumnsAlign` | `verified` |
| NOT-7-04 | Test: MIDI export re-rendered within -60 dBFS | notation-export §7 | NOTATION tab MIDI goes through the MIDI OUT profile (DECISIONS "The performance capture is fed from the engine's string activity") | `MidiExportTests.cpp` `MidiExport::luthierRoundTripNullsEveryFactoryPreset` (the shared path, not a notation-tab export) | `partial` - no null test of a captured take's export |
| NOT-7-05 | Test: chord extraction > 95% on 100 progressions | notation-export §7 | - | none | `pending` |
| NOT-7.1-01 | Test: capture records voiced notes not MIDI | notation-export §7.1 | - | `CaptureTests.cpp` `Capture::recordsVoicedNotesNotMidi` (suite green at `18a1396`) | `verified` |
| NOT-7.1-02 | Test: no audio-thread allocation over 10 000 notes | notation-export §7.1 | the assertion is under `#if defined (LUTHIER_ALLOCATION_COUNTER)`, which nothing in `CMakeLists.txt` or `Source` defines | `CaptureTests.cpp` `Capture::capturingTenThousandNotesDoesNotAllocate` (checks record and drop counts only as built) | `implemented` - allocation check compiled out |
| NOT-7.1-03 | Test: overflow drops oldest with exact counter | notation-export §7.1 | - | `CaptureTests.cpp` `Capture::ringOverflowDropsTheOldestAndCountsExactly` | `verified` |
| NOT-7.1-04 | Test: off costs nothing (bit-identical audio) | notation-export §7.1 | - | `CaptureTests.cpp` `Capture::offWritesNothingAndLeavesTheAudioAlone` | `verified` |
| NOT-7.1-05 | Test: transport timing at 120 bpm within 1 ms | notation-export §7.1 | - | `CaptureTests.cpp` `Capture::transportTimingIsInQuarterNotes` | `verified` |
| NOT-7.1-06 | Test: free-play timing in seconds | notation-export §7.1 | - | `CaptureTests.cpp` `Capture::freePlayIsInSecondsAndQuantisesAfterwards` | `verified` |
| NOT-7.1-07 | Test: live TAB shows what was played | notation-export §7.1 | - | `CaptureTests.cpp` `Capture::theLiveTabShowsWhatWasPlayed`; `NotationPanelTests.cpp` `NotationTab::stateButtonsLiveTabAndPreview` | `verified` |
| NOT-7.1-08 | Test: captured score round-trips via Luthier-profile MIDI | notation-export §7.1 | - | `CaptureTests.cpp` `Capture::aCapturedPhraseRoundTripsThroughLuthierMidi` | `verified` |
| NOT-UI-01 | NOTATION tab in column 4 (gui-integration 4.4) | GI 4.4 | `NotationPanel`, between CHARACTER and MIDI OUT (`18a1396`) | `EditorTests.cpp` `Editor::everyWorkspaceTabSelectsAndPaints`; `NotationPanelTests.cpp` `NotationTab::*` | `verified` |
| NOT-IMP-01 | Importers honest about what they read; empty score refused with reason | notation-export §0.4 | `NotationExport.cpp:1208-1226` | `NotationTests.cpp` `Notation::importerIsHonestAboutWhatItReads`, `Notation::anEmptyScoreIsRefusedWithAReason` | `verified` |

## 9. character-wear.md (phase 1)

Built as milestone M39 (`Source/Character/CharacterEngine.*`,
`Source/UI/CharacterPanel.*`). The CHARACTER tab now also hosts the
STRING NOISE, PICK, SETUP and SLIDE groups from later specs.

| ID | Requirement (short) | Source (file §section) | Implementation location | Verification | Status |
|---|---|---|---|---|---|
| CW-0-01 | Deterministic per instance seed | character-wear §0.1 | `CharacterEngine.h:10-72` | `CharacterTests.cpp` `Character::fixedSeedIsByteIdentical`, `Character::valuesDoNotDependOnAccessOrder` | `verified` |
| CW-0-02 | Per-note / per-region influence, smoothed; not per-sample | character-wear §0.2 | per-note sustain scale (DECISIONS "Per-note sustain scale") | none | `implemented` |
| CW-0-03 | On by default at low intensity | character-wear §0.3 | `enableToggle`, `amountSlider` defaults | none | `implemented` - default level not asserted |
| CW-0-04 | Stacks with humanize | character-wear §0.4, §11 | - | none | `implemented` |
| CW-1-01 | 64-bit `character_seed` in preset; "New Character" rerolls and saves | character-wear §1 | `CharacterEngine::setSeed`; `CharacterPanel.h:121` | `CharacterTests.cpp` `Character::stateRoundTrips`, `Character::differentSeedsProduceDifferentInstruments` | `verified` |
| CW-2-01 | 0-3 dead spots per string, beta around frets 6-12; depth 0.1-0.7; width 2-5 | character-wear §2 | `CharacterEngine` | `CharacterTests.cpp` `Character::deadSpotsAreInRangeAndClusterCorrectly` | `verified` |
| CW-2-02 | Loop gain reduced by depth x Gaussian weight | character-wear §2 | per-note sustain scale | `CharacterTests.cpp` `Character::deadSpotsReduceSustainWhereTheyAre` | `verified` |
| CW-2-03 | Attenuation strongest near body air resonance | character-wear §2 | not found | none | `pending` |
| CW-3-01 | Per-fret wear 0-1, heavier frets 1-5 and 12-17 on plain strings | character-wear §3 | `CharacterEngine` | `CharacterTests.cpp` `Character::fretWearFollowsRealWearPatterns` | `verified` |
| CW-3-02 | Worn frets: less sustain, few cents detune | character-wear §3 | `CharacterEngine` | `CharacterTests.cpp` `Character::wornFretsBehaveAsDescribed` | `verified` |
| CW-3-03 | Worn frets raise buzz probability at low action | character-wear §3 | waits on per-fret wear height (TODO step 4 note; fret-buzz 8) | none | `pending` |
| CW-3-04 | "Refret" resets wear (Options -> Character) | character-wear §3 | `CharacterEngine::refret`; button on CHARACTER tab `CharacterPanel.h:130` | none | `implemented` |
| CW-4-01 | Per-string drift LFO 0-5 cents, 20-90 s, random phase; scaled by `tuner_looseness` default 15% | character-wear §4 | `CharacterEngine.h:123-133` | `CharacterTests.cpp` `Character::tunerDriftStaysWithinItsStatedAmplitude` | `verified` |
| CW-4-02 | Environmental envelope over minutes; reroll reseeds drift | character-wear §4 | `CharacterEngine` | none | `implemented` |
| CW-5-01 | Aged volume-pot linearity error | character-wear §5 | `CharacterEngine.h:141` | `CharacterTests.cpp` `Character::agedPotTaperIsMonotonicAndPinned` | `verified` |
| CW-5-02 | Tone cap drifts ±5% per seed | character-wear §5 | `CharacterEngine` | `CharacterTests.cpp` `Character::capacitorDriftIsInRangeAndDeterministic` | `verified` |
| CW-5-03 | Intermittent jack dropouts 20-100 ms, off by default | character-wear §5 | `CharacterEngine.h:153-164` | `CharacterTests.cpp` `Character::intermittentJackIsOffByDefault` | `verified` |
| CW-5-04 | Piezo per-saddle balance ±2 dB | character-wear §5 | `getSaddleBalanceDb` `CharacterEngine.h:173` | none | `implemented` |
| CW-6-01 | Per-string per-pickup balance ±1.5 dB; pole height inconsistency | character-wear §6 | `getPickupBalanceDb` `CharacterEngine.h:170` | none | `implemented` |
| CW-7-01 | Nut slot wear damps open strings | character-wear §7 | `getNutDamping` `CharacterEngine.h:176` | none | `implemented` |
| CW-7-02 | Saddle height ±0.2 mm per seed (intonation) | character-wear §7 | `getSaddleHeightOffsetMm` `CharacterEngine.h:179` | none | `implemented` |
| CW-7-03 | Bone vs synthetic nut biases HF damping | character-wear §7 | `boneNutToggle` `CharacterPanel.h:141` (now also Workshop nut part) | `CharacterTests.cpp` `Character::nutMaterialChangesDamping` | `verified` |
| CW-8-01 | Body age raises modal Q, drops air 3-8%, lowers HF damping 5-10%; applied on load | character-wear §8 | `body_age` param, `CharacterEngine` | `CharacterTests.cpp` `Character::bodyBreakInMovesInTheRightDirection` | `verified` |
| CW-9-01 | Environment: temperature, humidity, session time, retune | character-wear §9 | CHARACTER tab environment group (`CharacterPanel.h:145`), not an Options page | `CharacterTests.cpp` `Character::temperatureProducesTheExpectedOffset`, `Character::humidityMovesTheBodyTheRightWay`, `Character::retuneResetsTheDrift` | `verified` |
| CW-10-01 | CHARACTER tab: seed + New Character, dead-spot map with drag editing, fret-wear map click-drag, looseness, electronics, body age, environment | character-wear §10 | `Source/UI/CharacterPanel.cpp` | `EditorTests.cpp` `Editor::everyWorkspaceTabSelectsAndPaints` (paint only) | `implemented` |
| CW-10-02 | "All fresh" and "All old" buttons | character-wear §10 | `CharacterPanel.h:149` | `CharacterTests.cpp` `Character::allFreshAndAllOldPresets` | `verified` |
| CW-12-01 | Test: seed determinism | character-wear §12 | - | `CharacterTests.cpp` `Character::fixedSeedIsByteIdentical` | `verified` |
| CW-12-02 | Test: dead spot >= 10% shorter T60 at depth >= 0.5 | character-wear §12 | - | `CharacterTests.cpp` `Character::deadSpotsReduceSustainWhereTheyAre` | `verified` |
| CW-12-03 | Test: 10 min drift at 5% looseness within 1 cent RMS | character-wear §12 | - | `CharacterTests.cpp` `Character::tunerDriftStaysWithinItsStatedAmplitude` | `verified` |
| CW-12-04 | Test: 20 K temperature step gives expected tuning offset | character-wear §12 | - | `CharacterTests.cpp` `Character::temperatureProducesTheExpectedOffset` | `verified` |
| CW-12-05 | Test: zero character bitwise identical to no-wear render | character-wear §12 | - | `CharacterTests.cpp` `Character::zeroCharacterIsExactlyNeutral` | `verified` |

## 10. accessibility.md (phase 1)

Built as milestone M40 (`Source/Accessibility/*`). Screen-reader and locale
work is thin: the catalog holds English only (242 entries in
`Localisation.cpp`) and no `Resources/i18n` or `Resources/Themes` folder
exists on disk.

| ID | Requirement (short) | Source (file §section) | Implementation location | Verification | Status |
|---|---|---|---|---|---|
| ACC-0-01 | Every control has accessible label, role, value description | accessibility §0.1, §1 | JUCE default handlers for Slider/Button; explicit titles in few files (`CircuitPanel.cpp`, `RangesUi.cpp`, `Widgets.cpp`) | none | `partial` - no per-control audit; fretboard not exposed |
| ACC-0-02 | Nothing conveyed by colour alone | accessibility §0.2 | e.g. `LiveStrip.cpp:37,139`; heatmap cells | `BuzzTests.cpp` `BuzzUi::heatmapCellsReadInMonochromeTerms`; `AccessibilityTests.cpp` `Accessibility::colourblindPalettesSeparateTheStatesTheyTarget` | `partial` - spot-checked panels only |
| ACC-0-03 | Every shortcut rebindable and printable | accessibility §0.3, §2 | shortcut registry `Accessibility.cpp:462-515` | `AccessibilityTests.cpp` `Accessibility::shortcutsRebindAndRefuseClashes`, `Accessibility::everyShortcutHasADescriptionInTheCatalog` | `verified` |
| ACC-0-04 | All UI strings in a catalog | accessibility §0.4 | `Source/Accessibility/Localisation.cpp` (subset); many panels hard-code strings (e.g. `RhythmPanel.cpp:813`) | `AccessibilityTests.cpp` `Localisation::catalogCoversTheUi` (checks listed keys only) | `partial` |
| ACC-0-05 | UI scale 75-200% without breaking layout | accessibility §0.5, §4 | `Accessibility.cpp:8` scales | `AccessibilityTests.cpp` `Accessibility::uiScaleStepsAndFontFloor` | `partial` - reflow at scale not tested |
| ACC-1-01 | Meters report peak dBFS as accessible values | accessibility §1 | not found | none | `pending` |
| ACC-1-02 | Fretboard exposes each fret as a labelled child ("String 3, fret 5 ...") | accessibility §1 | not found in `FretboardComponent.*` | none | `pending` |
| ACC-1-03 | Snapshot strip buttons expose snapshot names | accessibility §1 | `LiveStrip.cpp` | none | `implemented` - unverified with a reader |
| ACC-1-04 | Overlays announce on open; focus to first control; Escape returns focus to launcher | accessibility §1 | Escape closes (`PluginEditor.cpp`); announcements only in `InlineNotice` (`Widgets.cpp:1238`) | `EditorTests.cpp` `Editor::everyOverlayShortcutOpensItsOwnOverlayAndEscapeClosesIt` | `partial` - announce/focus return not built |
| ACC-1-05 | Tested with NVDA, VoiceOver, Orca | accessibility §1 | - | not performed | `pending` |
| ACC-2-01 | Tab / Shift-Tab order left-right, top-bottom by panel | accessibility §2 | JUCE focus traversal | none | `pending` - no defined order or test |
| ACC-2-02 | Arrows adjust focused control (Shift finer, Ctrl coarser); Enter opens dropdowns | accessibility §2 | JUCE defaults | none | `implemented` |
| ACC-2-03 | F1 context help for focused control | accessibility §2 | F1 opens Help overlay (GAPS A2) | none | `partial` - not context-sensitive |
| ACC-2-04 | Every action reachable by shortcut | accessibility §2 | registry | `AccessibilityTests.cpp` `Accessibility::shortcutDefaultsMatchTheCanonicalTable` | `verified` |
| ACC-2-05 | "Show all shortcuts" overlay with search | accessibility §2 | `Ctrl+Shift+/` table (GAPS A5) | none | `implemented` |
| ACC-3-01 | Palettes Default, Deuteranopia, Protanopia, Tritanopia, High contrast, Light in Options -> Appearance | accessibility §3 | `Accessibility.cpp` palettes, now applied to the UI live (`a406915`) | `AccessibilityTests.cpp` `Accessibility::colourblindPalettesSeparateTheStatesTheyTarget`, `Accessibility::palettesMeetContrastRequirements`; `ThemeTests.cpp` `Theme::aPaletteChangeReachesBuiltComponents` | `verified` |
| ACC-3-02 | Palettes ship as `Resources/Themes/*.json` | accessibility §3 | loader `Accessibility.cpp:342`; no files on disk | `AccessibilityTests.cpp` `Accessibility::palettesRoundTripThroughJson` | `partial` - built-in only |
| ACC-3-03 | Meters use shape too: narrow strip below -18 dB, bracket icon over 0 dB | accessibility §3 | not found in `Widgets.cpp` | none | `pending` |
| ACC-4-01 | Scales 75-200; min 10 px effective font | accessibility §4 | `Accessibility.cpp:8` | `AccessibilityTests.cpp` `Accessibility::uiScaleStepsAndFontFloor` | `verified` |
| ACC-4-02 | Panels reflow (small knobs, wrap) rather than clip | accessibility §4 | not found | none | `pending` |
| ACC-4-03 | Window minimum grows with scale; restore-too-big picks smaller scale and warns once | accessibility §4 | not found | none | `pending` |
| ACC-5-01 | Reduced motion: stops data stream, 0 ms control animation, no LED pulse; static colour feedback | accessibility §5 | `Accessibility.h:150-157` | `AccessibilityTests.cpp` `Accessibility::reducedMotionRemovesAnimation` | `verified` |
| ACC-6-01 | Catalog `Resources/i18n/<locale>.json`, flat key-value | accessibility §6 | loader `Localisation.cpp:60`; no files on disk | none | `partial` |
| ACC-6-02 | 15 ship locales (en .. it) | accessibility §6 | locale table `Localisation.cpp:26-34`; no translations | `AccessibilityTests.cpp` `Localisation::everyShipLocaleIsOffered` (offered, not translated) | `partial` - translations missing |
| ACC-6-03 | Locale switch without restart | accessibility §6 | Options LOCALIZATION | none | `implemented` |
| ACC-6-04 | Missing key -> en; missing en -> debug warning | accessibility §6 | `Localisation::translate` | `AccessibilityTests.cpp` `Localisation::missingKeysFallBackRatherThanBlank` | `verified` |
| ACC-6-05 | Named placeholders, no concatenation | accessibility §6 | `Localisation` | `AccessibilityTests.cpp` `Localisation::placeholdersAreNamedAndSubstitute` | `partial` - many UI strings still concatenated in code |
| ACC-6-06 | Layout not LTR-assuming; bidi fixture before ship | accessibility §6 | not found | none | `pending` |
| ACC-7-01 | Manual translated; Help shows current locale | accessibility §7 | English only | none | `pending` |
| ACC-7-02 | Shortcuts panel is a live view of bindings | accessibility §7 | `getPrintableShortcuts` | `AccessibilityTests.cpp` `Accessibility::everyShortcutHasADescriptionInTheCatalog` | `verified` |
| ACC-8-01 | System font settings honoured; tabular numerics; CJK/Arabic fallback stack | accessibility §8 | `Theme.cpp:27-42` fallback lists; `fontOverride` | none | `partial` - CJK/Arabic fallback not verified |
| ACC-9-01 | Options -> Accessibility: verbosity, rebind table with search/reset, scale, palette, reduced motion, font override | accessibility §9 | `Accessibility.h:107-166`; ACCESSIBILITY page | `AccessibilityTests.cpp` `Accessibility::settingsRoundTrip` | `implemented` |
| ACC-9-02 | Options -> Localization: locale, fallback locale, custom catalog path | accessibility §9 | LOCALIZATION page | none | `implemented` - fallback and custom path not verified |
| ACC-10-01 | Test: automated NVDA smoke | accessibility §10 | - | none | `pending` |
| ACC-10-02 | Test: automated Tab walk, no dead end | accessibility §10 | - | none | `pending` |
| ACC-10-03 | Test: contrast >= 4.5 on Default, High contrast, Light; switch does not clip | accessibility §10 | - | `AccessibilityTests.cpp` `Accessibility::palettesMeetContrastRequirements`, `Accessibility::contrastRatioIsCorrect` | `partial` - clipping not checked |
| ACC-10-04 | Test: every locale renders every panel unclipped at 100% and 150% | accessibility §10 | - | none | `pending` |
| ACC-10-05 | Test: reduced motion stops animation frames | accessibility §10 | - | `AccessibilityTests.cpp` `Accessibility::reducedMotionRemovesAnimation` | `verified` |
| ACC-10-06 | Test: CJK font fallback, no missing glyphs | accessibility §10 | - | none | `pending` |

## 11. updates-telemetry.md (phase 1)

Built as milestone M41 (`Source/Updates/Telemetry.*`). The client side is
largely there; the server side (manifest, ingest, crash upload, licence
activation at `*.luthieraudio.com`) does not exist in this repo, and nothing
writes the crash dumps the upload path looks for.

| ID | Requirement (short) | Source (file §section) | Implementation location | Verification | Status |
|---|---|---|---|---|---|
| UT-0-01 | Every outbound option off by default | updates-telemetry §0.1 | `Telemetry.h:279-281` defaults | `TelemetryTests.cpp` `Telemetry::everythingIsOffByDefault` | `verified` |
| UT-0-02 | No PII (licence, filenames, preset names) in payloads | updates-telemetry §0.2 | payload builders | `TelemetryTests.cpp` `Telemetry::recordsAreLoggedLocallyAndSentWhenAllowed` | `implemented` - PII absence not grep-asserted |
| UT-0-03 | Every outbound call logged locally (destination, size, time, category) | updates-telemetry §0.3 | `Telemetry.h:12-13,265` | `TelemetryTests.cpp` `Telemetry::everyOutboundCallIsLogged` | `verified` |
| UT-0-04 | Network on a worker thread only | updates-telemetry §0.4 | launch-then-callAsync (PROGRESS A6) | none | `implemented` |
| UT-0-05 | Never auto-installs | updates-telemetry §0.5 | notify only | none | `implemented` |
| UT-1-01 | Opt-in; on load throttled to 24 h, or "Check now" | updates-telemetry §1 | `Telemetry.cpp:450` | `TelemetryTests.cpp` `Telemetry::updateCheckIsThrottled` | `verified` |
| UT-1-02 | Manifest schema (schema, latest_stable, latest_beta, minimum_supported, changelog_url, downloads per platform) | updates-telemetry §1 | `Telemetry.h:42-67` | `TelemetryTests.cpp` `Telemetry::updateCheckReadsTheManifest` | `verified` |
| UT-1-03 | Semantic version comparison | updates-telemetry §1 | `Version::parse` | `TelemetryTests.cpp` `Telemetry::versionComparison` | `verified` |
| UT-1-04 | Non-modal header banner with What's new + Download | updates-telemetry §1 | `NotificationCentre` update trigger | `EditorTests.cpp` `Editor::notificationBannersQueueDismissAndRespectTheirActions` (mechanism) | `implemented` |
| UT-1-05 | Beta channel toggle | updates-telemetry §1 | `Telemetry.h:171` | none | `implemented` |
| UT-1-06 | Live manifest endpoint exists | updates-telemetry §1 | `manifestUrl` `Telemetry.h:283` points at `updates.luthieraudio.com` | none | `blocked` - server not in scope of this repo |
| UT-2-01 | Delta update packages (bsdiff binaries, rsync-style resources), full installer fallback | updates-telemetry §2 | none | none | `pending` - installer work |
| UT-3-01 | Usage telemetry: panels, presets loaded, CPU, flags; no names; daily if active | updates-telemetry §3 | `Category::usage` `Telemetry.h:143` | `TelemetryTests.cpp` `Telemetry::recordsAreLoggedLocallyAndSentWhenAllowed` | `implemented` |
| UT-3-02 | Diagnostics telemetry: non-crash errors, warnings, host info | updates-telemetry §3 | `Category::diagnostics` | none | `implemented` |
| UT-3-03 | Local log `Diagnostics/telemetry-<yyyymm>.log`, JSON lines | updates-telemetry §3 | `getTelemetryLogFile` | `TelemetryTests.cpp` `Telemetry::recordsAreLoggedLocallyAndSentWhenAllowed` | `verified` |
| UT-4-01 | Crash reporting opt-in in Options -> Diagnostics | updates-telemetry §4 | `setCrashUploadEnabled` `Telemetry.h:165` | `TelemetryTests.cpp` `Telemetry::everythingIsOffByDefault` | `verified` |
| UT-4-02 | On crash write minidump `crash-<ts>.dmp` + troubleshooting file | updates-telemetry §4 | no crash handler writes `.dmp` (rg `setApplicationCrashHandler` finds nothing); reader at `Telemetry.cpp:575` | none | `pending` |
| UT-4-03 | Next launch prompts with viewer of exactly what is sent | updates-telemetry §4 | crash banner "Review" -> PRIVACY; `describePendingCrashReport` | `TelemetryTests.cpp` `Telemetry::crashReportDescribesItself` | `implemented` - depends on UT-4-02 |
| UT-4-04 | Single HTTPS POST, max three attempts, keep dump on failure | updates-telemetry §4 | `uploadPendingCrashReport` `Telemetry.h:220` | none | `implemented` |
| UT-4-05 | Dumps never contain audio/MIDI | updates-telemetry §4 | - | none | `pending` - no dumps written yet |
| UT-5-01 | Commercial licence: online activation, revalidate every 30 days, 14-day offline grace | updates-telemetry §5 | `Telemetry.h:361-362` | `TelemetryTests.cpp` `Telemetry::licenceActivationAndGrace`, `Telemetry::revalidationCountdownAndOfflineTolerance` | `verified` |
| UT-5-02 | Offline challenge/response activation | updates-telemetry §5 | `Telemetry.h:327-333` | none | `implemented` |
| UT-5-03 | Revalidation sends signed proof, not key | updates-telemetry §5 | `License` | none | `implemented` |
| UT-5-04 | One-click deactivation frees the seat | updates-telemetry §5 | `License::deactivate` `Telemetry.h:324` | none | `implemented` - server side absent |
| UT-5-05 | Activation server and signing keys | updates-telemetry §5 | none | none | `blocked` - server/business setup outside repo |
| UT-6-01 | Privacy dashboard: per-category explanation, toggles, view last upload, clear logs, editable endpoints, turn-everything-off-and-delete | updates-telemetry §6 | Options PRIVACY page; `Telemetry.h:245-253` | `TelemetryTests.cpp` `Telemetry::turnEverythingOffDeletesAndDisables`, `Telemetry::settingsRoundTrip` | `verified` |
| UT-7-01 | `luthier-policy.json` forces off / private mirror / no crash upload | updates-telemetry §7 | `Policy::getPolicyFile` `Telemetry.cpp:184-194` | `TelemetryTests.cpp` `Telemetry::policyOverridesTheUser` | `verified` |
| UT-7-02 | "Managed by policy" indicator | updates-telemetry §7 | banner trigger (GAPS A6) | `EditorTests.cpp` `Editor::theWindowRaisesSectionFifteensTriggersAndIsQuietWhenItShould` (quiet path) | `implemented` |
| UT-8-01 | Test: no-network mode silent | updates-telemetry §8 | - | `TelemetryTests.cpp` `Telemetry::noNetworkIsSilentRatherThanAnError` | `verified` |
| UT-8-02 | Test: fresh install all four toggles off | updates-telemetry §8 | - | `TelemetryTests.cpp` `Telemetry::everythingIsOffByDefault` | `verified` |
| UT-8-03 | Test: policy prevents enabling disallowed features | updates-telemetry §8 | - | `TelemetryTests.cpp` `Telemetry::policyOverridesTheUser` | `verified` |
| UT-8-04 | Test: crash dump privacy grep | updates-telemetry §8 | - | none | `pending` |
| UT-8-05 | Test: 5 sequential deltas match full-installer SHA | updates-telemetry §8 | - | none | `pending` |

## 12. advanced-ranges.md (phase 2)

Built (TODO done items 1, 2b; `Source/PhysicalRange.*`, `Source/UI/RangesUi.*`).
Precedence 7 for range semantics. DECISIONS 2026-09-22 records three calls
here (restrict item per 4 not GI 16, bubble notice, master toggle clears
per-control unlocks).

| ID | Requirement (short) | Source (file §section) | Implementation location | Verification | Status |
|---|---|---|---|---|---|
| AR-0-01 | Stock is the default for new, factory and first-run | advanced-ranges §0.1 | `RangeState` defaults | `RangeTests.cpp` `Ranges::theRangesBlockRoundTripsAndDerivesWhenAbsent` | `verified` |
| AR-0-02 | Advanced always marked | advanced-ranges §0.2 | arc + `*` + padlock (`RangesUi`) | `RangeTests.cpp` `Ranges::markingFollowsTheValueNotTheMode` | `verified` |
| AR-0-03 | Mode switch alone never changes audio; narrowing clamps, undoable, announced | advanced-ranges §0.3 | `processor.changeRanges` `RangesUi.cpp:300` | `RangeTests.cpp` `Ranges::wideningPreservesEveryPlainValue`, `Ranges::narrowingClampsAndReportsTheCount` | `verified` |
| AR-0-04 | Range mode belongs to the preset; display prefs to the user | advanced-ranges §0.4 | preset `ranges` block; `UiPreferences` | `IntegrationTests.cpp` `Presets::anAdvancedValueSurvivesTheRoundTrip` | `verified` |
| AR-0-05 | Only physical parameters get a PhysicalRange | advanced-ranges §0.5 | sparse registry `PhysicalRange.cpp:99-149` | `RangeTests.cpp` `Ranges::everyPhysicalRangeIsValid` | `verified` |
| AR-0-06 | No parameter count change | advanced-ranges §0.6 | - | `IntegrationTests.cpp` `Parameters::everyParameterHasAUniqueIdAndSaneDefault` (pinned count) | `verified` |
| AR-1-01 | PhysicalRange fields and invariants (advanced contains stock; default in stock; non-degenerate) | advanced-ranges §1 | `Source/PhysicalRange.h` | `RangeTests.cpp` `Ranges::everyPhysicalRangeIsValid` | `verified` |
| AR-1.0-01 | Existing parameter's stock = its shipped declared range | advanced-ranges §1.0 | `RangeRegistry::noteDeclaration` | `RangeTests.cpp` `Ranges::stockMatchesTheDeclaredRange` | `verified` |
| AR-1.1-01 | Normalisation against the live range | advanced-ranges §1.1 | live `NormalisableRange` swap | `RangeTests.cpp` `Ranges::normalisationFollowsTheLiveRange` | `verified` |
| AR-1.1-02 | Mode switch is structural (command queue); writes plain values back | advanced-ranges §1.1-1.3 | `processor.changeRanges` | `RangeTests.cpp` `Ranges::wideningPreservesEveryPlainValue` | `verified` |
| AR-1.1-03 | qa-polish automation matrix gains one case per family | advanced-ranges §1.1 | - | none | `pending` - see qa-polish rows |
| AR-1.3-01 | Narrowing produces a banner with clamp count and an undo entry restoring values | advanced-ranges §1.3 | confirmation with count / bubble (DECISIONS: no separate banner) | `RangeTests.cpp` `Ranges::narrowingClampsAndReportsTheCount`; `RangesUiTests.cpp` `RangesUi::theRangesPageListsLocksAndClamps` | `verified` - banner replaced by in-place report (DECISIONS) |
| AR-1.4-01 | Modulation depth sweeps the live range | advanced-ranges §1.4 | `ParameterBridge::value` | none | `implemented` |
| AR-2-01 | Seven families amp, circuit, squeak, buzz, pick, slide, modulation | advanced-ranges §2 | `enum class RangeFamily` `PhysicalRange.h:29` | `RangeTests.cpp` `Ranges::theRangesBlockRoundTripsAndDerivesWhenAbsent` | `verified` |
| AR-2.1-01 | `modulation` family works by clamping ModMatrix setters (LFO rate, EG times, seq rate, follower times) | advanced-ranges §2.1, §3.4 | no clamp found (rg `RangeFamily::modulation` only in name tables) | none | `pending` |
| AR-3.1-01 | amp stock/advanced pairs (gain 0-1/0-2, EQ 0-1/-0.5-1.5, presence, master) | advanced-ranges §3.1 | `PhysicalRange.cpp:99-104` | `CircuitTests.cpp` `AmpRanges::pastTheKnobIsAudible` | `verified` |
| AR-3.2-01 | circuit pairs: pots 100k-1M/1k-10M, cap 10n-100n/1n-1u, cable 0.5-15/0-100 m, input Z 220k-1M/10k-10M | advanced-ranges §3.2 | `PhysicalRange.cpp:111-119` (cap in nF, DECISIONS) | `CircuitTests.cpp` `Circuit::everyCornerOfTheAdvancedRangeIsStable` | `verified` |
| AR-3.3-01 | squeak, pick, buzz, slide pairs from their own specs | advanced-ranges §3.3 | `PhysicalRange.cpp:125-149` | `RangeTests.cpp` `Ranges::everyPhysicalRangeIsValid` | `verified` |
| AR-4-01 | Preset `ranges` block: families map, `per_control_unlocks`; redundant unlocks dropped on save | advanced-ranges §4 | `RangeState` toVar/fromVar | `RangeTests.cpp` `Ranges::theRangesBlockRoundTripsAndDerivesWhenAbsent` | `verified` |
| AR-4-02 | Restrict item only for controls in `per_control_unlocks` | advanced-ranges §4 | `RangesUi` context menu | `RangesUiTests.cpp` `RangesUi::rightClickUnlocksAndRestrictsOneControl` | `verified` |
| AR-4.1-01 | Legacy load derives per family from plain values, after params set; malformed block = absent | advanced-ranges §4.1 | `RangeState` derivation | `RangeTests.cpp` `Ranges::theRangesBlockRoundTripsAndDerivesWhenAbsent` | `verified` |
| AR-5-01 | Snapshots never carry range mode; recall clamps under a lock | advanced-ranges §5 | snapshot blobs exclude ranges | none | `implemented` - untested |
| AR-5-02 | A/B slots each carry a ranges block | advanced-ranges §5 | A/B slot state | none | `implemented` |
| AR-5-03 | Randomise respects stock by default (`randomise_respects_stock`) | advanced-ranges §5 | `randomiseRespectsStock` `PluginProcessor.cpp:1467` | `RangesUiTests.cpp` `RangesUi::randomiseStaysInStockUnlessToldOtherwise` | `verified` |
| AR-5-04 | Reset writes default, never changes mode | advanced-ranges §5 | reset path | `IntegrationTests.cpp` `Presets::resetRestoresDefaults` | `implemented` - mode invariance not asserted |
| AR-5-05 | MIDI Learn maps across live range | advanced-ranges §5 | `MidiLearn` | none | `implemented` |
| AR-6.1-01 | Arc past stock in warning colour; `*` suffix; tab-header padlock (accent unlocked / muted locked) | advanced-ranges §6.1 | `RangesUi::drawPadlock`; CHARACTER tab padlock | `RangesUiTests.cpp` `RangesUi::controlsFollowASwappedRangeAndMarkTheValue`; `NoiseTests.cpp` `NoiseUi::theCharacterTabCarriesAPadlockWhenUnlocked` | `verified` |
| AR-6.2-01 | Options RANGES: master toggle with clamp count, warning-colour pref, randomise pref, out-of-stock summary with per-row clamp + empty state | advanced-ranges §6.2 | `OptionsPages.cpp:1326-1432` | `RangesUiTests.cpp` `RangesUi::theRangesPageListsLocksAndClamps` | `verified` |
| AR-6.3-01 | Locked notice text at the control (not a banner); drag stops at stockMax | advanced-ranges §6.3 | `BubbleMessageComponent` (DECISIONS) | `RangesUiTests.cpp` `RangesUi::rightClickUnlocksAndRestrictsOneControl` | `implemented` - drag-stop and wording not asserted |
| AR-6.4-01 | First-unlock explainer once, on first transition | advanced-ranges §6.4 | onboarding explainer (TODO done 1) | none | `implemented` - see ONB rows; TODO 14c open |
| AR-7-01 | Undo class `ranges-toggle`, 200 ms grouping; lock undo restores clamped values | advanced-ranges §7 | processor undo stack | `RangesUiTests.cpp` `Undo::stepsOneActionAtATimeBothWays` | `partial` - 200 ms grouping not asserted |
| AR-8-01 | Telemetry boolean `advanced_ranges_used`; mirrored on Options -> Diagnostics | advanced-ranges §8 | not found (rg `advanced_ranges_used`) | none | `pending` |
| AR-9-01 | No audio-path cost; mode read on change only | advanced-ranges §9 | - | none | `implemented` |
| AR-10-01 | Test: invariants sweep | advanced-ranges §10 | - | `RangeTests.cpp` `Ranges::everyPhysicalRangeIsValid` | `verified` |
| AR-10-02 | Test: widening silent (plain within 1e-9, bit-identical block) | advanced-ranges §10 | - | `RangeTests.cpp` `Ranges::wideningPreservesEveryPlainValue` | `verified` |
| AR-10-03 | Test: narrowing clamps and reports 1 | advanced-ranges §10 | - | `RangeTests.cpp` `Ranges::narrowingClampsAndReportsTheCount` | `verified` |
| AR-10-04 | Test: undo restores a clamp | advanced-ranges §10 | - | `RangesUiTests.cpp` `Undo::stepsOneActionAtATimeBothWays` | `partial` - confirm it covers a lock clamp |
| AR-10-05 | Test: normalisation follows live range | advanced-ranges §10 | - | `RangeTests.cpp` `Ranges::normalisationFollowsTheLiveRange` | `verified` |
| AR-10-06 | Test: legacy load derives per family / ordinary stays stock | advanced-ranges §10 | - | `RangeTests.cpp` `Ranges::theRangesBlockRoundTripsAndDerivesWhenAbsent` | `verified` |
| AR-10-07 | Test: marking follows the value | advanced-ranges §10 | - | `RangeTests.cpp` `Ranges::markingFollowsTheValueNotTheMode` | `verified` |
| AR-10-08 | Test: randomise respects stock (1000 passes) | advanced-ranges §10 | - | `RangesUiTests.cpp` `RangesUi::randomiseStaysInStockUnlessToldOtherwise` | `verified` |
| AR-10-09 | Test: snapshots do not carry mode | advanced-ranges §10 | - | none | `pending` |

## 13. volume-knob-interaction.md (phase 2)

Built (TODO done item 2; `Source/DSP/Circuit/GuitarCircuit.*`,
`Source/UI/CircuitPanel.*`). Precedence 9 (pickup-to-amp path). DECISIONS
records: MNA solver instead of a biquad pair; recompute on audio thread
between blocks; sub-1k pot sections as wires; coil resonance moved from
PickupEngine; 50s wiring on the wiper; active tone corner; bypass measured
against bare coil; nF units; +11 params not +9; presence-based tests.

| ID | Requirement (short) | Source (file §section) | Implementation location | Verification | Status |
|---|---|---|---|---|---|
| VK-0-01 | Pickup, pots, cap, bleed, cable, amp input solved as one network | volume-knob §0.1, §1.1 | `GuitarCircuit.h:8-17` (nodal MNA, trapezoidal companions) | `CircuitTests.cpp` `Circuit::theAudioPathMatchesTheResponse` | `verified` - method deviates (Conflict C-08) |
| VK-0-02 | Passive by default; active toggle | volume-knob §0.2 | `circuit_active` default false | `CircuitTests.cpp` `Circuit::activeModeRemovesTheLoading` | `verified` |
| VK-0-03 | Component values in ohms/farads | volume-knob §0.3 | params in ohm; caps in nF (DECISIONS) | `CircuitTests.cpp` `Circuit::ohmsReadTheWayThePartsArePrinted` | `verified` |
| VK-0-04 | Honest magnitudes (a few dB at 5 kHz) | volume-knob §0.4 | - | `CircuitTests.cpp` `Circuit::turningDownDarkensAsWellAsQuietens` | `verified` |
| VK-0-05 | Coefficients recomputed off the audio path; audio thread evaluates fixed-size filter | volume-knob §0.5 | recomputed on audio thread between blocks, allocation-free (DECISIONS) | `CircuitTests.cpp` `Circuit::sweepingEveryControlDoesNotAllocate` | `verified` - thread choice deviates (Conflict C-08) |
| VK-1-01 | Component table sources: coil Ls/Rs/Cp from pickup part; pots/cap/bleed/cable/Rin from params | volume-knob §1 | `GuitarCircuit.h:45-64`; pickup part values via Workshop | `CircuitTests.cpp` `Circuit::potValueMovesTheResonance` | `verified` |
| VK-1.2-01 | Volume: level by taper, heavier load lowers/flattens peak, cable disconnect at low settings | volume-knob §1.2 | solver | `CircuitTests.cpp` `Circuit::turningDownDarkensAsWellAsQuietens` | `verified` |
| VK-1.3-01 | Treble bleed None / Kinman 130k ∥ 1.1n / Fender 1n / Custom R, C, series or parallel | volume-knob §1.3 | `Parameters::trebleBleedNames`, `bleedModeNames` `Parameters.cpp:352-353` | `CircuitTests.cpp` `Circuit::aKinmanBleedKeepsTheTop` | `verified` |
| VK-1.4-01 | Active: buffer after pickup; resonance independent of cable/amp; volume plain attenuator; active LP tone | volume-knob §1.4 | `GuitarCircuit` active path | `CircuitTests.cpp` `Circuit::activeModeRemovesTheLoading` | `verified` |
| VK-2-01 | Cable C = length x pF/m: Studio 52, Standard 98, Cheap 160, Vintage 220 | volume-knob §2 | `cableQualityNames` `Parameters.cpp:354`; `GuitarCircuit` | `CircuitTests.cpp` `Circuit::cableCapacitanceMovesTheResonance` | `verified` |
| VK-2-02 | `cable_on` off = zero-length studio cable, not silence | volume-knob §2 | `GuitarCircuit` | `CircuitTests.cpp` `Circuit::bypassIsNeutral` | `verified` |
| VK-3-01 | Params: guitar_volume/tone as wipers; circuit_volume_pot 500k; tone_pot 500k; tone_cap 22n; pot_taper; treble_bleed; bleed_r 130k; bleed_c 1.1n; bleed_mode; circuit_active; amp_input_impedance 1M; cable_on; cable_length 3.0; cable_quality Standard | volume-knob §3 | `Parameters.h:154-168` | `IntegrationTests.cpp` `Parameters::everyParameterHasAUniqueIdAndSaneDefault` | `implemented` - declared; specific defaults or the refusal path are not asserted by the cited test |
| VK-3-02 | Net +9 parameters | volume-knob §3 | +11 (DECISIONS: the table lists eleven) | `IntegrationTests.cpp` `Parameters::everyParameterHasAUniqueIdAndSaneDefault` | `verified` - count corrected by DECISIONS |
| VK-3.1-01 | 50s wiring changes the tone tap; keeps top as volume drops | volume-knob §3.1 | tone on wiper (DECISIONS "50s wiring") | `CircuitTests.cpp` `Circuit::turningDownDarkensAsWellAsQuietens` (confirm it covers 50s) | `implemented` |
| VK-4-01 | Pickup coil values from part (table by pickup type until Workshop) | volume-knob §4 | Workshop pickup parts now drive it | `PartAcousticsTests.cpp` `PartAcoustics::aCoverCostsTopEnd` | `implemented` |
| VK-4-02 | Feedback loop taken after the circuit; volume 5 reduces feedback by the measured attenuation within 0.5 dB | volume-knob §4, §6 | `FeedbackLoop` injects at the excitation point, so the circuit is inside the loop | `FeedbackTests.cpp` `Feedback::theVolumeKnobLowersTheLoopByTheCircuitsAttenuation` | `verified` |
| VK-4-03 | Chain position String -> Pickup -> GuitarCircuit -> Pre FX; linear, no oversampling | volume-knob §4 | `LuthierEngine` | `CircuitTests.cpp` `Circuit::theEngineRunsThroughTheCircuit` | `verified` |
| VK-4-04 | CableSim module removed | volume-knob intro; BRIEF precedence 9 | `GuitarCircuit.h:25`; no CableSim class | rg `class CableSim` finds nothing | `verified` |
| VK-5-01 | CIRCUIT panel replaces CABLE in Advanced column 2: volume/tone knobs, pot/cap/taper dropdowns, bleed with Custom R/C, active toggle, cable length/quality | volume-knob §5 | `Source/UI/CircuitPanel.cpp`; `StandardValueChoice` | none | `implemented` |
| VK-5-02 | Live circuit-response visualiser with resonant peak marked | volume-knob §5 | `CircuitResponseView` `CircuitPanel.h:28` | none | `implemented` |
| VK-5-03 | CHARACTER tab CIRCUIT group shows the visualiser full size | volume-knob §5; GI 4.4 | not found in `CharacterPanel.cpp` | none | `pending` |
| VK-5-04 | Easy compact card: volume, tone, visualiser miniature | volume-knob §5; GI 3 | not built (TODO 2e) | none | `pending` |
| VK-6-01 | Test: volume 1.0 vs 0.7, level within 0.5 dB of taper and -3 dB corner down >= 15% | volume-knob §6 | - | `CircuitTests.cpp` `Circuit::turningDownDarkensAsWellAsQuietens` (asserts presence, not the -3 dB corner; DECISIONS) | `verified` - reinterpreted by DECISIONS |
| VK-6-02 | Test: Kinman corner within 5% | volume-knob §6 | - | `CircuitTests.cpp` `Circuit::aKinmanBleedKeepsTheTop` | `verified` |
| VK-6-03 | Test: active mode constant gain within 0.1 dB | volume-knob §6 | - | `CircuitTests.cpp` `Circuit::activeModeRemovesTheLoading` | `verified` |
| VK-6-04 | Test: 250k vs 1M peak differs >= 10% (1M higher) | volume-knob §6 | - | `CircuitTests.cpp` `Circuit::potValueMovesTheResonance` | `verified` |
| VK-6-05 | Test: 1 m studio vs 10 m cheap peak drops >= 20% | volume-knob §6 | - | `CircuitTests.cpp` `Circuit::cableCapacitanceMovesTheResonance` | `verified` |
| VK-6-06 | Test: tone sweeps down monotonically, never above unity | volume-knob §6 | - | `CircuitTests.cpp` `Circuit::theToneControlSweepsDownAndNeverBoosts` | `verified` |
| VK-6-07 | Test: bypass flat within 0.1 dB | volume-knob §6 | - | `CircuitTests.cpp` `Circuit::bypassIsNeutral` (vs bare coil; DECISIONS) | `verified` |
| VK-6-08 | Test: stability across advanced range at 44.1/48/96/192 kHz | volume-knob §6 | - | `CircuitTests.cpp` `Circuit::everyCornerOfTheAdvancedRangeIsStable` | `verified` |
| VK-6-09 | Test: no allocation while sweeping | volume-knob §6 | - | `CircuitTests.cpp` `Circuit::sweepingEveryControlDoesNotAllocate` | `verified` |
| VK-6-10 | Test: feedback coupling within 0.5 dB | volume-knob §6 | - | `FeedbackTests.cpp` `Feedback::theVolumeKnobLowersTheLoopByTheCircuitsAttenuation` | `verified` |

## 14. pick-noise.md (phase 2)

Built (TODO done item 3; `Source/DSP/Noise/NoiseEngine.*`,
`PlayingNoise.*`, `Source/UI/NoiseGroups.*`). Open: Aux 8 (TODO 2f), scrape
trigger (TODO 3f). DECISIONS: `pick_material` keeps its 12-choice list
(no Ultex/Tortex/stone); thickness/angle stay 0-1 mapped to mm/deg; chirp at
pluck release; noise calibrated at the output.

| ID | Requirement (short) | Source (file §section) | Implementation location | Verification | Status |
|---|---|---|---|---|---|
| PN-0-01 | Noise generated, not sampled | pick-noise §0.1 | `NoiseEngine` synthesis | none | `implemented` |
| PN-0-02 | Noise is an event that runs to completion | pick-noise §0.2 | `NoiseGenerator::start` `NoiseEngine.h:109` | none | `implemented` |
| PN-0-03 | Noise rides the instrument path | pick-noise §0.3, §1.2 | injection points in `LuthierEngine` | none | `implemented` - two-body spectra test missing (PN-9-05) |
| PN-0-04 | Click 25-35 dB under the note at moderate velocity | pick-noise §0.4 | calibration constants (DECISIONS) | `NoiseTests.cpp` `PickNoise::aClickSitsAboutThirtyDecibelsUnderTheNote` | `verified` |
| PN-0-05 | Zero is silent and free | pick-noise §0.5 | - | `NoiseTests.cpp` `NoisePool::zeroIsFree` | `verified` |
| PN-1-01 | Pools squeak 16, click 16, chirp 16, scrape 8, buzz 16, clank 8; pre-allocated | pick-noise §1 | `kPoolSizes` `NoiseEngine.h:169` | `NoiseTests.cpp` `NoisePool::aFullPoolStealsTheOldest` | `verified` |
| PN-1-02 | Steal oldest on exhaustion | pick-noise §1 | `NoiseEngine` | `NoiseTests.cpp` `NoisePool::aFullPoolStealsTheOldest` | `verified` |
| PN-1-03 | Degradation halves pools | pick-noise §1 | `setDegraded` `NoiseEngine.h:186` | none | `implemented` - no degradation trigger yet (see PB rows) |
| PN-1-04 | Per-material texture tables synthesised once at load; random per-event offset | pick-noise §1 | `synthesiseTexture` `NoiseEngine.h:233`, `kTextureLength` | `NoiseTests.cpp` `NoisePool::aSeedRepeatsExactly` | `implemented` |
| PN-1.1-01 | Generator = excitation -> resonator (1-3 poles) -> envelope | pick-noise §1.1 | `NoiseEngine.h:8` | none | `implemented` |
| PN-1.2-01 | Injection: click at string excitation; others at string output pre-body | pick-noise §1.2 | `LuthierEngine` | none | `implemented` |
| PN-1.3-01 | Aux 8 noise bus (sum of generators, pre-body), 128-sample latency allowance, appended after per-string buses | pick-noise §1.3 | Aux 8 declared after the twelve per-string buses; buses classified by name (`a901c72`) | `PluginBusTests.cpp` `PluginBuses::aux8NoiseIsDeclaredLastSoNoBusNumberMoved`, `PluginBuses::aux8CarriesThePlayingNoiseAndObeysItsStrip` | `verified` |
| PN-2-01 | Pick fields material, thickness 0.38-3.0 mm (def 0.73), tip radius, bevel, wear, angle 0-60 (def 20), use_fingers | pick-noise §2 | `Parameters.h:73-176`; `Parameters::pickThicknessMm/pickAngleDegrees` | `NoiseTests.cpp` `PickNoise::clickPitchTracksMaterialAndThickness` | `verified` |
| PN-2.1-01 | Materials: Celluloid (default), Nylon, Delrin, Ultex, Tortex, Metal, Stone/horn, Wood with density and damping | pick-noise §2.1 | 12-choice list kept; no Ultex/Tortex/Stone (DECISIONS) | `NoiseTests.cpp` `PickNoise::clickPitchTracksMaterialAndThickness` | `partial` - three materials absent by decision; default material to check |
| PN-3-01 | Click level `amount x vel^0.7 x stiffness`; -30 dB at 0.5 / vel 100 | pick-noise §3 | `PlayingNoise.cpp` | `NoiseTests.cpp` `PickNoise::clickScalesWithVelocityToThePower0_7`, `PickNoise::aClickSitsAboutThirtyDecibelsUnderTheNote` | `verified` |
| PN-3-02 | Click fundamental from density and thickness; decay 3-15 ms; `cos(angle)^1.5`; wear second resonance; brighter near bridge | pick-noise §3 | `PlayingNoise.cpp` | `NoiseTests.cpp` `PickNoise::clickPitchTracksMaterialAndThickness`, `PickNoise::angleTradesClickForChirp` | `verified` |
| PN-4-01 | Chirp on wound strings only; level `amount x sin(angle) x roughness x windingDepth`; band at windingPitch x pickSpeed; 8-40 ms | pick-noise §4 | `PlayingNoise.cpp`; fires at pluck release (DECISIONS) | `NoiseTests.cpp` `PickNoise::plainStringsNeverChirp`, `PickNoise::angleTradesClickForChirp` | `verified` |
| PN-5-01 | Scrape triggered by `pick_scrape` MIDI event class or Easy rake gesture; 100 ms-3 s; sweeps across strings; 8-pool | pick-noise §5 | `LuthierEngine::triggerPickScrape` exists; no trigger path (TODO 3f) | none | `partial` |
| PN-6-01 | Fingers: click/chirp/scrape silent; nail/flesh crossfade; fingertip release noise ~12 dB under | pick-noise §6 | `PlayingNoise.cpp:194-206` | `NoiseTests.cpp` `PickNoise::fingersNeitherClickNorChirp` | `verified` |
| PN-7-01 | Params in `pick` family with stock/advanced pairs; +6 net | pick-noise §7 | `PhysicalRange.cpp:125-130`; `Parameters.h:171-176` | `RangeTests.cpp` `Ranges::everyPhysicalRangeIsValid` | `verified` |
| PN-7-02 | `pick_bevel`, `pick_wear` advanced ranges | pick-noise §7 | not in `PhysicalRange.cpp:125-130` (stock == advanced 0-1 per table) | none | `implemented` |
| PN-8-01 | CHARACTER tab PICK group: pick + strum striker dropdowns, material, thickness, tip, bevel, wear, angle, click, chirp, scrape amounts | pick-noise §8 | `Source/UI/NoiseGroups.cpp` | `NoiseTests.cpp` `NoiseUi::theCharacterTabCarriesAPadlockWhenUnlocked` | `partial` - strum striker dropdown waits on `strum-dynamics.md` |
| PN-8-02 | Illustration pick at true size and angle with 8 px drag handles (angle drag, thickness handles) | pick-noise §8; GI 21 | not built (TODO V; guitar-illustration 14) | none | `pending` |
| PN-9-01 | Test: click ~ velocity^0.7 within 1 dB, vel 20-127 | pick-noise §9 | - | `NoiseTests.cpp` `PickNoise::clickScalesWithVelocityToThePower0_7` | `verified` |
| PN-9-02 | Test: nylon 0.6 mm click >= octave above metal 3 mm | pick-noise §9 | - | `NoiseTests.cpp` `PickNoise::clickPitchTracksMaterialAndThickness` | `verified` |
| PN-9-03 | Test: angle 0 max click/no chirp; 45 chirp within 3 dB of max | pick-noise §9 | - | `NoiseTests.cpp` `PickNoise::angleTradesClickForChirp` | `verified` |
| PN-9-04 | Test: plain strings never chirp on every factory guitar | pick-noise §9 | - | `NoiseTests.cpp` `PickNoise::plainStringsNeverChirp` | `verified` |
| PN-9-05 | Test: click through two bodies differs; amount 0 bit-identical to disabled | pick-noise §9 | - | none found | `pending` |
| PN-9-06 | Test: all amounts 0, no pool allocation over 10 000 notes | pick-noise §9 | - | `NoiseTests.cpp` `NoisePool::zeroIsFree` | `verified` |
| PN-9-07 | Test: 20 clicks into 16-pool steal 4 oldest, no allocation | pick-noise §9 | - | `NoiseTests.cpp` `NoisePool::aFullPoolStealsTheOldest` | `verified` |
| PN-9-08 | Test: no audio-thread allocation for any noise event | pick-noise §9 | - | `NoiseTests.cpp` `NoisePool::aFullPoolStealsTheOldest` (pool only) | `partial` |
| PN-9-09 | Test: fixed seed byte-identical noise | pick-noise §9 | - | `NoiseTests.cpp` `NoisePool::aSeedRepeatsExactly` | `verified` |

## 15. string-squeak.md (phase 2)

Built (TODO done item 3). DECISIONS: finger squeak replaces the string's own
glide noise; CHARACTER winding selector edits `string_material` directly
"until the Workshop exists" (the Workshop now exists, so SQ-9-03 is open).

| ID | Requirement (short) | Source (file §section) | Implementation location | Verification | Status |
|---|---|---|---|---|---|
| SQ-0-01 | Wound strings only | string-squeak §0.1 | `StringNoiseInfo::fromSpec` `PlayingNoise.cpp:7` | `NoiseTests.cpp` `Squeak::flatwoundIsNearlySilentAndPlainIsSilent` | `verified` |
| SQ-0-02 | Squeak is a consequence; no user "squeak now" control | string-squeak §0.2 | - | none | `implemented` |
| SQ-0-03 | Level/pitch follow hand speed, not note velocity | string-squeak §0.3, §3 | `PlayingNoise` squeak event | `NoiseTests.cpp` `Squeak::pitchTracksSpeedAndWinding` | `verified` |
| SQ-0-04 | 20-30 dB under the note on clean acoustic | string-squeak §0.4 | calibration (DECISIONS) | `NoiseTests.cpp` `Squeak::aShiftSitsTwentyToThirtyDecibelsUnderTheNote` | `verified` |
| SQ-0-05 | Zero silent and free | string-squeak §0.5 | - | `NoiseTests.cpp` `Squeak::zeroIsFreeAndSlideModeSuppressesIt` | `verified` |
| SQ-1-01 | `f = speed x windingPitch`, glided over the shift | string-squeak §1, §3 | `PlayingNoise.cpp` | `NoiseTests.cpp` `Squeak::pitchTracksSpeedAndWinding` | `verified` |
| SQ-2-01 | Triggers: legato slide, tab-playback position change, voicer revoicing of a held note, MIDI slide events | string-squeak §2 | legato slide in engine | `NoiseTests.cpp` `Squeak::aLegatoSlideInTheEngineSqueaksAndABendDoesNot` | `partial` - tab-playback, revoicing and MIDI slide-class triggers not found |
| SQ-2-02 | Not triggered by new pluck, bend or vibrato | string-squeak §2 | - | `NoiseTests.cpp` `Squeak::aLegatoSlideInTheEngineSqueaksAndABendDoesNot` | `partial` - vibrato not asserted |
| SQ-2.1-01 | Minimum travel `squeak_min_travel` default 1.5 frets | string-squeak §2.1 | `Parameters.h:183` | `NoiseTests.cpp` `Squeak::theMinimumTravelIsRespected` | `verified` |
| SQ-3-01 | Level formula (amount x windingDepth x pressure^1.3 x roughness(moisture) x min(1, speed/300)) | string-squeak §3 | `PlayingNoise.cpp` | `NoiseTests.cpp` `Squeak::pitchTracksSpeedAndWinding` | `implemented` - formula terms not individually asserted |
| SQ-4-01 | Per-winding brightness/texture table (PB 0.75 ... flatwound 0.10, coated 0.40) | string-squeak §4 | `PlayingNoise.cpp` material table | `NoiseTests.cpp` `Squeak::flatwoundIsNearlySilentAndPlainIsSilent` | `verified` |
| SQ-5-01 | Injected at string output pre-body; summed to Aux 8 | string-squeak §5 | engine injection; summed to Aux 8 (`a901c72`) | `PluginBusTests.cpp` `PluginBuses::aux8CarriesThePlayingNoiseAndObeysItsStrip` | `verified` |
| SQ-6-01 | `squeak_probability` 0.65 roll deterministic per seed and note index | string-squeak §6 | `Parameters.h:180` | `NoiseTests.cpp` `Squeak::theProbabilityRollIsDeterministic` | `verified` |
| SQ-6-02 | Moisture (0.35) lowers probability and brightness | string-squeak §6 | `squeak_finger_moisture` | none | `implemented` |
| SQ-7-01 | Pressure (0.5) raises level, coarsens texture; bass default lower | string-squeak §7 | `squeak_finger_pressure`; bass default waits on `bass-techniques.md` | none | `partial` |
| SQ-8-01 | Style presets Silent, Studio, Natural, Folk, Exaggerated with table values | string-squeak §8 | `Parameters::squeakStyleNames` `Parameters.cpp:357` | `NoiseTests.cpp` `NoiseUi::squeakStylesApplyAndReadModified` | `verified` |
| SQ-8-02 | Ship default: Natural with amount 0.25, reads "Natural (modified)" | string-squeak §8; onboarding 1 | defaults | `NoiseTests.cpp` `NoiseUi::squeakStylesApplyAndReadModified` | `verified` |
| SQ-9-01 | Six params in `squeak` family with stock/advanced pairs | string-squeak §9 | `Parameters.h:179-184`; `PhysicalRange.cpp:133-134` | `RangeTests.cpp` `Ranges::everyPhysicalRangeIsValid` | `verified` |
| SQ-9-02 | STRING NOISE group on CHARACTER: amount, probability, moisture, pressure, material selector, style, event strip | string-squeak §9 | `Source/UI/NoiseGroups.cpp` | `NoiseTests.cpp` `NoiseUi::squeakStylesApplyAndReadModified` | `verified` |
| SQ-9-03 | Winding selector mirrors the Workshop string part and edits it once the Workshop exists | string-squeak §9.1 | edits `string_material` directly (DECISIONS) | none | `partial` - should now edit the Workshop string part |
| SQ-9.1-01 | Noise-event strip: 24 px, last 8 s, ticks coloured by class, height by level; 30 Hz drain; grey after 2 s | string-squeak §9.1 | `NoiseEventStrip` `NoiseGroups.cpp:24-104` | `NoiseTests.cpp` `NoiseUi::theEventStripShowsWhatTheEngineTriggered` | `verified` |
| SQ-10-01 | Buzz and squeak both audible, no ducking | string-squeak §10 | separate pools | none | `implemented` |
| SQ-10-02 | Slide Mode replaces finger squeak | string-squeak §10 | `SlideEngine` | `NoiseTests.cpp` `Squeak::zeroIsFreeAndSlideModeSuppressesIt`; `SlideTests.cpp` `Slide::squeakStopsUnderTheBarButNotBesideIt` | `verified` |
| SQ-10-03 | Fret wear raises squeak slightly | string-squeak §10 | not found | none | `pending` |
| SQ-10-04 | Old strings: roughness up to x1.4 | string-squeak §10 | `PlayingNoise.cpp:24` | none | `implemented` |
| SQ-11-01 | MIDI `squeak` event class (string, start, end, duration, level); exact in Luthier profile, dropped in Generic | string-squeak §11 | SQUEAK fields `str`, `start`, `end`, `dur`, `intensity` (the level) with midi-export's `trigger` and `material` (`LuthierMidiEvents.cpp`); Generic leaves it out unless realism text is turned on (C-50); the engine does not report squeaks yet | `MidiExportTests.cpp` `MidiExport::everyEventClassRoundTripsWithEveryField` (not yet run) | `partial` - no engine capture |
| SQ-12-01 | 16-generator pool; degradation to 8 | string-squeak §12 | `kPoolSizes` | `NoiseTests.cpp` `NoisePool::aFullPoolStealsTheOldest` | `verified` |
| SQ-13-01 | Test: plain strings never squeak on every factory guitar | string-squeak §13 | - | `NoiseTests.cpp` `Squeak::flatwoundIsNearlySilentAndPlainIsSilent` (confirm factory sweep) | `verified` |
| SQ-13-02 | Test: 300 mm/s on 6.5 wraps/mm within 10% of 1.95 kHz; double speed doubles | string-squeak §13 | - | `NoiseTests.cpp` `Squeak::pitchTracksSpeedAndWinding` | `verified` |
| SQ-13-03 | Test: glide >= 15% start vs middle | string-squeak §13 | - | none found | `pending` |
| SQ-13-04 | Test: flatwound >= 15 dB quieter than phosphor bronze | string-squeak §13 | - | `NoiseTests.cpp` `Squeak::flatwoundIsNearlySilentAndPlainIsSilent` | `verified` |
| SQ-13-05 | Test: 1-fret no squeak, 2-fret squeak at min 1.5 | string-squeak §13 | - | `NoiseTests.cpp` `Squeak::theMinimumTravelIsRespected` | `verified` |
| SQ-13-06 | Test: probability deterministic by seed | string-squeak §13 | - | `NoiseTests.cpp` `Squeak::theProbabilityRollIsDeterministic` | `verified` |
| SQ-13-07 | Test: bend and 6 Hz vibrato produce no squeak | string-squeak §13 | - | `NoiseTests.cpp` `Squeak::aLegatoSlideInTheEngineSqueaksAndABendDoesNot` (bend only) | `partial` |
| SQ-13-08 | Test: Slide Mode suppresses squeak | string-squeak §13 | - | `NoiseTests.cpp` `Squeak::zeroIsFreeAndSlideModeSuppressesIt` | `verified` |
| SQ-13-09 | Test: amount 0 allocates nothing over 10 000 shifts | string-squeak §13 | - | `NoiseTests.cpp` `Squeak::zeroIsFreeAndSlideModeSuppressesIt` | `verified` |
| SQ-13-10 | Test: no audio-thread allocation | string-squeak §13 | - | none dedicated | `partial` |

## 16. fret-buzz.md (phase 2)

Built (TODO done item 4; `Source/DSP/Noise/FretBuzz.*`,
`Source/UI/SetupGroup.*`). DECISIONS: +13 params not +16; `fret_action`
superseded but kept for automation; 2.4 mm per unit calibration; default
guitars ship Player-friendly; relief test uses a high nut. Open: fret-wear
interaction (8) waits on per-fret wear height.

| ID | Requirement (short) | Source (file §section) | Implementation location | Verification | Status |
|---|---|---|---|---|---|
| FB-0-01 | Buzz sensed from amplitude vs clearance, never scheduled | fret-buzz §0.1, §3.1 | `Source/DSP/Noise/FretBuzz.cpp` | `BuzzTests.cpp` `Buzz::lowActionBuzzesAndHighActionDoesNot` | `verified` |
| FB-0-02 | Geometry in real units (mm) | fret-buzz §0.2, §1 | `setup_*` params `Parameters.h:187-193` | `BuzzTests.cpp` `Buzz::reliefMovesWhereItBuzzes` | `verified` |
| FB-0-03 | Light buzz 30-40 dB below the note, attack only | fret-buzz §0.4 | calibration (DECISIONS) | `BuzzTests.cpp` `Buzz::buzzStopsAsTheNoteDecays` | `implemented` - level ratio not asserted |
| FB-1-01 | Fields: action treble 1.6, bass 2.0, relief 0.20, nut depth x6, fret height 1.0, threshold 0.35, sitar mode | fret-buzz §1 | `Parameters.h:187-207` | `IntegrationTests.cpp` `Parameters::everyParameterHasAUniqueIdAndSaneDefault` | `implemented` - declared; specific defaults or the refusal path are not asserted by the cited test |
| FB-1-02 | Clearance interpolated nut -> 12th + parabolic relief peaking at 7; only frets between finger and bridge | fret-buzz §1 | `FretBuzz.cpp` | `BuzzTests.cpp` `Buzz::onlyFretsAheadOfTheFingerBuzz` | `verified` |
| FB-2-01 | Modal amplitude at fret positions, block rate | fret-buzz §2 | `FretBuzz.cpp:103` | none | `implemented` |
| FB-3-01 | Excess = amp - clearance drives level; fret drives spectrum | fret-buzz §3.1 | `FretBuzz.cpp` | `BuzzTests.cpp` `Buzz::fretHeightChangesLevelNotPosition` | `verified` |
| FB-3.2-01 | Threshold trim ±0.15 mm; trim not mute | fret-buzz §3.2 | `setup_buzz_threshold` | `BuzzTests.cpp` `Buzz::theThresholdIsATrimNotAMute` | `verified` |
| FB-4-01 | Generator: bursts at string fundamental; metallic 3-6 kHz rising with fret; fret material brightness; level min(1, excess/0.3) x height; 0.5 ms attack | fret-buzz §4 | `NoiseEngine` FretBuzz class | `BuzzTests.cpp` `Buzz::fretHeightChangesLevelNotPosition` | `implemented` - fret-material brightness not asserted |
| FB-4-02 | Injection pre-body; Aux 8 | fret-buzz §4 | pre-body; summed to Aux 8 (`a901c72`) | `PluginBusTests.cpp` `PluginBuses::aux8CarriesThePlayingNoiseAndObeysItsStrip` | `verified` |
| FB-5-01 | Sitar mode: jawari grazing contact, continuous pitched buzz, threshold bypassed | fret-buzz §5 | `setup_sitar_mode` | `BuzzTests.cpp` `Buzz::sitarModeIsContinuous` | `verified` |
| FB-6-01 | SETUP group on CHARACTER: actions, relief, nut depths, fret height, threshold, sitar, heatmap | fret-buzz §6 | `Source/UI/SetupGroup.cpp` | `BuzzTests.cpp` `BuzzUi::setupStylesApplyAsOneStepAndReadModified` | `verified` |
| FB-6.1-01 | Setup styles Factory low, Player-friendly (default), Clean/high, Slide, Blues, Needs a tech with table values | fret-buzz §6.1 | `FretBuzz.cpp:71`; `setupStyleNames` | `BuzzTests.cpp` `Buzz::playerFriendlyBuzzesOnlyWhenAttackedHard`, `BuzzUi::setupStylesApplyAsOneStepAndReadModified` | `verified` |
| FB-6.2-01 | Heatmap: warning near-threshold, accent buzzing, dot glyph for monochrome; 30 Hz; grey after 2 s | fret-buzz §6.2 | `SetupGroup.cpp` | `BuzzTests.cpp` `BuzzUi::heatmapCellsReadInMonochromeTerms`, `Buzz::theHeatmapAgreesWithTheGenerator` | `verified` |
| FB-7-01 | Params in `buzz` family with stock/advanced pairs | fret-buzz §7 | `PhysicalRange.cpp:137-146` | `RangeTests.cpp` `Ranges::everyPhysicalRangeIsValid` | `verified` |
| FB-7-02 | Net +16 params | fret-buzz §7 | +13 (DECISIONS) | `IntegrationTests.cpp` `Parameters::everyParameterHasAUniqueIdAndSaneDefault` | `verified` - count corrected |
| FB-8-01 | Fret wear moves buzz (lower worn fret, neighbours buzz more) | fret-buzz §8 | waits on per-fret wear height from CharacterEngine (TODO step 4 note) | none | `pending` |
| FB-8-02 | Slide Mode suggests the Slide setup | fret-buzz §8 | low-action warning with "Use Slide setup" button (DECISIONS) | `SlideTests.cpp` `SlideUi::theSlideGroupAppearsWithSlideModeAndTheTabFitsIt` | `implemented` |
| FB-8-03 | Bass: slap/pop drive into frets; bass defaults lower action | fret-buzz §8 | waits on `bass-techniques.md` | none | `pending` |
| FB-8-04 | Bends reduce buzz at fretted position, raise it further up | fret-buzz §8 | not found | none | `pending` |
| FB-9-01 | Test: Needs-a-tech buzzes at vel 100; Clean/high none at 127 | fret-buzz §9 | - | `BuzzTests.cpp` `Buzz::lowActionBuzzesAndHighActionDoesNot` | `verified` |
| FB-9-02 | Test: buzz in first 300 ms, silent by 2 s | fret-buzz §9 | - | `BuzzTests.cpp` `Buzz::buzzStopsAsTheNoteDecays` | `verified` |
| FB-9-03 | Test: fret at 7, frets 1-6 never buzz | fret-buzz §9 | - | `BuzzTests.cpp` `Buzz::onlyFretsAheadOfTheFingerBuzz` | `verified` |
| FB-9-04 | Test: relief 0.0 vs 0.35 moves max-excess fret >= 3 | fret-buzz §9 | - | `BuzzTests.cpp` `Buzz::reliefMovesWhereItBuzzes` | `verified` |
| FB-9-05 | Test: doubling fret height +3 dB, same fret | fret-buzz §9 | - | `BuzzTests.cpp` `Buzz::fretHeightChangesLevelNotPosition` | `verified` |
| FB-9-06 | Test: threshold 0 still buzzes | fret-buzz §9 | - | `BuzzTests.cpp` `Buzz::theThresholdIsATrimNotAMute` | `verified` |
| FB-9-07 | Test: sitar continuous over 4 s | fret-buzz §9 | - | `BuzzTests.cpp` `Buzz::sitarModeIsContinuous` | `verified` |
| FB-9-08 | Test: heatmap matches generator over 30 s | fret-buzz §9 | - | `BuzzTests.cpp` `Buzz::theHeatmapAgreesWithTheGenerator` | `verified` |
| FB-9-09 | Test: buzz test within 0.2-unit budget at 6 voices | fret-buzz §9 | - | none | `pending` |
| FB-9-10 | Test: no allocation on audio thread | fret-buzz §9 | - | none dedicated | `pending` |

## 17. slide-guitar.md (phase 2)

Built (TODO done item 5; `Source/DSP/Slide/SlideEngine.*`,
`Source/UI/SlideGroup.*`). DECISIONS: `slide_guitar` re-pointed as
`slide_enabled` (+7 not +8); hybrid plays one string under the bar at a
time; vibrato depth in tenths of a mm; bar moves linearly; SLIDE group also
carries mode, damping and assist. User-facing slide controls are extended by
`slide-technique-controls.md` (INDEX 43, TODO 5b).

| ID | Requirement (short) | Source (file §section) | Implementation location | Verification | Status |
|---|---|---|---|---|---|
| SG-0-01 | Pitch continuous in Slide Mode | slide-guitar §0.1 | `SlideEngine` | `SlideTests.cpp` `Slide::pitchIsContinuous` | `verified` |
| SG-0-02 | Bar damps the segment behind | slide-guitar §0.2, §3 | `slide_damping_behind` | `SlideTests.cpp` `Slide::theSegmentBehindIsDamped` | `verified` |
| SG-0-03 | Bar is a Workshop part (material, mass, length, diameter) | slide-guitar §0.3, §2 | `PartCategory::slide` `Part.h:27`; mirror `SlideGroup.cpp:129` | none | `implemented` |
| SG-0-04 | Hybrid is the default for electric | slide-guitar §0.4 | `slide_mode` default | `IntegrationTests.cpp` `Parameters::everyParameterHasAUniqueIdAndSaneDefault` | `implemented` |
| SG-0-05 | Setup matters; plugin says so | slide-guitar §0.5, §6 | low-action warning `SlideGroup.cpp:17` | `SlideTests.cpp` `SlideUi::theSlideGroupAppearsWithSlideModeAndTheTabFitsIt` | `verified` |
| SG-1-01 | Modes bottleneck, lap steel, dobro, hybrid; hybrid routes non-bar notes through fretted path | slide-guitar §1 | `Parameters::slideModeNames` `Parameters.cpp:372`; hybrid rule (DECISIONS) | `SlideTests.cpp` `Slide::squeakStopsUnderTheBarButNotBesideIt` | `verified` |
| SG-2-01 | Bar fields: material (Glass default), mass 20-220 g (65), length 40-100 (70), diameter 15-30 (22) | slide-guitar §2 | slide part schema; factory slide parts | none | `implemented` |
| SG-2.1-01 | Seven bar materials with damping/brightness/friction | slide-guitar §2.1 | `getSlideMaterial` | `SlideTests.cpp` `Slide::theBarClanksWhenItLands` (brass vs glass) | `implemented` |
| SG-3-01 | Sounding length bridge-to-contact; `f = f_open L/(L-x)` | slide-guitar §3 | `SlideEngine` | `SlideTests.cpp` `Slide::pitchIsContinuous` | `verified` |
| SG-3-02 | Damping behind 1.0 lap/dobro, 0.55 bottleneck/hybrid | slide-guitar §3 | `Parameters::defaultDampingBehind` `Parameters.cpp:376` | `SlideTests.cpp` `Slide::theSegmentBehindIsDamped` | `verified` |
| SG-3-03 | Contact loss by material damping and mass | slide-guitar §3 | `SlideEngine` | `SlideTests.cpp` `Slide::aHeavierBarSustainsLonger` | `verified` |
| SG-3-04 | Pressure: light rattles against bar, heavy chokes on frets | slide-guitar §3 | `slide_pressure` | `SlideTests.cpp` `SlideUi::pressureSaysWhatItMeans` | `implemented` |
| SG-3.1-01 | Slant -30..+30, per-string contact offset `tan(slant) x spacing x (s - centre)` | slide-guitar §3.1 | `slide_slant` | `SlideTests.cpp` `Slide::slantGivesEachStringItsOwnInterval` | `verified` |
| SG-3.2-01 | Vibrato moves the bar (pitch and damped length); depth in mm | slide-guitar §3.2 | `vibrato_depth` as tenths of mm (DECISIONS) | none | `implemented` |
| SG-4-01 | Intonation assist 0-1 (0.15), 120 ms pull to ET; marked as an aid | slide-guitar §4 | `SlideEngine.h:110`; "(aid)" label | `SlideTests.cpp` `Slide::theAssistPullsToPitch` | `verified` |
| SG-5.1-01 | Friction noise `amount x friction x barSpeed`; replaces squeak on contacted strings | slide-guitar §5.1 | `noise_slide` drives bottleneck friction (DECISIONS) | `SlideTests.cpp` `Slide::squeakStopsUnderTheBarButNotBesideIt` | `verified` |
| SG-5.2-01 | Clank pool 8; on landing and low-pressure rattle; brass ~1.2 kHz, glass ~2.5 kHz, mass lowers | slide-guitar §5.2 | `NoiseEngine` clank class | `SlideTests.cpp` `Slide::theBarClanksWhenItLands` | `verified` |
| SG-6-01 | Warning below 2.2 mm bass action with GI 14 text; setup not changed | slide-guitar §6 | `SlideGroup.cpp:17`; "Use Slide setup" button (DECISIONS) | `SlideTests.cpp` `SlideUi::theSlideGroupAppearsWithSlideModeAndTheTabFitsIt` | `verified` |
| SG-7-01 | Params `slide_enabled`, mode, pressure 0.55, slant, damping, noise 0.4, clank 0.45, assist 0.15 in `slide` family | slide-guitar §7 | `Parameters.h:197-203`; `PhysicalRange.cpp:149` | `RangeTests.cpp` `Ranges::everyPhysicalRangeIsValid` | `implemented` - declared; specific defaults or the refusal path are not asserted by the cited test |
| SG-7-02 | Net +8 params | slide-guitar §7 | +7 (DECISIONS) | `IntegrationTests.cpp` `Parameters::everyParameterHasAUniqueIdAndSaneDefault` | `verified` - count corrected |
| SG-7-03 | SLIDE group on CHARACTER only in Slide Mode: pressure, slant, material/mass mirror, noise, clank | slide-guitar §7 | `Source/UI/SlideGroup.cpp` | `SlideTests.cpp` `SlideUi::theSlideGroupAppearsWithSlideModeAndTheTabFitsIt` | `verified` |
| SG-7-04 | Header Slide toggle, shortcut `S` | slide-guitar §7 | header toggle + `S` (TODO done 5) | none | `implemented` |
| SG-7-05 | Workshop Slide category enabled in Slide Mode | slide-guitar §7 | WORKSHOP tab not built | none | `pending` |
| SG-7-06 | Fretboard bar overlay: 6 px rounded, material colour at 80%, 80 ms ease, at position and slant | slide-guitar §7 | `FretboardComponent.cpp:168-183` | none | `implemented` - slant drawing not verified |
| SG-7-07 | Tuning popover shows continuous pitch | slide-guitar §7 | not found | none | `pending` |
| SG-8-01 | MIDI `slide` class: bar position over time, slant, pressure; Luthier round-trips, Generic as pitch bend | slide-guitar §8 | SLIDE_BAR fields `pos`, `path` (ms:frets over time), `pressure`, `slant`, `material` (`LuthierMidiEvents.cpp`); Generic carries the performance's own pitch bend; the engine does not report bar events yet | `MidiExportTests.cpp` `MidiExport::everyEventClassRoundTripsWithEveryField` (not yet run) | `partial` - no engine capture |
| SG-9-01 | Test: bar sweep fret 3->5 monotonic, unquantised at assist 0 | slide-guitar §9 | - | `SlideTests.cpp` `Slide::pitchIsContinuous` | `verified` |
| SG-9-02 | Test: assist 1.0 settles 40 c sharp to within 5 c in 400 ms | slide-guitar §9 | - | `SlideTests.cpp` `Slide::theAssistPullsToPitch` | `verified` |
| SG-9-03 | Test: lap-steel slide T60 >= 25% shorter than fretted | slide-guitar §9 | - | `SlideTests.cpp` `Slide::theSegmentBehindIsDamped` | `verified` |
| SG-9-04 | Test: 20 deg slant shifts 1-6 interval >= 40 c | slide-guitar §9 | - | `SlideTests.cpp` `Slide::slantGivesEachStringItsOwnInterval` | `verified` |
| SG-9-05 | Test: 200 g bar sustains >= 15% longer than 30 g | slide-guitar §9 | - | `SlideTests.cpp` `Slide::aHeavierBarSustainsLonger` | `verified` |
| SG-9-06 | Test: clank within 5 ms; brass centroid below glass | slide-guitar §9 | - | `SlideTests.cpp` `Slide::theBarClanksWhenItLands` | `verified` |
| SG-9-07 | Test: squeak suppressed under bar, fires beside it in hybrid | slide-guitar §9 | - | `SlideTests.cpp` `Slide::squeakStopsUnderTheBarButNotBesideIt` | `verified` |
| SG-9-08 | Test: low-action warning fires; setup untouched | slide-guitar §9 | - | `SlideTests.cpp` `SlideUi::theSlideGroupAppearsWithSlideModeAndTheTabFitsIt` | `verified` |
| SG-9-09 | Test: mode toggle mid-note, no discontinuity above -60 dBFS | slide-guitar §9 | - | `SlideTests.cpp` `Slide::switchingModeMidNoteIsClean` | `verified` |
| SG-9-10 | Test: no allocation on audio thread | slide-guitar §9 | - | none dedicated | `pending` |

## 18. guitar-workshop.md (phase 2)

TODO step 6 done at `819be5b` (415 tests green, partial capo included);
`d8893b5` then fixed the capo mask to the engine's string order. Precedence
8 (part data model). `Source/Model/Workshop/*`;
`Resources/Parts` (148 parts), `Resources/Guitars` (27 guitars) generated by
`Tools/generate_factory_parts.py`. DECISIONS: `WorkshopGuitar` is this spec's
`GuitarSpec`; 27 factory guitars; part names avoid `:` `/`; several part
fields not yet consumed; swap parks the audio thread behind a 5 ms fade
(Conflict C-09).

| ID | Requirement (short) | Source (file §section) | Implementation location | Verification | Status |
|---|---|---|---|---|---|
| GW-0-01 | A guitar is its parts, setup, finish | guitar-workshop §0.1 | `WorkshopGuitar` (`PluginProcessor.h:194`) | `WorkshopTests.cpp` `Workshop::everyFactoryGuitarLoadsAndRoundTrips` | `verified` |
| GW-0-02 | Parts are files; factory read-only, user in `~/Documents/Luthier/Parts/` | guitar-workshop §0.2, §4 | `PartLibrary.h:5` | `WorkshopTests.cpp` `Workshop::theFactoryLibraryIsThere`, `Workshop::aUserPartBeatsTheFactoryOne` | `verified` |
| GW-0-03 | Every part swap is audible (or field does not belong) | guitar-workshop §0.3 | several fields unconsumed (tuners, nut friction/width, fretboard radius/thickness, coil turns, pole shape, spring count, plies; DECISIONS) | `PartAcousticsTests.cpp` `PartAcoustics::everyMappedFieldMovesSomething` (mapped fields only) | `partial` |
| GW-0-04 | Compatibility advisory: warn, never refuse | guitar-workshop §0.4, §5 | `getCompatibilityWarnings` `PartLibrary.h:93` | `WorkshopTests.cpp` `Workshop::incompatiblePartsFitWithAWarning` | `verified` |
| GW-0-05 | Committed spec owned by audio thread; edits as commands, atomic swap | guitar-workshop §0.5, §3 | rebuild on message thread with audio parked; MIDI arriving while parked is queued (DECISIONS C-09); IR reloads skipped when unchanged (~14 ms park) | `WorkshopPresetTests.cpp` `WorkshopSwap::aPartSwapDuringANoteIsClickFree`, `WorkshopSwap::aNotePlayedWhileParkedIsKeptNotDropped` | `partial` - part swaps still park until the bench's off-thread swap (C-09) |
| GW-0-06 | Factory guitars ship as `.luthierguitar`; enum is a browser shortcut | guitar-workshop §0.6 | `PluginProcessor::getFactoryGuitarPath` (27 files) | `WorkshopPresetTests.cpp` `WorkshopPresets::choosingAGuitarTypeFitsItsParts` | `verified` |
| GW-1-01 | Slots body, top, neck, fretboard, frets, nut, bridge, tailpiece, tuners, pickups x3, wiring, strings, pickguard with cardinalities | guitar-workshop §1 | `GuitarSlot`, `PartType` `Part.h:22-28` | `WorkshopTests.cpp` `Workshop::everyFactoryGuitarLoadsAndRoundTrips` | `verified` |
| GW-1-02 | Guitar fields hardware_color, finish, setup, character_seed; zero pickups legal | guitar-workshop §1 | `WorkshopGuitar` | `WorkshopTests.cpp` `Workshop::anEmbeddedGuitarNeedsNoPartFiles` | `implemented` |
| GW-2-01 | Part field sets per `part_type` (16 types incl. slide, pick, capo) | guitar-workshop §2 | `Source/Model/Workshop/Part.cpp` | `WorkshopTests.cpp` `Workshop::theFactoryLibraryIsThere` | `verified` |
| GW-3-01 | GuitarSpec in memory: meta, resolved part pointers, placements, strings, finish, hardware, setup, seed, derived | guitar-workshop §3 | `WorkshopGuitar`; `PartPtr` | `WorkshopTests.cpp` `Workshop::everyFactoryGuitarLoadsAndRoundTrips` | `verified` |
| GW-3-02 | Parts resolved on message thread; audio thread never touches files | guitar-workshop §3 | `PartLibrary` load report | `WorkshopPresetTests.cpp` `WorkshopSwap::noFileIsTouchedFromTheAudioThreadDuringASwap` | `verified` |
| GW-3.1-01 | Existing compiled GuitarSpec kept as fallback (widening, not replacement) | guitar-workshop §3.1 | `mapSpec` re-points compiled spec (DECISIONS) | `WorkshopPresetTests.cpp` `WorkshopPresets::aMissingGuitarFileFallsBackToItsType` | `verified` |
| GW-3.2-01 | DerivedAcoustics computed once per swap; 5 ms crossfade | guitar-workshop §3.2 | `PartAcoustics::mapSpec`; `LuthierEngine::applyWorkshopGuitar`; 5 ms fade out/in (`LuthierEngine.h:80`) | `WorkshopPresetTests.cpp` `WorkshopSwap::aSwapMapsOnceNotPerBlock`, `WorkshopSwap::aPartSwapDuringANoteIsClickFree` | `verified` |
| GW-4-01 | Factory `Resources/Parts/<Category>/`, `Resources/Guitars/<Family>/`; user Parts and Guitars folders | guitar-workshop §4 | on disk; `PartLibrary` | `WorkshopTests.cpp` `Workshop::theFactoryLibraryIsThere` | `verified` |
| GW-4-02 | Scanned at startup and on folder change; index by type, compatibility, tags | guitar-workshop §4 | `PartLibrary::rescan` `PartLibrary.h:113`; no folder watcher found | none | `partial` - rescan on folder change not found |
| GW-4-03 | User part with same name wins | guitar-workshop §4 | `PartLibrary` | `WorkshopTests.cpp` `Workshop::aUserPartBeatsTheFactoryOne` | `verified` |
| GW-4.1-01 | Missing part -> category default, banner with jump-to-Workshop, error log | guitar-workshop §4.1 | `PartLibrary.cpp:483`; `PluginEditor.cpp:908-911` "missing-part" banner | `WorkshopTests.cpp` `Workshop::aMissingPartFallsBackAndSaysSo` | `partial` - the WORKSHOP tab now exists (`d45fcd6`); jump action and error-log entry not asserted |
| GW-5.1-01 | String count = min(neck, bridge); excess reported | guitar-workshop §5.1 | `PartLibrary` | `WorkshopTests.cpp` `Workshop::aStringCountMismatchClamps` | `verified` |
| GW-6-01 | Save As Guitar (`Ctrl+G`) to `Guitars/<name>.luthierguitar` by reference; preset reference updated | guitar-workshop §6 | `PluginProcessor::saveGuitarAs` `PluginProcessor.h:217`; dialog `PluginEditor.cpp:197` | `WorkshopPresetTests.cpp` `WorkshopPresets::saveAsGuitarWritesAFileAndPointsThePresetAtIt` | `verified` |
| GW-6-02 | "Bundle parts" export option | guitar-workshop §6 | "Save with parts" button (DECISIONS) | none | `implemented` |
| GW-7-01 | Save As Part from inspector; factory parts not editable, edits marked unsaved | guitar-workshop §7 | `savePartAs`; inspector "Save as user part"; editing a factory field fits a user copy (`WorkshopPanel::editInspectorField`) | `WorkshopPresetTests.cpp` `WorkshopPresets::saveAsPartMakesAUserPartAndFitsIt`; `WorkshopPanelTests.cpp` `WorkshopPanel::editingAFieldMakesAUserCopy` | `verified` |
| GW-8-01 | Preset `guitar.reference` + `guitar.override`; override wins | guitar-workshop §8 | `getGuitarBlock` `PluginProcessor.h:226` | `WorkshopPresetTests.cpp` `WorkshopPresets::anEditedGuitarTravelsWholeInTheState` | `verified` |
| GW-9-01 | Workshop adds no parameters; part fields structural | guitar-workshop §9 | - | `IntegrationTests.cpp` `Parameters::everyParameterHasAUniqueIdAndSaneDefault` | `verified` |
| GW-9-02 | Retire pickup position/height params (-9) with migration into placements | guitar-workshop §9 | retired in `b9a9b5d` (PROGRESS) | `WorkshopPresetTests.cpp` `WorkshopPresets::oldPickupPlacementParametersBecomeTheGuitars` | `verified` |
| GW-CAPO-01 | Capo part (full/partial, string_mask, pressure); partial capo clamps only masked strings; travels with preset | guitar-workshop §2; AMB 4.5 | `TuningEngine` capo mask, `setCapoPart` `PluginProcessor.h:232`; `Resources/Parts/Capos/Partial 3-String Capo.luthierpart` | `WorkshopPresetTests.cpp` `WorkshopCapo::aPartialCapoClampsOnlyItsStrings`, `WorkshopCapo::theCapoTravelsWithThePreset` - green at `0d225f0` (448 tests) | `verified` |
| GW-10-01 | Test: every factory guitar round-trips | guitar-workshop §10 | - | `WorkshopTests.cpp` `Workshop::everyFactoryGuitarLoadsAndRoundTrips` | `verified` |
| GW-10-02 | Test: each slot swap changes spectrum; swap back within -80 dBFS | guitar-workshop §10 | - | none found per slot | `pending` |
| GW-10-03 | Test: swap during a note, no discontinuity above -60 dBFS | guitar-workshop §10 | - | `WorkshopPresetTests.cpp` `WorkshopSwap::aPartSwapDuringANoteIsClickFree` | `verified` |
| GW-10-04 | Test: missing bridge -> default, loads, notification, error log | guitar-workshop §10 | - | `WorkshopTests.cpp` `Workshop::aMissingPartFallsBackAndSaysSo` | `partial` - notification/error log parts not confirmed |
| GW-10-05 | Test: user part beats factory | guitar-workshop §10 | - | `WorkshopTests.cpp` `Workshop::aUserPartBeatsTheFactoryOne` | `verified` |
| GW-10-06 | Test: incompatible parts fit with warning | guitar-workshop §10 | - | `WorkshopTests.cpp` `Workshop::incompatiblePartsFitWithAWarning` | `verified` |
| GW-10-07 | Test: 7-string neck + 6-saddle bridge clamps and reports | guitar-workshop §10 | - | `WorkshopTests.cpp` `Workshop::aStringCountMismatchClamps` | `verified` |
| GW-10-08 | Test: no filesystem access on audio thread during swap | guitar-workshop §10 | `Support/ThreadProbe.h` | `WorkshopPresetTests.cpp` `WorkshopSwap::noFileIsTouchedFromTheAudioThreadDuringASwap` | `verified` |
| GW-10-09 | Test: mapping runs once, not per block | guitar-workshop §10 | - | `WorkshopPresetTests.cpp` `WorkshopSwap::aSwapMapsOnceNotPerBlock` | `verified` |
| GW-10-10 | Test: preset override loads with no part files | guitar-workshop §10 | - | `WorkshopTests.cpp` `Workshop::anEmbeddedGuitarNeedsNoPartFiles` | `verified` |

## 19. part-acoustics.md (phase 2)

Built (TODO 6c; `Source/Model/Workshop/PartAcoustics.*`). Precedence 6
(part-to-engine mappings). DECISIONS: termination brightness normalised to
nickel-silver + bone; magnet pull formula; unconsumed fields listed.

| ID | Requirement (short) | Source (file §section) | Implementation location | Verification | Status |
|---|---|---|---|---|---|
| PA-0-01 | One function `mapSpec` evaluated once per swap; audio path never reads raw fields | part-acoustics §0.1 | `PartAcoustics::mapSpec` | `WorkshopPresetTests.cpp` `WorkshopSwap::aSwapMapsOnceNotPerBlock` | `verified` |
| PA-0-02 | Physical units in, DSP units out | part-acoustics §0.2 | `PartAcoustics.cpp` | none | `implemented` |
| PA-0-03 | Monotonic and continuous | part-acoustics §0.3 | - | `PartAcousticsTests.cpp` `PartAcoustics::theMappingIsMonotonic` | `verified` |
| PA-0-04 | Honest magnitudes | part-acoustics §0.4 | - | `PartAcousticsTests.cpp` `PartAcoustics::aReferenceGuitarSoundsLikeTheEngineDefault` | `implemented` |
| PA-0-05 | Every constant named and sourced | part-acoustics §0.5 | comments in `PartAcoustics.cpp` | code review | `implemented` |
| PA-1-01 | Wood table (17 woods: density, E, tan delta) | part-acoustics §1 | `PartAcoustics.cpp:20-84` | `PartAcousticsTests.cpp` `PartAcoustics::theMappingIsMonotonic` | `verified` |
| PA-1.1-01 | Mode f ∝ sqrt(E/rho) x thickness/area; Q ≈ 1/(2 tan delta); neck density feeds coupling and dead spots | part-acoustics §1.1 | `PartAcoustics.cpp` | `PartAcousticsTests.cpp` `PartAcoustics::theMappingIsMonotonic` | `partial` - neck density to dead-spot placement not found |
| PA-2-01 | Body fields wood, density override, thickness, area, chambering, bracing | part-acoustics §2 | `PartAcoustics.cpp:360-396` | `PartAcousticsTests.cpp` `PartAcoustics::everyMappedFieldMovesSomething` | `verified` |
| PA-2.1-01 | Chambering table: modes, gain, air resonance ranges and Q, sustain, feedback; air ∝ 1/sqrt(V) | part-acoustics §2.1 | `PartAcoustics.cpp` | `PartAcousticsTests.cpp` `PartAcoustics::chamberingPutsTheAirModeInItsRange` | `verified` |
| PA-2.1-02 | Chambering feedback coupling feeds ambiguity-resolutions 1 feedback gain | part-acoustics §2.1 | `FeedbackLoop` built (`14864ca`), but its k_couple has no chambering term (no reference to chambering in `Source/DSP/Feedback/`) | none | `pending` - not in TODO |
| PA-3-01 | Scale length sets tension `T = (2Lf)^2 mu` | part-acoustics §3 | `PartAcoustics.cpp` | `PartAcousticsTests.cpp` `PartAcoustics::scaleLengthSetsTension` | `verified` |
| PA-3-02 | Joint coupling bolt 0.55, set 0.80, through 0.95; profile = mass only | part-acoustics §3 | `PartAcoustics.cpp:426` | `PartAcousticsTests.cpp` `PartAcoustics::couplingsMultiply` | `verified` |
| PA-3-03 | Fretboard wood sets termination damping; radius feeds buzz clearance | part-acoustics §3 | radius unconsumed (DECISIONS) | none | `partial` |
| PA-4-01 | Fret material brightness NS 0.70, SS 0.90, gold-evo 0.80, brass 0.60; width slightly duller; count = range | part-acoustics §4 | `PartAcoustics.cpp:210,449` | `PartAcousticsTests.cpp` `PartAcoustics::everyMappedFieldMovesSomething` | `verified` |
| PA-4-02 | Nut material open strings only: bone 0.75, brass 0.85, graphite 0.70, plastic 0.60, Tusq 0.72; slot depths to buzz | part-acoustics §4 | `PartAcoustics.cpp:219-221` | `CharacterTests.cpp` `Character::nutMaterialChangesDamping` | `verified` |
| PA-4-03 | Nut friction -> tuning stability under bends | part-acoustics §4 | unconsumed; consumer is missing `tuning-stability.md` | none | `blocked` - phase 2b spec missing |
| PA-5-01 | Bridge mass, coupling, type defaults, tremolo enable, piezo; tailpiece mass and break angle | part-acoustics §5 | `PartAcoustics.cpp:426-434,507-546` | `PartAcousticsTests.cpp` `PartAcoustics::couplingsMultiply` | `verified` |
| PA-5-02 | Bridge type table (8 types with mass and coupling) | part-acoustics §5 | factory bridge parts | `WorkshopTests.cpp` `Workshop::theFactoryLibraryIsThere` | `implemented` |
| PA-5-03 | `tremolo_type` sets spring count and return | part-acoustics §5 | `spring_count` unconsumed (DECISIONS) | none | `partial` |
| PA-6-01 | Pickup fields L, R, C, magnet, coil_turns, pole material, cover, position, heights | part-acoustics §6 | `PartAcoustics.cpp:538`; coil_turns unconsumed (DECISIONS) | `PartAcousticsTests.cpp` `PartAcoustics::aCoverCostsTopEnd`, `PartAcoustics::pickupPositionSetsTheComb` | `partial` - coil_turns output level not mapped |
| PA-6.2-01 | Magnet pull table (A2..Neodymium); pull damps and flattens (Stratitis) from height | part-acoustics §6.2 | `PartAcoustics.cpp:59,202` | `PartAcousticsTests.cpp` `PartAcoustics::magnetPullShortensSustainAndPullsFlat` | `verified` |
| PA-7-01 | Wiring fields map to circuit components; `switching` topology | part-acoustics §7 | `PartAcoustics.cpp` wiring; switching not found | none | `partial` - switching topology field not mapped |
| PA-8-01 | Strings: gauges -> mu; winding, material, core round/hex; winding pitch; tension computed | part-acoustics §8 | `PartAcoustics.cpp:254,488` | `PartAcousticsTests.cpp` `PartAcoustics::scaleLengthSetsTension` | `implemented` - core round/hex effect not found |
| PA-8-02 | Inharmonicity B ∝ d^4 E / (T L^2) | part-acoustics §8 | `StringMaterials.cpp` | `ModelTests.cpp` `StringPhysics::woundStringsAreLessStiffThanTheirDiameterSuggests` | `verified` |
| PA-9-01 | Pickguard mass damps the top; hardware colour silent; finish gloss up to -0.5 dB / Q -8% on acoustic; finish aging feeds body break-in | part-acoustics §9 | `PartAcoustics.cpp:330,396,453-455` | `PartAcousticsTests.cpp` `PartAcoustics::hardwareColourIsSilent` | `verified` |
| PA-10-01 | Composition: masses add, couplings multiply, dampings add in loss domain | part-acoustics §10 | `PartAcoustics.cpp` | `PartAcousticsTests.cpp` `PartAcoustics::couplingsMultiply` | `verified` |
| PA-11-01 | Test: every numeric field moves the spectrum at ±10% | part-acoustics §11 | - | `PartAcousticsTests.cpp` `PartAcoustics::everyMappedFieldMovesSomething` (mapped fields; unconsumed ones exempted) | `partial` |
| PA-11-02 | Test: monotonicity for density, mass, thickness, inductance, position | part-acoustics §11 | - | `PartAcousticsTests.cpp` `PartAcoustics::theMappingIsMonotonic` | `verified` |
| PA-11-03 | Test: 628 vs 648 mm tension ratio within 1% | part-acoustics §11 | - | `PartAcousticsTests.cpp` `PartAcoustics::scaleLengthSetsTension` | `verified` |
| PA-11-04 | Test: ceramic at 1.5 mm >= 15% shorter T60 than A3 at 3.5 mm, flat pull | part-acoustics §11 | - | `PartAcousticsTests.cpp` `PartAcoustics::magnetPullShortensSustainAndPullsFlat` | `verified` |
| PA-11-05 | Test: 38 mm pickup first null within 5% | part-acoustics §11 | - | `PartAcousticsTests.cpp` `PartAcoustics::pickupPositionSetsTheComb` | `verified` |
| PA-11-06 | Test: nickel cover -0.8 ± 0.2 dB at 4 kHz | part-acoustics §11 | - | `PartAcousticsTests.cpp` `PartAcoustics::aCoverCostsTopEnd` | `verified` |
| PA-11-07 | Test: chambering air mode in range | part-acoustics §11 | - | `PartAcousticsTests.cpp` `PartAcoustics::chamberingPutsTheAirModeInItsRange` | `verified` |
| PA-11-08 | Test: hardware colour bit-identical | part-acoustics §11 | - | `PartAcousticsTests.cpp` `PartAcoustics::hardwareColourIsSilent` | `verified` |
| PA-11-09 | Test: two half-couplings give a quarter | part-acoustics §11 | - | `PartAcousticsTests.cpp` `PartAcoustics::couplingsMultiply` | `verified` |
| PA-11-10 | Test: mapping once per swap | part-acoustics §11 | - | `WorkshopPresetTests.cpp` `WorkshopSwap::aSwapMapsOnceNotPerBlock` | `verified` |
| PA-11-11 | Test: DerivedAcoustics deterministic | part-acoustics §11 | - | `PartAcousticsTests.cpp` `PartAcoustics::theMappingIsDeterministic` | `verified` |

## 20. workshop-ui.md (phase 2)

**Model and bench UI built** (TODO 7). `0d225f0` added the model
(`Source/Workshop/WorkshopBench.*`, `SpectrumDelta.*`); `d45fcd6` the bench
(`Source/UI/WorkshopPanel.*`: WORKSHOP tab over columns 3 and 4, Easy
overlay, drawer, inspector, setup strip, spectrum pane, A-H slots) and
`bc25f89` the Guitar (family) category and inline field editing, with
`WorkshopPanel::*` tests. Evidence is the lead's run after the MIDI export
integration (all green except one `MidiExport` null test).

| ID | Requirement (short) | Source (file §section) | Implementation location | Verification | Status |
|---|---|---|---|---|---|
| WUI-0-01 | Direct manipulation first; fields are the precise path | workshop-ui §0.1 | `BenchIllustration` drags; inspector fields (`editInspectorField`) | `WorkshopPanelTests.cpp` `WorkshopPanel::aPickupDragIsOneEntryAndTheRulerValueFollows`, `WorkshopPanel::editingAFieldMakesAUserCopy` | `verified` |
| WUI-0-02 | Illustration authoritative: drawn = loaded | workshop-ui §0.2 | `BenchIllustration` renders the bench's guitar with `GuitarRenderer` | `WorkshopBenchTests.cpp` `WorkshopBench::aMovedPickupIsSeenReadAndHeard`; `GuitarRendererTests.cpp` `GuitarIllustration::theKeyChangesWithEveryVisibleChange` | `verified` |
| WUI-0-03 | Three feedbacks per interaction (visual, numeric, audible) | workshop-ui §0.3 | illustration, ruler / inspector value, spectrum pane and live audio | `WorkshopBenchTests.cpp` `WorkshopBench::aMovedPickupIsSeenReadAndHeard` (pickup only) | `partial` - other interactions not tested |
| WUI-0-04 | Audition never commits (shadow GuitarSpec) | workshop-ui §0.4 | `WorkshopBench` shadow audition (`Source/Workshop/WorkshopBench.*`) | `WorkshopBenchTests.cpp` `WorkshopBench::auditionNeverCommits` | `verified` |
| WUI-0-05 | Each committed change one undo entry in real units | workshop-ui §0.5, §8 | `WorkshopBench` fit / revert / remove / drag commits | `WorkshopBenchTests.cpp` `WorkshopBench::aDragIsOneUndoEntryWithItsBeforeAndAfter`, `WorkshopBench::fittingAPartSaysWhatItReplaced`, `WorkshopBench::heightsAndSetupEditsAreOneEntryEach` | `verified` |
| WUI-0-06 | Bench not modal; instrument keeps playing | workshop-ui §0.6 | a workspace tab and an Easy overlay, not a dialog | none | `implemented` |
| WUI-1-01 | Layout: header (name, modified, Save As Guitar, A/B), illustration, inspector, parts drawer (13 categories), setup strip, spectrum delta; takes Advanced columns 3+4; Easy overlay via header wrench | workshop-ui §1; GI 6 | `WorkshopPanel` (layout in its header comment); drawer categories start with Guitar (family) | `WorkshopPanelTests.cpp` `WorkshopPanel::itPaintsAndTheWorkshopTabTakesOverColumnsThreeAndFour`, `WorkshopPanel::theGuitarCategorySwitchesFamily` | `implemented` - regions not individually asserted |
| WUI-1-02 | Min width 900; inspector collapses below 900; drawer to dropdown below 700; Easy overlay to window minimum | workshop-ui §1 | `WorkshopPanel::resized` switches at 900; no 700 dropdown found | none | `partial` |
| WUI-2-01 | Ruler in mm from saddle; hover/selection outlines; live overlays (pick, slide, capo, buzz heatmap) | workshop-ui §2 | ruler with pickup rail; hover outline at 60% accent | `WorkshopPanelTests.cpp` `WorkshopPanel::aPickupDragIsOneEntryAndTheRulerValueFollows` | `partial` - live overlays not built |
| WUI-2-02 | Repaint budget full < 8 ms, overlay < 2 ms; overlay layer separate | workshop-ui §2 | - | none | `pending` |
| WUI-3.1-01 | Hover outlines at 60% accent with name + summary tooltip; does not select; click selects (sticky) | workshop-ui §3.1 | `BenchIllustration` hover and selection | `WorkshopPanelTests.cpp` `WorkshopPanel::hoverDoesNotSelect` | `verified` |
| WUI-3.2-01 | Alt-hover drawer card auditions on shadow spec; greyed inspector; delta vs committed; 30 ms crossfade back | workshop-ui §3.2 | `WorkshopPanel::hoverCard` + `WorkshopBench` shadow audition; spectrum pane shows candidate vs committed | `WorkshopPanelTests.cpp` `WorkshopPanel::auditionFromTheDrawerNeverCommits` | `partial` - 30 ms crossfade not tested |
| WUI-3.3-01 | Per-string selection shows set + override; override drawn in its material colour | workshop-ui §3.3 | inspector names the selected string's saddle | `WorkshopPanelTests.cpp` `WorkshopPanel::theInspectorShowsTheSelectedPart` (saddle line) | `partial` - string-set override display not built |
| WUI-4-01 | Drag table: pickup position/height/tilt, saddles, nut slots, fret wear brush, pick, slide, capo with snaps and ranges | workshop-ui §4 | `WorkshopBench` pickup position, heights (0.8-6 mm, DECISIONS), saddle intonation, nut slots; live pickup placement path | `WorkshopBenchTests.cpp` `WorkshopBench::heightsAndSetupEditsAreOneEntryEach`, `WorkshopBench::aPickupStopsBeforeItOverlapsAndSaysWhy`; `WorkshopPanelTests.cpp` `WorkshopPanel::aPickupDragIsOneEntryAndTheRulerValueFollows` | `partial` - pickup, height and saddle drags built; fret wear, pick, slide and capo drags not |
| WUI-4-02 | Drag rules: axis-constrained, snap default / Shift fine / Alt free, live inspector value, live comb notches, collision stop with reason | workshop-ui §4 | `WorkshopBench` snap and limits; `BenchIllustration` drags; `SpectrumDelta` comb notches | `WorkshopBenchTests.cpp` `WorkshopBench::snapIsOneMillimetreFineWithShiftFreeWithAlt`, `WorkshopBench::aPickupStopsBeforeItOverlapsAndSaysWhy`, `WorkshopSpectrum::combNotchesSitWhereThePickupIsANode`; `WorkshopPanelTests.cpp` `WorkshopPanel::aPickupDragIsOneEntryAndTheRulerValueFollows` (live value) | `verified` |
| WUI-5-01 | Inspector: name, origin, fields with units, compatibility; editable; factory edits mark modified and offer Save as user part; Swap; Revert; tooltips and accessibility | workshop-ui §5 | `WorkshopPanel` inspector, Swap, Revert, Save as user part; factory edits fit a user copy | `WorkshopPanelTests.cpp` `WorkshopPanel::theInspectorShowsTheSelectedPart`, `WorkshopPanel::editingAFieldMakesAUserCopy`, `WorkshopPanel::clickingACardFitsItAsOneUndoEntry` | `verified` |
| WUI-5-02 | Part fields plain-clamped from part-acoustics tables; no stock/advanced marking | workshop-ui §5 | - | none | `pending` |
| WUI-6-01 | Spectrum delta: fixture render committed vs candidate; flat when nothing changed; ±12 dB fixed axis + auto-zoom; worker pool; 40 ms; coalesced; comb notches during pickup drag | workshop-ui §6 | `SpectrumDelta` (after amp and cab, DECISIONS); spectrum pane with ±12 dB axis and Auto-zoom toggle | `WorkshopBenchTests.cpp` `WorkshopSpectrum::aNullChangeIsFlat`, `WorkshopSpectrum::aRealChangeShowsAndIsDescribed`, `WorkshopSpectrum::combNotchesSitWhereThePickupIsANode`, `WorkshopSpectrum::theWorkerCoalescesAndStaysInBudget`; `WorkshopPanelTests.cpp` `WorkshopPanel::auditionFromTheDrawerNeverCommits` (pane summary) | `verified` - axis and auto-zoom drawing not asserted |
| WUI-7-01 | Eight A/B GuitarSpec slots in bench header; recall = undoable swap; stored in uiState; Shift-click clears | workshop-ui §7 | `WorkshopBench` slots | `WorkshopBenchTests.cpp` `WorkshopBench::abRecallRoundTrips` | `partial` - A-H header buttons built (`d45fcd6`); uiState persistence not confirmed |
| WUI-8-01 | Drag = one entry (mouse-down to up); swaps never grouped; audition pushes nothing | workshop-ui §8 | `WorkshopBench` | `WorkshopBenchTests.cpp` `WorkshopBench::aDragIsOneUndoEntryWithItsBeforeAndAfter`, `WorkshopBench::aClickWithoutAMoveChangesNothing`, `WorkshopBench::auditionNeverCommits` | `verified` |
| WUI-9-01 | Empty/blocked states: no user parts, Slide category off, single-option type, incompatible hover warning | workshop-ui §9 | Slide category says "Turn on Slide Mode (S)"; unsuited cards marked | `WorkshopPanelTests.cpp` `WorkshopPanel::aSlideNeedsSlideMode` | `partial` - no-user-parts and single-option states not tested |
| WUI-10-01 | Hit regions focusable in builder order; arrow-key nudges; spectrum delta announced as summary; keyboard parity | workshop-ui §10 | Tab walks `BenchIllustration::builderOrder`; arrow nudges; titles and descriptions set | `WorkshopPanelTests.cpp` `WorkshopPanel::keyboardNudgesMatchADrag` | `partial` - spectrum announcement not tested |
| WUI-11-01 | Test: every part hit-testable on every factory guitar, no phantom parts | workshop-ui §11 | - | `WorkshopPanelTests.cpp` `WorkshopPanel::everyFittedPartIsReachableAndNothingElseIs` | `verified` |
| WUI-11-02 | Test: hover does not select or push undo | workshop-ui §11 | - | `WorkshopPanelTests.cpp` `WorkshopPanel::hoverDoesNotSelect` | `verified` |
| WUI-11-03 | Test: audition does not commit; audio back within 30 ms | workshop-ui §11 | - | `WorkshopBenchTests.cpp` `WorkshopBench::auditionNeverCommits`; `WorkshopPanelTests.cpp` `WorkshopPanel::auditionFromTheDrawerNeverCommits` | `partial` - 30 ms audio return not tested |
| WUI-11-04 | Test: drag = one undo entry with before/after values | workshop-ui §11 | - | `WorkshopBenchTests.cpp` `WorkshopBench::aDragIsOneUndoEntryWithItsBeforeAndAfter` | `verified` |
| WUI-11-05 | Test: drag constrained at collision with reason | workshop-ui §11 | - | `WorkshopBenchTests.cpp` `WorkshopBench::aPickupStopsBeforeItOverlapsAndSaysWhy` | `verified` |
| WUI-11-06 | Test: snap 7.4 -> 7 mm; Shift 7.4 | workshop-ui §11 | - | `WorkshopBenchTests.cpp` `WorkshopBench::snapIsOneMillimetreFineWithShiftFreeWithAlt` | `verified` |
| WUI-11-07 | Test: three feedbacks per draggable part | workshop-ui §11 | - | `WorkshopBenchTests.cpp` `WorkshopBench::aMovedPickupIsSeenReadAndHeard` (pickup only) | `partial` - other draggable parts not tested |
| WUI-11-08 | Test: null audition delta within ±0.05 dB | workshop-ui §11 | - | `WorkshopBenchTests.cpp` `WorkshopSpectrum::aNullChangeIsFlat` | `verified` |
| WUI-11-09 | Test: delta within 40 ms for every factory part swap | workshop-ui §11 | - | `WorkshopBenchTests.cpp` `WorkshopSpectrum::theWorkerCoalescesAndStaysInBudget` (one fixture swap) | `partial` - not every factory swap |
| WUI-11-10 | Test: nothing on the audio thread during bench interaction | workshop-ui §11 | - | none | `pending` |
| WUI-11-11 | Test: A/B recall round-trips after six changes | workshop-ui §11 | - | `WorkshopBenchTests.cpp` `WorkshopBench::abRecallRoundTrips` | `verified` |
| WUI-11-12 | Test: keyboard parity with drags | workshop-ui §11 | - | `WorkshopPanelTests.cpp` `WorkshopPanel::keyboardNudgesMatchADrag` | `verified` |

## 21. strum-dynamics.md (phase 2)

**Not built** (TODO 8: `StrumGesture` and the RHYTHM STRUM group). Today a
strum is a fixed `strum_speed` spread in ms (`Parameters.cpp:563`) plus
`strum_evenness` in the rhythm engine; none of the gesture model exists.

| ID | Requirement (short) | Source (file §section) | Implementation location | Verification | Status |
|---|---|---|---|---|---|
| SD-0-01 | Strum is one gesture; per-string events derived | strum-dynamics §0.1 | not built | none | `pending` |
| SD-1-01 | Crossing velocity in strings per second; delay = 1/sps | strum-dynamics §1 | `strum_speed` in ms only | none | `pending` |
| SD-1.1-01 | Source priority: MPE/hex passthrough > live keyboard spread > pattern `crossing_sps` > global default (200 guitar / 100 bass) | strum-dynamics §1.1; AMB 6 | not built | none | `pending` |
| SD-2-01 | Acceleration profile `ease(u,a)` smoothstep blend, default 0.35 | strum-dynamics §2 | not built | none | `pending` |
| SD-2.1-01 | Down steeper/brighter; up faster (ratio 1.25), softer, more chirp | strum-dynamics §2.1 | not built | none | `pending` |
| SD-3-01 | Force = base x tilt x evenness x accent; tilt ±0.15; evenness ±40% at 0, deterministic; accent +1.5 dB leading pair | strum-dynamics §3 | `strum_evenness` exists (`RhythmEngine.h:128`) | none | `partial` - only evenness exists |
| SD-3.1-01 | Miss probability 0.04, leading string x3, deterministic; `missPercent` retained | strum-dynamics §3.1 | `missPercent` exists `RhythmEngine.h:54` | none | `partial` |
| SD-4-01 | Guitar and bass default tables | strum-dynamics §4 | not built | none | `pending` |
| SD-5-01 | Strikers pick, thumb, nails, flesh, thumbpick, brush with crossing multipliers and noise; separate down/up strikers; striker selects noise generator | strum-dynamics §5 | not built | none | `pending` |
| SD-6.1-01 | Chuck: damp all strings (0.92) before crossing; `chuck_amount`; pattern chuck step or MIDI chuck key range | strum-dynamics §6.1 | not built | none | `pending` |
| SD-6.2-01 | Palm-muted strum keeps pitch; chuck loses pitch | strum-dynamics §6.2 | palm mute exists | none | `partial` |
| SD-6.3-01 | STRUM group on RHYTHM tab: sps, acceleration, tilt, evenness, miss, two striker dropdowns, chuck amount/damping | strum-dynamics §6.3 | not built | none | `pending` |
| SD-6.3-02 | Easy Feel knob maps sps 60->400 and evenness 0.45->0.95; 0.5 = defaults | strum-dynamics §6.3 | Easy rhythm strip has a feel control of different meaning | none | `pending` |
| SD-7-01 | Params (+9): crossing_sps, acceleration, up ratio, tilt, miss, strikers x2, chuck amount, chuck damping | strum-dynamics §7 | none in `Parameters.h` | none | `pending` |
| SD-8-01 | Test: 200 sps, accel 0 -> 5 ms ± 1 sample | strum-dynamics §8 | - | none | `pending` |
| SD-8-02 | Test: accel 1.0 keeps duration within 2%, first gap >= 1.6x middle | strum-dynamics §8 | - | none | `pending` |
| SD-8-03 | Test: up faster by ratio within 1% | strum-dynamics §8 | - | none | `pending` |
| SD-8-04 | Test: rhythm engine supplies velocity within 1 sample | strum-dynamics §8 | - | none | `pending` |
| SD-8-05 | Test: live 40 ms spread wins | strum-dynamics §8 | - | none | `pending` |
| SD-8-06 | Test: MPE passes through | strum-dynamics §8 | - | none | `pending` |
| SD-8-07 | Test: misses weighted and deterministic over 10 000 strums | strum-dynamics §8 | - | none | `pending` |
| SD-8-08 | Test: evenness bounds variation | strum-dynamics §8 | - | none | `pending` |
| SD-8-09 | Test: striker selects noise | strum-dynamics §8 | - | none | `pending` |
| SD-8-10 | Test: chuck kills pitch, keeps body resonance | strum-dynamics §8 | - | none | `pending` |
| SD-8-11 | Test: Feel knob mapping within 1% | strum-dynamics §8 | - | none | `pending` |
| SD-8-12 | Test: bass defaults on bass family | strum-dynamics §8 | - | none | `pending` |

## 22. bass-techniques.md (phase 2)

**Not built** (TODO 8: `BassTechniques`, SLAP group, bass step grid). Only an
`Excitation::Kind::Slap` shape exists (`Source/DSP/String/Excitation.h:42`),
not the collision model. Bass-family defaults elsewhere are not applied.

| ID | Requirement (short) | Source (file §section) | Implementation location | Verification | Status |
|---|---|---|---|---|---|
| BT-0-01 | Bass techniques only on family `bass`; SLAP hidden otherwise with fixed empty-state text | bass-techniques §0.1, §1 | not built | none | `pending` |
| BT-0-02 | Slap is a collision driving the string into the frets | bass-techniques §0.2, §2.1 | `Excitation::Kind::Slap` is a shortened bright pluck (`Excitation.cpp:94`) | none | `pending` |
| BT-0-03 | Ghost notes cheap and plentiful | bass-techniques §0.3 | not built | none | `pending` |
| BT-0-05 | Headroom survives slaps without the limiter | bass-techniques §0.5 | not built | none | `pending` |
| BT-2-01 | Slap collision: fast 0.3 ms rise, fret contact handed to FretBuzz with large excess, inharmonic attack settling 30-80 ms | bass-techniques §2.1 | not built | none | `pending` |
| BT-2-02 | Slap params strength, position, thumb hardness, fret contact | bass-techniques §2.2 | not built | none | `pending` |
| BT-3-01 | Pop: release is the event, snap-back collision, 2-4 kHz transient; strength 0.75, position 40 | bass-techniques §3 | not built | none | `pending` |
| BT-4-01 | Double thump: return stroke x0.65 brighter at `1/(2 x sps)` | bass-techniques §4 | not built (depends on strum-dynamics) | none | `pending` |
| BT-5-01 | Ghosts: damping 0.94 (shares chuck code), level 0.45, auto below velocity 32 (on by default for bass), BASS_TECH trigger | bass-techniques §5 | not built | none | `pending` |
| BT-6-01 | Fingerstyle alternation variation 0.25; rest stroke damps next-lower string; bass pluck position default | bass-techniques §6 | not built | none | `pending` |
| BT-7-01 | Pick bass: 1.14 mm default; bass palm-mute profile | bass-techniques §7 | not built | none | `pending` |
| BT-8-01 | Bass defaults elsewhere: 100 sps, miss 0.01, squeak pressure 0.35, Factory-low setup, 864 mm scale, 1.14 mm pick, compressor on 2:1 | bass-techniques §8 | factory bass guitar files carry scale; others not keyed on family | none | `pending` |
| BT-9-01 | SLAP group on CHARACTER (bass only) with all technique controls | bass-techniques §9 | not built | none | `pending` |
| BT-9-02 | Bass step grid on RHYTHM (thumb, pop, ghost, fingerstyle, dead); factory bass kits | bass-techniques §9 | not built | none | `pending` |
| BT-10-01 | `BASS_TECH` export: exact in Luthier profile; velocities in Generic | bass-techniques §10 | not built | none | `pending` |
| BT-11-01 | +14 params; slap/pop positions in `buzz` family | bass-techniques §11 | none in `Parameters.h` | none | `pending` |
| BT-12-01 | Test: inert on a guitar; SLAP hidden | bass-techniques §12 | - | none | `pending` |
| BT-12-02 | Test: slap triggers FretBuzz at contact 0.8, not at 0 | bass-techniques §12 | - | none | `pending` |
| BT-12-03 | Test: slap centroid >= 1.5x and rise >= 3x faster than a vel-127 pluck | bass-techniques §12 | - | none | `pending` |
| BT-12-04 | Test: pop brighter and shorter than slap by >= 20% | bass-techniques §12 | - | none | `pending` |
| BT-12-05 | Test: double thump two events at spacing, ratio within 0.5 dB | bass-techniques §12 | - | none | `pending` |
| BT-12-06 | Test: ghosts pitchless | bass-techniques §12 | - | none | `pending` |
| BT-12-07 | Test: auto-ghost fires below threshold only | bass-techniques §12 | - | none | `pending` |
| BT-12-08 | Test: rest stroke damps neighbour >= 12 dB in 10 ms | bass-techniques §12 | - | none | `pending` |
| BT-12-09 | Test: finger alternation varies; not at 0 | bass-techniques §12 | - | none | `pending` |
| BT-12-10 | Test: bass defaults applied on each factory bass | bass-techniques §12 | - | none | `pending` |
| BT-12-11 | Test: full slap on all strings peaks below 0 dBFS, limiter bypassed | bass-techniques §12 | - | none | `pending` |
| BT-12-12 | Test: BASS_TECH round-trips; Generic keeps slap/ghost velocity distinction | bass-techniques §12 | - | none | `pending` |

## 23. midi-export.md (phase 2)

**Model built and integrated, not wired** (`Source/Export/*`, in the build
since the lead's integration; 25 `MidiExport::*` tests, all passing on the
lead's run except `MidiExport::luthierRoundTripNullsEveryFactoryPreset`,
which fails on 4 presets because of an engine determinism bug the lead is
fixing).
`LuthierMidiEvents` encodes the 18 classes as tagged 7-bit text (text meta and
checksummed SysEx); `MidiPerformance` holds the channel stream at exact
samples plus the events and converts to and from `PerformanceScore` and
`MidiCapture`; `MidiProfiles` writes and reads both profiles (header,
`LUTHIER-AT` sample corrections, splits, ranges, `.midprofile`, drag-out file,
preview) and refuses damaged files with a byte reference; `LiveMidiOut` adds
the beat clock and the SysEx source for live MIDI out. The encoding is in
`docs/MIDI_EXPORT_LUTHIER_PROFILE.md`. Not done: the dialog, MIDI OUT tab,
drag gesture, import targets, Options page, `processBlock` wiring, Tune path,
and engine capture of realism events.

| ID | Requirement (short) | Source (file §section) | Implementation location | Verification | Status |
|---|---|---|---|---|---|
| MX-0-01 | Export on a worker thread from PerformanceScore or Tune | midi-export §0.1 | `MidiPerformance::fromScore`, `MidiPerformance::fromCapture`, `MidiProfiles::exportToMemory` (`Source/Export/`; nothing on the audio thread); no Tune, no call site | `MidiExportTests.cpp` `MidiExport::aScoreSurvivesBothProfiles`, `MidiExport::captureBecomesAPerformanceAtItsOwnSamples` | `partial` - model only; Tune and worker call site pending |
| MX-0-02 | Import parses to PerformanceScore or Tune; delivered via swap | midi-export §0.2 | `MidiProfiles::importFromMemory` -> `MidiPerformance`, `MidiPerformance::toScore`; `MidiPerformance::renderBlock` plays it on the audio thread; no swap into the engine, no Tune | `MidiExport::aScoreSurvivesBothProfiles` | `partial` - swap and Tune pending |
| MX-0-03 | Luthier profile self-describing with schema version | midi-export §0.3 | header wire version and `LUTHIER-BEGIN <class> <schema>` (`MidiProfiles.cpp` `makeHeader`, `writeExtensionEvent`); newer schemas read with a warning | `MidiExport::importWarnsOfAdvancedRangesAndNewerSchemas` | `verified` |
| MX-0-04 | Sample-accurate internal, beat-accurate ticks at 960 PPQ default | midi-export §0.4 | `MidiPerformance` holds samples; ticks through the file tempo map; `LUTHIER-AT dt=` restores the sample (`MidiProfiles.cpp` `FileTempoMap`); another sample rate falls back to the tick with a warning | `MidiExport::luthierProfileIsSampleExactAtEveryPpqAndSplit`, `MidiExport::anotherSampleRateFallsBackToTheTick` | `verified` |
| MX-1-01 | SMF format 1; PPQ 96-3840; meta track (title, copyright, tempo, time sig, key); per-instrument or per-string tracks; 14-bit bend | midi-export §1 | `MidiProfiles::exportToMemory` (meta track, four splits, PPQ clamp); notation writer `NotationExport.cpp` | `MidiExport::luthierProfileIsSampleExactAtEveryPpqAndSplit`, `MidiExport::trackSplitsNameTheirTracks`, `MidiExport::genericProfileIsPlainMidi`; `NotationTests.cpp` `Notation::midiExportIsPerString` | `verified` |
| MX-2-01 | LUTHIER header chunk; `LUTHIER-BEGIN <class> <ver>` / `LUTHIER-END` markers; stripped file plays as Generic | midi-export §2 | `MidiProfiles.cpp` `makeHeader` (FF 7F, 7D "LUTHIER", checksum), `writeExtensionEvent` | `MidiExport::headerStrippedFileLoadsAsGenericWithoutWarning` | `verified` |
| MX-2.1-01 | Event classes NOTE, BEND, SLIDE, VIBRATO, WHAMMY, STRUM, RASGUEADO, PICK, SQUEAK, BUZZ, SLIDE_BAR, CLANK, CHARACTER, WORKSHOP, BASS_TECH, RANGES, SNAPSHOT, SECTION | midi-export §2.1 | `LuthierMidiEvents.cpp` `kClasses`, `getFields`; NOTE / BEND / SLIDE / VIBRATO / WHAMMY / SECTION filled from `PerformanceScore` | `MidiExport::everyEventClassRoundTripsWithEveryField` | `partial` - encoded and round-tripped; the engine does not yet report realism events (STRUM, PICK, SQUEAK, BUZZ, CLANK, SLIDE_BAR, ...) for capture |
| MX-2.1-02 | Byte-level encoding documented in `docs/MIDI_EXPORT_LUTHIER_PROFILE.md` | midi-export §2.1 | `docs/MIDI_EXPORT_LUTHIER_PROFILE.md` | document review | `implemented` |
| MX-2.2-01 | Round-trip within -60 dBFS RMS null; unknown classes preserved as opaque blobs | midi-export §2.2 | exact channel stream plus events; an unknown class is kept as fields, or as bytes when it is not tagged text (`MidiProfiles.cpp` `finishEvent`) | `MidiExport::luthierRoundTripNullsEveryFactoryPreset`, `MidiExport::everyEventClassRoundTripsWithEveryField` | `verified` - renders repeat after a reset since `a357142` (531 tests green) |
| MX-2.3-01 | Every extension event as text meta + SysEx; reader accepts either | midi-export §2.3 | `writeExtensionEvent`; `finishEvent` reads either and refuses copies that disagree | `MidiExport::eitherCopyOfAnEventIsEnough`, `MidiExport::damagedExtensionEventsAreRefused` | `verified` |
| MX-3-01 | Generic profile: notes, bend, CC 1/11/64/74; `LUTHIER:` text metas; bend range RPN at track start | midi-export §3 | `MidiProfiles.cpp` Generic path; CC 101/100/6/38 then null RPN per channel; realism text without timing or identifiers | `MidiExport::genericProfileIsPlainMidi` | `verified` |
| MX-4.1-01 | Export dialog (profile, range, track split, include realism, PPQ, destination, preview) from MIDI OUT tab, tune builder, File -> Export -> MIDI | midi-export §4.1 | MIDI OUT tab exports the capture whole or last N seconds with a preview in the chosen profile (`a357142`); no tune-builder or File-menu entry | `MidiOutPanelTests.cpp` `MidiOutPanel::exportWritesTheCaptureInTheChosenProfile`, `MidiOutPanel::profileEditsAreTheExportDefaults`; `MidiExportTests.cpp` `MidiExport::previewDescribesTheOpeningBar` | `partial` - tune builder and File menu entry points |
| MX-4.2-01 | Drag-out from session recorder "Save last take" (Alt forces Generic) | midi-export §4.2 | MIDI OUT tab drags the capture out (Alt: Generic) and the header's Save last MIDI take uses the profile (`a357142`); `MidiProfiles::writeDragOutFile` | `MidiExportTests.cpp` `MidiExport::dragOutWritesAValidMidiFile` (file only) | `implemented` - the drag gesture is not tested |
| MX-5-01 | Import via File menu or window drop; auto-detect profile; target session / tune / looper | midi-export §5 | `MidiProfiles::importFromFile` detects the profile by header; no File menu entry, drop target or session / tune / looper target | `MidiExport::headerStrippedFileLoadsAsGenericWithoutWarning` | `partial` - UI and targets pending |
| MX-6-01 | Live MIDI-out sources incl. tune playback, character/noise SysEx, workshop changes | midi-export §6 | routing panel switches plus TUNE, EVENTS (each noise trigger as Luthier SysEx on its sample) and WORKSHOP (part fits) (`a357142`); `NEEDS_MIDI_OUTPUT FALSE` still leaves the VST3 without a MIDI out bus (RIO-6-05) | `MidiOutPanelTests.cpp` `MidiOutPanel::liveEventsAndWorkshopChangesGoOutAsLuthierSysEx`, `MidiOutPanel::liveSwitchesAreTheRoutingPanelsSwitches`; `MidiExportTests.cpp` `MidiExport::liveMidiOutKeepsTenThousandEventsOnTheirSample` | `partial` - tune playback has no Tune transport to feed it; VST3 MIDI out bus |
| MX-7-01 | `.mid`/`.midi` for both; `.midprofile` JSON user export configs, registered by installers | midi-export §7 | `MidiProfiles::saveProfile` / `loadProfile` / `profileFromVar`; installers do not register `.midprofile` | `MidiExport::midprofileSavesAndLoads`, `MidiExport::classMaskLimitsWhatIsWritten` | `partial` - installer registration pending |
| MX-8-01 | Options -> MIDI: default profile, PPQ, track split, realism in Generic, SysEx redundancy | midi-export §8 | the export profile is Options -> MIDI's user-global default, saved and loaded as `.midprofile` (`a357142`) | `MidiOutPanelTests.cpp` `MidiOutPanel::profileEditsAreTheExportDefaults`; `MidiExportTests.cpp` `MidiExport::midprofileSavesAndLoads` | `verified` |
| MX-9-01 | Cross-spec: shares PerformanceScore with notation; tune export; session `.mid`; snapshots; macro CCs; strums; BASS_TECH; range annotations | midi-export §9 | `fromScore` / `toScore` use NotationExporter's layout; SNAPSHOT, BASS_TECH, RANGES classes; `fromCapture` for the session take (still written by `MidiCapture`) | `MidiExport::aScoreSurvivesBothProfiles` | `partial` - tune, snapshot and macro-CC sources not wired |
| MX-10-01 | Older files load with missing events reconstructed and a notification | midi-export §10 | `MidiImportResult::defaultedFields` ("CLASS.field") and newer-schema warnings; whole missing events are not reconstructed from the plain MIDI | `MidiExport::importWarnsOfAdvancedRangesAndNewerSchemas`, `MidiExport::stripIdentifiersLeavesNoNamesOrSeeds` | `partial` - event reconstruction pending |
| MX-11-01 | No personal data; Generic omits seed/guitar/preset names; "strip identifiers" option | midi-export §11 | `MidiExportOptions::stripIdentifiers`; identifier fields (`LuthierFieldSpec::isIdentifier`); Generic never writes them | `MidiExport::stripIdentifiersLeavesNoNamesOrSeeds`, `MidiExport::genericProfileIsPlainMidi` | `verified` |
| MX-UI-01 | MIDI OUT tab in column 4 | GI 4.4 | `MidiOutPanel` (`a357142`), in the fixed tab order, remembered by name | `MidiOutPanelTests.cpp` `MidiOutPanel::theTabSitsInTheFixedOrderAndIsRememberedByName` | `verified` |
| MX-12-01 | Test: Luthier round trip for every factory preset and fixture within -60 dBFS | midi-export §12 | one render fixture (sub-tick notes, bend, CC 1/11/64, pressure) through every factory preset | `MidiExport::luthierRoundTripNullsEveryFactoryPreset` | `verified` - green at `a357142` |
| MX-12-02 | Test: Generic round trip within -30 dBFS | midi-export §12 | score fixture on the tick grid | `MidiExport::genericRoundTripNullsWithinThirtyDb` | `verified` |
| MX-12-03 | Test: SysEx off still round-trips via text metas | midi-export §12 | - | `MidiExport::eitherCopyOfAnEventIsEnough` | `verified` |
| MX-12-04 | Test: PPQ 96/480/960/3840 timing within 1 sample | midi-export §12 | exact (0 samples) in the Luthier profile at every PPQ and split | `MidiExport::luthierProfileIsSampleExactAtEveryPpqAndSplit` | `verified` |
| MX-12-05 | Test: drag-out produces valid MIDI in a DAW fixture | midi-export §12 | JUCE's `MidiFile` reader stands in for the DAW; no drag gesture to start from | `MidiExport::dragOutWritesAValidMidiFile` | `partial` - no session-recorder drag |
| MX-12-06 | Test: stripped Luthier file loads as Generic silently | midi-export §12 | - | `MidiExport::headerStrippedFileLoadsAsGenericWithoutWarning` | `verified` |
| MX-12-07 | Test: byte-flip corrupt import refuses gracefully with banner | midi-export §12 | every refusal starts "Byte 0x..." with the offset; no banner UI | `MidiExport::everyFlippedByteIsRefusedGracefully`, `MidiExport::damagedExtensionEventsAreRefused` | `partial` - banner not built |
| MX-12-08 | Test: live MIDI-out 10 000-event fuzz within 1 sample across all sources | midi-export §12 | router, beat clock and SysEx out driven as `processBlock` would drive them | `MidiExport::liveMidiOutKeepsTenThousandEventsOnTheirSample`; `RoutingTests.cpp` `Routing::midiOutPassThroughIsSampleExact` | `verified` - with the EVENTS and WORKSHOP sources wired (`a357142`) |
| MX-12-09 | Test: non-Luthier host drops SysEx; second instance parses | midi-export §12 | host simulated by dropping SysEx from the buffer | `MidiExport::liveSysExIsDroppedByOtherHostsAndReadByLuthier` | `verified` |

## Phase 2b (INDEX 23a-23i): referenced but missing

`INDEX.md` (phase 2b list and the "What each file adds" table) and
`CLAUDE_CODE_BRIEF.md` (reading order, step 1) list nine extended-realism
specs. **None of them exists on disk** (`rg --files spec -g '*.md'`,
2026-09-23). DECISIONS (C-02) keeps them blocked rather than written from
the one-line summaries. `DECISIONS.md` (2026-09-23 spec update) and `TODO.md` 13c
record them as blocked rather than written from one-line descriptions; the
user was asked. Their requirements cannot be enumerated, so each file is one
row. Known downstream consumers are listed so nothing waits silently.

| ID | Requirement (short) | Source (file §section) | Implementation location | Verification | Status |
|---|---|---|---|---|---|
| P2B-23a-01 | `string-aging.md`: fresh-to-dead arc, oxidation, contamination, corrosion pits, core fatigue, per-string state, coated vs uncoated | INDEX §Phase 2b 23a; INDEX "What each file adds" | Nearest existing: `string_age` 3-choice param (`Source/Parameters.h:62`), `StringPhysics::ageDullsAndShortens` | none | `blocked` - file not on disk |
| P2B-23b-01 | `environment.md`: temperature/humidity on relief, action, tuning drift, body resonance, wolf notes; session drift profiles | INDEX 23b | Nearest existing: character-wear temperature/humidity (`Source/Character/CharacterEngine.cpp`) | none | `blocked` - file not on disk |
| P2B-23c-01 | `body-coupling.md`: bridge-mediated string-body-string coupling, body-mode bank, wolf notes, tap tones, sympathetic ring (depends on part-acoustics) | INDEX 23c; BRIEF §order 1 | Nearest existing: `CouplingMatrix`, `BodyEngine` modal bank | none | `blocked` - file not on disk |
| P2B-23d-01 | `harmonic-realism.md`: natural, pinch, tapped, artificial harmonics as boundary conditions | INDEX 23d | Nearest existing: harmonic techniques in `TechniqueEngine` / `StringEngine` | none | `blocked` - file not on disk |
| P2B-23e-01 | `string-interaction.md`: air-path sympathetic ring, palm-mute spread, adjacent finger damping, chord release stagger, pickup crosstalk, muted-string thump (depends on body-coupling) | INDEX 23e | none | none | `blocked` - file not on disk |
| P2B-23f-01 | `fingerstyle-attack.md`: nail/pad/thumb/hybrid/Travis/classical rest-free stroke/slap-pop contact profiles; per-string tool assignment | INDEX 23f | Nearest existing: `Excitation` finger/nail/thumb filters | none | `blocked` - file not on disk |
| P2B-23g-01 | `noise-floor.md`: single-coil hum, amp hiss, tube microphonics, ground loop, radio, fluorescent buzz, cable movement, passive hiss; region + position | INDEX 23g | Nearest existing: `noise_amp_buzz` param | none | `blocked` - file not on disk |
| P2B-23h-01 | `sustain-and-decay.md`: attack transient, two-stage decay, amplitude-driven pitch drift, physical note-off release, sustain macros | INDEX 23h | Nearest existing: `StringEngine` loop filter | none | `blocked` - file not on disk |
| P2B-23i-01 | `tuning-stability.md`: settling, nut binding, tuner backlash, saddle creep, bend memory, capo bias, per-string retune / auto-retune | INDEX 23i | Workshop tuner and nut fields are parsed but unconsumed (DECISIONS "Part fields not yet consumed") | none | `blocked` - file not on disk; also blocks consumers of tuner ratio/mass/stability/locking and nut friction |

## 24. tune-builder.md (phase 3)

**Model built** (`d284547`, `Source/Tune/*`, written by the feature
assistant): `Tune` value model with `extra` for unknown fields,
`.luthiertune` load / save (canonical, atomic, dated backup, named errors),
chord spans and a strict shorthand parser, progression tools, seeded melody
and bass generators with locked notes kept, the MIDI timeline, the `.mid`
writer, `PerformanceScore` conversion and the ten templates in
`Resources/Tunes/Templates`, with 35 `TuneBuilder::*` tests (green on the
lead's run). Not built: the TUNE tab and every UI row, transport and engine
playback, audio export, recording from MIDI in, Sing, and the Luthier-profile
MIDI path (C-53). Note: ground rule 7 says "five methods (section 5)" but section 2
lists four plus section 13's Sing; step numbering skips 4 (editorial only).

| ID | Requirement (short) | Source (file §section) | Implementation location | Verification | Status |
|---|---|---|---|---|---|
| TB-0-01 | Control-only: writes MIDI into rhythm and note engines, never the audio path | tune-builder §0.1; INDEX global rules | `TunePlayer` renders the timeline as MIDI; the processor merges the chord channel into host MIDI and sends the rest via `LuthierEngine::setDirectMidi` | `TuneProcessorTests.cpp` `TuneProcessor::theMelodySoundsWhileTheRhythmEngineStrums`; `TuneBuilder::timelinePutsChordsMelodyBassAndSectionsOnTheBeat` | `verified` |
| TB-0-02 | Single `.luthiertune` file < 100 KB, portable, forward-compatible | tune-builder §0.2, §11 | `TuneFile` (JSON, `extra` kept, `tooLarge` refusal) | `TuneBuilderTests.cpp` `TuneBuilder::aHundredRandomTunesRoundTripByteIdentical`, `TuneBuilder::unknownFieldsAreKeptAndWrittenBack` | `verified` - the 100 KB bound is enforced but not tested |
| TB-0-03 | Nothing destructive: every edit undoable; regenerate preserves locked notes | tune-builder §0.3 | `Tune` is a value (undo = before / after pair) with `TuneEditClass`; generators keep locked notes; no undo-stack wiring | `TuneBuilderTests.cpp` `TuneBuilder::regenerateLeavesLockedNotesByteIdentical` | `partial` - undo stack not wired |
| TB-0-04 | Same UI standalone and in a host | tune-builder §0.4 | not built | none | `pending` |
| TB-0-05 | Export first-class: audio, MIDI, MusicXML/GP from one PerformanceScore | tune-builder §0.5 | `buildTuneMidiFile`, `buildTuneScore`; no audio export | `TuneBuilderTests.cpp` `TuneBuilder::theMidiFileHasAMetaTrackInstrumentTracksAndBeatAccurateTicks`, `TuneBuilder::thePerformanceScoreKeepsSectionsChordSymbolsAndFrettedNotes` | `partial` - audio export and one shared export path (C-53) |
| TB-1-01 | `Tune` model: meta, sections (name, bars, chords, pattern, kit, melody, bass, layers), setlist, variations; every field versioned | tune-builder §1 | `Source/Tune/TuneModel.*` | `TuneBuilderTests.cpp` `TuneBuilder::aHundredRandomTunesRoundTripByteIdentical`, `TuneBuilder::editsKeepNamesUniqueAndTheSetlistInStep` | `verified` |
| TB-1.1-01 | ChordCell: root, quality, bass, extensions, duration, strum override, emphasis; last cell fills the section | tune-builder §1.1 | `ChordCell`, `resolveChordSpans` | `TuneBuilderTests.cpp` `TuneBuilder::chordSpansRepeatAShortProgressionAndHoldAnUnderspecifiedLastCell` | `verified` |
| TB-1.2-01 | MelodyTrack / MelodyNote with absolute or relative pitch (`root+7`, `chord_tone_3`), articulation, technique, locked | tune-builder §1.2 | `MelodyPitch` (absolute, root offset, chord tone), `MelodyNote` | `TuneBuilderTests.cpp` `TuneBuilder::relativePitchesFollowTheChordTheyLandOn` | `verified` |
| TB-2-01 | Three-minute flow: kit (9 presets with tempo/feel/palette), shorthand progression (pipes, `[Section]`, `*2`), melody one-click, play loops with bar-boundary edits, `Ctrl+S` save / `Ctrl+E` export | tune-builder §2 | TUNE tab: progression field, kit box, melody buttons, transport; edits wait for the bar line (`TunePlayer`); Ctrl+S/E when the tab has focus (DECISIONS "TUNE in the plugin") | `TunePanel::theProgressionFieldWritesTheSectionAndShowsErrorsWhereTheyAre`, `TunePanel::theHeaderEditsTitleTempoKeyAndSavesAndLoads`, `TunePlayer::anEditWhilePlayingWaitsForTheBarLineAndAPausedOneDoesNot` | `partial` - kit suggested tempo (GenreKit has no tempo); one-screen export dialog (9) not built |
| TB-3-01 | TUNE tab between RHYTHM and LIVE; default layout (header, section strip, progression, rhythm, melody, transport); fits at 1280 | tune-builder §3, §3.1 | `Source/UI/TunePanel.*`; `AdvancedPanel` tab list | `TunePanel::rendersWithTheLookAndFeel` (PNG `_tune.png`) | `partial` - tab position and 1280 fit not asserted |
| TB-3.2-01 | Progression pills coloured by function; popover editor; drag duration/reorder; shorthand field live; right-click insert/duplicate/delete/copy/paste/suggest substitution | tune-builder §3.2 | shorthand field live with inline errors; pills drawn | `TunePanel::theProgressionFieldWritesTheSectionAndShowsErrorsWhereTheyAre`; model tests | `partial` - popover, drag and right-click menus deferred |
| TB-3.3-01 | Section strip: select, drag reorder, rename, duplicate, delete, repeat count, Vary, colour tag; setlist timeline | tune-builder §3.3 | `TunePanel` section strip and menu | `TunePanel::theSectionStripsMenuRenamesDuplicatesRepeatsTagsLinksAndDeletes`; model tests | `partial` - drag reorder and Vary deferred |
| TB-3.4-01 | Piano roll: draw, snap to key (`C` toggles chromatic), right-click note menu incl. unlock, multi-select, clipboard, arrow nudge | tune-builder §3.4 | `TunePanel` piano roll: draw, snap, lock, delete | `TunePanel::thePianoRollDrawsSnappedLockedNotesAndDeletesThem` | `partial` - note menu, multi-select, clipboard, nudge deferred |
| TB-3.5-01 | Rhythm strip reuses kit/feel/strum; per-section; "Link rhythm to X" | tune-builder §3.5 | `TunePanel` rhythm strip; section menu link; `TuneSession::applyRhythmChange` | `TunePanel::theRhythmStripSetsTheSectionsKitFeelStrumAndOn`, `theSectionStripsMenuRenamesDuplicatesRepeatsTagsLinksAndDeletes`, `aRhythmChangeReachesTheRhythmEngine` | `verified` |
| TB-3.6-01 | Transport: play/pause, skip section, loop, count-in, metronome; independent unless host plays; Space / Shift+Space | tune-builder §3.6 | `TunePlayer` clocks; `TunePanel` transport and keys (focus-scoped) | `TunePlayer::theHostWinsWhenItPlaysAndTheClockRunsWhenItDoesNot`, `skippingMovesBySectionsAndWrapsWithLoop`, `TunePanel::theTransportAndSpaceDriveThePlayer` | `verified` |
| TB-3.6-01b | Count-in and metronome audible; click route (practice-tools 0.2) | tune-builder §3.6 | `Metronome::renderClicksAt` on the tune's grid; monitor bus or main out | `TuneProcessor::theCountInIsHeardOnTheMainOutWhenSentThere`, `TuneProcessor::theMetronomeTabSendsTheClickToTheMainOut`, `TunePlayer::aCountInWaitsABarAndClicksIt` | `verified` |
| TB-MX-01 | MIDI out carries the tune's playback when its source is on | midi-export §6; tune-builder §8 | processor adds the player's MIDI-out buffer after `MidiOutRouter::emit` | `TuneProcessor::midiOutCarriesTheTuneWhenAskedTo` | `verified` |
| TB-4.1-01 | Auto melody: chord-tone start, 70/20/10 motion, cadence rests, kit phrasing, range C3-G5, density 4/bar, deterministic seed | tune-builder §4.1 | `Source/Tune/TuneMelody.*` | `TuneBuilderTests.cpp` `TuneBuilder::autoMelodyIsByteIdenticalForASeedAcrossAThousandRuns`, `TuneBuilder::autoMelodyStaysInRangeRestsAtCadencesAndStartsOnAChordTone` | `verified` |
| TB-4.3-01 | Record: quantise grids incl. triplets; velocity kept; "Follow chord changes" | tune-builder §4.3 | `TunePlayer::captureInput` fed the host MIDI in `processBlock`; `TuneSession` gathers and quantises | `TunePlayer::recordingPlacesMidiInInTheTune`, `TunePanel::aTakeFromMidiInLandsInTheSectionItWasPlayedIn`, `TuneBuilder::recordQuantiseKeepsVelocityAndRefitsHeldNotesToTheNewChord` | `verified` |
| TB-4.4-01 | Improvise reseeds each pass; locked notes kept; Freeze captures | tune-builder §4.4 | `TuneMelody` improvise and freeze | `TuneBuilderTests.cpp` `TuneBuilder::improviseVariesEachPassAndFreezeWritesItDown` | `verified` |
| TB-4.5-01 | Style transfer: articulation/micro-timing profiles, pitch unchanged | tune-builder §4.5 | `MelodyStyle` profiles | `TuneBuilderTests.cpp` `TuneBuilder::styleTransferChangesPhrasingButNeverPitchOrCount` | `verified` |
| TB-5-01 | Diatonic palette, suggest next chord, reharmonize, transpose (melody follows), modal shift | tune-builder §5 | `Source/Tune/TuneHarmony.*` | `TuneBuilderTests.cpp` `TuneBuilder::diatonicPaletteAndFunctionsFollowTheKey`, `TuneBuilder::suggestNextChordOffersThreeDistinctCommonMoves`, `TuneBuilder::reharmonizeSubstitutesAndKeepsTheLength`, `TuneBuilder::transposeMovesChordsKeyAndAbsoluteMelodyAndIsReversible`, `TuneBuilder::modalShiftMovesDiatonicChordsAndOptionallyTheMelody` | `verified` |
| TB-6-01 | Bass track: off, root, root-fifth, walking, genre, manual; through bass mode or separate MIDI output | tune-builder §6 | bass modes in `TuneMelody`; the engine plays the bass line only when the guitar is a bass, MIDI out otherwise (`serviceTune`) | `TunePlayer::theBassGoesToTheEngineOnlyForABass`, `TuneBuilder::bassLinesFollowTheChordsAndWalkIntoTheNextRoot` | `partial` - bass editor UI (6) deferred |
| TB-7-01 | Layers pad, arpeggio, countermelody, percussion (chuck/palm-mute noise) with on/off, volume, pan | tune-builder §7 | `TuneLayer` types; countermelody generator; layers on their own channels | `TuneBuilderTests.cpp` `TuneBuilder::countermelodyStaysUnderTheMelodyOnChordTones` | `partial` - pad, arpeggio, percussion output not tested |
| TB-8-01 | Playback through the live rhythm/note engines with every realism detail; per-section state boundary; loop plays setlist | tune-builder §8 | processor wiring (DECISIONS "TUNE in the plugin"); state boundary resets the rhythm engine and mod envelopes | `TuneProcessor::theMelodySoundsWhileTheRhythmEngineStrums`, `TunePlayer::loopingWrapsOnTheSampleWithTheEndBeforeTheStart`, `TuneBuilder::aLoopedTuneRunsSixtySecondsWithoutDrift` | `partial` - state-boundary reset not asserted at processor level |
| TB-9.1-01 | Audio export WAV/FLAC/MP3, 16/24/32f, rate, stems per aux 1-8, loop tail 0-5 s, `Renders/` | tune-builder §9.1 | `AudioExporter` has WAV/AIFF/FLAC, no stems/MP3 | none | `pending` |
| TB-9.2-01 | MIDI export via midi-export profiles and track splits | tune-builder §9.2 | `buildTuneMidiFile` with its own track splits, not `MidiProfiles` (C-53) | `TuneBuilderTests.cpp` `TuneBuilder::theMidiFileHasAMetaTrackInstrumentTracksAndBeatAccurateTicks` | `partial` - no Luthier profile |
| TB-9.3-01 | Notation export with section headings, chord symbols, tab/standard | tune-builder §9.3 | `buildTuneScore` feeds the notation exporters; no dialog | `TuneBuilderTests.cpp` `TuneBuilder::thePerformanceScoreKeepsSectionsChordSymbolsAndFrettedNotes` | `partial` - dialog not built |
| TB-9.4-01 | Project export `.luthiertune`, optional bundled preset and guitar | tune-builder §9.4 | `.luthiertune` save; preset and guitar bundling not built | `TuneBuilderTests.cpp` `TuneBuilder::saveIsAtomicAndKeepsADatedBackup` | `partial` |
| TB-10-01 | Ten templates (Blank .. Instrumental fingerstyle) | tune-builder §10; FC | `TuneTemplateLibrary`, `Resources/Tunes/Templates/01-10` | `TuneBuilderTests.cpp` `TuneBuilder::theTenTemplatesLoadInOrderAndAreValid`, `TuneBuilder::templateFilesAreInCanonicalFormAndBlankMatchesTheBuiltIn` | `verified` |
| TB-11-01 | `.luthiertune` JSON schema (schema, meta, sections, chords, melody, setlist) | tune-builder §11; FF | `Source/Tune/TuneFile.*` | `TuneBuilderTests.cpp` `TuneBuilder::aHundredRandomTunesRoundTripByteIdentical`, `TuneBuilder::loadErrorsAreNamedAndLeaveTheTuneAlone`, `TuneBuilder::tenThousandCorruptedFilesLoadOrRefuseCleanly`, `TuneBuilder::saveIsAtomicAndKeepsADatedBackup` | `verified` |
| TB-12-01 | Standalone opens last tune; MIDI in armed; audio in for hum | tune-builder §12 | the tune is in the plugin state (restored on relaunch); Record arms MIDI in | `TuneProcessor::thePluginStateKeepsTheTuneAndTheClickRoute` | `partial` - hum capture (13) not built; standalone relaunch not run |
| TB-13-01 | Sung/hummed capture: mono pitch tracker, confidence 0.6, snap to key; opt-in "Sing" button | tune-builder §13 | not built; referenced "Bend Trainer" pitch detector does not exist in practice-tools | none | `pending` |
| TB-14-01 | Mod routes over timeline; snapshots capture section state; looper captures tune render | tune-builder §14 | not built | none | `pending` |
| TB-UI-01 | New Tune shortcut `Ctrl+T` | GI 17; GAPS A5 | `TunePanel::keyPressed` opens the template menu while the tab has focus; not in the shortcut registry | none | `partial` - not in the registry, not tested |
| TB-15-01 | Test: 100 shorthand strings parse; malformed named errors | tune-builder §15 | - | `TuneBuilderTests.cpp` `TuneBuilder::aHundredGeneratedProgressionsRoundTripThroughShorthand`, `TuneBuilder::malformedShorthandIsRefusedWithANamedError` | `verified` |
| TB-15-02 | Test: auto melody deterministic 1000 runs | tune-builder §15 | - | `TuneBuilderTests.cpp` `TuneBuilder::autoMelodyIsByteIdenticalForASeedAcrossAThousandRuns` | `verified` |
| TB-15-03 | Test: locked notes untouched by regenerate | tune-builder §15 | - | `TuneBuilderTests.cpp` `TuneBuilder::regenerateLeavesLockedNotesByteIdentical` | `verified` |
| TB-15-04 | Test: 1000 section reorders preserve length and positions | tune-builder §15 | - | `TuneBuilderTests.cpp` `TuneBuilder::aThousandSectionReordersKeepTheLengthAndEveryNotesPosition` | `verified` |
| TB-15-05 | Test: 32-bar loop 60 s without drift, within 1 sample | tune-builder §15 | timeline level, not an engine render | `TuneBuilderTests.cpp` `TuneBuilder::aLoopedTuneRunsSixtySecondsWithoutDrift` | `partial` - engine playback not wired |
| TB-15-06 | Test: 100 random tunes byte-identical round trip | tune-builder §15 | - | `TuneBuilderTests.cpp` `TuneBuilder::aHundredRandomTunesRoundTripByteIdentical` | `verified` |
| TB-15-07 | Test: offline vs live render within -80 dBFS | tune-builder §15 | - | none | `pending` |
| TB-15-08 | Test: Luthier MIDI export re-import byte-identical audio | tune-builder §15 | - | none | `pending` |
| TB-15-09 | Test: hum fixture 95% semitone-correct | tune-builder §15 | - | none | `pending` |
| TB-15-10 | Test: standalone relaunch reloads last tune | tune-builder §15 | - | none | `pending` |

## 25. ambiguity-resolutions.md (phase 4)

Precedence 4 on the seven items it resolves. Built: the physical feedback
loop (1, `14864ca`), Freeze (2.1), the E-Bow on the per-string injection
(2.2, `9c672aa`), the Doubler pedal (3, `f904956`), the capo (4.5) and the
guitar migration table (7, `5ccf238`). Open (TODO 2d): voicer `Bass` style,
transition bonus and 4.7 tests, preset morph (5), crossing velocity (6),
and 8's snapshot-cancels-morph and Aux 1 toggle.

| ID | Requirement (short) | Source (file §section) | Implementation location | Verification | Status |
|---|---|---|---|---|---|
| AMB-1.1-01 | Feedback as physical per-string loop: `k_couple(s) x H_cab_to_pickup(f) x amp_out`, one-block delay, post-circuit | ambiguity §1.1 | `Source/DSP/Feedback/FeedbackLoop.*` (DECISIONS "Feedback is the physical loop") | `FeedbackTests.cpp` `Feedback::aLoudRigTakesOverAndACleanOneDoesNot`, `Feedback::eachStringHearsItsOwnNote` | `verified` |
| AMB-1.2-01 | Params feedback_amount 0, distance 0.5 m, angle 0, focus 60%, octave_bias 0; zero amount bypassed and free | ambiguity §1.2 | `feedback_amount`, `feedback_distance`, `feedback_angle`, `feedback_focus`, `feedback_octave_bias`; old `feedback_on` presets load at 50% | `FeedbackTests.cpp` `Feedback::zeroAmountIsBitIdenticalWhateverTheOtherSettings`, `Feedback::oldPresetsThatSwitchedItOnGetAnAmount` | `verified` |
| AMB-1.3-01 | SUSTAIN card feedback row with five params and resonance LED | ambiguity §1.3 | `AdvancedPanel` feedback row: five knobs and `FeedbackLed` (`Widgets.cpp`) | none (no UI test) | `implemented` |
| AMB-1.4-01 | Tests: zero bypass bit-identical; 60 s stability at 100%; peak within 5 c of note; volume-5 attenuation within 0.5 dB | ambiguity §1.4 | - | `FeedbackTests.cpp` `Feedback::zeroAmountIsBitIdenticalWhateverTheOtherSettings`, `Feedback::staysBoundedForAMinuteAtFullTilt`, `Feedback::eachStringHearsItsOwnNote`, `Feedback::theVolumeKnobLowersTheLoopByTheCircuitsAttenuation` | `verified` |
| AMB-2.1-01 | Freeze captured loop 200-1000 ms (400), level -6 dB, attack 5-500, release 20-2000, LP/HP; new freeze replaces | ambiguity §2.1 | `Source/DSP/Master/FreezeOverlay.*`; params `Parameters.h:137-143` | `SustainTests.cpp` `Sustain::freezeLoopRepeatsExactly`, `Sustain::aSecondFreezeReplacesTheFirst`, `Sustain::releaseFadesOutAndStops`, `Sustain::anIdleFreezeLeavesTheAudioAlone` | `verified` |
| AMB-2.2-01 | E-Bow via the section 1 feedback path: enable, string mask, intensity 50% (maps to feedback_amount), harmonic | ambiguity §2.2 | `Source/DSP/Feedback/EBowDriver.*`: each string driven through section 1's per-string narrowband injection (Q 30); `ebow_enable`, `ebow_string_mask` (0 = held strings), `ebow_intensity` 50%, `ebow_harmonic`; own intensity, not feedback_amount (C-54) | `EBowTests.cpp` `EBow::aHeldNoteIsSteadyWithinHalfASecondAtHalfIntensity`, `EBow::heldStringsLetGoOnReleaseChosenStringsDoNot`, `EBow::theHarmonicChoiceTakesTheString` | `verified` - as amended (C-54) |
| AMB-2.3-01 | SUSTAIN card rows Freeze and E-Bow | ambiguity §2.3 | SUSTAIN section: Freeze row; E-Bow toggle, `StringMaskSelector`, intensity knob, harmonic choice (`9c672aa`) | none (no UI test) | `implemented` |
| AMB-2.4-01 | Test: freeze 60 s RMS within 0.5 dB | ambiguity §2.4 | - | `SustainTests.cpp` `Sustain::freezeLayerHoldsItsLevelForASixtySecondHold` | `verified` |
| AMB-2.4-02 | Test: E-Bow steady state within 500 ms at 50%, silent within 200 ms of disable | ambiguity §2.4 | new `Silenced` damping (absolute 80 ms T60) in `StringEngine` | `EBowTests.cpp` `EBow::aHeldNoteIsSteadyWithinHalfASecondAtHalfIntensity`, `EBow::switchingItOffSilencesTheStringWithin200ms` (suite green at `9c672aa`) | `verified` |
| AMB-3-01 | Doubler params: enable, delay 5-40 (22), pitch ±25 c (-8), pan ±0.7, width, mix 40%, HP 100, LP 8k | ambiguity §3 | `PedalType::Doubler` (`PedalsMod.*`): delay, pitch, pan, width, mix, HP, LP; enable is the slot bypass (DECISIONS "The doubler is `PedalType::Doubler`") | `DoublerTests.cpp` `Doubler::mixZeroIsTheDrySignal`, `Doubler::mixFullIsACopyTwentyTwoMillisecondsLate`, `Doubler::panPutsTheTakesToTheSides` | `implemented` - pitch, HP, LP defaults and ranges not asserted |
| AMB-3-02 | Doubler post-amp pre-cab, as an always-available post-rack pedal | ambiguity §3, §3.1 | post-amp rack pedal before the cabinet; old engine doubler removed; `doubler_on` presets get the pedal in the first free post-amp slot | `DoublerTests.cpp` `Doubler::isAPostAmpRackPedal`, `Doubler::presetsWithTheOldDoublerGetThePedal` | `verified` |
| AMB-3.2-01 | Tests: mix 0 null within -80 dBFS; mix 100 delay 22 cross-correlation | ambiguity §3.2 | - | `DoublerTests.cpp` `Doubler::mixZeroIsTheDrySignal`, `Doubler::mixFullIsACopyTwentyTwoMillisecondsLate` (suite green at `f904956`) | `verified` |
| AMB-4.1-01 | Voicer constraints 1-6 incl. barre reachability | ambiguity §4.1 | `ChordVoicer` | `ModelTests.cpp` `ChordVoicer::commonChordsAreVoicedPlayably` | `implemented` |
| AMB-4.2-01 | Score terms incl. hand_move_penalty 0.4, dup_note_penalty 1, extension_dropped 3 | ambiguity §4.2 | `ChordVoicer.cpp` | none | `implemented` - weights not asserted |
| AMB-4.3-01 | Style bias values per style incl. `Bass` style | ambiguity §4.3 | `VoicingStyle` has no `Bass` entry (`RhythmEngine.h:61`) | none | `partial` |
| AMB-4.4-01 | Transition bonus (+2 common note, +1 common position, -2 jump > 5) | ambiguity §4.4 | not found (rg `transition` in `ChordVoicer.cpp`) | none | `pending` |
| AMB-4.5-01 | Capo raises minimum fret; open strings are capo'd notes | ambiguity §4.5 | `capo_fret` in `TuningEngine` (GAPS B1) | `GenreKitTests.cpp` `GenreKits::capoRemovesFretsBelowItAndMovesThePitch` | `verified` |
| AMB-4.5-02 | Partial capo from Workshop capo part's string mask | ambiguity §4.5 | `TuningEngine` capo mask (`d8893b5`) | `WorkshopPresetTests.cpp` `WorkshopCapo::aPartialCapoClampsOnlyItsStrings` - green at `0d225f0` | `verified` |
| AMB-4.6-01 | Determinism with tie-breaks (string count desc, fret sum asc) | ambiguity §4.6 | `ChordVoicer` | none | `implemented` - tie-break order not asserted |
| AMB-4.7-01 | Tests: 84 templates every key and style; I-IV-V-I travel <= 3 frets; determinism; BEAD bass C7 root / root-fifth | ambiguity §4.7 | - | `RhythmSchedulerTests.cpp` `RhythmPatterns::voicerHandlesEveryChordOnEveryGuitar` (templates) | `partial` - travel, determinism, bass cases missing |
| AMB-5.1-01 | Preset morph: continuous interpolate; discrete, structural and guitar switch at 0.5 | ambiguity §5.1 | `Source/Presets/PresetMorph.*`; processor timer `updatePresetMorph` | `PresetMorphTests.cpp` `PresetMorph::theEndsAreThePresetsThemselves` (null < -100 dB at 0 and 1), `theMidpointSwitchesDiscretesAndHalvesTheRest`, `aFourSecondSweepDoesNotClick` | `verified` |
| AMB-5.2-01 | Preset browser Morph toggle, A/B slots, slider; automatable `preset_morph_position` | ambiguity §5.2 | `Overlays.cpp` `PresetBrowserPanel` morph row; param 402 | `PresetMorph::theBrowserMorphRowFillsTheSelectedSlot`, `thePositionIsNotPartOfAPreset` | `verified` |
| AMB-5.3-01 | Tests: 0 = A, 1 = B, 0.5 midpoint/switched, 4 s automation click-free | ambiguity §5.3 | `PresetMorphTests.cpp` | `PresetMorph::theEndsAreThePresetsThemselves`, `theMidpointSwitchesDiscretesAndHalvesTheRest`, `aFourSecondSweepDoesNotClick` (steepest step within 2x either preset alone) | `verified` |
| AMB-6-01 | Crossing velocity from pattern `crossing_sps`, else global default; progression looper uses pattern; MPE passes through | ambiguity §6 | depends on strum-dynamics (not built) | none | `pending` |
| AMB-6.1-01 | Tests: pattern crossing_sps within 1 sample; 220 sps -> 22.7 ms | ambiguity §6.1 | - | none | `pending` |
| AMB-7-01 | Old guitar names resolved via `Resources/Guitars/migration.json` (versioned) | ambiguity §7 | `Resources/Guitars/migration.json` (magic `luthier.guitar-migration`, schema 1, version 2: `renamed`, `names`), read by `PartLibrary` | `GuitarMigrationTests.cpp` `GuitarMigration::everyPreM49NameResolvesToItsShippedGuitar`, `GuitarMigration::aPresetNamingAnOldGuitarLoadsItsReplacement` | `verified` |
| AMB-7-02 | Unresolved -> factory default + banner text; preset params still apply | ambiguity §7 | fallback to the type's factory guitar with 7's banner text verbatim | `GuitarMigrationTests.cpp` `GuitarMigration::anUnknownGuitarKeepsThePresetAndSaysSo`; `WorkshopPresetTests.cpp` `WorkshopPresets::aMissingGuitarFileFallsBackToItsType` | `verified` |
| AMB-7.1-01 | Tests: every pre-M49 factory guitar name resolves; unknown name loads with banner | ambiguity §7.1 | - | `GuitarMigrationTests.cpp` `GuitarMigration::everyPreM49NameResolvesToItsShippedGuitar`, `GuitarMigration::anUnknownGuitarKeepsThePresetAndSaysSo` (suite green at `5ccf238`) | `verified` |
| AMB-8-01 | Feedback / freeze / E-Bow are modulation destinations; snapshot recall cancels preset morph; Aux 1 pre/post-circuit toggle | ambiguity §8 | every APVTS parameter is a mod destination (`ModMatrix.h`), so `feedback_*`, `freeze_*`, `ebow_*` qualify; a snapshot recall cancels the preset morph (`PresetMorph::aSnapshotRecallCancelsTheMorph`); no Aux 1 toggle | none | `partial` - destinations not tested; Aux 1 toggle pending |

## 26. gui-integration.md (phase 4, master GUI spec)

Precedence 2 (UI location and access). Its section 19 is the definition of
done for the UI (BRIEF "When you are done"). GAPS.md A1-A6 record earlier
audits; rows below re-check them against the code at `173a292`.

### Ground rules, window, header, Easy mode

| ID | Requirement (short) | Source (file §section) | Implementation location | Verification | Status |
|---|---|---|---|---|---|
| GI-0-01 | Every automatable parameter has exactly one canonical control | GI §0.1 | panels across `Source/UI` | none (GI-22-02 not built) | `partial` - several params have no control (e.g. `fineTuneCents`, per-string detune not automatable) |
| GI-0-02 | Every feature within three interactions of the header | GI §0.2 | - | none | `partial` - unbuilt tabs make some features unreachable |
| GI-0-03 | Easy Mode never hides an audible behaviour | GI §0.3 | Easy layout is the old three-band one (TODO 2e) | none | `partial` |
| GI-0-04 | Nothing only by right-click | GI §0.4 | MIDI Learn header button; Modulate in MOD tab | `EditorTests.cpp` `Editor::rightClickOffersModulationAndBuildsTheRoute` | `implemented` |
| GI-0-05 | Fixed panel layout, no docking | GI §0.5 | - | none | `implemented` |
| GI-0-06 | Every panel header: name, accent bar, status line, collapse chevron; collapse state per preset | GI §0.6 | section headers drawn; no status line or chevron found | none | `pending` |
| GI-0-07 | Panels present or absent, never greyed (no whammy -> no WHAMMY panel) | GI §0.7 | SLIDE group hides/shows | `SlideTests.cpp` `SlideUi::theSlideGroupAppearsWithSlideModeAndTheTabFitsIt` | `partial` - WHAMMY panel conditional not verified |
| GI-0-08 | Minimum 1280x800 design size, reflow below, cap 2560x1600 | GI §0.8, §13 | `PluginEditor.h:34-35` (940x560 min), `PluginEditor.cpp:106` (max 2400x1440) | `EditorTests.cpp` `Editor::itLaysOutAndPaintsAcrossItsResizeRange` | `partial` - cap 2400x1440 vs 2560x1600 |
| GI-0-09 | Advanced values marked (warning arc, `*`, tab padlock) | GI §0.9, §21 | `RangesUi` | `RangeTests.cpp` `Ranges::markingFollowsTheValueNotTheMode` | `verified` |
| GI-1-01 | Window: header (32 px), main, live strip (32), practice drawer (32-360), footer (16) | GI §1 | header 48 px (`Theme.h:74`), footer 18 (`Theme.h:75`) | `EditorTests.cpp` `Editor::theModesThatChangeTheLayoutTakeEffectAndUndoThemselves` | `partial` - heights differ (Conflict C-10) |
| GI-2-01 | Header regions: brand + notch, preset controls, mode + wrench + slide glyph, snapshot strip, meters (in/out/LED), utility (panic, learn, tap, kill), overflow (gear, help, dice, reset) | GI §2 | `HeaderBar.h:84-102`: LED, guitar/tuning selectors, preset prev/name/next, padlock, File, A/B/A>B, undo/redo, panic, Learn, ?, mode, Live, Slide | none | `partial` - no wrench, header snapshot strip, input meter, tap, gear/dice/reset icons |
| GI-2-02 | Header collapses below 1280 (numeric snapshot readout, three-dot overflow) | GI §2 | not found | none | `pending` |
| GI-2-03 | A/B compare transient, not serialised | GI §2 | `compareA/B` | none | `implemented` |
| GI-2-04 | Range padlock beside preset name opens Options -> Ranges | GI §2 | `rangePadlock` `HeaderBar.h:90` | `RangesUiTests.cpp` `RangesUi::theHeaderPadlockShowsOnlyWhenSomethingIsUnlocked` | `verified` |
| GI-3-01 | Easy layout: illustration + rig strip (280 px) + playing, tone, rhythm strips | GI §3 | `EasyPanel` (`0052205`) | `EasyLayoutTests.cpp` `EasyLayout::theWindowMatchesSection3` | `verified` |
| GI-3.1-01 | Illustration hit regions: headstock tuning popover (tuning, capo, temperament), pickup select, bridge whammy popover (only if fitted), fretboard notes | GI §3.1 | `GuitarBodyComponent.cpp`; headstock and bridge popovers | `EditorTests.cpp` `Editor::everyHitRegionOnTheIllustrationDescribesItself`, `Editor::theHeadstockPopoverEditsPerStringTuning`, `Editor::theBridgePopoverAppearsOnlyWhenAWhammyIsFitted` | `verified` |
| GI-3.1-02 | Illustration live-rendered from the (parts) GuitarSpec | GI §3.1 | `GuitarBodyComponent` draws with `GuitarRenderer` (`a406915`) | `GuitarRendererTests.cpp` `GuitarIllustration::everyFactoryGuitarHasItsParts` | `verified` |
| GI-3.2-01 | Rig strip cards: circuit (vol, tone, visualiser mini), pre rack, amp, post rack, cab, room; slots open pedal popovers | GI §3.2 | `EasyPanel` rig strip: guitar volume / tone, `CircuitResponseView`, compact pre / post racks, amp, cab + mics, room | `EasyLayoutTests.cpp` `EasyLayout::theWindowMatchesSection3` (controls on screen) | `implemented` - pedal popovers from rack slots not tested |
| GI-3.3-01 | Playing strip: mode, humanize macro, character macro, whammy display | GI §3.3 | `EasyPanel` playing strip; `macro_character` parameter (DECISIONS); five older macros kept (C-52) | `EasyLayoutTests.cpp` `EasyLayout::theWindowMatchesSection3`, `EasyLayout::theCharacterMacroIsTheCharacterAmount` | `verified` |
| GI-3.4-01 | Tone strip: input gain, output gain, wet/dry post-master, stereo width | GI §3.4 | `input_gain`, `output_mix`, `stereo_width` parameters; wet/dry before the limiter (C-51) | `EasyLayoutTests.cpp` `EasyLayout::theToneStripIsHeard`, `EasyLayout::theWindowMatchesSection3` | `verified` - as amended |
| GI-3.5-01 | Rhythm strip: kit + dice, Feel, enable, chord and next-strum readout | GI §3.5 | `EasyPanel` rhythm strip: kit, dice, feel, on/off, readout | none (the layout test does not check this strip's controls) | `implemented` |
| GI-3.6-01 | Easy intentionally omits the listed deep panels | GI §3.6 | - | - | `n/a` - constraint |

### Advanced mode, Options, Workshop, Slide, strips

| ID | Requirement (short) | Source (file §section) | Implementation location | Verification | Status |
|---|---|---|---|---|---|
| GI-4-01 | Four columns, independently scrollable; columns 1-3 fixed; column 4 tabbed | GI §4 | `AdvancedPanel::buildColumn1/2/3`, `buildWorkspace` | `EditorTests.cpp` `Editor::everyWorkspaceTabSelectsAndPaints` | `verified` |
| GI-4.1-01 | Col 1 GUITAR: library (user + factory Guitars), tuning, capo, temperament, scale readout, "Open in Workshop" | GI §4.1 | GUITAR section (guitar type choice, tuning, capo, temperament) | none | `partial` - no user guitar library list, scale readout or Workshop button |
| GI-4.1-02 | Col 1 BODY: model, dimensions, woods, bracing, air readout, link to CHARACTER | GI §4.1 | BODY section | none | `implemented` |
| GI-4.1-03 | Col 1 STRINGS: material and gauge per string, age, tension readout | GI §4.1 | STRINGS section (global material/gauge) | none | `partial` - not per string |
| GI-4.1-04 | Col 1 WHAMMY: type, range, spring, dive/up stops; absent without a whammy | GI §4.1 | WHAMMY section | none | `implemented` |
| GI-4.2-01 | Col 2 PICKUPS: selector, per-pickup gain and phase; "Edit in Workshop" | GI §4.2 | PICKUPS section | none | `partial` - per-pickup phase and Workshop jump not found |
| GI-4.2-02 | Col 2 CIRCUIT replaces CABLE | GI §4.2 | `CircuitPanel` | none | `implemented` |
| GI-4.2-03 | Col 2 PRE-FX: 8 slots, drag reorder, click controls, right-click bypass/delete | GI §4.2 | `PedalRack.cpp` | `EngineTests.cpp` `Effects::chainReordersWithoutGlitching` (engine) | `implemented` |
| GI-4.3-01 | Col 3 AMP (model, tone, sag, bright, bias, master), POST-FX, CAB (mics, blend, phase, delay), ROOM, SUSTAIN (Freeze row, E-Bow row, feedback LED) | GI §4.3 | column 3 sections; SUSTAIN has the Freeze row, the E-Bow row (enable, strings, intensity, harmonic) and the feedback row with `FeedbackLed`; Doubler in the POST-FX rack | none | `partial` - no sag/bias controls |
| GI-4.4-01 | Col 4 tab order WORKSHOP, MOD, RHYTHM, TUNE, LIVE, ROUTING, TONE MATCH, CHARACTER, PRACTICE, NOTATION, MIDI OUT, CONTROLLERS, HELP | GI §4.4; BRIEF rules | `AdvancedPanel` builds 10 in the fixed order: WORKSHOP, MOD, RHYTHM, LIVE, ROUTING, TONE MATCH, CHARACTER, NOTATION, MIDI OUT, CONTROLLERS | `EditorTests.cpp` `Editor::everyWorkspaceTabSelectsAndPaints`; `MidiOutPanelTests.cpp` `MidiOutPanel::theTabSitsInTheFixedOrderAndIsRememberedByName` | `partial` - TUNE, PRACTICE, HELP (and TECHNIQUES, C-32) missing |
| GI-4.4-02 | WORKSHOP tab (bench over cols 3+4, col 4 strip stays visible) | GI §4.4 | `WorkshopPanel` (`d45fcd6`) | `WorkshopPanelTests.cpp` `WorkshopPanel::itPaintsAndTheWorkshopTabTakesOverColumnsThreeAndFour` | `verified` |
| GI-4.4-03 | TUNE tab | GI §4.4 | not built | none | `pending` |
| GI-4.4-04 | LIVE tab incl. expression calibration | GI §4.4 | `LivePanel`; calibration stays in Options (GAPS A2; Conflict C-11) | `EditorTests.cpp` `Editor::theLiveTabEditsTheSnapshotBankAndTheSetlist` | `verified` - calibration placement per C-11 |
| GI-4.4-05 | ROUTING tab incl. Aux 8 | GI §4.4 | `RoutingPanel` with the Aux 8 strip (`a901c72`) | `PluginBusTests.cpp` `PluginBuses::aux8CarriesThePlayingNoiseAndObeysItsStrip`; `EditorTests.cpp` `Editor::everyWorkspaceTabSelectsAndPaints` | `verified` |
| GI-4.4-06 | CHARACTER tab + STRING NOISE, PICK, SETUP, SLIDE, CIRCUIT groups | GI §4.4 | `CharacterPanel` with noise, pick, setup, slide groups; no CIRCUIT mirror | `NoiseTests.cpp` `NoiseUi::theCharacterTabCarriesAPadlockWhenUnlocked` | `partial` - CIRCUIT group missing |
| GI-4.4-07 | PRACTICE tab | GI §4.4 | `PracticeSetupPanel` in the workspace | `EditorTests` `everyWorkspaceTabSelectsAndPaints` | `verified` |
| GI-4.4-08 | NOTATION tab | GI §4.4 | `NotationPanel` (`18a1396`) | `EditorTests.cpp` `Editor::everyWorkspaceTabSelectsAndPaints`; `NotationPanelTests.cpp` `NotationTab::*` | `verified` |
| GI-4.4-09 | MIDI OUT tab | GI §4.4 | `MidiOutPanel` (`a357142`) | `MidiOutPanelTests.cpp` `MidiOutPanel::*` | `verified` |
| GI-4.4-10 | CONTROLLERS tab | GI §4.4 | `ControllersPage` | `EditorTests.cpp` `Editor::everyWorkspaceTabSelectsAndPaints` | `verified` |
| GI-4.4-11 | HELP tab with live shortcut cheat sheet | GI §4.4 | Help is an F1 overlay only | none | `pending` |
| GI-4.4-12 | Last-used tab persists user-globally | GI §4.4 | `UiPreferences` `config/ui.json` | `EditorTests.cpp` `Editor::theWorkspaceTabWrapsAndIsRemembered` | `verified` |
| GI-4.5-01 | Widths 260 / min 220 / col 4 min 480; stack cols 2+3 below 1280; Advanced unavailable below 1000 with notice | GI §4.5, §13 | `AdvancedPanel` layout; `InlineNotice` | `EditorTests.cpp` `Editor::advancedModeIsRefusedBelowItsMinimumWidth` | `verified` |
| GI-5-01 | Options overlay (`Ctrl+,`), modal, 11 tabs in order | GI §5 | `Overlays.cpp:787-797` (11 tabs incl. RANGES) | `EditorTests.cpp` `Editor::everyOptionsPageSelectsAndPaints` | `verified` |
| GI-5-02 | AUDIO (standalone device, buffer, rate, sidechain in) | GI §5 | `AudioPage` | none | `implemented` |
| GI-5-03 | MIDI (standalone input picker, virtual MIDI out) | GI §5 | `MidiPage` | none | `implemented` - virtual out not verified |
| GI-5-04 | APPEARANCE: accent tint, palette, reduced motion, scale, tooltips, data-stream toggle, noise-strip toggle | GI §5 | `AppearancePage`; accent tint / data-stream / noise-strip toggles say "not built" (GAPS A3) | none | `partial` |
| GI-5-05 | ACCESSIBILITY, LOCALIZATION, EXPRESSION, RANGES, UPDATES (changelog viewer), PRIVACY pages | GI §5 | pages in `OptionsPages.cpp` | `RangesUiTests.cpp` `RangesUi::theRangesPageListsLocksAndClamps` | `partial` - changelog viewer shows links only |
| GI-5-06 | DIAGNOSTICS: debug, hard reset, troubleshooting export, crash log, session recorder toggle, Workshop/Slide/ranges mirror | GI §5 | `DiagnosticsPage` (mirror says not built, GAPS A3) | none | `partial` |
| GI-5-07 | FILE LOCATIONS incl. Guitars and Parts folders | GI §5 | `FileLocationsPage` (Guitars/Parts missing per GAPS A3; recheck now Workshop exists) | none | `partial` |
| GI-5-08 | Save-on-change; Escape closes | GI §5 | - | `EditorTests.cpp` `Editor::everyOverlayShortcutOpensItsOwnOverlayAndEscapeClosesIt` | `implemented` |
| GI-6-01 | Workshop bench entry via wrench or tab; layout; interactions; 8 A/B; undo; Alt-hover; Easy overlay | GI §6 | header wrench, WORKSHOP tab, Easy overlay (`d45fcd6`); see workshop-ui section 20 | `WorkshopPanelTests.cpp` `WorkshopPanel::*` | `partial` - see WUI rows |
| GI-7-01 | Slide Mode header toggle: SLIDE group, Workshop Slide category, bar overlay in fretboard and illustration, tuning popover glide target, squeak suppressed, `S` | GI §7 | header toggle, SLIDE group, fretboard overlay, `S` | `SlideTests.cpp` `SlideUi::theSlideGroupAppearsWithSlideModeAndTheTabFitsIt` | `partial` - Workshop category, illustration bar, popover glide target missing |
| GI-8-01 | Header snapshot strip: prev, 8 buttons, next, bank selector; colour tag + 12-char label; accent outline; click load / Shift-click write / right-click rename, colour, clear | GI §8 | snapshot strip lives in the Live strip, not the header | none | `partial` |
| GI-9-01 | Live strip contents and 44 px / tooltip / Advanced lock rules | GI §9 | `LiveStrip.cpp` | see LIVE-10-* | `partial` |
| GI-10-01 | Practice drawer collapsed strip and eight tabs | GI §10 | `PracticePanel` | see PRA-9-* | `implemented` |
| GI-11.1-01 | Mod arcs 4 px outside value arc, 2 px, segmented by source colour | GI §11.1 | `Widgets.cpp:562-600` single colour | none | `partial` |
| GI-11.2-01 | Drag source card onto control creates 25% route; Escape cancels; right-click Modulate submenu | GI §11.2 | right-click only (route at 33% depth) | `EditorTests.cpp` `Editor::rightClickOffersModulationAndBuildsTheRoute` | `partial` - drag absent; depth 0.33 vs 25% (Conflict C-13) |
| GI-12-01 | Footer 16 px: version left, status centre, CPU % and voice count right; data stream in main area; static count under reduced motion | GI §12 | `PluginEditor.cpp:319-331` (version, CPU, latency) | none | `partial` - no status line or voice count |
| GI-13-01 | Reflow: Easy below 900 turns rig strip into a tab; scale >125% auto-picks smaller scale with one-time notice | GI §13 | not found | none | `pending` |
| GI-14-01 | Empty-state hints (snapshot slot, no mod routes, empty setlist, no backing track, non-slide guitar, bass inactive, stock-range edit) | GI §14 | slide and range hints built; others not verified | `SlideTests.cpp` `SlideUi::theSlideGroupAppearsWithSlideModeAndTheTabFitsIt` | `partial` |
| GI-15-01 | Banners under header, 32 px, dismissible, 5 s auto-dismiss unless actionable | GI §15 | `Source/UI/Notifications.cpp` `NotificationCentre` | `EditorTests.cpp` `Editor::notificationBannersQueueDismissAndRespectTheirActions` | `verified` |
| GI-15-02 | Triggers: preset error, missing IR, missing guitar, missing part (jump to Workshop), sample-rate change, update, policy, range clamp on save, crash, licence grace | GI §15 | all but range-clamp wired; missing part/guitar via `takeGuitarNotices` (`PluginEditor.cpp:908`) | `EditorTests.cpp` `Editor::aFailedPresetLoadAndAMissingIrEachRaiseABannerOnce`, `Editor::aSampleRateChangeIsAnnouncedOnceAndTheFirstOneIsNot`, `Editor::theWindowRaisesSectionFifteensTriggersAndIsQuietWhenItShould` | `partial` - range-clamp banner replaced by in-place report (DECISIONS); missing-part banner untested; no Workshop to jump to |
| GI-16-01 | Control menu 13 items: value, reset, copy/paste, MIDI Learn, Assign to macro, Modulate, unlock/restrict range, Automation ID, Show in shortcuts | GI §16 | `Widgets.cpp:39-80` has value, reset, copy/paste, Learn, Lock, Randomise, Modulate; range items via `RangesUi` | `EditorTests.cpp` `Editor::rightClickOffersModulationAndBuildsTheRoute`; `RangesUiTests.cpp` `RangesUi::rightClickUnlocksAndRestrictsOneControl` | `partial` - no Assign to macro, Automation ID, Show in shortcuts |
| GI-16-02 | Panel menu: collapse, reset panel, screenshot to clipboard, Docs | GI §16 | not found | none | `pending` |
| GI-17-01 | Shortcut table defaults, all rebindable | GI §17 | registry `Accessibility.cpp:494-560` | `AccessibilityTests.cpp` `Accessibility::shortcutDefaultsMatchTheCanonicalTable`, `Accessibility::noTwoShortcutsShareADefaultKey` | `partial` - W, Ctrl+T absent; Space is audition (GAPS A5); digits not in registry |
| GI-18-01 | Undo stack 64, 200 ms grouping, state boundaries needing Shift, per-instance | GI §18 | processor snapshot undo stack | `RangesUiTests.cpp` `Undo::stepsOneActionAtATimeBothWays` | `partial` - see action-and-undo |

### Section 19: feature-to-location index

One row per section 19 entry. "Present" means the primary location exists
and works; secondary access is noted.

| ID | Feature -> primary location | Implementation location | Verification | Status |
|---|---|---|---|---|
| GI-19-01 | Instrument load -> Col 1 GUITAR, preset browser; `Ctrl+O` | guitar type choice; preset browser overlay | `IntegrationTests.cpp` `Engine::everyGuitarTypeLoadsAndSounds` | `partial` - user Guitars folder not listed |
| GI-19-02 | Save As Guitar -> Workshop header; `Ctrl+G` | `Ctrl+G` dialog; "Save As Guitar" in the Workshop header | `WorkshopPresetTests.cpp` `WorkshopPresets::saveAsGuitarWritesAFileAndPointsThePresetAtIt` | `implemented` |
| GI-19-03 | Per-string tuning -> Col 1 GUITAR; Easy headstock | headstock popover only | `EditorTests.cpp` `Editor::theHeadstockPopoverEditsPerStringTuning` | `partial` - no Col 1 per-string control |
| GI-19-04 | Capo (fret / partial) -> Col 1 GUITAR, Workshop capo drag | `capo_fret` in Col 1; partial via capo part (untested) | `GenreKitTests.cpp` `GenreKits::capoRemovesFretsBelowItAndMovesThePitch` | `partial` |
| GI-19-05 | Temperament -> Col 1 GUITAR | GUITAR section | none | `implemented` |
| GI-19-06 | Body dimensions / wood / bracing -> Workshop body part; Col 1 BODY summary | Col 1 BODY only | none | `partial` |
| GI-19-07 | String material / gauge per string -> Workshop strings part; Col 1 STRINGS | Col 1 STRINGS (global) | none | `partial` |
| GI-19-08 | String age -> Col 1 STRINGS; Easy character macro | Col 1 STRINGS; Easy Character knob | `EasyLayoutTests.cpp` `EasyLayout::theCharacterMacroIsTheCharacterAmount` | `verified` |
| GI-19-09 | Whammy -> Col 1 WHAMMY, Workshop bridge; Easy bridge click | Col 1 WHAMMY; bridge popover | `EditorTests.cpp` `Editor::theBridgePopoverAppearsOnlyWhenAWhammyIsFitted` | `implemented` - Workshop bridge via the drawer |
| GI-19-10 | Pickup model / coil / magnet -> Workshop pickup part; Col 2 summary | Col 2 PICKUPS | none | `partial` |
| GI-19-11 | Pickup position -> Workshop drag | `BenchIllustration` pickup drag, live placement | `WorkshopPanelTests.cpp` `WorkshopPanel::aPickupDragIsOneEntryAndTheRulerValueFollows`, `WorkshopPanel::keyboardNudgesMatchADrag` | `verified` |
| GI-19-12 | Pickup height / tilt -> Workshop screws | `WorkshopBench` heights; bench drag on the pickup (Shift treble side, Alt bass side) | `WorkshopBenchTests.cpp` `WorkshopBench::heightsAndSetupEditsAreOneEntryEach` (model) | `implemented` |
| GI-19-13 | Pickup selector -> Col 2 PICKUPS; Easy pickup click | present | `EditorTests.cpp` `Editor::everyHitRegionOnTheIllustrationDescribesItself` | `verified` |
| GI-19-14 | Guitar volume / tone -> Col 2 CIRCUIT, Easy rig strip; CHARACTER CIRCUIT mirror | Col 2 CIRCUIT; Easy body knobs | none | `partial` - Easy rig strip has them (`0052205`); CHARACTER CIRCUIT mirror missing |
| GI-19-15 | Pots / cap / bleed / cable / active -> Col 2 CIRCUIT | `CircuitPanel` | none | `implemented` |
| GI-19-16 | Pre-effects rack -> Col 2 PRE-FX; Easy rig strip | Col 2 | `EasyLayoutTests.cpp` `EasyLayout::theWindowMatchesSection3` (Easy rig strip) | `implemented` |
| GI-19-17 | Post-effects rack -> Col 3 POST-FX; Easy rig strip | Col 3 | `EasyLayoutTests.cpp` `EasyLayout::theWindowMatchesSection3` (Easy rig strip) | `implemented` |
| GI-19-18 | Amp model / tone / sag -> Col 3 AMP; Easy rig strip | Col 3 AMP (no sag) | `EasyLayoutTests.cpp` `EasyLayout::theWindowMatchesSection3` (Easy rig strip) | `implemented` |
| GI-19-19 | Cabinet, mics -> Col 3 CAB; Easy rig strip | Col 3 | `EasyLayoutTests.cpp` `EasyLayout::theWindowMatchesSection3` (Easy rig strip) | `implemented` |
| GI-19-20 | Room -> Col 3 ROOM; Easy rig strip | Col 3 | `EasyLayoutTests.cpp` `EasyLayout::theWindowMatchesSection3` (Easy rig strip) | `implemented` |
| GI-19-21 | Feedback simulation -> Col 3 SUSTAIN readout | feedback row with `FeedbackLed` | none | `implemented` |
| GI-19-22 | Freeze -> Col 3 SUSTAIN | present | `SustainTests.cpp` `Sustain::freezeLayerHoldsItsLevelForASixtySecondHold` | `verified` |
| GI-19-23 | E-Bow -> Col 3 SUSTAIN | E-Bow row: enable, strings, intensity, harmonic (`9c672aa`) | `EBowTests.cpp` `EBow::*` (engine; the row itself untested) | `implemented` |
| GI-19-24 | Playing mode -> Easy playing strip; Adv header | Easy playing strip mode selector | `EasyLayoutTests.cpp` `EasyLayout::theWindowMatchesSection3` | `implemented` |
| GI-19-25 | MIDI Learn -> header; right-click; `Ctrl+L` | present | `IntegrationTests.cpp` `MidiLearn::armingIsSeparateFromLearningUntilAControlClaimsIt` | `verified` |
| GI-19-26 | Preset browser -> header name; `Ctrl+O` | present | none | `implemented` |
| GI-19-27 | A / B compare -> header; `Ctrl+/` | present | none | `implemented` |
| GI-19-28 | Save preset -> header; `Ctrl+S` | File menu | none | `implemented` |
| GI-19-29 | Panic -> header; `P` | present | `IntegrationTests.cpp` `Engine::panicSilencesEverything` | `verified` |
| GI-19-30 | Randomize -> header dice; `Ctrl+R` | File menu / shortcut (no dice icon) | `IntegrationTests.cpp` `Presets::randomiseRespectsLocks` | `partial` |
| GI-19-31 | Reset -> header reset; `Ctrl+Shift+R` | File menu / shortcut | `IntegrationTests.cpp` `Presets::resetRestoresDefaults` | `partial` |
| GI-19-32 | Export audio -> File menu, Options AUDIO | export overlay | none | `implemented` |
| GI-19-33 | Export MIDI -> Col 4 MIDI OUT, session drag-out; NOTATION | MIDI OUT export and drag-out (`a357142`); NOTATION exports MIDI through the same profile (`18a1396`) | `MidiOutPanelTests.cpp` `MidiOutPanel::exportWritesTheCaptureInTheChosenProfile`; `NotationPanelTests.cpp` `NotationTab::exportsEveryFormat` | `verified` - drag from the session recorder's own Save button pending (TODO 10) |
| GI-19-34 | Import MIDI -> File menu, drag onto plugin | not built | none | `pending` |
| GI-19-35 | Help -> header ?, Col 4 HELP; F1 | overlay only | `EditorTests.cpp` `Editor::everyOverlayShortcutOpensItsOwnOverlayAndEscapeClosesIt` | `partial` |
| GI-19-36 | Debug -> Options DIAGNOSTICS | present | none | `implemented` |
| GI-19-37 | Easter egg -> notch pixel | `PluginEditor.cpp:231-255` | none | `implemented` |
| GI-19-38 | Bus layout / aux 1-8 / per-string / sidechain / MIDI out -> ROUTING | ROUTING (aux 1-8, per-string buses by name) | `PluginBusTests.cpp` `PluginBuses::perStringLayoutPutsEachStringOnItsOwnBus`, `PluginBuses::aux8CarriesThePlayingNoiseAndObeysItsStrip` | `verified` |
| GI-19-39 | Aux 1 pre / post circuit toggle -> ROUTING | not found | none | `pending` |
| GI-19-40 | Aux 8 noise bus -> ROUTING | Aux 8 strip (`a901c72`) | `PluginBusTests.cpp` `PluginBuses::aux8CarriesThePlayingNoiseAndObeysItsStrip` | `verified` |
| GI-19-41 | LFOs 1-8 -> MOD LFO cards; right-click Modulate | present | `EditorTests.cpp` `Editor::rightClickOffersModulationAndBuildsTheRoute` | `verified` |
| GI-19-42 | Envelopes 1-4 -> MOD ENV cards | present | `ModulationTests.cpp` `Modulation::envelopeStageTimesAreAccurate` | `implemented` |
| GI-19-43 | Step sequencers 1-2 -> MOD STEP cards | present | none | `implemented` |
| GI-19-44 | Envelope followers 1-2 -> MOD FOLLOW cards | present | none | `implemented` |
| GI-19-45 | Macros 1-8 -> MOD MACROS; header in Live Mode | MOD panel; no header macros | none | `partial` |
| GI-19-46 | Random source -> MOD RAND card | present | none | `implemented` |
| GI-19-47 | Mod routes -> MOD route table; drag / right-click | table + right-click; no drag | `EditorTests.cpp` `Editor::rightClickOffersModulationAndBuildsTheRoute` | `partial` |
| GI-19-48 | Chord detector / voicer style / density / hand pos -> RHYTHM; Easy rhythm strip | present | `EditorTests.cpp` `Editor::everyWorkspaceTabSelectsAndPaints` | `implemented` |
| GI-19-49 | Strum pattern editor -> RHYTHM | present | none | `implemented` |
| GI-19-50 | Fingerpick pattern editor -> RHYTHM | present | none | `implemented` |
| GI-19-51 | Crossing velocity / accel / striker -> RHYTHM STRUM group; CHARACTER PICK mirror | not built | none | `pending` |
| GI-19-52 | Genre kit -> RHYTHM, Easy rhythm strip | present | `GenreKitTests.cpp` `GenreKits::applyingAKitInstallsItsPatternAndSettings` | `verified` |
| GI-19-53 | Bass step grid -> RHYTHM (bass) | not built | none | `pending` |
| GI-19-54 | Snapshots -> header strip, LIVE setup; Live strip; `[ ]`, 1-9 | Live strip + LIVE tab | `EditorTests.cpp` `Editor::theLiveTabEditsTheSnapshotBankAndTheSetlist` | `partial` - no header strip |
| GI-19-55 | Setlist -> LIVE; Live strip; PgUp/Dn | present | `EditorTests.cpp` `Editor::theLiveTabEditsTheSnapshotBankAndTheSetlist` | `verified` |
| GI-19-56 | Morph -> LIVE, Live strip knob | present | `LiveTests.cpp` `LiveSnapshots::morphFollowsItsCurve` | `implemented` |
| GI-19-57 | Tap tempo -> header tap; Live strip pad; `T` | Live strip + `T`; no header button | none | `partial` |
| GI-19-58 | Kill switch -> Live strip pill; `\` | present | `LiveTests.cpp` `LiveKillSwitch::reachesSilenceAndRecoversInsideFiveMilliseconds` | `verified` |
| GI-19-59 | Monitor mix -> Live strip, LIVE | present | `LiveTests.cpp` `LiveMonitor::idleWhenNothingToMonitorAndSumsWhenThereIs` | `implemented` |
| GI-19-60 | Expression cal -> Options EXPRESSION | present | `LiveTests.cpp` `LiveExpression::wizardCapturesHeelAndToe` | `verified` |
| GI-19-61 | Controller profile / latency / multi -> CONTROLLERS | present | `EditorTests.cpp` `Editor::everyWorkspaceTabSelectsAndPaints` | `verified` |
| GI-19-62 | Practice tools -> drawer + PRACTICE setup | drawer only | none | `partial` |
| GI-19-63 | IR slots, cab/EQ match, capture, library -> TONE MATCH | present | `EditorTests.cpp` `Editor::everyWorkspaceTabSelectsAndPaints` | `verified` |
| GI-19-64 | Notation export -> NOTATION, File menu; MIDI OUT | NOTATION tab export; header menu "Export notation..."; drawer TAB tab | `NotationPanelTests.cpp` `NotationTab::exportsEveryFormat` | `verified` - no MIDI OUT entry point |
| GI-19-65 | Live TAB view -> NOTATION, drawer TAB | NOTATION live tab fed by the capture; drawer TAB shows imported scores only | `NotationPanelTests.cpp` `NotationTab::stateButtonsLiveTabAndPreview` | `verified` - drawer TAB not fed live |
| GI-19-66 | Chord symbol history -> NOTATION | NOTATION chord history line; nothing reports chord changes to the capture (NOT-4-01) | none in use (`Capture::techniquesBendsChordsAndMetersReachTheScore` covers the model) | `partial` - empty in use |
| GI-19-67 | MIDI export profile -> MIDI OUT | MIDI OUT tab | `MidiOutPanelTests.cpp` `MidiOutPanel::profileEditsAreTheExportDefaults` | `verified` |
| GI-19-68 | Character seed / dead spots / wear / drift / body age / environment -> CHARACTER; Easy character macro | CHARACTER tab | `CharacterTests.cpp` `Character::allFreshAndAllOldPresets` | `verified` - with `EasyLayoutTests.cpp` `EasyLayout::theCharacterMacroIsTheCharacterAmount` |
| GI-19-69 | Squeak amount / probability / material / style -> CHARACTER STRING NOISE; Easy macro | present | `NoiseTests.cpp` `NoiseUi::squeakStylesApplyAndReadModified` | `implemented` - Easy Character macro exists |
| GI-19-70 | Pick fields -> CHARACTER PICK | present | `NoiseTests.cpp` `NoiseUi::theCharacterTabCarriesAPadlockWhenUnlocked` | `implemented` |
| GI-19-71 | Fret buzz / setup style / sitar / heatmap -> CHARACTER SETUP; Workshop setup strip | CHARACTER SETUP | `BuzzTests.cpp` `BuzzUi::setupStylesApplyAsOneStepAndReadModified` | `implemented` - Workshop setup strip (`d45fcd6`) |
| GI-19-72 | Slide material / mass / wall / pressure / slant / noise / clank -> CHARACTER SLIDE, Workshop slide part; `S` | CHARACTER SLIDE (mirror) | `SlideTests.cpp` `SlideUi::theSlideGroupAppearsWithSlideModeAndTheTabFitsIt` | `partial` - no Workshop slide part editing |
| GI-19-73 | Circuit visualiser -> Col 2 CIRCUIT, CHARACTER CIRCUIT | Col 2 only | none | `partial` |
| GI-19-74 | Advanced-range toggle -> Options RANGES, header padlock; right-click per control | present | `RangesUiTests.cpp` `RangesUi::theRangesPageListsLocksAndClamps`, `RangesUi::rightClickUnlocksAndRestrictsOneControl` | `verified` |
| GI-19-75 | Workshop bench -> wrench, Col 4 WORKSHOP; `W` | wrench (`HeaderBar`), WORKSHOP tab; no `W` shortcut found | `WorkshopPanelTests.cpp` `WorkshopPanel::itPaintsAndTheWorkshopTabTakesOverColumnsThreeAndFour` | `partial` - `W` shortcut |
| GI-19-76 | Parts drawer -> bench; right-click illustration part | 13-category drawer (`d45fcd6`); no right-click menu on the bench illustration | `WorkshopPanelTests.cpp` `WorkshopPanel::clickingACardFitsItAsOneUndoEntry` | `partial` - right-click entry missing |
| GI-19-77 | Inspector -> bench right column | `WorkshopPanel` inspector | `WorkshopPanelTests.cpp` `WorkshopPanel::theInspectorShowsTheSelectedPart`, `WorkshopPanel::editingAFieldMakesAUserCopy` | `verified` |
| GI-19-78 | Spectrum delta -> bench bottom right | `WorkshopPanel` spectrum pane over `SpectrumDelta` | `WorkshopBenchTests.cpp` `WorkshopSpectrum::aNullChangeIsFlat` (model; the pane is untested) | `implemented` |
| GI-19-79 | Bench A / B (8 slots) | A-H header slots over `WorkshopBench` slots | `WorkshopBenchTests.cpp` `WorkshopBench::abRecallRoundTrips` | `partial` - uiState persistence not confirmed (WUI-7-01) |
| GI-19-80 | Audition (Alt-hover) | Alt-hover on drawer cards, shadow audition | `WorkshopPanelTests.cpp` `WorkshopPanel::auditionFromTheDrawerNeverCommits` | `verified` - 30 ms return untested (WUI-11-03) |
| GI-19-81 | Guided build (templates) -> Workshop custom mode rail | not built; no spec detail beyond this row | none | `pending` |
| GI-19-82 | Bass slap / pop / ghost / LH slap / double thump / alternation / pluck position -> RHYTHM (bass), CHARACTER (bass amounts) | not built | none | `pending` |
| GI-19-83 | Tune builder -> TUNE; File menu New Tune | not built | none | `pending` |
| GI-19-84 | Chord progression editor -> TUNE | not built | none | `pending` |
| GI-19-85 | Melody piano roll -> TUNE | not built | none | `pending` |
| GI-19-86 | Melody generators (Auto / Draw / Record / Improvise / Style / Sing) -> TUNE | not built | none | `pending` |
| GI-19-87 | Section strip + setlist -> TUNE | not built | none | `pending` |
| GI-19-88 | Tune templates -> File menu New Tune | not built | none | `pending` |
| GI-19-89 | Bass line per section -> TUNE layers | not built | none | `pending` |
| GI-19-90 | Layers -> TUNE layer strip | not built | none | `pending` |
| GI-19-91 | Tune export -> TUNE export dialog; `Ctrl+E` | not built (`Ctrl+E` exports audio) | none | `pending` |
| GI-19-92 | MIDI export profile / drag-out / SysEx / PPQ -> MIDI OUT, session drag-out | MIDI OUT tab: profile, drag-out, live SysEx | `MidiOutPanelTests.cpp` `MidiOutPanel::profileEditsAreTheExportDefaults`, `MidiOutPanel::liveEventsAndWorkshopChangesGoOutAsLuthierSysEx` | `implemented` - drag gesture not tested |
| GI-19-93 | MIDI import -> File menu, window drop | not built | none | `pending` |
| GI-19-94 | Accessibility / localisation / scale / palette / reduced motion -> Options | present | `AccessibilityTests.cpp` `Accessibility::settingsRoundTrip` | `implemented` |
| GI-19-95 | Updates / telemetry / crash / licence / privacy -> Options UPDATES, PRIVACY; header notification | present | `TelemetryTests.cpp` `Telemetry::settingsRoundTrip`; `EditorTests.cpp` `Editor::theWindowRaisesSectionFifteensTriggersAndIsQuietWhenItShould` | `implemented` |

### Discoverability, realism marking, tests

| ID | Requirement (short) | Source (file §section) | Implementation location | Verification | Status |
|---|---|---|---|---|---|
| GI-20-01 | `?` icon on every multi-row panel opening Help pinned to it | GI §20 | not found | none | `pending` |
| GI-20-02 | "NEW" dot for one week after a version introduces a feature | GI §20 | not found | none | `pending` |
| GI-20-03 | Diagnostics "What's on the audio path right now" live block diagram | GI §20 | not found | none | `pending` |
| GI-20-04 | First-unlock explainer once | GI §20 | built (TODO 1) | none | `implemented` |
| GI-21-01 | Padlock in CHARACTER and WORKSHOP tab headers | GI §21 | CHARACTER padlock; no WORKSHOP tab | `NoiseTests.cpp` `NoiseUi::theCharacterTabCarriesAPadlockWhenUnlocked` | `partial` |
| GI-21-02 | Slide bar overlay rotated by slant, 80 ms ease | GI §21 | `FretboardComponent.cpp:168-183` | none | `implemented` - slant rotation unverified |
| GI-21-03 | Pick overlay at true size and angle with 8 px handles | GI §21 | not built | none | `pending` |
| GI-21-04 | Buzz heatmap with dot glyph | GI §21 | `SetupGroup.cpp` | `BuzzTests.cpp` `BuzzUi::heatmapCellsReadInMonochromeTerms` | `verified` |
| GI-21-05 | Circuit visualiser mini EQ curve live | GI §21 | `CircuitResponseView` | none | `implemented` |
| GI-21-06 | Noise-event strip; static count under reduced motion | GI §21 | `NoiseEventStrip` | `NoiseTests.cpp` `NoiseUi::theEventStripShowsWhatTheEngineTriggered` | `partial` - reduced-motion static count not verified |
| GI-21-07 | Slide pressure-state readout as mono text | GI §21 | `SlideGroup` | `SlideTests.cpp` `SlideUi::pressureSaysWhatItMeans` | `verified` |
| GI-22-01 | Test: every section 19 entry resolves to a real component (startup walk) | GI §22 | - | none | `pending` |
| GI-22-02 | Test: every automation ID resolves to exactly one control | GI §22 | - | none | `pending` |
| GI-22-03 | Test: Tab-order walk visits every element in column order | GI §22 | - | none | `pending` |
| GI-22-04 | Test: reflow at 6 widths x 6 scales without clipping | GI §22 | - | `EditorTests.cpp` `Editor::itLaysOutAndPaintsAcrossItsResizeRange` (3 sizes, 100% only) | `partial` |
| GI-22-05 | Test: every panel's empty-state hint present | GI §22 | - | none | `pending` |
| GI-22-06 | Test: all 13 right-click items per parameter; range items state-correct | GI §22 | - | `RangesUiTests.cpp` `RangesUi::rightClickUnlocksAndRestrictsOneControl` (range items) | `partial` |
| GI-22-07 | Test: 1000-op undo random walk incl. Workshop swaps | GI §22 | - | none | `pending` |
| GI-22-08 | Test: 10 000 random clicks select the intended part by z-order | GI §22 | - | `EditorTests.cpp` `Editor::everyHitRegionOnTheIllustrationDescribesItself` (grid sweep, current illustration) | `partial` |
| GI-22-09 | Test: Slide Mode toggled 100x during playback, no click, correct panels | GI §22 | - | `SlideTests.cpp` `Slide::switchingModeMidNoteIsClean` (single toggle) | `partial` |
| GI-22-10 | Test: warning arc appears past stock and disappears on return | GI §22 | - | `RangesUiTests.cpp` `RangesUi::controlsFollowASwappedRangeAndMarkTheValue` | `verified` |

## 27. ui-wiring.md (phase 4)

Precedence 3 (how UI attaches to backend). TODO 14 says this spec has not
been audited against the build; this section is that first pass. The build
uses APVTS attachments (`Source/UI/Widgets.*`) and atomics polled on timers;
there is **no command / result queue and no display FIFO** (rg
`AbstractFifo`, `Command` find nothing in `Source/` outside tests). Preset
loads and guitar swaps run on the message thread (the swap parks the audio
thread, DECISIONS). The BRIEF says to follow this threading contract
"exactly"; see Conflict C-09 and blockers.

| ID | Requirement (short) | Source (file §section) | Implementation location | Verification | Status |
|---|---|---|---|---|---|
| UW-0-01 | Every parameter in the single APVTS; no side-channel audible state | ui-wiring §0.1 | `Parameters.cpp`; exceptions: per-string detune (engine direct, GAPS A4), character state, rhythm state | none | `partial` |
| UW-0-02 | UI attaches via Slider/Button/ComboBox attachments; never raw writes | ui-wiring §0.2 | `Widgets.cpp` attachments; headstock detune sliders write engine directly | none | `partial` |
| UW-0-03 | Non-parameter UI state in `uiState`, saved with the preset | ui-wiring §0.3 | `UiState` struct `PluginProcessor.h:347` (not a ValueTree) | `StateModelTests.cpp` `StateModel::loadingAPresetLeavesTheLayersAboveItAlone` | `implemented` |
| UW-0-04 | Listeners removed in destructors | ui-wiring §0.4 | per component | none (UW-23-01 missing) | `implemented` |
| UW-0-05 | Thread separation via documented mechanisms | ui-wiring §0.5, §4 | atomics + timers | none | `partial` |
| UW-0-06 | Structural state via command/result queue with atomic pointer swaps | ui-wiring §0.6, §4.3 | not built: structural state applied on message thread (`ParameterBridge` AsyncUpdater, processor methods) | none | `pending` |
| UW-1-01 | Parameter contract: ID, translated display name, PhysicalRange or range, default, unit enum, text functions, category tag | ui-wiring §1 | `Parameters.cpp`; names not translated; no unit enum or category tag | `IntegrationTests.cpp` `Parameters::everyParameterTextRoundTrips` | `partial` |
| UW-1-02 | PhysicalRange switches live min/max in place | ui-wiring §1 | `RangeState` swap | `RangeTests.cpp` `Ranges::normalisationFollowsTheLiveRange` | `verified` |
| UW-2-01 | Base classes AttachedKnob (value arc, mod arc, `*`, right-click per GI 16), AttachedSwitch, AttachedCombo (rebuilds on metadata change), PartSlotWidget | ui-wiring §2 | `LuthierKnob`, `LuthierToggle`, `LuthierChoice` in `Widgets.h`; no PartSlotWidget | none | `partial` |
| UW-3-01 | Panels `<Name>Panel` with id, display name, collapse state; never own DSP state | ui-wiring §3 | `*Panel` classes; no collapse API | none | `partial` |
| UW-4.2-01 | Display FIFO per subsystem; drains meters 30 Hz, fretboard 60, chord/voices 10, noise strip 15 | ui-wiring §4.2 | no FIFO; editor 4 Hz poll + component timers | none | `pending` - see gui-engine-dataflow |
| UW-4.3-01 | SPSC command queue; pointer payloads from a pool; old pointers returned for message-thread destruction; eight commands | ui-wiring §4.3 | not built | none | `pending` |
| UW-5-01 | Preset load: parse on message thread, apply at audio block boundary, swap structural state, apply ranges, post result | ui-wiring §5 | applied on message thread (ranges before values, TODO done) | `StateModelTests.cpp` `StateModel::loadingAPresetWhileRenderingProducesNoGarbage` | `partial` - mechanism differs; no-garbage property tested |
| UW-5-02 | Snapshot recall never touches disk or ranges | ui-wiring §5 | `SnapshotBank` | `StateModelTests.cpp` `StateModel::recallingASnapshotStaysInsideThePreset` | `verified` |
| UW-6.1-01 | LoadGuitarCommand swaps spec on audio thread | ui-wiring §6.1 | message-thread rebuild behind a park; kept for whole-guitar loads by DECISIONS C-09; notes queued | `WorkshopPresetTests.cpp` `WorkshopSwap::aPartSwapDuringANoteIsClickFree`, `WorkshopSwap::aNotePlayedWhileParkedIsKeptNotDropped` | `verified` - as amended by C-09 |
| UW-6.2-01 | SwapPartCommand: copy spec, map, atomic swap, 5 ms coefficient crossfade, return old | ui-wiring §6.2 | still the park path; DECISIONS C-09 moves same-string-count part swaps to the off-thread swap with the bench | `WorkshopPresetTests.cpp` `WorkshopSwap::aSwapMapsOnceNotPerBlock` | `partial` |
| UW-6.3-01 | Shadow audition: render against shadow spec, 30 ms crossfade back, never mutates committed/undo | ui-wiring §6.3 | not built | none | `pending` |
| UW-6.4-01 | Spectrum delta on worker pool within 40 ms | ui-wiring §6.4 | not built | none | `pending` |
| UW-7-01 | SetRangeModeCommand per family / per control; clamp posts ClampNotification | ui-wiring §7 | `processor.changeRanges` on message thread | `RangeTests.cpp` `Ranges::narrowingClampsAndReportsTheCount` | `partial` - mechanism differs |
| UW-8-01 | MIDI Learn: header/Ctrl+L arm, next control claims, first non-note MIDI maps | ui-wiring §8 | `MidiLearn`; global arm layer | `IntegrationTests.cpp` `MidiLearn::armingIsSeparateFromLearningUntilAControlClaimsIt`, `MidiLearn::disarmingCancelsAnInFlightLearn` | `verified` |
| UW-8-02 | Mappings per preset; "Save as global" option | ui-wiring §8 | global flag not found | none | `partial` |
| UW-9-01 | Meters via FIFO; peak hold UI-side; heatmap from per-string amplitude and clearance | ui-wiring §9 | heatmap reads engine state | `BuzzTests.cpp` `Buzz::theHeatmapAgreesWithTheGenerator` | `partial` |
| UW-10-01 | Noise strip from FIFO events; static per-class count at 5 Hz under reduced motion | ui-wiring §10 | `NoiseEventStrip` | `NoiseTests.cpp` `NoiseUi::theEventStripShowsWhatTheEngineTriggered` | `partial` |
| UW-11-01 | LogStream: 200 lines, alpha fade, stop after 500 ms idle, off under reduced motion | ui-wiring §11 | data stream widget `Widgets.cpp` | `AccessibilityTests.cpp` `Accessibility::reducedMotionRemovesAnimation` | `implemented` |
| UW-12-01 | Mod values via FIFO at 30 Hz; route edits via commands; drag-to-assign with 0.25 depth and ghost drag | ui-wiring §12 | direct `ModMatrix` calls; no drag | none | `partial` |
| UW-13-01 | GuitarIllustration subscribes to GuitarSpec; one class for Easy and bench with `interactionMode`; overlays from FIFO | ui-wiring §13 | `GuitarBodyComponent` (compiled spec); `GuitarRenderer.h` API only | none | `pending` |
| UW-14-01 | NoiseEngine pool per 14 incl. Aux 8 tap and FIFO event per trigger | ui-wiring §14 | pool built; Aux 8 tap (`a901c72`); trigger events reach MIDI OUT as SysEx (`a357142`) | `NoiseTests.cpp` `NoisePool::aFullPoolStealsTheOldest`; `PluginBusTests.cpp` `PluginBuses::aux8CarriesThePlayingNoiseAndObeysItsStrip` | `verified` |
| UW-15-01 | GuitarCircuit coefficients at control rate, < 0.05% CPU per change; coefficient FIFO for user changes | ui-wiring §15 | audio-thread recompute between blocks (DECISIONS) | `CircuitTests.cpp` `Circuit::sweepingEveryControlDoesNotAllocate` | `partial` - cost not measured |
| UW-16-01 | SlideEngine pressure state machine (Lifted/Light/Normal/Heavy/Fretted); bar position continuous parameter | ui-wiring §16 | `SlideEngine` (pressure is a parameter; bar position engine state) | `SlideTests.cpp` `SlideUi::pressureSaysWhatItMeans` | `partial` |
| UW-17-01 | getStateInformation carries APVTS, uiState, mod matrix, snapshots, setlist ref, MIDI maps, ranges, guitar reference/inline, circuit, MIDI export profile | ui-wiring §17, §22 | `PluginProcessor.cpp:1748-1833` | `IntegrationTests.cpp` `Presets::stateRoundTripsExactly`; `WorkshopPresetTests.cpp` `WorkshopPresets::anEditedGuitarTravelsWholeInTheState` | `partial` - no MIDI export profile |
| UW-18-01 | Every APVTS change undoable; structural changes compound; Workshop entries with real-unit strings; undo manager on processor | ui-wiring §18 | processor snapshot undo; `ScopedUndoAction` | `RangesUiTests.cpp` `Undo::stepsOneActionAtATimeBothWays` | `partial` |
| UW-19-01 | Threading table; worker pool `juce::ThreadPool` with 2 threads for long tasks | ui-wiring §19 | no shared ThreadPool (rg `ThreadPool`) | none | `pending` |
| UW-20-01 | Every string from `LocaleCatalog::get`; LocaleChanged broadcaster -> refreshStrings | ui-wiring §20 | partial catalog; many literals in panels | `AccessibilityTests.cpp` `Localisation::catalogCoversTheUi` | `partial` |
| UW-21-01 | AccessibilityHandler on every component; custom widgets per-child handlers; unit in announcements | ui-wiring §21 | see ACC rows | none | `partial` |
| UW-22-01 | Preset files use the state serializer minus instance UI state | ui-wiring §22 | `PresetManager` | `IntegrationTests.cpp` `Presets::stateRoundTripsExactly` | `implemented` |
| UW-23-01 | Test: panels instantiated 100x leave no listeners | ui-wiring §23 | - | none | `pending` |
| UW-23-02 | Test: 60 s session with message-manager lock detector, no audio-thread UI access | ui-wiring §23 | - | none | `pending` |
| UW-23-03 | Test: 20 presets x 10 random loads match offline render | ui-wiring §23 | - | none | `pending` |
| UW-23-04 | Test: 1000 random changes with undo/redo equal forward sequence | ui-wiring §23 | - | none | `pending` |
| UW-23-05 | Test: all 128 CCs map within one block | ui-wiring §23 | - | `IntegrationTests.cpp` `MidiLearn::mapsAndUnmapsCleanly` (not all 128) | `partial` |
| UW-23-06 | Test: display FIFO 10x overflow for 60 s | ui-wiring §23 | - | none | `pending` |
| UW-23-07 | Test: 100 shadow auditions leave spec byte-identical | ui-wiring §23 | - | none | `pending` |
| UW-23-08 | Test: every physical param stock/advanced x100, clamps right, no allocation | ui-wiring §23 | - | `RangeTests.cpp` `Ranges::wideningPreservesEveryPlainValue` (single pass) | `partial` |
| UW-23-09 | Test: every slot swapped 100x during playback below click threshold | ui-wiring §23 | - | `WorkshopPresetTests.cpp` `WorkshopSwap::aPartSwapDuringANoteIsClickFree` (one swap) | `partial` |
| UW-23-10 | Test: spectrum delta vs offline within 0.2 dB | ui-wiring §23 | - | none | `pending` |

## 28. onboarding.md (phase 4)

Not audited before (TODO 14). Only the advanced-range first-unlock explainer
is built (`RangesUi.cpp:12-276`, key `ranges_first_unlock_explained`). No
welcome banner, tour, first-week hints, OS-preference detection, example
content or "Restore first-run experience" exist (rg `tour`, `welcome`,
`firstRun` find nothing in `Source/`).

| ID | Requirement (short) | Source (file §section) | Implementation location | Verification | Status |
|---|---|---|---|---|---|
| ONB-0-01 | Great sound in < 30 s with no configuration; no modal on first run | onboarding §0.1-0.2 | default preset loads | none | `implemented` - not measured |
| ONB-0-03 | Tour optional; declined tours never reappear | onboarding §0.3 | no tour | none | `pending` |
| ONB-0-05 | First run does no network activity | onboarding §0.5 | all telemetry/update off by default | `TelemetryTests.cpp` `Telemetry::everythingIsOffByDefault` | `verified` |
| ONB-1-01 | Fresh install: preset `Factory / Rock / Modern Overdrive`, guitar Les Paul Standard, Easy, Live off, drawer collapsed, Workshop closed, Slide off, ranges stock | onboarding §1 | no "Modern Overdrive" preset or "Rock" category in `FactoryPresets.cpp`; default guitar type 0 is a double-cut (DECISIONS) | `WorkshopPresetTests.cpp` `WorkshopPresets::aFreshInstanceNamesItsFactoryGuitar` | `partial` - default preset and guitar differ from spec |
| ONB-1-02 | Realism defaults: squeak 25%, pick click at material default, Player-friendly setup | onboarding §1 | squeak 0.25, Player-friendly (DECISIONS) | `NoiseTests.cpp` `NoiseUi::squeakStylesApplyAndReadModified`; `BuzzTests.cpp` `Buzz::playerFriendlyBuzzesOnlyWhenAttackedHard` | `verified` |
| ONB-2-01 | Welcome banner each new version: Yes / Maybe later (max 3) / Don't ask again | onboarding §2 | not built | none | `pending` |
| ONB-3-01 | 12-step tour with Next/Back/Skip, Escape ends, closing line | onboarding §3 | not built (several targets - wrench, TUNE, header snapshot strip, gear - do not exist either) | none | `pending` |
| ONB-4-01 | First-week hints: `?` pulse, unused-tab dots, dice tooltip, wrench/TUNE/slide pulses | onboarding §4 | not built | none | `pending` |
| ONB-5-01 | First-run defaults: sidechain off, sidechain-to-amp off, session recorder off, updates/telemetry/crash/beta off, stock, Slide off, Live off | onboarding §5 | defaults as listed | `TelemetryTests.cpp` `Telemetry::everythingIsOffByDefault`; `PracticeTests.cpp` `PracticeSession::disabledByDefault` | `implemented` - declared; specific defaults or the refusal path are not asserted by the cited test |
| ONB-5-02 | Reduced motion follows OS; high-contrast OS -> high-contrast palette; DPI > 150% snaps to 125%; locale follows OS if shipped | onboarding §5 | not found | none | `pending` |
| ONB-6-01 | 36 factory presets across Electric, Acoustic, Classical, Bass, Utility | onboarding §6 | `FactoryPresets.cpp` (36) | `IntegrationTests.cpp` `Presets::everyFactoryPresetLoadsAndPlays` | `verified` |
| ONB-6-02 | 12 factory guitars as `.luthierguitar` | onboarding §6 | 27 shipped (DECISIONS; Conflict C-15) | `WorkshopTests.cpp` `Workshop::everyFactoryGuitarLoadsAndRoundTrips` | `verified` |
| ONB-6-03 | 60+ factory parts across every slot | onboarding §6 | 148 in `Resources/Parts` | `WorkshopTests.cpp` `Workshop::theFactoryLibraryIsThere` | `verified` |
| ONB-6-04 | 12 tune templates; 6 example tunes | onboarding §6 | ten templates (C-16) in `Resources/Tunes/Templates`; no example tunes | `TuneBuilderTests.cpp` `TuneBuilder::theTenTemplatesLoadInOrderAndAreValid` | `partial` - example tunes |
| ONB-6-05 | 12 example MIDI clips in `Resources/Examples/`; 6 royalty-free backing tracks; 10 example setlists | onboarding §6 | none on disk | none | `pending` |
| ONB-7-01 | First advanced-range encounter popover with fixed text, once | onboarding §7 | `RangesUi::showExplainerIfFirstTime` | none | `implemented` |
| ONB-7-02 | Re-triggered by "Restore first-run experience" (must clear `ranges_first_unlock_explained`) | onboarding §7, §12 | not built (TODO 14c) | none | `pending` |
| ONB-8-01 | TUNE first-encounter inline hint | onboarding §8 | no TUNE tab | none | `pending` |
| ONB-9-01 | Workshop first-encounter hint | onboarding §9 | no bench | none | `pending` |
| ONB-10-01 | Three documented paths (30 s, 2 min, 5 min) in manual and videos | onboarding §10 | not documented | none | `pending` |
| ONB-11-01 | Returning user: last preset, window size, mode, tab, drawer, Slide state, last tune; update banner | onboarding §11 | host state restore; tab via `UiPreferences` | `EditorTests.cpp` `Editor::theWorkspaceTabWrapsAndIsRemembered` | `partial` - standalone "last preset" and tune not verified |
| ONB-12-01 | Options -> Diagnostics "Restore first-run experience" with confirm; keeps libraries | onboarding §12 | not built | none | `pending` |
| ONB-13-01 | Version upgrade banner, NEW dots, changelog, migrations with dated Backup folder | onboarding §13 | preset backups exist (file-formats 13.4); rest not built | `IntegrationTests.cpp` `Presets::savingBacksUpTheVersionItReplaces` | `partial` |
| ONB-14-01 | Test: fresh install default state | onboarding §14 | - | `WorkshopPresetTests.cpp` `WorkshopPresets::aFreshInstanceNamesItsFactoryGuitar` (guitar only) | `partial` |
| ONB-14-02 | Test: tour 12 steps aligned at every scale | onboarding §14 | - | none | `pending` |
| ONB-14-03 | Test: skip tour leaves plugin playable | onboarding §14 | - | none | `pending` |
| ONB-14-04 | Test: OS high contrast and DPI 200 detection | onboarding §14 | - | none | `pending` |
| ONB-14-05 | Test: version upgrade banner and data intact | onboarding §14 | - | none | `pending` |
| ONB-14-06 | Test: restore first-run keeps libraries | onboarding §14 | - | none | `pending` |
| ONB-14-07 | Test: first-encounter popovers fire once until reset | onboarding §14 | - | none | `pending` |

## 29. performance-budget.md (phase 4)

Precedence 5 (CPU and memory). Not audited before (TODO 14/15). Nothing here
is measured to the spec's units: there is no per-module measurement, no CI,
no heap or lock trap, no relief mechanism. Two tests touch CPU:
`Engine::cpuStaysWithinBudget` (< 85% of real time, whole engine) and
`Modulation::thousandRouteStressTest` (< 1% using thread CPU time).

| ID | Requirement (short) | Source (file §section) | Implementation location | Verification | Status |
|---|---|---|---|---|---|
| PB-0-01 | Every module has a budget and measured cost captured in CI on every merge | performance-budget §0.1, §9 | no CI, no per-module measurement | none | `pending` |
| PB-0-02 | Modules measured isolated in the offline renderer over 60 s | performance-budget §0.2 | `LuthierRender` exists; no measurement mode | none | `pending` |
| PB-0-03 | Regression > 10% flagged; > 20% blocks | performance-budget §0.3 | - | none | `pending` |
| PB-0-04 | Audio callback allocates zero bytes; heap hook in test mode | performance-budget §0.4 | `Support/ThreadProbe.h` (file probe); circuit allocation probe | `CircuitTests.cpp` `Circuit::sweepingEveryControlDoesNotAllocate` (circuit only) | `partial` |
| PB-0-05 | Audio callback locks nothing | performance-budget §0.5 | cabinet try-lock fallback (`CabinetEngine.h:153`) | none | `partial` - try-lock on audio thread; no lock trap |
| PB-1-01 | Per-module CPU budgets (27 modules, units = % of a core at 48 kHz/128) | performance-budget §1 | - | `ModulationTests.cpp` `Modulation::thousandRouteStressTest` (ModMatrix < 1%, budget 0.15) | `pending` - one module measured, against a looser bar |
| PB-1-02 | Totals: idle <= 1.5, steady <= 8, realism <= 12, slide <= 18, bass <= 15, heavy <= 22 units | performance-budget §1 | - | `IntegrationTests.cpp` `Engine::cpuStaysWithinBudget` (< 85% real time) | `pending` |
| PB-2-01 | No pedal > 0.5 units; per-pedal budgets; eco Quality option above | performance-budget §2 | - | none | `pending` |
| PB-3-01 | RSS: baseline <= 350 MB ... cap 900 MB; breakdown | performance-budget §3 | - | none | `pending` |
| PB-4-01 | Latency: main <= 128 samples excl. oversampling; per-string, DI <= 32; Aux 8 <= 128; reported per routing-io 7 | performance-budget §4 | `PluginProcessor.cpp:759` | `IntegrationTests.cpp` `Engine::latencyIsReportedAndPlausible`; `RoutingTests.cpp` `Routing::perOutputLatencyIsConsistent` | `partial` - reporting verified; values vs budget not asserted |
| PB-5-01 | Boot: cold <= 400 ms, warm <= 200, standalone <= 1.5 s, guitar load <= 300, tune <= 100, part swap <= 50, delta <= 40 ms | performance-budget §5 | guitar rebuild ~14 ms parked after IR caching (DECISIONS, `0d225f0`) | `WorkshopBenchTests.cpp` `WorkshopSpectrum::theWorkerCoalescesAndStaysInBudget` (delta < 40 ms) | `partial` - only the delta budget is asserted |
| PB-6-01 | Voice-count scaling curve | performance-budget §6 | - | none | `pending` |
| PB-7-01 | Sample-rate scaling; oversampling downgrades above 96 kHz | performance-budget §7 | not found | none | `pending` |
| PB-8-01 | CPU relief ladder at 85% rolling 200 ms (7 steps; step 7 opt-out with banner) | performance-budget §8 | only `NoiseEngine::setDegraded` exists, nothing calls it on load | none | `pending` |
| PB-9-01 | Dashboard of per-merge results | performance-budget §9 | none | none | `pending` |
| PB-10-01 | Test: per-module budget within 10% | performance-budget §10 | - | none | `pending` |
| PB-10-02 | Test: idle, heavy, realism, slide, bass totals | performance-budget §10 | - | none | `pending` |
| PB-10-03 | Test: 60-min session peak RSS <= 900 MB, no monotonic growth | performance-budget §10 | - | none | `pending` |
| PB-10-04 | Test: 5 min with heap-alloc trap, zero triggers | performance-budget §10 | - | none | `pending` |
| PB-10-05 | Test: lock trap, zero triggers | performance-budget §10 | - | none | `pending` |
| PB-10-06 | Test: latency within 1 sample of reported | performance-budget §10 | - | `IntegrationTests.cpp` `Engine::latencyIsReportedAndPlausible` | `implemented` - check it measures within 1 sample |
| PB-10-07 | Test: SR scaling within 10% | performance-budget §10 | - | none | `pending` |
| PB-10-08 | Test: boot time cold and warm | performance-budget §10 | - | none | `pending` |
| PB-10-09 | Test: part swap budget over 100 swaps | performance-budget §10 | - | none | `pending` |
| PB-10-10 | Test: shadow audition budget over 100 events | performance-budget §10 | - | none | `pending` |
| PB-10-11 | Test: spectrum delta budget over 100 renders | performance-budget §10 | - | none | `pending` |

## 30. qa-polish.md (phase 4, ship gate)

Precedence 1 (ship-readiness). Every item is a gate. None of the five gates
is green: there is no CI, no host/platform matrix, no golden renders, and
several advertised features are unbuilt. The trademark sweep landed in
`25e6139` (legal review still open, C-17); there is no `THIRD_PARTY_LICENCES.txt` at the
repo root.

| ID | Requirement (short) | Source (file §section) | Implementation location | Verification | Status |
|---|---|---|---|---|---|
| QA-0-01 | Gate 1: zero crashes across the matrix | qa-polish §0, §3 | - | matrix not run | `pending` |
| QA-0-02 | Gate 2: zero broken UI (clipping, dead buttons, unlocalised strings, missing tooltips) | qa-polish §0, §4 | - | none | `pending` - English-only catalog; unbuilt tabs |
| QA-0-03 | Gate 3: zero audio artefacts on any factory preset | qa-polish §0, §5 | - | `IntegrationTests.cpp` `Presets::everyFactoryPresetLoadsAndPlays` (smoke) | `pending` |
| QA-0-04 | Gate 4: every advertised feature works end to end | qa-polish §0, §6 | - | see GI-19 rows | `pending` |
| QA-0-05 | Gate 5: performance targets met | qa-polish §0, §7 | - | see PB rows | `pending` |
| QA-1-01 | Host matrix (9 hosts incl. Pro Tools, Standalone) | qa-polish §1 | - | not run (`docs/KNOWN_ISSUES.md`) | `pending` |
| QA-1-02 | Platform matrix (Win 10/11, macOS 13-15 ARM/Intel, Ubuntu 22.04/24.04) | qa-polish §1 | Windows only here | none | `blocked` - Windows-only machine (TODO 16) |
| QA-1-03 | Sample rates 44.1-192 and buffers 32-2048 | qa-polish §1 | - | `IntegrationTests.cpp` `Engine::sampleRateChangesAreSurvived`, `Engine::blockSizeChangesAreSurvived` (subset) | `partial` |
| QA-1-04 | CPU classes low/mid/high | qa-polish §1 | - | none | `pending` |
| QA-2-01 | Golden render of 36 presets per (rate, block), -80 dBFS | qa-polish §2 | - | none | `pending` |
| QA-2-02 | Parameter fuzz 100 000 states, no NaN/denormal, <= +3 dBFS | qa-polish §2 | - | `IntegrationTests.cpp` `Parameters::fuzzAcrossTenThousandStates` (10 000) | `partial` |
| QA-2-03 | State fuzz 10 000 ops (preset, snapshot, setlist, route, part swap) | qa-polish §2 | - | none | `pending` |
| QA-2-04 | Boot time 400/200 ms | qa-polish §2 | - | none | `pending` |
| QA-2-05 | 60-min memory, < 900 MB, < 8 MB per 10 min | qa-polish §2 | - | none | `pending` |
| QA-2-06 | Preset round trip within float ulp | qa-polish §2 | - | `IntegrationTests.cpp` `Presets::stateRoundTripsExactly` | `verified` |
| QA-2-07 | Guitar round trip, audio within -80 dBFS | qa-polish §2 | - | `WorkshopTests.cpp` `Workshop::everyFactoryGuitarLoadsAndRoundTrips` (spec, not audio) | `partial` |
| QA-2-08 | Migration round trip of pre-M49 presets within -60 dBFS | qa-polish §2 | - | none | `pending` |
| QA-2-09 | pluginval strictness 10, every platform and format, zero warnings | qa-polish §2 | - | Windows VST3 at `f18bf22` (2026-09-19) | `partial` |
| QA-2-10 | CI on every push; nightly full matrix | qa-polish §2 | no CI config in repo | none | `pending` |
| QA-3-01 | Crash sources: 32 instances; format switch; SR change mid-play; block change; preset/snapshot/guitar load under MIDI storm; part swap at 22 units; 95% host CPU; MIDI Learn 100x; undo 1000x; bus change; Slide 100x; range toggle 100x | qa-polish §3 | - | `IntegrationTests.cpp` `Engine::sampleRateChangesAreSurvived`; `StateModelTests.cpp` `StateModel::loadingAPresetWhileRenderingProducesNoGarbage` | `partial` |
| QA-3-02 | Corrupt preset byte-flip refusal | qa-polish §3 | `PresetManager` | `IntegrationTests.cpp` `Presets::mutatedPresetsNeverCrashTheLoader` | `verified` |
| QA-3-03 | Corrupt `.luthierguitar` byte-flip | qa-polish §3 | - | none | `pending` |
| QA-3-04 | Missing IR/sample/part/guitar -> fallback + banner | qa-polish §3 | fallbacks exist | `EditorTests.cpp` `Editor::aFailedPresetLoadAndAMissingIrEachRaiseABannerOnce`; `WorkshopTests.cpp` `Workshop::aMissingPartFallsBackAndSaysSo` | `partial` |
| QA-4-01 | Per-control checklist (tooltip and label per locale, a11y with units, interactions, reset, value entry/paste, arcs, focus ring, undo, range items) | qa-polish §4 | - | none | `pending` |
| QA-4-02 | Per-panel checklist (header/status/chevron/padlock, collapse per preset, 6 scales, palettes, reduced motion, empty-area menu, `?`) | qa-polish §4 | - | none | `pending` |
| QA-4-03 | Dialogs: Escape, focus first element, announced, not lost behind host, save-on-change | qa-polish §4 | Escape yes | `EditorTests.cpp` `Editor::everyOverlayShortcutOpensItsOwnOverlayAndEscapeClosesIt` | `partial` |
| QA-4-04 | Empty-state hints per GI 14 | qa-polish §4 | - | none | `pending` |
| QA-4-05 | Workshop checklist (hit-test, drag bounds, A/B 30 ms, audition byte-identical, delta 0.2 dB, Save As Guitar reloads) | qa-polish §4 | bench not built | `WorkshopPresetTests.cpp` `WorkshopPresets::saveAsGuitarWritesAFileAndPointsThePresetAtIt` | `pending` |
| QA-4-06 | Slide Mode checklist (glyph/overlay/part together, squeak suppressed, continuous announcement) | qa-polish §4 | - | `SlideTests.cpp` `Slide::squeakStopsUnderTheBarButNotBesideIt` | `partial` |
| QA-4-07 | Advanced-range marking checklist | qa-polish §4 | `RangesUi` | `RangesUiTests.cpp` `RangesUi::controlsFollowASwappedRangeAndMarkTheValue` | `verified` |
| QA-5-01 | Every preset: fixture without clicks/denormals/over +3 dBFS; clean load; sounds like its name (audio-lead signoff); DC null -100 dBFS; mono compatibility; bypass null | qa-polish §5 | - | `IntegrationTests.cpp` `Engine::monoCompatibility`, `Engine::silenceInSilenceOut` | `partial` - signoff and bypass null missing |
| QA-5-02 | Every pedal: click-free toggle, bounded extremes, zero-mix bypass -80 dBFS | qa-polish §5 | - | `EngineTests.cpp` `Effects::everyPedalTypeRunsCleanly`, `Effects::bypassIsTransparent` | `verified` |
| QA-5-03 | Every amp: no cold-start transient; monotonic gain sweep; neutral stack flat within 1 dB | qa-polish §5 | - | `EngineTests.cpp` `Amp::standbyIsSilentAndWarmsUp` (partial) | `partial` |
| QA-5-04 | Realism checks: squeak off/determinism, pick material energy within 0.5 dB, rake audible, buzz threshold ±, slide within 2 c, slide vibrato, clank, circuit vs CableSim table, volume-5 cleanup, bleed ratios, Aux 8 null, strum click energy, rasgueado, bass slap/pop/ghost/thump/alternation | qa-polish §5 | mixed: squeak/buzz/slide/circuit built; Aux 8, strum, bass not built | `NoiseTests.cpp` `Squeak::theProbabilityRollIsDeterministic`; `BuzzTests.cpp` `Buzz::theThresholdIsATrimNotAMute`; `SlideTests.cpp` `Slide::pitchIsContinuous` | `partial` |
| QA-6-01 | Every GI 19 entry present, wired, persistent, automatable, shortcut, tested | qa-polish §6 | - | see GI-19 rows | `pending` |
| QA-6-02 | Every referenced user document exists, current, screenshots < 30 days, translated | qa-polish §6 | `docs/*.md` English; `docs/MIDI_EXPORT_LUTHIER_PROFILE.md` missing | none | `pending` |
| QA-6-03 | MIDI export completeness (Luthier/Generic/drag-out/live alignment) | qa-polish §6 | not built | none | `pending` |
| QA-7-01 | Performance targets (idle 1.5% ... memory, latency, load times; > 5% regression gates) | qa-polish §7 | - | none | `pending` |
| QA-8-01 | Bug bash: 48 h lockdown, 8 external guitarists, 90-min script, triage rules | qa-polish §8 | - | not run | `pending` |
| QA-9-01 | Windows EV-signed installer, component picker, uninstaller preserving data, silent flag | qa-polish §9 | none | none | `pending` |
| QA-9-02 | macOS notarised .pkg (VST3 + AU), uninstall script | qa-polish §9 | none | none | `blocked` - no macOS machine or certificate |
| QA-9-03 | Linux .tar.gz and .deb, .desktop file | qa-polish §9 | none | none | `blocked` - no Linux build (include.md defers Linux) |
| QA-9-04 | Every installer: licence accept, disk space, newer-version guard, clear result | qa-polish §9 | none | none | `pending` |
| QA-10-01 | Manual complete and translated with Workshop and Realism chapters; docs current; two videos; support email monitored; community seeded | qa-polish §10 | `docs/USER_MANUAL.md` (English; no Workshop chapter checked) | none | `pending` |
| QA-11-01 | Every third-party library in `THIRD_PARTY_LICENCES.txt` | qa-polish §11 | file absent at repo root | none | `pending` |
| QA-11-02 | No trademarks in preset, amp, guitar, speaker or part names | qa-polish §11 | reference-style display names (`25e6139`); internal enum identifiers unchanged; `Tools/trademark_scan.py` | `TrademarkTests.cpp` `Trademarks::noChoiceListNamesABrand`, `Trademarks::noFactoryPresetPartOrGuitarNamesABrand`, `Trademarks::oldNamesStillLoad`, `Trademarks::sourceTreeHasNoUnmarkedBrandNames` (`5ccf238`); `FacesTests.cpp` `Faces::noFaceTextNamesABrand` | `verified` - legal review is FC-0-01 |
| QA-11-03 | Every IR generated or licensed with documentation | qa-polish §11 | synthesised by `scripts/make_irs.py` | README deviations | `implemented` |
| QA-11-04 | EULA finalised; refund policy on website | qa-polish §11 | none | none | `pending` |
| QA-12-01 | Final human check: 30 min fresh-ears play; build a guitar from a template, save, reopen, reload | qa-polish §12 | Workshop bench absent | none | `pending` |
| QA-13-01 | Post-release: crash-rate monitoring, 8 h support SLA, 30-min manifest rollback, 24 h hot-fix path | qa-polish §13 | none | none | `pending` |

## 31. installer.md (phase 4)

**Not built.** No installer scripts, signing, packaging, content-update
format or first-run folder tree exist in the repo (TODO 16: the platform
matrix cannot be run from this Windows-only machine). Rows are grouped by
section.

| ID | Requirement (short) | Source (file §section) | Implementation location | Verification | Status |
|---|---|---|---|---|---|
| INS-0-01 | Never runs in a live host's audio thread; signed/notarised/PGP; uninstall preserves user data unless purge; minimal elevation; deterministic output | installer §0 | none | none | `pending` |
| INS-1-01 | Windows NSIS/Inno `.exe`, EV-signed, `Luthier-<v>-Setup-win64.exe` | installer §1 | none | none | `pending` - needs EV certificate |
| INS-1.1-01 | Windows flow: splash, language, licence, components, locations (VST3 fixed, standalone, ProgramData content, docs), space check, version check, progress with rollback | installer §1.1 | none | none | `pending` |
| INS-1.1-02 | Post-install: Add/Remove entry, file associations (6 types), Start menu, optional launch, "Install complete" | installer §1.1 | none | none | `pending` |
| INS-1.2-01 | Silent `/S`, `/D=`, documented exit codes, HKLM keys | installer §1.2 | none | none | `pending` |
| INS-1.3-01 | Uninstaller: manifest-tracked removal, keeps Documents/Luthier, purge checkbox, refuses while a DAW has the plugin loaded | installer §1.3 | none | none | `pending` |
| INS-2-01 | macOS signed + notarised `.dmg` with `.pkg`; component picker; standard paths; Launch Services associations; admin only for system paths; "Open Luthier" | installer §2, §2.1 | none; AU target not declared (`CMakeLists.txt:25`) | none | `blocked` - no macOS machine or Apple certificates |
| INS-2.2-01 | Universal binary (Intel + Apple Silicon) | installer §2.2 | none | none | `blocked` - macOS |
| INS-2.3-01 | `Uninstall.command` with purge option | installer §2.3 | none | none | `blocked` - macOS |
| INS-3-01 | Linux `.tar.gz`, `.deb`, best-effort `.rpm`; file layout incl. desktop file, MIME xml, icons; post-install cache updates; `install.sh`/`uninstall.sh`; dependencies documented | installer §3 | no Linux build (include.md defers Linux) | none | `deferred` - include.md "future versions will need ... a linux version"; qa-polish lists Ubuntu (Conflict C-18) |
| INS-4-01 | Standalone-only bundle per platform | installer §4 | Standalone target builds on Windows | none | `pending` |
| INS-5-01 | Update banner, release notes, Download to Downloads, no auto-launch | installer §5.1 | update banner exists (UT-1-04); download action not verified | none | `partial` |
| INS-5.2-01 | Delta patches < 50% size, one-way, SHA-256 check, rollback | installer §5.2 | none | none | `pending` |
| INS-6-01 | First load creates `~/Documents/Luthier/` tree (18 subfolders), `config/plugin.json`, `.installed_version` marker; absent marker triggers onboarding; version change triggers upgrade path | installer §6 | folders created lazily per feature; no marker (rg `installed_version`) | none | `pending` |
| INS-7-01 | Enterprise: command-line config, pre-placed policy, MSI wrapper on request | installer §7 | policy reader exists (UT-7-01) | `TelemetryTests.cpp` `Telemetry::policyOverridesTheUser` | `partial` |
| INS-8-01 | Migrations: ranges block added and original backed up by date; guitar migration table; loop tags; `.mid` without chunk as Generic; info banner | installer §8 | ranges derivation on load; preset backups on save; guitar migration table `Resources/Guitars/migration.json` (`5ccf238`) | `RangeTests.cpp` `Ranges::theRangesBlockRoundTripsAndDerivesWhenAbsent`; `IntegrationTests.cpp` `Presets::savingBacksUpTheVersionItReplaces`; `GuitarMigrationTests.cpp` `GuitarMigration::everyPreM49NameResolvesToItsShippedGuitar` | `partial` - dated backup at migration time, loop tags, `.mid` as Generic on upgrade and the info banner not built |
| INS-9-01 | Windows portable `.zip` (no registry, no system VST3) | installer §9 | none | none | `pending` |
| INS-10-01 | SHA-256 checksums, PGP-signed manifest, canonical URL | installer §10 | none | none | `pending` |
| INS-11-01 | Signed `.luthiercontent` packages applied to `ContentUpdates/`; data only | installer §11 | none | none | `pending` |
| INS-12-01 | Rollback plan (manifest revert, old installers, downgrade prompt, banner) | installer §12 | none | none | `pending` |
| INS-13-01 | Tests: install/uninstall manifest, upgrade, downgrade, silent + policy, signature, portable, file associations, 200-preset migration within -60 dBFS | installer §13 | - | none | `pending` |

## 32. CLAUDE_CODE_BRIEF.md (phase 4, front door)

Mostly process. Its product-level rules and its definition of done are
rows; the reading order and conflict list are used by this file.

| ID | Requirement (short) | Source (file §section) | Implementation location | Verification | Status |
|---|---|---|---|---|---|
| BRF-ORD-01 | Read the whole spec set in the given order (incl. 9 phase-2b files) | BRIEF §order 1 | this audit | - | `blocked` - phase 2b files missing |
| BRF-ORD-02 | Steps 2-12: audit GI 19, build gaps, resolve ambiguities, wire realism, Tune + MIDI export (round trip -60 dBFS), polish, performance, onboarding, installer, bug bash, human check | BRIEF §order 2-12 | TODO.md tracks; steps 6-12 open | - | `pending` |
| BRF-RUL-01 | Do not add features not in spec; proposals first | BRIEF §rules | `spec/proposals/visual-polish.md` followed this | - | `n/a` - process |
| BRF-RUL-02 | Column 4 tab order fixed | BRIEF §rules | see GI-4.4-01 | - | `partial` |
| BRF-RUL-03 | Follow ui-wiring threading contract exactly | BRIEF §rules | see UW-0-06, Conflict C-09 | - | `pending` |
| BRF-RUL-04 | Every parameter in APVTS, physical ones in PhysicalRange | BRIEF §rules | see UW-0-01, IDX-GR-03 | - | `partial` |
| BRF-RUL-05 | Structural state never through parameters; command/result queue with pointer swaps | BRIEF §rules | no queue | - | `pending` |
| BRF-RUL-06 | Every user-visible string in the locale catalog | BRIEF §rules | partial catalog | `AccessibilityTests.cpp` `Localisation::catalogCoversTheUi` | `partial` |
| BRF-RUL-07 | Every feature has tests in LuthierTests | BRIEF §rules, §Test discipline | 415 declared tests | see per-spec rows | `partial` |
| BRF-RUL-08 | PROGRESS.md updated after every milestone | BRIEF §rules | `spec/PROGRESS.md` | - | `n/a` - process |
| BRF-DONE-01 | Every GI 19 row present in the UI | BRIEF §When you are done | see GI-19-* (many pending) | - | `pending` |
| BRF-DONE-02 | Every test in every spec passes | BRIEF §When you are done | see per-spec Tests rows | - | `pending` |
| BRF-DONE-03 | Every qa-polish section 0 gate green | BRIEF §When you are done | see QA-0-* | - | `pending` |
| BRF-DONE-04 | Final human check performed and passed | BRIEF §When you are done | - | - | `pending` |
| BRF-DONE-05 | PROGRESS.md "READY TO SHIP" marker and signed-off checklist | BRIEF §When you are done | absent | - | `pending` |
| BRF-CON-01 | Genuine unresolved conflict: stop and write a question with proposed answers | BRIEF §Handling conflicts | see Conflicts section (UNRESOLVED items) | - | `implemented` (this file; all five flagged conflicts since decided in DECISIONS) |

## 33. file-formats.md (phase 5)

Precedence 11 (schema, migration, atomicity). The Workshop files follow it
closely; the older formats predate it and use their own top-level keys.

| ID | Requirement (short) | Source (file §section) | Implementation location | Verification | Status |
|---|---|---|---|---|---|
| FF-0-01 | UTF-8 JSON | file-formats §0.1 | all writers | none | `implemented` |
| FF-0-02 | Top-level `schema` integer (missing = 1) | file-formats §0.2 | parts/guitars yes (`Part.cpp:128`, `PartLibrary.cpp:133`); presets use `schemaVersion` (`PresetManager.cpp:357`); setlist and pattern have none | none | `partial` |
| FF-0-03 | Unknown fields preserved on load and written back | file-formats §0.3 | presets (`PresetManager.cpp:350-354`) | `IntegrationTests.cpp` `Presets::unknownFieldsSurviveARoundTrip` | `partial` - presets only verified |
| FF-0-04 | Documented migrations | file-formats §0.4 | ranges derivation; pickup placement migration | `RangeTests.cpp` `Ranges::theRangesBlockRoundTripsAndDerivesWhenAbsent`; `WorkshopPresetTests.cpp` `WorkshopPresets::oldPickupPlacementParametersBecomeTheGuitars` | `partial` |
| FF-0-05 | Canonical extension + `magic` marker per file type | file-formats §0.5, §1 | preset `luthier.preset`, guitar `luthier.guitar`, part `luthier.part`; setlist writes `format: luthierset`; pattern, loop, tune, midprofile, content have no magic | `IntegrationTests.cpp` `Presets::aFileWithoutTheMagicMarkerIsRefused` | `partial` |
| FF-0-06 | Relative paths under registered folders, else absolute | file-formats §0.6 | IR slots; guitar reference `Factory/...` or `User/...` (DECISIONS) | `ToneMatchTests.cpp` `ToneMatch::presetPathsAreRelativeUnderTheLibraryRoot` | `partial` - no registered-folders config |
| FF-1-01 | File type table (12 types) | file-formats §1 | `.luthierpreset`, `.luthierguitar`, `.luthierpart`, `.luthierpattern`, `.luthierset`, `.luthiertune` (`d284547`), `.midprofile` (`Source/Export`) exist; `.luthierloop` (folder instead), `.luthiercontent` absent | none | `partial` |
| FF-2-01 | Preset schema 3: meta block (name, author, category, tags, created/modified, versions, notes), guitar {reference, override}, parameters, ranges, modulation, snapshots, midi_mappings, rhythm_engine, effects_state, midi_out_profile | file-formats §2 | flat root with `name`, `category`, `tags`, `pluginVersion`; `guitar` block; `ranges`; `modulation`; `rhythm` etc. | `IntegrationTests.cpp` `Presets::stateRoundTripsExactly` | `partial` - key names/layout differ from the canonical schema (no `meta` block, no created/modified, no `midi_out_profile`) |
| FF-2-02 | Migration schema 1 -> add stock ranges; schema 2 `guitar.name` -> reference via `migration.json`; original backed up to `Presets/Backup/<date>/` | file-formats §2 | ranges derived (advanced-ranges 4.1 supersedes "add stock"); guitar names resolved via `migration.json` (`5ccf238`); no backup to `Presets/Backup/<date>/` when a load migrates | `RangeTests.cpp` `Ranges::theRangesBlockRoundTripsAndDerivesWhenAbsent`; `GuitarMigrationTests.cpp` `GuitarMigration::aPresetNamingAnOldGuitarLoadsItsReplacement` | `partial` - migration-time backup missing |
| FF-3-01 | `.luthierguitar` schema: meta, parts with references, pickups with position/heights, wiring, strings + overrides, pickguard, hardware_color, finish, setup, character_seed | file-formats §3 | `PartLibrary.cpp:120-260` | `WorkshopTests.cpp` `Workshop::everyFactoryGuitarLoadsAndRoundTrips` | `verified` |
| FF-4-01 | `.luthierpart` schema: meta (name, part_type, author, tags, compatibility), fields, illustration hints | file-formats §4 | `Part.cpp:120-160` | `WorkshopTests.cpp` `Workshop::theFactoryLibraryIsThere` | `verified` |
| FF-5-01 | `.luthiertune` per tune-builder 11 | file-formats §5 | `Source/Tune/TuneFile.*` | `TuneBuilderTests.cpp` `TuneBuilder::aHundredRandomTunesRoundTripByteIdentical`, `TuneBuilder::unknownFieldsAreKeptAndWrittenBack`, `TuneBuilder::loadErrorsAreNamedAndLeaveTheTuneAlone` | `verified` |
| FF-6-01 | `.luthierpattern` per rhythm-engine 6 | file-formats §6 | `Patterns.cpp:160-290` | `RhythmSchedulerTests.cpp` `RhythmPatterns::patternsRoundTripThroughJson` | `implemented` - no schema/magic |
| FF-7-01 | `.luthierset` schema with schema, magic, meta, entries {preset, snapshot, notes} | file-formats §7 | `Setlist.cpp:108-120` (`format`, flat name/notes/bpm_default) | `LiveTests.cpp` `LiveSetlist::roundTripsThroughJson` | `partial` - layout differs |
| FF-8-01 | `.luthierloop` file with MIDI, relative WAV refs, layer settings | file-formats §8 | folder with `loop.json` (`Looper.cpp:746`) | none | `partial` |
| FF-9-01 | `.luthiercontent` signed zip manifest | file-formats §9 | not built | none | `pending` |
| FF-10-01 | `.midprofile` schema | file-formats §10 | not built | none | `pending` |
| FF-12-01 | Common meta rules (name, author, ISO dates, versions, tags, notes) | file-formats §12 | parts/guitars partly; presets lack dates | none | `partial` |
| FF-13-01 | Atomic save: temp, fsync, rename; previous to backup for preset/guitar/tune | file-formats §13 | presets `TemporaryFile` (`PresetManager.cpp:881`); parts/guitars `.tmp` + move (`Part.cpp:221`, `PartLibrary.cpp:252`) | `IntegrationTests.cpp` `Presets::savingBacksUpTheVersionItReplaces` | `partial` - guitar backups and fsync not verified |
| FF-13-02 | Backups older than 30 days pruned on startup | file-formats §13 | `PresetManager::pruneOldBackups` `PresetManager.cpp:838` | `IntegrationTests.cpp` `Presets::savingBacksUpTheVersionItReplaces` | `implemented` - 60-day pruning fixture not confirmed |
| FF-14-01 | Load path: read, UTF-8/JSON/magic, schema, migrate, required fields, references with fallback + named notification, refuse with banner; never overwrite corrupt files | file-formats §14 | `PresetManager` load; `PartLibrary` load report | `IntegrationTests.cpp` `Presets::mutatedPresetsNeverCrashTheLoader`, `ErrorLog::arefusedPresetLoadIsRecordedAndChangesNothing` | `partial` - preset path verified; guitar/part paths less so |
| FF-15-01 | Version discipline: tail-only additions, deprecation period, schema bump on semantic change | file-formats §15 | process | - | `n/a` - process |
| FF-16-01 | Test: every factory file byte-identical round trip (canonical formatting) | file-formats §16 | - | `WorkshopTests.cpp` `Workshop::everyFactoryGuitarLoadsAndRoundTrips` (spec equality, not bytes) | `partial` |
| FF-16-02 | Test: 10 000 mutated bytes load or refuse cleanly | file-formats §16 | - | `IntegrationTests.cpp` `Presets::mutatedPresetsNeverCrashTheLoader` (presets only) | `partial` |
| FF-16-03 | Test: migrations across 200 fixtures | file-formats §16 | - | none | `pending` |
| FF-16-04 | Test: missing guitar -> banner and fallback | file-formats §16 | - | `WorkshopPresetTests.cpp` `WorkshopPresets::aMissingGuitarFileFallsBackToItsType` | `verified` |
| FF-16-05 | Test: kill mid-save 100 times, never partial | file-formats §16 | - | none | `pending` |
| FF-16-06 | Test: 60-day backup pruning at 30 days | file-formats §16 | - | none confirmed | `pending` |

## 34. factory-content.md (phase 5)

Guitars and parts ship (generated by `Tools/generate_factory_parts.py`). The
36 factory presets that ship are the M19 set, **not** section 1's list:
names differ and several carry trademarks. Tunes, pattern files, kit files,
setlists, backing tracks and MIDI clips do not ship.

| ID | Requirement (short) | Source (file §section) | Implementation location | Verification | Status |
|---|---|---|---|---|---|
| FC-0-01 | No trademarks in any shipped name; legal review | factory-content §0.1, §12 | sweep done (`25e6139`, DECISIONS "Trademarks out of every shipped name") | `TrademarkTests.cpp` `Trademarks::noChoiceListNamesABrand`, `Trademarks::noFactoryPresetPartOrGuitarNamesABrand`, `Trademarks::sourceTreeHasNoUnmarkedBrandNames` | `partial` - final legal review is a ship gate |
| FC-0-02 | Tonal spread; difficulty ladder per category; two presets per genre (rock, blues, jazz, country, folk, classical, metal, funk, reggae, latin, indie, ambient; same for bass) | factory-content §0.2-0.4 | current 36 presets lack reggae, latin, indie, bass genres beyond 3 | none | `pending` |
| FC-0-05 | Every factory guitar playable at every factory preset | factory-content §0.5 | - | none | `pending` - no cross-product test |
| FC-0-06 | Every factory tune loops and sounds finished | factory-content §0.6 | no tunes | none | `pending` |
| FC-0-07 | No copyrighted third-party audio, MIDI or images | factory-content §0.7 | IRs synthesised; icon generated | none | `implemented` |
| FC-0-08 | Factory content <= 200 MB compressed | factory-content §0.8 | `Resources/` is 30 MB on disk | `du -sh Resources` = 30M | `verified` |
| FC-1-01 | The 36 named presets (Fresh Strings Clean ... Jazz Walking Bass) with categories and default guitars; Modern Overdrive default first-run | factory-content §1 | `FactoryPresets.cpp` ships a different 36 (Clean Strat Funk, Les Paul Crunch, ... Microtonal Just); only a few names overlap (Modern Metal Chug, Flamenco Rasgueado, Piedmont/Folk variants) | `IntegrationTests.cpp` `Presets::everyFactoryPresetLoadsAndPlays` (current set) | `pending` - preset list not rebuilt to spec |
| FC-2-01 | 15 factory guitars with named designs and finishes | factory-content §2 | all 15 present under `Resources/Guitars/*` plus 12 more (27; DECISIONS) | `WorkshopTests.cpp` `Workshop::everyFactoryGuitarLoadsAndRoundTrips` | `verified` |
| FC-3-01 | ~90 parts across categories with the named items | factory-content §3 | 148 parts; category counts meet or exceed the lists | `WorkshopTests.cpp` `Workshop::theFactoryLibraryIsThere` | `verified` - individual names not diffed against the list |
| FC-4-01 | Six factory tunes | factory-content §4 | not built | none | `pending` |
| FC-5-01 | ~28 `.luthierpattern` files incl. 8 bass patterns | factory-content §5 | patterns compiled in (37 strum/fingerpick); no files; bass patterns absent | `RhythmSchedulerTests.cpp` `RhythmPatterns::factoryPatternsAreWellFormed` | `partial` |
| FC-6-01 | ~28 `.luthierkit` files with voicer, humanize, pick style, setup style, string-noise style | factory-content §6 | kits compiled in (`GenreKit.cpp`); no pick/setup/noise styles; no `.luthierkit` format | `GenreKitTests.cpp` `GenreKits::factoryKitsAreWellFormed` | `partial` - see Conflict C-19 on the extension |
| FC-7-01 | 10 example setlists referencing factory presets only | factory-content §7 | none shipped | none | `pending` |
| FC-8-01 | 6 royalty-free backing tracks in `Resources/Practice/BackingTracks/` | factory-content §8 | none | none | `pending` - needs produced audio |
| FC-9-01 | 12 example MIDI clips in `Resources/Examples/MIDI/` | factory-content §9 | none | none | `pending` |
| FC-10-01 | 720 IRs (216 body, 504 cab) from `make_irs.py` | factory-content §10 | `Resources/BodyIRs`, `Resources/CabIRs` | README counts | `implemented` |
| FC-11-01 | Post-release content packs as `.luthiercontent` | factory-content §11 | not built | none | `deferred` - post-release roadmap |
| FC-12-01 | Naming discipline: catalog names, noun-phrase presets, `[Style] [Family]` guitars, physical-fact part names, tune names < 32 chars | factory-content §12 | guitars and parts follow it; presets do not; names not in catalog | none | `partial` |
| FC-13-01 | Test: every preset in every host, no missing-reference banner | factory-content §13 | - | `IntegrationTests.cpp` `Presets::everyFactoryPresetLoadsAndPlays` (renderer, not hosts) | `partial` |
| FC-13-02 | Test: every factory guitar matches its spectrum-delta fixture within 0.2 dB | factory-content §13 | - | none | `pending` |
| FC-13-03 | Test: every factory tune plays end to end | factory-content §13 | - | none | `pending` |
| FC-13-04 | Test: every factory setlist resolves | factory-content §13 | - | none | `pending` |
| FC-13-05 | Test: every backing track streams at 48 kHz | factory-content §13 | - | none | `pending` |
| FC-13-06 | Legal sign-off recorded per name | factory-content §13 | - | none | `pending` |
| FC-13-07 | Content-size gate <= 200 MB | factory-content §13 | - | manual `du` (30 MB) | `verified` |

## 35. error-recovery.md (phase 5)

Precedence 12 (failure response). The error log and the preset load/save
paths are built and tested; most other failure responses are not. There is
no `Tests/Fixtures/Errors/` folder.

| ID | Requirement (short) | Source (file §section) | Implementation location | Verification | Status |
|---|---|---|---|---|---|
| ER-0-01 | Never crash on bad input | error-recovery §0.1 | loaders | `IntegrationTests.cpp` `Presets::mutatedPresetsNeverCrashTheLoader` | `partial` - presets only |
| ER-0-02 | Never silently degrade; visible notifications | error-recovery §0.2 | `NotificationCentre` | `EditorTests.cpp` `Editor::aFailedPresetLoadAndAMissingIrEachRaiseABannerOnce` | `partial` |
| ER-0-03 | Never destroy user work (refused load untouched; migration backs up; failed save keeps previous) | error-recovery §0.3 | `PresetManager.cpp:881-897` | `IntegrationTests.cpp` `ErrorLog::arefusedPresetLoadIsRecordedAndChangesNothing`, `Presets::savingBacksUpTheVersionItReplaces` | `verified` |
| ER-0-04 | Prefer partial success | error-recovery §0.4 | IR and part fallbacks | `WorkshopTests.cpp` `Workshop::aMissingPartFallsBackAndSaysSo` | `implemented` |
| ER-0-05 | Every failure logged to `Diagnostics/errors-<yyyymm>.log` regardless of telemetry | error-recovery §0.5, §13 | `Source/Support/ErrorLog.cpp` | `IntegrationTests.cpp` `ErrorLog::failuresAreLoggedAsReadableJsonLines` | `verified` - only preset paths report through it |
| ER-0-06 | Severity levels: banner / banner + halt / modal with reset instructions | error-recovery §0.6 | banners only | none | `partial` |
| ER-1-01 | File not found / unreadable / not UTF-8 / not JSON / bad magic -> named banner, keep current state | error-recovery §1 | `PresetManager::getLastLoadError` | `IntegrationTests.cpp` `Presets::aFileWithoutTheMagicMarkerIsRefused`; `EditorTests.cpp` `Editor::aFailedPresetLoadAndAMissingIrEachRaiseABannerOnce` | `verified` - presets |
| ER-1-02 | Newer schema -> refuse with "made by a newer Luthier" banner | error-recovery §1 | loaded with `NEWER_SCHEMA` info log (`PresetManager.cpp:490`; PROGRESS judgement call) | none | `partial` - deviates (Conflict C-20) |
| ER-1-03 | Old schema without migration refused; migration runs with backup and 5 s info banner; failed migration preserves original | error-recovery §1 | ranges derivation silent; no migration banner | none | `partial` |
| ER-1-04 | Missing/corrupt referenced file -> info banner, load succeeds | error-recovery §1 | IR and part/guitar fallbacks with banners | `EditorTests.cpp` `Editor::aFailedPresetLoadAndAMissingIrEachRaiseABannerOnce`; `WorkshopPresetTests.cpp` `WorkshopPresets::aMissingGuitarFileFallsBackToItsType` | `verified` |
| ER-1-05 | Cyclic reference refused and logged | error-recovery §1 | not found | none | `pending` |
| ER-2-01 | Save failures: not writable / disk full / rename failed -> banner, nothing partial | error-recovery §2 | `PresetManager.cpp:894-897` | none | `implemented` |
| ER-2-02 | Concurrent saves: later wins with warning banner; backups keep both | error-recovery §2 | not found | none | `pending` |
| ER-2-03 | Session recorder auto-save avoids a user save's name | error-recovery §2 | timestamped names (`Looper.cpp:966`) | none | `implemented` |
| ER-3-01 | SR change: re-prepare within < 20 ms gap; info banner | error-recovery §3 | `prepareToPlay`; `claimSampleRateChange` banner | `EditorTests.cpp` `Editor::aSampleRateChangeIsAnnouncedOnceAndTheFirstOneIsNot`; `IntegrationTests.cpp` `Engine::sampleRateChangesAreSurvived` | `verified` |
| ER-3-02 | Block-size change re-prepare | error-recovery §3 | - | `IntegrationTests.cpp` `Engine::blockSizeChangesAreSurvived` | `verified` |
| ER-3-03 | Unadvertised bus layout refused via `isBusesLayoutSupported` | error-recovery §3 | `PluginProcessor.cpp:212` | `RoutingTests.cpp` `Routing::everyLayoutRendersCleanly` | `implemented` - declared; specific defaults or the refusal path are not asserted by the cited test |
| ER-3-04 | NaN/denormal guard logs module id; Diagnostics counter, no banner | error-recovery §3 | `sanitise` guards; no logging or counter found | none | `partial` |
| ER-3-05 | CPU overrun relief ladder, "CPU limit reached" banner, offer to disable heaviest module | error-recovery §3 | relief not built (PB-8-01) | none | `pending` |
| ER-3-06 | Host underrun logged + counter | error-recovery §3 | not found | none | `pending` |
| ER-3-07 | Convolution failure -> bypass + banner | error-recovery §3 | procedural cab fallback (no banner) | `EngineTests.cpp` `Cabinet::procedualFallbackRemovesTheFizz` | `partial` |
| ER-4-01 | MIDI: malformed dropped and logged; CC clamped; unknown SysEx ignored; corrupt Luthier SysEx dropped | error-recovery §4 | `MidiInterpreter` | `RoutingTests.cpp` `Routing::midiOutPassThroughIsSampleExact` (fuzz) | `implemented` - logging not verified |
| ER-4-02 | MIDI flood > 5000/s: process what fits, throttled banner | error-recovery §4 | not found | none | `pending` |
| ER-4-03 | MIDI Learn 30 s timeout disarms with banner | error-recovery §4 | not found | none | `pending` |
| ER-5-01 | Workshop failures: queued swap under CPU; incompatible part not offered / refused via API; family change banner; audition timeout; invalid coefficients refused; invalid guitar save refused | error-recovery §5 | compatibility is advisory (Conflict C-21); others not built | `WorkshopTests.cpp` `Workshop::incompatiblePartsFitWithAWarning` | `pending` |
| ER-6-01 | Tune Builder failures (parse highlight, empty melody, low pitch confidence, zero sections, long loop) | error-recovery §6 | named parse errors with position; named load errors; no UI | `TuneBuilderTests.cpp` `TuneBuilder::malformedShorthandIsRefusedWithANamedError`, `TuneBuilder::loadErrorsAreNamedAndLeaveTheTuneAlone` | `partial` - highlight, pitch confidence and UI pending |
| ER-7-01 | Snapshot recall queued during preset load | error-recovery §7 | - | `StateModelTests.cpp` `StateModel::aProgramChangeRightAfterAStateRestoreDoesNotWipeIt` | `partial` |
| ER-7-02 | Recall while looper records; preset load mid-tune; undo with nothing; empty B slot banner | error-recovery §7 | undo disables; others not found | none | `partial` |
| ER-8-01 | Network: update error silent + log; interrupted download discarded; crash upload failure banner with path; offline activation fallback; grace countdown | error-recovery §8 | `Telemetry` | `TelemetryTests.cpp` `Telemetry::noNetworkIsSilentRatherThanAnError`, `Telemetry::revalidationCountdownAndOfflineTolerance` | `partial` |
| ER-8-02 | Grace expired: modal + demo mode (mute 2 s every 60 s) | error-recovery §8 | not found | none | `pending` |
| ER-9-01 | Host close saves state and releases | error-recovery §9 | JUCE lifecycle | none | `implemented` |
| ER-9-02 | Standalone device / MIDI loss: poll, auto-recover, banners | error-recovery §9 | not found | none | `pending` |
| ER-10-01 | Corrupt user config renamed `.corrupted-<ts>`, defaults written, banner; missing folders created | error-recovery §10 | `UiPreferences` defaults at call sites; no rename | none | `partial` |
| ER-11-01 | Broken first-run environment (no Documents, old OS, no device) prompts | error-recovery §11 | not found | none | `pending` |
| ER-12-01 | Crash: minidump, troubleshooting bundle, upload prompt or info banner | error-recovery §12 | no dump writer (UT-4-02) | none | `pending` |
| ER-13-01 | Log format: JSON lines `ts, severity, module, code, message, context`; debug/info only when verbose; monthly rotation; 30-day prune | error-recovery §13 | `ErrorLog.cpp:102` | `IntegrationTests.cpp` `ErrorLog::failuresAreLoggedAsReadableJsonLines` | `verified` |
| ER-14-01 | Up to 3 banners visible; 4th replaces oldest; priority errors > warnings > info; 5 s rule | error-recovery §14 | one banner visible, rest queued "+N" (PROGRESS A6) | `EditorTests.cpp` `Editor::notificationBannersQueueDismissAndRespectTheirActions` | `partial` - deviates (Conflict C-22) |
| ER-15-01 | Every failure mode has a fixture in `Tests/Fixtures/Errors/` and a test | error-recovery §15 | folder absent | none | `pending` |

## 36. state-model.md (phase 5)

Precedence 13 (state layers and intersections). Four tests exist
(`StateModelTests.cpp`); the intersection matrix is mostly untested and
several layers (tune, workshop bench, shadow audition, looper state
boundaries) do not exist yet. No `Tests/StateModel/Intersections/` folder.

| ID | Requirement (short) | Source (file §section) | Implementation location | Verification | Status |
|---|---|---|---|---|---|
| SM-0-01 | Layers nest (guitar and snapshots in preset; tune and loop siblings) | state-model §0.1, §1 | preset carries guitar block and snapshots | `StateModelTests.cpp` `StateModel::recallingASnapshotStaysInsideThePreset` | `implemented` |
| SM-0-02 | Loads atomic per layer at an audio-thread swap, never mid-block | state-model §0.2 | message-thread apply (UW-5-01) | `StateModelTests.cpp` `StateModel::loadingAPresetWhileRenderingProducesNoGarbage` | `partial` |
| SM-0-04 | Undo respects state boundaries (Shift to cross) | state-model §0.4 | no state-boundary concept in the undo stack found | none | `pending` |
| SM-0-05 | Structural state via command/result queue | state-model §0.5 | not built | none | `pending` |
| SM-0-06 | uiState per instance, not in presets | state-model §0.6, §1 | `UiState`; `UiPreferences` for user-global | `StateModelTests.cpp` `StateModel::loadingAPresetLeavesTheLayersAboveItAlone` | `verified` |
| SM-2-01 | Preset load: resolve override / reference / fallback + banner; resolve IRs and patterns; apply params, guitar, mod matrix, snapshots, mappings, ranges; 5-30 ms crossfades; push state boundary; banners | state-model §2 | `PresetManager::loadPreset` + processor (message thread) | `StateModelTests.cpp` `StateModel::loadingAPresetLeavesTheLayersAboveItAlone`; `WorkshopPresetTests.cpp` `WorkshopPresets::aStateLoadKeepsItsOwnRefinements` | `partial` - no boundary, no crossfade |
| SM-2-02 | Preset load never touches user-global, uiState, session, setlist, tune, loop state | state-model §2 | - | `StateModelTests.cpp` `StateModel::loadingAPresetLeavesTheLayersAboveItAlone` | `verified` - for the layers that exist |
| SM-3-01 | Snapshot recall: params, modulation diff, 30 ms crossfade, discrete at midpoint, tails preserved; undo entry not boundary; never touches file, ranges, guitar, recorder, upper layers | state-model §3 | `SnapshotBank` | `StateModelTests.cpp` `StateModel::recallingASnapshotStaysInsideThePreset`; `LiveTests.cpp` `LiveSnapshots::recallCrossfadesContinuousAndStepsDiscrete` | `partial` - tails double-buffer and undo entry not verified |
| SM-4-01 | Tune load semantics (lazy presets, bundle extraction, stop playback, boundary) | state-model §4 | no Tune | none | `pending` |
| SM-5-01 | Setlist load: verify entries, flag unresolved, session state, load first entry | state-model §5 | `SetlistPlayer` | `LiveTests.cpp` `LiveSetlist::reportsPreviousCurrentAndNext` | `partial` - unresolved flagging not verified |
| SM-6-01 | Guitar load: resolve parts with fallback banner; swap; reprepare; 30 ms crossfade; preset params on top; boundary | state-model §6 | processor guitar loader; 5 ms fade out/in (DECISIONS) | `WorkshopPresetTests.cpp` `WorkshopPresets::aStateLoadKeepsItsOwnRefinements`, `WorkshopSwap::aPartSwapDuringANoteIsClickFree` | `partial` - no boundary |
| SM-7-01 | Part swap never a boundary; own undo entry | state-model §7 | no bench | none | `pending` |
| SM-8.1-01 | Preset load while: snapshot recall superseded; tune pauses; setlist override flag; looper/recorder capture boundary; MIDI Learn arm persists or disarms; bench prompt; Slide/Live/drawer persist; A/B clears with banner; Freeze/E-Bow clear; feedback damps 100 ms; held notes decay | state-model §8.1 | partly by construction; A/B-clear banner, freeze clear, setlist override not found | `StateModelTests.cpp` `StateModel::aProgramChangeRightAfterAStateRestoreDoesNotWipeIt` | `partial` |
| SM-8.2-01 | Snapshot recall while: held notes, tune, setlist, looper, Learn, Freeze/E-Bow clear, A/B replace, feedback damp | state-model §8.2 | partly | `LiveTests.cpp` `LiveSnapshots::thousandRecallsNeverJumpAParameter` | `partial` |
| SM-8.3-01 | Tune load intersections | state-model §8.3 | no Tune | none | `pending` |
| SM-8.4-01 | Guitar load while: held notes / string count change, unsaved bench prompt, in-flight swap discarded, looper boundary, Slide banner | state-model §8.4 | partly (swap parking) | none | `partial` |
| SM-8.5-01 | Part swap while: held notes crossfade, swaps queue, audition drops back | state-model §8.5 | no bench | none | `pending` |
| SM-8.6-01 | Ranges toggle while automated / modulated: clamps and writes clamped value; recorder boundary | state-model §8.6 | `RangeState` clamp | `RangeTests.cpp` `Ranges::narrowingClampsAndReportsTheCount` | `partial` |
| SM-8.7-01 | MIDI Learn arm while flood / recall / preset load | state-model §8.7 | - | none | `pending` |
| SM-8.8-01 | Undo while held notes / tune / looper | state-model §8.8 | - | none | `pending` |
| SM-9-01 | Save interactions (bank captured, wait for swap, committed not shadow, tune live edits) | state-model §9 | swap parks audio so save sees a settled spec | none | `partial` |
| SM-10-01 | Persistence list: files, host state (params, matrix, bank, mappings, ranges, guitar ref, uiState); not persisted: undo, A/B, arm, tap, ring, freeze, feedback, shadow | state-model §10 | `getStateInformation` | `IntegrationTests.cpp` `Presets::stateRoundTripsExactly` | `implemented` |
| SM-11-01 | Multi-instance: no shared state beyond user-global; atomic writes; independent learn and recorder | state-model §11 | per-instance objects; `ExpressionCalibrationSet` global singleton | none | `implemented` - not tested |
| SM-12-01 | Diagnostics "State Inspector" live tree at 4 Hz | state-model §12 | not found | none | `pending` |
| SM-13-01 | Test per section 8 intersection in `Tests/StateModel/Intersections/`; "never touches" test per load path | state-model §13 | 4 tests in `StateModelTests.cpp` | see above | `partial` |
| SM-13-02 | Fuzz 10 000 random operations across layers | state-model §13 | - | none | `pending` |

## 37. gui-engine-dataflow.md (phase 5)

Precedence 14 (live UI drain rates and staleness). The build has no display
FIFO: live elements poll engine atomics from component timers. Staleness
greying exists only on the noise-event strip and buzz heatmap. No
`Tests/Ui/Dataflow/` folder.

| ID | Requirement (short) | Source (file §section) | Implementation location | Verification | Status |
|---|---|---|---|---|---|
| GED-0-01 | Each live element reads one source (APVTS, display FIFO, uiState, parts model) | gui-engine-dataflow §0.1 | engine getters polled; no FIFO | none | `partial` |
| GED-0-02 | Fixed drain rate per element; stale rule per element; no busy-wait; elements don't read each other; UI never triggers audio work | gui-engine-dataflow §0.2-0.6 | per-component timers | none | `partial` |
| GED-2-01 | Input and output meters in header: 30 Hz, stale 200 ms -> -inf with 20 dB/s decay | gui-engine-dataflow §2 | output meter + LED in header; no input meter | none | `partial` |
| GED-2-02 | Aux meters (8) in ROUTING at 30 Hz; muted skip compute | gui-engine-dataflow §2 | `RoutingPanel` meters (7) | none | `partial` |
| GED-2-03 | Per-string activity: 60 Hz fretboard, 30 Hz strip, stale 100 ms dark | gui-engine-dataflow §2 | `FretboardComponent` timer | none | `implemented` - rates not verified |
| GED-3-01 | Output LED: 60 Hz, stale 100 ms unlit #5A5F66, linear to #FFFFFF at 0 dBFS, red #F2544E held 400 ms | gui-engine-dataflow §3 | `OutputLed` `Widgets.cpp` | none | `implemented` - colours/hold not verified |
| GED-4-01 | Chord readout: 10 Hz, stale 3 s -> dimmed last chord | gui-engine-dataflow §4 | `RhythmPanel` live indicator | none | `implemented` - stale dimming not verified |
| GED-5-01 | Next-strum arrow: 60 Hz, stale 500 ms hidden, lights on beat | gui-engine-dataflow §5 | `RhythmPanel` | none | `implemented` - Easy strip readout missing (GI-3.5) |
| GED-6.1-01 | Played-notes layer: dot sized by amplitude, 60 ms alpha decay, 60 Hz | gui-engine-dataflow §6.1 | `FretboardComponent` | none | `implemented` |
| GED-6.2-01 | Scale highlight from uiState | gui-engine-dataflow §6.2 | fretboard scale overlay | none | `implemented` |
| GED-6.3-01 | Buzz heatmap: 30 Hz, stale 500 ms fade 200 ms, > 6 dB transparent / 0-6 warning / buzzing accent + dot | gui-engine-dataflow §6.3 | `SetupGroup` | `BuzzTests.cpp` `BuzzUi::heatmapCellsReadInMonochromeTerms` | `partial` - stale fade not verified |
| GED-6.4-01 | Slide bar overlay: 60 Hz, stale 200 ms frozen at 60% alpha, rotated by slant | gui-engine-dataflow §6.4 | `FretboardComponent.cpp:168-183` | none | `implemented` |
| GED-6.5-01 | Pick overlay from `pick_state`, 30 Hz, stale 500 ms | gui-engine-dataflow §6.5 | not built | none | `pending` |
| GED-6.6-01 | Pickup pulse layer: pole pieces brighten 40 ms per NoteOn | gui-engine-dataflow §6.6 | not built | none | `pending` |
| GED-7-01 | Mod arcs from per-destination contribution, 30 Hz, stale 500 ms desaturate, per-source segments | gui-engine-dataflow §7 | single-colour arc (`Widgets.cpp:562`) | none | `partial` |
| GED-8-01 | Circuit visualiser 64-point curve, updates within 100 ms of a knob move | gui-engine-dataflow §8 | `CircuitResponseView` | none | `implemented` |
| GED-9-01 | Spectrum delta pane via worker, 10 Hz poll, stale 30 s placeholder | gui-engine-dataflow §9 | not built | none | `pending` |
| GED-10-01 | Noise strip 15 Hz; events fall off after 1 s; reduced motion -> per-class count at 5 Hz | gui-engine-dataflow §10 | `NoiseEventStrip` (8 s window, 2 s stale per string-squeak 9.1) | `NoiseTests.cpp` `NoiseUi::theEventStripShowsWhatTheEngineTriggered` | `partial` - window and stale rules differ (Conflict C-23) |
| GED-11-01 | Footer voice count and CPU at 4 Hz; stale 5 s "-" | gui-engine-dataflow §11 | footer CPU only (`PluginEditor.cpp:329`) | none | `partial` |
| GED-12-01 | Data stream 200 lines, stop after 500 ms idle, off under reduced motion | gui-engine-dataflow §12 | data stream widget | `AccessibilityTests.cpp` `Accessibility::reducedMotionRemovesAnimation` | `implemented` |
| GED-13-01 | Snapshot strip names/colours from bank, active from current index | gui-engine-dataflow §13 | `LiveStrip` | none | `implemented` |
| GED-14-01 | Setlist triptych from uiState | gui-engine-dataflow §14 | `LiveStrip` | `LiveTests.cpp` `LiveSetlist::reportsPreviousCurrentAndNext` | `implemented` |
| GED-15-01 | Tap LED in header flashes on beat, 60 Hz, stale 500 ms | gui-engine-dataflow §15 | tap pad in Live strip; no header LED | none | `pending` |
| GED-16-01 | Kill pill from `kill_switch_active` parameter | gui-engine-dataflow §16 | kill switch is not a parameter (engine state) | `LiveTests.cpp` `LiveKillSwitch::fadesRatherThanJumping` | `partial` |
| GED-18-01 | Preset browser guitar thumbnails: SHA-keyed cache, worker render < 100 ms, 200 x 128x256 | gui-engine-dataflow §18 | not built (TODO V) | none | `pending` |
| GED-19-01 | A/B state highlight | gui-engine-dataflow §19 | header A/B buttons | none | `implemented` |
| GED-20-01 | MIDI Learn button and target pulse at 1 Hz when armed | gui-engine-dataflow §20 | global arm overlay | none | `implemented` - pulse not verified |
| GED-21-01 | Practice drawer loop LED red/green/off, 4 Hz pulse | gui-engine-dataflow §21 | `PracticePanel` | none | `implemented` |
| GED-22-01 | Feedback readout LED in SUSTAIN at 30 Hz | gui-engine-dataflow §22 | `FeedbackLed` (`Widgets.cpp`) polls `FeedbackLoop` activity with `startTimerHz (20)` | none | `partial` - 20 Hz, not 30 |
| GED-23-01 | Session recorder buffer bar at 1 Hz | gui-engine-dataflow §23 | SESSION tab | none | `implemented` - not verified |
| GED-24-01 | Tune transport playhead at 30 Hz | gui-engine-dataflow §24 | no Tune | none | `pending` |
| GED-25-01 | Illustration static layers cached per spec change; live overlays on top; hit-test invalidated on change | gui-engine-dataflow §25 | `GuitarBodyComponent` caches geometry | none | `partial` |
| GED-26-01 | Per element: latency/drain unit test, integration test, stale-state test, reduced-motion test in `Tests/Ui/Dataflow/` | gui-engine-dataflow §26 | - | none | `pending` |
| GED-27-01 | Diagnostics "Show data-flow overlay" per-element counters | gui-engine-dataflow §27 | not found | none | `pending` |

## 38. guitar-illustration.md (phase 5)

Precedence 15 (the visible guitar). TODO G landed in `a406915`:
`Source/UI/Guitar/GuitarRenderer.*`, generated `BodyOutlines.h` and
`HeadstockOutlines.h`, and `GuitarBodyComponent` now draws the parts guitar
that is playing. Family switching landed in `e60d708`
(`PartLibrary::switchFamily`, `LuthierAudioProcessor::switchGuitarFamily`).
Tests: `GuitarRendererTests.cpp` (11 `GuitarIllustration::*`) and
`WorkshopFamily::aFamilySwitchKeepsWhatSection12_4Keeps`; 448 tests passed at
`0d225f0`. DECISIONS amends four points: visual-polish 1 lighting (C-25),
bursts follow the outline (C-49), strings over the neck (C-47), no bolt dots
on the top view (C-48). Still open: zoom and pan, live overlays beyond notes
and slide, thumbnails in the browser, Workshop drag targets.

| ID | Requirement (short) | Source (file §section) | Implementation location | Verification | Status |
|---|---|---|---|---|---|
| GIL-0-01 | Procedurally drawn from GuitarSpec; one renderer for Easy, Workshop, thumbnails | guitar-illustration §0.1 | `Source/UI/Guitar/GuitarRenderer.*`; `GuitarBodyComponent` draws with it | `GuitarRendererTests.cpp` `GuitarIllustration::everyFactoryGuitarRendersWithoutClipping` | `partial` - bench and preset-browser thumbnails not wired |
| GIL-0-02 | Flat visual language: fills, single highlight, low-opacity grain, one gradient per burst | guitar-illustration §0.2 | superseded by visual-polish 1 lighting (DECISIONS "Guitar illustration materials"); flat under High contrast | `GuitarRendererTests.cpp` `GuitarIllustration::highContrastHasNoLighting` | `verified` - as amended |
| GIL-0-03 | Every part a first-class visual that changes on swap | guitar-illustration §0.3 | `GuitarRenderer` | `GuitarRendererTests.cpp` `GuitarIllustration::everyFactoryGuitarHasItsParts`, `GuitarIllustration::theKeyChangesWithEveryVisibleChange` | `verified` |
| GIL-0-04 | Family switch rebuilds the whole illustration | guitar-illustration §0.4, §12 | `PartLibrary::switchFamily`, `LuthierAudioProcessor::switchGuitarFamily` (`e60d708`) | `GuitarRendererTests.cpp` `GuitarIllustration::aFamilySwitchGivesTheTargetFamilysGuitar` | `verified` |
| GIL-0-05 | Drawn = loaded (never a visible pickup the engine lacks) | guitar-illustration §0.5 | renderer reads the playing `WorkshopGuitar` | `GuitarRendererTests.cpp` `GuitarIllustration::everyFactoryGuitarHasItsParts` | `verified` |
| GIL-0-07 | Full repaint < 8 ms; overlay < 2 ms | guitar-illustration §0.7, §17 | cached static scene | `GuitarRendererTests.cpp` `GuitarIllustration::fullRenderIsFastEnough` (asserts static render < 120 ms) | `partial` - test bar looser than the 8 ms / 40 ms budgets |
| GIL-1-01 | mm coordinates, origin at saddle, X to headstock, Y to treble; fit zoom; Ctrl-scroll zoom to 4x with pan | guitar-illustration §1 | `GuitarRenderer` scene in saddle-origin mm; zoom/pan only in the uncommitted `WorkshopPanel.h` | none | `partial` - zoom and pan not committed |
| GIL-2.1-01 | Static scene cached by GuitarSpec hash; invalidated on swap/position/finish change | guitar-illustration §2.1 | `GuitarRenderer::keyFor` | `GuitarRendererTests.cpp` `GuitarIllustration::theKeyChangesWithEveryVisibleChange` | `verified` |
| GIL-2.2-01 | Live overlays per frame from display FIFO; never invalidate the cache | guitar-illustration §2.2 | overlays for played notes, slide bar, hover, selection (`a406915`), read by polling (no FIFO) | none | `partial` |
| GIL-2.3-01 | Thumbnails 128x256 on worker, hash cache, ~40 ms first render | guitar-illustration §2.3, §15 | renderer thumbnail path; preset browser does not show them | none | `partial` |
| GIL-3-01 | Families electric/acoustic/classical/bass/resonator/extended with defaults; incompatible parts replaced with a banner | guitar-illustration §3 | `PartLibrary::switchFamily`; no `extended` family (DECISIONS, C-24) | `GuitarRendererTests.cpp` `GuitarIllustration::aFamilySwitchGivesTheTargetFamilysGuitar`; `WorkshopPresetTests.cpp` `WorkshopFamily::aFamilySwitchKeepsWhatSection12_4Keeps` | `verified` |
| GIL-4.1-01 | Electric bodies: single-cut arched, double-cut offset, T slab, thin double-cut, offset contoured, angular, V, reverse, superstrat, semi-hollow, archtop, multi-scale with the listed dimensions | guitar-illustration §4.1 | `Source/UI/Guitar/BodyOutlines.h` (from `Tools/body_outlines.py`) | `GuitarRendererTests.cpp` `GuitarIllustration::everyFactoryGuitarRendersWithoutClipping` (factory guitars only), `GuitarIllustration::contactSheetForReview` (by eye) | `implemented` - styles without a factory guitar are not exercised |
| GIL-4.2-01 | Acoustic bodies: dreadnought, GA, OM, 000, parlor, jumbo, 12-string jumbo, Selmer-style (oval/D hole) | guitar-illustration §4.2 | `BodyOutlines.h` | as GIL-4.1-01 | `implemented` |
| GIL-4.3-01 | Classical, flamenca (golpeador, wooden pegs), cutaway classical | guitar-illustration §4.3 | `BodyOutlines.h`, `HeadstockOutlines.h` | as GIL-4.1-01 | `implemented` |
| GIL-4.4-01 | Bass bodies: P, J, MM-style, T-bird-style, hollow, multi-scale, acoustic bass, headless | guitar-illustration §4.4 | `BodyOutlines.h` | as GIL-4.1-01 | `implemented` |
| GIL-4.5-01 | Resonators: steel body, wood body, square-neck | guitar-illustration §4.5 | `BodyOutlines.h` | as GIL-4.1-01 | `implemented` |
| GIL-5-01 | 25-layer z-order (shadow ... truss rod cover) | guitar-illustration §5 | `GuitarRenderer` layer order; strings drawn over the neck (DECISIONS "Strings draw over the neck", C-47) | `GuitarRendererTests.cpp` `GuitarIllustration::hitTestingFindsThePartOnTop` | `verified` - as amended |
| GIL-5-02 | Live layers 26-32 (notes, heatmap, slide, pick, pickup pulse, hover, drag ghost) | guitar-illustration §5 | notes, slide bar, hover, selection overlays; no heatmap, pick, pickup pulse, drag ghost | none | `partial` |
| GIL-6-01 | Neck joints (bolt dots, set heel, neck-through seam); radius shading; fretboard wood colours; inlay styles; headstock layouts (3+3, 6-in-line, reverse, 4-in-line, 2+2, slotted, 6+6); truss cover; faint "L" mark | guitar-illustration §6 | `GuitarRenderer`, `HeadstockOutlines.h`; pocket seam instead of bolt dots (DECISIONS, C-48) | `GuitarRendererTests.cpp` `GuitarIllustration::everyFactoryGuitarHasItsParts` | `implemented` |
| GIL-7-01 | Bridge drawings: TOM + stopbar, vintage trem with arm, 2-point, Floyd with locking nut, hardtail ferrules, wraparound, Bigsby; pin, pinless, floating + trapeze, moustache, tie-block, biscuit/spider; bass vintage, high-mass, mutes | guitar-illustration §7 | `GuitarRenderer` bridges and tailpieces by type | `GuitarRendererTests.cpp` `GuitarIllustration::everyFactoryGuitarHasItsParts` | `implemented` - per-type drawings not individually asserted |
| GIL-8-01 | Pickup drawings (single open/covered, humbucker open/closed, P90, mini, Firebird, split-P, J, MM, piezo indicator, soundhole); pole spacing; cover colour; rings | guitar-illustration §8 | `GuitarRenderer` pickups by family and cover | `GuitarRendererTests.cpp` `GuitarIllustration::everyFactoryGuitarHasItsParts` | `implemented` |
| GIL-9-01 | Pickguard colours and per-body default outlines | guitar-illustration §9 | `GuitarRenderer`, `BodyOutlines.h` pickguards | none | `implemented` |
| GIL-10-01 | String colours and styles per material (16 rows with hex); counts per family; 12-string pairs; per-string override colour | guitar-illustration §10 | `GuitarRenderer` strings | `GuitarRendererTests.cpp` `GuitarIllustration::stringColoursFollowSection10` | `verified` |
| GIL-11-01 | Finish block: type, color_a/b, burst_shape, gloss, aging | guitar-illustration §11 | finish in `.luthierguitar`; renderer finishes (WIP) | `WorkshopTests.cpp` `Workshop::everyFactoryGuitarLoadsAndRoundTrips` (data) | `implemented` |
| GIL-11.1-01 | Solid finish palette (18 named colours with hex) | guitar-illustration §11.1 | renderer (WIP) | none run | `implemented` |
| GIL-11.2-01 | Bursts (8 styles with stops); grain over burst | guitar-illustration §11.2 | burst follows the outline (DECISIONS "A burst follows the outline", C-49) | none | `implemented` - as amended |
| GIL-11.3-01 | Transparent 60% tint; natural; metallic highlight; sparkle 3% dots | guitar-illustration §11.3-11.6 | renderer (WIP) | none run | `implemented` |
| GIL-11.7-01 | Relic aging: edge wear, buckle wear, fade, yellowing, seeded dings, checking | guitar-illustration §11.7 | `GuitarRenderer` aging seeded by the character seed | `GuitarRendererTests.cpp` `GuitarIllustration::agingIsSeededAndStable` | `verified` |
| GIL-11.8-01 | Hardware colours nickel/chrome/gold/black/aged nickel/aged gold applied to all metal parts | guitar-illustration §11.8 | renderer (WIP) | none run | `implemented` |
| GIL-12.1-01 | Family change from drawer: session-first confirmation, 250 ms crossfade, banner listing replaced parts | guitar-illustration §12.1 | drawer "Guitar" category first; confirmation on the session's first switch; `switchGuitarFamily` banner; undoable | `WorkshopPanelTests.cpp` `WorkshopPanel::theGuitarCategorySwitchesFamily`; `WorkshopPresetTests.cpp` `WorkshopFamily::aFamilySwitchKeepsWhatSection12_4Keeps` | `partial` - 250 ms crossfade not tested |
| GIL-12.2-01 | Family change mechanics with per-family default templates (6 `*_default_template.luthierguitar`) | guitar-illustration §12.2 | templates are the named factory guitars (DECISIONS "Family templates"); no `extended` template (C-24) | `GuitarRendererTests.cpp` `GuitarIllustration::aFamilySwitchGivesTheTargetFamilysGuitar` | `verified` - as amended |
| GIL-12.3-01 | What changes on family switch (body, strings, scale, bridge, pickups, nut, frets, tuners, wiring, amp defaults; triggers bass mode, slide hint, MIDI profile default) | guitar-illustration §12.3 | template body, setup, finish, hardware and non-suiting parts replaced; string count and tuning follow | `WorkshopPresetTests.cpp` `WorkshopFamily::aFamilySwitchKeepsWhatSection12_4Keeps`, `WorkshopPresets::choosingATypeGivesItsStringCount` | `partial` - amp defaults, bass mode, slide hint, MIDI profile default not done |
| GIL-12.4-01 | Preserved across switch: preset meta, effects, amp, mod matrix, snapshots, seed | guitar-illustration §12.4 | `switchGuitarFamily` | `WorkshopPresetTests.cpp` `WorkshopFamily::aFamilySwitchKeepsWhatSection12_4Keeps` | `verified` |
| GIL-13.1-01 | Hit regions from part polygons; top-of-z wins; Alt audition, Shift below-top, Ctrl string override drag | guitar-illustration §13.1 | `GuitarRenderer` hit test | `GuitarRendererTests.cpp` `GuitarIllustration::hitTestingFindsThePartOnTop` | `partial` - modifier behaviours need the bench UI |
| GIL-13.2-01 | Drag targets for part cards (pickup, bridge, string, body, pick, slide, capo) | guitar-illustration §13.2 | - | none | `pending` |
| GIL-13.3-01 | Direct drags (pickup position, heights, saddles, nut slots, capo, pick, slide) with snap and modifiers | guitar-illustration §13.3 | - | none | `pending` |
| GIL-14-01 | Frequency-band warm/cool tint during audition; static label under reduced motion | guitar-illustration §14 | - | none | `pending` |
| GIL-15-01 | Thumbnail reduced detail rules | guitar-illustration §15 | - | none | `pending` |
| GIL-16-01 | Accessible child per part with documented strings; announce selection; arrow/Tab navigation; Enter to inspector; reduced-motion rules | guitar-illustration §16 | per-part descriptions in the scene; builder-order Tab walk only in the uncommitted `WorkshopPanel.h` | `GuitarRendererTests.cpp` `GuitarIllustration::accessibleDescriptionsNameTheParts` | `partial` - no accessibility handlers committed |
| GIL-17-01 | Performance: cache miss <= 40 ms, hit <= 2 ms, overlay <= 2 ms, family switch <= 120 ms, thumbnail <= 100 ms, cache ~2.4 MB | guitar-illustration §17 | - | none | `pending` |
| GIL-18-01 | New shapes as data (parts), families enumerated in code | guitar-illustration §18 | part `illustration` hints (`file-formats` 4) | none | `partial` |
| GIL-19-01 | Test: every factory guitar at 5 widths without clipping or missing parts | guitar-illustration §19 | - | `GuitarRendererTests.cpp` `GuitarIllustration::everyFactoryGuitarRendersWithoutClipping`, `GuitarIllustration::everyFactoryGuitarHasItsParts` | `verified` |
| GIL-19-02 | Test: SVG hash changes on every swap and > 0.5 mm move | guitar-illustration §19 | - | `GuitarRendererTests.cpp` `GuitarIllustration::theKeyChangesWithEveryVisibleChange` | `verified` |
| GIL-19-03 | Test: every family to every family gives a valid spec | guitar-illustration §19 | - | `GuitarRendererTests.cpp` `GuitarIllustration::aFamilySwitchGivesTheTargetFamilysGuitar` | `verified` |
| GIL-19-04 | Test: 10 000 random clicks per factory guitar select the intended part | guitar-illustration §19 | - | `GuitarRendererTests.cpp` `GuitarIllustration::hitTestingFindsThePartOnTop` | `verified` |
| GIL-19-05 | Test: drag bounds (route, 0.8 mm height) | guitar-illustration §19 | - | none | `pending` |
| GIL-19-06 | Test: string colours within 1 hex step | guitar-illustration §19 | - | `GuitarRendererTests.cpp` `GuitarIllustration::stringColoursFollowSection10` | `verified` |
| GIL-19-07 | Test: 3-tone burst vs reference SVG | guitar-illustration §19 | - | none | `pending` |
| GIL-19-08 | Test: seeded aging dings repeat | guitar-illustration §19 | - | `GuitarRendererTests.cpp` `GuitarIllustration::agingIsSeededAndStable` | `verified` |
| GIL-19-09 | Test: played-note dot within 60 ms, 60 ms decay | guitar-illustration §19 | - | none | `pending` |
| GIL-19-10 | Test: family switch preserves name, rack, matrix, seed | guitar-illustration §19 | - | `WorkshopPresetTests.cpp` `WorkshopFamily::aFamilySwitchKeepsWhatSection12_4Keeps` | `verified` |
| GIL-19-11 | Test: reduced motion static overlays | guitar-illustration §19 | - | none | `pending` |
| GIL-19-12 | Test: thumbnail cold < 100 ms, hit < 1 ms, eviction at 200 | guitar-illustration §19 | - | none | `pending` |
| GIL-19-13 | Test: every part Tab-reachable; announcements match fixture strings | guitar-illustration §19 | - | `GuitarRendererTests.cpp` `GuitarIllustration::accessibleDescriptionsNameTheParts` (strings only) | `partial` |

## 39. input-routing.md (phase 5)

Precedence 16 (consumer order and veto rules). The MIDI chain's first steps
follow the spec (`PluginProcessor::processBlock`: MIDI-out capture
`:821` -> MIDI Learn `:856` -> program change / bank select `:860` ->
engine). There is no root file-drop handler, no MIDI clock handling and no
`Tests/InputRouting/` folder.

| ID | Requirement (short) | Source (file §section) | Implementation location | Verification | Status |
|---|---|---|---|---|---|
| IR-0-01 | One documented path per input; consumers veto or pass; focus wins; MIDI via host buffer | input-routing §0 | - | none | `partial` |
| IR-1-01 | MIDI order: MIDI-out pass-through -> MIDI Learn -> controller profile -> interpreter -> technique -> rhythm -> tune -> practice -> strings | input-routing §1 | `PluginProcessor.cpp:818-860` then engine | `RoutingTests.cpp` `Routing::midiOutPassThroughIsSampleExact` | `partial` - tune stage absent; order past Learn not tested |
| IR-1.1-01 | Learn consumes first non-note event (note learn opt-in); rhythm engine consumes chord notes; technique tags only | input-routing §1.1 | `MidiLearn::processMidi` | `IntegrationTests.cpp` `MidiLearn::mapsAndUnmapsCleanly` | `partial` - note-learn option not found |
| IR-1.2-01 | CC routing: macro CC sources, learned mappings, expression calibration remap; CCs never reach strings/rhythm directly | input-routing §1.2 | `MidiLearn`, `ExpressionCalibrationSet` | `LiveTests.cpp` `LiveExpression::calibrationMapsRealTravelOntoFullRange` | `implemented` |
| IR-1.3-01 | Pitch bend / AT via interpreter; channel pressure via mapping | input-routing §1.3 | `MidiInterpreter` | `ControllerTests.cpp` `Controllers::pitchDeadZoneRejectsTrackingNoiseButNotRealBends` | `implemented` |
| IR-1.4-01 | PC -> snapshot; Bank Select -> preset when "Bank + PC" mapping is set; PC never reaches strings | input-routing §1.4 | `handleLiveMidi` `PluginProcessor.cpp:1240-1271` (always, no mapping switch) | `LiveTests.cpp` `LiveSnapshots::programChangeMapsAcrossAllOneTwentyEight` | `partial` - no "Bank + PC" setting |
| IR-1.5-01 | Luthier SysEx dispatched per class (character, workshop, ranges); other SysEx ignored | input-routing §1.5 | not built | none | `pending` |
| IR-1.6-01 | MIDI clock / Start / Stop / Continue / SPP to tap tempo and rhythm transport | input-routing §1.6 | not found (rg `isMidiClock`) | none | `pending` |
| IR-2-01 | Mouse order: overlays -> live strip -> header -> main (Easy / Advanced incl. bench) -> drawer -> footer | input-routing §2 | JUCE z-order in `PluginEditor` | `EditorTests.cpp` `Editor::everyOverlayShortcutOpensItsOwnOverlayAndEscapeClosesIt` | `implemented` |
| IR-2.1-01 | Overlays consume all events; outside click dismisses and is consumed; popovers same; tooltips pass | input-routing §2.1 | `Overlays.cpp` | none | `implemented` - click-outside consumption not tested |
| IR-2.2-01 | Drag consumers: part cards -> illustration; pedals -> rack; snapshots -> strip; presets -> setlist; mod sources -> knobs; OS files -> root handler | input-routing §2.2 | rack drag exists; others absent | none | `partial` |
| IR-3-01 | Keyboard order: text field -> overlay -> popover -> global shortcuts -> focused component -> dropped | input-routing §3 | `PluginEditor::keyPressed` + registry | `EditorTests.cpp` `Editor::theWorkspaceTabShortcutsStepTheTabsInAdvancedModeOnly` | `implemented` |
| IR-3.1-01 | Text fields swallow global shortcuts (e.g. P while typing) | input-routing §3.1 | JUCE focus | none | `implemented` - untested |
| IR-3.2-01 | Rebind conflicts detected at rebind time | input-routing §3.2 | registry | `AccessibilityTests.cpp` `Accessibility::shortcutsRebindAndRefuseClashes` | `verified` |
| IR-3.3-01 | IME composition blocks shortcuts | input-routing §3.3 | JUCE | none | `implemented` - untested |
| IR-4-01 | Root file-drop handler by extension (preset, guitar, tune, part prompt, set, loop, content, midprofile, mid, audio by context, mp3) with unknown-type banner; batch/mixed rules | input-routing §4 | only `ToneMatchPanel` accepts drops (IRs) | none | `pending` |
| IR-5-01 | Transport consumers: rhythm (bar 0 on start, resync on position), tune, tap defers to host, metronome, recorder | input-routing §5 | `RhythmEngine`, `TapTempo`, `Metronome` | `RhythmSchedulerTests.cpp` `RhythmPatterns::strumSchedulingIsSampleAccurate`; `LiveTests.cpp` `LiveTapTempo::respectsRangeSnapAndHostPriority` | `partial` - tune absent |
| IR-6-01 | Sidechain consumers: followers, sidechain compressor, sidechain-to-amp, EQ/cab match capture | input-routing §6 | followers, sidechain-to-amp, capture yes; compressor no | `RoutingTests.cpp` `Routing::sidechainToAmpReplacesTheInstrument` | `partial` |
| IR-7-01 | Standalone audio input for sidechain, hum capture, backing capture, trainers; no pitch-to-MIDI | input-routing §7 | sidechain only | none | `partial` |
| IR-8-01 | Diagnostics "Inject fixture MIDI / audio" at the front of the chain | input-routing §8 | not found | none | `pending` |
| IR-9-01 | Tests in `Tests/InputRouting/` per consumer/veto; 60-s integration session | input-routing §9 | - | none | `pending` |

## 40. host-integration.md (phase 5)

Precedence 17 (host contract and quirks). pluginval passed at strictness 10
on Windows VST3 at `f18bf22` (2026-09-19); nothing since. No per-host
testing has been done and `docs/HOST_COMPATIBILITY.md` does not exist.

| ID | Requirement (short) | Source (file §section) | Implementation location | Verification | Status |
|---|---|---|---|---|---|
| HI-0-01 | Follow host contract; no blocking on audio thread; announce capabilities; host transport wins; changes only at documented moments; state holds references not blobs | host-integration §0 | - | pluginval run (2026-09-19) | `partial` |
| HI-1-01 | VST3 all platforms; AU macOS; Standalone everywhere; CLAP v1.1; AAX v1.5 | host-integration §1 | `CMakeLists.txt:25` VST3 + Standalone (no AU) | PROGRESS targets | `partial` - AU missing; CLAP/AAX `deferred` by the spec itself |
| HI-1-02 | Version to host: major.minor.patch + build string | host-integration §1 | `project(Luthier VERSION 1.0.0)`; no build string | none | `partial` |
| HI-2-01 | Layouts A-D with 8 aux in B/D; any subset accepted | host-integration §2 | Aux 8 declared last; buses classified by declared name so a host's subset maps correctly (`a901c72`) | `PluginBusTests.cpp` `PluginBuses::perStringLayoutPutsEachStringOnItsOwnBus`, `PluginBuses::aux8NoiseIsDeclaredLastSoNoBusNumberMoved`; `RoutingTests.cpp` `Routing::everyLayoutRendersCleanly` | `verified` |
| HI-2-02 | Mono main out rejected | host-integration §2 | `isBusesLayoutSupported` accepts mono main (`PluginProcessor.cpp:218`) | none | `pending` - contradicts spec (Conflict C-26) |
| HI-2-03 | Optional stereo sidechain on all layouts | host-integration §2 | sidechain input bus optional | none | `implemented` |
| HI-2-04 | Layout change -> prepareToPlay, never a crash | host-integration §2 | - | pluginval bus suites (2026-09-19) | `partial` |
| HI-3-01 | APVTS only; stable count; parameters grouped by category; translated names; ranges; defaults; text converters | host-integration §3 | APVTS flat (no `AudioProcessorParameterGroup`); English names | `IntegrationTests.cpp` `Parameters::everyParameterHasAUniqueIdAndSaneDefault`, `Parameters::everyParameterTextRoundTrips` | `partial` - no groups, no translation |
| HI-3.1-01 | Internal changes notify host; last write per block wins | host-integration §3.1 | `setValueNotifyingHost` paths | none | `implemented` |
| HI-3.3-01 | Discrete params as integers; module crossfade 5-30 ms | host-integration §3.3 | choice params | `ModulationTests.cpp` `Modulation::discreteDestinationsStepAtBoundaries` | `implemented` |
| HI-4-01 | State blob: format version tag, APVTS XML, uiState XML, structural JSON (matrix, snapshots, mappings, ranges, guitar ref/inline, circuit, MIDI export profile), padding | host-integration §4 | JSON root with preset, midiLearn, ui, locks, routing, modulation, rhythm, guitar (`PluginProcessor.cpp:1700-1750`) | `IntegrationTests.cpp` `Presets::stateRoundTripsExactly` | `partial` - no explicit format version tag; no MIDI export profile |
| HI-4-02 | Size < 200 KB typical, < 2 MB with inline guitar; never IR data | host-integration §4 | IR references only | none | `implemented` - size not measured |
| HI-4.1-01 | Older build loading newer blob: known sections only, warn, preserve unknown on write-back | host-integration §4.1 | presets preserve unknown fields; host blob not verified | none | `partial` |
| HI-4.2-01 | Newer build loading older blob: migrate and back up old blob to diagnostics | host-integration §4.2 | no blob backup | none | `pending` |
| HI-5-01 | Latency via getLatencySamples; change triggers `updateHostDisplay`; per-output bus latency | host-integration §5 | `setLatencySamples` `PluginProcessor.cpp:759`; no `updateHostDisplay` call found | `IntegrationTests.cpp` `Engine::latencyIsReportedAndPlausible` | `partial` |
| HI-6-01 | Transport read every block (tempo, sig, playing, recording, beats, rate); fallback to internal | host-integration §6 | playhead reads in processor/rhythm | `RhythmSchedulerTests.cpp` `RhythmPatterns::silentWhenStoppedUnlessFreeRunning` | `implemented` |
| HI-7-01 | acceptsMidi and producesMidi true; channels 1-16; clock/transport/SysEx accepted; MPE; sample-accurate out | host-integration §7 | `PluginProcessor.h:53-59`; `NEEDS_MIDI_OUTPUT FALSE` in CMake; no clock/SysEx | `RoutingTests.cpp` `Routing::midiOutPassThroughIsSampleExact` | `partial` - see RIO-6-05, IR-1.6-01 |
| HI-8-01 | Instances independent; shared only user settings, content, factory folders | host-integration §8 | per-instance objects | none | `implemented` - untested |
| HI-9.1-01 | Ableton: swallow first program change after state restore | host-integration §9.1 | `setCurrentProgram` guard | `StateModelTests.cpp` `StateModel::aProgramChangeRightAfterAStateRestoreDoesNotWipeIt` | `verified` |
| HI-9.1-02 | Ableton MPE auto-detect from channel-1 + member traffic | host-integration §9.1 | not found | none | `pending` |
| HI-9.2-01 | Logic: state < 500 KB; explicit PC mapping mode in routing panel; idempotent fast prepare | host-integration §9.2 | no PC mapping mode | none | `partial` |
| HI-9.3-01 | Cubase / Studio One / Reaper / FL / Bitwig notes (no special handling; VST3 note expression v1.5) | host-integration §9.3-9.7 | - | host matrix not run | `pending` |
| HI-9.8-01 | Pro Tools via AAX v1.5 | host-integration §9.8 | - | - | `deferred` - spec schedules AAX for v1.5 |
| HI-9.9-01 | Standalone: device polling, virtual MIDI-out toggle, resizable with minimum, native dialogs | host-integration §9.9 | JUCE standalone wrapper; no polling | none | `partial` |
| HI-10-01 | pluginval strictness 10 on every merge | host-integration §10 | - | manual 2026-09-19 | `partial` |
| HI-11-01 | Undo per instance, not integrated with host undo | host-integration §11 | processor undo stack | none | `implemented` |
| HI-12-01 | Factory presets enumerated via getNumPrograms / getProgramName | host-integration §12 | `PluginProcessor.cpp:1683-1693` | `LiveTests.cpp` `LiveSnapshots::programChangeMapsAcrossAllOneTwentyEight` (snapshots) | `implemented` |
| HI-13-01 | Threading contract (state save via swap while audio runs) | host-integration §13 | message-thread state capture | `StateModelTests.cpp` `StateModel::loadingAPresetWhileRenderingProducesNoGarbage` | `partial` |
| HI-14.1-01 | VST3 units: one per column-4 tab and Easy strip | host-integration §14.1 | no parameter groups | none | `pending` |
| HI-15-01 | Tests: pluginval all platforms/formats; 32 instances; VST3->AU switch; SR/block/bus change mid-play; transport follow; per-host state round trip; per-host MIDI I/O | host-integration §15 | - | `IntegrationTests.cpp` `Engine::sampleRateChangesAreSurvived`, `Engine::blockSizeChangesAreSurvived` | `partial` |
| HI-16-01 | `docs/HOST_COMPATIBILITY.md` documents every quirk | host-integration §16 | file absent | none | `pending` |

## 41. action-and-undo.md (phase 5)

Precedence 18 (undo grouping and boundaries). TODO 14b: audit against the
snapshot undo stack. The build stores whole-state snapshots
(`UndoEntry { state, redoState, description }`, `PluginProcessor.h:577-596`,
max 200); parameter entries come from host gestures (one per drag). There is
no action class, target, timestamp, 200 ms grouping, or boundary flag, and
most structural edits push nothing (only 17 non-test `pushUndoState` call
sites: preset loads, reset, randomise, style load, detune drag, clamp, Save
As Guitar, range changes).

| ID | Requirement (short) | Source (file §section) | Implementation location | Verification | Status |
|---|---|---|---|---|---|
| AU-0-01 | One user-intended change = one entry | action-and-undo §0.1 | gesture-based entries (`parameterGestureChanged`) | `RangesUiTests.cpp` `Undo::stepsOneActionAtATimeBothWays` | `implemented` |
| AU-0-02 | State boundaries; Shift-Ctrl-Z to cross | action-and-undo §0.2, §5 | not built | none | `pending` |
| AU-0-03 | 200 ms grouping of same class + target | action-and-undo §0.3, §4 | gesture grouping instead | none | `partial` - Conflict C-27 |
| AU-0-04 | Never undoable: live audio/MIDI, recorder writes, banner dismissals | action-and-undo §0.4, §7 | gesture-only entries exclude automation, MIDI, modulation | none | `implemented` |
| AU-0-05 | Per-instance stack; not persisted | action-and-undo §0.5-0.6, §10 | processor member | none | `implemented` |
| AU-1-01 | Entry: class, target, before/after, timestamp, description, boundary flag | action-and-undo §1 | before/after state + description only | none | `partial` |
| AU-1-02 | Undo History dropdown (View menu) | action-and-undo §1, §9 | not built | none | `pending` |
| AU-2-01 | Max 200; oldest drop; redo cleared on new action | action-and-undo §2 | `kMaxUndoSteps = 200` | `RangesUiTests.cpp` `Undo::stepsOneActionAtATimeBothWays` (branch clears redo) | `partial` - overflow not tested |
| AU-3.1-01 | Parameter changes: "Change X from A to B"; pause > 200 ms splits; no entries from modulation, automation, MIDI CC | action-and-undo §3.1 | gesture entries; description is parameter name | none | `partial` |
| AU-3.2-01 | Discrete switches merged within 200 ms | action-and-undo §3.2 | combo change = one gesture | none | `partial` |
| AU-3.3-01 | Toggles "Turn on/off X" | action-and-undo §3.3 | gesture entries | none | `implemented` - description wording differs |
| AU-3.4-01 | Part swaps: one entry each, "Change slot to part" | action-and-undo §3.4 | no bench; guitar type change via parameter gesture | none | `pending` |
| AU-3.5-01 | Illustration drags "Move handle from X mm to Y mm" | action-and-undo §3.5 | no bench | none | `pending` |
| AU-3.6-01 | Mod-matrix entries (route create/delete/edit, source edit) | action-and-undo §3.6 | no `pushUndoState` in `ModMatrixPanel` or menu route creation | none | `pending` |
| AU-3.7-01 | Snapshot entries (save, recall, rename, colour, delete, move) | action-and-undo §3.7 | no undo calls in `LivePanel`/`LiveStrip` | none | `pending` |
| AU-3.8-01 | Preset load = boundary; save/rename not on stack | action-and-undo §3.8 | preset load pushes a plain entry (`HeaderBar.cpp:29`) | none | `partial` - not a boundary |
| AU-3.9-01 | Tune entries and tune-load boundary | action-and-undo §3.9 | no Tune | none | `pending` |
| AU-3.10-01 | Setlist load/step/edit entries | action-and-undo §3.10 | not built | none | `pending` |
| AU-3.11-01 | `ranges-toggle` entries grouped 200 ms; lock restores clamped values | action-and-undo §3.11 | `processor.changeRanges` pushes entry (`PluginProcessor.cpp:267`) | `RangesUiTests.cpp` `Undo::stepsOneActionAtATimeBothWays` | `implemented` |
| AU-3.12-01 | MIDI Learn entries (learn, delete) | action-and-undo §3.12 | not found | none | `pending` |
| AU-3.13-01 | Rack entries (add, remove, move, param, bypass) | action-and-undo §3.13 | slot params are APVTS (gesture entries); add/move/remove not found | none | `partial` |
| AU-3.14-01 | Practice entries (scale, layer settings, track load); recording not undoable | action-and-undo §3.14 | not found | none | `pending` |
| AU-3.15-01 | Character control entries grouped | action-and-undo §3.15 | CharacterEngine state edits push nothing found | none | `pending` |
| AU-3.17-01 | UI state changes not undoable | action-and-undo §3.17 | Easy/Advanced and Live toggles undo themselves (EditorTests name) | `EditorTests.cpp` `Editor::theModesThatChangeTheLayoutTakeEffectAndUndoThemselves` | `partial` - check whether "undo themselves" means they push undo entries (spec says they must not) |
| AU-4-01 | Merge rules (same class, target, <= 200 ms, adjacent, no boundary) | action-and-undo §4 | not built | none | `pending` |
| AU-6-01 | Multi-target actions are one entry, reversed atomically | action-and-undo §6 | whole-state snapshots (inherently atomic) | `RangesUiTests.cpp` `Undo::stepsOneActionAtATimeBothWays` | `implemented` |
| AU-7-01 | Skips: audio, MIDI out, recorder, meters, banners, tooltips, browsing, A/B flip, panic, tap | action-and-undo §7 | none of these push | none | `implemented` |
| AU-8-01 | Family-switch undo warning; post-save undo leaves file | action-and-undo §8 | no family switch | none | `pending` |
| AU-9-01 | Shortcuts: Ctrl-Z, Ctrl-Shift-Z, Ctrl-Y (Windows), Ctrl-Alt-Z across boundary with banner | action-and-undo §9 | Ctrl-Z / Ctrl-Shift-Z (`Accessibility.cpp:518-519`); no Ctrl-Y or Ctrl-Alt-Z | `AccessibilityTests.cpp` `Accessibility::shortcutDefaultsMatchTheCanonicalTable` | `partial` - Conflict C-28 on the cross-boundary key |
| AU-11-01 | Host automation not undoable inside Luthier | action-and-undo §11 | gesture-only | none | `implemented` |
| AU-12-01 | Diagnostics "Show Undo Depth" footer counter | action-and-undo §12 | not found | none | `pending` |
| AU-13-01 | Test per action class (entry type, description, reversal, grouping) | action-and-undo §13 | - | none | `pending` |
| AU-13-02 | Test: boundaries push, stop Ctrl-Z, cross with Shift | action-and-undo §13 | - | none | `pending` |
| AU-13-03 | Test: 250 actions drop oldest 50 | action-and-undo §13 | - | none | `pending` |
| AU-13-04 | Test: new action clears redo | action-and-undo §13 | - | `RangesUiTests.cpp` `Undo::stepsOneActionAtATimeBothWays` | `verified` |
| AU-13-05 | Test: 199 vs 201 ms grouping | action-and-undo §13 | - | none | `pending` |
| AU-13-06 | Test: preset load one entry, reversed in one swap | action-and-undo §13 | - | none | `pending` |
| AU-13-07 | Test: undo mid-play without dropouts | action-and-undo §13 | - | none | `pending` |
| AU-13-08 | Test: new instance has empty stack | action-and-undo §13 | - | none | `pending` |
| AU-13-09 | Test: 1000 mod-driven changes create zero entries | action-and-undo §13 | - | none | `pending` |

## 42. string-scraping.md (phase 5b)

**Not built** (TODO 13b). The only scrape today is pick-noise's noise-based
rake (`LuthierEngine::triggerPickScrape`, no trigger path, TODO 3f). This
spec replaces it with per-winding impulses from a `ScrapeEngine` (Conflict
C-29).

| ID | Requirement (short) | Source (file §section) | Implementation location | Verification | Status |
|---|---|---|---|---|---|
| SCR-0-01 | Scrape as per-winding impulses; wound strings only; speed matters; user-triggered | string-scraping §0 | not built | none | `pending` |
| SCR-1-01 | ScrapeGesture {string, start/end mm, duration, pressure, tool, angle}; catches = distance x windings/mm; evenly spaced; pressure scales amplitude and adds slight pitch loading | string-scraping §1 | winding pitch exists in part data (`PartAcoustics.cpp:488`) | none | `pending` |
| SCR-2-01 | Controls: trigger (keyswitch/CC/MPE zone/button), direction (incl. hold + sweep), sweep source (auto/modwheel/expression/AT/CC), range 200-900 mm, pressure 0.5, tool pick/nail/thumb, angle 20, string mask (wound), retrigger 200 ms | string-scraping §2 | not built | none | `pending` |
| SCR-3-01 | `ScrapeEngine` {trigger, processBlock, reset}; audio rate; idle = one flag; after TechniqueEngine, before StringEngine; impulses at sample offsets | string-scraping §3 | not built | none | `pending` |
| SCR-4-01 | Presets Classic Rock Scrape, Metal Zipper, Slow Ratchet, Nail Scrape, Modwheel-Sweep | string-scraping §4 | not built | none | `pending` |
| SCR-5-01 | Cascade: compatible with mute, slide (bar lifted), bends; not tap on same string | string-scraping §5 | see technique-cascade | none | `pending` |
| SCR-6-01 | Test: wound low E 500 ms at 0.5 -> catches at windings x speed | string-scraping §6 | - | none | `pending` |
| SCR-6-02 | Test: plain high E < -30 dB vs wound | string-scraping §6 | - | none | `pending` |
| SCR-6-03 | Test: pressure doubling ~doubles catch amplitude (±20%), more pitch modulation | string-scraping §6 | - | none | `pending` |
| SCR-6-04 | Test: direction reversal mirrors the catch stream | string-scraping §6 | - | none | `pending` |
| SCR-6-05 | Test: modwheel sweep follows within one block | string-scraping §6 | - | none | `pending` |
| SCR-6-06 | Test: CPU idle < 0.05%, active < 0.5% | string-scraping §6 | - | none | `pending` |
| SCR-6-07 | Test: retrigger below threshold dropped | string-scraping §6 | - | none | `pending` |

## 43. slide-technique-controls.md (phase 5b)

**Not built** (TODO 5b). `SlideEngine` exists (slide-guitar.md); none of
the source, mode, speed-limit, auto-vibrato, mask or scripted-gesture
controls do. Section 4 places a copy in "Advanced Mode Col 3 SLIDE group";
the SLIDE group lives on the CHARACTER tab in column 4 (Conflict C-31).

| ID | Requirement (short) | Source (file §section) | Implementation location | Verification | Status |
|---|---|---|---|---|---|
| STC-0-01 | No duplication of slide-guitar; every slide gesture user-controlled; continuous position at control rate; pitch bend usable as position | slide-technique-controls §0 | `SlideEngine` bar driven from notes | none | `pending` |
| STC-1-01 | Position source: modwheel, pitch bend, MPE Y, expression, CC, fretboard drag | slide-technique-controls §1 | not built | none | `pending` |
| STC-1-02 | Position mode absolute (0-1 -> fret 0-24) or relative | slide-technique-controls §1 | not built | none | `pending` |
| STC-1-03 | Slant and pressure each with a selectable source | slide-technique-controls §1 | `slide_slant`, `slide_pressure` params only | none | `partial` |
| STC-1-04 | Contact string mask (all / bass 3 / treble 3) | slide-technique-controls §1 | not built | none | `pending` |
| STC-1-05 | Speed limit (default 4800 c/s, advanced higher) | slide-technique-controls §1 | not built | none | `pending` |
| STC-1-06 | Auto-vibrato after 300 ms hold (off by default, depth/rate) | slide-technique-controls §1 | not built | none | `pending` |
| STC-1-07 | Gesture trigger keyswitch/CC for scripted A->B over T | slide-technique-controls §1 | not built | none | `pending` |
| STC-2-01 | `SlideGesture` {from, to, duration, curve, slant start/end, pressure} via keyswitch, MIDI meta, tune builder | slide-technique-controls §2 | not built | none | `pending` |
| STC-3-01 | `SlideEngine::setPositionSource`, `triggerGesture`, `setSpeedLimit`; defaults leave slide-guitar behaviour unchanged | slide-technique-controls §3 | not built | none | `pending` |
| STC-4-01 | GUI: TECHNIQUES SLIDE sub-tab; Easy slide glyph "..." popover; SLIDE group expandable section | slide-technique-controls §4 | not built | none | `pending` |
| STC-5-01 | Cascade: compatible with mute, bends, scrape (bar lifted); not tap or slap on contacted strings | slide-technique-controls §5 | - | none | `pending` |
| STC-6-01 | Presets Standard (modwheel, 0.7), Pitch-Bend (range 24), Lap Steel Full Control (MPE Y/Z, AT), Auto-Vibrato Hold (5 Hz, 10 c) | slide-technique-controls §6 | not built | none | `pending` |
| STC-7-01 | Test: modwheel drives position through the range | slide-technique-controls §7 | - | none | `pending` |
| STC-7-02 | Test: speed limit clamps a full-throw jump | slide-technique-controls §7 | - | none | `pending` |
| STC-7-03 | Test: scripted gesture reaches target within ±5 ms | slide-technique-controls §7 | - | none | `pending` |
| STC-7-04 | Test: auto-vibrato after 300 ms hold | slide-technique-controls §7 | - | none | `pending` |
| STC-7-05 | Test: source swap mid-play, 10 ms crossfade, no click | slide-technique-controls §7 | - | none | `pending` |
| STC-7-06 | Test: bass-only mask leaves treble pluckable | slide-technique-controls §7 | - | none | `pending` |
| STC-7-07 | Test: preset round trip of every control | slide-technique-controls §7 | - | none | `pending` |

## 44. string-slap-technique.md (phase 5b)

**Not built** (TODO 13b). Depends on `bass-techniques.md` (not built), the
missing `body-coupling.md` (Body Tap drives its mode bank) and the missing
`fingerstyle-attack.md` (Playing strip Tool selector).

| ID | Requirement (short) | Source (file §section) | Implementation location | Verification | Status |
|---|---|---|---|---|---|
| SLP-0-01 | Slap is a strike with slap-buzz; position and force independent; works on any guitar with wound strings | string-slap-technique §0 | `Excitation::Kind::Slap` shape only | none | `pending` |
| SLP-1-01 | Slap types Thumb Slap, Finger Pop, Palm Slap, Body Tap | string-slap-technique §1 | not built | none | `pending` |
| SLP-1-02 | Controls: trigger source, contact position (60/40/100 mm), force 0.6, string mask, ghost mode, rebound (60 ms), snap-back (bass), body-tap part (top/side/back) | string-slap-technique §1 | not built | none | `pending` |
| SLP-2-01 | `SlapEngine` {trigger, processBlock, reset}; excitations to StringEngine; slap-buzz to FretBuzz path | string-slap-technique §2 | not built | none | `pending` |
| SLP-2-02 | Body Tap bypasses strings into BodyCoupling mode bank | string-slap-technique §2 | BodyCoupling does not exist | none | `blocked` - `body-coupling.md` missing |
| SLP-3-01 | bass-techniques slap/pop share SlapEngine; old bass presets keep working | string-slap-technique §3 | not built | none | `pending` |
| SLP-4-01 | Presets Bass Slap Standard/Aggressive, Funk Guitar Palm Slap, Acoustic Body Tap, Percussive Fingerstyle | string-slap-technique §4 | not built | none | `pending` |
| SLP-5-01 | Cascade: with mute, bends, alternating taps; not slide or scrape on same string | string-slap-technique §5 | - | none | `pending` |
| SLP-6-01 | GUI: TECHNIQUES SLAP sub-tab; Playing strip Tool selector gains Slap and Pop | string-slap-technique §6 | Tool selector is a `fingerstyle-attack.md` feature | none | `blocked` - tool selector spec missing |
| SLP-7-01 | Test: bass thumb slap matches bass-techniques reference within 1 dB | string-slap-technique §7 | - | none | `pending` |
| SLP-7-02 | Test: palm slap < -25 dB pitched content | string-slap-technique §7 | - | none | `pending` |
| SLP-7-03 | Test: body tap < -60 dB on damped string outputs | string-slap-technique §7 | - | none | `blocked` - needs BodyCoupling |
| SLP-7-04 | Test: ghost mode thump without clear pitch | string-slap-technique §7 | - | none | `pending` |
| SLP-7-05 | Test: rebound gap ±3 ms | string-slap-technique §7 | - | none | `pending` |
| SLP-7-06 | Test: plain-string slap has reduced buzz | string-slap-technique §7 | - | none | `pending` |
| SLP-7-07 | Test: CPU idle < 0.05%, active < 0.6% per event | string-slap-technique §7 | - | none | `pending` |
| SLP-7-08 | Test: preset round trip | string-slap-technique §7 | - | none | `pending` |

## 45. muting-rhythm.md (phase 5b)

**Not built** (TODO 13b). Existing pieces: palm-mute technique
(`StringEngine.cpp:256`), `MutedPick`, strum types `downMute`/`upMute`/`rake`
(`Patterns.h:22`). Fretting-hand mute style depends on the missing
`string-interaction.md`. Engine-technique-layer adds a `MuteEngine` that
this spec says is unnecessary (Conflict C-30).

| ID | Requirement (short) | Source (file §section) | Implementation location | Verification | Status |
|---|---|---|---|---|---|
| MR-0-01 | Muting is a rhythmic voice that makes sound; can flip mid-groove; multiple types per groove | muting-rhythm §0 | palm mute technique | `EngineTests.cpp` `StringEngine::palmMuteShortensAndDarkens` | `partial` |
| MR-1-01 | Mute types Open, PM Light (T60 ~150 ms), PM Heavy (~50), PM Extreme (~20), Ghost, Chuka, Fret Mute, with position/pressure defaults | muting-rhythm §1 | single palm-mute amount | none | `pending` |
| MR-2-01 | Pattern steps gain `mute_type`; missing = open | muting-rhythm §2 | not in `Patterns.cpp` schema | none | `pending` |
| MR-2-02 | Live 16-step Mute Grid overlay synced to host tempo | muting-rhythm §2 | not built | none | `pending` |
| MR-3-01 | Controls: master mute mode, per-step type grid, palm position 35 mm, pressure 0.5, fretting-hand style, chuka source (< 0.3), random humanise, ghost velocity 0.4 | muting-rhythm §3 | not built | none | `pending` |
| MR-3-02 | Fretting-hand mute style (rock spread vs classical) from string-interaction | muting-rhythm §3 | - | none | `blocked` - `string-interaction.md` missing |
| MR-4-01 | RhythmEngine reads/passes `mute_type`; StringEngine applies per-note damping and release | muting-rhythm §4 | not built | none | `pending` |
| MR-5-01 | Muting stacks with every technique | muting-rhythm §5 | - | none | `pending` |
| MR-6-01 | Presets Metal Chug 16ths, Funk Chuka, Reggae Skank, Country Boom-Chick, Metal Gallop, Classical Staccato | muting-rhythm §6 | not built | none | `pending` |
| MR-7-01 | GUI: TECHNIQUES MUTE sub-tab; Easy Playing strip 4-way Mute button; RHYTHM editor Mute Row | muting-rhythm §7 | not built | none | `pending` |
| MR-8-01 | MIDI export: NOTE gains `mute_type` (Luthier SysEx; Generic text meta) | muting-rhythm §8 | midi-export not built | none | `pending` |
| MR-9-01 | Test: PM heavy on low E T60 40-60 ms | muting-rhythm §9 | - | none | `pending` |
| MR-9-02 | Test: ghost < -30 dB pitched | muting-rhythm §9 | - | none | `pending` |
| MR-9-03 | Test: chuka at 0.2 dynamics percussive only | muting-rhythm §9 | - | none | `pending` |
| MR-9-04 | Test: grid paint applies within one bar | muting-rhythm §9 | - | none | `pending` |
| MR-9-05 | Test: humanise 0.5 shifts ~50% over 100 loops | muting-rhythm §9 | - | none | `pending` |
| MR-9-06 | Test: preset round trip of the grid | muting-rhythm §9 | - | none | `pending` |
| MR-9-07 | Test: patterns without mute_type unchanged | muting-rhythm §9 | - | none | `pending` |
| MR-9-08 | Test: slap carries own mute_type; grid override propagates | muting-rhythm §9 | - | none | `pending` |

## 46. two-hand-tapping.md (phase 5b)

**Not built** (TODO 13b). Existing: `Technique::Tap` (harder hammer-on
excitation) and legato inference with a velocity threshold
(`TechniqueEngine.cpp:121`, `legato_window` 40 ms). No `TapEngine`, no
movable-capo boundary, no tap channel routing. Section 4 relies on
`harmonic-realism.md` 2's excitation interface (missing).

| ID | Requirement (short) | Source (file §section) | Implementation location | Verification | Status |
|---|---|---|---|---|---|
| THT-0-01 | Tap is a fret event; tap-off returns to fretted pitch; both hands same physics; per-tap fret/string | two-hand-tapping §0 | `Technique::Tap` is an excitation variant only | none | `pending` |
| THT-1-01 | `TapGesture` {string, fret, strength, hand, pull_off_after, target, duration} | two-hand-tapping §1 | not built | none | `pending` |
| THT-2-01 | Tap-on damping+contact with impulse; tap-hold as movable capo; tap-off with optional lateral flick; multi-finger = concurrent taps | two-hand-tapping §2 | not built | none | `pending` |
| THT-3-01 | Controls: trigger (MIDI ch 2 default, keyswitch, fretboard layer), strength curve, auto pull-off (on), hammer-on threshold 40, lateral flick 0.5, duration 200 ms, max concurrent 2 (advanced 8), fret snap on | two-hand-tapping §3 | legato velocity threshold exists (different default) | none | `pending` |
| THT-4-01 | `TapEngine` {trigger, release, processBlock, reset} into StringEngine boundary input | two-hand-tapping §4 | not built | none | `pending` |
| THT-4-02 | Reuse StringEngine damp-position input per harmonic-realism 2 | two-hand-tapping §4 | - | none | `blocked` - `harmonic-realism.md` missing |
| THT-5-01 | Left-hand legato: same string, within 150 ms, below threshold = hammer-on; note-off revealing lower fret = pull-off; routed through TapEngine | two-hand-tapping §5 | `TechniqueEngine` legato inference (40 ms window, velocity < `legatoVelocity`) | `ModelTests.cpp` `Technique::legatoBecomesHammerOnAndPullOff` | `partial` - window/threshold differ; no TapEngine routing |
| THT-6-01 | GUI: TECHNIQUES TAP sub-tab; Easy TAP pill; CHARACTER Right Hand "Tapping" section; square tap markers fading 100 ms | two-hand-tapping §6 | not built | none | `pending` |
| THT-7-01 | Cascade: with mute and bends; not slide; alternate with slap; not scrape | two-hand-tapping §7 | - | none | `pending` |
| THT-8-01 | Presets Standard, Legato Runs, Eight-Finger, Microtonal, Percussive | two-hand-tapping §8 | factory "Tapping Etude" preset exists (different model) | none | `pending` |
| THT-9-01 | MIDI export: tap SysEx (Luthier); ch 2 notes (Generic) | two-hand-tapping §9 | not built | none | `pending` |
| THT-10-01 | Test: tap 12 over fret 5 pitches, returns with transient | two-hand-tapping §10 | - | none | `pending` |
| THT-10-02 | Test: flick 1.0 audible release transient; auto pull-off off has none | two-hand-tapping §10 | - | none | `pending` |
| THT-10-03 | Test: hammer-on without picking transient | two-hand-tapping §10 | - | `EngineTests.cpp` `StringEngine::legatoDoesNotRetriggerTheAttack` (existing legato path) | `partial` |
| THT-10-04 | Test: two taps on one string act as capos in series | two-hand-tapping §10 | - | none | `pending` |
| THT-10-05 | Test: fret snap off, 12.3 sounds sharp | two-hand-tapping §10 | - | none | `pending` |
| THT-10-06 | Test: CPU idle < 0.05%, active < 0.8% | two-hand-tapping §10 | - | none | `pending` |
| THT-10-07 | Test: preset round trip | two-hand-tapping §10 | - | none | `pending` |

## 47. microtonal-bends.md (phase 5b)

**Mostly not built** (TODO 13b). Existing: global pitch bend with
`bend_range`, MPE per-note bend, vibrato rate/depth/shape params, per-string
bend range from controller profiles, continuous (unquantised) pitch.

| ID | Requirement (short) | Source (file §section) | Implementation location | Verification | Status |
|---|---|---|---|---|---|
| MB-0-01 | Continuous pitch per string; selectable sources; layered sources; per-string ranges | microtonal-bends §0 | continuous pitch yes; single bend range | `EngineTests.cpp` `StringEngine::bendIsSmoothAndReachesTarget` | `partial` |
| MB-1-01 | Per-string source stack: global, per-string (MPE), vibrato, pre-bend release, slide contribution; scale factor + latency comp per source | microtonal-bends §1 | global + MPE sum in `MidiInterpreter`; no stack object | none | `partial` |
| MB-2-01 | Global bend source selectable; range 200 c default, advanced 2400 | microtonal-bends §2 | `bend_range` param (semitones) | none | `partial` - source not selectable |
| MB-2-02 | Per-string bend source (MPE Y default) and 6 per-string range sliders | microtonal-bends §2 | profile per-string pitch-bend range (`ControllerProfile`) | `ControllerTests.cpp` `Controllers::applyingAProfileConfiguresTheInterpreter` | `partial` - no UI sliders; MPE Y not a bend source |
| MB-2-03 | Vibrato source (LFO/AT/MPE Z), rate 3-10 (6), depth 5-50 (20), onset delay 200 ms | microtonal-bends §2 | `vibrato_rate/depth/shape`; no onset delay | none | `partial` |
| MB-2-04 | Bend quantise: none / quarter-tone / semitone / 24-EDO / custom | microtonal-bends §2, §3 | not built | none | `pending` |
| MB-2-05 | Custom scale from `.scala` / `.tun` | microtonal-bends §2, §3 | not built | none | `pending` |
| MB-2-06 | Pre-bend via keyswitch/CC (default -200 c) | microtonal-bends §2 | not built (see SPEC-4-06) | none | `pending` |
| MB-2-07 | Bend and release curves (linear, exponential, drawn) | microtonal-bends §2 | not built | none | `pending` |
| MB-3-01 | Quantise presets 22/24/31/53-EDO; snap strength 0-1 | microtonal-bends §3 | not built | none | `pending` |
| MB-4-01 | `StringEngine::setBendSourceStack`, `setBendQuantise`; `PreBendEvent` mod source; per-string range change at next note-on | microtonal-bends §4 | not built | none | `pending` |
| MB-5-01 | GUI: TECHNIQUES BEND sub-tab; Easy bend "..." popover; CHARACTER PLAYING "Microtonal" section; fretboard cents badge and dot shift | microtonal-bends §5 | not built | none | `pending` |
| MB-6-01 | Bends cascade with everything as an offset | microtonal-bends §6 | bends are additive today | none | `implemented` |
| MB-7-01 | Presets Standard Whole-Tone, Quarter-Tone Blues, Maqam, Wide Vibrato, Whammy-Style Two-Octave | microtonal-bends §7 | not built | none | `pending` |
| MB-8-01 | MIDI export: per-source bends and quantise (Luthier); per-channel pitch bend (Generic) | microtonal-bends §8 | not built | none | `pending` |
| MB-9-01 | Test: global +100 c on every ringing note | microtonal-bends §9 | - | none | `pending` |
| MB-9-02 | Test: per-string +200 c only on string 3 | microtonal-bends §9 | - | `ControllerTests.cpp` `Controllers::perChannelRoutingSendsEachChannelToItsString` (routing, not pitch) | `pending` |
| MB-9-03 | Test: vibrato onset delay | microtonal-bends §9 | - | none | `pending` |
| MB-9-04 | Test: quarter-tone snap 1.0 lands on 50 c steps | microtonal-bends §9 | - | none | `pending` |
| MB-9-05 | Test: `.scala` within 0.5 c | microtonal-bends §9 | - | none | `pending` |
| MB-9-06 | Test: pre-bend starts -200 c and releases | microtonal-bends §9 | - | none | `pending` |
| MB-9-07 | Test: range change mid-note no jump | microtonal-bends §9 | - | none | `pending` |
| MB-9-08 | Test: vibrato depth within 50 ms of onset | microtonal-bends §9 | - | none | `pending` |
| MB-9-09 | Test: bend + slap on low E | microtonal-bends §9 | - | none | `pending` |
| MB-9-10 | Test: preset round trip | microtonal-bends §9 | - | none | `pending` |

## 48. technique-cascade.md (phase 5b)

Precedence 21 (technique compatibility and conflict resolution). **Not
built** (TODO 13b). References `fingerstyle-attack.md` and
`body-coupling.md` (both missing) in its class table and schedule.

| ID | Requirement (short) | Source (file §section) | Implementation location | Verification | Status |
|---|---|---|---|---|---|
| TC-0-01 | Compatibility classes; compatible combos never refused; deterministic conflict resolution; cross-string always compatible; UI conflict indicators | technique-cascade §0 | not built | none | `pending` |
| TC-1-01 | Class table (scrape, slide, slap, mute, tap, bend, fingerstyle, pick) | technique-cascade §1 | not built | none | `pending` |
| TC-2-01 | Same-string 6x6 matrix (compatible / queue / alternate / conflict) within ~200 ms window | technique-cascade §2 | not built | none | `pending` |
| TC-3-01 | Priority: user beats automatic; most recent wins; 10 ms graceful preemption; slide holds priority once engaged; mute and bend always addable | technique-cascade §3 | not built | none | `pending` |
| TC-4-01 | Cascade schedule: TechniqueEngine -> resolver -> excitation (pick, fingerstyle, scrape, slap) -> damping (mute, tap, slide) -> pitch (tap, slide, bend) -> StringEngine -> BodyCoupling | technique-cascade §4 | not built | none | `pending` - FingerstyleEngine and BodyCoupling stages `blocked` on missing specs |
| TC-5-01 | Combined presets Metal Lead Combo, Funk Slap Groove, Slide Blues, Percussive Tap, Scrape Intro, Full Cascade Demo | technique-cascade §5 | not built | none | `pending` |
| TC-6-01 | Conflict pill: red slash + tooltip naming the conflict | technique-cascade §6 | not built | none | `pending` |
| TC-8-01 | Cascade decisions push no undo; arming does | technique-cascade §8 | not built | none | `pending` |
| TC-9-01 | Resolver O(strings); all six active ~2.5% CPU; body tap +1% | technique-cascade §9 | not built | none | `pending` |
| TC-10-01 | Test: every matrix cell, same string and cross string | technique-cascade §7, §10 | - | none | `pending` |
| TC-10-02 | Test: combined presets match references within 0.5 dB | technique-cascade §7, §10 | - | none | `pending` |
| TC-10-03 | Test: 10 000 random gesture sequences, no crash/orphans, CPU in budget | technique-cascade §7 | - | none | `pending` |
| TC-10-04 | Test: preemption crossfade 10 ms without click (< 0.5 dB discontinuity) | technique-cascade §10 | - | none | `pending` |
| TC-10-05 | Test: cross-string independence (slap E, slide B, tap G, bend A) | technique-cascade §10 | - | none | `pending` |
| TC-10-06 | Test: conflict indicator within one UI frame | technique-cascade §10 | - | none | `pending` |

## 49. gui-techniques-updates.md (phase 5b)

Precedence 20 (Techniques tab layout and Playing strip pill row). **Not
built** (TODO 13b). It inserts TECHNIQUES before HELP, making column 4 a
14-tab strip (Conflict C-32). Its CHARACTER references say "Col 3" but
CHARACTER is a column 4 tab (Conflict C-31).

| ID | Requirement (short) | Source (file §section) | Implementation location | Verification | Status |
|---|---|---|---|---|---|
| GTU-0-01 | Additive only; TECHNIQUES tab between CONTROLLERS and HELP | gui-techniques-updates §0 | not built | none | `pending` |
| GTU-1-01 | TECHNIQUES tab: vertical sub-tab rail SCRAPE, SLIDE, SLAP, MUTE, TAP, BEND, CASCADE; controls on the right; arm pill per sub-tab; AttachedKnob pattern | gui-techniques-updates §1 | not built | none | `pending` |
| GTU-2-01 | Easy Playing strip pill row (6 pills): tap arms, hold opens popover (Escape closes), right-click opens sub-tab; filled when armed; fire-indicator dot | gui-techniques-updates §2 | Playing strip itself not built (GI-3.3) | none | `pending` |
| GTU-3-01 | No header changes | gui-techniques-updates §3 | - | - | `n/a` - constraint |
| GTU-4-01 | Fretboard overlays: scrape trail with catch ticks, tap squares (100 ms fade), mute-zone band, bend arc + cents badge, slap flash; toggleable; layers 33+; < 2 ms | gui-techniques-updates §4 | not built | none | `pending` |
| GTU-5-01 | CHARACTER Right Hand "Tapping" and PLAYING "Microtonal" mirror sections | gui-techniques-updates §5 | not built; Right Hand group is a `fingerstyle-attack.md` item (missing) | none | `pending` |
| GTU-6-01 | RHYTHM pattern editor Mute Row | gui-techniques-updates §6 | not built | none | `pending` |
| GTU-7-01 | Preset browser "Uses Techniques" filter chip | gui-techniques-updates §7 | not built | none | `pending` |
| GTU-8-01 | Onboarding tour gains a Techniques stop | gui-techniques-updates §8 | no tour | none | `pending` |
| GTU-9-01 | Accessibility: pill labels, Tab/Space/Enter, non-colour armed indicator, reduced motion | gui-techniques-updates §9 | not built | none | `pending` |
| GTU-10-01 | Design cues: theme radius pills, vertical rail, pulsing dots, CASCADE strings x techniques grid | gui-techniques-updates §10 | not built | none | `pending` |
| GTU-11-01 | GI section 19 additions (7 rows: scrape, slide controls, slap, muting, tapping, bends, cascade) | gui-techniques-updates §11 | not built | none | `pending` |
| GTU-12-01 | Test: every sub-tab renders at 1280x800 | gui-techniques-updates §12 | - | none | `pending` |
| GTU-12-02 | Test: each pill arms/disarms on click | gui-techniques-updates §12 | - | none | `pending` |
| GTU-12-03 | Test: long-press popover; Escape closes | gui-techniques-updates §12 | - | none | `pending` |
| GTU-12-04 | Test: CASCADE view reflects arm state | gui-techniques-updates §12 | - | none | `pending` |
| GTU-12-05 | Test: overlays within 2 ms | gui-techniques-updates §12 | - | none | `pending` |
| GTU-12-06 | Test: tour reaches Techniques tab | gui-techniques-updates §12 | - | none | `pending` |
| GTU-12-07 | Test: existing GI tests still pass | gui-techniques-updates §12 | - | existing `EditorTests.cpp` suite | `n/a` - regression guard |

## 50. engine-technique-layer.md (phase 5b)

Precedence 19 (the six technique modules' integration). **Not built**
(TODO 13b). It assumes a command queue (none exists, UW-4.3-01), a
`BodyCoupling` module and a `harmonic-realism.md` excitation interface
(both from missing phase-2b specs), and a `MuteEngine` that
`muting-rhythm.md` 4 says is not needed (Conflict C-30).

| ID | Requirement (short) | Source (file §section) | Implementation location | Verification | Status |
|---|---|---|---|---|---|
| ETL-0-01 | Additive only; zero-cost idle; engine.md DSP rules; attach via command queue | engine-technique-layer §0 | no command queue | none | `pending` |
| ETL-1-01 | New modules ScrapeEngine, SlapEngine (incl. Body Tap), TapEngine, MuteEngine, CascadeResolver | engine-technique-layer §1 | none exist | none | `pending` |
| ETL-2-01 | Pipeline: TechniqueEngine -> CascadeResolver (2b) -> technique modules (2c) -> RhythmEngine (MuteGrid) -> ModMatrix -> StringEngine -> pickup/circuit/amp/cab (Body Tap to BodyCoupling) -> FX -> master | engine-technique-layer §2 | `LuthierEngine::process` has no 2b/2c stages | none | `pending` |
| ETL-3.1-01 | TechniqueEngine gesture types (scrape, slap, tap, body-tap) and `emit` | engine-technique-layer §3.1 | not built | none | `pending` |
| ETL-3.2-01 | StringEngine: multi-source excitation per harmonic-realism 2; N concurrent contact points (was 1); summed bend offset | engine-technique-layer §3.2 | single slide contact; no multi-source interface spec | none | `blocked` - `harmonic-realism.md` missing |
| ETL-3.3-01 | RhythmEngine step `mute_type` (default open), emitted with note-ons | engine-technique-layer §3.3 | not built | none | `pending` |
| ETL-3.4-01 | BodyCoupling `driveDirect(impulse, position)` | engine-technique-layer §3.4 | module does not exist | none | `blocked` - `body-coupling.md` missing |
| ETL-3.5-01 | ModMatrix `PreBendEvent` source; per-string microtonal range scale | engine-technique-layer §3.5 | not built | none | `pending` |
| ETL-3.6-01 | MidiInterpreter keyswitches from a reserved range; MPE Y/Z to slide position and bends | engine-technique-layer §3.6 | not built; controllers.md defines no keyswitch range | none | `pending` |
| ETL-4-01 | Commands ArmTechnique, SetTechniqueParam, TriggerGesture, LoadMuteGrid; result TechniqueFired | engine-technique-layer §4 | no queue | none | `pending` |
| ETL-5-01 | ~60 APVTS params: 6 arm booleans + per-technique controls under `parameters/techniques/<technique>/*` | engine-technique-layer §5 | none declared | none | `pending` - parameter-count change needs the pinned count updated |
| ETL-6-01 | Old presets load with techniques disarmed; old bass-slap presets map to SlapEngine | engine-technique-layer §6 | - | none | `pending` |
| ETL-7-01 | Technique state in preset; snapshots capture arm state; live triggers session-only | engine-technique-layer §7 | - | none | `pending` |
| ETL-8-01 | Undo classes `technique-arm`, `technique-param` (200 ms), `mute-grid-paint` (200 ms); triggers not undoable | engine-technique-layer §8 | - | none | `pending` |
| ETL-9-01 | Budgets: idle < 0.1%, single ~0.5-1%, all six ~2.5%; low CPU class defaults off with warning banner | engine-technique-layer §9 | - | none | `pending` |
| ETL-10-01 | Test: pipeline order via instrumented counters | engine-technique-layer §10 | - | none | `pending` |
| ETL-10-02 | Test: zero regressions in existing suites | engine-technique-layer §10 | existing suite | last recorded green: 448 at `0d225f0` | `n/a` - regression guard |
| ETL-10-03 | Test: 100 pre-delta presets byte-identical playback | engine-technique-layer §10 | - | none | `pending` |
| ETL-10-04 | Test: 1000 commands/s without audio-thread allocation | engine-technique-layer §10 | - | none | `pending` |
| ETL-10-05 | Test: old bass-slap preset maps to SlapEngine | engine-technique-layer §10 | - | none | `pending` |

## spec.md (companion: product brief)

Superseded in places: the GUI layout (Easy bands, Advanced columns, header)
by `gui-integration.md` (precedence 2); `CableSim` by
`volume-knob-interaction.md` (precedence 9); hard-coded guitars by
`guitar-workshop.md` (precedence 8). Those rows are `n/a` here and tracked in
the overriding spec's section.

| ID | Requirement (short) | Source (file §section) | Implementation location | Verification | Status |
|---|---|---|---|---|---|
| SPEC-1-01 | Extended digital waveguide per string, up to 12 strings | spec.md §1 | `Source/DSP/String/StringEngine.h`; `kMaxStrings = 12` `Source/DSP/Common/DspCommon.h:21` | `EngineTests.cpp` `StringEngine::pluckProducesCorrectPitch`; `ModelTests.cpp` `GuitarLibrary::twelveStringCoursesAreOctavePaired` | `verified` |
| SPEC-1-02 | Per-string length 500-750 mm; tension from pitch/mass/length; linear density from gauge+material; Young's modulus | spec.md §1 | `Source/Model/Guitar/StringMaterials.cpp`; scale clamp in `PartAcoustics.cpp` | `ModelTests.cpp` `StringPhysics::standardSetsLandInTheUsualTensionRange`, `thickerStringsAreHeavierAndTighter`; `PartAcousticsTests.cpp` `PartAcoustics::scaleLengthSetsTension` | `verified` |
| SPEC-1-03 | Continuous fretting position 0-24, pluck position, pluck strength, pluck material | spec.md §1 | `TuningEngine`, `Excitation.cpp:76`, `pick_material` param | `ModelTests.cpp` `Tuning::fretPositionIsContinuous` | `implemented` - continuity verified; other fields not asserted |
| SPEC-1-04 | Shaped excitation: pick 2-4 ms bright, fingers 5-8 ms warmer, thumb low-passed, nail vs pad selectable | spec.md §1 | `Source/DSP/String/Excitation.cpp`; `nail_vs_flesh` param | `EngineTests.cpp` `StringEngine::harderPluckIsBrighterNotJustLouder` (velocity only; material spectra untested) | `implemented` |
| SPEC-1-05 | Inharmonicity `f_n = n f0 sqrt(1+Bn^2)`, wound strings higher B | spec.md §1 | `StringEngine` dispersion allpasses (`StringEngine.h:162`) | `EngineTests.cpp` `StringEngine::dispersionStretchesPartialsSharp`; `ModelTests.cpp` `StringPhysics::woundStringsAreLessStiffThanTheirDiameterSuggests` | `verified` |
| SPEC-1-06 | Sympathetic resonance through the bridge, NxN coupling matrix | spec.md §1 | `Source/DSP/Coupling/CouplingMatrix.cpp` | `EngineTests.cpp` `Coupling::aStruckStringRingsItsNeighbour`, `matrixIsSymmetricAndZeroOnTheDiagonal` | `verified` |
| SPEC-1-07 | Two-stage decay: fast HF decay in first 200 ms, then slow decay; loop cutoff drops over time | spec.md §1 | Loop filter in `StringEngine.cpp`; no explicit two-stage stage found (rg `twoStage`) | `EngineTests.cpp` `StringEngine::higherNotesDecayFaster` (frequency dependence only) | `partial` - explicit two-stage/time-varying cutoff not found; phase 2b `sustain-and-decay.md` would own it |
| SPEC-1-08 | Sustain as target decay per string; steel > nylon; coated > uncoated; per-string sustain in Advanced | spec.md §1 | `sustain_scale` global param; material decay in `StringMaterials.cpp` | `ModelTests.cpp` `StringPhysics::nylonIsQuiteDifferentFromSteel` | `partial` - no per-string sustain control (rg `perStringSustain`) |
| SPEC-2-01 | Body option A: convolution with per-guitar body IR; IR library for every type | spec.md §2 | `Source/DSP/Body/BodyEngine.cpp`; `Resources/BodyIRs` (216 IRs) | `IntegrationTests.cpp` `Engine::everyGuitarTypeLoadsAndSounds` | `verified` |
| SPEC-2-02 | Body option B: modal bank 20-40 resonators; dimensions shift modes | spec.md §2 | `BodyEngine` modal path | `EngineTests.cpp` `Body::modalBankReproducesTheAirResonance`, `Body::dimensionsMoveTheModes`, `Body::modalBankIsStableAndBounded` | `verified` |
| SPEC-2-03 | Body params: wood (8 listed), size, depth, top thickness, bracing, soundhole, air resonance, age | spec.md §2 | `body_*` params `Source/Parameters.h:89-99`; `BodyModels.cpp` | `EngineTests.cpp` `Body::dimensionsMoveTheModes` (dimensions); `CharacterTests.cpp` `Character::bodyBreakInMovesInTheRightDirection` (age) | `implemented` |
| SPEC-3-01 | Pickup types: single, humbucker, P90, piezo, magnetic soundhole, internal mic, blended piezo+mic | spec.md §3 | `Parameters::pickupTypeNames` `Source/Parameters.cpp:230`; `piezo_mic_blend` | `EngineTests.cpp` `Pickup::resonantFrequencyMatchesTheLcrValues` | `implemented` |
| SPEC-3-02 | Pickup params: position, coil count, coil spacing, R, L, C, magnet, pole spacing, height | spec.md §3 | `PickupEngine.h:56` (coil spacing), `PickupEngine.cpp:428`; pole spacing not found | `EngineTests.cpp` `Pickup::positionCombNullsTheExpectedHarmonic` | `partial` - pole spacing absent (rg `poleSpacing`); position/height now come from Workshop parts |
| SPEC-3-03 | Pickup selector per guitar type; blend knob; coil-tap for humbuckers | spec.md §3 | `pickup_selector`, `pickup_blend`, `coil_tap` params | `EngineTests.cpp` `Pickup::silenceWhenEverythingIsOff` | `implemented` |
| SPEC-4-01 | Fretted mode snaps to frets; 24 frets default, 12-27 by type | spec.md §4 | `GuitarSpec::maxFrets`; `PartAcoustics.cpp:349` clamps 12-27 | `IntegrationTests.cpp` `Engine::chromaticScalePlaysAtTheRightPitch` | `implemented` - pitch verified; 12-27 range not asserted |
| SPEC-4-02 | Temperaments: 12-TET default, just, meantone, well-tempered, custom | spec.md §4 | `enum class Temperament` `Source/Model/Playing/TuningEngine.h:20` | `ModelTests.cpp` `Tuning::temperamentsDifferButStayInRange` | `verified` |
| SPEC-4-03 | Fret noise click (adjustable), fret buzz, fret action height | spec.md §4 | `noise_fret`, `fret_buzz`, setup geometry (`fret-buzz.md`) | `BuzzTests.cpp` `Buzz::lowActionBuzzesAndHighActionDoesNot` | `verified` |
| SPEC-4-04 | Fretless mode: continuous pitch, no buzz, reduced sustain, different attack | spec.md §4 | `fretless` param; `TechniqueEngine` fretless glide | `IntegrationTests.cpp` `Engine::fretlessModeIsGenuinelyContinuous`; `ModelTests.cpp` `Technique::fretlessTurnsLegatoIntoGlide` | `verified` |
| SPEC-4-05 | Bend (cents; half/whole/user; multi-string) | spec.md §4 | `StringEngine` bend; `bend_range` | `EngineTests.cpp` `StringEngine::bendIsSmoothAndReachesTarget` | `verified` |
| SPEC-4-06 | Pre-bend (bend before pluck, release down) | spec.md §4 | Notation only (`PerformanceScore.h:30`); no engine pre-bend found | none | `pending` - see `microtonal-bends.md` pre-bend |
| SPEC-4-07 | Vibrato 3-8 Hz, 5-50 cents; shapes sine, triangle, finger, classical, blues | spec.md §4 | `vibrato_*` params; shapes Sine/Triangle/Square/Saw/Random/Finger (`Parameters.cpp:255`) | none | `partial` - "classical" and "blues" shapes absent |
| SPEC-4-08 | Slide (legato vs picked) | spec.md §4 | `TechniqueEngine::decide` slide branch | `ModelTests.cpp` `Technique::fastNotesBecomeASlide`, `Technique::slideDurationScalesWithDistance` | `verified` |
| SPEC-4-09 | Hammer-on / pull-off with light re-excitation | spec.md §4 | `TechniqueEngine.cpp:121` | `ModelTests.cpp` `Technique::legatoBecomesHammerOnAndPullOff`; `EngineTests.cpp` `StringEngine::legatoDoesNotRetriggerTheAttack` | `verified` |
| SPEC-4-10 | Palm mute: aggressive LP + shortened loop, depth adjustable | spec.md §4 | `StringEngine.cpp:256` | `EngineTests.cpp` `StringEngine::palmMuteShortensAndDarkens` | `verified` |
| SPEC-4-11 | Natural harmonic at nodal points | spec.md §4 | `TechniqueEngine::harmonicPartialForFret` | `ModelTests.cpp` `Technique::harmonicNodesAreDetected` | `verified` |
| SPEC-4-12 | Pinch harmonic; artificial harmonic on any fretted note | spec.md §4 | `LuthierEngine.cpp:778-779`; `Excitation::Kind::PinchHarmonic` | none | `implemented` |
| SPEC-4-13 | Tapping as multiple re-excitations | spec.md §4 | `Technique::Tap` | none | `implemented` (user controls in `two-hand-tapping.md`) |
| SPEC-4-14 | Slide guitar (bottleneck) toggle regardless of type | spec.md §4 | `SlideEngine`, `slide_guitar` re-pointed as `slide_enabled` | `SlideTests.cpp` `Slide::pitchIsContinuous` | `verified` |
| SPEC-4-15 | Whammy: vintage down-only, Floyd up/down, TransTrem keeps intervals, dive-bomb | spec.md §4 | `Source/DSP/Whammy/WhammyEngine.cpp` | `EngineTests.cpp` `Whammy::transTremPreservesChordIntervals`, `vintageTremDetunesChords`, `fixedBridgeDoesNothing` | `verified` |
| SPEC-4-16 | String scrape / pick scrape (filtered noise along wound string) | spec.md §4 | `triggerPickScrape` exists; no trigger path (TODO 3f) | none | `partial` - no user or MIDI trigger |
| SPEC-4-17 | Muted picking distinct from palm mute | spec.md §4 | `Technique::MutedPick` `PlayingEvents.h:24` | none | `implemented` |
| SPEC-5-01 | Pick or fingers per string or per note | spec.md §5 | `use_fingers` global param only | none | `partial` - no per-string/per-note choice |
| SPEC-5-02 | Pick material (nylon, celluloid, delrin, metal, wood, felt), thickness, angle, position | spec.md §5 | `Parameters::pickMaterialNames` `Parameters.cpp:199`; `pick_thickness`, `pick_angle`, `pluck_position` | `NoiseTests.cpp` `PickNoise::clickPitchTracksMaterialAndThickness` | `verified` |
| SPEC-5-03 | Fingerstyle per-finger spectra; finger-to-string assignment | spec.md §5 | not found (rg `fingerAssign`) | none | `pending` - phase 2b `fingerstyle-attack.md` would own it (blocked) |
| SPEC-5-04 | Nail vs flesh | spec.md §5 | `nail_vs_flesh` param | none | `implemented` |
| SPEC-5-05 | Strum direction and speed (2-15 ms spread) | spec.md §5 | `strum_direction`, `strum_speed` | `RhythmSchedulerTests.cpp` `RhythmPatterns::strumSchedulingIsSampleAccurate` | `verified` |
| SPEC-6-01 | Finger slide noise on wound strings, speed-proportional | spec.md §6 | `NoiseEngine` squeak | `NoiseTests.cpp` `Squeak::pitchTracksSpeedAndWinding` | `verified` |
| SPEC-6-02 | Pick attack transient click | spec.md §6 | `PlayingNoise.cpp` | `NoiseTests.cpp` `PickNoise::clickScalesWithVelocityToThePower0_7` | `verified` |
| SPEC-6-03 | Fret noise, release thump, body knock | spec.md §6 | `noise_fret`, `noise_release`, `noise_body_knock` | none | `implemented` |
| SPEC-6-04 | Pickup handling noise on pickup switching | spec.md §6 | not found (rg `pickupHandling`) | none | `pending` |
| SPEC-6-05 | Amp buzz 60 Hz for single-coils, absent for humbuckers, controllable | spec.md §6 | `noise_amp_buzz` | none | `implemented` |
| SPEC-6-06 | Each mechanical sound has global and per-type volume | spec.md §6 | `noise_*` params, `hum_noise` | none | `implemented` |
| SPEC-GT-01 | Ship 24 named guitar types (11 electric, 8 acoustic, 5 bass) | spec.md §Guitar types | `enum class GuitarType` `Source/Model/Guitar/GuitarLibrary.h:24` | `IntegrationTests.cpp` `Engine::everyGuitarTypeLoadsAndSounds`; `ModelTests.cpp` `GuitarLibrary::everyEntryIsInternallyConsistent` | `verified` |
| SPEC-GT-02 | Custom mode: build a guitar from woods, shape, scale, string count, pickups; save | spec.md §Guitar types | `GuitarType::Custom`; superseded by Workshop | see `guitar-workshop.md` | `n/a` - superseded (precedence 8) |
| SPEC-ST-01 | 13 string materials incl. roundwound | spec.md §String types | `enum class StringMaterial` `StringMaterials.h:21` (12; no Roundwound entry) | `ModelTests.cpp` `StringPhysics::everyFactoryGuitarIsStringedPlausibly` | `partial` - "Roundwound" not a selectable material |
| SPEC-ST-02 | Gauges extra light to heavy + custom per-string gauge | spec.md §String types | `enum class StringGauge` `StringMaterials.h:38`; `LuthierEngine::setCustomStringGauge` | none | `implemented` |
| SPEC-ST-03 | String age fresh / broken-in / old | spec.md §String types | `Parameters::stringAgeNames` | `ModelTests.cpp` `StringPhysics::ageDullsAndShortens` | `verified` |
| SPEC-TU-01 | Standard + 11 alternates (Drop D/C/B, DADGAD, Open G/D/E/C, half/full step down, Nashville) | spec.md §Tuning | `enum class TuningPreset` `TuningEngine.h:33` | `ModelTests.cpp` `Tuning::standardTuningIsExact`, `Tuning::everyPresetProducesSaneFrequencies`; `IntegrationTests.cpp` `Engine::everyTuningLoadsAndSounds` | `verified` |
| SPEC-TU-02 | Custom per-string tuning; per-string detune -100..+100 | spec.md §Tuning | `TuningEngine.cpp:148`; headstock popover | `EditorTests.cpp` `Editor::theHeadstockPopoverEditsPerStringTuning` | `verified` |
| SPEC-TU-03 | Realism detune 0-20 cents, refreshed on load or request | spec.md §Tuning | `realism_detune` param | none | `implemented` |
| SPEC-TU-04 | Intonation error scales with fret | spec.md §Tuning | `intonation_error` param | `ModelTests.cpp` `Tuning::intonationErrorGoesSharpUpTheNeck` | `verified` |
| SPEC-TU-05 | Fine tuner per string in Advanced | spec.md §Tuning | `fineTuneCents` `TuningEngine.h:77` | none | `partial` - engine field exists; no Advanced control found |
| SPEC-PM-01 | Three MIDI modes: mono, poly/chord, guitar controller | spec.md §Playing modes | `Parameters::playingModeNames`; `MidiInterpreter` | `ControllerTests.cpp` `Controllers::perChannelRoutingSendsEachChannelToItsString` | `implemented` - controller mode verified; mono and poly modes not individually asserted |
| SPEC-PM-02 | Poly: chord detection and realistic voicing; strum simulation | spec.md §Playing modes | `ChordVoicer`, `ChordDetector` | `ModelTests.cpp` `ChordVoicer::commonChordsAreVoicedPlayably`; `RhythmTests.cpp` `Rhythm::chordDetectorRoundTripsEveryTemplateInEveryKey` | `verified` |
| SPEC-PM-03 | Controller mode: channel = string, MPE compatible | spec.md §Playing modes | `MidiInterpreter`, `ControllerProfile` | `ControllerTests.cpp` `Controllers::perChannelRoutingSendsEachChannelToItsString` | `verified` |
| SPEC-PM-04 | Auto technique detection: legato, pitchbend->bend, aftertouch->vibrato, modwheel->vibrato/whammy, sustain, sostenuto, velocity->pinch, CC palm mute etc. | spec.md §Playing modes | `MidiInterpreter` CC map (`MidiInterpreter.h:112`), `TechniqueEngine::decide` | `ModelTests.cpp` `Technique::controllersTakePriorityOverInference` | `implemented` - priority rule verified; full mapping list not asserted |
| SPEC-FX-01 | Signal flow: cable -> pre-FX -> amp -> loop FX -> cab/mic -> room -> master | spec.md §Effects | `LuthierEngine.cpp` render chain (cable replaced by `GuitarCircuit`) | `IntegrationTests.cpp` `Engine::aNoteProducesSound` | `implemented` - chain produces sound; order not asserted |
| SPEC-FX-02 | 14 amp types incl. Ampeg bass amp; tube stages, tone stack, NFB | spec.md §Effects | `AmpEngine.cpp` model table | `EngineTests.cpp` `Amp::gainProducesHarmonicDistortion` | `implemented` - per-model accuracy untested |
| SPEC-FX-03 | Gain/drive/master, bright, mid boost, presence, standby | spec.md §Effects | `amp_*` params `Parameters.h:210-219` | `EngineTests.cpp` `Amp::standbyIsSilentAndWarmsUp` | `verified` |
| SPEC-FX-04 | Cabinet sizes (8), speaker types, speaker age | spec.md §Effects | `CabinetEngine`, `cab_*` params | `EngineTests.cpp` `Cabinet::procedualFallbackRemovesTheFizz` | `implemented` |
| SPEC-FX-05 | Mic type (6), position, distance with proximity, dual mic blend | spec.md §Effects | `mic_*` params, `dual_mic`, `mic_blend` | none | `implemented` |
| SPEC-FX-06 | Room size (7), material (4), room-mic blend | spec.md §Effects | `room_*` params, `RoomEngine` | `EngineTests.cpp` `Room::biggerRoomsRingLonger` | `verified` |
| SPEC-FX-07 | Stereo via dual cab, stereo effects, dual amp | spec.md §Effects | `mic_width`, stereo post FX | `IntegrationTests.cpp` `Engine::monoCompatibility` | `implemented` |
| SPEC-AF-01 | MIDI capture: last 60 s buffered, "Save last take" to .mid | spec.md §Additional | `Source/Support/MidiCapture.cpp` | `IntegrationTests.cpp` `MidiCapture::capturesAndWritesAFile` | `verified` |
| SPEC-AF-02 | Searchable chord library; user-editable fingerings | spec.md §Additional | `ChordVoicer` library shapes | `ModelTests.cpp` `ChordVoicer::libraryShapesAreSane` | `partial` - no user editing UI found |
| SPEC-AF-03 | Scale/mode overlay on fretboard | spec.md §Additional | `FretboardComponent` right-click "scale overlay" (GAPS B1) | none | `implemented` |
| SPEC-AF-04 | Real-time tab display, exportable | spec.md §Additional | `PerformanceCapture` + NOTATION tab live tab and export (`18a1396`) | `NotationPanelTests.cpp` `NotationTab::stateButtonsLiveTabAndPreview`, `NotationTab::exportsEveryFormat` | `verified` - techniques not captured yet (NOT-6-01) |
| SPEC-AF-05 | Practice tools: metronome, progression looper, backing track | spec.md §Additional | `Source/Practice/*` | see `practice-tools.md` | `implemented` |
| SPEC-AF-06 | Freeze / infinite sustain | spec.md §Additional | `FreezeOverlay`; E-Bow `EBowDriver` | `SustainTests.cpp` `Sustain::freezeLayerHoldsItsLevelForASixtySecondHold`; `EBowTests.cpp` `EBow::aHeldNoteIsSteadyWithinHalfASecondAtHalfIntensity` | `verified` |
| SPEC-AF-07 | Doubler with timing/pitch variation | spec.md §Additional | `PedalType::Doubler` post-amp pedal (`f904956`); defaults per ambiguity-resolutions 3 | `DoublerTests.cpp` `Doubler::mixFullIsACopyTwentyTwoMillisecondsLate`, `Doubler::panPutsTheTakesToTheSides` (timing only) | `implemented` - pitch variation not asserted |
| SPEC-AF-08 | Feedback simulation with threshold and speed | spec.md §Additional | physical loop (`FeedbackLoop`); threshold and speed superseded by ambiguity-resolutions 1 | `FeedbackTests.cpp` `Feedback::aLoudRigTakesOverAndACleanOneDoesNot` | `verified` - as amended |
| SPEC-HU-01 | Humanize: timing, velocity, micro-detune, attack, vibrato variation, noise probability; zero = machine-perfect | spec.md §Humanize | `hum_*` params `Parameters.h:251-256` | none | `partial` - vibrato-variation slider not found (params are timing/velocity/detune/attack/noise/strum) |
| SPEC-GUI-01 | Window: rounded rect with cutaway; 1200x720 default; resizable, aspect preserved | spec.md §GUI | `PluginEditor.cpp:264`; `PluginEditor.h:32` | `EditorTests.cpp` `Editor::theProcessorHandsOverAnEditorAtItsDocumentedSize`, `Editor::itLaysOutAndPaintsAcrossItsResizeRange` | `implemented` - size and resize verified; cutaway and aspect lock not asserted |
| SPEC-GUI-02 | Header, Easy bands, Advanced columns layout | spec.md §GUI | superseded by `gui-integration.md` 2-4 | - | `n/a` - superseded (precedence 2) |
| SPEC-GUI-03 | Easy: fretboard click plays a note; notes highlight | spec.md §Easy | `FretboardComponent.cpp` | none | `implemented` |
| SPEC-GUI-04 | Easy: pickup switch and tone/volume knobs overlaid on body | spec.md §Easy | `GuitarBodyComponent.cpp` hit regions | `EditorTests.cpp` `Editor::everyHitRegionOnTheIllustrationDescribesItself` | `verified` |
| SPEC-GUI-05 | Six macro knobs with dice and lock | spec.md §Easy | `macro_*` params; `EasyPanel.cpp` | `IntegrationTests.cpp` `Presets::randomiseRespectsLocks` | `implemented` |
| SPEC-GUI-06 | AUDITION button plays a phrase | spec.md §Easy | `AuditionPhrase` | `IntegrationTests.cpp` `Audition::everyPhraseProducesUsableMidi` | `verified` |
| SPEC-GUI-07 | Export / drag-out handle | spec.md §Easy | not implemented (`docs/KNOWN_ISSUES.md` "Drag-out export") | none | `pending` |
| SPEC-INT-01 | Knob context menu: Enter value / Reset / Copy / Paste / MIDI Learn / Assign to macro / Lock / Randomize; double-click reset | spec.md §Interaction | `Source/UI/Widgets.cpp:43`, `Widgets.h:10` | `EditorTests.cpp` `Editor::rightClickOffersModulationAndBuildsTheRoute` (modulate items only) | `implemented` |
| SPEC-INT-02 | Fretboard right-click: mute string, set capo, mark | spec.md §Interaction | `FretboardComponent` menu | `GenreKitTests.cpp` `GenreKits::capoRemovesFretsBelowItAndMovesThePitch` (capo pitch) | `implemented` |
| SPEC-INT-03 | MIDI Learn from right-click | spec.md §Interaction | `Source/Support/MidiLearn.cpp` | `IntegrationTests.cpp` `MidiLearn::mapsAndUnmapsCleanly` | `verified` |
| SPEC-INT-04 | Overlays close by Escape, click-outside, close button; one at a time | spec.md §Interaction | `Overlays.cpp`, `PluginEditor.cpp` | `EditorTests.cpp` `Editor::everyOverlayShortcutOpensItsOwnOverlayAndEscapeClosesIt` | `partial` - Escape and exclusivity tested; click-outside untested |
| SPEC-PR-01 | JSON `.luthierpreset` with guitar, tuning, mode, rig, FX, humanize, MIDI maps, tags | spec.md §Preset system | `Source/Presets/PresetManager.cpp` | `IntegrationTests.cpp` `Presets::stateRoundTripsExactly` | `verified` |
| SPEC-PR-02 | Factory presets in bundle `Resources/Presets/Factory/<category>`; user folder per OS | spec.md §Preset system | `Resources/Presets`; README runtime table | `IntegrationTests.cpp` `Presets::everyFactoryPresetLoadsAndPlays` | `implemented` - presets load; install location not asserted |
| SPEC-PR-03 | Additional preset folders registered via Preferences | spec.md §Preset system | not found (rg `presetFolders`) | none | `pending` |
| SPEC-EX-01 | Quick export of audition phrase to WAV | spec.md §Audio export | `Source/Support/AudioExporter.cpp` | none | `implemented` |
| SPEC-EX-02 | Export As: WAV/AIFF/FLAC, bit depth, sample rate, length, normalize, filename pattern | spec.md §Audio export | `Overlays.cpp:1042-1080` | none | `partial` - normalize and filename pattern not found |
| SPEC-EX-03 | MIDI capture export `.mid`; export folder `Renders/` | spec.md §Audio export | `MidiCapture` | `IntegrationTests.cpp` `MidiCapture::capturesAndWritesAFile` | `verified` |
| SPEC-ID-01 | Tension validity 30-90 N; impossible tunings rejected with warning | spec.md §Identity 1 | `Source/Validator.cpp` | `ModelTests.cpp` `Validator::correctsRatherThanCrashing`, `Validator::strictModeRejects` | `verified` |
| SPEC-ID-02 | Fret range validity: out-of-range notes rejected or transposed | spec.md §Identity 2 | `Validator::checkFretRange` `Validator.cpp:84` | `ModelTests.cpp` `Validator::passesGoodValues` | `verified` |
| SPEC-ID-03 | Body always present except explicit "no body" | spec.md §Identity 3 | `BodyEngine.h:14`; "No Body (Experimental)" choice | none | `implemented` |
| SPEC-ID-04 | At least one pickup; all off -> silence + warning | spec.md §Identity 4 | `PickupEngine` | `EngineTests.cpp` `Pickup::silenceWhenEverythingIsOff` (silence; warning untested) | `partial` - warning not verified |
| SPEC-ID-05 | Velocity changes brightness, not just level | spec.md §Identity 5 | `Excitation` | `EngineTests.cpp` `StringEngine::harderPluckIsBrighterNotJustLouder` | `verified` |
| SPEC-ID-06 | Higher notes decay faster | spec.md §Identity 6 | loop filter | `EngineTests.cpp` `StringEngine::higherNotesDecayFaster` | `verified` |
| SPEC-ID-07 | Sympathetic resonance always active, cannot be disabled | spec.md §Identity 7 | `coupling_amount` param range | none | `implemented` - check `coupling_amount` minimum is above 0 |
| SPEC-ID-08 | Finger slides always produce some noise on wound strings; zero is a choice not a default | spec.md §Identity 8 | squeak defaults (`string-squeak.md`) | `NoiseTests.cpp` `Squeak::zeroIsFreeAndSlideModeSuppressesIt` | `implemented` |
| SPEC-ID-09 | Chord voicings physically playable; closest playable fallback | spec.md §Identity 9 | `ChordVoicer` | `ModelTests.cpp` `ChordVoicer::impossibleChordDegradesGracefully` | `verified` |
| SPEC-VP-01 | Validator pipeline: physics, fret, damping, body, pickup checks; nudge or reject with log | spec.md §Validator | `Source/Validator.cpp` | `ModelTests.cpp` `Validator::correctsRatherThanCrashing`, `strictModeRejects`, `passesGoodValues` | `verified` |
| SPEC-TH-01 | Audio thread no alloc/locks; denormals off; NaN guards | spec.md §Threading | `LuthierEngine.cpp:1283`, `PluginProcessor.cpp:782`; `sanitise` `DspCommon.h:51` | `CircuitTests.cpp` `Circuit::sweepingEveryControlDoesNotAllocate`; `WorkshopPresetTests.cpp` `WorkshopSwap::noFileIsTouchedFromTheAudioThreadDuringASwap` | `partial` - no suite-wide allocation probe on processBlock |
| SPEC-TH-02 | Worker threads for preset load, IR load, coupling recalculation | spec.md §Threading | `ConvolutionInstaller.h` | none | `implemented` |
| SPEC-TH-03 | Sample-rate and block-size agnostic | spec.md §Threading | `prepareToPlay` paths | `IntegrationTests.cpp` `Engine::sampleRateChangesAreSurvived`, `Engine::blockSizeChangesAreSurvived` | `verified` |
| SPEC-TH-04 | 20 ms smoothing on continuous params; cached, rate-limited coefficient updates | spec.md §Threading | `kParamSmoothSeconds` `DspCommon.h` | none | `implemented` |
| SPEC-MIDI-01 | CC control for all continuous params | spec.md §MIDI | `MidiLearn` | `IntegrationTests.cpp` `MidiLearn::mapsAndUnmapsCleanly` | `verified` |
| SPEC-MIDI-02 | MPE per-note bend, pressure, timbre | spec.md §MIDI | `MidiInterpreter.h:204` | none | `implemented` |
| SPEC-MIDI-03 | Bend range per string in controller mode | spec.md §MIDI | `ControllerProfile` | `ControllerTests.cpp` `Controllers::applyingAProfileConfiguresTheInterpreter` | `implemented` |
| SPEC-MIDI-04 | Program change recalls presets | spec.md §MIDI | `PluginProcessor.cpp:1252` | `StateModelTests.cpp` `StateModel::aProgramChangeRightAfterAStateRestoreDoesNotWipeIt` | `verified` |
| SPEC-PREF-01 | Preferences: default preset, preset folders, MIDI defaults, export defaults, oversampling, FFT block size, max polyphony, realism defaults, appearance | spec.md §Preferences | Options pages `OptionsPages.cpp` (oversampling on AUDIO) | `EditorTests.cpp` `Editor::everyOptionsPageSelectsAndPaints` (paint only) | `partial` - default preset, preset folders, FFT block size, max polyphony not found |
| SPEC-DEL-01 | CMake project; Windows VST3 + macOS VST3/AU | spec.md §Deliverables 1 | `CMakeLists.txt` | Windows VST3 built (PROGRESS); macOS not built here | `partial` |
| SPEC-DEL-02 | Source layout DSP/Model/UI/Presets folders | spec.md §Deliverables 2 | `Source/` tree (UI not split into Easy/Advanced/Fretboard folders) | - | `implemented` |
| SPEC-DEL-03 | Body IR library 100+; speaker IR library 50+ | spec.md §Deliverables 3-4 | `Resources/BodyIRs` (216), `Resources/CabIRs` (504), synthesised | README "Deliberate deviations" | `implemented` - synthesised, not measured |
| SPEC-DEL-04 | Nine docs (README, GUITAR_PHYSICS, PLAYING_TECHNIQUES, PRESET_FORMAT, KEYBOARD_SHORTCUTS, USER_MANUAL, CHANGELOG, KNOWN_ISSUES, TROUBLESHOOTING) | spec.md §Deliverables 5 | `docs/*.md` (8 present) + `spec/README.md` | file listing | `implemented` - content currency not audited |
| SPEC-DEL-05 | Tests: unit per DSP module, tension, chord voicing, pluginval CI, overlay dismissal, 10k fuzz, latency | spec.md §Deliverables 6 | `Source/Tests/` | `IntegrationTests.cpp` `Parameters::fuzzAcrossTenThousandStates`, `Engine::latencyIsReportedAndPlausible` | `partial` - pluginval not in CI (last manual run 2026-09-19) |
| SPEC-DEL-06 | Signed and notarized installers per platform | spec.md §Deliverables 7 | none (`docs/KNOWN_ISSUES.md`) | none | `pending` - see `installer.md` |
| SPEC-DEL-07 | CLI offline batch renderer | spec.md §Deliverables 8 | `Tools/RenderCli.cpp` (`LuthierRender`) | none | `implemented` |
| SPEC-SHIP-01 | Strings stable at extreme values | spec.md §Ship 1 | `StringEngine` | `EngineTests.cpp` `StringEngine::survivesExtremeParameters` | `verified` |
| SPEC-SHIP-02 | Coupling never runs away | spec.md §Ship 2 | `CouplingMatrix` | `EngineTests.cpp` `Coupling::cannotRunAway` | `verified` |
| SPEC-SHIP-03 | Body convolution latency reported correctly | spec.md §Ship 3 | `PluginProcessor.cpp:759` | `IntegrationTests.cpp` `Engine::latencyIsReportedAndPlausible` | `verified` |
| SPEC-SHIP-04 | All body IRs load without clicks | spec.md §Ship 4 | `IrLibrary` | `IntegrationTests.cpp` `Engine::everyGuitarTypeLoadsAndSounds` (types only, not every IR) | `partial` |
| SPEC-SHIP-05 | Playable fingerings for every common chord | spec.md §Ship 5 | `ChordVoicer` | `ModelTests.cpp` `ChordVoicer::commonChordsAreVoicedPlayably` | `verified` |
| SPEC-SHIP-06 | Fretless continuous pitch verified by sweep | spec.md §Ship 6 | - | `IntegrationTests.cpp` `Engine::fretlessModeIsGenuinelyContinuous` | `verified` |
| SPEC-SHIP-07 | Bends/vibrato without zipper noise | spec.md §Ship 7 | - | `EngineTests.cpp` `StringEngine::bendIsSmoothAndReachesTarget` | `verified` |
| SPEC-SHIP-08 | Pluginval level 10 on both platforms | spec.md §Ship 8 | - | Manual run 1.0.3 strictness 10 at `f18bf22` (2026-09-19, PROGRESS "Not done"); not re-run since phase 2; macOS never | `partial` |
| SPEC-SHIP-09 | Overlay dismissal tests for every overlay type | spec.md §Ship 9 | - | `EditorTests.cpp` `Editor::everyOverlayShortcutOpensItsOwnOverlayAndEscapeClosesIt` | `verified` |
| SPEC-SHIP-10 | 30 min in each of 7 hosts, no stuck windows / focus bugs / spikes | spec.md §Ship 10 | - | not performed (`docs/KNOWN_ISSUES.md`) | `pending` - manual host matrix |
| SPEC-SHIP-11 | Real MIDI guitar and MPE controller end to end | spec.md §Ship 11 | - | not performed | `pending` - needs hardware |
| SPEC-SHIP-12 | Blind listening: 70%+ cannot distinguish | spec.md §Ship 12 | - | not performed | `pending` - needs listening panel |

## engine.md (companion: DSP contract)

Precedence 10 (DSP ground rules). `CableSim` (§9, §21 step 10) is removed by
`volume-knob-interaction.md` (precedence 9). Four deliberate deviations are
documented in `spec/README.md` "Deliberate deviations" (Lagrange default,
damping as cutoff, per-string pickup comb, per-sample coupling); see Conflicts
C-03..C-06.

| ID | Requirement (short) | Source (file §section) | Implementation location | Verification | Status |
|---|---|---|---|---|---|
| ENG-0-01 | All DSP in double; float only at output | engine.md §0.1 | `Source/DSP/Common/DspCommon.h` header note; all DSP classes use `double` | code review only | `implemented` |
| ENG-0-02 | No alloc / string / file I/O in processBlock; buffers pre-allocated | engine.md §0.2 | `LuthierEngine.h:11` note | `CircuitTests.cpp` `Circuit::sweepingEveryControlDoesNotAllocate`; `WorkshopPresetTests.cpp` `WorkshopSwap::noFileIsTouchedFromTheAudioThreadDuringASwap` | `partial` - probes cover circuit and swap only |
| ENG-0-03 | DC blocker 5-10 Hz + NaN/Inf guard [-4,4] on every recursive stage | engine.md §0.3 | `DCBlocker` (7 Hz), `sanitise` `DspCommon.h:51`, `kGuardLimit = 4.0` | `EngineTests.cpp` `Common::sanitiseClampsNonFinite`, `Common::dcBlockerRemovesOffset` | `implemented` - primitives verified; per-stage use by code review only |
| ENG-0-04 | Smoothing: 20 ms linear params, 30 ms exp cutoffs, 5 ms discrete crossfades | engine.md §0.4 | `kParamSmoothSeconds`, `kCutoffSmoothSeconds`, `kSwitchCrossfadeSeconds` `DspCommon.h` | none | `implemented` |
| ENG-0-05 | All SR-dependent work in prepareToPlay; survive 44.1k -> 96k | engine.md §0.5 | `LuthierEngine::prepare` | `IntegrationTests.cpp` `Engine::sampleRateChangesAreSurvived` | `verified` |
| ENG-0-06 | Time params stored in seconds/Hz | engine.md §0.6 | params in ms/s/Hz (`Parameters.cpp`) | `IntegrationTests.cpp` `Presets::audioIsIdenticalAfterARoundTrip` | `implemented` |
| ENG-0-07 | ScopedNoDenormals at top of every processBlock | engine.md §0.7 | `LuthierEngine.cpp:1283`, `PluginProcessor.cpp:782` | `IntegrationTests.cpp` `Engine::fastSlidesProduceNoNansOrDenormals` | `verified` |
| ENG-0-08 | Every module has reset(); called on transport start, preset load, panic | engine.md §0.8 | `reset()` in every `Source/DSP/**/*.h` | `IntegrationTests.cpp` `Engine::panicSilencesEverything` | `partial` - transport-start reset not verified |
| ENG-0-09 | Every module testable in isolation (prepare/process/no globals) | engine.md §0.9 | per-module classes | `EngineTests.cpp` per-module suites | `verified` |
| ENG-0-10 | Nonlinear stages oversampled, 4x default, 2x/8x selectable | engine.md §0.10 | `Source/DSP/Common/Oversampler.h`; `oversampling` param (1x/2x/4x/8x) | `EngineTests.cpp` `Common::oversamplingSuppressesAliasing`, `Common::oversamplerIsTransparentAtUnityShaper` | `verified` |
| ENG-1-01 | Module chain MIDI -> technique -> tuning -> strings -> body -> pickup -> circuit -> pre FX -> amp -> post FX -> cab -> room -> master | engine.md §1 | `LuthierEngine::process` | `IntegrationTests.cpp` `Engine::aNoteProducesSound` | `implemented` - chain produces sound; order not asserted |
| ENG-1-02 | Only back-flow is the coupling matrix | engine.md §1 | `CouplingMatrix` | code review | `implemented` |
| ENG-2-01 | Typed events NoteOn/Off, PitchBend, Pressure, CC, Sustain, Whammy | engine.md §2 | `Source/Model/Playing/PlayingEvents.h` | none | `implemented` |
| ENG-2-02 | Modes A mono, B poly/chord, C controller (per-channel or MPE) | engine.md §2 | `MidiInterpreter` | `ControllerTests.cpp` `Controllers::perChannelRoutingSendsEachChannelToItsString`, `Controllers::chordGroupsSoundOneWindowAfterTheyWerePlayed` (Poly chords sound one reported window after they were played, `a357142`) | `verified` |
| ENG-2-03 | String assignment: lowest string below pitch, within maxFrets; clip above; prefer nearest active string | engine.md §2 | `MidiInterpreter` / `ChordVoicer` | `ModelTests.cpp` `ChordVoicer::singleNotesStayNearTheHand` | `verified` |
| ENG-2-04 | Default CC map: 1 vibrato, 2 whammy, 4 expression, 11 master, 64 sustain, 65 slide, 66 sostenuto, 67 palm mute, 70-79 user; aftertouch vibrato | engine.md §2 | `MidiInterpreter::resetCcMapToDefaults` `MidiInterpreter.h:114` | none | `implemented` |
| ENG-3-01 | TuningEngine `(string, fret, bendCents) -> Hz`; ET formula with detune and intonation slope | engine.md §3 | `Source/Model/Playing/TuningEngine.cpp` | `ModelTests.cpp` `Tuning::standardTuningIsExact`, `Tuning::twelfthFretIsAnOctave` | `verified` |
| ENG-3-02 | Temperaments ET12, Just, Meantone, Werckmeister3, Kirnberger3, Custom (12 doubles in preset) | engine.md §3 | `TuningEngine.h:20`, `TuningEngine.cpp:24-30` | `ModelTests.cpp` `Tuning::temperamentsDifferButStayInRange` | `verified` |
| ENG-3-03 | Standard, 7- and 8-string open frequencies; Drop D/C, DADGAD, Open G, half/full step | engine.md §3 | `TuningEngine` presets | `ModelTests.cpp` `Tuning::everyPresetProducesSaneFrequencies` | `verified` |
| ENG-3-04 | Manual detune ±100; realism detune ±0-20 stored in preset | engine.md §3 | `TuningEngine.cpp:148`; `realism_detune` | none | `implemented` |
| ENG-3-05 | Drift: off by default; every 30 s up to ±5 cents | engine.md §3 | `TuningEngine.cpp:86` (30 s interval); `tuning_drift` | `CharacterTests.cpp` `Character::tunerDriftStaysWithinItsStatedAmplitude` (character drift) | `implemented` |
| ENG-3-06 | Intonation slope default 0.3 c/fret, per string | engine.md §3 | `intonation_error` (global) | `ModelTests.cpp` `Tuning::intonationErrorGoesSharpUpTheNeck` | `partial` - per-string slope not exposed |
| ENG-4-01 | Techniques Pluck..Strum (12) | engine.md §4 | `enum class Technique` `PlayingEvents.h:15` (adds MutedPick, ArtificialHarmonic) | none | `implemented` |
| ENG-4-02 | Detection priority: palm mute > pinch > harmonic > tap > slide guitar > legato (slide <40 ms / hammer / pull, vel < 80) > pluck | engine.md §4 | `TechniqueEngine::decide` `TechniqueEngine.cpp:48-140` (`legato_window` default 40 ms) | `ModelTests.cpp` `Technique::legatoBecomesHammerOnAndPullOff`, `fastNotesBecomeASlide`, `controllersTakePriorityOverInference` | `verified` |
| ENG-4-03 | Per-technique string actions (re-excite 10-30%, slide ramp + noise, palm mute 5k->800 Hz, harmonic filtering, strum offsets) | engine.md §4 | `LuthierEngine.cpp:778-823`, `StringEngine.cpp:256` | `EngineTests.cpp` `StringEngine::palmMuteShortensAndDarkens`, `StringEngine::legatoDoesNotRetriggerTheAttack` | `verified` |
| ENG-5-01 | Lumped EKS delay line; delay = SR / f | engine.md §5.2 | `StringEngine`, `FractionalDelayLine.h` | `EngineTests.cpp` `StringEngine::pluckProducesCorrectPitch` | `verified` |
| ENG-5-02 | Allpass fractional delay (recommended) | engine.md §5.2, §20.11 | Lagrange 5th order default; allpass and linear selectable (`FractionalDelayLine.h:7`) | `EngineTests.cpp` `DelayLine::allInterpolatorsPreserveLevel`, `DelayLine::modulationStaysClean` | `implemented` - deviation, see Conflict C-03 |
| ENG-5-03 | Buffer sized for 40 Hz at 96 kHz (4096) | engine.md §5.2 | `kMinStringHz = 18` `DspCommon.h` | none | `implemented` |
| ENG-5-04 | Delay length smoothed (~2 ms), never jumps | engine.md §5.2 | `StringEngine` | `EngineTests.cpp` `StringEngine::bendIsSmoothAndReachesTarget` | `verified` |
| ENG-5-05 | Loop filter one-pole; damping per material; palm mute ~0.60, muted pick ~0.75 | engine.md §5.3 | expressed as cutoff (`StringEngine.cpp:256`) | `EngineTests.cpp` `StringEngine::palmMuteShortensAndDarkens` | `implemented` - deviation, see Conflict C-04 |
| ENG-5-06 | Inharmonicity allpass in loop; default B per string 0.00008..0.00080 | engine.md §5.4 | `StringEngine.h:162` dispersion cascade; B from core diameter | `EngineTests.cpp` `StringEngine::dispersionStretchesPartialsSharp` | `verified` |
| ENG-5-07 | Excitation: triangle of pluck length, material filters (pick BP 2-4k, nail 3-5k, flesh LP 2k, thumb LP 1k), comb notch, 0.5-2 ms random attack | engine.md §5.5 | `Source/DSP/String/Excitation.cpp` | `EngineTests.cpp` `StringEngine::harderPluckIsBrighterNotJustLouder` | `implemented` - per-material spectra untested |
| ENG-5-08 | Coupling: bandpass at receiver f0; symmetric; 0.01-0.03; energy cap | engine.md §5.6 | `CouplingMatrix.cpp` | `EngineTests.cpp` `Coupling::matrixIsSymmetricAndZeroOnTheDiagonal`, `neighboursCoupleMoreThanDistantStrings`, `cannotRunAway` | `verified` |
| ENG-5-09 | reset() clears delay, loop filter, fractional state, DC blocker | engine.md §5.7 | `StringEngine::reset` | `IntegrationTests.cpp` `Engine::panicSilencesEverything` | `implemented` - panic silence verified; full state clearing not asserted |
| ENG-5-10 | Mono voice steal: 5 ms ramp down, never hard cut | engine.md §5.8 | `StringEngine` | none | `implemented` |
| ENG-6-01 | Body convolution default; partitioned FFT; latency reported | engine.md §6.1 | `BodyEngine`, `ConvolutionInstaller.h` | `IntegrationTests.cpp` `Engine::latencyIsReportedAndPlausible` | `verified` |
| ENG-6-02 | Body IRs at `Resources/BodyIRs/<type>/<size>_<wood>_<age>.wav`, 100+ | engine.md §6.1 | `Resources/BodyIRs` (216) | file listing | `implemented` |
| ENG-6-03 | Modal: 30-50 resonators; scale with dimensions; air mode 90-140 Hz; modal params in JSON per body | engine.md §6.2 | `BodyEngine` modal bank (compiled tables, not JSON) | `EngineTests.cpp` `Body::modalBankReproducesTheAirResonance`, `Body::dimensionsMoveTheModes` | `partial` - modal data not in per-body JSON |
| ENG-7-01 | Pickup types and per-pickup params (position, coils, spacing, R/L/C, magnet, height) | engine.md §7 | `Source/DSP/Pickup/PickupEngine.h` | `EngineTests.cpp` `Pickup::resonantFrequencyMatchesTheLcrValues` | `verified` |
| ENG-7-02 | Position comb nulls harmonic n where n*p integer | engine.md §7.1 | per-string comb (deviation: before sum) | `EngineTests.cpp` `Pickup::positionCombNullsTheExpectedHarmonic` | `verified` |
| ENG-7-03 | LCR tank resonance f=1/(2pi sqrt(LC)), Q | engine.md §7.2 | moved into `GuitarCircuit` (DECISIONS "coil's resonance moved") | `EngineTests.cpp` `Pickup::resonantFrequencyMatchesTheLcrValues`; `CircuitTests.cpp` `Circuit::potValueMovesTheResonance` | `verified` |
| ENG-7-04 | Magnet EQ: A2 mid boost 800 Hz, A3 scoop, A5 bright, ceramic aggressive | engine.md §7.3 | `pickup_magnet_*` params, `PickupEngine` | none | `implemented` |
| ENG-7-05 | Humbucker = two opposite-polarity coils with offset comb; coil tap one coil | engine.md §7.4, §20.12 | `PickupEngine.cpp:308,428` | none | `implemented` |
| ENG-7-06 | Piezo: HP 40 Hz, LP 15 kHz, ~3 kHz resonance, no position comb | engine.md §7.5 | `PickupEngine` piezo branch | none | `implemented` |
| ENG-7-07 | Internal mic: body output with gentle tilt | engine.md §7.6 | `PickupEngine` | none | `implemented` |
| ENG-7-08 | 3/5-position switching, per-pickup volume, coil tap; selection crossfades 5 ms | engine.md §7.7 | `pickup_selector` (7 positions), `pickup_volume_*` | none | `implemented` |
| ENG-8-01 | Whammy vintage ±2 st, Floyd -24..+12, TransTrem ratio, Bigsby ±1 | engine.md §8.1-8.4 | `WhammyEngine`, `bridge_type` | `EngineTests.cpp` `Whammy::transTremPreservesChordIntervals`, `vintageTremDetunesChords` | `verified` |
| ENG-8-02 | TransTrem transpose-lock detents | engine.md §8.3 | `WhammyEngine.h:60`; `transpose_lock` | none | `implemented` |
| ENG-8-03 | 5 ms whammy smoothing | engine.md §8 | `WhammyEngine.cpp:13` | none | `implemented` |
| ENG-8-04 | Floyd spring resonance: 50-100 ms noise burst 200-500 Hz on return to centre | engine.md §8.2 | `LuthierEngine.cpp:1434` | none | `implemented` |
| ENG-8-05 | Default whammy CC 2; MPE Y mappable | engine.md §8 | CC map | none | `implemented` |
| ENG-8-06 | Per-string whammy toggle (MPE) | engine.md §8 | `WhammyEngine::setStringPosition` `WhammyEngine.h:68` | none | `implemented` |
| ENG-9-01 | CableSim one-pole | engine.md §9 | removed; `GuitarCircuit` | - | `n/a` - superseded (precedence 9) |
| ENG-10-01 | Pre-FX: compressor, wah, envelope filter, octaver, pitch shifter, overdrive, distortion, fuzz, boost, chorus, volume pedal | engine.md §10 | `enum class PedalType` `Source/DSP/Effects/Pedal.h:21` | `EngineTests.cpp` `Effects::everyPedalTypeRunsCleanly` | `verified` |
| ENG-10-02 | 8 slots, drag reorder, 10 ms bypass crossfade, 4x oversampled drive | engine.md §10 | `EffectsChain.h:22` (`kNumSlots = 8`); `PedalRack.cpp` | `EngineTests.cpp` `Effects::chainReordersWithoutGlitching`, `Effects::bypassIsTransparent` | `verified` |
| ENG-11-01 | Amp models: Twin, Tweed, Plexi, JCM800, AC30, Rectifier, Bogner, Diezel, Orange, SVT, Custom | engine.md §11.1 | `AmpEngine.cpp` voicing table (adds Champ, Acoustic DI) | `EngineTests.cpp` `Amp::gainProducesHarmonicDistortion` | `implemented` - Custom builder (preamp count, tube, stack) UI not verified |
| ENG-11-02 | Preamp stages with asymmetric tanh, cathode bypass, coupling HP, 4x oversampled | engine.md §11.2 | `AmpEngine.cpp` | `EngineTests.cpp` `Amp::gainProducesHarmonicDistortion` | `verified` |
| ENG-11-03 | Coupled analog tone stack (Yeh); presence shelf ~4 kHz in NFB | engine.md §11.3, §20.10 | `Source/DSP/Amp/ToneStack.cpp` | none | `implemented` |
| ENG-11-04 | Phase inverter, push-pull power amp, sag, output transformer | engine.md §11.4-11.6 | `AmpEngine.cpp` | none | `implemented` |
| ENG-11-05 | Standby mutes; 30 s warm-up fade | engine.md §11.7 | `amp_standby` | `EngineTests.cpp` `Amp::standbyIsSilentAndWarmsUp` | `verified` |
| ENG-12-01 | Post-FX: chorus, phaser, flanger, tremolo, rotary, delay, reverb, spring reverb, EQ | engine.md §12 | `PedalType` post entries | `EngineTests.cpp` `Effects::everyPedalTypeRunsCleanly` | `verified` |
| ENG-13-01 | Cab IR per cab/speaker/mic/position; 200+ IRs; partitioned convolution | engine.md §13.1 | `CabinetEngine`, `Resources/CabIRs` (504) | none | `implemented` |
| ENG-13-02 | Mic selector switches IR; positions and distances | engine.md §13.2 | `mic_*` params | none | `implemented` |
| ENG-13-03 | Dual-mic blend, second panned | engine.md §13.3 | `dual_mic`, `mic_blend`, `mic_width` | none | `implemented` |
| ENG-13-04 | Async IR load with zero-latency fallback, never silent | engine.md §13.4 | `CabinetEngine.h:153-176` procedural fallback | `EngineTests.cpp` `Cabinet::procedualFallbackRemovesTheFizz` | `verified` |
| ENG-14-01 | Room: 8-16 ER taps + FDN late reverb; size, damping, mix | engine.md §14 | `RoomEngine.h:90-91` (16 taps, FDN 8) | `EngineTests.cpp` `Room::biggerRoomsRingLonger` | `verified` |
| ENG-15-01 | Master gain -60..+12 dB | engine.md §15 | `Parameters.cpp:647` | `IntegrationTests.cpp` `Parameters::everyParameterHasAUniqueIdAndSaneDefault` | `implemented` - range declared; not asserted by a test |
| ENG-15-02 | Limiter -0.3 dBFS | engine.md §15 | `MasterBus.h:57` | `EngineTests.cpp` `Master::limiterHoldsTheCeiling` | `verified` |
| ENG-15-03 | Metering peak, RMS, LUFS | engine.md §15 | `MasterBus.h:40` | `EngineTests.cpp` `Master::meteringTracksTheSignal` | `verified` |
| ENG-15-04 | Final DC blocker 5 Hz | engine.md §15 | `MasterBus` | none | `implemented` |
| ENG-16-01 | Per-preset state list; JSON `.luthierpreset`; host state = JSON + UI state (mode, view, A/B, history) | engine.md §16 | `PresetManager`, `PluginProcessor::getStateInformation` | `IntegrationTests.cpp` `Presets::stateRoundTripsExactly` | `verified` |
| ENG-17-01 | Threading: strings serial on audio thread; workers for IR load, preset scan, capture flush; AbstractFifo + atomics | engine.md §17 | `ConvolutionInstaller.h`, `ParameterBridge` atomics | none | `implemented` |
| ENG-18-01 | Latency = body + cab partitions + oversampling + lookahead via setLatencySamples | engine.md §18 | `PluginProcessor.cpp:759` | `IntegrationTests.cpp` `Engine::latencyIsReportedAndPlausible`; `RoutingTests.cpp` `Routing::perOutputLatencyIsConsistent` | `verified` |
| ENG-19-01 | Unit: StringEngine pitch, decay, bend | engine.md §19 | - | `EngineTests.cpp` `StringEngine::pluckProducesCorrectPitch`, `higherNotesDecayFaster`, `bendIsSmoothAndReachesTarget` | `verified` |
| ENG-19-02 | Unit: TuningEngine within 0.1 cents | engine.md §19 | - | `ModelTests.cpp` `Tuning::standardTuningIsExact` | `verified` |
| ENG-19-03 | Unit: TechniqueEngine each technique from MIDI | engine.md §19 | - | `ModelTests.cpp` `Technique::*` (6 tests; tap/pinch/palm-mute triggers not individually asserted) | `partial` |
| ENG-19-04 | Unit: PickupEngine comb and resonance/Q | engine.md §19 | - | `EngineTests.cpp` `Pickup::positionCombNullsTheExpectedHarmonic`, `resonantFrequencyMatchesTheLcrValues` | `verified` |
| ENG-19-05 | Unit: Whammy TransTrem intervals | engine.md §19 | - | `EngineTests.cpp` `Whammy::transTremPreservesChordIntervals` | `verified` |
| ENG-19-06 | Unit: BodyEngine IR loads; modal frequencies | engine.md §19 | - | `EngineTests.cpp` `Body::modalBankReproducesTheAirResonance` | `verified` |
| ENG-19-07 | Unit: AmpEngine harmonic content per stage | engine.md §19 | - | `EngineTests.cpp` `Amp::gainProducesHarmonicDistortion` | `verified` |
| ENG-19-08 | Unit: CouplingMatrix no runaway | engine.md §19 | - | `EngineTests.cpp` `Coupling::cannotRunAway` | `verified` |
| ENG-19-09 | Integration: chromatic scale pitch; bend A->B smooth; fast slide no NaN/denormal | engine.md §19 | - | `IntegrationTests.cpp` `Engine::chromaticScalePlaysAtTheRightPitch`, `Engine::fastSlidesProduceNoNansOrDenormals` | `verified` |
| ENG-19-10 | Integration: 30 s at 96 kHz all FX on, CPU < 15% | engine.md §19 | - | `IntegrationTests.cpp` `Engine::cpuStaysWithinBudget` asserts < 85% of real time at test rate, 3 s | `partial` - weaker bar than spec |
| ENG-19-11 | Round-trip: every factory preset save/reload, output within 0.01 dB | engine.md §19 | - | `IntegrationTests.cpp` `Presets::audioIsIdenticalAfterARoundTrip`, `Presets::everyFactoryPresetLoadsAndPlays` | `verified` |
| ENG-19-12 | Pluginval level 10 in CI on both platforms | engine.md §19 | - | manual 2026-09-19 Windows only | `partial` |
| ENG-19-13 | Host matrix 30 min each; MIDI controller testing | engine.md §19 | - | not performed | `pending` |
| ENG-20-01 | Pitfalls 1-20 (double state, fractional delay, oversampled amp, DC blockers, per-block coupling, reset, no std::vector on audio thread, no logging on audio thread, SR changes, coupled tone stack, allpass interp, two-coil humbucker, continuous bend, shaped excitation, voice steal, smooth delay, MPE, feedback cap 0.998, mono check, modular classes) | engine.md §20 | covered by rows above; #5 and #11 deliberately deviated (C-05, C-03) | `IntegrationTests.cpp` `Engine::monoCompatibility` (#19) | `implemented` |
| ENG-20-02 | Feedback amount capped at 0.998 | engine.md §20.18 | pedal feedback params | `EngineTests.cpp` `Effects::secretEffectIsStableAtMaximumRegeneration` | `implemented` |
| ENG-20-03 | Every factory preset passes mono-compatibility | engine.md §20.19 | - | `IntegrationTests.cpp` `Engine::monoCompatibility` (check it iterates every preset) | `implemented` |
| ENG-21-01 | Build order 1-25 | engine.md §21 | PROGRESS M0-M30 | PROGRESS milestones | `n/a` - process |
| ENG-22-01 | 6 strings, all FX, 96 kHz / 128: < 8% CPU on 2020 mid-range machine | engine.md §22 | - | not measured to spec | `pending` - see `performance-budget.md` |
| ENG-22-02 | 16 instances at 48 kHz / 256: no glitches | engine.md §22 | - | not measured | `pending` |
| ENG-22-03 | Preset load < 500 ms incl. async IR load | engine.md §22 | - | not measured | `pending` |
| ENG-22-04 | MIDI-in to audio-out < 2 ms plus reported latency | engine.md §22 | sample-accurate event handling | `RoutingTests.cpp` `Routing::midiOutPassThroughIsSampleExact` (MIDI out, not in->audio) | `pending` |

## theme.md (companion: shared visual identity)

**Overridden for this plugin** by `spec/proposals/visual-polish.md` section 6
wherever section 6 speaks (user decision 2026-09-23, DECISIONS.md). Rows
section 6 replaces are `n/a` here and tracked under visual-polish. What
section 6 does not mention (grid, spacing, value arc role, output LED, data
stream, header, version footer) still comes from theme.md. Since `a406915`
the UI uses the guitar-shop theme (`Source/UI/Theme.*`);
`docs/THEME_AS_BUILT.md` still describes the older walnut re-tint and needs
updating.

| ID | Requirement (short) | Source (file §section) | Implementation location | Verification | Status |
|---|---|---|---|---|---|
| THM-PAL-01 | Palette (#0E1116 background, #E8532A accent, teal secondary, etc.) | theme.md §Color palette | `Source/UI/Theme.h:34-63` (walnut re-tint) | - | `n/a` - replaced by VP-6.1 |
| THM-PAL-02 | Shadow rgba(0,0,0,0.55) 8 px blur y+2 | theme.md §Color palette | `Source/UI/Theme.cpp` | none | `implemented` |
| THM-TYP-01 | Fonts Inter / JetBrains Mono with fallbacks; label 11 px uppercase +0.08em; values 13-14 px; tabular numerics | theme.md §Typography | `Source/UI/Theme.cpp:27,42` | none | `partial` - headings replaced by VP-6.2; body/numeric rules still apply |
| THM-KNB-01 | Knob 48/36/64 px, gradient body, 2 px indicator, center dot | theme.md §Knobs | `Source/UI/Widgets.cpp` | - | `n/a` - knob body replaced by VP-6.3 |
| THM-KNB-02 | 270 deg value arc outside the knob, 3 px, 4 px gap | theme.md §Knobs | `Widgets.cpp`; `RangesUi` arc marking | `RangesUiTests.cpp` `RangesUi::controlsFollowASwappedRangeAndMarkTheValue` (marking) | `implemented` - kept by VP-6.3 |
| THM-KNB-03 | Double-click resets; right-click value entry; value readout on hover/drag else label | theme.md §Knobs | `Widgets.cpp` | none | `implemented` |
| THM-SLD-01 | Slider 4 px track, 16x24 thumb, ticks | theme.md §Sliders | `Widgets.cpp` | - | `n/a` - replaced by VP-6.3 fader with brass cap |
| THM-BTN-01 | Buttons 4 px radius, 28 px tall, on/off styles, 100 ms flash | theme.md §Buttons | `Widgets.cpp` | none | `partial` - on/off toggles become mini toggle switches (VP-6.3); pills stay |
| THM-MTR-01 | Meters: smooth segments, teal->orange->yellow->red, peak hold 1.5 s then 20 dB/s, mono peak readout | theme.md §Meters | `Widgets.cpp:941` | none | `implemented` |
| THM-LAY-01 | 8 px grid, 16 px min edge padding, 1 px section rules, radii 6/4/2 | theme.md §Layout | `Theme.h` metrics | none | `implemented` |
| THM-LAY-02 | Section headers uppercase with 2x12 accent bar | theme.md §Layout | `Widgets.cpp` | - | `n/a` - replaced by VP-6.2 engraved plate |
| THM-LAY-03 | No skeuomorphism, no faux wood/metal | theme.md §Layout | - | - | `n/a` - reversed by VP-6 (wood grain, Tolex, brass) |
| THM-INT-01 | Value changes animate 80 ms ease-out | theme.md §Interaction | not found | none | `deferred` - animated transitions declined by the user (visual-polish 0.4); confirm this covers value easing (Conflict C-12) |
| THM-INT-02 | Hover brightens ~8%; resize cursor on knobs | theme.md §Interaction | `Widgets.cpp` | none | `implemented` |
| THM-INT-03 | Drag vertical; Shift coarse; Ctrl/Cmd ultra-fine | theme.md §Interaction | `Widgets.cpp:397` | none | `implemented` |
| THM-INT-04 | Tooltips dark pill after 400 ms | theme.md §Interaction | `PluginEditor.cpp:447` (`Metrics::tooltipDelayMs`) | none | `implemented` |
| THM-HDR-01 | Header strip 32 px with name, gear, preset selector, A/B | theme.md §Header | `HeaderBar.cpp` (48 px per spec.md / gui-integration) | - | `n/a` - gui-integration 2 owns header (precedence 2) |
| THM-SIG-01 | Diagonal 12 px notch top-left | theme.md §Signature | `Theme.cpp` | - | `n/a` - replaced by VP-6.4 brass headstock inlay |
| THM-SIG-02 | Version in 9 px muted mono bottom-right | theme.md §Signature | `PluginEditor.cpp:324` | none | `implemented` |
| THM-ANI-01 | Output LED: grey to white with level, red above 0 dBFS | theme.md §Animations | `Widgets.cpp` (OutputLed) | none | `implemented` |
| THM-ANI-02 | Data stream: ~10 lines, faded top/bottom two, light green, real data, pauses when idle, skipped if no space | theme.md §Animations | `Widgets.cpp`, `EasyPanel.cpp`, `Theme.h:63` | none | `implemented` |

## include.md (companion: mandatory VST features)

| ID | Requirement (short) | Source (file §section) | Implementation location | Verification | Status |
|---|---|---|---|---|---|
| INC-HLP-01 | Help section explaining every feature, workflow, GUI | include.md §Help | `Source/UI/Overlays.cpp` Help overlay | `EditorTests.cpp` `Editor::everyOverlayShortcutOpensItsOwnOverlayAndEscapeClosesIt` (opens only) | `implemented` - content completeness unreviewed |
| INC-HLP-02 | Help shows version, licence, GitHub link, homepage, support email | include.md §Help | `Overlays.cpp:394-462` | none | `partial` - URLs and email are placeholders (`luthieraudio.example`); release blocker |
| INC-HLP-03 | Troubleshooting: manual install/uninstall; where presets go | include.md §Help | Help overlay text; `docs/TROUBLESHOOTING.md` | none | `implemented` |
| INC-HLP-04 | Debug button in help | include.md §Help | `Overlays.cpp` debug section | none | `implemented` |
| INC-ICO-01 | Unique app icon | include.md §Custom Icon | `Resources/icon.png`, `icon_small.png`, `luthier.ico`; `CMakeLists.txt` ICON_BIG | file present | `implemented` |
| INC-PRE-01 | Bank of descriptively named presets | include.md §Presets | `Source/Presets/FactoryPresets.cpp` (36) | `IntegrationTests.cpp` `Presets::everyFactoryPresetLoadsAndPlays` | `verified` |
| INC-RST-01 | Reset button restores defaults | include.md §Reset | `HeaderBar.cpp`; `PluginProcessor` reset | `IntegrationTests.cpp` `Presets::resetRestoresDefaults` | `verified` |
| INC-FIL-01 | Save / Save As / Open / Options via dropdown | include.md §save | `HeaderBar.cpp` menu | none | `implemented` |
| INC-FIL-02 | Options: tooltips on/off, MIDI and audio device, open location in Explorer | include.md §save | `OptionsPages.cpp`; `tooltipsEnabled` `PluginEditor.cpp:447` | `EditorTests.cpp` `Editor::everyOptionsPageSelectsAndPaints` (paint only) | `implemented` |
| INC-EXP-01 | Export audio to WAV with quality options; success popup with location, length, name, quality | include.md §Export audio | `Overlays.cpp:1255-1270` | none | `implemented` |
| INC-MID-01 | Import and export MIDI | include.md §import/export midi | MIDI import as export source `Overlays.cpp:1023`; `MidiCapture` export | `IntegrationTests.cpp` `MidiCapture::capturesAndWritesAFile` | `partial` - general MIDI import to the instrument is `midi-export.md` scope |
| INC-RCL-01 | Right-click any control: MIDI map, reset, set value | include.md §Right click | `Widgets.cpp:43` | `IntegrationTests.cpp` `MidiLearn::mapsAndUnmapsCleanly` | `implemented` |
| INC-DND-01 | Drag and drop wherever a sample can be imported | include.md §drag and drop | `ToneMatchPanel::filesDropped` (IRs) | none | `implemented` - IR is the only sample import |
| INC-TIP-01 | Hover tooltips | include.md §tool tips | tooltips on controls | none | `implemented` - coverage of every control unverified |
| INC-RND-01 | Randomize; each press after the first resets before randomising | include.md §Randomize | `PluginProcessor.cpp:1464`; `PresetManager::randomise` | `IntegrationTests.cpp` `Presets::randomiseNeverProducesSomethingBroken`, `Presets::randomiseRespectsLocks` | `partial` - "reset before each subsequent randomise" not asserted |
| INC-BLD-01 | VST and standalone builds; CLAP and Linux later | include.md §Other notes | `CMakeLists.txt:25` `FORMATS VST3 Standalone` | PROGRESS targets | `implemented` (CLAP/Linux deferred by include.md itself) |
| INC-BLD-02 | Progress file kept; test every combination; logic-error pass | include.md §Other notes | `spec/PROGRESS.md` | - | `n/a` - process |
| INC-DBG-01 | Debug window shows raw data live | include.md §extra | `Overlays.cpp` debug panel; `Diagnostics` | `IntegrationTests.cpp` `Diagnostics::ringBufferAndSelfTestWork` | `implemented` |
| INC-DBG-02 | "Create log file on crash" checkbox, off every load, dated file; where to find and how to send | include.md §extra | `Diagnostics.cpp`, `OptionsPages.cpp` | none | `implemented` |
| INC-DBG-03 | Hard reset to defaults removing cache files | include.md §extra | `PluginProcessor.cpp` hard reset (`ensureFactoryPresetsInstalled`) | none | `implemented` |
| INC-DBG-04 | Export troubleshooting file: self-diagnostic, settings, audio/MIDI, licence, version, DAW; copied at head of crash log | include.md §extra | `Diagnostics.h`, `Overlays.cpp:347` | `IntegrationTests.cpp` `Diagnostics::ringBufferAndSelfTestWork` | `implemented` |
| INC-EGG-01 | Easter egg: specific pixel reveals hidden effect tab with close button and "secret" tooltips | include.md §easter egg | `PluginEditor.cpp:231-255`; `SecretEffect.h` | `EngineTests.cpp` `Effects::secretEffectIsStableAtMaximumRegeneration` (DSP only) | `implemented` |
| INC-THM-01 | Implement theme.md | include.md §theme.md | see theme.md / visual-polish sections | - | `partial` |

## proposals/visual-polish.md (approved 2026-09-23)

Approved by the user in full (knob caps, accent, guitar-shop theme); section 6
overrides `theme.md` for Luthier. Ordered after `guitar-illustration.md`
rebuild (TODO G) and before other visual work (TODO V). `a406915` built
section 1 (lighting in `GuitarRenderer`) and section 6 (guitar-shop and
maple palettes, bell knobs, mini toggles, brass fader caps, framed panels,
engraved plates, brass headstock mark, Lato and Bebas Neue bundled under OFL)
with `Theme::*` tests. `52cb9d7` added sections 2-3 as standalone painters
(`Source/UI/Faces/AmpFace.*`, `PedalFace.*`, `KnobCaps.*`,
`FaceMaterials.*`) with `Faces::*` tests; they are not yet on the AMP
section or the racks (that integration is uncommitted). Section 4's VU and
room light and section 5's accent picker are not built.

| ID | Requirement (short) | Source (file §section) | Implementation location | Verification | Status |
|---|---|---|---|---|---|
| VP-0-01 | Section 6 replaces theme.md for Luthier; theme.md still supplies grid, spacing, value arc role, LED, data stream | visual-polish §0.1 | `Source/UI/Theme.*` guitar-shop theme (`a406915`) | `ThemeTests.cpp` `Theme::theDefaultIsTheGuitarShop` | `verified` |
| VP-0-02 | Textures keep 4.5:1 text contrast on Default, High contrast, Light; High contrast turns textures/sheen off | visual-polish §0.2 | `Theme.*` palettes, `Palette::textured` | `ThemeTests.cpp` `Theme::everyTextPairMeetsContrastOnTheThreePalettes`, `Theme::highContrastIsFlat` | `verified` |
| VP-0-03 | Textures/lighting cached; redrawn only when depicted thing changes | visual-polish §0.3 | cached renders | `ThemeTests.cpp` `Theme::controlsRenderInEveryPaletteAndRepeatExactly` (repeatability) | `implemented` - redraw-on-change not measured |
| VP-0-04 | No new motion/transitions | visual-polish §0.4 | - | - | `n/a` - constraint |
| VP-1-01 | Key + fill light; baked body shading; specular edge | visual-polish §1 | `GuitarRenderer.cpp` lighting | `GuitarRendererTests.cpp` `GuitarIllustration::highContrastHasNoLighting` (presence of lit layers) | `implemented` |
| VP-1-02 | Lacquer sheen by `finish.gloss`: gloss band, satin faint, oil/natural none | visual-polish §1 | `GuitarRenderer.cpp` sheen | none | `implemented` |
| VP-1-03 | Metal hardware reflection gradients per `hardware_color` | visual-polish §1 | `GuitarRenderer.cpp` hardware | none | `implemented` |
| VP-1-04 | Drop shadows from pickups, bridge, pickguard | visual-polish §1 | `GuitarRenderer.cpp` shadows | none | `implemented` |
| VP-2-01 | Amp faces per family: Tolex, grille, faceplate, generic logo plate, pilot light follows Standby | visual-polish §2 | `Source/UI/Faces/AmpFace.*` painter for every amp model (`52cb9d7`); shown on the Easy amp card and the Advanced Amplifier section (`AmpFacePanel.*`) | `FacesTests.cpp` `Faces::everyAmpFaceDrawsInsideItsBoundsInEveryPalette`, `Faces::thePilotFollowsStandbyAndTheLedFollowsBypass`, `Faces::noFaceTextNamesABrand` | `verified` - with `FacesIntegration::theAdvancedAmpSectionHasItsControlsOnTheFace`, `theEasyAmpCardHasItsKnobsOnTheFace`, `rendersOfBothWindowsInEveryPalette` |
| VP-2-02 | Pedal faces: enclosure colour, footswitch, bypass LED, knob layout, generic name; same params/locations | visual-polish §2 | `Source/UI/Faces/PedalFace.*` painter for every pedal incl. Doubler (`52cb9d7`); each rack slot draws its face (`PedalRack.cpp`); slot dry/wet labelled BLEND | `FacesTests.cpp` `Faces::everyPedalFaceDrawsInsideItsBoundsInEveryPalette`, `Faces::layoutsKeepEveryControlOnTheFace`, `Faces::knobValuesReachTheFace` | `verified` - with `FacesIntegration::everyRackSlotHasItsControlsOnItsFace` |
| VP-3-01 | Model-specific knob caps on amp/pedal faces only, keeping value arc, indicator colour, hit area | visual-polish §3 | `Source/UI/Faces/KnobCaps.*` | `FacesTests.cpp` `Faces::everyKnobCapRendersAndKeepsTheArc` | `verified` - with `FacesIntegration::everyRackSlotHasItsControlsOnItsFace`, `theAdvancedAmpSectionHasItsControlsOnTheFace` |
| VP-4-01 | Tube glow tracks amp drive level | visual-polish §4 | `AmpFace` valves glow with a drive input; nothing feeds it the live drive level | none | `partial` - painter input only |
| VP-4-02 | Optional VU needle meter beside the LED | visual-polish §4 | - | none | `pending` |
| VP-4-03 | Room card warms/widens with size and wet level | visual-polish §4 | - | none | `pending` |
| VP-4-04 | Live touches update at dataflow meter rates and grey out when stale | visual-polish §4 | - | none | `pending` |
| VP-5-01 | Options > Appearance accent picker: brass + five others, each 4.5:1 on every palette | visual-polish §5 | APPEARANCE page says accent tint unbuilt (GAPS A3) | none | `pending` |
| VP-5-02 | "Follow the guitar" accent from finish colour, contrast-adjusted | visual-polish §5 | - | none | `pending` |
| VP-5-03 | Default accent is section 6's brass | visual-polish §5 | `Theme.*` accent `#D4A24C` | `ThemeTests.cpp` `Theme::theDefaultIsTheGuitarShop` | `verified` |
| VP-6.1-01 | Palette: rosewood bg ~#1E1511, walnut panel with grain, Tolex raised, ivory text ~#EFE3CC, tan muted ~#B9A58A, brass accent ~#D4A24C, jewel green-teal ~#6FA58A, tube-glow warning | visual-polish §6.1 | `Theme.*` guitar-shop palette | `ThemeTests.cpp` `Theme::theDefaultIsTheGuitarShop` (background, panel, text, accent) | `verified` |
| VP-6.1-02 | Light palette maple/cream; High contrast unchanged with textures off; all pairs 4.5:1 | visual-polish §6.1 | `Theme.*` maple palette | `ThemeTests.cpp` `Theme::everyTextPairMeetsContrastOnTheThreePalettes`, `Theme::highContrastIsFlat` | `verified` - maple colours by eye |
| VP-6.2-01 | Condensed vintage display face for headings (open licence, shipped); warm sans body; tabular numbers | visual-polish §6.2 | Bebas Neue + Lato bundled (`Resources/Fonts`, OFL) | `ThemeTests.cpp` `Theme::theBundledFontsLoad` | `verified` |
| VP-6.2-02 | Section headers as engraved brass/ivory plates | visual-polish §6.2 | `Theme.*` / `Widgets.*` engraved plates | `ThemeTests.cpp` `Theme::controlsRenderInEveryPaletteAndRepeatExactly` (render only) | `implemented` |
| VP-6.3-01 | Standard knob: black bell/dome, cream/brass pointer, skirt, value arc outside | visual-polish §6.3 | bell knobs | `ThemeTests.cpp` `Theme::controlsRenderInEveryPaletteAndRepeatExactly` (render only) | `implemented` |
| VP-6.3-02 | Mini toggle switches for on/off; pills for tabs and modes | visual-polish §6.3 | mini toggles | as VP-6.3-01 | `implemented` |
| VP-6.3-03 | Fader sliders with brass cap | visual-polish §6.3 | brass fader caps | as VP-6.3-01 | `implemented` |
| VP-6.3-04 | Panels framed like cabinet/pedalboard; corner screws on larger panels | visual-polish §6.3 | framed walnut panels | as VP-6.3-01 | `implemented` |
| VP-6.4-01 | Brand mark: brass headstock inlay replaces diagonal notch; LED stays | visual-polish §6.4 | brass headstock mark | none | `implemented` |
| VP-6.5-01 | Layout, widths, arc meaning, hit areas, focus rings (restyled), accessibility unchanged | visual-polish §6.5 | - | - | `n/a` - constraint |
| VP-7-01 | Test: textured/lit surfaces render identically twice (cached, not regenerated per frame) | visual-polish §7 | - | `ThemeTests.cpp` `Theme::controlsRenderInEveryPaletteAndRepeatExactly`; `GuitarRendererTests.cpp` `GuitarIllustration::theKeyChangesWithEveryVisibleChange`; `FacesTests.cpp` `Faces::facesRenderIdenticallyTwice` | `verified` |
| VP-7-02 | Test: High contrast has no gradients/sheen/textures | visual-polish §7 | - | `ThemeTests.cpp` `Theme::highContrastIsFlat`; `GuitarRendererTests.cpp` `GuitarIllustration::highContrastHasNoLighting`; `FacesTests.cpp` `Faces::highContrastFacesAreFlat` | `verified` |
| VP-7-03 | Test: every accent option on every palette meets 4.5:1 | visual-polish §7 | - | none | `pending` |
| VP-7-04 | Test: Standby and bypass change pilot light and pedal LEDs | visual-polish §7 | - | `FacesTests.cpp` `Faces::thePilotFollowsStandbyAndTheLedFollowsBypass` (painter state in, not the plugin's Standby / bypass) | `partial` - no test through the panels |
| VP-7-05 | Test: palette text pairs 4.5:1 (automated); standard controls render in three palettes (PNG, by eye) | visual-polish §7 | - | `ThemeTests.cpp` `Theme::everyTextPairMeetsContrastOnTheThreePalettes`, `Theme::controlsRenderInEveryPaletteAndRepeatExactly` | `verified` |
| VP-7-06 | Test: Advanced window with every face visible within UI frame budget | visual-polish §7 | - | none | `pending` |

## README.md (spec/README.md: project readme)

Mostly descriptive. Its claims that are requirements (targets, DSP rules,
runtime locations, deviations) are rows; the rest duplicates engine.md.

| ID | Requirement (short) | Source (file §section) | Implementation location | Verification | Status |
|---|---|---|---|---|---|
| RDM-BLD-01 | Targets `Luthier_VST3`, `Luthier_Standalone`, `Luthier_AU` (macOS), `LuthierTests`, `LuthierRender` | README §Targets | `CMakeLists.txt:25` lists `VST3 Standalone` only; AU not in FORMATS | PROGRESS "Targets" | `partial` - AU target not declared |
| RDM-BLD-02 | Build copies `Resources/` beside each artefact | README §Install | `CMakeLists.txt` | none | `implemented` |
| RDM-BOX-01 | Box contents: 25 instruments, 17 tunings, 7 temperaments, 12 materials, 11 gauges, 21 pedals, 13 amps, 10 cabs / 8 speakers / 7 mics, 720 IRs, 36 presets | README §What is in the box | enums in `GuitarLibrary.h`, `TuningEngine.h`, `StringMaterials.h`, `Pedal.h`, `AmpEngine.cpp` | `ModelTests.cpp` `GuitarLibrary::everyEntryIsInternallyConsistent` | `implemented` - counts should be rechecked after Workshop (27 factory guitar files) |
| RDM-DSP-01 | Ten DSP rules | README §Rules | see ENG-0-01..10 | see engine.md rows | `implemented` |
| RDM-CLI-01 | `luthier-render` options `--midi --preset --out --audition --guitar --list-presets --help --verbose` | README §Offline renderer | `Tools/RenderCli.cpp` | none | `implemented` |
| RDM-TST-01 | Test runner filter by name, `--list`, non-zero exit on failure | README §Tests | `Source/Tests/TestMain.cpp` | suite runs (TODO) | `implemented` |
| RDM-IR-01 | `make_irs.py` / `make_icon.py` deterministic, byte-identical | README §Regenerating | `scripts/make_irs.py`, `scripts/make_icon.py` | none | `implemented` |
| RDM-LOC-01 | Runtime locations: Presets/User, Renders, Diagnostics, Factory fallback to Documents if read-only; Options button for each | README §Where things live | `PresetManager`, `OptionsPages.cpp` FILE LOCATIONS | none | `implemented` |
| RDM-DEV-01 | Deliberate deviations: synthesised IRs, drawn guitar, Lagrange default, damping as cutoff, per-string comb, per-sample coupling, core-diameter inharmonicity, drawn cutaway, re-tinted theme | README §Deliberate deviations | as described | - | `n/a` - recorded as Conflicts C-03..C-06 |
| RDM-LIC-01 | Licence text in Help > About; `THIRD_PARTY_LICENCES.txt` | README §Licence | Help overlay "About and Licence" | none | `partial` - verify `THIRD_PARTY_LICENCES.txt` exists before release |

## JUCE_CLAUDE_GUIDELINES.md (development guidelines)

Process guidance for the implementer. Rows are the checkable code
requirements; the context7 workflow rule is process (`n/a`).

| ID | Requirement (short) | Source (file §section) | Implementation location | Verification | Status |
|---|---|---|---|---|---|
| JCG-1-01 | Query context7 before writing JUCE code | JCG §1 | - | - | `n/a` - process |
| JCG-2-01 | No alloc, locks, file/network, GUI calls, exceptions, locking RNG on the audio thread | JCG §2 | see ENG-0-02 | `CircuitTests.cpp` `Circuit::sweepingEveryControlDoesNotAllocate` (partial probe) | `partial` |
| JCG-4-01 | Pin JUCE to a tag | JCG §4 | `ThirdParty/JUCE` 8.0.10 (clone, not submodule) | PROGRESS Environment | `implemented` |
| JCG-4-02 | Explicit FORMATS; stable PLUGIN_CODE / MANUFACTURER_CODE | JCG §4 | `CMakeLists.txt:23-25` (`Ltha` / `Lthr`) | none | `implemented` |
| JCG-4-03 | Static MSVC runtime on Windows | JCG §4 | `CMakeLists.txt:12` | none | `implemented` |
| JCG-4-04 | C++17 or 20, no GNU extensions | JCG §4 | `CMakeLists.txt:5-7` | none | `implemented` |
| JCG-4-05 | Warnings as errors in CI (/W4 /WX) | JCG §4 | `CMakeLists.txt:10` disables C4244/4267/4305/4996; no /W4 /WX; no CI | none | `pending` |
| JCG-5-01 | Single APVTS; param IDs in one header; cached raw pointers; Attachments in editor; state via APVTS | JCG §5 | `Source/Parameters.h`, `ParameterBridge::cachePointers` | `IntegrationTests.cpp` `Parameters::everyParameterHasAUniqueIdAndSaneDefault` | `implemented` - unique IDs verified; attachments not asserted |
| JCG-6-01 | ScopedNoDenormals; variable block sizes; clear unused outputs; MIDI at sample position | JCG §6 | `PluginProcessor.cpp:782` | `IntegrationTests.cpp` `Engine::blockSizeChangesAreSurvived`; `RoutingTests.cpp` `Routing::stringActivityIsSampleAccurate` | `implemented` - block-size and sample-accurate MIDI verified; output clearing not asserted |
| JCG-6-02 | `isBusesLayoutSupported` validates layouts | JCG §6 | `PluginProcessor.cpp:212` | `RoutingTests.cpp` `Routing::everyLayoutRendersCleanly` | `verified` |
| JCG-7-01 | Audio->GUI via FIFO/AsyncUpdater; background->audio via pointer swap | JCG §7 | `ParameterBridge` AsyncUpdater; `ConvolutionInstaller.h` | none | `implemented` |
| JCG-8-01 | Cache paths/images; buffered components; 30 Hz timers | JCG §8 | editor 4 Hz poll timer; per-panel timers | none | `implemented` |
| JCG-10-01 | State backward-compatible forever, version-tagged; no absolute paths | JCG §10 | `PresetManager.cpp:478-502` schema version | `IntegrationTests.cpp` `Presets::unknownFieldsSurviveARoundTrip`; `ToneMatchTests.cpp` `ToneMatch::presetPathsAreRelativeUnderTheLibraryRoot` | `verified` |
| JCG-11-01 | Parameter IDs locked once shipped | JCG §11 | pinned count assertion | `IntegrationTests.cpp` `Parameters::everyParameterHasAUniqueIdAndSaneDefault` | `verified` |
| JCG-12-01 | Definition of done: pluginval strictness 10, clean warnings, round-trip | JCG §12 | - | pluginval last run 2026-09-19 | `partial` |

## INDEX.md (spec index)

| ID | Requirement (short) | Source (file §section) | Implementation location | Verification | Status |
|---|---|---|---|---|---|
| IDX-ORD-01 | Build order phases 1, 2, 2b, 3, 4, 5, 5b, 6 | INDEX §Order to build in | `spec/TODO.md` ordering | - | `n/a` - process |
| IDX-2B-01 | Nine phase-2b specs exist | INDEX §Phase 2b | not on disk | - | `blocked` - see "Phase 2b: referenced but missing" |
| IDX-GR-01 | engine.md §0 ground rules hold everywhere | INDEX §Global rules | see ENG-0-* | - | `implemented` |
| IDX-GR-02 | Physical deltas only: part/setup changes land as physical parameter changes, never downstream EQ | INDEX §Global rules | `PartAcoustics::mapSpec` | `PartAcousticsTests.cpp` `PartAcoustics::hardwareColourIsSilent`, `PartAcoustics::everyMappedFieldMovesSomething` | `verified` |
| IDX-GR-03 | Real ranges by default: every physical parameter declares stock and advanced ranges | INDEX §Global rules | `Source/PhysicalRange.h`, `RangeRegistry` (sparse: amp, circuit, pick, setup families) | `RangeTests.cpp` `Ranges::everyPhysicalRangeIsValid` | `partial` - registry sparse by design; later realism params must register |
| IDX-GR-04 | Honest magnitudes; Workshop shows small effects as small | INDEX §Global rules | Workshop spectrum delta pane (`SpectrumDelta`, `d45fcd6`), fixed ±12 dB axis with auto-zoom as an option | `WorkshopBenchTests.cpp` `WorkshopSpectrum::aNullChangeIsFlat` (null only) | `implemented` - small-effect scaling not asserted |
| IDX-GR-05 | Tune Builder is a MIDI writer, never on the audio path | INDEX §Global rules | `TuneTimeline` produces MIDI only; not yet wired to the engine | `TuneBuilderTests.cpp` `TuneBuilder::timelinePutsChordsMelodyBassAndSectionsOnTheBeat` | `implemented` |
| IDX-GR-06 | Every composition event undoable; regeneration keeps locked notes | INDEX §Global rules | not built | none | `pending` |
| IDX-GR-07 | Every output exportable: audio, MIDI, notation, project | INDEX §Global rules | audio, MIDI (MIDI OUT) and notation (NOTATION, from the capture) export exist; tune audio and project export not built | `NotationTests.cpp` `Notation::musicXmlRoundTrips`; `MidiOutPanelTests.cpp` `MidiOutPanel::exportWritesTheCaptureInTheChosenProfile`; `NotationPanelTests.cpp` `NotationTab::exportsEveryFormat` | `partial` - tune and project export |
| IDX-GR-08 | gui-integration = UI location truth; ui-wiring = attachment truth; qa-polish = ship gate | INDEX §Global rules | - | - | `n/a` - precedence |
| IDX-TST-01 | Every new file's Tests section lands in `LuthierTests` | INDEX §closing | `Source/Tests/` | per-spec rows below | `partial` - many spec Tests sections have no test yet |

## Process documents (no product requirements)

Read in full for status. They record decisions and progress rather than
demanding behaviour; where one records a decision that resolves a conflict,
the Conflicts section cites it.

| File | Role | Requirements | Notes used in this audit |
|---|---|---|---|
| `spec/TODO.md` | Work list | none (`n/a`) | Steps 2g and 6 done; 2c (feedback) and 2f (Aux 8) closed; G, 7 (bench), 9 (capture + NOTATION), 10 (MIDI OUT), 11 (practice model) and 12 (tune model) partly done with remainders listed; 13c phase-2b blocked; items V, 2d, 2h, 5b, 6e, 8, 13-18 open (read at `52cb9d7`) |
| `spec/DECISIONS.md` | Judgement calls | none (`n/a`) | Cited as D-refs in Conflicts; the visual-polish override decision; entries through `18a1396` (feedback loop, E-Bow, doubler, migration table, capture feed) |
| `spec/GAPS.md` | Gap audit (2026-09-18, partly stale) | none (`n/a`) | Self-described as least trustworthy about what exists; rows re-checked against code |
| `spec/PROGRESS.md` | Build log | none (`n/a`) | 413 tests green at `753fb05` (448 at `0d225f0`); pluginval 2026-09-19 at `f18bf22` |
| `spec/REVIEW.md` | Review of the four base specs | none (`n/a`) | Its five ambiguities are resolved by `ambiguity-resolutions.md` |
