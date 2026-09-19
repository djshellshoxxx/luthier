# GAP LIST — current build vs `gui-integration.md`

Produced per `CLAUDE_CODE_BRIEF.md` step 2: audit the build against
`gui-integration.md` section 18, record every row whose UI location does not
exist, fix nothing yet.

Audited at commit `996f89d`. `gui-integration.md` is the single source of truth
for where a feature lives; where this build disagrees, this build is wrong.

## Summary

Every backend feature in the section 18 index **is reachable** in the current UI.
Nothing is stranded with no way to get to it. What does not match is the
**structure**: the Advanced layout and the Options overlay both predate
`gui-integration.md` and organise the same features differently from the
canonical map.

So this is not a "features missing from the GUI" list. It is a "features in the
wrong place" list, plus five Column 4 tabs that genuinely do not exist.

## G1 — Advanced Mode column scheme does not match section 4

**Canonical.** Four columns. Column 1 GUITAR/BODY/STRINGS/WHAMMY, Column 2
PICKUPS/CABLE/PRE-FX, Column 3 AMP/POST-FX/CAB/ROOM, Column 4 a tabbed
workspace.

**Built.** `AdvancedPanel` uses its own column scheme in which AMP/CAB/ROOM is
called "column 4" (see the `// --- column 4 ---` comment at `AdvancedPanel.h:162`).
There is no tabbed workspace column at all.

**Effect.** Column 3 and Column 4 of the spec do not exist as such. The widths
and reflow rules in section 4.5 have nothing to apply to.

**Size.** Large. This is a restructure of `AdvancedPanel`, not a patch.

## G2 — Column 4 has no tab strip; five of its ten tabs do not exist

**Canonical.** A tab strip across the top of Column 4, in this fixed order:

`MOD | RHYTHM | LIVE | ROUTING | TONE MATCH | CHARACTER | PRACTICE | NOTATION | CONTROLLERS | HELP`

**Built.** Five of the ten exist as panels, and they are stacked vertically in a
scrollable column rather than tabbed (`AdvancedPanel.cpp:775-806`):

| Tab | Panel | State |
|---|---|---|
| MOD | `ModMatrixPanel` | exists, stacked not tabbed |
| RHYTHM | `RhythmPanel` | exists, stacked not tabbed |
| LIVE | — | **missing as a Col 4 tab** |
| ROUTING | `RoutingPanel` | exists, stacked not tabbed |
| TONE MATCH | `ToneMatchPanel` | exists, stacked not tabbed |
| CHARACTER | `CharacterPanel` | exists, stacked not tabbed |
| PRACTICE | — | **missing as a Col 4 tab** |
| NOTATION | — | **missing as a Col 4 tab** |
| CONTROLLERS | — | **missing as a Col 4 tab** |
| HELP | — | **missing as a Col 4 tab** |

The five missing ones are not missing features. LIVE has `LiveStrip` (the
runtime surface; section 4.4 asks for a separate *setup* surface). PRACTICE has
the drawer (`PracticePanel`, eight tabs, again the runtime surface; the spec asks
for a larger setup mirror). CONTROLLERS currently lives in the Options overlay,
which section 18 says is the wrong place. HELP exists as an overlay. NOTATION is
the only one with no Column 4 surface of any kind — its export path exists but
the live TAB view and chord-symbol history have no tab to live in.

Also unimplemented: "last-used tab persists across sessions in the plugin's
user-global settings" (section 4.4, final line).

**Size.** Large, and blocked on G1.

## G3 — Options overlay tabs do not match section 5

**Canonical.** Ten tabs:

`AUDIO | MIDI | APPEARANCE | ACCESSIBILITY | LOCALIZATION | EXPRESSION | UPDATES | PRIVACY | DIAGNOSTICS | FILE LOCATIONS`

**Built.** Five tabs: `GENERAL | CONTROLLERS | EXPRESSION | ACCESSIBILITY | PRIVACY`.

This is my own work from this session and it is worth being blunt that it does
not match the canonical map. The mapping:

| Canonical tab | Where it is now |
|---|---|
| AUDIO | GENERAL (a button that explains the host owns the devices) |
| MIDI | nowhere |
| APPEARANCE | split: tooltips in GENERAL, palette/scale/motion/font in ACCESSIBILITY |
| ACCESSIBILITY | ACCESSIBILITY, mixed with APPEARANCE and LOCALIZATION content |
| LOCALIZATION | ACCESSIBILITY |
| EXPRESSION | EXPRESSION — matches |
| UPDATES | PRIVACY |
| PRIVACY | PRIVACY — matches, with UPDATES folded in |
| DIAGNOSTICS | the separate Debug overlay, reached from Help |
| FILE LOCATIONS | GENERAL |
| *(CONTROLLERS)* | **should not be here at all** — section 18 puts it in Col 4 |

**Effect.** Everything is reachable; four canonical tabs do not exist as tabs,
two are merged into one, and one tab exists that the spec does not want here.

**Size.** Medium. The page classes in `OptionsPages.cpp` are written and working;
this is re-splitting them along the canonical seams and adding AUDIO, MIDI,
DIAGNOSTICS and FILE LOCATIONS pages.

## G4 — Section 18 rows whose secondary access is absent

Primary locations all exist. These secondary paths do not:

- **Easy mode instrument interactions** — headstock click (tuning), alt-click
  (temperament), body click, nut click, bridge click, pickup click and drag.
  `GuitarBodyComponent` draws the instrument; it is not clickable as a control
  surface. Affects eight rows.
- **Right-click → Modulate** on any control, to reach LFOs/envelopes/steps/
  followers/random. The right-click menu exists (MIDI Learn, value entry); the
  Modulate entry and drag-to-assign do not. Affects seven rows.
- **Header notification** for an available update. Affects one row.
- **Post-crash prompt** for crash reporting. Affects one row.
- **Help > About** as a route to license. Affects one row.
- **Column 4 HELP tab** as secondary access to Help. Covered by G2.

## G5 — Shortcuts in section 16 not verified against the build

Section 16 is described as the canonical binding list. `docs/KEYBOARD_SHORTCUTS.md`
and `PluginEditor::keyPressed` were not audited row by row against it in this
pass. Section 18 alone names `Ctrl+L`, `Ctrl+O`, `Ctrl+/`, `Ctrl+S`, `P`,
`Ctrl+R`, `Ctrl+Shift+R`, `F1`, `[`, `]`, `1-9`, `PageUp/PageDown`, `T`, `\`.

**Size.** Small, but do it as its own pass.

## Not audited yet

`ambiguity-resolutions.md`, `ui-wiring.md`, `onboarding.md`,
`performance-budget.md`, `qa-polish.md` and `installer.md` have not been read
against the build. Section 19 (first-run), section 20 (discoverability) and
section 21 (tests) of `gui-integration.md` are also unaudited.

## Suggested order

1. **G3** — self-contained, medium, and it corrects work from this session that
   is currently wrong against the canonical map.
2. **G1** — the structural restructure everything else sits on.
3. **G2** — the Col 4 tab strip and the five missing tabs, once G1 lands.
4. **G4** — the secondary access paths, which are mostly independent.
5. **G5** — the shortcut audit.

Do not start G2 before G1; the tab strip has no column to live in until the
column scheme is right.
