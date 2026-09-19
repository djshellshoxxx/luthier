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

## A1 — Advanced Mode column scheme does not match section 4

**Canonical.** Column 1 GUITAR/BODY/STRINGS/WHAMMY, Column 2 PICKUPS/CIRCUIT/
PRE-FX, Column 3 AMP/POST-FX/CAB/ROOM/SUSTAIN, Column 4 a tabbed workspace.

**Built.** `AdvancedPanel` uses its own scheme in which AMP/CAB/ROOM is called
"column 4" (`AdvancedPanel.h:162`). There is no tabbed workspace column, no
CIRCUIT panel (CABLE is still there) and no SUSTAIN panel.

SUSTAIN is built - see "Fixed since the first audit" - and currently sits after
ROOM in the column the build calls 4. Its *content* is right; only its column
number is wrong, and that moves with A1 rather than needing its own work.

**Size.** Large. A restructure of `AdvancedPanel`, not a patch.

## A2 — Column 4 has no tab strip; ten of its thirteen tabs do not exist

**Canonical order.**
`WORKSHOP | MOD | RHYTHM | TUNE | LIVE | ROUTING | TONE MATCH | CHARACTER | PRACTICE | NOTATION | MIDI OUT | CONTROLLERS | HELP`

**Built.** Three of the thirteen exist as panels, stacked vertically in a
scrollable column rather than tabbed (`AdvancedPanel.cpp:775-806`):

| Tab | State |
|---|---|
| WORKSHOP | blocked on `guitar-workshop.md` / `workshop-ui.md` |
| MOD | `ModMatrixPanel` exists, stacked not tabbed |
| RHYTHM | `RhythmPanel` exists, stacked not tabbed; STRUM group blocked |
| TUNE | not built (`tune-builder.md` is on disk, so unblocked but large) |
| LIVE | no setup surface; `LiveStrip` is the runtime surface only |
| ROUTING | `RoutingPanel` exists, stacked not tabbed |
| TONE MATCH | `ToneMatchPanel` exists, stacked not tabbed |
| CHARACTER | `CharacterPanel` exists, stacked not tabbed |
| PRACTICE | no setup surface; the drawer is the runtime surface only |
| NOTATION | not built — no live TAB view or chord-symbol history surface |
| MIDI OUT | not built |
| CONTROLLERS | exists, but in the Options overlay, which section 19 says is wrong |
| HELP | exists as an overlay, not as a Col 4 tab |

Also unimplemented: "last-used tab persists across sessions in the plugin's
user-global settings" (section 4.4).

**Size.** Large, and blocked on A1 for the column itself.

## A3 — Options overlay tabs do not match section 5

**Canonical.** Eleven tabs:
`AUDIO | MIDI | APPEARANCE | ACCESSIBILITY | LOCALIZATION | EXPRESSION | RANGES | UPDATES | PRIVACY | DIAGNOSTICS | FILE LOCATIONS`

**Built.** Five: `GENERAL | CONTROLLERS | EXPRESSION | ACCESSIBILITY | PRIVACY`.

This is work from the session that produced this audit, and it does not match
the canonical map. Everything is reachable; the structure is wrong.

| Canonical tab | Where it is now |
|---|---|
| AUDIO | GENERAL (a button explaining the host owns the devices) |
| MIDI | nowhere |
| APPEARANCE | split between GENERAL (tooltips) and ACCESSIBILITY (palette, scale, motion, font) |
| ACCESSIBILITY | ACCESSIBILITY, mixed with APPEARANCE and LOCALIZATION content |
| LOCALIZATION | ACCESSIBILITY |
| EXPRESSION | EXPRESSION — matches |
| RANGES | blocked on `advanced-ranges.md` |
| UPDATES | PRIVACY |
| PRIVACY | PRIVACY — matches, with UPDATES folded in |
| DIAGNOSTICS | the separate Debug overlay, reached from Help |
| FILE LOCATIONS | GENERAL |
| *(CONTROLLERS)* | should not be here — section 19 puts it in Col 4 |

The class split is cleaner than the tab count suggests: `AccessibilityPage`
already lays its controls out in three separate groups matching APPEARANCE,
LOCALIZATION and ACCESSIBILITY, and `PrivacyPage` in two matching UPDATES and
PRIVACY. Splitting them is mostly moving members, not rewriting layout.

**Caution.** CONTROLLERS cannot simply be deleted from Options — its Col 4 home
does not exist yet (A2), and removing it would strand controller setup entirely.
It has to move, not disappear, and the move is blocked on A1.

**Size.** Medium, and the only structural gap that is fully actionable today.

## A4 — Section 19 rows whose secondary access is absent

Primary locations exist for everything not listed under "What this blocks".
These secondary paths do not:

- **Easy mode instrument interactions** — headstock click (tuning), body click,
  bridge click, pickup click. `GuitarBodyComponent` draws the instrument but is
  not hit-tested as a control surface. Section 3.1 requires it.
- **Right-click → Modulate** on any control. The right-click menu exists (MIDI
  Learn, value entry); the Modulate entry and drag-to-assign do not. Affects
  seven mod-matrix rows. Note ground rule 4: right-click may not be the *only*
  path, so the MOD tab cards remain the primary surface.
- **Header notification** for an available update.
- **Post-crash prompt** for crash reporting.
- **Help > About** as a route to license.

## A5 — Shortcuts in section 17 not audited row by row

Section 17 is the canonical binding list. `docs/KEYBOARD_SHORTCUTS.md` and
`PluginEditor::keyPressed` have not been checked against it. Section 19 alone
names `Ctrl+O`, `Ctrl+G`, `Ctrl+L`, `Ctrl+/`, `Ctrl+S`, `Ctrl+E`, `P`, `Ctrl+R`,
`Ctrl+Shift+R`, `F1`.

**Size.** Small. Worth doing as its own pass because it is cheap and it is the
kind of thing that silently rots.

## Fixed since the first audit

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

1. **A3**, as far as it can go — split the Options pages along the canonical
   seams, add AUDIO, MIDI, DIAGNOSTICS and FILE LOCATIONS, leave RANGES out and
   CONTROLLERS in place with a comment saying why.
2. **A5** — the shortcut audit. Cheap, independent.
3. **A1**, restricted to what is unblocked: the column restructure. The
   SUSTAIN content itself is now built and sits after ROOM; it needs moving with
   the rest of the column, not rebuilding.
4. **A2** once A1 lands, for the tabs that are not blocked: LIVE, PRACTICE,
   NOTATION, MIDI OUT, CONTROLLERS, HELP.
5. **A4** — secondary access paths, mostly independent.
6. Everything else waits on the eleven missing specs.
