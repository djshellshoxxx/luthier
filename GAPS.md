# GAP LIST — current build vs the spec set

Produced per `CLAUDE_CODE_BRIEF.md` step 2: audit the build against
`gui-integration.md` section 19 (the feature-to-location index), record every
row whose UI location does not exist, fix nothing yet.

Audited at commit `88b0796`, against the `gui-integration.md`, `INDEX.md` and
`CLAUDE_CODE_BRIEF.md` present on 2026-09-18. **Those three files changed twice
during the session that produced this audit** — the Options tab list went from
ten tabs to eleven and the Column 4 tab list from ten to thirteen — so check the
spec dates before trusting any row here.

## B0 — Eleven specs referenced by `INDEX.md` are not on disk

This is the blocking finding and it comes before everything else.

`INDEX.md` Phase 2 lists twelve realism specs. Eleven of them do not exist:

| Spec | On disk |
|---|---|
| `advanced-ranges.md` | **missing** |
| `volume-knob-interaction.md` | **missing** |
| `pick-noise.md` | **missing** |
| `string-squeak.md` | **missing** |
| `fret-buzz.md` | **missing** |
| `slide-guitar.md` | **missing** |
| `guitar-workshop.md` | **missing** |
| `part-acoustics.md` | **missing** |
| `workshop-ui.md` | **missing** |
| `strum-dynamics.md` | **missing** |
| `bass-techniques.md` | **missing** |
| `midi-export.md` | present |

`tune-builder.md` (Phase 3) and all seven Phase 4 gap-fill specs are present.

Phase 2 cannot be implemented until these are written. `CLAUDE_CODE_BRIEF.md`
is explicit that `advanced-ranges.md` must land first because every physical
parameter depends on its `PhysicalRange` wrapper, and that nothing may be
improvised where a spec is meant to be explicit. So the correct action is to
wait for the files, not to guess at their contents.

## What this blocks

Section 19 rows whose backend module does not exist and cannot be built yet:

- **`GuitarCircuit`** — guitar volume / tone, pot values, tone cap, treble
  bleed, cable, active/passive. Replaces the existing `CableSim`, which
  `CLAUDE_CODE_BRIEF.md` says is removed rather than deprecated. Blocks Adv
  Col 2 **CIRCUIT**, which replaces the built CABLE panel.
- **`Workshop`** (`guitar-workshop.md`, `part-acoustics.md`, `workshop-ui.md`)
  — the parts model and the bench. Blocks the Col 4 **WORKSHOP** tab, Save As
  Guitar, and the primary location for body, strings, pickup model, pickup
  position and pickup height, all of which section 19 moves out of the Advanced
  columns and into the Workshop.
- **`SlideEngine`** (`slide-guitar.md`) — blocks Slide mode, section 7.
- **`NoiseEngine`** (`pick-noise.md`, `string-squeak.md`, `fret-buzz.md`) —
  blocks the noise-event strip, the Aux 8 noise bus row, and the CHARACTER tab's
  PICK group.
- **`StrumDynamics`** (`strum-dynamics.md`) — blocks the RHYTHM tab STRUM group.
- **Advanced ranges** (`advanced-ranges.md`) — blocks Options **RANGES**, the
  header range-lock padlock, and the warning-colour marking in ground rule 9.
- **Bass techniques** — blocks the SLAP group and the bass step grid.

`tune-builder.md` and `midi-export.md` are present and unblocked, but both are
large and both sit behind the realism phase in the build order.

## A1 — Advanced Mode column scheme — **done**

**Canonical.** Column 1 GUITAR/BODY/STRINGS/WHAMMY, Column 2 PICKUPS/CIRCUIT/
PRE-FX, Column 3 AMP/POST-FX/CAB/ROOM/SUSTAIN, Column 4 a tabbed workspace.

**Built.** That, in that order. `AdvancedPanel` has `buildColumn1/2/3` and
`buildWorkspace`, one per column of section 4, and the section blocks moved
between them wholesale. Column 3 also puts the post-effects rack directly after
the amp, which is 4.3's order and was not the old one. SUSTAIN's content was
already right; only its column number was wrong, and it moved with the rest.

Section 4.5's widths are built too: 260 points per column with a 220 floor, a
480 floor for the workspace, and columns 2 and 3 stacked into one slot below
1280 rather than one of them being hidden.

**Two departures, both deliberate.**

- **CIRCUIT is still CABLE.** `volume-knob-interaction.md` specifies the circuit
  panel and that file does not exist. See "What this blocks".
- **Sections section 4 has no slot for are kept**, each on the nearest column
  with a comment in the source saying why: SELECTED STRING, NECK and SYMPATHETIC
  on column 1; PLAYING HAND and STRING NOISE on column 2; PERFORMANCE, HUMANISE,
  FEEDBACK and MASTER at the end of column 3. Dropping a built control to match
  a column list would have removed working features to satisfy a layout.

**Section 4.5's minimum width is enforced.** Advanced Mode is unavailable below
1000 points. The window's own minimum is 940, so this is one drag from the
default rather than a theoretical size: the mode toggle refuses and says why,
a window dragged below it while Advanced is on is forced back to Easy with the
same notice, and the header's Easy/Advanced switch is disabled while it is too
narrow with a tooltip giving the reason. `InlineNotice` is the surface, and it
is new - there was no way for the window to explain itself before.

**Covered by.** `Editor::advancedModeIsRefusedBelowItsMinimumWidth` drives both
routes into the mode at both sizes and checks the notice is on screen and has
painted, and `Editor::itLaysOutAndPaintsAcrossItsResizeRange` still paints the
window at 940, 1200 and 1920.

## A2 — Column 4's tab strip — built, with seven tabs still absent

**Canonical order.**
`WORKSHOP | MOD | RHYTHM | TUNE | LIVE | ROUTING | TONE MATCH | CHARACTER | PRACTICE | NOTATION | MIDI OUT | CONTROLLERS | HELP`

**Built.** A tab strip across the top of column 4 with the six panels that exist
behind it - `MOD | RHYTHM | ROUTING | TONE MATCH | CHARACTER | CONTROLLERS` - in
section 4.4's relative order, each in its own viewport, one on screen at a time.
The other seven are listed below. A tab that opens on nothing is worse than no
tab, so they are absent rather than present and empty.

| Tab | State |
|---|---|
| WORKSHOP | blocked on `guitar-workshop.md` / `workshop-ui.md` |
| MOD | **built** — `ModMatrixPanel` |
| RHYTHM | **built** — `RhythmPanel`; STRUM group still blocked on `strum-dynamics.md` |
| TUNE | not built (`tune-builder.md` is on disk, so unblocked but large) |
| LIVE | no setup surface; `LiveStrip` is the runtime surface only |
| ROUTING | **built** — `RoutingPanel` |
| TONE MATCH | **built** — `ToneMatchPanel` |
| CHARACTER | **built** — `CharacterPanel` |
| PRACTICE | no setup surface; the drawer is the runtime surface only |
| NOTATION | not built — no live TAB view or chord-symbol history surface |
| CONTROLLERS | **built** — `ControllersPage`, moved here from Options |
| MIDI OUT | not built |
| HELP | exists as an overlay, reachable on F1, not as a tab here |

**The last-used tab persists** (4.4), in `Documents/Luthier/config/ui.json`
through the new `UiPreferences`. That store is the thing that was missing when
this was written down as needing a decision before it needed code:
`AccessibilitySettings` describes the person and `uiState` travels with the
preset, and a workspace tab is neither. `UiPreferences` is the third case -
settings global to this user's copy of the plugin and about the window - and it
is deliberately not a mirror of anything, so losing the file costs a preference
and nothing else.

**CONTROLLERS has moved**, and section 19 has its home back. The question this
entry used to pose - share the library, or move the page - was answered by
noticing that only one of them was ever a real choice. `ControllersPage` owns a
`ControllerProfileLibrary` by value, so *two* pages would scan the Controllers
folder separately and go stale against each other the moment either saved a
profile. Moving the page leaves one instance and no staleness to design around,
and it is what section 19 asked for in the first place. Sharing the library would
have been work done to support a duplicate nobody wanted.

The page is unchanged apart from where it is constructed: it still derives from
`OptionsPage`, which is a `Component` holding the processor with a `refresh()`
hook and nothing to do with the overlay. `AdvancedPanel::showWorkspaceTab` calls
that `refresh()` on the way in, which is what `OptionsPanel` did for it, because
a controller can be unplugged while the tab is not looking. Both panels are
constructed once per editor either way, so the folder is scanned exactly as often
as before.

Options is ten tabs now - section 5's eleven minus RANGES, in section 5's order -
which is A3's remaining departure and nothing else.

**Covered by.** `Editor::everyWorkspaceTabSelectsAndPaints` walks the six tabs by
name and checks each one puts its own panel - and only its own panel - on screen,
painted, and different from every other. `Editor::theWorkspaceTabWraps‑
AndIsRemembered` covers the stepping, the clamping and the round trip through
the config file. The tab list in the first of those is a copy of the list above,
so a tab that disappears or is renamed fails there. CONTROLLERS appearing in
*both* places fails `Editor::everyOptionsPageSelectsAndPaints` on its page count,
so the duplicate this entry warned about cannot come back unnoticed.

## A3 — Options overlay tabs: fixed, with one departure on the record

**Canonical.** Eleven tabs:
`AUDIO | MIDI | APPEARANCE | ACCESSIBILITY | LOCALIZATION | EXPRESSION | RANGES | UPDATES | PRIVACY | DIAGNOSTICS | FILE LOCATIONS`

**Built.** Ten tabs, in that order:
`AUDIO | MIDI | APPEARANCE | ACCESSIBILITY | LOCALIZATION | EXPRESSION | UPDATES | PRIVACY | DIAGNOSTICS | FILE LOCATIONS`

Every tab section 5 names is present except RANGES, every one of them is in the
order section 5 gives, and nothing is here that section 5 does not name.
`GENERAL`, which was not in the canonical list, is gone: its contents went to the
tabs that do own them.

One departure, deliberate:

- **No RANGES.** `advanced-ranges.md` specifies its entire contents - the
  per-preset toggle, the warning-colour preference, the stock-range preference,
  the out-of-range summary - and that file does not exist. There is nothing to
  build that would not be invented. The slot is simply absent rather than held
  open by something else, so this list is ten tabs until that spec is written.

**The second departure is closed.** A CONTROLLERS tab that section 5 does not
list used to sit in RANGES' slot, because section 19's home for controller setup
- the Advanced column 4 tab strip - did not exist, and deleting the tab would
have stranded controller setup completely. The strip exists now and the page has
moved there. A2 has the reasoning and what covers it.

Three controls sat on the old GENERAL tab and section 5's list has no slot for
any of them. All three already have canonical homes in `AdvancedPanel` - see
"`chordWindow` had no canonical home" below, which is the work that gave the
last of them one - so every option here was safe: what was on GENERAL was a
mirror, not the only way to reach the setting.

| Control | Now on | Why |
|---|---|---|
| Oversampling | AUDIO | An audio-quality setting, and AUDIO is the only audio tab section 5 has |
| Chord window | MIDI | It decides how Luthier reads the MIDI it is given |
| Tuning drift | *(dropped)* | Nothing on section 5's list is about the instrument, and the Advanced control is a better home than a tab it does not fit |

What each new page could not build, because the thing behind it does not exist:
the accent tint, the data-stream toggle and the noise-event strip toggle on
APPEARANCE; the changelog viewer on UPDATES, which shows the manifest's links
instead because the manifest carries no notes; the Workshop / Slide /
advanced-ranges flag mirror on DIAGNOSTICS; and `Guitars/` and `Parts/` on FILE
LOCATIONS. Each says so on the page rather than showing a dead control.

**Covered by.** `Editor::everyOptionsPageSelectsAndPaints` walks the ten tabs by
name, and checks that each one puts its own page - and only its own page - on
screen. The tab list in that test is a copy of the list above, so a tab that
disappears or is renamed fails there - and so does a CONTROLLERS page added back
here while column 4 still has one, because the count would no longer match.

## A4 — Section 19 rows whose secondary access is absent

Primary locations exist for everything not listed under "What this blocks".
These secondary paths do not:

- ~~**Easy mode instrument interactions**~~ — done, and the entry above it was
  wrong. `GuitarBodyComponent` *was* hit-tested: the volume and tone knobs, the
  selector switch and the pickups have been live since it was written, and its
  own header comment said so. What was missing was two of section 3.1's four
  regions - the headstock and the bridge - so the right description was "half
  hit-tested", not "not hit-tested".

  Both are built now. The headstock opens a tuning popover and the bridge opens
  a whammy popover, the latter only when a bridge with an arm is fitted, which is
  section 3.1's own condition. Every region also describes itself on hover,
  including the hardtail case, which says there is nothing to set rather than
  going quiet: 3.1 makes the *popover* conditional, not the affordance.

  The headstock popover is the only way to author per-string detune. That is the
  part worth noticing - `TuningEngine::StringTuning::detuneCents` is written into
  the preset by `PresetManager` and read back out of it, and no control anywhere
  in the UI could set it. A preset field with no way to author it is the same
  shape of gap as a panel nobody constructs, and it had been there all along.

  **Its six detune sliders are not automatable.** There is no per-string tuning
  parameter to attach them to, so they write `TuningEngine` directly and cost
  MIDI Learn and host automation on those controls. Closing that is a parameter
  count change and a preset schema question rather than a UI one, so it is here
  rather than done: see B1.
- ~~**Right-click → Modulate**~~ — **the menu exists and this row was wrong**,
  in the same way and for the same reason as the Easy-mode row above it: the
  entry was read instead of the code. `showParameterContextMenu` has offered
  every modulation source since `996f89d`, grouped LFO / Envelope / Sequencer /
  Follower / Macro / Performance, with the destination taken from the control
  under the cursor, a new route built at a third of full depth, a "Remove
  modulation (n)" entry when routes exist, and a full destination saying so on
  its face rather than offering sources it would silently drop.

  What let it stay wrong for three milestones is that **nothing tested it**.
  Nothing else in the plugin goes through that menu, so a secondary path could
  have broken in any release without a failure anywhere.
  `Editor::rightClickOffersModulationAndBuildsTheRoute` covers it now, and
  covering it needed the menu split into `buildParameterContextMenu` and
  `applyParameterMenuResult`: a function that builds items and shows them in one
  breath can only be checked by a human looking at the screen.

  **Drag-to-assign is genuinely absent.** There is no `DragAndDropContainer`
  anywhere in `Source/UI/` except `ToneMatchPanel`'s file drop. Ground rule 4 is
  satisfied without it - the MOD tab cards are the primary surface and the menu
  is a second route - so this is a convenience rather than a missing path, and it
  is the only part of this row still open.
- **Header notification** for an available update.
- **Post-crash prompt** for crash reporting.
- **Help > About** as a route to license.

## B1 — Capo is documented, promised by the UI spec, and does not exist

Found while building section 3.1's headstock popover, which asks for "per-string
tuning, capo, temperament".

There is no capo. Not a parameter, not a field in `TuningEngine`, not a line of
code anywhere in `Source/`. What exists is four specs that describe it and two
user-facing documents that tell the user how to use it:

| Where | What it says |
|---|---|
| `gui-integration.md` 3.1 | the headstock popover offers capo |
| `gui-integration.md` 19 | "Capo (fret / partial) - TuningEngine - Adv Col 1 GUITAR, Workshop capo drag" |
| `ambiguity-resolutions.md` 4.5 | how a capo and a partial capo behave |
| `guitar-illustration.md` | drag a capo card onto a fret; drag it along the neck |
| `factory-content.md` | three capo parts ship with the plugin |
| **`docs/USER_MANUAL.md`** | "Right-click for mute, capo, string selection and scale overlays" |
| **`docs/KEYBOARD_SHORTCUTS.md`** | right-click menu includes "set capo" |

The last two are the problem. The specs describing an unbuilt feature is the
normal state of this project and `INDEX.md` tracks it. The *manual* describing it
is a promise to the user that the build does not keep, and a user following the
manual finds a right-click menu with no capo in it and no explanation.

**Size.** The engine half is small - `TuningEngine` already has per-string open
frequency and max frets, and a capo is a per-string minimum fret plus a pitch
offset, which `ambiguity-resolutions.md` 4.5 specifies exactly. The UI half has
three named homes (Adv Col 1 GUITAR, the headstock popover, the Workshop drag),
two of which exist. The partial-capo string mask depends on the capo *part*,
which is Workshop, which is blocked.

**Until it is built**, the headstock popover says on its face that capo is not
built, rather than leaving a gap where section 3.1 says a control goes. The two
docs have not been corrected here because changing them is a decision about
whether to describe the build or the plan, and that is the user's call rather
than a gap to close silently.

## A5 — Shortcuts: audited, mostly closed, four rows still open

The audit found something worse than drift. There were **three** sources of
truth: the hard-coded key comparisons in `PluginEditor::keyPressed`, the
rebindable registry in `AccessibilitySettings`, and section 17. They disagreed,
and because the editor never consulted the registry, **rebinding a shortcut
changed the row in the Options table and nothing else**. Section 17's "all
rebindable" and accessibility.md 2's rebind table were both decorative.

The registry is now the single source of truth and the editor reads from it.
Defaults match section 17, two tests hold them there, and `Ctrl + Shift + /`
opens the table. Fixed on the way: the preset browser was `Ctrl+P` where section
17 says `Ctrl+O`; `Ctrl+Shift+R` randomised instead of resetting; and A/B
compare, Live Mode, the Practice drawer and Options had no binding at all.

Still open:

| Section 17 row | Why not done |
|---|---|
| New preset, `Ctrl+N` | No "new preset" action exists. `resetEverything()` is Reset All, which is a different thing. Needs an init-preset concept first. |
| Reveal preset file, `Ctrl+Alt+E` | `PresetManager` tracks the current preset's name and index but not its file path, so there is nothing to reveal. |
| Workshop `W`, Slide `S`, Save As Guitar `Ctrl+G`, New Tune `Ctrl+T` | Blocked on the missing specs. Deliberately absent from the registry rather than present and dead. |

Closed since: **Next / Prev Col 4 tab, `Ctrl+]` / `Ctrl+[`**. They were blocked on
A2 because there were no tabs to step, and there are now. They answer only in
Advanced Mode - in Easy there is no column 4, and a key that returns true and
does nothing is how a host stops passing it on - and they wrap, because a strip
that stops dead at the end makes the last tab need a different key to leave than
every other tab does.

**Every binding now has a description.** The rebind table and
`getPrintableShortcuts` both read `tr (binding.descriptionKey)`, and not one of
the twenty-five keys was in the English catalog, so `translate` returned the key
and every row of accessibility.md 2's table read
`accessibility.shortcut.undo` rather than `Undo`. The table was built, ordered
and rebindable, and every label in it was a key. The strings are in the catalog
now, and `Accessibility::everyShortcutHasADescriptionInTheCatalog` asks for the
one thing the fallback cannot fake - a description that is not its own key.
The test that existed asserted `isNotEmpty`, which a key satisfies.

Two deliberate deviations, both commented in the code:

- **Snapshot digits are not in the registry.** Eighteen rows for eighteen digits
  would bury the table, and the binding is positional - digit *n* recalls
  snapshot *n* - so there is nothing meaningful to rebind it to.
- **Space auditions rather than driving the tune transport.** Section 17 gives
  Space to the transport, which does not exist. When `tune-builder.md` lands the
  transport takes Space and audition moves.

`Escape` is intentionally not rebindable: accessibility.md 2 makes it the way out
of a dialog, so it should not be losable to a clumsy rebind.

## Fixed since the first audit

- **`Source/UI` was excluded from every target that runs**, so nothing in this
  repository ever constructed a `LuthierAudioProcessorEditor`. The panels were
  compiled only into the plugin, and the only thing that opened the window was a
  host: pluginval if someone remembered to run it, a DAW if they did not. That is
  how eight panels once sat in the tree for a whole milestone without being
  compiled at all. `LuthierTests` now builds the UI as well, with
  `LUTHIER_HEADLESS=0`, and `Source/Tests/EditorTests.cpp` opens the editor, lays
  it out at the minimum, default and a large size, drives every overlay shortcut
  and every layout mode through the registry, and selects all five Options tabs.
  Rendering goes into an offscreen image, so no desktop window is needed.

  What it does not do is judge what it sees. These are smoke tests: they know
  whether a panel is on screen and whether it painted, not whether it looks
  right. The assertions read state rather than only pixels because the first
  draft did the opposite and proved nothing - it compared renders of the whole
  Options panel, and passed with `showPage` neutered to `setVisible (false)`,
  since selecting a tab lights that tab up whether or not its page appears.

- **`PluginProcessor` was excluded from the test target**, so everything the
  processor alone owns - the undo stack, `uiState`, the A/B slots, snapshot recall
  - could be read in the source but never exercised. `state-model.md` specifies
  all of it precisely, and a specification nothing checks is a wish. It now
  compiles into both console targets, the renderer under `LUTHIER_HEADLESS=1`,
  which removes its single reference to the editor. The coupling turned out
  to be two guards and one define, which is worth knowing: the old comment implied
  the plugin-client macros made this hard, and they did not.

  This is the same shape of blind spot that let five test suites sit unlinked
  earlier in this session. The first `state-model.md` tests written against it
  pass, so nothing was broken behind the exclusion - but nothing was proving it
  either.

- **There was no error log.** `error-recovery.md` 5 requires one, explicitly not
  conditional on telemetry consent: a user who has opted out of sending anything
  still deserves a local record, and support cannot ask for a log that was never
  written. `Diagnostics` had an in-memory ring for the debug stream, gated behind
  the debug-panel toggle, and nothing persistent. `ErrorLog` now writes JSON lines
  to `errors-<yyyymm>.log`, and the preset load and save paths report through it
  with the codes from sections 1 and 2.

- **MIDI Learn was reachable only by right-click**, which broke ground rule 4 and
  left the section 19 header button and `Ctrl+L` unimplemented. There is now a
  global arm: the header **Learn** button or `Ctrl+L` arms it, a transparent layer
  takes the next click, walks up from whatever was hit to the nearest
  `LearnTarget`, and starts learning for that parameter.

  Consuming the click rather than observing it is the point. A global mouse
  listener would see the press but the control would still act on it, so arming
  and then clicking a knob would move the knob. Two tests cover the state machine,
  and the second one caught a real bug: disarming has to cancel a learn in flight
  even though `claimArmedLearn` has already cleared the armed flag, which is the
  normal case rather than an edge - by the time the user presses Escape, armed is
  false and learning is true. The editor therefore takes its overlay down directly
  after a claim instead of going through the disarm path, which would cancel the
  learn it had just started.

- **Freeze and E-Bow were one control, implemented as E-Bow.** The build had a
  single `freeze` bool driving the resonance-drive mechanism under the Freeze
  name - which is precisely the ambiguity `ambiguity-resolutions.md` section 2
  exists to settle. They are now two features with two enables, as 2.3 requires:
  `ebow_enable` keeps the resonance drive, and a new `FreezeOverlay` implements
  2.1's captured-loop overlay with its seven parameters. Both live in a SUSTAIN
  section. Five tests cover it, including 2.4's sixty-second hold.

  Worth recording for whoever builds the rest: the obvious implementation of a
  freeze - two read heads half a window apart under Hann windows - does not meet
  2.4. Constant overlap-add guarantees the *windows* sum to one, but the heads
  are hundreds of milliseconds apart and so read uncorrelated material, which
  means power adds rather than amplitude and the level follows
  sqrt(wA^2 + wB^2), swinging about 3 dB a cycle. Crossfading the seam once at
  capture time and reading a single head makes every cycle bit-identical, which
  is what the test actually asks for.

- **`chordWindow` had no canonical home.** It existed only in the Options
  overlay, so the section 5 restructure would have stranded it. It now has a
  control in the Advanced Performance section beside its siblings
  (`legatoWindow`, `bendRange`, `strumSpeed`), which makes the Options copy a
  legitimate mirror under ground rule 1 rather than the sole access path.
  `oversample` and `tuningDrift` were checked at the same time and already had
  canonical homes in `AdvancedPanel`.

## Not audited yet

`ui-wiring.md`, `onboarding.md`, `performance-budget.md`, `qa-polish.md`,
`installer.md`, `tune-builder.md` and `midi-export.md` have not been read
against the build. `gui-integration.md` sections 20 (discoverability), 21
(realism empty-states) and 22 (tests) are also unaudited.

## Suggested order

1. ~~**A3**~~ — done. Ten tabs, section 5's order, nothing present that section 5
   does not name; RANGES is the one thing left out. See A3 for what each new page
   could not build and why.
2. ~~**A1**~~ — done. Four columns in section 4's order, section 4.5's widths,
   and the 1000-point minimum enforced with a notice rather than declared in a
   constant nothing read.
3. **A2**, the rest of it. The strip is built and the six panels that exist are
   on it; what is left is the seven tabs that are not, and they split two ways:

   - **LIVE, PRACTICE, NOTATION, MIDI OUT** are surfaces nobody has built. Each
     has a working runtime half (the live strip, the practice drawer) or a
     working engine (notation, MIDI out) and no setup page in front of it.
     Smallest first: LIVE and PRACTICE already have the state to show.
   - **WORKSHOP, TUNE, HELP** wait on `guitar-workshop.md` / `workshop-ui.md`,
     on `tune-builder.md` being large rather than missing, and on deciding
     whether an overlay should also be a tab.

   CONTROLLERS was the third way and is done: it was a placement move rather
   than a build, and moving the page rather than sharing the library was what
   made it small. Options lost the tab and is ten, which is what section 5 asks
   for anyway.
4. **A4** — the rest of the secondary access paths, and there is no ground-rule-4
   offender left in it. The Easy-mode instrument is done, MIDI Learn has a header
   route, and right-click → Modulate turned out to have been built all along.
   What remains is the three notification routes (update available, post-crash
   prompt, Help > About), which are small and independent, and drag-to-assign,
   which is a convenience rather than a path.

   Two of this entry's rows have now been closed by reading the code instead of
   the list, which is worth saying out loud: **this file is the least trustworthy
   document in the repository about what exists.** It was written in one pass
   against a build it did not run, and a row here is a question to check rather
   than a fact to act on.
5. **B1**, the capo, or at least the half of it the docs have already promised.
   The engine side is specified exactly by `ambiguity-resolutions.md` 4.5 and two
   of its three UI homes now exist. Whether to build it or to correct the manual
   is a call for whoever owns the shipping story.
6. Everything else waits on the eleven missing specs.
