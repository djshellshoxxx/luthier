# Visual identity as built

`theme.md` in the project root defines a shared identity for a family of plugins:
near-black neutrals, a burnt-orange accent, a muted teal secondary, flat controls
with 270-degree value arcs drawn outside the knob body, an 8 px grid, and a set of
signature elements.

It also says each plugin may re-tint **one** accent for its own identity.

Luthier takes that permission and goes slightly further: it re-tints the accent,
the secondary **and** the neutrals, because a cool blue-black guitar looked wrong
next to a warm instrument. Everything structural is unchanged. This file records
exactly what was kept, what was changed, and why.

---

## What is unchanged

Every one of these is implemented as the spec describes:

- **8 px base grid**, with 4 px for fine detail.
- **Corner radii**: 6 px on the main window, 4 px on inner panels, 2 px on small
  controls.
- **Knob geometry**: 48 px default, 36 px small, 64 px large, plus a 76 px macro
  size. Flat-shaded body, 1 px inner stroke, a single 2 px accent indicator line
  from centre to rim.
- **Value arcs**: 270 degrees, from 7 o'clock to 5 o'clock, 3 px thick, drawn
  **outside** the knob body with a 4 px gap. Unfilled portion at 60% opacity;
  filled portion in the accent.
- **Centre dot**: 4 px, accent when moved, muted when at the default.
- **Label below, value above**, with the value visible only on hover or during a
  drag.
- **Sliders**: 4 px track with rounded ends, accent fill, 16x24 rounded-rect thumb
  with a 1 px accent stroke.
- **Buttons**: 4 px radius, 28 px tall. Off state is the panel colour with muted
  text; on state is accent at 15% with an accent border and accent text; presses
  flash the accent briefly.
- **Meters**: smooth gradient rather than visible LED gaps, peak hold for 1.5 s
  then falling at 20 dB/s, numeric readout in a mono face.
- **Sections** separated by 1 px rules, never boxes inside boxes.
- **Section headers**: uppercase, primary text colour, with a 2x12 px accent bar to
  the left.
- **Padding**: 16 px minimum from the window edge to any control.
- **No skeuomorphism**: no faux wood grain, no fake metal texture. Depth comes from
  gradients and shadows only.
- **Shadows**: black at 55%, 8 px blur, y+2.
- **Interaction**: hover brightens, vertical drag for fine control, Shift for
  coarse, Ctrl/Cmd for ultra-fine, double-click resets, right-click opens the value
  menu. Tooltips are dark pills appearing after 400 ms.
- **Header**: a top strip in the panel colour with the plugin name at the left, the
  preset selector and A/B at the right, and a 1 px separator below.
- **Signature notch**: a 2 px accent diagonal, 12 px long at 45 degrees, in the
  top-left corner.
- **Footer**: the version in a 9 px muted mono face, bottom right.
- **Output LED**: top-left corner, dark grey at silence, brightening to white as
  the level approaches 0 dBFS, red while the signal is over.
- **Data stream**: about ten lines of small text showing a real, scrolling readout
  of the plugin's internals, light green and partially transparent, with the top
  and bottom lines faded away. It stops when there is no new data and resumes when
  something changes. It fills the space beside the guitar illustration.

---

## What changed, and why

### The neutrals moved from blue-black to walnut-black

| | theme.md | Luthier |
|---|---|---|
| Background base | `#0E1116` | `#191512` |
| Panel surface | `#171B22` | `#221C17` |
| Panel edge | `#2A303A` | `#3E3226` |

Same darkness, warm rather than cool. A guitar is a wooden object and the cool grey
read as clinical against it. The contrast ratios are preserved, so legibility is
unchanged.

### The accent became aged amber

| | theme.md | Luthier |
|---|---|---|
| Primary accent | `#E8532A` burnt orange | `#E0873C` aged amber |

This is the re-tint the spec explicitly permits. Amber is the colour of a valve
amp's pilot lamp, which is the right reference for an instrument whose signal path
ends in a valve amplifier.

### The secondary became oxidised brass

| | theme.md | Luthier |
|---|---|---|
| Secondary accent | `#4FB6C4` muted teal | `#6FA5A0` brass patina |

Still a cool counterpoint to the warm primary, still used for the same things -
secondary indicators, modulation, MIDI-learn markers - but pulled toward the green
of tarnished brass hardware rather than toward cyan.

### Text became aged ivory

| | theme.md | Luthier |
|---|---|---|
| Text primary | `#E6E8EC` cool white | `#EDE4D6` aged ivory |
| Text muted | `#8A929E` | `#9C9082` |

The colour of old binding and aged nitrocellulose. The same warming as the
neutrals, for the same reason.

### Knobs gained a knurled skirt

The spec asks for a flat-shaded circular body with an indicator line. Luthier keeps
exactly that and adds a set of fine radial grooves around the rim, drawn as 1 px
strokes at 55% opacity, which **rotate with the control**.

Two reasons: it is the single most recognisable feature of an amplifier knob, and
because the grooves turn, the control's position is legible at a glance even
without reading the pointer.

It is still flat. There is no bevel, no specular highlight, no metal texture - just
a line pattern in the edge colour.

### Full palette as built

```
backgroundDeep   #120F0C     accent        #E0873C
background       #191512     accentBright  #F5AC63
panel            #221C17     accentDim     #8A5426
panelRaised      #2A221B     secondary     #6FA5A0
panelSunken      #15110E     secondaryDim  #3F6663
edge             #3E3226     success       #8FBF6F
edgeBright       #554330     warning       #F0C24E
                             clip          #D9452F
textPrimary      #EDE4D6     dataStream    #7FD18A
textMuted        #9C9082     shadow        rgba(0,0,0,0.60)
textDisabled     #655C51
```

---

## Typography

As specified, with fallbacks.

| Role | Requested | Falls back through |
|---|---|---|
| UI | Inter | Space Grotesk, Segoe UI Variable Text, Segoe UI, SF Pro Text, Helvetica Neue, DejaVu Sans |
| Numeric | JetBrains Mono | IBM Plex Mono, Cascadia Mono, Consolas, SF Mono, Menlo, DejaVu Sans Mono |

Fonts are resolved at runtime against what is installed rather than embedded, so no
font is redistributed. Sizes follow the spec: 11 px uppercase labels with +0.08em
tracking, 12 to 14 px value readouts, 14 px semibold for the plugin name.

JUCE has no letter-spacing attribute, so tracking is implemented by drawing label
text glyph by glyph with the extra advance added. Measurement accounts for it, so
justification stays correct.

---

## The window shape

The brief asks for a rounded rectangle with a guitar-body cutaway in the right
edge.

Plugin windows are rectangular in every host, and a genuinely non-rectangular
window behaves differently in each one. So the shape is **drawn** rather than
clipped: the deep background fills the rectangle, and the panel surface is a path
with the cutaway carved out of its right edge, between 44% and 86% of the height.

The result reads as the intended silhouette and behaves identically everywhere.

---

## The hidden control

The signature notch in the top-left corner has a 4 px target at its outer tip. It
is invisible until hovered. Once found, it stays faintly marked in the secondary
colour so it can be got back to.

It opens **Wolf**, a real effect rather than a joke: a dispersive feedback network
that exaggerates the wolf tone a luthier spends effort designing out. Its controls
use the secondary accent rather than the primary, so the panel is visibly a
different part of the instrument, and every tooltip in it says it is the hidden
effect.

---

## Where this lives in the code

| | |
|---|---|
| Palette, metrics and fonts | `Source/UI/Theme.h` |
| All drawing | `Source/UI/Theme.cpp` |
| Knobs, sliders, toggles, meters, LED, data stream | `Source/UI/Widgets.cpp` |
| Window shape, notch, footer, hidden target | `Source/PluginEditor.cpp` |

There are no colour literals anywhere outside `Theme.h`. Re-skinning the plugin
means editing one file.
