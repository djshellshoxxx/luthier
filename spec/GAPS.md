# GAP LIST — current build vs the spec set

Produced per `CLAUDE_CODE_BRIEF.md` step 2: audit the build against
`gui-integration.md` section 19 (the feature-to-location index), record every
row whose UI location does not exist, fix nothing yet.

Audited at commit `88b0796`, against the `gui-integration.md`, `INDEX.md` and
`CLAUDE_CODE_BRIEF.md` present on 2026-09-18. **Those three files changed twice
during the session that produced this audit** — the Options tab list went from
ten tabs to eleven and the Column 4 tab list from ten to thirteen — so check the
spec dates before trusting any row here.

## B0 — The eleven missing specs — **written**

This was the blocking finding and it is closed. All twelve of `INDEX.md`
Phase 2's realism specs are on disk as of 2026-09-22.

| Spec | Status |
|---|---|
| `advanced-ranges.md` | written — `PhysicalRange`, families, the `ranges` block |
| `volume-knob-interaction.md` | written — `GuitarCircuit`, removes `CableSim` |
| `pick-noise.md` | written — click, chirp, scrape, and the shared `NoiseEngine` pool |
| `string-squeak.md` | written — wound-string finger squeak |
| `fret-buzz.md` | written — setup geometry and buzz sensing |
| `slide-guitar.md` | written — `SlideEngine`, four modes |
| `guitar-workshop.md` | written — parts model, `GuitarSpec` |
| `part-acoustics.md` | written — every part field's engine effect |
| `workshop-ui.md` | written — the bench |
| `strum-dynamics.md` | written — crossing velocity, strikers, chucks |
| `bass-techniques.md` | written — slap, pop, ghosts, bass defaults |
| `midi-export.md` | was already present |

### How they were written

Not from nothing. Each of the eleven was already referenced by specs that
had shipped — 29 references to `advanced-ranges.md` alone — and those
references had **already fixed** section numbers, schemas, budgets,
defaults and wording:

- `file-formats.md` fixed the `ranges` block schema and the
  `.luthierguitar` / `.luthierpart` files.
- `ui-wiring.md` fixed `PhysicalRange`'s shape, the `GuitarSpec` swap
  protocol and the shadow-audition flow.
- `performance-budget.md` fixed the `NoiseEngine` class list, pool sizes
  and CPU budgets.
- `gui-integration.md` fixed every panel's contents and the Workshop
  bench layout.
- `onboarding.md` fixed the first-unlock explainer's exact words and the
  ship defaults.
- `action-and-undo.md` fixed the undo entry classes.
- `state-model.md` fixed what happens when a range narrows under
  automation.
- `ambiguity-resolutions.md` 6 had already chosen where strum crossing
  velocity comes from.

So the specs were written to **match** those commitments rather than to
invent alongside them. Where a referencing spec named a section number,
the new file has that section: `string-squeak.md` 9 is the STRING NOISE
group because `gui-integration.md` says so, and `workshop-ui.md` 6 is the
spectrum delta because `ui-wiring.md` says so.

### Decisions made where nothing had chosen

Four places needed a judgement rather than a transcription:

1. **Legacy presets and the `ranges` block** (`advanced-ranges.md` 4.1).
   Treating old presets as stock would clamp and change the sound of
   every preset already saved; treating them all as advanced would
   padlock ordinary ones. Chosen: derive per family from the file — a
   family is advanced if and only if a stored value is actually outside
   stock. Sound preserved, padlock honest, derivation runs once.
2. **Compatibility is advisory** (`guitar-workshop.md` 5). A bass bridge
   on an electric warns and fits. The Workshop's value is in building
   things that do not exist.
3. **The PRACTICE tab's contents** (`practice-tools.md` 11). No spec said
   what a setup surface held. Chosen split: the drawer is for during
   practice, the tab is for arranging it and reviewing it, and the test
   for any item is "would a player touch this with a guitar on their
   lap?"
4. **`PerformanceCapture`** (`notation-export.md` 6). Sections 3 and 4
   assumed a captured score and nothing produced one. Specified as a
   lock-free ring filled from voiced notes — not incoming MIDI, because
   the score should record the string and fret the voicer chose.

### What is still genuinely blocked

Nothing in phase 2. The specs are the input to the engine work, and
`INDEX.md` now carries the module inventory and the build order for it.

`tune-builder.md` (phase 3) and the phase 4 and 5 specs were already
present throughout.

## What this blocked

Section 19 rows whose backend module does not exist. Every one of them now
has a written spec and a place in `INDEX.md`'s engine build order; what
remains is the implementation, not the decision:

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

`tune-builder.md` and `midi-export.md` were present and unblocked all along;
both are large and both sit behind the realism phase in the build order.

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

- **CIRCUIT is still CABLE.** `volume-knob-interaction.md` now exists and
  specifies `GuitarCircuit`; the panel follows the module. See B0.
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

## A2 — Column 4's tab strip — built, with six tabs still absent

**Canonical order.**
`WORKSHOP | MOD | RHYTHM | TUNE | LIVE | ROUTING | TONE MATCH | CHARACTER | PRACTICE | NOTATION | MIDI OUT | CONTROLLERS | HELP`

**Built.** A tab strip across the top of column 4 with the seven panels that
exist behind it - `MOD | RHYTHM | LIVE | ROUTING | TONE MATCH | CHARACTER |
CONTROLLERS` - in section 4.4's relative order, each in its own viewport, one on
screen at a time. The other six are listed below. A tab that opens on nothing is
worse than no tab, so they are absent rather than present and empty.

**LIVE is the one that was built rather than moved.** Section 4.4 calls it "the
setup surface" against the live strip's runtime one, and that division is the
whole design: `LiveStrip` is for a player mid-set who needs one thing one press
away, and this is for the hour beforehand, when the question is which of the 128
snapshots exist and what order they come in. Until now the bank had no editor at
all - snapshots could be captured and recalled, and never surveyed, renamed or
reordered.

It has the snapshot bank as a 16x8 grid rather than a list, because 128 rows is a
scroll and what a player wants at a glance is which pads are filled, which is a
shape; the setlist as a list, because there the order *is* the content; and the
crossfade and morph controls. Three states on each pad - filled, selected, loaded
- are distinguishable without colour, since accessibility.md 6 swaps the hues.

**Expression-pedal calibration is not on it**, and section 4.4 does list it there.
It already exists as the Options EXPRESSION page, and `ExpressionCalibrationSet`
is a user-global singleton with a file behind it, so a second editor is a second
writer to that file - the same shape of problem as the CONTROLLERS page owning
its library by value (see above), which is the one this build has already been
bitten by. Section 5 does not list EXPRESSION in Options and section 4.4 does
list it here, so the two sections disagree; that is recorded rather than resolved
by building the page twice.

| Tab | State |
|---|---|
| WORKSHOP | blocked on `guitar-workshop.md` / `workshop-ui.md` |
| MOD | **built** — `ModMatrixPanel` |
| RHYTHM | **built** — `RhythmPanel`; STRUM group still blocked on `strum-dynamics.md` |
| TUNE | not built (`tune-builder.md` is on disk, so unblocked but large) |
| LIVE | **built** — `LivePanel`; expression calibration stays in Options, see below |
| ROUTING | **built** — `RoutingPanel` |
| TONE MATCH | **built** — `ToneMatchPanel` |
| CHARACTER | **built** — `CharacterPanel` |
| PRACTICE | no spec for what a setup surface would hold — see below |
| NOTATION | needs live score capture, which does not exist — see below |
| CONTROLLERS | **built** — `ControllersPage`, moved here from Options |
| MIDI OUT | needs the profile system `midi-export.md` specifies — see below |
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

**The three tabs left are not "a surface in front of a working engine", which is
what this entry used to say about all four.** LIVE was, and it took an afternoon.
The others were checked before starting one of them, and each fails differently:

- **PRACTICE.** `practice-tools.md` section 9 specifies the **drawer** - eight
  tabs, the collapsed strip, the global controls - and nothing else. Only
  `gui-integration.md` 4.4 says there is also a setup surface, and no spec
  anywhere says what would be on it. Everything plausible (practice stats, the
  loop and session folders, click-sample choice) is a guess, and
  `CLAUDE_CODE_BRIEF.md` is explicit that nothing may be improvised where a spec
  is meant to be explicit. **Blocked on a decision, not on work**: either the
  drawer is the whole feature and 4.4's tab should go, or someone writes down
  what the tab holds.
- **NOTATION.** The export engine works - `NotationExporter` writes MusicXML,
  Guitar Pro, ASCII tab and MIDI, and it is tested. What does not exist is
  anything that captures what the player *played*: `PerformanceScore` is only
  ever filled by `NotationImporter` reading a file, in the drawer's TAB tab.
  There is no live score, so "live TAB view" and "chord-symbol history" have
  nothing to display and the export dialog would have nothing to export.
  **Needs engine work first**: capturing performance into a score.
- **MIDI OUT.** `midi-export.md` is on disk and specifies the whole thing -
  Luthier and Generic profiles, per-event-class round-tripping, a
  self-describing extension format. `MidiOutRouter` has none of it: no profile,
  no event classes. The tab is the editor for a system that has not been built,
  and several of the event classes it would toggle (squeak, pick, buzz, slide,
  workshop) are themselves blocked on the missing realism specs. **This is a
  spec to implement, not a panel to draw.**

`Editor::theLiveTabEditsTheSnapshotBankAndTheSetlist` covers LIVE, and covers the
editing rather than the painting: a panel of controls wired to nothing paints
exactly as well as one wired correctly, which is the failure every panel here has
had at least once. It found three real defects on its first run — `slotAt`
returned pad 0 for a point above and left of the grid, because integer division
truncates toward zero; the crossfade slider offered five seconds against
`SnapshotBank`'s own 500 ms clamp, so it could show a number the engine had
silently refused; and `juce::Button::triggerClick` posts a message that a console
test never pumps, so the first version's clicks quietly never happened. Its
mutant points every setlist entry at slot 0 and dies on "the setlist entry points
at snapshot 1 rather than the selected 6".

**Covered by.** `Editor::everyWorkspaceTabSelectsAndPaints` walks the seven tabs by
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

Primary locations exist for everything not listed under "What this blocked".
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
- ~~**Header notification** for an available update~~ — built, see A6.
- ~~**Post-crash prompt** for crash reporting~~ — built, see A6.
- ~~**Help > About** as a route to license~~ — built as a banner rather than as a
  Help section, see A6. The Help overlay's "About and Licence" section already
  existed and carries the licence *terms*; what was missing was any surface at
  all for the licence *state*, which `License` has tracked since it was written -
  activated, grace, expired, with a countdown - and which nothing displayed.

## A6 — Section 15's notification banners

Found while closing A4's last three rows. All three are triggers of the same
mechanism, and the mechanism did not exist: **section 15 was entirely unbuilt**,
and A4 listed three of its nine triggers without noticing they had nowhere to
appear.

**Canonical.** "Non-modal banners under the header strip, 32 px, dismissible."
Nine triggers. "Banners auto-dismiss after 5 s unless they contain an action."

**Built.** `NotificationCentre`, under the header and above the live strip, 32
points, one banner at a time with the rest queued. Every banner is dismissible;
the five-second clock runs only when there is no action button, and it is never
started for an actionable banner rather than being started and cancelled, so
there is no window in which a slow hand loses the button.

Banners carry an id rather than being identified by their text, so a trigger that
reposts - a countdown does, every time the window opens - replaces itself instead
of stacking. Queued rather than stacked because two at 32 points is 64 points of
window, and the startup triggers arrive together.

**Seven of the nine triggers are wired.** Four are checked when the window opens:

| Trigger | Source |
|---|---|
| Crash on last session | `Telemetry::hasPendingCrashReport` — action opens PRIVACY |
| License grace countdown | `License::State::grace` — warning level |
| Managed by policy | `Telemetry::isManagedByPolicy` |
| Update available | `Telemetry::checkForUpdate`, **only if the user opted in** — action opens UPDATES |

The update check is the only one that touches the network, and it runs only when
`isUpdateCheckEnabled()`, which is off by default and which a policy can switch
off but never on. Opening a window is not a reason to make a request the user
declined. It is unthrottled at the call site because `checkForUpdate` is itself
throttled to 24 hours, so opening the window twenty times a day is still one
request.

The crash banner says **Review**, not Send. `updates-telemetry.md` 4 asks for a
viewer showing exactly what would be uploaded, and a single button that sent a
crash dump would be the opt-in equivalent of a dark pattern.

**Two more were wired next**, and the sentence that used to sit here - "preset
load error, missing IR, missing guitar and missing part are one change to the
preset loader" - was wrong on every count. It was written without checking, which
is the failure this file keeps making. What is actually true:

- **Preset load error** — needed no loader change. `loadPreset` already returned
  `false` and logged; what was missing was a message a *user* could read, because
  most callers discard the bool. The header's Open dialog was the worst case: a
  preset that would not load did nothing at all, silently, which is a ground rule
  0.2 violation. `PresetManager::getLastLoadError` now carries a sentence naming
  the file, cleared by the next load that works.
- **Missing IR** — needed no loader change either. `IrSlot::fromVar` already fell
  back to the built-in model and left `lastError` set, with a comment saying it
  was there "so the header can show the banner the spec asks for". The banner did
  not exist when that was written. It does now, and the banner offers a button to
  the TONE MATCH tab.

Both are **polled** on the editor's existing timer rather than pushed, because
presets load from five places - the header, the browser, the Easy panel's style
list, a host program change, and the processor's own state restore - and five
call sites each remembering to report would be five chances to forget. They post
only when the message changes, so a dismissed banner stays dismissed while the
condition holds.

**Sample-rate change** was the last one nothing blocked, and it is built.
`prepareToPlay` records the rate and nothing else; `claimSampleRateChange` on the
message thread decides whether it is worth saying, and returns the new rate once.
Once matters twice over: `prepareToPlay` is called whenever the host feels like it
- a buffer-size change alone does it - so a banner per call would appear every
time a user touched their audio settings, and the first prepare of all is not a
change, because opening a plugin at 48 kHz is the normal state rather than news.
The claim clears the record, so two windows cannot both announce one change and a
closed window does not lose it.

It reads the processor's own `preparedSampleRate` rather than
`AudioProcessor::getSampleRate()`, which is set by `setRateAndBufferSizeDetails`
and so only by a host. The first draft used `getSampleRate()` and the test caught
it: calling `prepareToPlay` directly leaves that accessor stale, which made the
feature depend on something that is not `prepareToPlay`'s job to set.

**The last two triggers are blocked, and a third cannot happen at all:**

- **Missing guitar** — *cannot happen*. Section 15 describes a missing
  `.luthierguitar` referenced by a preset, and there is no such file format:
  `GuitarLibrary` is a compiled-in enum and a preset stores an index, which the
  parameter clamps. This trigger presupposes user guitar files, which is a
  Workshop feature that does not exist.
- **Missing part** — blocked on `guitar-workshop.md`, same as WORKSHOP itself.
- **Advanced-range clamped on save** — needs advanced ranges, which
  `advanced-ranges.md` has not specified (A3).

So section 15 is as complete as the rest of the build allows: everything it asks
for that has something to report is reporting.

**Covered by.** `Editor::aSampleRateChangeIsAnnouncedOnceAndTheFirstOneIsNot`
covers what must *not* raise a banner as well as what must: an unprepared
processor, the first prepare, and a re-prepare at the same rate with a different
block size all stay silent, and a real change is reported once with section 15's
wording. Its mutant records only the first rate, so a change reports forever -
"the same rate change was reported twice, so two windows would both announce it".

`Editor::aFailedPresetLoadAndAMissingIrEachRaiseABannerOnce`
drives the two polled triggers through the paths a user takes - `loadPreset` on a
file that is not there, `IrSlot::fromVar` on a path that is gone - and then
dismisses and pumps the timer four more times to prove a dismissed banner stays
dismissed. Its mutant is reposting every tick, which fails with "the preset
banner came back after being dismissed, so it cannot be got rid of while the
condition holds". It also checks the IR banner's button lands on a TONE MATCH tab
that exists, and that `setWorkspaceTabNamed` returns false for an unbuilt one.

`Editor::notificationBannersQueueDismissAndRespectTheirActions`
drives the queue, the id-replacement, the dismissal and both halves of the
five-second rule; `Editor::theWindowRaisesSectionFifteensTriggersAndIsQuietWhen‑
ItShould` checks that a healthy plugin opens silently and that a licence put into
its grace period - by the dates `License` derives its state from, not by a flag -
raises the banner, that the strip is laid out with a real height, and that it is
under the header. Two mutants: starting the timer unconditionally fails the
actionable-banner check, and dropping the strip from the layout fails the height
and position checks.

## B1 — Capo — **built**, and this entry was wrong about almost all of it

**What this used to say.** "There is no capo. Not a parameter, not a field in
`TuningEngine`, not a line of code anywhere in `Source/`." Then a table of four
specs and two user docs describing a feature that did not exist, and a conclusion
that `docs/USER_MANUAL.md` and `docs/KEYBOARD_SHORTCUTS.md` made "a promise the
build does not keep".

**What was actually there.** Three capos, none of which knew about the others:

| Where | What it did |
|---|---|
| `RhythmEngine::capoFret` | moved `ChordVoicer::setMinFret`, so chords were voiced above it. Persisted in the rhythm state. Reachable from the Rhythm panel's up/down buttons. |
| `FretboardComponent::capoFret` | drew a capo, and shifted that component's own fret display. Reachable from its right-click menu — **"Set capo here"**. Read by nothing else. |
| `TuningEngine` | nothing. |

So "not a line of code anywhere in `Source/`" was wrong twice over, and the
manual was not lying: the right-click menu it describes - "Mute string, select
string, set capo, scale overlay" - is exactly `FretboardComponent`'s menu, and
every item in it was there. **Neither document needed correcting.**

The real gap was narrower and worse than the entry described: *no capo anywhere
changed the pitch of a note*. Setting one on the fretboard moved a line on a
picture. Setting one in the Rhythm panel moved where chords were voiced, which
changes which notes are chosen but not what a fret sounds like. A user following
the manual got a capo that drew itself and did nothing.

**Built.** One capo, in `TuningEngine`, which is where `gui-integration.md` 19
says it belongs, behind `ParamIDs::capoFret` - a choice from Off to Fret 12, so
it automates, saves with the preset and takes MIDI Learn.

`ambiguity-resolutions.md` 4.5 is three sentences and two of them are implemented
exactly: "capo raises effective minimum fret to `capo_fret`" and "open strings
are the capo'd notes". Fret positions are measured **from the capo**, so fret 0
is the capo, `getEffectiveOpenFrequency` returns the capo'd note without any
caller having to know a capo exists, and `getHighestPlayableFret` is
`maxFrets - capoFret` - the neck really does get shorter.

**One subtlety worth naming.** The capo is applied as a *fret position*, not as a
cent offset on the open string. Under an unequal temperament those differ: the
frets are at fixed places, so a capo at 5 gives exactly what fret 5 gives, not
the open string shifted by a tempered fourth. Under equal temperament they are
identical, which is precisely what would have let the wrong one ship.

**The three capos are one.** `RhythmEngine::setCapoFret` delegates to
`TuningEngine` and its own field is gone; `ChordVoicer`'s `minFret` is back to
meaning "a floor on where to voice" rather than doubling as the capo, because
capo-relative frets make filtering below it a second time subtract the capo
twice. `FretboardComponent`'s right-click drives the parameter and reads it back
on its timer, so the drawn capo cannot disagree with the sound. `ChordVoicer` is
now also bounded by the playable span, which fixes a real defect the unification
exposed: with a capo at 12 on a 24-fret neck it would still have voiced up to its
own `maxFret` of 22, ten frets past where the neck ends.

**Three of the three UI homes exist.** Advanced column 1 GUITAR (section 19), the
headstock popover (section 3.1) and the fretboard right-click. The popover's
footer used to read "Capo is not built yet"; it now names the part that genuinely
is not.

**Still open: partial capos.** 4.5's third sentence takes the string mask from
the capo part in the Workshop, and the Workshop is blocked on
`guitar-workshop.md`. This is one fret across all strings. `factory-content.md`
ships three capo parts that nothing can load yet, for the same reason.

**A note on the rhythm state.** `RhythmEngine` no longer writes `capoFret` into
its own state - the parameter carries it, in the same preset - but `fromVar`
still reads an old one so a session saved by a previous build does not lose it.
It is not written back; re-saving moves it to the parameter.

**Covered by.** `GenreKits::capoRemovesFretsBelowItAndMovesThePitch`, which
replaces `capoRemovesFretsBelowIt`. The old test asserted `fretPosition >= capo`,
which under capo-relative coordinates only restates the coordinate system, and
which never looked at pitch at all - **a capo that transposed nothing passed it**,
and for three milestones one did. The new one checks that nothing is voiced below
the capo or past the shortened neck, that a bar actually produced notes so the
loop was not vacuous, and that an open string with a capo at 5 sounds exactly
what fret 5 sounded without one.

## A5 — Shortcuts: audited, closed except what the missing specs block

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
| Workshop `W`, Slide `S`, Save As Guitar `Ctrl+G`, New Tune `Ctrl+T` | Blocked on the missing specs. Deliberately absent from the registry rather than present and dead. |

**Two more rows closed, and both of this entry's reasons were wrong.**

- **New preset, `Ctrl+N`.** This said "no new preset action exists... needs an
  init-preset concept first". The **Init** factory preset has been in the set the
  whole time, filed under Utility, and its own blurb calls it the place to start
  when building your own. `Ctrl+N` loads it, through a new
  `PresetManager::indexOfPreset` which prefers a factory preset so that saving a
  user preset called "Init" cannot redefine what the shortcut does. It pushes an
  undo state first, because losing an unsaved sound to a mistyped `Ctrl+N` is the
  worst thing a shortcut in this window could do. It is a load rather than a
  reset, and the difference is visible: the preset name afterwards is "Init".
- **Reveal preset file, `Ctrl+Alt+E`.** This said `PresetManager` "tracks the
  current preset's name and index but not its file path". Half true, and the
  wrong half: `PresetInfo` carries a `file`, so a library preset's path was
  always reachable through the index - but `loadPreset (File)`, which is what the
  header's Open dialog calls, never sets an index, so a preset opened from
  anywhere else had nothing to map back from. `getCurrentPresetFile()` records it
  on load and covers both. With no file yet, the shortcut says so in a banner
  rather than opening some arbitrary folder.

**Covered by** `Editor::newPresetLoadsInitAndRevealSaysSoWhenThereIsNoFile`,
which loads a different preset first so that "it loaded Init" is not merely "it
did nothing" - the mutant that makes the lookup fail reports the preset is still
"DADGAD Drone". The successful reveal branch is deliberately never pressed:
`revealToUser` opens a file manager, and a test that spawned Explorer on every
run would be worse than the gap it closed. The test presses the key only in the
state with nothing to reveal, and checks the file the other branch would use
directly.

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
3. **A2**, the rest of it. The strip is built and the seven panels that exist are
   on it; what is left is the six tabs that are not, and they split two ways:

   - **PRACTICE, NOTATION, MIDI OUT** are *not* surfaces waiting on a layout,
     which is what this list said before they were checked. PRACTICE has no spec
     for its contents and is blocked on a decision; NOTATION needs live score
     capture, which does not exist; MIDI OUT needs the profile system
     `midi-export.md` describes, which is an unimplemented spec rather than a
     missing panel. None of the three is an afternoon's work. Details in A2.
   - **LIVE** is done. It was the first of this group and the pattern it set is
     worth repeating: the engines were complete and tested, and the only thing
     missing was a surface, so the work was almost entirely layout and wiring
     with the interesting decisions being which control shape suits which data
     (a grid for a bank, a list for an order) and what *not* to duplicate.
   - **WORKSHOP, TUNE, HELP** wait on `guitar-workshop.md` / `workshop-ui.md`,
     on `tune-builder.md` being large rather than missing, and on deciding
     whether an overlay should also be a tab.

   CONTROLLERS was the third way and is done: it was a placement move rather
   than a build, and moving the page rather than sharing the library was what
   made it small. Options lost the tab and is ten, which is what section 5 asks
   for anyway.
4. ~~**A4**~~ — done, and there is no ground-rule-4 offender left in it. The
   Easy-mode instrument is built, MIDI Learn has a header route, right-click →
   Modulate turned out to have been built all along, and the three notification
   routes turned out to be three triggers of section 15, which was not built at
   all and now is (A6). Only drag-to-assign is still open, and it is a
   convenience rather than a path.

   Two of this entry's rows were closed by reading the code instead of the list,
   which is worth saying out loud: **this file is the least trustworthy document
   in the repository about what exists.** It was written in one pass against a
   build it did not run, and a row here is a question to check rather than a fact
   to act on. A third row was closed by noticing that what it described was a
   symptom of something larger the file never mentioned.
5. ~~**A6**~~ — done as far as the build allows. Seven of section 15's nine
   triggers are wired; missing-guitar cannot happen (there is no
   `.luthierguitar` format), missing-part is blocked on `guitar-workshop.md`, and
   advanced-range-clamped is blocked on `advanced-ranges.md`. Nothing here is
   waiting on work rather than on a spec.
6. ~~**B1**~~ — done, and it was never the decision this list said it was. The
   question was framed as "build the capo, or correct the manual", and the answer
   was neither: the manual was accurate, the right-click menu it described had a
   capo in it, and what was missing was any connection between that menu and the
   pitch of a note. Full capo is built; partial capo needs the Workshop.

   That makes **four** entries in this file closed by reading the code instead of
   the entry, and this one was the worst of them - it opened with "not a line of
   code anywhere in `Source/`" about a feature with two half-implementations and
   a working menu item. Read a row here as a question.
7. Everything else waits on the eleven missing specs.

## Audit 2026-09-24: ui-wiring, onboarding, performance-budget, qa-polish, installer, gui-integration 20-22, action-and-undo

Read-only audit for TODO 14 / 14b against the tree at `fa89382` (branch
`claude/clever-hopper-07uz7t`). Nothing was built or run; every claim below
is from reading `Source/` and `Source/Tests/` (test names are the
`LUTHIER_TEST (Suite, name)` registrations). `docs/spec-coverage.md`
(sections 26-31, 41) was the starting index; its rows were re-checked, and
each spec ends with the rows the table gets wrong. Severity is for a $200
product: **SB** = ship blocker, **UV** = user-visible, **INT** = internal
(test, infrastructure, architecture). Size: S (under a day), M (days), L
(a week or more).

Status legend as in spec-coverage: verified (test named) / implemented (no
test) / partial (what is missing) / missing / n/a (not applicable on this
platform or not a code requirement).

### Headline

1. **The two undo stacks fight.** The processor's snapshot undo
   (`PluginProcessor.cpp:1976-2033`) captures the whole state block, and
   the state block carries the tune (`PluginProcessor.cpp:2143`,
   `root->setProperty ("tune", tuneSession.toState())`). Undoing a knob
   move therefore also reverts every tune edit made since, and because
   `setStateInformation` -> `TuneSession::restoreState` ->
   `newTune` (`TuneSession.cpp:193-206`, `114-124`) clears the tune's own
   undo and redo stacks, those edits cannot be recovered. Which stack Ctrl-Z
   reaches depends only on keyboard focus (`TunePanel.cpp:1439-1443` handles
   Ctrl-Z/Ctrl-Y itself when focused; otherwise `PluginEditor.cpp:861-862`).
   Ship blocker, size M.
2. **Nothing of installer.md exists** and the tree only builds `VST3
   Standalone` (`CMakeLists.txt:25`). Ship blocker by definition, L; the
   macOS rows are n/a on this machine but the Linux rows no longer are (the
   tree builds and tests on Linux, `scripts/build.sh`).
3. **Onboarding is half built and half absent.** First-run defaults, the
   two first-encounter hints, the range explainer and "Restore first-run
   experience" are in and tested (`Source/UI/FirstRun*`,
   `FirstEncounterHint*`, `FirstRunTests.cpp`); the welcome banner, tour,
   first-week hints, NEW dots, upgrade banner and every piece of sample
   content beyond presets / guitars / parts / templates are missing. The
   support address is still `support@luthieraudio.example`
   (`HelpContent.h:94`).
4. **Performance budgets are not measured.** Two engine-level smoke checks
   and three module checks exist; there is no per-module figure, no idle /
   heavy total, no memory, no boot time, no relief ladder, no heap or lock
   trap on `processBlock`, and the `LUTHIER_ALLOCATION_COUNTER` guards in
   `CaptureTests.cpp` / `TunePlayerTests.cpp` compile out because nothing
   defines it (`CMakeLists.txt:130-147`). Gate 5 cannot be declared green.
5. **qa-polish gates:** none of the five is green. Gate 2 fails on
   localisation alone (fifteen locales offered, one English string table,
   `Localisation.cpp:25-39`, `257+`). Gate 3's tests are looser than the
   spec's numbers (DC null asserted at peak < 0.05, i.e. -26 dBFS, not
   -100 dBFS RMS: `IntegrationTests.cpp:177`).

---

### action-and-undo.md (TODO 14b)

Two stacks exist. The processor's (`PluginProcessor.h:732-751`,
`.cpp:1916-2048`): `UndoEntry { state, redoState, description }`, 200 max,
redo tail dropped on a new entry, one entry per host gesture on the message
thread, `ScopedUndoAction` for multi-write actions, 23 non-test
`pushUndoState` / `ScopedUndoAction` sites. The Tune's (`TuneSession.h:33-55, 133-150`,
`.cpp:26-125`): per-entry class / target / timestamp, 200 ms same-class
same-target merge, 200 max, redo cleared, load is a boundary that clears
the history. The Tune stack is the closer of the two to the spec.

| Sev | Requirement (§) | Status | Code | Gap and proposed fix | Size |
|---|---|---|---|---|---|
| SB | 0.5 / 3.9 / 10: one intent trail; tune load is a boundary, not a wipe of everything else | **partial - defect** | `PluginProcessor.cpp:2143, 2235`; `TuneSession.cpp:193-206`; `TunePanel.cpp:1439-1443`; `PluginEditor.cpp:861` | Processor undo reverts the tune and wipes its history (Headline 1). Fix: capture the state block without the `tune` property for undo entries (a `captureStateBlock (excludeTune)` used by `pushUndoState` / `parameterGestureChanged` / `undo` / `redo`), and route Ctrl-Z to the Tune stack only while the TUNE tab has focus, as now. Same exclusion for A/B slots (`setSlotBActive`, `PluginProcessor.cpp:1876-1900`), which also wipe the tune history today. Add a processor-level test: knob gesture, tune edit, `undo()` leaves the tune and its `canUndo()` alone. | M |
| UV | 0.2 / 5 / 9: state boundaries (preset, tune, guitar, family, setlist step); Ctrl-Z stops at one; Ctrl-Alt-Z crosses with a banner | **missing** | preset load pushes a plain entry: `HeaderBar.cpp:73, 82, 410`, `Overlays.cpp:1262`, `PluginEditor.cpp:839`; family: `PluginProcessor.cpp:605`; `Accessibility.cpp:523-524` | Add `bool boundary` to `UndoEntry`, a `pushUndoBoundary (description)`, make `undo()` refuse to step past `undoStack[undoPosition]` when the entry *below* the one just undone is a boundary unless `crossBoundary` is set; bind `Ctrl+Alt+Z` (DECISIONS C-28) and post a section-15 banner. Setlist step (`PluginEditor.cpp:848-858`) pushes nothing today and should push the boundary. | M |
| UV | 3.7: snapshot save / recall / rename / colour / delete / move push entries | **missing** | `PluginProcessor.cpp:1612` (`recallSnapshot`), `LivePanel.cpp`, `LiveStrip.cpp` (no undo calls) | Wrap `recallSnapshot` and the LIVE tab's bank edits in `pushUndoState ("Recall snapshot 3 Verse")` etc. Recall is the important one: it rewrites every parameter and cannot be undone today. | S |
| UV | 3.6: mod-route create / delete / edit and source edits | **missing** | `Widgets.cpp:278-290` (right-click Modulate builds the route with no push), `ModMatrixPanel.cpp` (none) | `pushUndoState ("Add LFO 1 to Amp Gain")` before `modMatrix.addRoute`, same for remove / depth edits (depth drags are sliders; if they are not APVTS parameters they get no gesture entry either). | S |
| UV | 3.13: pedal add / remove / move / bypass | **partial** | slot type and bypass are parameters (gesture entries via combo / toggle); reorder drag `PedalRack.cpp:262-268` pushes nothing | Push "Move Overdrive to slot 3" around the drop; the entry text for the combo path reads "Change Pre slot 1 type" rather than "Add pedal". | S |
| UV | 3.12 / 3.14 / 3.15 / 3.10: MIDI Learn, practice, character, setlist edits | **missing** | `MidiLearn.cpp`, `PracticePanel.cpp`, `CharacterPanel.cpp`, `LivePanel.cpp` setlist editor: no undo calls; `PracticeLooper::layerUndoAndRedo` is the looper's own layer undo, not the plugin stack | One `pushUndoState` per operation at the UI call site; character edits and scale-trainer changes want `ScopedUndoAction` since they write several values. | M |
| UV | 1 / 9: Undo History dropdown (View menu), searchable, boundaries as rules | **missing** | no menu (`rg "Undo History"` finds only a comment in `TunePanel.h:24`) | A popup listing `undoStack` descriptions newest first with the boundary separators once boundaries exist; click = undo back to that index. | M |
| UV | 12: Diagnostics "Show Undo Depth" footer counter | **missing** | `OptionsPages.cpp` DiagnosticsPage (no such toggle) | Toggle in `DiagnosticsPage`, footer label reading `"Undo: " + getNumUndoSteps() + " / 200; Redo: " + (undoStack.size() - undoPosition - 1)`. | S |
| UV | 9: Ctrl-Y redo on Windows | **partial** | `Accessibility.cpp:523-524` binds only Ctrl-Z / Ctrl-Shift-Z; `TunePanel.cpp:1442` accepts Ctrl-Y locally | Add a second binding for `redo` (the registry is one key per action, so either allow an alternate or add a `redoAlt` action). | S |
| UV | 3.1 / 3.2: description "Change X from A to B"; discrete switches within 200 ms merge; drag pause > 200 ms splits | **partial** | `PluginProcessor.cpp:1967-1969` ("Change " + name only); every combo step is its own gesture entry; a drag is one entry however long it pauses | Record `gestureStartValue` / end value as text in the description; in `addUndoEntry`, merge with the previous entry when it names the same parameter and arrived within 200 ms (keep the older `state`, drop the newer). Conflict C-27 already ranks action-and-undo over gui-integration 18 for this. | M |
| UV | 3.3 vs 3.17: Easy/Advanced toggle - 3.3 lists it as an undoable toggle, 3.17 says not undoable | **spec conflict** | mode toggles push nothing (`EditorTests::theModesThatChangeTheLayoutTakeEffectAndUndoThemselves` asserts render digests, not stack size); Slide Mode does push (`HeaderBar.cpp:243`) | Record in DECISIONS: 3.17 wins (view state). Nothing to build. | - |
| UV | 8: family-switch undo warns about lost parts | **missing** | `PluginProcessor.cpp:605` pushes "Change guitar family" plainly | When the entry being undone is a family switch, post a warning banner before restoring. Depends on the boundary flag. | S |
| INT | 2: overflow drops the oldest; 13 stack-overflow test (250 -> 51st) | **implemented** | `PluginProcessor.cpp:2002-2003` | Add the 250-push test; also assert `undoPosition` after the trim. | S |
| INT | 13: per-class fixtures; 199 / 201 ms grouping; preset load single entry; undo mid-play; 1000 mod-driven changes push nothing | **partial** | verified: `RangesUiTests.cpp:256` `Undo::stepsOneActionAtATimeBothWays` (branch clears redo; fresh instance empty), `ResetStop::resetAndStopIsOneUndoStep`, `WorkshopBench::aDragIsOneUndoEntryWithItsBeforeAndAfter`, `WorkshopPanel::clickingACardFitsItAsOneUndoEntry`, `TunePanel::sessionUndoGroupsSameTargetEditsWithin200ms` (50 + 150 ms merge, 500 ms split; not 199 / 201) | Add: a 1000-route `ModMatrix` run asserting `getNumUndoSteps()` unchanged; a parameter change from a non-message thread pushes nothing (`PluginProcessor.cpp:1932` guard); undo during `processBlock` on a second thread with no NaNs / no dropped block. | M |
| INT | 3.9: Tune stack - boundary should be an undoable entry, not a wipe | **partial (by design)** | `TuneSession.cpp:118-124` | Acceptable once Headline 1 is fixed; note it in DECISIONS rather than change it. | - |
| INT | 3.5 wording "Move [handle] from X mm to Y mm" vs built "Moved neck pickup 150 → 142 mm" (workshop-ui 8 / ui-wiring 18) | **n/a** | `WorkshopBench.cpp:97` | ui-wiring 18 names workshop-ui 8's form; keep. | - |

Verified rows worth keeping in the table: 0.1 (gesture entries), 0.4 / 7 / 11 (only message-thread gestures push: `PluginProcessor.cpp:1932`), 0.5-0.6 / 10 (member of the processor, not serialised: `getStateInformation` property list has no undo key), 2 (200, redo cleared), 3.4 / 3.5 (bench), 3.11 (`changeRanges`, `PluginProcessor.cpp:299-301`), 6 (whole-state snapshots are atomic).

**spec-coverage 41 corrections:** AU-3.4-01 and AU-3.5-01 are `verified`, not `pending` ("no bench"): `WorkshopPanelTests.cpp` `WorkshopPanel::clickingACardFitsItAsOneUndoEntry`, `WorkshopBenchTests.cpp:74`. AU-3.9-01 is `partial` (TuneSession implements classes, grouping and the boundary), not `pending` ("no Tune"). AU-3.17-01's open question is settled: the test checks render digests, mode toggles push nothing. AU-13-05 is `verified` for the Tune stack only. AU-13-08 is covered by the first assertion of `Undo::stepsOneActionAtATimeBothWays`. AU-8-01 "no family switch" is stale (`PluginProcessor.cpp:605`). AU-3.14-01 should cite `PracticeLooper::layerUndoAndRedo` as the looper-local mechanism. The section preamble's "17 non-test call sites" is now 23 and the bench entries carry real-unit strings.

---

### ui-wiring.md

The build attaches through APVTS attachments (`Widgets.cpp`), polls atomics
on timers and applies structural changes on the message thread behind a
park (DECISIONS C-09). The command / result queue, display FIFO and shared
`ThreadPool` of sections 4, 12 and 19 are not built, and the coverage
table already says so. What changed since it was written: a real SPSC
queue exists for one command class (`PluginProcessor.h:537-539`,
`workshopFifo`), the spectrum delta runs on its own `juce::Thread`
(`SpectrumDelta.h:24`), shadow audition exists (`WorkshopBench.h:114-117`)
and the illustration is one renderer for Easy and bench
(`GuitarBodyComponent.cpp`, `WorkshopPanel.cpp:86-91`).

| Sev | Requirement (§) | Status | Code | Gap and proposed fix | Size |
|---|---|---|---|---|---|
| UV | 21: `AccessibilityHandler` on every custom widget (fretboard, step grid, spectrum pane, buzz heatmap, bench illustration) with per-child handlers; units in announcements | **partial** | custom handlers only in `CircuitPanel.h/.cpp` and `StringRoll.cpp` (`rg createAccessibilityHandler`); JUCE defaults elsewhere | `FretboardComponent`, `RhythmPanel` step grid, `SetupGroup` heatmap and `BenchIllustration` need handlers exposing children; `LuthierKnob` should append the unit to the value text for screen readers. | M |
| UV | 20: every string from the catalog; locale change re-labels live | **partial** | `Localisation.cpp:25-39` offers 15 locales, one English table from `:257`; 69 `tr (` calls against 278 `setTooltip` literals in `Source/UI` | Either ship only `en` / `en-GB` for 1.0 (drop the other 13 from the picker) or move the literals into the catalog. Shipping a picker that changes nothing is a gate-2 failure. | L (translate) / S (narrow the list) |
| UV | 8: MIDI mappings per preset with a "Save as global" flag | **partial** | `midiLearn.toVar()` in the state (`PluginProcessor.cpp:2094`); no global flag (`MidiLearn.h`) | Per-mapping `global` bool persisted in `UiPreferences`; merge on load. | M |
| UV | 12: drag a source card onto a control, 0.25 depth, ghost drag | **missing** | `DragAndDropContainer` only in `PluginEditor.h`, `ToneMatchPanel.h`, `MidiOutPanel.cpp`; route creation is right-click only (`Widgets.cpp:278`, verified by `Editor::rightClickOffersModulationAndBuildsTheRoute`) | Make `ModMatrixPanel` source cards drag sources and `LuthierKnob` a `DragAndDropTarget`; reuse the right-click route code. | M |
| UV | 17: MIDI export profile selection in the state | **partial** | `MidiExportDefaults` lives in `UiPreferences` (global), state property list has no `midiExport` (`PluginProcessor.cpp:2094-2143`) | Add the chosen profile id to `getStateInformation`; keep the defaults global. | S |
| UV | 6.3: audition crossfades back over 30 ms | **partial** | `WorkshopBench.cpp` (`rg crossfade` finds nothing); `WorkshopBench::auditionNeverCommits`, `WorkshopPanel::auditionFromTheDrawerNeverCommits` cover "never mutates" | Fade the engine's output over 30 ms around `endAudition`; workshop-ui rows say the same. | S |
| INT | 6.2: same-string-count part swaps built off-thread and swapped at a block boundary with a 5 ms coefficient crossfade | **partial** | park path; `WorkshopSwap::aPartSwapDuringANoteIsClickFree`, `aNotePlayedWhileParkedIsKeptNotDropped`, `aSwapMapsOnceNotPerBlock` | Already TODO 6e (DECISIONS C-09). | L |
| INT | 4.2 / 9 / 10: display FIFO with per-subsystem drain rates | **missing (by decision)** | timers: `CircuitPanel.cpp:16` (15 Hz), `FretboardComponent` 30 Hz, editor 4 Hz poll | gui-engine-dataflow.md rows already track the rates (FeedbackLed 20 vs 30 Hz is TODO 2k). No further action here. | - |
| INT | 19: shared 2-thread `ThreadPool` for loads, exports, matches, deltas | **partial** | `SpectrumDelta` owns a `juce::Thread`; exports and loads are synchronous on the message thread (notation export off-thread is TODO 2k) | Introduce one `juce::ThreadPool (2)` on the processor and move `SpectrumDelta`'s job, notation export and guitar-file loads onto it. | M |
| INT | 23: attachment-leak test (every panel x100), 60 s lock-detector session, 20-preset determinism, 1000-op undo walk, 128-CC learn, FIFO overflow, 100 auditions byte-identical, ranges x100 without allocation, 100 swaps per slot, delta vs offline 0.2 dB | **partial** | present: `MidiLearn::mapsAndUnmapsCleanly` (not all 128), `Ranges::wideningPreservesEveryPlainValue` (one pass), `WorkshopSwap::aPartSwapDuringANoteIsClickFree` (one swap), `WorkshopSpectrum::aNullChangeIsFlat` / `aRealChangeShowsAndIsDescribed` (no offline reference) | The cheap ones first: panel x100 leak test (JUCE leak detector already fires; add an editor-construction loop), 128-CC learn loop, 100-audition byte compare of `getGuitarBlock()`. | M |
| INT | 1: unit enum and category tag per parameter; translated display names | **partial** | `Parameters.cpp` | Not worth a change before localisation is decided. | - |
| n/a | 0.3 / 17: `uiState` as a ValueTree holding Slide Mode | **n/a** | `UiState` struct `PluginProcessor.h:439-454`; Slide Mode is the `slideGuitar` parameter (`HeaderBar.cpp:238`) | Struct serialises fine; Slide as a parameter is automatable, which is better. Record in DECISIONS. | - |

Verified and unchanged: 1 (`RangeState` swap, `Ranges::normalisationFollowsTheLiveRange`), 5 snapshot recall (`StateModel::recallingASnapshotStaysInsideThePreset`), 7 (`Ranges::narrowingClampsAndReportsTheCount`), 8 arm / claim (`MidiLearn::armingIsSeparateFromLearningUntilAControlClaimsIt`), 14 pool and Aux 8 (`NoisePool::aFullPoolStealsTheOldest`, `PluginBuses::aux8CarriesThePlayingNoiseAndObeysItsStrip`), 15 (`Circuit::sweepingEveryControlDoesNotAllocate`), 17 / 22 state round trip (`Presets::stateRoundTripsExactly`, `WorkshopPresets::anEditedGuitarTravelsWholeInTheState`).

**spec-coverage 27 corrections:** UW-6.3-01 is `partial`, not `pending` (audition exists, two tests). UW-6.4-01 is `verified` (`SpectrumDelta` worker, `WorkshopSpectrum::theWorkerCoalescesAndStaysInBudget`), not `pending`. UW-13-01 is `partial`, not `pending` (`GuitarRenderer` serves both surfaces; only the pick overlay is missing). UW-4.3-01 "not built" should say one SPSC queue exists (`workshopFifo`). UW-18-01 should cite the bench tests above for the Workshop entries. The preamble's "`rg AbstractFifo` finds nothing" is stale.

---

### onboarding.md

| Sev | Requirement (§) | Status | Code | Gap and proposed fix | Size |
|---|---|---|---|---|---|
| SB | 10 / 12 (HELP) / qa 10: support address and links are placeholders | **missing** | `HelpContent.h:94` `support@luthieraudio.example` | Real address and site before any build leaves the machine (already TODO 13). | S |
| UV | 2: welcome banner per version (Yes / Maybe later x3 / Don't ask again); 13 upgrade banner "Version X installed. What's new?" | **missing** | `Notifications.cpp` has the section-15 banner system; `rg -i "welcome|tour|what's new"` in `Source/` finds nothing | A `Notification` with three actions posted from the editor ctor when `UiPreferences` has no `welcome_shown_<version>`; counter for "Maybe later". | S |
| UV | 3: twelve-step tour with Next / Back / Skip, Escape; Help -> Take the tour | **missing** | no tour code; every target now exists (wrench `HeaderBar`, TUNE tab, snapshot strip, gear, slide glyph) | A `TourOverlay` component with a table of (component finder, text); reuse `FirstEncounterHint`'s look. | L |
| UV | 4: first-week pulses (`?`, wrench, TUNE, slide), unused-tab dots, dice tooltip | **missing** | - | Needs the per-panel `?` (GI 20) and a `first_launch_date` preference; pulse is a 1 Hz alpha animation guarded by reduced motion. | M |
| UV | 13: NEW dot on new entry points for a week; changelog one click away | **missing** | `docs/CHANGELOG.md` exists; nothing surfaces it | Same preference key as the banner; HELP tab link to the changelog. | S |
| UV | 6: sample content - 6 example tunes, 12 MIDI clips in `Resources/Examples/`, 6 backing tracks, 10 setlists, 12 tune templates | **partial** | `Resources/Tunes/Templates` (ten, C-16); no `Resources/Examples`, no tunes, tracks or setlists (`ls Resources`) | Author six `.luthiertune` files from the templates and twelve clips from the genre kits (`TuneMidi` can render them); backing tracks need licensed audio. | M |
| UV | 1: fresh install loads `Factory / Rock / Modern Overdrive` on a `Les Paul Standard` | **partial** | no preset of that name in `FactoryPresets.cpp` (`rg Overdrive`); ctor installs factory presets and the type's factory guitar (`PluginProcessor.cpp` ctor, `WorkshopPresets::aFreshInstanceNamesItsFactoryGuitar`) | Decide the fresh-instance preset (a named factory preset, not parameter defaults) and load it in the ctor; record the name in DECISIONS. | S |
| UV | 5: OS preference detection on macOS / Linux | **n/a on this platform / partial** | `FirstRunOs.cpp` reads Windows only; macOS and Linux answer "off"; `FirstRun::defaultsFor` is pure and verified (`FirstRun::theOsPreferencesMapToSectionFivesDefaults`) | Linux: read `gtk-enable-animations` / `org.gnome.desktop.interface` via `gsettings` when present. macOS needs the Objective-C++ file the comment names. | S each |
| UV | 11: returning user - last preset (standalone), last tune, update banner | **partial** | practice drawer verified (`ReturningUser::thePracticeDrawerComesBackAsItWasLeft`); tune in the state (`TunePanel::theSessionRoundTripsThroughPluginState`); tab via `UiPreferences`; standalone last-preset restore not found | Standalone: persist the state block in `UiPreferences` on close and restore on launch. | S |
| INT | 14: tests - tour alignment at every scale, skip tour playable, version-upgrade banner, first-encounter popovers once until reset | **partial** | verified: `FirstRun::appliesOnceOnAFreshInstallAndNeverAgain`, `restoreClearsTheSettingsAndTheOneTimeFlagsAndKeepsTheLibraries`, `theDiagnosticsPageRestores`, `FirstEncounterHint::showsOnceAndOnlyInTheFirstSession`, `theTuneTabAndTheBenchCarryTheirHints`, `theRangeExplainerSaysSectionSevensWords` | The range explainer's own once-only behaviour has no direct test (`RangesUi::showExplainerIfFirstTime`); the rest wait on the features. | S |
| INT | installer 6 / FirstRun: `.installed_version` marker | **partial** | `FirstRun.cpp:96-108` infers a first run from the absence of both config files, or the `first_run_pending` flag | Fine until an installer writes the marker; then read it first. | S |

Verified and unchanged: 0.5 / 5 network off (`Telemetry::everythingIsOffByDefault`), 1 realism defaults (`NoiseUi::squeakStylesApplyAndReadModified`, `Buzz::playerFriendlyBuzzesOnlyWhenAttackedHard`), 6 presets / guitars / parts counts, 7 explainer text, 8 / 9 hints, 12 restore with a confirm modal (`OptionsPages.cpp:2016-2037`, `:2058`).

**spec-coverage 28 corrections (most of this section is stale):** the preamble ("No welcome banner, tour, first-week hints, OS-preference detection, example content or Restore first-run experience exist") is wrong on OS detection and Restore. ONB-5-02 `pending` -> `verified` (Windows reads) / n/a elsewhere. ONB-7-02 `pending` -> `verified` (`FirstRun::restoreClearsTheSettingsAndTheOneTimeFlagsAndKeepsTheLibraries`, `FirstRunTests.cpp:208`). ONB-8-01 / ONB-9-01 `pending` ("no TUNE tab / no bench") -> `verified` (`FirstEncounterHint::theTuneTabAndTheBenchCarryTheirHints`). ONB-12-01 `pending` -> `verified` (`FirstRun::theDiagnosticsPageRestores`). ONB-14-06 -> `verified`; ONB-14-07 -> `partial`. ONB-11-01 should cite `ReturningUser::thePracticeDrawerComesBackAsItWasLeft`.

---

### performance-budget.md

| Sev | Requirement (§) | Status | Code | Gap and proposed fix | Size |
|---|---|---|---|---|---|
| UV | 8: CPU relief ladder at 85 % of a rolling 200 ms budget (7 steps, step 7 opt-out with banner) | **missing** | `NoiseEngine::setDegraded` (`NoiseEngine.cpp:268`) is the only lever and nothing calls it | A load measurer in `processBlock` (`juce::AudioProcessLoadMeasurer`), a 200 ms average, and steps 1-4 (drain rate, data stream, mod rate, `setDegraded (true)`) which are all cheap; 5-7 later. | M |
| UV | 7: above 96 kHz oversampled modules drop 4x -> 2x -> 1x | **missing** | `Oversampler.h:110-119` takes a fixed factor; no sample-rate rule (`rg 96000|88200` in `Source/DSP`) | In `prepare`, pick the factor from `sampleRate` (4 at <= 48 k, 2 at <= 96 k, 1 above) for amp and drive pedals. | S |
| INT | 0.4 / 10: zero allocations in `processBlock`, heap trap in test mode | **partial** | `CircuitTests.cpp:32-50` replaces global `operator new` for the whole test binary; `Circuit::sweepingEveryControlDoesNotAllocate` uses it; `CaptureTests.cpp:276-292` and `TunePlayerTests.cpp:782-799` are behind `LUTHIER_ALLOCATION_COUNTER`, which no target defines (`CMakeLists.txt:130-147`); `ThreadProbe.h` counts file accesses only | Move the counter to `ThreadProbe.h`, define `LUTHIER_ALLOCATION_COUNTER` in `LUTHIER_TEST_DEFS`, and add one test that renders every factory preset for 5 s through `LuthierAudioProcessor::processBlock` asserting zero calls (already TODO 2k for the capture). | S |
| INT | 0.5 / 10: no locks on the audio thread, lock trap | **partial** | audio path uses try-locks and falls through (`BodyEngine.cpp:301`, `CabinetEngine.cpp:173, 229`); blocking `SpinLock` / `CriticalSection` only in message-thread setters (`BodyEngine.cpp:134, 192, 233`) | Acceptable; the fall-through means a block renders without the body while an IR installs (`irLoaded` false), which the "first note after an IR load differs" note in TODO 7 already records. A trap would be a `ThreadProbe::noteLock()` in a `ScopedLock` wrapper; low value. | S |
| INT | 1 / 10: per-module budgets in units | **partial** | measured (thread CPU, machine-relative): `Modulation::thousandRouteStressTest` (< 1 %), `Slap::idleAndActiveStayInBudget` (`SlapTests.cpp:586`), `Scrape::idleCostsNothing` / `anActiveScrapeStaysInBudget` (`ScrapeTests.cpp:594`) | A `LuthierRender --profile` mode (Tools/RenderCli.cpp) printing per-engine percentages for the six fixture presets; assert the totals with a generous factor and publish the numbers in PROGRESS.md. | M |
| INT | 1 totals / 6 / 7 / 9: idle, steady, heavy totals; voice and sample-rate curves; CI dashboard | **missing** | `Engine::cpuStaysWithinBudget` (`IntegrationTests.cpp:667-716`) asserts < 85 % of real time for one loaded engine | Same profiling mode; there is no CI (`.github` absent) so "on every merge" is n/a until one exists. | M |
| INT | 3 / 10: memory ceilings, 60-minute no-growth | **missing** | none | A long-running standalone soak is manual; an automated proxy is `PracticeSession::ringBufferNeverGrows` plus a 5-minute preset-cycling loop asserting `juce::SystemStats` RSS delta < 8 MB (platform-specific read). | M |
| INT | 4 / 10: latency within 1 sample of reported | **partial** | `IntegrationTests.cpp:580-591` asserts `0 <= latency < 50 ms`; `Routing::perOutputLatencyIsConsistent` | Impulse through `processBlock`, find the first non-zero output, compare to `getLatencySamples()` +- 1. | S |
| INT | 5 / 10: boot time (400 / 200 ms), guitar load 300 ms, tune load 100 ms, part swap 50 ms | **partial** | spectrum delta 40 ms verified (`WorkshopSpectrum::theWorkerCoalescesAndStaysInBudget`); guitar rebuild ~14 ms per DECISIONS but not asserted | Time `LuthierAudioProcessor` ctor + `prepareToPlay` + first block in a test with a soft (logged) bound; time `takeGuitarBlock` and `TuneSession::load`. | S |
| INT | 2: per-pedal caps; eco Quality option for expensive pedals | **missing** | `Pedal.cpp` (no Quality parameter) | Part of the profiling pass; only the pitch shifter and reverb are likely near their caps. | M |

**spec-coverage 29 corrections:** PB-10-06 "implemented - check it measures within 1 sample" is wrong: the test only bounds latency below 50 ms; it is `pending`. PB-1-01 now has three modules measured, not one. PB-0-04 should name the `LUTHIER_ALLOCATION_COUNTER` guards that compile out. PB-0-05's "cabinet try-lock" note should add `BodyEngine.cpp:301` and that blocking locks are message-thread only.

---

### qa-polish.md sections 0, 4, 5, 8, 12

**Section 0 (five gates).** None green. Gate 1: no matrix run (Known: Windows VST3 pluginval at `f18bf22`; the tree now also builds on Linux). Gate 2: fails on localisation and the missing per-panel items below. Gate 3: the tests exist but assert looser numbers than the spec (below). Gate 4: TODO 7-13 remainders. Gate 5: nothing measured (performance-budget above).

**Section 4 (UI polish).** Per control: right-click menu (`Widgets.cpp:41-130`) has Enter value, Reset to default, Copy / Paste value, MIDI Learn / Clear mapping, Lock, Randomise this control, Modulate submenu, Remove modulation, and the two range items; double-click reset comes from JUCE's `SliderParameterAttachment` (default value) and is drawn as "at default" by `Theme.cpp:728`; tooltips on 278 controls; arcs and `*` verified (`RangesUi::controlsFollowASwappedRangeAndMarkTheValue`).

| Sev | Requirement (§4) | Status | Code | Gap and proposed fix | Size |
|---|---|---|---|---|---|
| UV | Every control: tooltip and label in every locale | **partial** | English only (`Localisation.cpp:257+`) | See ui-wiring 20. | L / S |
| UV | Every control: focus via Tab with a visible focus ring in every palette | **missing** | `rg -i "focus" Source/UI/Theme.cpp Source/UI/Widgets.cpp` finds no focus drawing and no `setWantsKeyboardFocus (true)` on the attached widgets; no tab-order test | Draw a 2 px accent ring in `LuthierLookAndFeel` when `hasKeyboardFocus (true)`; make `LuthierKnob` / `LuthierToggle` / `LuthierChoice` focusable; then the GI 22 tab-order walk test. | M |
| UV | Every control: right-click has Assign to macro, Automation ID (read-only), Show in Options -> Shortcuts (GI 16 items 6, 12, 13) | **partial** | `Widgets.cpp:41-130` (items absent; Lock / Randomise are extras GI 16 does not list) | Add the three items; the automation ID is `parameter->getParameterID()`. | S |
| UV | Every control: a11y role and value description with units | **partial** | see ui-wiring 21 | - | M |
| UV | Every panel: header with status line and chevron; collapses and remembers state per preset | **missing** | `rg -i collapsed Source/UI/AdvancedPanel.*` finds nothing; ui-wiring 3's `isCollapsed` / `setCollapsed` API does not exist | Per-section collapse toggle in `AdvancedPanel`'s section headers with the state in the preset's `ui` block. | M |
| UV | Every panel: right-click empty-area menu (Collapse, Reset panel, Screenshot, Docs) | **missing** | `rg -i "screenshot|Reset panel"` in `Source/UI` finds nothing | One `PopupMenu` in a shared `PanelBase::mouseDown`; Docs calls `AdvancedPanel::showHelp` (`AdvancedPanel.cpp:1106`), which exists. | S |
| UV | Every panel: `?` that opens docs for that panel | **partial** | only the header `?` (`HeaderBar.cpp:150-152`); F1 pins to the panel in use (`HelpTab::f1AndTheHeaderOpenHelpOnThePanelYouAreIn`) | Small `?` button in each section header calling `showHelp (panelName)`; `HelpTab::everyPanelNamePinsOneTopic` already proves the topics exist. | S |
| UV | Every panel: reduced motion - noise strip becomes a static count, slide bar has no ease | **missing** | `FretboardComponent.cpp:173-183` eases unconditionally; `NoiseGroups.cpp` `NoiseEventStrip` does not read reduced motion (`rg reducedMotion Source/UI` hits only renderer, string roll, body, options, first run) | `if (AccessibilitySettings::get().isReducedMotion())` -> `ease = 1.0`; strip paints per-class counts refreshed at 5 Hz. | S |
| UV | Every panel: renders at 75-200 % UI scale without clipping; every palette | **partial** | palettes verified (`Theme::controlsRenderInEveryPaletteAndRepeatExactly`, `FacesIntegration::rendersOfBothWindowsInEveryPalette`); scale steps verified (`Accessibility::uiScaleStepsAndFontFloor`); reflow test uses three window sizes at 100 % (`EditorTests.cpp:233-235`) | Extend `Editor::itLaysOutAndPaintsAcrossItsResizeRange` over the six scales and six widths GI 22 lists, asserting no child exceeds its parent's bounds. | S |
| UV | Every dialog: focus lands on the first interactive element; screen reader announces the dialog | **partial** | `Overlays.cpp:100` gives focus to the overlay itself; Escape verified (`Editor::everyOverlayShortcutOpensItsOwnOverlayAndEscapeClosesIt`); no `postAnnouncement` (`rg announce Overlays.cpp`) | `grabKeyboardFocus` on the first child that wants focus; `juce::AccessibilityHandler::postAnnouncement (title)` on open. | S |
| UV | Empty states per GI 14 on every panel that can be empty | **partial** | built in `ModMatrixPanel.h:103`, `LivePanel`, `StringRoll`, `RangesUi.h` (`rg "gui-integration 14"`) | Walk GI 14's list against the remaining panels (rack, routing, tone match, notation, practice) and add the hint label; then the GI 22-05 test. | S |
| UV | Workshop: A/B strip loads a slot within a 30 ms crossfade; spectrum-delta matches an offline render within 0.2 dB | **partial** | hit-testing (`WorkshopPanel::everyFittedPartIsReachableAndNothingElseIs`), drag bounds (`WorkshopBench::aPickupStopsBeforeItOverlapsAndSaysWhy`), audition byte-identical (`WorkshopBench::auditionNeverCommits`), A/B round trip (`WorkshopBench::abRecallRoundTrips`, no crossfade), Save As Guitar (`WorkshopPresets::saveAsGuitarWritesAFileAndPointsThePresetAtIt`, `Workshop::everyFactoryGuitarLoadsAndRoundTrips`) | 30 ms fade around `recallSlot`; an offline-render comparison test for `SpectrumDelta::renderNow`. | S |
| UV | Slide Mode: pitch tracker announces continuous position | **partial** | glyph / overlay / part together (`WorkshopPanel::aSlideNeedsSlideMode`, `SlideUi::theSlideGroupAppearsWithSlideModeAndTheTabFitsIt`); squeak suppressed (`Slide::squeakStopsUnderTheBarButNotBesideIt`); pressure readout (`SlideUi::pressureSaysWhatItMeans`); no announcement of bar position | Accessibility handler on the fretboard's bar overlay reading "fret 7.3". | S |
| INT | Advanced-range marking: first-time unlock explainer once, later unlocks not | **implemented** | `RangesUi::showExplainerIfFirstTime`; text verified (`FirstRun::theRangeExplainerSaysSectionSevensWords`) | Test: unlock twice on a fresh `UiPreferences`, assert one popover. | S |

**Section 5 (audio polish).**

| Sev | Requirement (§5) | Status | Code | Gap and proposed fix | Size |
|---|---|---|---|---|---|
| UV | Every preset: bypass null (bypassed output bit-identical to no plugin) | **missing** | no `processBlockBypassed` override (`rg Bypass Source/PluginProcessor.*`); JUCE's default passes input through, which for an instrument with sidechain input means the sidechain is heard | Override `processBlockBypassed` to clear the outputs (an instrument bypassed is silent) and test it. | S |
| INT | Every preset: DC null - silent input produces silent output within -100 dBFS RMS | **partial** | `IntegrationTests.cpp:169-177` `Engine::silenceInSilenceOut` asserts peak < 0.05 (-26 dBFS), finite | Assert RMS < 1e-5 (-100 dBFS) per factory preset in `Presets::everyFactoryPresetLoadsAndPlays`; if noise floors legitimately exceed it, record the number in DECISIONS. | S |
| INT | Every preset: mono compatibility - no comb cancellation beyond 3 dB in the presence range | **partial** | `IntegrationTests.cpp:623-662` `Engine::monoCompatibility` asserts summed energy retained > 0.5 (-3 dB broadband) on one setup | Band-limit the check to 1-5 kHz and run it across the factory presets. | S |
| INT | Every preset: fixture without clicks / denormals / over +3 dBFS; sounds like its name (audio-lead sign-off) | **partial** | `Presets::everyFactoryPresetLoadsAndPlays` (smoke), `Parameters::fuzzAcrossTenThousandStates` (10 000 of 100 000), `Presets::randomiseNeverProducesSomethingBroken` | Add a per-preset peak / sample-delta check to the smoke test; the listening sign-off is qa 12's human check. | S |
| INT | Every amp: gain sweep monotonic in loudness at 1 kHz; tone stack neutral flat within 1 dB | **missing** | `EngineTests.cpp` Amp suite has `gainProducesHarmonicDistortion`, `standbyIsSilentAndWarmsUp` only; no ToneStack test | Two tests on `AmpEngine` / `ToneStack`. | S |
| INT | Realism: pick material energy within 0.5 dB of profile; buzz at threshold -1 / +3 dB; slide within 2 cents; slide vibrato; circuit vs CableSim table; volume-5 cleanup; bleed ratios; strum crossing click energy 25-35 %; rasgueado; bass slap two-stage within 5 ms; two-finger alternation | **partial** | verified or near: `Squeak::zeroIsFreeAndSlideModeSuppressesIt`, `Squeak::theProbabilityRollIsDeterministic`, `NoisePool::aSeedRepeatsExactly`, `PickNoise::clickPitchTracksMaterialAndThickness`, `ScrapeEngineWiring::theRakeHasATrigger` / `thePlainHighEIsThirtyDecibelsUnderTheWoundLowE`, `Buzz::theThresholdIsATrimNotAMute`, `Slide::pitchIsContinuous`, `Slide::theBarClanksWhenItLands`, `Circuit::aKinmanBleedKeepsTheTop`, `Circuit::turningDownDarkensAsWellAsQuietens`, `PluginBuses::aux8CarriesThePlayingNoiseAndObeysItsStrip`, `Slap::theClackIsTheFretBuzzGenerator`, `Slap::theUpStrokeComesAtItsGapAndItsRatio`, `SlapWiring::aGhostIsAThumpWithNoPitch`, `SlapWiring::theDoubleThumpComesBackAtItsGap`, `Muting::aGhostNoteHasNoPitchedContent`, `StrumDynamics::*` (timing and velocity, not click energy) | Missing tests: slide vibrato depth, the pre-M44 CableSim cutoff table, volume-5 cleanup at high gain, strum click-energy ratio, rasgueado spacing, two-finger alternation signature; the +-dB tolerances of the existing ones should be checked against the spec's numbers when they are tightened. | M |

Verified: every pedal click-free / bounded / zero-mix bypass (`Effects::everyPedalTypeRunsCleanly`, `Effects::bypassIsTransparent`, `Effects::chainReordersWithoutGlitching`, `Doubler::mixZeroIsTheDrySignal`); amp cold start (`Amp::standbyIsSilentAndWarmsUp`, partial: warm-up, not transient-free).

**Sections 8 and 12 (bug bash, final human check).** Process items, not code: neither has been run, and neither can be until the blockers above are closed. Classified **SB (process), n/a in code**. Section 12's Luthier-specific step (build a guitar from a template in the Workshop, save, close, reopen, load, verify) has an automated proxy in `WorkshopPresets::saveAsGuitarWritesAFileAndPointsThePresetAtIt` + `WorkshopPresets::anEditedGuitarTravelsWholeInTheState`; what it lacks is the "from a template" start (no guitar templates exist in the Workshop drawer) and the relaunch. Proposal: a `PROGRESS.md` checklist entry for each with date and name when performed, and a `Tools/bugbash-script.md` holding section 8's 90-minute script so the eight testers get the same run (S).

**spec-coverage 30 corrections:** QA-1-02 `blocked - Windows-only machine` is stale (Linux builds and runs the suite; macOS remains blocked). QA-4-05 "bench not built" is stale: `partial` with the five tests named above. QA-5-01's citation of `silenceInSilenceOut` for the -100 dBFS DC null overstates it (peak < 0.05). QA-5-04 "Aux 8, strum, bass not built" is stale (all three built and tested). QA-6-03 "not built" is stale (`MidiExportTests.cpp`, `MidiOutPanelTests.cpp`). QA-12-01 "Workshop bench absent" is stale. QA-9-03 / INS-3-01 "no Linux build" is stale.

---

### installer.md

Nothing in this spec exists in the repository: no NSIS / Inno / pkg /
deb / desktop / MIME / signing / manifest / content-package code (`rg -il
"nsis|inno|\.desktop|mime|notariz|codesign|signtool"` outside `spec/`
hits only unrelated words), no CI (`.github` absent), no
`THIRD_PARTY_LICENCES.txt`, and `CMakeLists.txt:25` builds `VST3
Standalone` only (no AU target for 2.1). The folder tree of section 6 is
created lazily per feature (77 `createDirectory` /
`userDocumentsDirectory` sites) and there is no `.installed_version`
marker (FirstRun infers first runs, see onboarding).

| Sev | Requirement (§) | Status | Code | Gap and proposed fix | Size |
|---|---|---|---|---|---|
| SB | 1: Windows installer (EV-signed `.exe`, component picker, paths, space / version checks, associations, Start menu, uninstaller with manifest and purge, `/S` `/D=`) | **missing** | none | Inno Setup script under `installer/windows/` driven by the CMake build outputs; sign in a later step (certificate is outside the repo). | L |
| SB | 3: Linux `.tar.gz` + `.deb` with `install.sh` / `uninstall.sh`, `.desktop`, MIME xml, icon paths, post-install cache updates | **missing (now applicable)** | Linux build exists (`scripts/build.sh`); `Resources/icon.png` exists | `cpack` with the DEB and TGZ generators from `CMakeLists.txt`, plus the four data files under `installer/linux/`. The cheapest installer to make real on this machine. | M |
| SB | 10 / 0.2: checksums, PGP-signed manifest, deterministic output | **missing** | none | A `scripts/release.sh` that builds, hashes, and writes `manifest.json`; signing key outside the repo. | S |
| UV | 6: first-run folder tree in one go, `config/plugin.json`, `.installed_version` | **partial** | lazy per-feature creation; `UiPreferences` writes its own json; no marker | A `Support/UserFolders.h` that creates the 18 subfolders on first load and writes the marker; `FirstRun::isFirstRun` reads it first. | S |
| UV | 5.1: update banner with release notes and Download to Downloads | **partial** | `Telemetry.h:72-80` carries per-platform download URLs; banner exists (UT-1-04) | Verify the banner's action opens the URL (untested); no auto-launch is already the case. | S |
| UV | 8: migrations back up the original to `Presets/Backup/<date>/` at load time; info banner on first affected load; `.mid` without the Luthier chunk loads as Generic | **partial** | ranges derivation (`Ranges::theRangesBlockRoundTripsAndDerivesWhenAbsent`), backup on save (`Presets::savingBacksUpTheVersionItReplaces`), guitar table (`GuitarMigration::everyPreM49NameResolvesToItsShippedGuitar`) | Load-time backup is TODO 2k (file-formats 2); the banner is one `Notification` from `PresetManager` when a migration ran. | S |
| UV | 4 / 9: standalone-only bundle; Windows portable `.zip` | **missing** | Standalone target builds | Both are packaging variants of the same outputs; add to the release script. | S |
| INT | 11: signed `.luthiercontent` packages into `ContentUpdates/` | **missing** | none | Post-1.0. | M |
| INT | 7: enterprise policy pre-placed | **partial** | policy reader verified (`Telemetry::policyOverridesTheUser`) | Only the installer side is missing. | - |
| INT | 13: install / uninstall / upgrade / downgrade / association / migration tests | **missing** | none | Follow the installers. | M |
| n/a | 2 / 2.2 / 2.3: macOS `.pkg`, notarisation, universal binary, `Uninstall.command` | **n/a on this platform** | no macOS machine or certificates (TODO 16) | Add `AU` to `FORMATS` now so the target at least configures when a Mac is available. | S |

**spec-coverage 31 corrections:** INS-3-01 `deferred - no Linux build` is stale; the row should be `pending` and the include.md conflict (C-18) re-read now that the tree builds on Ubuntu. INS-2-01's note "AU target not declared" is still true.

---

### gui-integration.md sections 20-22

| Sev | Requirement (§) | Status | Code | Gap and proposed fix | Size |
|---|---|---|---|---|---|
| UV | 20: `?` icon on every multi-row panel | **partial** | header `?` only (`HeaderBar.cpp:150`); `AdvancedPanel::showHelp` (`AdvancedPanel.cpp:1106`), `HelpTab::everyPanelNamePinsOneTopic` | See qa 4 above. | S |
| UV | 20: NEW dot for a week after a version introduces a feature | **missing** | - | See onboarding 13. | S |
| UV | 20: Diagnostics "What's on the audio path right now" live block diagram | **missing** | `OptionsPages.cpp` DiagnosticsPage has crash log, recorder, restore first-run | A row of labelled boxes (strings -> body -> pickup -> circuit -> pre -> amp -> cab -> room -> post -> master) with each lit from the engine's enabled flags at 4 Hz. | M |
| UV | 21: padlock on the WORKSHOP tab as well as CHARACTER | **partial** | `AdvancedPanel.cpp:1192-1197` makes a `RangeTabButton` for CHARACTER only ("WORKSHOP joins when it exists"; it exists) | Give WORKSHOP a `RangeTabButton` over the `RangeFamily` values (`PhysicalRange.h:29`) its inspector's fields belong to, once those fields carry ranges (TODO 7 lists the padlock). | S |
| UV | 21: slide bar overlay 6 px, material colour, 80 % opacity, slant, 80 ms ease | **implemented** | `FretboardComponent.cpp:173-183, 192, 372-382` | Reduced-motion exception missing (qa 4 above); a paint test asserting the rotated bar's colour is the material's would make it verified. | S |
| UV | 21: pick overlay at true size, rotated by attack angle, 8 px handles | **missing** | `GuitarRenderer.h` draws pickups, no pick (`rg -i "pick overlay|attack angle"`) | TODO 7 (4) and G (14) already list it. | M |
| UV | 21: noise-event strip hidden under reduced motion in favour of a static count | **partial** | `NoiseEventStrip` (`NoiseGroups.cpp:41-91`), verified moving (`NoiseUi::theEventStripShowsWhatTheEngineTriggered`); no reduced-motion branch | See qa 4 above. | S |
| INT | 22-01: startup walk of every section-19 row to a real component | **missing** | `HelpTab::everyPanelNamePinsOneTopic` walks panel names, not GI 19 rows | A table of (row, finder lambda) in `EditorTests.cpp`; fails on null. Most rows are now findable (TODO 7-13 remainders excepted). | M |
| INT | 22-02: every automation ID resolves to exactly one focused control | **missing** | - | Walk `getParameters()`, find `LuthierKnob` / `LuthierToggle` / `LuthierChoice` by parameter id in the editor tree, assert count == 1 (Easy / Advanced duplicates need a visible-only filter). | S |
| INT | 22-03: Tab-order walk in column order | **missing** | depends on the focus ring work | After qa 4's focus work. | S |
| INT | 22-04: reflow at 6 widths x 6 scales | **partial** | `EditorTests.cpp:233-235` (3 sizes, 100 %) | See qa 4. | S |
| INT | 22-05: empty-state hint per panel | **missing** | - | See qa 4. | S |
| INT | 22-06: all 13 right-click items present; range items state-correct | **partial** | range items verified (`RangesUi::rightClickUnlocksAndRestrictsOneControl`); menu has 10 of the 13 (qa 4) | After the three items are added, a test that opens the menu on every attached control and counts. | S |
| INT | 22-07: 1000-op undo random walk incl. Workshop swaps returns to the initial state | **missing** | - | Random `pushUndoState` / gesture / bench swap sequence, then `undo()` until `!canUndo()`, compare `captureStateBlock()` minus timestamps. Also proves Headline 1 once fixed. | M |
| INT | 22-08: 10 000 random clicks select the intended part by z-order | **partial** | `WorkshopPanel::everyFittedPartIsReachableAndNothingElseIs`, `Editor::everyHitRegionOnTheIllustrationDescribesItself` (grid sweeps) | Randomised sweep over the bench with the expected part from the renderer's own hit regions. | S |
| INT | 22-09: Slide Mode toggled 100x during playback, no click, correct panel visibility | **partial** | `Slide::switchingModeMidNoteIsClean` (one toggle), `SlideUi::theSlideGroupAppearsWithSlideModeAndTheTabFitsIt` | Loop the existing test 100 times with a peak-delta assertion. | S |

Verified: 20 first-unlock explainer text (`FirstRun::theRangeExplainerSaysSectionSevensWords`), 21 buzz heatmap dot glyph (`BuzzUi::heatmapCellsReadInMonochromeTerms`), 21 pressure readout (`SlideUi::pressureSaysWhatItMeans`), 21 circuit visualiser implemented at 15 Hz (`CircuitPanel.cpp:16`, `Circuit::theAudioPathMatchesTheResponse` covers the numbers), 22-10 warning arc (`RangesUi::controlsFollowASwappedRangeAndMarkTheValue`).

**spec-coverage 26 corrections:** GI-21-01's reason "no WORKSHOP tab" is stale; the tab exists and is simply not given the button (`AdvancedPanel.cpp:1193`). GI-21-02 "slant rotation unverified" - slant is implemented (`FretboardComponent.cpp:192, 382`); what is missing is the reduced-motion exception. GI-22-08 should also cite `WorkshopPanel::everyFittedPartIsReachableAndNothingElseIs`.

---

### Suggested order for TODO 14 / 14b fixes

1. Undo: exclude the tune from processor undo / A/B snapshots and add the
   processor-level test (SB, M). Then boundaries + Ctrl+Alt+Z + Ctrl+Y (M),
   then the missing push sites (snapshot recall first, S).
2. Support address and links (SB, S).
3. Localisation decision: narrow the picker to `en` / `en-GB` for 1.0 or
   commit to translating (SB for gate 2).
4. Reduced-motion exceptions (slide ease, noise strip), per-panel `?`,
   panel right-click menu, three missing control menu items, dialog focus
   and announcement (UV, all S).
5. Allocation counter wired into the test target and a `processBlock`
   zero-allocation test over every factory preset; latency-within-1-sample
   test; DC null at -100 dBFS (INT, S each).
6. Linux `.deb` / `.tar.gz` through CPack and a release script with
   checksums (SB, M) - the one installer this machine can produce and test.
7. Welcome banner + upgrade banner + NEW dots (UV, S), then the tour (L).
8. Relief ladder steps 1-4 and the > 96 kHz oversampling downgrade (UV, M + S).
