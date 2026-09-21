# LUTHIER — Build Progress

Resumable build log. Update after every milestone.

## Environment
- Windows 10 Pro 19045, 4 logical cores, 3.88 GB RAM (build with low parallelism)
- CMake 4.4.3, MSVC 14.44.35207 (VS2022 BuildTools), Windows SDK 10.0.26100.0
- JUCE 8.0.10 at `ThirdParty/JUCE`
- Python 3.14.5 (used for IR generation scripts only, not at runtime)

## Milestones
- [x] M0  Specs read (spec.md, engine.md, include.md, theme.md), tree scaffolded
- [x] M1  Build system + JUCE + trivial plugin compiles
- [x] M2  DSP Common (guards, filters, smoothing)
- [x] M3  StringEngine (fixed pitch)
- [x] M4  StringEngine (bend/vibrato/slide/technique)
- [x] M5  TuningEngine
- [x] M6  PickupEngine
- [x] M7  BodyEngine (convolution + modal)
- [x] M8  CouplingMatrix
- [x] M9  MidiInterpreter
- [x] M10 TechniqueEngine + ChordVoicer
- [x] M11 WhammyEngine
- [x] M12 CableSim
- [x] M13 PreEffectsChain
- [x] M14 AmpEngine
- [x] M15 PostEffectsChain
- [x] M16 CabinetEngine
- [x] M17 RoomEngine
- [x] M18 MasterBus
- [x] M19 Preset system
- [x] M20 UI Theme + widgets
- [x] M21 UI Easy mode
- [x] M22 UI Advanced mode
- [x] M23 MIDI Learn + right-click
- [x] M24 Export (audio + MIDI)
- [x] M25 Help / Options / Randomize / Reset
- [x] M26 IR libraries generated
- [x] M27 Test suite
- [x] M28 Debug + troubleshooting features
- [x] M29 Easter egg
- [x] M30 Docs complete

### Extension specs (INDEX.md build order)

- [x] M31 routing-io.md   — multi-out buses, sidechain, re-amp, MIDI out, per-output latency
- [x] M32 modulation-matrix.md — 8 LFOs, 4 DAHDSR envelopes, 2 step sequencers, 2 followers, note/CC/macro/random sources, 1024-route matrix, MOD panel
- [x] M33 rhythm-engine.md — chord detector, voicer, strum and fingerpick schedulers, 37 factory patterns, 28 genre kits, RHYTHM panel, Easy-mode strip
- [x] M34 live-performance.md — 128 snapshots with crossfade and morph, setlists, tap tempo, kill switch, monitor mix, expression calibration, Live strip
- [x] M35 controllers.md — 9 profiles, per-string channel map, latency wizard, pitch dead zone, lazy note-off handling, multi-controller merge
- [x] M36 practice-tools.md — metronome, looper, backing track, scale and ear trainers, tab reader, progression looper, session recorder, PRACTICE drawer with eight tabs
- [x] M37 tone-match.md — user IR loading, sweep/MLS/burst cab match, EQ match, capture utility, TONE MATCH panel
- [x] M38 notation-export.md — PerformanceScore, live TAB view, MusicXML, Guitar Pro, ASCII tab and MIDI export, with importers
- [x] M39 character-wear.md — dead spots, fret wear, tuner drift, aged electronics, body break-in, temperature and humidity
- [x] M40 accessibility.md — screen reader, keyboard-only navigation, colourblind palettes, UI scale, localisation
- [x] M41 updates-telemetry.md — update checks, opt-in telemetry, crash reporting, license activation, privacy dashboard

## Current state

All forty-one milestones are done. Both targets build clean and the whole suite
passes: **302 tests, 647,366 checks**, exit code 0.

The last stretch was less about writing the remaining specs than about finding out
that the code written for them had never actually run. `LuthierTests` excluded
`Source/UI/` at the time, so eight new panels had never been compiled at all, and
the test binary on disk was stale: the build that was supposed to produce it had
failed and left the previous exe in place. Five whole suites - ToneMatch,
Notation, Character, Accessibility and Telemetry - were sitting in the tree,
compiled into object files, and never linked into anything that ran them. Running
them for the first time is what produced the rest of this list.

**`Capture` had no default constructor.** `JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR`
declares a deleted copy constructor, and a user-declared constructor of any kind
suppresses the implicit default one. `PluginProcessor` holds a `Capture` by value,
so this broke the plugin build and the test build together. It is now asked for
explicitly, with a comment saying why.

**`TuningEngine::reset()` was missing its braces.** `characterDriftCents = 0.0` sat
outside the range-for that was meant to contain it, so character drift was never
cleared on reset. The compiler only caught it because the stray line happened to
reference the loop variable; had it referenced anything else it would have compiled
and quietly misbehaved. A sweep for the same shape across `Source/` found no others.

**The ASCII tab importer read its own beat ruler as a string.** `looksLikeTab`
tested for "contains a dash and is more than half dashes" and never tested for a
bar line, which the writer's ruler - `1---2---3---4---` - passes. So every import
invented a note per beat and shifted every real string index down by one. The
comment above the function had described the correct rule all along; the code just
did not implement it. This is the one defect here that would have corrupted user
data rather than merely failing to build.

Two of the three failing tests turned out to be wrong rather than the code:

- `eqMatchFitsKnownCurves` expected a low shelf to reach its full gain at its
  corner frequency. A shelf's corner is its half-gain point by definition - the RBJ
  form sets `A = 10^(dB/40)` and the magnitude at w0 is exactly `A` - so the fit
  returning 3.07 dB where the reference filter genuinely does +3.0 dB was accurate
  to within 0.07 dB. The expectation now names the reference response.
- `deadSpotsReduceSustainWhereTheyAre` compared a dead spot against a fret eight
  away, which can sit under a different and deeper spot; the engine takes the worst
  spot at each fret, so it was sometimes comparing two dead notes. It now measures
  against the liveliest fret clear of the spot, and skips a pair the neck is too
  crowded to measure.

**The Options pages were implemented and instantiated nowhere.** `OptionsPages.cpp`
is about eleven hundred lines defining four pages that no code constructed. They
are now five tabs inside the one Options overlay - General plus Controllers,
Expression, Accessibility and Privacy - which is what the file's own header comment
said they were for.

| | |
|---|---|
| Source | ~78 300 lines of C++ across 178 files |
| Parameters | 349, every one automatable, named and text-round-tripping |
| Guitars | 25 |
| Factory presets | 36 (17 electric, 7 acoustic, 5 bass, 5 utility, 2 classical) |
| Impulse responses | 216 body, 504 cabinet (synthesised - see `docs/KNOWN_ISSUES.md`) |
| Tests | 297 across 52 suites, 647 208 checks |

### Phase 2 and beyond

`INDEX.md` was extended after phase 1 landed. It now describes a realism phase
(twelve specs), a composition phase (`tune-builder.md`), a gap-fill phase and a
ship phase, governed by `CLAUDE_CODE_BRIEF.md`.

**Eleven of the twelve realism specs are not on disk yet**, so that phase cannot
start. `GAPS.md` lists which, and what each one blocks. The audit of the build
against `gui-integration.md` section 19 is done and lives there too.

Two pieces of phase-4 work have landed early, because it was unblocked and it was
a genuine contradiction rather than a missing feature: **Freeze and E-Bow**.
`ambiguity-resolutions.md` section 2 exists to settle whether infinite sustain is
a captured loop or a feedback drive, and answers "both, as two features". The
build had one bool called Freeze that drove the feedback mechanism. There are now
two, with a `FreezeOverlay` implementing the captured-loop half and a SUSTAIN
section holding both.

The second is the **keyboard shortcuts**. Auditing them against
`gui-integration.md` section 17 turned up three disagreeing sources of truth -
the hard-coded comparisons in `PluginEditor::keyPressed`, the rebindable registry
in `AccessibilitySettings`, and the document - and, because the editor never
consulted the registry, rebinding a shortcut changed the row in the Options table
and nothing else. "All rebindable" was decorative. The registry is now the only
place a binding is defined and the editor reads from it, so the table works. Four
actions that had no binding at all (A/B compare, Live Mode, the Practice drawer,
Options) gained one, and two that were wrong were corrected: the preset browser
was on Ctrl+P where the document says Ctrl+O, and Ctrl+Shift+R randomised instead
of resetting.

The interesting part of the freeze was the loop itself. Two read heads half a window apart under
Hann windows is the obvious construction and it fails the spec's test: constant
overlap-add makes the *windows* sum to one, but heads hundreds of milliseconds
apart read uncorrelated material, so power adds rather than amplitude and the
level follows sqrt(wA^2 + wB^2) - about 3 dB of swing every cycle. Crossfading the
seam once at capture and then reading a single head makes every cycle
bit-identical, which is what "RMS varies less than 0.5 dB over sixty seconds"
actually demands.

### Phase 5: deep integration

`INDEX.md` gained nine deep-integration specs. Two structural things have landed
from them, both chosen because everything else rests on them.

**The test target now builds `PluginProcessor`.** It was excluded, so the undo
stack, `uiState`, the A/B slots and snapshot recall - all of which `state-model.md`
specifies precisely - could be read in the source and never exercised. It compiles
into both console targets now; the renderer builds it under `LUTHIER_HEADLESS=1`,
which removes its one reference to the editor. The coupling was two guards and a
define; the old comment implied the plugin-client macros made it hard, and they
did not. This is the same blind spot that let five suites sit unlinked earlier,
and it is worth saying that the first tests written against it all pass - nothing
was broken back there, but nothing was proving it either.

**There is an error log.** `error-recovery.md` 5 asks for one and is explicit that
it is not conditional on telemetry consent, which is the right call: a user who
opted out of sending anything still deserves a local record, and support cannot
ask for a log that was never written. `ErrorLog` writes JSON lines to
`errors-<yyyymm>.log` - one object per line, so the file stays greppable and a
truncated write costs one record rather than the file. `debug` and `info` are
opt-in behind verbose; `warn` and `error` always land.

Both the error log and the preset backups date their retention sweeps from the
filename rather than the filesystem timestamp. Copying a diagnostics folder to
send it to support rewrites every timestamp, and that should not decide what gets
deleted.

One judgement call to revisit: a preset from a newer schema is loaded rather than
refused, against `error-recovery.md` 1's table, because 0.4 prefers partial
success and every parameter has a default. It logs `NEWER_SCHEMA`, because a
preset that half-loads and says nothing is the silent degradation 0.2 forbids.

### Targets

| Target | What it is |
|---|---|
| `Luthier_VST3` | the plugin |
| `Luthier_Standalone` | the same thing, hosted |
| `LuthierTests` | the test suite; exit code is the result |
| `LuthierRender` | offline renderer, for regression listening and CI |

Build with `scripts/build.ps1`, or:

```
cmake --build build --config Release
build/LuthierTests_artefacts/Release/LuthierTests.exe
```

`scripts/build.ps1` caps parallelism at two jobs by default, which is what fits in
this machine's memory; `-Jobs` raises it. Note that a `cmake --build` whose output
you redirect still needs its exit code checked - appending `echo` to the command
masks it, which is how the failing test build above went unnoticed in the first
place.

Build all four, not just the two you are working on. `LuthierRender` had not
compiled since the phase-1 extension commit: `RenderCli.cpp` carried a literal
newline inside a string constant, and the exe sitting in `build/` was two days
older than the source that could no longer produce it. It was found by building
the renderer to check that adding the UI to the test target had not disturbed it,
which is the only reason anything built it at all.

### Not done

Listed honestly in `docs/KNOWN_ISSUES.md` under "Not yet implemented": CLAP and
Linux builds, signed installers, the manual per-host test matrix, and drag-out
export.

Two things below were listed as unverified rather than missing. Both are now
verified, and the second one is verified here rather than by an external tool.

- **pluginval has been re-run.** 1.0.3 at strictness 10 against a Release build of
  `f18bf22`: twenty-five suites, exit 0, no failures and no warnings. The bus
  suites enumerate the full multi-out set and then enable all buses, disable the
  non-main ones and restore the default layout, so the extension work that made
  the old result stale is covered rather than merely present. Worth recording why
  the old claim was stale for a worse reason than its date: the Release artefact
  on disk predated `f18bf22` and was an exactly-2 MiB, non-executable file, which
  the rebuild replaced with a 9.6 MB one. Validating what was there would have
  certified code that did not contain the Ableton program-change fix.
- **The editor is run-verified by the test target.** `Source/UI/` and
  `PluginEditor.cpp` now build into `LuthierTests`, which is the one console
  target that compiles with `LUTHIER_HEADLESS=0`, and `Source/Tests/EditorTests.cpp`
  opens the window: it checks the size the processor hands back, lays the editor
  out and paints it at 940x560, 1200x720 and 1920x1080, drives every overlay
  shortcut through the registry and back out with escape, flips advanced mode,
  the practice drawer and Live Mode and flips them back, and selects all five
  Options tabs. Painting goes into an offscreen image through
  `paintEntireComponent`, so none of it needs a desktop window. pluginval's
  `Editor`, `Editor Automation` and `Open editor whilst processing` suites still
  pass, and now cover the same ground from the outside rather than being the only
  thing that covered it.

  These are smoke tests and the file says so: they know whether a panel is on
  screen and whether it drew anything, not whether it looks right. Both mutants
  tried against them died - `showPage` with the page-visibility line replaced by
  `setVisible (false)`, and `showOverlay` rewired to always open Help - which is
  the reason the assertions read state as well as pixels. The first draft compared
  renders of the whole Options panel and survived the first of those mutants,
  because selecting a tab lights that tab up whether or not the page behind it
  ever appears.

## History

See `docs/CHANGELOG.md`.
