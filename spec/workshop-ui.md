# WORKSHOP UI SPEC

The bench. Where a user takes the guitar apart.

`guitar-workshop.md` gives the parts model and `part-acoustics.md` gives
what each part does. This file specifies the surface: a live-drawn guitar
whose every part is a hit region, an inspector for the selected part, a
parts drawer, a setup strip and a spectrum-delta pane that shows what a
change actually did.

The bench has one job that no other panel has: **making a physical change
legible**. A user drags the neck pickup 8 mm towards the bridge and needs
to see the comb notches move, hear the difference, and read the new
number. If any one of those three is missing, the Workshop becomes a
folder of dropdowns.

## 0. Ground rules

1. **Direct manipulation first.** Every part that has a position, a height
   or a depth is dragged on the illustration. Dropdowns and number fields
   are the precise path, not the primary one.
2. **The illustration is authoritative** (`guitar-illustration.md` 0.5).
   What is drawn is what is loaded. There is never a visible pickup the
   engine does not have.
3. **Every visible interaction has three feedbacks**: **visual** (the
   illustration changes), **numeric** (the inspector shows the new value),
   and **audible** (audition, or the next note played). A change that
   produces fewer than three is not finished.
4. **Audition never commits.** Alt-hover previews on a shadow
   `GuitarSpec`; releasing discards it. Nothing the user only hovered over
   can end up saved.
5. **Every committed change is one undo entry** with a sentence describing
   it in real units.
6. **The bench is not modal.** The instrument keeps playing while it is
   open. A user auditions bridges while holding a chord.

## 1. Layout

Fixed by `gui-integration.md` 6. The bench takes over Advanced columns
3 + 4; in Easy mode it is an overlay opened by the header wrench.

```
+---------------------------------------------------------------+
| WORKSHOP  [guitar name (modified)]  [Save As Guitar]  [A/B]   |
+-----------------------------------------------------+---------+
|         GUITAR ILLUSTRATION (interactive)           |INSPECTOR|
|      body, neck, headstock, every part hit-tested   |(selected|
|   [ruler: mm from saddle, pickup rail]              |  part)  |
+-----------------------------------------------------+         |
| PARTS DRAWER: Body | Neck | Frets | Nut | Bridge |  |         |
|   Tuners | Strings | Pickups | Wiring | Preamp |    |         |
|   Pick | Slide | Capo                               |         |
+-----------------------------------------------------+         |
| SETUP STRIP: action H/L, relief, nut depth |    SPECTRUM      |
|                                            |    DELTA         |
+---------------------------------------------------------------+
```

Minimum bench width is 900 points. Below that the inspector collapses to
a drawer opened from the selected part, and below 700 the parts drawer
becomes a dropdown. The bench is not available at all below the Advanced
minimum of 1000 (`gui-integration.md` 4.5) in Advanced mode, but the Easy
overlay works down to the window minimum because it is not sharing space.

## 2. Rendering

`ui-wiring.md` 2's `GuitarIllustration` renders procedurally from the
`GuitarSpec` (`guitar-illustration.md` is the deep spec). The bench adds:

- **The ruler**: millimetres from the saddle along the string axis, drawn
  under the pickup rail. It is what makes pickup position a measurement
  rather than a feeling.
- **Hit-region outlines** on hover and selection (section 3).
- **Live overlays**: the pick at its angle, the slide bar at its slant, the
  capo at its fret, the buzz heatmap when the setup strip has focus.

Repaint budget: full repaint under 8 ms, live-overlay repaint under 2 ms
(`guitar-illustration.md` 0.7). Overlays are a separate layer so that
dragging the pick does not repaint the body.

## 3. Selection and hover

### 3.1 The hover rule

Hovering a part outlines it in the accent colour at 60% and shows its name
and one summary value in a tooltip ("PAF 57 — 7.6k, alnico 2"). Hovering
does **not** select, and does not change the inspector: a user sweeping
the mouse across the guitar on the way to something else must not have the
inspector flicker through six parts.

Selection is a click, is sticky, and draws a full-strength outline.

### 3.2 Alt-hover audition

Holding Alt while hovering a **part card in the drawer** auditions that
part on a shadow spec (`ui-wiring.md` 6.3). The illustration draws the
candidate, the inspector shows its fields greyed, and the spectrum delta
shows candidate-versus-committed. Releasing Alt or moving off the card
crossfades back over 30 ms.

Audition is the feature that makes the drawer usable: twelve bridges is a
list until you can hear them by pointing at them.

### 3.3 Per-string selection

Clicking a string selects that string. The inspector then shows the string
set **and** that string's override fields, so a user can put a heavier
third on without building a new set. An overridden string is drawn in the
override's material colour (`guitar-illustration.md` 608).

## 4. Hit regions and dragging

Every part is a hit region (`guitar-illustration.md` 792). Regions are
generated from the same geometry that draws the part, so a part that moves
takes its region with it.

| Part | Gesture | Snap | Range |
|---|---|---|---|
| Pickup | Drag along string axis | 1 mm, Shift for 0.1 mm | Between neighbouring parts |
| Pickup height | Drag the screw handles, or scroll | 0.1 mm | 0.5 – 6.0 mm |
| Pickup tilt | Drag one screw handle alone | 0.1 mm differential | ±2 mm treble-to-bass |
| Bridge saddles | Drag along axis | 0.1 mm | ±6 mm intonation |
| Nut slots | Drag down per string | 0.05 mm | 0 – 1.2 mm |
| Frets | Click to select; brush to wear | – | Wear 0 – 1 |
| Pick | Drag along string axis; rotate at corner | 1 mm / 1° | Angle 0 – 89° |
| Slide | Drag position; rotate for slant | 1 mm / 1° | Slant ±60° |
| Capo | Drag along neck | 1 fret | Fret 0 – 12 |

Drag rules:

- **Constrained to the axis that means something.** A pickup drags along
  the strings, not across them, because across is not a thing you can do.
- **Snap is on by default**, Shift is fine, Alt is free.
- **The live value follows the pointer in the inspector**, not on release.
- **While dragging a pickup**, the spectrum delta draws the comb notches
  for the candidate position live (`gui-integration.md` 6).
- **A drag that would collide** (a pickup into the bridge) stops at the
  limit and the inspector shows why.

## 5. The inspector

Shows the selected part: its name, its library origin (factory or user),
every field with its unit, and the part's compatibility.

- Fields are editable. Editing a **factory** part marks the guitar
  modified and offers "Save as user part"
  (`guitar-workshop.md` 7); it never writes to the factory file.
- A **Swap** control opens the drawer filtered to that part type.
- A **Revert** control restores the slot to what the loaded guitar had.
- Fields carry the same tooltips and accessibility treatment as
  parameters, even though they are not parameters.

Part fields are **not** `PhysicalRange` parameters, so they have no
stock/advanced marking. Their limits come from `part-acoustics.md`'s
tables and are enforced as plain clamps. A user who wants a 20 H pickup
sets it and gets whatever that is; the Workshop is already the advanced
surface.

## 6. Spectrum delta

The pane that makes the whole thing honest.

- A fixture render (a fixed pluck on a fixed string at a fixed velocity)
  is run against the **committed** spec and the **candidate** spec, and
  the magnitude difference is drawn in dB against frequency.
- Flat at 0 dB means the change did nothing. That outcome is shown
  plainly, because a Workshop that always draws an impressive curve is
  lying.
- Y axis is fixed at ±12 dB by default, with an auto-zoom toggle. Fixed by
  default so that a 0.4 dB change **looks** like a 0.4 dB change
  (`INDEX.md`'s "honest magnitudes").
- Runs on a message-thread worker pool, never on the audio thread.
  **Budget: 40 ms per delta** (`ui-wiring.md` 6.4).
- Coalesced while dragging: at most one render in flight, the latest
  request wins.
- During a pickup drag it additionally draws the comb-notch positions for
  the candidate.

## 7. A/B slots

Eight `GuitarSpec` slots in the bench header strip.

- Click stores the current committed spec; click again recalls it.
- Recall is a full guitar swap (`ui-wiring.md` 6.1) and is undoable.
- Slots live in `uiState`, not the preset, because they are a workspace.
- Shift-click clears a slot.

Eight, not two, because the comparison a builder wants is rarely binary -
four bridges against two pickup positions is the real shape of the
question.

## 8. Undo

Every committed change is one entry, described in real units
(`ui-wiring.md` 395, `action-and-undo.md`):

- "Moved neck pickup 150 → 142 mm"
- "Fitted ABR-1 Tune-o-matic (was Hardtail)"
- "Raised bridge pickup treble side 2.4 → 2.1 mm"
- "Set string 3 to 0.018 plain (was 0.017 plain)"
- "Recalled bench slot B"

Grouping: a drag is one entry, opened on mouse-down and closed on
mouse-up, per `action-and-undo.md`'s gesture rule. A part swap is never
grouped with a neighbouring swap.

Audition never pushes an entry (ground rule 4).

## 9. Empty and blocked states

- **No user parts yet**: the drawer's user section reads "Your saved parts
  appear here. Edit any factory part and Save As to start."
- **Slide category with Slide Mode off**: shown greyed with "Turn on Slide
  Mode (S) to fit a slide."
- **A part type with one option**: the drawer still shows it, so the
  category is never an empty box.
- **Incompatible part hovered**: the warning from `guitar-workshop.md` 5
  appears in the card, and the audition still works.

## 10. Accessibility

- Every hit region is a focusable element with a name and a value, in the
  order a builder would work: body, neck, fretboard, frets, nut, bridge,
  tailpiece, tuners, pickups (neck to bridge), wiring, strings, pickguard.
- Arrow keys nudge the selected part by one snap unit; Shift-arrow by the
  fine unit.
- The spectrum delta is announced as a summary ("candidate is 1.8 dB
  brighter above 2 kHz") rather than as a curve, because a curve has no
  screen-reader representation worth having.
- Everything reachable by drag is reachable by keyboard.

## 11. Tests

- **Every part is hit-testable.** Sweep a grid over the illustration for
  every factory guitar and assert every slot in the spec is reachable by
  at least one point, and that no point reports a part the spec does not
  have.
- **Hover does not select.** Move across five parts; assert the inspector's
  subject is unchanged and no undo entry was pushed.
- **Audition does not commit.** Alt-hover a different bridge, release;
  assert the committed spec is unchanged, the undo stack is unchanged, and
  the audio returns to the committed render within 30 ms.
- **Drag produces one undo entry** with the correct before and after
  values in its description.
- **Drag is constrained.** Dragging a neck pickup towards the bridge stops
  before overlapping it, and the inspector reports the limit.
- **Snap works.** A drag of 7.4 mm with snap on lands on 7 mm; with Shift
  it lands on 7.4.
- **The three feedbacks are present.** For each draggable part: assert the
  illustration's rendered digest changed, the inspector's displayed value
  changed, and the rendered audio changed.
- **Spectrum delta is flat for a null change.** Auditioning a part against
  itself produces a curve within ±0.05 dB of zero everywhere.
- **Spectrum delta stays inside 40 ms** for every factory part swap.
- **Nothing runs on the audio thread**: no filesystem, no allocation, no
  spectrum work during any bench interaction.
- **A/B recall round-trips.** Store, change six parts, recall, assert the
  spec is identical to what was stored.
- **Keyboard parity.** Every drag interaction produces the same committed
  result when driven by arrow keys.
