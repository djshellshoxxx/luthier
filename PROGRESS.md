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

All forty-one milestones are done. All four targets build clean and the whole
suite passes: **310 tests, 647,923 checks**, exit code 0 - including on a machine
whose four cores are busy with something else, which was not true until the CPU
budget test stopped measuring with a stopwatch.

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
| Source | ~82 600 lines of C++ across 186 files |
| Parameters | 349, every one automatable, named and text-round-tripping |
| Guitars | 25 |
| Factory presets | 36 (17 electric, 7 acoustic, 5 bass, 5 utility, 2 classical) |
| Impulse responses | 216 body, 504 cabinet (synthesised - see `docs/KNOWN_ISSUES.md`) |
| Tests | 310 across 52 suites, 647 923 checks |

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

`INDEX.md` gained nine deep-integration specs. Three structural things have
landed from them, the first two because everything else rests on them.

**The test target now builds `PluginProcessor`.** It was excluded, so the undo
stack, `uiState`, the A/B slots and snapshot recall - all of which `state-model.md`
specifies precisely - could be read in the source and never exercised. It compiles
into both console targets now; the renderer builds it under `LUTHIER_HEADLESS=1`,
which removes its one reference to the editor. The coupling was two guards and a
define; the old comment implied the plugin-client macros made it hard, and they
did not. This is the same blind spot that let five suites sit unlinked earlier,
and it is worth saying that the first tests written against it all pass - nothing
was broken back there, but nothing was proving it either.

**The Options overlay matches `gui-integration.md` section 5.** It had five tabs
- GENERAL, CONTROLLERS, EXPRESSION, ACCESSIBILITY, PRIVACY - against a canonical
eleven, with APPEARANCE and LOCALIZATION buried inside ACCESSIBILITY, UPDATES
inside PRIVACY, and AUDIO, MIDI, DIAGNOSTICS and FILE LOCATIONS nowhere.
`GAPS.md` called it the only structural gap that was fully actionable, and it
now carries all eleven of section 5's tabs bar RANGES, in section 5's order.

Two departures are on the record rather than hidden. RANGES is absent because
`advanced-ranges.md` specifies every control on it and that file does not exist.
CONTROLLERS is present although section 5 does not list it, because section 19's
home for it is the Advanced column 4 tab strip, which does not exist either;
deleting the tab would leave a MIDI guitar unconfigurable, so it sits in the slot
RANGES will take. Both are in `GAPS.md` A3 with what moves when the blockers
clear.

The pages are where this gets interesting rather than mechanical. Section 5 asks
each tab for things that have nothing behind them yet - an accent tint, a
data-stream toggle, a changelog viewer, a mirror of feature flags for Workshop
and Slide, the Workshop's own folders. Every one of those says on the page that
it is not built, which is the ground rule about silent degradation applied to a
control that would otherwise look real and do nothing. The changelog viewer is
the interesting one: the update manifest carries links rather than notes, so the
page shows the links it actually received.

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

## A1: the Advanced Mode columns, and three things found wiring them up

`GAPS.md` A1 and A2: the panel used its own column scheme, in which AMP/CAB/ROOM
was called "column 4", with no tabbed workspace and the five extension panels
stacked at the bottom of the rig column. It now has `gui-integration.md`
section 4's four columns.

- **Columns 1 to 3 hold what section 4.1 to 4.3 say they hold**, in that order.
  The builders are `buildColumn1/2/3`, one per column, and the section blocks
  moved between them wholesale. Column 3 also puts the post-effects rack
  directly after the amp, which is 4.3's order and was not the old one.
- **Column 4 is a tab strip**, with the six panels that exist behind it - MOD,
  RHYTHM, ROUTING, TONE MATCH, CHARACTER, CONTROLLERS - in section 4.4's
  relative order, each in its own viewport. The other seven tabs 4.4 names are
  blocked or unbuilt, and a tab that opens on nothing is worse than no tab.
  CONTROLLERS arrived last and from the Options overlay rather than from
  nothing; see "CONTROLLERS goes where section 19 always put it" below.
- **Section 4.5's widths**: 260 per column with a 220 floor, a 480 floor for the
  workspace, and columns 2 and 3 stacked into one slot below 1280.
- Sections section 4 has no slot for are kept, each on the nearest column with a
  comment saying why: SELECTED STRING, NECK and SYMPATHETIC in column 1,
  PLAYING HAND and STRING NOISE in column 2, PERFORMANCE, HUMANISE, FEEDBACK
  and MASTER at the end of column 3. CABLE is still CABLE, because CIRCUIT is
  specified by `volume-knob-interaction.md`, which does not exist.

### The minimum width, and the window's first way of explaining itself

`minimumUsableWidth` was declared and nothing called it. Section 4.5 makes
Advanced Mode unavailable below 1000 points and the window's own minimum is 940,
so this was not a theoretical size - it was one drag from the default, and
crossing it laid out four columns too narrow to read.

The toggle now refuses, a window dragged below it while Advanced is on is forced
back to Easy, and the header's Easy/Advanced switch is disabled while it is too
narrow with a tooltip saying which of the two reasons it is. `InlineNotice` is
the new part: a transient strip in the layout, because the window had no way to
say anything to the user that was not an overlay, and an overlay for "your window
is too narrow" is far too much ceremony. It announces itself to a screen reader,
takes itself away after six seconds and gives the space back.

Two things fell out of enforcing it. The forcing happens inside `resized()`
rather than by calling `setAdvancedMode`, which ends in `resized()` and would
re-enter it. And the constructor now hands the header what `setAdvancedMode`
actually decided rather than what it was asked for - restoring a session that was
in Advanced Mode into a window too narrow for it used to leave the switch reading
"Easy" over an Easy panel, which is to say reading as though Advanced was one
click away.

### Three defects found on the way, none of them in the column work

**Every keyboard shortcut's description was its own key.** The rebind table in
Options > ACCESSIBILITY and `getPrintableShortcuts` both read
`tr (binding.descriptionKey)`, and not one of the twenty-five keys was in the
English catalog. `Localisation::translate` returns the key when there is no
string for it - deliberately, so a missing translation shows something - so
every row of accessibility.md 2's table read `accessibility.shortcut.undo`
where it should have read `Undo`. The table was built, ordered, searchable and
rebindable, and every label in it was a key.

The existing test asked whether each printed line `isNotEmpty`, which a key
satisfies. The new one asks for the thing the fallback cannot fake: a
description that is not the key it was looked up by. This is the same shape as
the stale test binary and the unlinked suites - the surface existed, the test
existed, and the test could not fail.

**Live Mode from the keyboard left the header showing the old state.** `L` goes
through `processor.setLiveMode` and `updateLiveStripVisibility`, which never
touched the header, so the strip appeared, the mode was on, the Live pill stayed
dark - and, because the pill's `onClick` was the only thing that locked the
Easy/Advanced switch, live-performance 10's lock did not apply. Pressing `L` and
then Tab could swap the whole window out mid-set, which is the exact thing that
lock exists to prevent. The editor tells the header now, and the two reasons the
switch can be locked go through one function instead of each overwriting the
other's `setEnabled`.

**Section 17's `Ctrl+[` and `Ctrl+]` were blocked on there being no tabs.** There
are tabs now, so they are bound. They answer only in Advanced Mode - in Easy
there is no column 4, and a key that returns true and does nothing is how a host
stops passing it on to anything else - and they wrap.

### The last-used tab, and where user-global UI settings live now

Section 4.4 asks for the last-used tab to persist in the plugin's user-global
settings, and there was nowhere to put it: `AccessibilitySettings` describes the
person, `uiState` travels inside the preset. `UiPreferences` is the third case -
a flat key/value file at `Documents/Luthier/config/ui.json`, beside the two
config files that already live there - for settings global to this user's copy
of the plugin and about the window rather than the sound. It is deliberately not
a mirror of anything: every getter takes its default at the call site, so a fresh
install with no file behaves exactly like one with a file full of defaults.

### What this is covered by

Four new editor tests and one accessibility test. The column 4 one is modelled on
`everyOptionsPageSelectsAndPaints` and for the same reason: the workspace shows
one panel at a time, so "the tab lit up" and "the panel is on screen" are two
different facts and only the second is the feature. It drives the tabs through
their buttons rather than through `setWorkspaceTab`, because a tab whose
`onClick` was never wired would pass a test that called the method.

`theWorkspaceTabWrapsAndIsRemembered` writes to the real config file, because
that is where the feature has to write, and puts back whatever was there -
including deleting the file if the run created it - so a test run does not decide
which tab the user's next session opens on.

### The one intermittent failure: a stopwatch measuring the wrong thing

`Modulation::thousandRouteStressTest` failed once, passed three full runs, and
then failed again. It measured wall-clock time across 2000 blocks and asserted
the cost was under 1% of real time.

The cause was on the machine, not in the matrix: an unrelated project was
building on it - `wubforge-cli`, holding all four cores near 100% - and a
stopwatch does not measure what this code costs. It measures what this code costs
plus everything the scheduler preferred while it ran. Under that load the same
unchanged matrix measured 1.29% to 2.34%; when the machine was quiet it measured
under 1%. Nine runs in ten failed while the other build was up, and the reason
the earlier full runs passed is simply that it was not.

Finding it took a controlled comparison rather than a guess, and the first guess
was wrong: sampling five times and taking the minimum did not help, because
contention that never lets up raises the floor along with everything else. What
fixes it is changing the instrument. `threadCpuSeconds` reads the thread's own
CPU time - `GetThreadTimes` on Windows, `CLOCK_THREAD_CPUTIME_ID` elsewhere -
which by construction excludes every interval the thread was not running.

Measured that way, on the same saturated machine: **0.62% to 0.73% of a core**,
a spread of about a tenth of a percentage point where the stopwatch spread was
over a full point. Four consecutive runs at 93% external load pass, against nine
failures in ten before.

Two things are worth keeping from this. The minimum of several samples is still
the right statistic, for a reason that survives the change of instrument: a
stolen cache or a migration between cores costs real CPU time and can only ever
add, so the fastest sample is the closest estimate, and a regression raises the
floor rather than hiding in an average. And the honest reading of 0.65% is that
it is under the spec's 1% without being the "well below" the spec claims - about
a third of the budget is headroom. The bar stays at the spec's number rather than
being tightened onto today's measurement, because a bar set just above the
current figure fails on the next machine instead of on the next regression.

One process note, since this file already makes the same point about build exit
codes. The first failure was piped through `Select-Object -Last 45`, which kept
the summary and dropped the `[FAIL]` line naming the test. That cost two hours of
running the suite blind. Every run since has been captured whole and grepped
afterwards.

### CONTROLLERS goes where section 19 always put it

This was left open above as a choice between two small jobs: share the
`ControllerProfileLibrary` so two pages can hold one, or move the page and let
Options fall to ten tabs. Writing it down that way was the mistake. Only one of
them is a real option.

`ControllersPage` owns its library by value, so the staleness problem - two
instances scanning the Controllers folder separately, each going stale the moment
the other saves a profile - is a problem that **only exists if there are two
pages**. Section 19 asks for one, in column 4. Moving it leaves a single instance
and nothing to design around; sharing the library would have been work done to
support a duplicate nobody asked for. The second option was never a trade-off, it
was just the more expensive way to arrive somewhere worse.

So the page moved. It is constructed in `buildWorkspace` and owned by
`AdvancedPanel`, held by pointer behind a forward declaration so the panel's
header does not pull in every other Options page. It still derives from
`OptionsPage`, which turns out to be a `Component` that holds the processor and
can be told to `refresh()` - nothing about it was ever specific to the overlay,
and the name now says where the page came from rather than where it lives.

`showWorkspaceTab` calls that `refresh()` when CONTROLLERS is the tab being
opened, which is what `OptionsPanel` did for it: a controller can be unplugged
while the tab is not looking. It is a named case rather than a virtual on every
panel because the other five already track the processor on a timer.

Nothing about the cost changed. `AdvancedPanel` and `OptionsPanel` are both
members of the editor by value, so the folder is scanned exactly once per editor,
exactly as before.

Options is ten tabs now, which is not a hole - it is section 5's list minus
RANGES, with nothing in it section 5 does not name. The tab strip's row
arithmetic was already computed from the buttons that exist rather than from the
spec's count, so RANGES arriving will not need it rewritten; only the comment
claiming "eleven" did.

**Covered by** the two tests that pin the lists, one at each end: the workspace
list in `Editor::everyWorkspaceTabSelectsAndPaints` gained CONTROLLERS (56 checks
to 70), and the Options list in `Editor::everyOptionsPageSelectsAndPaints` lost
it (133 to 116). The second is what stops the duplicate coming back: a
CONTROLLERS page added to the overlay while column 4 still has one fails that
test's page count.

### Still open here

- **LIVE, PRACTICE, NOTATION and MIDI OUT** are the four tabs that are unbuilt
  rather than blocked. Each has a working runtime half or a working engine and no
  setup page in front of it.

## A4: the Easy-mode instrument, and a preset field nobody could author

`GAPS.md` A4's first row said `GuitarBodyComponent` "draws the instrument but is
not hit-tested as a control surface". That was wrong, and reading the code rather
than the gap list is what showed it: the volume knob, the tone knob, the selector
switch and the pickups have all been live since the component was written, and its
own header comment says so. Two of section 3.1's four regions were missing, not
four. The right description was "half hit-tested".

The two that were missing are built now.

**The headstock opens a tuning popover.** Tuning preset, temperament, Concert A,
and a detune slider per string.

**The bridge opens a whammy popover**, and only when a bridge with an arm is
fitted - index 0 of the bridge list is the hardtail. That condition is section
3.1's own. What the section does not say, and what matters more, is that the
affordance is not conditional even though the popover is: on a hardtail the
bridge still describes itself on hover and says there is nothing to set, because
a region that goes silent for a reason the user cannot see is indistinguishable
from one that is broken.

### The part that was not a UI gap

The headstock popover is the only way in the plugin to author per-string detune.

`TuningEngine::StringTuning::detuneCents` is written into the preset by
`PresetManager` and read back out of it. It has always been saved and restored.
No control anywhere could set it. The Advanced column has the tuning preset, the
temperament and Concert A; it has never had the per-string offsets, and the only
things that wrote them were the guitar loader and preset recall.

A preset field with no way to author it is the same shape of gap as a panel
nobody constructs - it looks complete from every direction except the one that
matters - and it had been sitting there through all forty-one milestones.

**The six sliders are not automatable**, and that is a real limitation rather
than an oversight. There is no per-string tuning parameter to attach them to, so
they write the engine directly and cost MIDI Learn and host automation on those
controls. Closing it means adding per-string parameters, which is a parameter
count change and a preset schema question rather than a UI one. It is in
`GAPS.md` A4 rather than quietly absent.

### Capo: documented, promised, and not built

Section 3.1 asks the headstock for "per-string tuning, capo, temperament". There
is no capo in this plugin - not a parameter, not a field in `TuningEngine`, not a
line of code. Four specs describe it and `factory-content.md` ships three capo
parts.

The specs describing an unbuilt feature is the normal state here and `INDEX.md`
tracks it. What is not normal is that **`docs/USER_MANUAL.md` and
`docs/KEYBOARD_SHORTCUTS.md` both tell the user the right-click menu has a capo
in it.** A user following the manual finds a menu without one and no explanation.
That is a promise the build does not keep, and it is `GAPS.md` B1 now.

The popover says on its face that capo is not built, which is ground rule 0.2
applied to an absence rather than to a degradation. The two docs are left alone
deliberately: correcting them is a decision about whether they describe the build
or the plan, and that is not a gap to close silently on someone's behalf.

### The test, and the two mutants that killed its first draft

`everyHitRegionOnTheIllustrationDescribesItself` sweeps a grid over the whole
component and collects what the tooltip says at every point, which is
`guitar-illustration.md`'s own idea of how to test hit regions - it asks for ten
thousand random clicks across each factory guitar. Sweeping rather than probing
two coordinates is the point: the geometry is generated from the `GuitarSpec`, so
there are no fixed coordinates to probe, and a region that shrank to nothing
would still pass a test that asked it directly.

The first draft opened both popovers and checked they did not throw. Two checks,
and it would have passed with both hit regions deleted - the same failure the
Options test had in its first draft, arrived at the same way. The version that
landed makes twelve checks and dies to both mutants tried against it: the
headstock branch disabled, which fails with "no point on the illustration offers
the headstock tuning popover", and the hardtail case rewired to claim an arm,
which fails with "on a hardtail the bridge region does not say why it does
nothing".

`theHeadstockPopoverEditsPerStringTuning` checks the sliders reach the engine,
that each writes only its own string, and that a detune actually moves the pitch
- without that last one it would pass on a popover wired to a field the engine
never reads, which is exactly what the feature's absence looked like.


## A4 again: a row that was wrong, and the reason it stayed wrong

`GAPS.md` A4 said "the Modulate entry and drag-to-assign do not [exist]". Half of
that was false. `showParameterContextMenu` has offered every modulation source
since `996f89d` - three milestones back - grouped LFO, Envelope, Sequencer,
Follower, Macro and Performance, with the destination taken from the control
under the cursor, a new route built at a third of full depth, a "Remove
modulation (n)" entry once routes exist, and a full destination saying so in the
submenu title rather than offering sources it would silently drop.

This is the second A4 row closed by reading the code instead of the list, after
the Easy-mode instrument. Both were written in one audit pass against a build the
audit did not run, and both described something absent that was in fact present.
That pattern is now recorded at the top of A4's suggested-order entry, because
the useful conclusion is not "fix two rows" but **this file is the least
trustworthy document in the repository about what exists** - a row in it is a
question to check, not a fact to act on.

### What actually let it stay wrong

Not the audit. **Nothing tested the menu.** Nothing else in the plugin goes
through it - it is a secondary path by design, with the MOD tab cards primary per
ground rule 4 - so every one of its behaviours could have broken in any release
and no test anywhere would have said a word. The audit being wrong was noticed in
ten minutes by grepping; the absence of coverage is what made the row survive
three milestones of people reading it.

### Splitting the menu so it could be tested at all

`showParameterContextMenu` built its items and called `showMenuAsync` in one
breath. A function shaped like that can only be checked by a human opening the
menu and looking at it, which is precisely how it went unchecked.

It is three functions now. `buildParameterContextMenu` returns the items without
showing them, `applyParameterMenuResult` performs a result id - which is exactly
what the old callback body did - and `showParameterContextMenu` is those two
either side of `showMenuAsync`, which is what every control still calls. The look
and feel moved to the show step, since it belongs to the control rather than to
the items, and an empty menu now returns early rather than putting an empty box
under the cursor.

`kModulateMenuBase` moved from an anonymous namespace in the .cpp to the header,
because a test driving a result has to know where the encoded source range
begins.

### The test, and the two mutants

`Editor::rightClickOffersModulationAndBuildsTheRoute` walks the real menu with
`juce::PopupMenu::MenuItemIterator` and drives the real handler. Twenty checks:
the Modulate entry exists and has sources under it, all six source groups are
present *by name*, LFO 1 and the mod wheel carry ids in the encoded range,
"Remove modulation" is absent until there is something to remove, choosing a
source builds one route from the right source at depth 0.33 and enabled, the
menu then offers to remove it and says how many, removing leaves none, and a
destination filled to `kMaxRoutesPerDestination` says "already routed" and takes
no more.

The groups are checked by name rather than by counting items on purpose: a count
passes if one group is dropped while another grows.

Two mutants, both killed:

- **`route.depth = 0.0f`** instead of `0.33f` fails with `built.depth = 0.000000,
  expected 0.330000`. This is the one worth having. A route at zero depth looks
  entirely correct in the matrix - right source, right destination, enabled - and
  does nothing whatsoever to the control. A test that only asserted "a route
  appeared" would pass on it.
- **The Modulate submenu never added to the menu** fails eleven of the twenty
  checks, naming each missing group and both encoded ids.

### Still open in A4

- **Drag-to-assign** does not exist - there is no `DragAndDropContainer` in
  `Source/UI/` outside `ToneMatchPanel`'s file drop. Ground rule 4 is satisfied
  without it, since the MOD cards are primary and the menu is a real second
  route, so this is a convenience rather than a missing path.
- **The three notification routes**: update available in the header, the
  post-crash prompt, and Help > About as a route to the licence.


## A6: three missing routes that were one missing mechanism

`GAPS.md` A4 ended with three rows that read like three small jobs: a header
notification for an available update, a post-crash prompt, and Help > About as a
route to the licence. Taken one at a time they would have been three ad-hoc bits
of UI in three different places.

They are not three jobs. `gui-integration.md` section 15 specifies a notification
system - "non-modal banners under the header strip, 32 px, dismissible", nine
triggers, "auto-dismiss after 5 s unless they contain an action" - and all three
of A4's rows are triggers of it. **Section 15 was not built at all**, and A4
listed three of its nine triggers without ever noting that they had nowhere to
appear.

That is a different failure from the two stale A4 rows above. Those described
something absent that was present. This one described the symptoms of an absence
correctly and missed the absence itself.

### What the spec's two rules are actually for

**Dismissible, always.** A banner that cannot be got rid of is a modal dialog
wearing a different hat, and the triggers include one - managed by policy - that
a user may not be able to do anything about at all.

**Auto-dismiss after five seconds unless there is an action.** This is the rule
with teeth, and it is implemented by never starting the timer for an actionable
banner rather than by starting one and cancelling it. The difference matters: a
banner offering to review a crash report that evaporates while the user is
reaching for the button is worse than no banner, because they now know something
happened and have no way back to it.

Two things the spec does not say, decided here:

- **Banners queue rather than stack.** Two at 32 px is 64 px of window, and the
  startup triggers arrive together - a crash dump and a policy file are both
  found in the same second. One shows, the rest wait, and the strip paints "+2"
  so a burst does not look like a single notification that keeps regenerating.
- **A banner has an id, not just text.** The countdown triggers repost every time
  the window opens, and identity-by-text would either stack them or fail to
  update the number. Posting an id that is already up rewrites it in place
  without restarting its clock.

### Why this did not fold into `InlineNotice`

The window already had a dismissible auto-expiring notice, built for Advanced
Mode refusing to open below 1000 points. Merging the two was the obvious move and
is wrong. An `InlineNotice` answers something the user just did, at the place
they did it - the reason the panel will not open belongs in the panel that will
not open. A banner interrupts with something unrelated to whatever is being done
at the time. Folding them together means either the mode refusal floating at the
top of the window away from the control that caused it, or the crash report
appearing at the bottom of a panel it has nothing to do with. They share a paint
style so they read as one family, and nothing else.

### The four triggers that could be wired, and the one that touches the network

Crash on last session, licence grace countdown, managed by policy, and update
available - all checked once, at window open. The crash banner is posted last so
it shows first, because the queue is first-in-first-out and it is the only one of
the four with something to do about it.

The update check is the only one that leaves the machine, and it runs **only when
the user has opted in**. `isUpdateCheckEnabled()` is false by default and a policy
can switch it off but never on; opening a window is not a reason to make a request
someone declined. It is not throttled at the call site because `checkForUpdate`
is already throttled to 24 hours, so twenty window opens is still one request. It
uses the same launch-then-`callAsync` shape as `UpdatesPage::checkForUpdate`
deliberately - a second threading idiom for the same job in the same file set
would be a worse thing to maintain than the one already there.

The crash banner's button says **Review**, not Send, and opens the Privacy page.
`updates-telemetry.md` 4 asks for a viewer showing exactly what would be
uploaded. A crash dump is the most sensitive thing this plugin ever offers to
transmit, and one button that sent it would be the opt-in equivalent of a dark
pattern.

Banners open Options by tab *name* through a new `OptionsPanel::showPageNamed`,
not by index. Section 5 fixes the order, but RANGES arriving would shift every
index after it, and a banner that quietly opened the wrong page would be worse
than one that did nothing.

### The five that are unreachable rather than skipped

Four of them - preset load error, missing IR, missing guitar, missing part - want
the loader to say what it substituted, and it currently falls back in silence.
That is one change to the loader rather than five to the banner. The fifth,
sample-rate change, is only awkward: `prepareToPlay` knows, but it runs on the
audio thread and the editor may not exist when it does, so the message needs
somewhere to wait. `GAPS.md` A6 has all five, and neither is blocked on a spec
that has not been written.

### The tests, and the two mutants

`notificationBannersQueueDismissAndRespectTheirActions` drives the queue: posting,
reposting the same id, queueing a second, dismissing through to the next, running
an action, and `clear()`. `isAutoDismissScheduled()` exists so the five-second
rule is checkable without a five-second sleep - without it, the one rule most
worth having is the one rule no test can see.

`theWindowRaisesSectionFifteensTriggersAndIsQuietWhenItShould` checks both
directions. A healthy plugin must open **silently** - a window that shows a banner
every time has trained the user to dismiss without reading - and that half is
guarded rather than assumed, because a machine that really does have a policy file
would otherwise fail a test that was not testing anything. The licence half
arranges grace through `License::fromVar`, which takes the dates the state is
derived from, so the licence is in grace by the plugin's own arithmetic rather
than by a flag set to make the test pass.

Both mutants died:

- **Starting the auto-dismiss timer unconditionally** fails with "an actionable
  banner is on the auto-dismiss clock, so its button disappears out from under
  the user".
- **Dropping the strip from the editor's layout** fails twice - no height, and
  not under the header - which is the failure worth catching, because a banner
  that is posted correctly and given no bounds is invisible and everything else
  about it still passes.


### Two more triggers, and a claim of mine that was wrong one commit later

The A6 entry above said the remaining five triggers were "one change to the
preset loader, which currently substitutes a fallback without saying so". I wrote
that without checking, which is the exact failure this file has been documenting
in `GAPS.md` for three sections running. It was wrong about all four of the
triggers it named.

- **Preset load error** needed no loader change. `loadPreset` already returned
  `false` and already wrote an error-log line. What was missing was a message a
  *user* could read, because most callers discard the bool - the header's Open
  dialog worst of all, where a preset that would not load did nothing at all,
  silently. That is a ground rule 0.2 violation that had been sitting in the
  header menu since it was written.
- **Missing IR** needed no loader change either, and the code it reads had been
  waiting for this banner. `IrSlot::fromVar` falls back to the built-in model when
  a preset names an IR that is gone, and leaves `lastError` set with a comment
  saying it is there "so the header can show the banner the spec asks for". The
  banner did not exist when that comment was written.
- **Missing guitar** cannot happen. Section 15 describes a missing
  `.luthierguitar` referenced by a preset; there is no such file format.
  `GuitarLibrary` is a compiled-in enum, a preset stores an index, and the
  parameter clamps it. The trigger presupposes user guitar files, which is a
  Workshop feature nobody has built.
- **Missing part** is blocked on `guitar-workshop.md`, like WORKSHOP itself.

So: two wired, two that were never reachable. `PresetManager::getLastLoadError`
is the only new surface, and it carries a sentence naming the file rather than a
code - "could not load preset" about one of several hundred is not useful.

### Polled, not pushed

Both new triggers are conditions the window reads on its existing 4 Hz timer
rather than events the loader fires at it. Presets load from five places - the
header, the browser, the Easy panel's style list, a host program change, and the
processor's own state restore - and five call sites each remembering to report
would be five chances to forget. One reader cannot forget.

The cost of polling is the thing that had to be got right. Posting on every tick
would be harmless to the queue, because a repeated id replaces itself rather than
stacking - and it would make the banner **impossible to dismiss**: the cross
works, and a quarter of a second later it is back. So the window remembers the
last message it raised for each trigger and posts only when it changes. That is
what the test's mutant checks, and it is the only mutant of the three tried here
that a reasonable implementation would actually get wrong.

Three IR slots share one banner. A preset naming three missing IRs has one thing
wrong with it - the folder moved - and three banners in turn would be three
dismissals for one problem. The first slot with something to say speaks for all
of them, and the error log has the detail.

The IR banner offers a button to Advanced Mode's TONE MATCH tab, through a new
`AdvancedPanel::setWorkspaceTabNamed` - by name, for the same reason
`OptionsPanel::showPageNamed` exists. Section 4.4 fixes the tab order, but the
seven unbuilt tabs arriving would shift every index, and a banner that quietly
opened the wrong panel is worse than one that did nothing. It returns false for a
tab that is not built, so a caller can tell the difference, and the test checks
that with NOTATION.

### The sample-rate notice, and a first draft the test caught

The last section 15 trigger nothing blocked. It is the one where the interesting
work is deciding when *not* to speak.

`prepareToPlay` is called whenever the host feels like it. Changing the buffer
size calls it. Some hosts call it when transport starts. So a banner per
`prepareToPlay` would appear every time a user touched their audio settings, which
is the fastest way to teach someone to dismiss banners without reading them. And
the first prepare of all is not a change at all: opening a plugin at 48 kHz is the
normal state of affairs, not news.

So the rate is **claimed** rather than compared, and claiming clears it. That
makes this an event rather than a condition, which is why it does not use the
"post only when the message changes" rule the preset and IR banners beside it use
- asking twice about one event must not answer twice. It also means two windows
cannot both announce one change, and a closed window does not lose it: the next
one to open picks it up, which is right, because the re-resampling happened
whether anyone was watching or not.

`prepareToPlay` gained one line - a relaxed store of the rate - and nothing else.
The deciding is all on the message thread.

**The first draft read `AudioProcessor::getSampleRate()` and the test failed.**
That accessor is set by `setRateAndBufferSizeDetails`, which is a host's job;
calling `prepareToPlay` directly leaves it stale, so the 48-to-96 change was never
seen. In a real host it would have worked, which is exactly what makes it the kind
of dependency worth removing: it was correct by accident, on something that is not
`prepareToPlay`'s contract to provide. The processor now keeps its own
`preparedSampleRate`, and the feature no longer depends on being driven by a host
to work.

The banner carries section 15's whole sentence, including the half that is easy to
drop: "IRs and circuit filters re-resampled". Knowing a number changed does not
explain the gap in the audio the user just heard. Knowing the convolution kernels
were rebuilt does.

**Covered by** `aSampleRateChangeIsAnnouncedOnceAndTheFirstOneIsNot`, which spends
most of its checks on silence - unprepared, first prepare, same rate with a new
block size - and two on the announcement. Its mutant records only the first rate
ever seen, so a change reports on every poll forever; it dies on "the same rate
change was reported twice, so two windows would both announce it". An earlier
mutant that removed the store entirely also failed, but by never reporting at all,
which is the less interesting half of the rule and was not worth keeping.

With this, **seven of section 15's nine triggers are wired**. Of the two that are
not, one is blocked on `guitar-workshop.md` and one on `advanced-ranges.md`, and a
third listed trigger - missing guitar - cannot happen in this build at all, since
there is no `.luthierguitar` file format for a preset to reference.


## B1: three capos, none of which changed a note

`GAPS.md` B1 opened with "There is no capo. Not a parameter, not a field in
`TuningEngine`, not a line of code anywhere in `Source/`", and concluded that
`docs/USER_MANUAL.md` and `docs/KEYBOARD_SHORTCUTS.md` made "a promise the build
does not keep". Almost none of that was true.

There were **three** capos:

- **`RhythmEngine::capoFret`** moved `ChordVoicer::setMinFret`, so chords were
  voiced above it. Persisted in the rhythm state. Reachable from the Rhythm
  panel's up/down buttons.
- **`FretboardComponent::capoFret`** drew a capo and shifted that component's own
  fret display. Reachable from its right-click menu - **"Set capo here"** - and
  read by nothing else in the plugin.
- **`TuningEngine`** had none, which is the one place `gui-integration.md` 19
  says the capo belongs.

And the manual was not lying. The menu it describes - "Mute string, select
string, set capo, scale overlay" - is `FretboardComponent`'s menu exactly, every
item present. **Neither document needed a word changed.**

The real gap was narrower and worse than the entry described: *no capo anywhere
changed the pitch of a note*. A user following the manual got a capo that drew a
line on a picture. A user pressing the Rhythm panel's capo buttons got chords
voiced higher up a neck whose notes sounded exactly as before. Two half-features
that each looked like the feature from one angle.

This is the fourth entry in that file closed by reading the code rather than the
entry, and the worst of the four, because it was the most confidently worded.

### One capo, in the place the spec names

`ParamIDs::capoFret`, a choice from Off to Fret 12, owned by `TuningEngine`. A
parameter rather than engine state because a capo is something a player moves
between songs and automates between sections, and because section 19 puts it in
Advanced column 1, which is a column of parameters. The count moves 351 to 352,
which the pinned assertion in `everyParameterHasAUniqueIdAndSaneDefault` now
carries.

`ambiguity-resolutions.md` 4.5 is three sentences and two are implemented
exactly. Fret positions are measured **from the capo**: fret 0 is the capo,
`getEffectiveOpenFrequency` returns the capo'd note so nothing downstream needs
to know a capo exists, and `getHighestPlayableFret` is `maxFrets - capoFret`, so
the neck genuinely gets shorter.

### The subtlety that would have shipped

The capo is applied as a **fret position**, not as a cent offset on the open
string. Under an unequal temperament those are different: the frets are at fixed
places, so a capo at 5 gives exactly what fret 5 gives, not the open string
shifted by a tempered fourth. Under equal temperament - the default, and what
anyone would test with - they are identical. That is exactly the shape of thing
that gets written the easy way and never fails until someone loads Werckmeister.

`frequencyToFretPosition` had to move with it: it solves in absolute frets, in
the same coordinate `computeFrequency` works in, and subtracts the capo at the
end. Inverting in any other coordinate would not round-trip once the temperament
is unequal.

### Unifying the three

`RhythmEngine::setCapoFret` delegates to `TuningEngine` and its own field is
gone. `ChordVoicer::minFret` goes back to meaning "a floor on where to voice"
rather than doubling as the capo - with capo-relative frets, filtering below it a
second time would subtract the capo twice. `FretboardComponent`'s right-click
drives the parameter and reads it back on its timer, so the drawn capo cannot
disagree with the sound.

That unification exposed a real defect: `ChordVoicer` bounded candidates only by
its own `maxFret` preference, default 22. With a capo at 12 on a 24-fret neck it
would have voiced up to fret 22 from the capo - ten frets past where the neck
ends. It is bounded by the playable span now as well.

`RhythmEngine` stops writing `capoFret` into its own state, since the parameter
carries it in the same preset and two copies could disagree. `fromVar` still
reads an old one so a session saved by a previous build does not lose it, and
does not write it back.

### All three UI homes exist

Advanced column 1 GUITAR (section 19), the headstock popover (section 3.1) and
the fretboard right-click. The popover's footer used to read "Capo is not built
yet"; it now names the part that genuinely is not - partial capos, which take
their string mask from the Workshop's capo part, and the Workshop is blocked on
`guitar-workshop.md`.

### The test that passed on a capo which transposed nothing

`capoRemovesFretsBelowIt` asserted `fretPosition >= capo` and nothing else. It
never looked at pitch, so **a capo that did nothing to the sound passed it** -
and for three milestones one did. That is the clearest example this build has
produced of a test that checks the mechanism it happens to have rather than the
behaviour anyone wanted.

Its replacement, `capoRemovesFretsBelowItAndMovesThePitch`, keeps the parts that
still mean something in capo-relative coordinates - nothing below the capo,
nothing past the shortened neck, and a bar that actually produced notes so the
loop is not vacuous - and adds the one that matters: an open string with a capo
at 5 must sound *exactly* what fret 5 sounded without one, not merely higher,
which a wrong-but-plausible implementation would also manage.


## A5's last two buildable rows, and two more reasons that were not true

`GAPS.md` A5 listed four open shortcut rows. Two were blocked on specs that do
not exist and still are. The other two came with reasons, and neither reason
survived being checked - which makes **six** entries in that file now closed by
reading the code instead of the entry.

**"New preset, `Ctrl+N`: no new preset action exists. Needs an init-preset
concept first."** There is an **Init** preset in the factory set, filed under
Utility, and its own description calls it the place to start when building your
own. The concept the entry asked for had shipped with the plugin.

`Ctrl+N` loads it. Three small decisions around that:

- It goes through a new `PresetManager::indexOfPreset`, which prefers a factory
  preset over a user one of the same name. Without that, saving a user preset
  called "Init" would quietly redefine what `Ctrl+N` does.
- It pushes an undo state first. Losing an unsaved sound to a mistyped `Ctrl+N`
  is the worst thing any shortcut in this window could do.
- It is a **load**, not a reset. `resetEverything()` would leave the session at
  defaults with no preset behind it; this leaves the preset named Init, with a
  file, which is what makes the next row work.

**"Reveal preset file, `Ctrl+Alt+E`: `PresetManager` tracks the current preset's
name and index but not its file path, so there is nothing to reveal."** Half
true, and the wrong half was the one that mattered. `PresetInfo` carries a
`juce::File`, so a library preset's path was always reachable through the index.
What the entry missed is that `loadPreset (File)` - the overload the header's
Open dialog calls - never sets an index at all, so a preset opened from anywhere
outside the library had nothing to map back from. Recording the file on load
covers both cases in one member.

With nothing loaded yet, the shortcut posts a banner saying so rather than
opening some arbitrary folder. That is the same ground rule 0.2 reasoning as the
rest of section 15's work, applied to a key that would otherwise appear broken.

### A test that must not open File Explorer

`revealToUser` puts a file manager window on screen. A test that pressed the
successful branch would spawn Explorer on every run of the suite, on every
machine, forever - which would be a worse thing to have done than leaving the row
open.

So `newPresetLoadsInitAndRevealSaysSoWhenThereIsNoFile` presses the key **only**
in the state where there is nothing to reveal, which is the branch with the
interesting behaviour anyway, and checks the file the other branch would use
directly rather than by triggering it. The ordering is load-bearing and says so
in the test: reveal is pressed before `Ctrl+N`, because `Ctrl+N` creates the very
file that would make pressing it again open a window.

The test loads a different preset before pressing `Ctrl+N`, so that "it loaded
Init" is not indistinguishable from "it did nothing". The mutant - making the
lookup return -1 - reports the preset is still "DADGAD Drone".

## History

See `docs/CHANGELOG.md`.
