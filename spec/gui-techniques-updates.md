# GUI TECHNIQUES TAB DELTA

Additive changes to `gui-integration.md`. No existing rows in the
feature-to-location index are removed or moved. Existing panels are
extended with technique-arm affordances where noted. All prior
locations remain valid.

## 0. Ground rules

1. **Additive only.** No feature previously placed loses its
   location.
2. **The new tab sits in Column 4.** Column 4 tab order becomes:
   WORKSHOP | MOD | RHYTHM | TUNE | LIVE | ROUTING | TONE MATCH |
   CHARACTER | PRACTICE | NOTATION | MIDI OUT | CONTROLLERS |
   **TECHNIQUES** | HELP.
3. **Easy Mode gains a Techniques pill row.** Compact controls in
   the existing Playing strip.
4. **Modern layout inside the Techniques tab.** Vertical sub-tabs on
   the left, live preview and controls on the right.

## 1. Techniques tab structure

Column 4, new tab labelled "TECHNIQUES". Layout:

```
+-----------------------------------------------------------------+
| [tab strip: ... | CONTROLLERS | TECHNIQUES | HELP]              |
+-----------------------------------------------------------------+
| [sub-tab rail]  |  [active technique panel]                     |
| SCRAPE          |                                               |
| SLIDE           |   [technique controls + live indicators]      |
| SLAP            |                                               |
| MUTE            |                                               |
| TAP             |                                               |
| BEND            |                                               |
| CASCADE         |                                               |
+-----------------+-----------------------------------------------+
```

Sub-tabs:
- **SCRAPE**: controls from `string-scraping.md` 2.
- **SLIDE**: controls from `slide-technique-controls.md` 1 (the
  Slide Mode toggle stays in the header per gui-integration.md).
- **SLAP**: controls from `string-slap-technique.md` 1.
- **MUTE**: 16-step grid editor plus controls from
  `muting-rhythm.md` 3.
- **TAP**: controls from `two-hand-tapping.md` 3.
- **BEND**: controls from `microtonal-bends.md` 2.
- **CASCADE**: overview visualising which techniques are active on
  which strings, plus compatibility conflicts from
  `technique-cascade.md` 6.

Each sub-tab uses the standard AttachedKnob pattern (per
`ui-wiring.md` 2). Each sub-tab has a "arm" pill at the top so the
user can enable / disable the technique globally without reaching
into the controls.

## 2. Easy Mode Playing strip additions

Existing Playing strip in Easy Mode gains a "Techniques" pill row
below the existing tool selector. Six pills, one per new technique:

```
[SCRAPE] [SLIDE] [SLAP] [MUTE] [TAP] [BEND]
```

Behaviour:
- Tap once: arms the technique globally (equivalent to the arm pill
  in the sub-tab). A single tap on any pill toggles arm state.
- Hold: opens a popover with the top 3-5 controls for that
  technique. Non-modal; hitting Escape closes.
- Right-click / long-press: opens the full sub-tab in Advanced Mode.

Modern design cues: rounded pills, filled with primary accent when
armed, outlined when not. Small state indicator dot next to each
armed pill flashes when the technique fires (a scrape event, a slap
hit, etc).

## 3. Header additions

No changes to the header layout. The existing wrench (Workshop) and
slide-glyph (Slide Mode toggle) remain. The new TECHNIQUES tab is
one click from the header via the Column 4 tab strip.

## 4. Fretboard illustration extensions

The fretboard already renders played-notes, buzz heatmap, slide bar,
pick position, pickup pulse (per `gui-engine-dataflow.md` 6). New
overlays, each independently toggleable:

- **Scrape trail**: while a scrape is active, a fading line along
  the string shows the scrape path with per-catch tick marks.
- **Tap markers**: taps render as small squares (distinct from
  played-note dots) at their positions; released taps fade over
  100 ms.
- **Mute-zone shading**: palm-mute zone renders as a translucent
  band across the strings behind the bridge.
- **Bend arc**: bent notes show a curved arc from fretted position
  to current pitch, with a cents-offset badge.
- **Slap impact indicator**: slap events flash the bridge area
  briefly.

Layer order continues from `guitar-illustration.md` 5 at layer 33+.
Live overlay repaint stays under the existing 2 ms budget.

## 5. Advanced Mode Col 3 CHARACTER updates

Existing CHARACTER tab groups gain modest additions:
- **Right Hand** group (from `fingerstyle-attack.md`): gains a
  "Tapping" section (from `two-hand-tapping.md` 6).
- **PLAYING** group: gains a "Microtonal" section (from
  `microtonal-bends.md` 5).

These are convenience mirrors; the primary location for each is
the Techniques tab.

## 6. Rhythm engine pattern editor update

Advanced Mode Col 4 RHYTHM tab: existing pattern editor gains a
new "Mute Row" showing `mute_type` per step (from `muting-rhythm.md`
7). Patterns without `mute_type` show all-open by default; users
can paint mute types per step.

## 7. Preset browser update

The existing preset browser gains a filter chip: "Uses Techniques".
Multi-select: user can filter presets that arm any of the six new
techniques. No changes to preset browser layout.

## 8. Onboarding update

The first-run tour (per `onboarding.md`) gains one optional stop
at the Techniques tab, introducing the six pill-armable techniques
and pointing at the cascade view. Skippable like every other tour
stop.

## 9. Accessibility

All new controls follow `accessibility.md`:
- Screen-reader labels for each pill: "Scrape technique, armed" /
  "not armed".
- Keyboard: Tab through pills, Space to arm / disarm, Enter to
  open sub-tab.
- Colour: pill armed state uses both fill and a small icon indicator
  (not colour-only).
- Reduced motion: overlay animations become instant state changes.

## 10. Modern design cues

- Pills use the theme's rounded-corner radius (already defined in
  `theme.md`).
- Sub-tab rail uses vertical text with generous padding and a
  subtle divider from the panel area.
- Live indicators use small dot glyphs that pulse rather than
  large flashing regions.
- CASCADE sub-tab uses a strings-across, techniques-down grid to
  show at-a-glance which combinations are active per string.

## 11. Feature-to-location index additions

Add to gui-integration.md section 19:

| Feature | Location |
|---|---|
| String scraping controls | Col 4 TECHNIQUES tab, SCRAPE sub-tab; Easy Mode Playing strip SCRAPE pill |
| Slide user controls (source, range, scripting) | Col 4 TECHNIQUES tab, SLIDE sub-tab |
| Slap type + controls | Col 4 TECHNIQUES tab, SLAP sub-tab; Playing strip SLAP pill |
| Muting grid + rhythm | Col 4 TECHNIQUES tab, MUTE sub-tab; RHYTHM tab pattern editor Mute Row |
| Two-hand tapping | Col 4 TECHNIQUES tab, TAP sub-tab; Playing strip TAP pill |
| Microtonal bends | Col 4 TECHNIQUES tab, BEND sub-tab; Playing strip BEND pill |
| Cascade overview | Col 4 TECHNIQUES tab, CASCADE sub-tab |

## 12. Tests

- Every sub-tab renders with correct controls at 1280x800.
- Every pill in Playing strip arms / disarms its technique on
  single click.
- Long-press opens the popover; Escape closes.
- CASCADE view correctly reflects live technique arm state.
- Fretboard overlays render within 2 ms live-overlay budget.
- Onboarding tour reaches the Techniques tab without breaking
  existing tour flow.
- All existing gui-integration.md tests still pass (no regressions).
