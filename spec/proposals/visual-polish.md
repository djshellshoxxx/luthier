# VISUAL POLISH PROPOSAL

Status: **proposed, approved in principle by the user (2026-09-23)**. Written
as a proposal because `CLAUDE_CODE_BRIEF.md` says features not in the spec go
to a proposal file before they go into code. Each section below is a
separate piece of work; none of them changes the sound.

Ordering: after the guitar illustration rebuild (TODO item G), because
several of these share its rendering helpers (lighting, materials).

## 0. Ground rules

1. **`theme.md` still governs.** Colours, type, control geometry and the
   value-arc conventions do not change. Everything here is surface and
   material, drawn inside the shapes `theme.md` fixes.
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

This is a deliberate, scoped exception to `theme.md` and needs the spec
owner's agreement before it is built.

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

`theme.md` says a plugin re-tints only one accent for its identity; a
user-chosen accent is a user preference layered on top of that identity,
not a change to it. Needs the spec owner's agreement, as 3 does.

## 6. Tests

- Every textured or lit surface renders identically twice (cached, not
  regenerated per frame).
- High-contrast palette: no gradients, sheen or textures present.
- Contrast: every accent option on every palette meets 4.5:1 for text.
- Standby off/on and bypass on/off change the pilot light and pedal LEDs.
- Performance: opening the Advanced window with every face visible stays
  inside `performance-budget.md`'s UI frame budget.
