# VISUAL POLISH PROPOSAL

Status: **approved by the user (2026-09-23)**, including the knob caps (3)
and the accent colour (5), and with a wider change the user asked for: this
plugin replaces `theme.md`'s shared futuristic look with its own
guitar-shop theme (section 7). Written
as a proposal because `CLAUDE_CODE_BRIEF.md` says features not in the spec go
to a proposal file before they go into code. Each section below is a
separate piece of work; none of them changes the sound.

Ordering: after the guitar illustration rebuild (TODO item G), because
several of these share its rendering helpers (lighting, materials).

## 0. Ground rules

1. **Section 7 replaces `theme.md` for Luthier.** `theme.md` is the house
   style shared with the other plugins and is left unchanged for them; for
   this plugin, where section 7 says something different, section 7 wins.
   What section 7 does not mention (layout grid, spacing, the value arc's
   role, the output LED, the data stream) still comes from `theme.md`.
2. **Accessibility is not traded for looks.** Every textured surface keeps
   `accessibility.md`'s 4.5:1 text contrast on all three palettes (Default,
   High contrast, Light), and High contrast turns textures and sheen off.
3. **Performance budget holds.** Textures and lighting are rendered once into
   cached images and redrawn only when the thing they depict changes (a knob
   turning redraws its indicator, not its material). No per-frame
   procedural texture work on the message thread
   (`performance-budget.md`, `gui-engine-dataflow.md`).
4. **No motion.** Animated transitions were considered and declined. The
   only things that move are things that were already live (meters, the
   guitar's playing overlays, the tube glow in 4).

## 1. Photographic materials on the guitar

Extends `guitar-illustration.md` 11 (finishes) with lighting:

- One fixed key light (top-left, matching `theme.md`'s knob gradient) and a
  soft fill. Each body outline gets a baked shading pass from it: a broad
  diffuse falloff across the body, a thin specular highlight along the
  bevelled edge.
- **Lacquer sheen**: gloss finishes (`finish.gloss` above 0.6) get a soft
  specular band; satin gets a wide, faint one; oil and natural get none.
- **Metal hardware**: chrome, nickel, gold and black hardware
  (`hardware_color`) each get a reflection gradient and a hot highlight,
  so a bridge reads as metal and not grey paint.
- **Depth**: pickups, bridge and pickguard cast a short, soft drop shadow
  on the body, from the same light.

Test: the per-guitar PNG renders from TODO item G, compared by eye, and a
check that a High-contrast render has no gradient or sheen.

## 2. Amp and pedal faces

The AMP section and the pedal racks currently draw rows of plain knobs.

- **Amp head**: each amp model gets a face drawn from its family - a Tolex
  texture (black, cream, brown, blonde), a grille cloth pattern, a
  faceplate colour, a model-appropriate logo plate (no trademarks; generic
  names as in `factory-content.md`) and a pilot light that follows
  Standby.
- **Pedals**: each pedal type gets an enclosure colour, a footswitch, an LED
  that follows bypass, and its knobs placed as a pedal's are. The pedal's
  name is its generic type name.
- The same knobs and parameters as today - this changes how the sections
  look, not what they contain or where anything is (`gui-integration.md`
  19 is unaffected).

## 3. Knobs with character

`theme.md` fixes one knob style. Proposal: the **amp and pedal faces
only** (2) may use a knob cap that suits the model - chicken-head,
top-hat, speed knob, witch-hat - while keeping `theme.md`'s value arc,
indicator colour and hit area, so reading a value works the same
everywhere. Every other panel keeps the standard knob.

Approved. With section 7's theme the exception widens: the model-specific
caps are used on the amp and pedal faces, and section 7's standard knob is
used everywhere else.

## 4. Stage and ambient touches

Live indicators, not animation for its own sake:

- **Tube glow**: the amp face shows its valves behind a vent, glowing with
  the amp's drive level (from the level the amp already reports).
- **VU-style output meter**: an optional needle meter in the header or the
  Easy rig strip, beside the existing LED, reading the master output.
- **Room light**: the ROOM card's background warms and widens with the
  room size and wet level, so the space you are playing in has a look.

All three update at the rates `gui-engine-dataflow.md` already allows for
meters, and grey out when stale like every other live display.

## 5. Accent colour

`accessibility.md` already specifies Default, High contrast and Light
palettes; if they are not all built, finishing them is the first step and
not part of this proposal.

New:

- **User accent**: Options -> Appearance offers a small set of accent
  colours (the default burnt orange plus five others, each checked for
  4.5:1 contrast on every palette).
- **Follow the guitar**: an option that takes the accent from the current
  guitar's finish colour, adjusted to meet contrast.

Approved. The default accent is section 7's, not `theme.md`'s burnt orange.

## 7. The Luthier theme (replaces `theme.md`'s look for this plugin)

`theme.md` gives every plugin in the family the same dark, futuristic
styling. Luthier is an instrument rather than a studio tool, and the user
has asked for it to look like one: a guitar shop and a workbench rather
than a control room. **This section is a starting direction for review**;
the exact values are to be tuned against rendered screenshots.

### 7.1 Palette

Warm and wooden instead of blue-black and neon:

| Role | Default palette | Replaces |
|---|---|---|
| Window background | Dark rosewood brown, around #1E1511 | #0E1116 blue-black |
| Panel | Dark walnut, around #2A1E17, with a faint wood grain | flat panel grey |
| Raised surface | Tolex black with a subtle texture | flat raised grey |
| Primary text | Warm ivory, around #EFE3CC (aged cream plastic) | cool white |
| Muted text | Parchment tan, around #B9A58A | cool grey |
| Primary accent | Aged brass / amber, around #D4A24C | burnt orange #E8532A |
| Secondary accent | Vintage green-teal of old amp jewel lights, around #6FA58A | muted teal |
| Warning | Tube-glow orange-red | unchanged role |

Light palette: maple and cream (a blonde guitar and a tweed amp). High
contrast: unchanged from `accessibility.md`, with textures off. Every pair
still meets 4.5:1 for text.

### 7.2 Type

- Headings: a condensed vintage display face in the style of 1950s-60s amp
  and guitar logos (an open-licence font, shipped with the plugin).
- Body and labels: a clean, slightly warm sans for readability.
- Numbers: tabular figures, as now.
- Section headers: engraved-plate style (text on a small brass or ivory
  plate) instead of the accent bar.

### 7.3 Controls

- **Standard knob**: a black "bell" or dome amp knob with a cream or brass
  pointer line and a small skirt, with `theme.md`'s value arc kept outside
  it (the arc is how values are read and marked for advanced ranges).
- **Toggles**: mini toggle switches (the kind on a guitar or amp) for
  on/off; pill buttons stay for tab strips and mode switches.
- **Sliders**: fader-style with a brass cap.
- **Panels**: framed like a cabinet or a pedalboard - a slightly raised
  border with corner screws on the larger ones - not flat cards.

### 7.4 Brand mark

The diagonal accent notch becomes a small inlaid headstock outline in
brass. The output LED stays (it is a function, not styling).

### 7.5 What does not change

Layout, column widths, the value arc's meaning and advanced-range
marking, hit areas, keyboard focus rings (restyled to the new accent, still
visible), and every accessibility rule.

## 8. Tests

- Every textured or lit surface renders identically twice (cached, not
  regenerated per frame).
- High-contrast palette: no gradients, sheen or textures present.
- Contrast: every accent option on every palette meets 4.5:1 for text.
- Standby off/on and bypass on/off change the pilot light and pedal LEDs.
- Theme: every palette's text pairs meet 4.5:1 (automated), and the
  standard knob, toggle and slider render in all three palettes (PNG
  renders reviewed by eye).
- Performance: opening the Advanced window with every face visible stays
  inside `performance-budget.md`'s UI frame budget.
