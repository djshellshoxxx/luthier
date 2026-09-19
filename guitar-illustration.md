# GUITAR ILLUSTRATION SPEC

The single most complex UI element in the plugin. This document
covers how the guitar is drawn, how it changes when parts change,
how it hosts every workshop interaction, and how the family switch
(electric / acoustic / classical / bass / resonator) rebuilds the
whole thing without a hitch.

Reference material: this spec draws on the visible visual grammar of
mid-20th-century solid-body electrics, dreadnought and orchestra-model
acoustics, classical guitars, and the standard 4-string bass shapes.
No trademarks are used. Every named "style" in this file corresponds to
a body-shape family in wide use for six decades or more.

## 0. Ground rules

1. **The illustration is procedurally drawn from `GuitarSpec`**, never
   a photograph or a bitmap sprite. Same renderer, same file, from Easy
   Mode to Workshop to preset-browser thumbnail.
2. **The visual language is flat**, per theme.md: fills, single-stroke
   highlights, grain as thin low-opacity strokes, one radial gradient
   max per burst. No skeuomorphic wood textures, no faux metal
   reflections beyond one highlight line.
3. **Every part is a first-class visual**. Swap a bridge, the bridge
   changes. Swap a pickup, that pickup redraws with its new geometry,
   pole spacing and cover style.
4. **Family switches rebuild the whole illustration**. Changing from a
   Vintage Single-Cut electric to a Dreadnought acoustic is not a
   colour swap; it's a full re-layout with a different body outline,
   different bridge, different soundhole, different scale length,
   different string count if the target dictates.
5. **The illustration is authoritative to the ear**. If a part is
   drawn on the illustration, the engine has that part loaded. There
   is never a mismatch between visible pickup and audible pickup.
6. **Every visible interaction has three feedbacks** (from
   workshop-ui.md 0.3): visual (the illustration changes), numeric
   (inspector shows the new value), and audible (audition or live
   play).
7. **Illustration performance target**: full repaint under 8 ms on
   the mid CPU class. Live-overlay repaint under 2 ms.

## 1. Coordinate system and scaling

Illustration coordinates are in millimetres, with the origin at the
centre of the bridge saddle for the current guitar. X positive toward
the headstock, Y positive toward the treble side.

Rendering transform: mm to px scale = `illustration_pixel_width /
guitar_total_length_mm`. At 1280 window width with the Workshop bench
taking about 60% of that (768 px), a typical 1000 mm-long acoustic
renders at ~0.77 px/mm.

The whole guitar always fits at "fit" zoom. User can zoom in with
Ctrl-scroll up to 4x. Pan follows.

## 2. Rendering pipeline

Every repaint runs through the same pipeline. Steps 1-3 are cached
per `GuitarSpec`; steps 4-7 run every frame that has live data.

### 2.1 Static geometry (cached)

1. Read `GuitarSpec`.
2. Build the part list in draw order (section 5).
3. For each part, ask the part renderer for its vector geometry in
   mm at the part's position from the spec. Cache the whole scene
   as an in-memory vector document.

Cache key: `GuitarSpec` canonical serialization hash. Cache invalidates
on any part swap, any position change, any finish change.

### 2.2 Live overlays (per frame)

4. Paint the static scene through the transform.
5. Paint live overlays (played notes, slide bar, pick position,
   pickup pulse, buzz heatmap) from the display FIFO
   (gui-engine-dataflow.md 6).
6. Paint hover / selection outlines (workshop-ui.md 3 hover rule).
7. Paint any drag ghost (part-card drag from the drawer).

Live overlays never invalidate the static scene cache.

### 2.3 Preset-browser thumbnails

Rendered on the worker thread at 128 x 256 px, cached by the same
`GuitarSpec` hash. First render of a novel spec takes about 40 ms;
cache hits are instant.

## 3. Guitar families

The `family` field on `GuitarSpec.meta.family` is the root switch.
Every family has its own body-shape catalogue, string count default,
scale-length default, hardware conventions, and a distinct drawing
rule set.

Families:

| Family | Default strings | Default scale (in) | Typical bodies |
|---|---|---|---|
| `electric` | 6 | 25.5 | solid, semi-hollow, hollow |
| `acoustic` | 6 | 25.5 | dreadnought, grand auditorium, parlor, jumbo, 12-string, archtop |
| `classical` | 6 (nylon) | 25.6 | classical, flamenco, cutaway classical |
| `bass` | 4 | 34 | solid, semi-hollow, hollow, acoustic bass |
| `resonator` | 6 | 25.0 | round-neck, square-neck |
| `extended` | 7 or 8 | 26.5 - 28 fanned | modern superstrat, multi-scale |

A family change is a structural change: the illustration completely
rebuilds. Any parts incompatible with the new family are replaced with
the family's factory defaults, with a banner listing what changed.

## 4. Body-shape catalogue

Every body is a `.luthierpart` with `part_type: "body"`. Each carries
an SVG-like outline in mm plus attachment points (bridge, tailpiece,
neck join, pickup routes, controls, jack, strap buttons, cutaway
depth, arm bevel, forearm contour).

### 4.1 Electric bodies

**Single-Cutaway Arched (LP-style)**
- Outline: roughly 328 mm wide, 495 mm long body, 46 mm deep at
  centre with a carved 15 mm arch on top.
- Single upper-bout cutaway, deep enough to reach the 17th fret.
- Waist at 235 mm long, 250 mm wide.
- Rounded lower bout, elegant single horn.
- Draws with a subtle centre highlight line to hint the arch.

**Double-Cutaway Offset (Strat-style)**
- Outline: 336 mm wide, 445 mm long body, 44 mm deep flat.
- Two symmetrical horns, upper reaches higher for cutaway access.
- Waist offset toward the treble side.
- Comfort contours: forearm bevel on the treble upper bout,
  tummy cut on the back (indicated by a soft shadow line).

**Slab Single-Cutaway (T-style)**
- Outline: 320 mm wide, 445 mm long body, 44 mm deep flat.
- Single upper cutaway, less deep than LP-style.
- No arch, no contours. Slab.
- Pickguard covers upper bout and pickup / control area.

**Thin Double-Cutaway (SG-style)**
- Outline: 310 mm wide, 490 mm long body, 32 mm deep.
- Two pointed "devil horn" cutaways of equal length.
- Bevelled edges around the top perimeter.
- Very thin body draws with a strong shadow underneath.

**Offset Contoured (Jazzmaster / Jaguar style)**
- Outline: 355 mm wide, 445 mm long, 44 mm deep.
- Asymmetric offset waist; upper bout larger than lower.
- Flowing horn on treble side.
- Pickguard covers most of the upper half.

**Explorer-Angular**
- Outline: 400 mm wide (at points), 550 mm long, 44 mm deep.
- Extreme angular shape, single deep cutaway, pointed lower horn.
- Straight-line panels rather than curves.

**Flying-V**
- Outline: V-shape 380 mm wide at the point tips, 500 mm long from
  neck join to V-point apex.
- Two straight-edged wings forming a V.

**Reverse Firebird**
- Outline: 340 mm wide, 495 mm long.
- Upper bout larger than lower (reverse offset).
- Slight banana curve to the waist.

**Modern Superstrat**
- Outline: 336 mm wide, 445 mm long, 42 mm deep.
- Similar to Strat but sharper horns, deeper double cutaway.
- Sculpted heel for high-fret access.

**Semi-Hollow Thin-Line (335 style)**
- Outline: 400 mm wide, 500 mm long, 45 mm deep.
- Symmetric Venetian cutaways.
- Two F-holes, positioned symmetrically on the upper bout.
- Rounded contours; no sharp corners.

**Full Hollow Archtop**
- Outline: 430 mm wide, 530 mm long, 82 mm deep.
- Single Venetian cutaway.
- Two F-holes.
- Trapeze tailpiece.
- Floating bridge.
- Carved top with pronounced arch (drawn as gradient centre-line).

**Extended-Range Multi-Scale**
- Outline: variant of superstrat with a fanned-fret indication (the
  frets are angled; the nut angles the opposite way).

### 4.2 Acoustic bodies

**Dreadnought**
- Outline: 400 mm wide (lower bout), 505 mm long, 122 mm deep.
- Wide shoulders (upper bout similar width to lower).
- Straight-ish waist.
- Central soundhole 100 mm diameter.
- Bridge below soundhole; pin bridge, six pins.
- Rosette pattern around soundhole (drawn as concentric lines).
- Herringbone or plain purfling around the top perimeter (per finish).

**Grand Auditorium**
- Outline: 405 mm wide, 502 mm long, 115 mm deep.
- Narrower waist than dreadnought.
- Cutaway variant (Venetian) available.
- Same soundhole and rosette.

**OM (Orchestra Model)**
- Outline: 380 mm wide, 495 mm long, 108 mm deep.
- Smaller than dreadnought, similar waist to grand auditorium.

**000 (Auditorium)**
- Outline: 380 mm wide, 490 mm long, 105 mm deep.
- Slightly less deep than OM.

**Parlor**
- Outline: 330 mm wide, 460 mm long, 95 mm deep.
- Small, narrow body.
- Sometimes 12-fret to body (bridge sits further from soundhole).

**Jumbo**
- Outline: 425 mm wide, 520 mm long, 122 mm deep.
- Larger than dreadnought, rounder shape.

**12-String Jumbo**
- Same outline as jumbo, but drawn with 12 tuners on the headstock
  (six pairs) and 12 strings across the fretboard, bridge and
  soundhole.

**Selmer-Style (gypsy jazz)**
- Outline: 400 mm wide, 505 mm long, 100 mm deep.
- Distinctive oval soundhole (not round).
- Floating bridge, moustache-shaped or straight.
- Cutaway (petit-bouche is oval, grand-bouche is D-shaped; user
  choice via part).

### 4.3 Classical bodies

**Classical**
- Outline: 370 mm wide, 490 mm long, 100 mm deep.
- Round soundhole, elaborate mosaic rosette (drawn as a concentric
  pattern with contrasting colour bands).
- Wide flat neck.
- Tie-block bridge (no bridge pins; strings tie behind a
  saddle-and-bar block).
- Slotted headstock (rollers visible through the slots).

**Flamenca Blanca**
- Same outline as classical but thinner body (95 mm deep).
- Golpeador (tap plate) drawn as a clear plastic sheet below and
  around the soundhole.
- Wooden tuning pegs (visible in the slotted headstock as darker
  cylinders).

**Cutaway Classical**
- Classical outline with a Florentine or Venetian cutaway.

### 4.4 Bass bodies

**P-Style Bass**
- Outline: 340 mm wide, 500 mm long, 44 mm deep.
- Similar to double-cut electric but larger overall.
- Split-P pickup drawn as two staggered rectangles across the
  strings (bass-side coil for E-A, treble-side coil for D-G).
- Long headstock with four large tuners (2+2 or 4-in-line depending
  on style variant).

**J-Style Bass**
- Outline: 336 mm wide, 505 mm long, 44 mm deep.
- Offset-waist double cutaway (Jazz Bass shape).
- Two single-coil J-pickups.

**Music Man-Style**
- Outline: 340 mm wide, 500 mm long.
- Single humbucker pickup drawn as a wide rectangle.
- 3+1 tuner arrangement.

**Thunderbird-Style**
- Outline: 380 mm wide (at the offset points), 545 mm long.
- Reverse offset body, elongated.
- Two humbuckers.

**Hollow-Body Bass**
- Outline: 400 mm wide, 525 mm long, 85 mm deep.
- Semi-hollow or full hollow, F-holes.

**Multi-Scale Extended Bass**
- 5 or 6 string variant with fanned frets.

**Acoustic Bass**
- Outline: acoustic-guitar-like but larger; 435 mm wide, 555 mm long,
  135 mm deep.
- Round soundhole.
- Pin bridge.

**Headless Modern Bass**
- Outline: 320 mm wide, 380 mm long body only, no headstock.
- Tuners at the bridge end.

### 4.5 Resonator bodies

**Steel Body Resonator (round-neck)**
- Outline: 385 mm wide, 505 mm long, 90 mm deep.
- Metal body drawn with a distinct sheen (single highlight line, no
  gradient).
- Cover plate over the resonator cone, drawn as a perforated disc
  with a diamond pattern (typical of biscuit-cone resonators).
- Round neck (playable as normal guitar).

**Wood Body Resonator**
- Same shape as steel, but wood body with the cover plate metal.

**Square-Neck Resonator (dobro / lap style)**
- Neck cross-section drawn as a square rather than round.
- Nut extended in height (raised strings for lap-slide play).
- Slide bar sits on the strings by default when Slide Mode is on.

## 5. Layered rendering z-order

Every layer draws in this fixed order, back to front:

1. **Backdrop shadow** — subtle drop shadow of the whole guitar
   silhouette below the body.
2. **Body back** — outline fill in the body's back wood colour.
3. **Body sides** — thin strip around the perimeter in the sides wood
   colour (acoustic; electric skips).
4. **Body top** — outline fill in the top wood colour. For bursts,
   the single radial gradient lives here.
5. **Top cap** (electric arched only) — if the guitar has a maple
   cap, drawn as a slightly offset outline suggesting the carved
   arch.
6. **Grain layer** — thin low-opacity strokes across the top
   suggesting grain direction. Flame maple: perpendicular horizontal
   bands at 8% opacity. Quilt: pillowy oval clusters. Ash: strong
   vertical grain. Mahogany: subtle grain, mostly hidden. Alder:
   soft, faint grain.
7. **Pickguard** — outline fill in the pickguard material colour.
   Tortoise: two-tone stipple. White pearloid: base colour + faint
   contrasting swirl strokes.
8. **F-holes / soundhole** — cutouts painted in the shadow colour
   under the top. Rosette on classical / acoustic is a concentric
   pattern of contrasting colours drawn on the top layer around the
   soundhole.
9. **Bracing shadow** (acoustic only) — very subtle shadow lines
   visible through the soundhole showing the top bracing pattern.
10. **Bridge** — drawn per bridge part (section 7). Pins for pin
    bridges are separate small dots.
11. **Tailpiece** — for guitars that separate tailpiece from bridge
    (LP-style stopbar, trapeze, Bigsby).
12. **Pickup routes / rings** — for electric, drawn as slightly
    darker rectangles beneath each pickup.
13. **Pickups** — drawn per pickup part (section 8), at their
    positions from the guitar spec.
14. **Strings** — drawn per string material (section 10) from
    tailpiece / bridge across the saddle, over the nut, to the
    tuners. Wound strings show the winding as a fine dotted / dashed
    stroke.
15. **Neck** — outline from body join to headstock end. Fretboard
    on top of neck.
16. **Fretboard inlays** — dot inlays, block inlays, trapezoids per
    the fretboard part.
17. **Frets** — drawn as thin horizontal lines across the fretboard
    at the correct fret positions.
18. **Nut** — drawn at the headstock end of the fretboard.
19. **Headstock** — outline from nut to headstock tip. String tree /
    string retainers drawn as small hardware.
20. **Tuners** — six or twelve per style, drawn as buttons (peg
    heads) and shafts.
21. **Control knobs** (electric) — volume / tone knobs drawn as
    small circles with a pointer line, positioned per wiring part.
22. **Selector switch** (electric) — 3-way or 5-way switch drawn as
    a rectangle with a lever.
23. **Output jack** — small hardware at the edge of the body.
24. **Strap buttons** — small dots at the horn and at the end of
    the body.
25. **Truss rod cover** (headstock) — small trapezoid or rectangle
    at the top of the headstock.

Live overlays paint after step 25:

26. Played-notes layer.
27. Buzz heatmap layer (SETUP group active).
28. Slide bar layer (Slide Mode on).
29. Pick overlay (pick tool selected or live pick sensing).
30. Pickup pulse layer (during audible activity).
31. Hover / selection outlines.
32. Drag ghost.

## 6. Necks and headstocks

Necks:
- **Bolt-on**: four bolts visible where neck joins body, drawn as
  small hex-head dots.
- **Set-neck**: no visible fasteners; heel drawn slightly wider.
- **Neck-through**: no visible transition; body wings meet at a
  centre-line seam.

Fretboard radius (indicated by centre-line shading intensity):
- Vintage-radius (7.25"): pronounced centre-arch shading.
- Modern (12"-16"): faint centre-arch.
- Compound: gradient centre-arch tapering to flat at the highest
  frets.

Fretboard woods: rosewood (warm brown), ebony (near-black),
maple (light cream), pau ferro (medium brown), maple with binding
(cream with a thin cream perimeter stroke).

Inlays:
- Dot: 3, 5, 7, 9, 12 (paired), 15, 17, 19, 21.
- Block: rectangles at the same frets, in a contrasting colour
  (pearl white on rosewood, black on maple).
- Trapezoid: acoustic-style trapezoids.
- Sharktooth: pointed inlays at frets 5, 7, 9, 12, etc.
- Vine (custom): flowing organic pattern from fret 3 to 21.

Headstocks:
- **3+3 tuned** (LP, PRS, acoustic): three tuners each side of the
  headstock top.
- **6-in-line** (Strat, Tele): all six tuners along one edge.
- **Reverse 6-in-line** (reverse headstock): mirror of the above.
- **4-in-line** (P-bass style): four tuners along one edge.
- **2+2** (Music Man style, some basses): two tuners each side.
- **Slotted** (classical): two rectangular slots cut through the
  headstock, rollers visible inside.
- **12-string 6+6**: 12 tuners in a longer 6-per-side layout.

Headstock face carries the truss-rod cover (small trapezoid or
rectangle) and, on shipped guitars, a small "L" mark (Luthier
signature) in a corner (secondary accent, muted, 4% opacity so it's
subtle).

## 7. Bridges and tailpieces

Every bridge is a `.luthierpart` with drawing metadata.

Electric bridges:

**Tune-o-matic + stopbar**
- Bridge: thin horizontal rectangle with six small saddle notches.
- Stopbar: thicker horizontal bar behind the bridge, drawn with two
  stud posts.

**Vintage 6-point tremolo (Strat-style)**
- Bridge plate: rectangular with six screws along the leading edge
  (fulcrum points).
- Six saddles.
- Tremolo arm: drawn extending out at a slight angle.
- Springs: not visible on the top view; a subtle shadow beneath
  suggests the trem cavity.

**Modern 2-point tremolo**
- Similar to vintage but with two large studs instead of six screws.

**Floyd Rose style**
- Bridge: floating trem with saddle-clamp fine tuners visible on
  each string.
- Locking nut at the headstock end.
- Trem arm.

**Hardtail string-through**
- Bridge: fixed with six saddles.
- Six ferrules on the back of the body (visible on the back layer).

**Wraparound**
- Single bar around which the strings wrap; no separate tailpiece.

**Bigsby vibrato**
- Wide flat plate covering the lower bout with a spring-loaded
  handle rising above the top.

Acoustic bridges:

**Pin bridge**
- Rectangular block with six visible bridge pins.
- Saddle: thin white strip across the top of the block.

**Pinless bridge**
- Similar but no pins; strings load through the back of the block.

**Floating bridge (archtop)**
- Two-piece bridge sitting on the top; height adjustable.
- Trapeze tailpiece attaches at the end of the body.

**Selmer-style moustache**
- Curved wooden bar with two "wings" like a moustache.

**Classical tie-block**
- Bridge block with the strings tied through holes and knotted
  behind.

**Resonator biscuit / spider**
- Biscuit: cover plate over the cone, drawn as perforated disc.
- Spider: eight-legged bridge with strings on the pins.

Bass bridges:

**Vintage bass**
- Rectangular plate, four adjustable saddles.

**High-mass bridge**
- Chunkier bridge with visible cast metal body.

**Bridge with mutes**
- Bridge plate with foam mute inserts visible under the strings.

## 8. Pickups

Every pickup is a `.luthierpart` with drawing metadata.

Draw variants:

**Single-coil (open pole pieces)**
- White rectangle 70 x 18 mm.
- Six visible pole pieces as small circles.
- Two mounting screws at the ends.

**Single-coil (with cover)**
- Same rectangle with a solid cover; no pole pieces visible.

**Humbucker (open)**
- Chrome rectangle 70 x 40 mm.
- Two rows of pole pieces (6 or 12 total).

**Humbucker (closed cover)**
- Solid rectangle in chrome, nickel, gold, or black.
- No pole pieces visible.

**P90**
- Soap-bar shape: rectangle 82 x 32 mm with rounded corners.
- No visible pole pieces (dog-ear variant shows small "ears" on
  each end for mounting).

**Mini-humbucker**
- Small humbucker, 60 x 22 mm.

**Firebird mini**
- Small chrome bar with no visible poles.

**Split-P (bass)**
- Two staggered rectangles side by side, each 40 x 22 mm, offset by
  half a rectangle length (one covers the E-A strings, one covers
  the D-G strings).

**J-bass single coil**
- Long slender rectangle 90 x 22 mm.

**Music Man humbucker (bass)**
- Very wide rectangle 100 x 40 mm.

**Under-saddle piezo**
- Not visually rendered on the guitar body; a small preamp box or
  input jack indicates its presence.

**Soundhole magnetic pickup (acoustic)**
- Drawn straddling the soundhole; a slim rectangle above the hole
  with a small connecting cable to the endpin jack.

Pole spacing follows the pickup's `pole_spacing_mm` field. Cover
colour follows `cover` field (nickel, chrome, gold, black, cream, no
cover). Mounting rings around humbuckers drawn as slightly larger
rectangles beneath (creme, black, per part).

## 9. Pickguards

Colours: white pearl, black, mint green, tortoise (two-tone stipple:
brown base with darker brown blotches), cream, aged white, red
pearloid, gold anodized (metallic sheen suggested with a single
highlight stroke), transparent (drawn as an outline only).

Shapes vary by body style; every body part provides its own default
pickguard outline as a template. User can select from the pickguard
category or the body's default.

## 10. Strings by material

Every string is drawn as a line with material-specific character.

| Material | Colour | Line style | Width variation |
|---|---|---|---|
| Plain steel (electric) | `#C4C7CC` (light silver) | Solid 1 px | Thinner for high strings, same base colour |
| Nickel-plated steel wound (electric) | `#B0B4BA` (medium silver) | 1.5 px solid with fine 0.5 px dashed overlay to hint winding | Thicker for lower strings |
| Pure nickel wound | `#A9AAAF` (warmer medium silver) | 1.5 px solid with slightly darker dashed overlay | |
| Stainless steel wound | `#D0D4D9` (bright silver) | 1.5 px solid with fine dashed overlay | |
| Bronze 80/20 acoustic wound | `#C9A75A` (warm gold-brown) | 2 px solid with dashed overlay | |
| Phosphor bronze acoustic wound | `#B5824A` (warmer amber-brown) | 2 px solid with dashed overlay | |
| Silk & steel | `#B8B4A8` (silver with silk hint) | 1.5 px solid with faint red dashed overlay for the silk core suggestion | |
| Flatwound | `#B0B4BA` (medium silver) | 1.5 px solid, no dashed overlay | |
| Half-round / groundwound | `#B0B4BA` | 1.5 px solid with very faint dashed | |
| Tapewound | `#252525` (matte black) | 2 px solid, no overlay | |
| Nylon (classical) trebles | `#F2E9D8` (off-white) | 2 px solid with subtle transparency | Thicker than steel |
| Nylon-core wound (classical basses) | `#F2E9D8` core with `#C9A75A` gold winding overlay | 2.5 px solid with gold dashed overlay | |
| Coated (Elixir-style) | Material colour with reduced saturation | 1.5 px solid, no dashed overlay (winding hidden) | |
| Bass nickel roundwound | `#A9AAAF` | 2.5 px solid with pronounced dashed overlay | Much thicker than guitar |
| Bass stainless roundwound | `#D0D4D9` | 2.5 px solid with pronounced dashed overlay | |
| Bass flatwound | `#B0B4BA` | 2.5 px solid, no overlay | |
| Bass tapewound | `#252525` | 3 px solid, no overlay | |

String count adapts to guitar family:
- 6-string standard: 6 strings across the neck.
- 7-string extended: 7 strings, low B added to the bass side.
- 8-string extended: 8 strings, low F# added.
- 12-string: 12 strings in six pairs, drawn as parallel pairs (paired
  strings at slight offset).
- Bass 4-string: 4 strings, wide spacing.
- Bass 5-string: 5 strings.
- Bass 6-string: 6 strings.

Per-string override (workshop-ui.md 3): a single string can carry a
different material. It renders in its own colour and style
independently. Common examples: heavy low E in phosphor bronze on an
otherwise 80/20 set; wound G on a set that's otherwise plain G.

## 11. Colours and finishes

Every finish is a `finish` block on the `GuitarSpec`. Fields:

- `type`: `solid`, `burst`, `transparent`, `natural`, `metallic`,
  `sparkle`, `relic`.
- `color_a`: primary hex.
- `color_b`: secondary hex (for bursts).
- `burst_shape`: `radial`, `2-tone`, `3-tone`, `diagonal`, `honey`.
- `gloss`: 0-1 (affects the strength of the single highlight line).
- `aging`: 0-1 (adds relic marks, dinged edges, faded colour).

### 11.1 Solid finishes

Colour palette (recognisable classic solid finishes, none trademarked):

| Name | Hex | Common on |
|---|---|---|
| Black | `#0F0F0F` | Everything |
| White | `#F5F1E8` | Everything |
| Olympic White | `#E8DEC8` (slight cream) | Vintage electrics |
| Vintage Cream | `#E8D8B2` | Vintage electrics |
| TV Yellow | `#D8C68C` | Vintage single-cut |
| Blonde | `#E4D2A0` | T-styles |
| Butterscotch Blonde | `#D2A860` | T-styles |
| Sonic Blue | `#96B8D2` | Vintage double-cut |
| Daphne Blue | `#B4CADC` | Vintage double-cut |
| Surf Green | `#B0D4B0` | Vintage double-cut |
| Seafoam Green | `#98C9B0` | Offset |
| Sherwood Green | `#2C6244` | Vintage |
| Lake Placid Blue | `#425878` | Vintage |
| Fiesta Red | `#D4593E` | Vintage double-cut |
| Candy Apple Red | `#B22820` | Vintage |
| Metallic Gold | `#B8A055` | Vintage |
| Silver | `#B0B4BA` | Modern |
| Purple | `#4B2E5A` | Custom shop |

### 11.2 Bursts

- **2-tone sunburst**: yellow centre (`#E4D2A0`) fading to dark
  brown (`#3E2A1A`) at edges. Radial gradient.
- **3-tone sunburst**: yellow centre, red middle (`#B2401E`), dark
  brown edges. Radial gradient with three colour stops.
- **Cherry sunburst**: yellow centre (`#F0DA8C`) fading to
  translucent cherry red (`#9E2A1E`) at edges.
- **Tobacco burst**: honey (`#B4834A`) centre to dark tobacco
  (`#2E1E12`) edges.
- **Honey burst**: light amber (`#DCB870`) centre to medium amber
  (`#8C6440`) edges.
- **Iced-tea burst**: light golden centre to smoky brown edges.
- **Blueburst**: silver centre to deep blue (`#1E3E62`) edges.
- **Green burst**: yellow centre to deep green edges.

Bursts render as a single radial gradient on the body top layer. On
flame or quilt maple tops, the grain layer paints over the burst so
the figure shows through as a play of highlights.

### 11.3 Transparent finishes

`type: "transparent"` reveals the wood grain colour beneath a tint.
Applied as a coloured overlay at 60% opacity above the wood-colour
fill.

Common: transparent black (raw grain darkens visibly), transparent
cherry, transparent blue, transparent amber, seethrough green.

### 11.4 Natural finishes

Just the wood colour, gloss or satin. No colour overlay.

### 11.5 Metallic finishes

Base solid colour + one strong highlight stroke suggesting metallic
sheen. Common: metallic gold, silver, blue, red, purple.

### 11.6 Sparkle finishes

Base solid colour + tiny dot pattern in a contrasting bright colour
across the top at 3% opacity, suggesting the sparkle flake.
Common: silver, gold, black, blue, purple sparkle.

### 11.7 Aged / relic finishes

`aging` field 0-1 controls:
- Edge wear on the top (thin scuff strokes along the outline).
- Belt buckle wear on the back (a lightened patch).
- Faded colour saturation.
- Slight yellowing on white / cream finishes.
- Dinged spots (small darker dots at random points seeded by the
  guitar's `character_seed`).
- Checked lacquer (thin cross-hatch lines on the top at high aging).

### 11.8 Hardware colour

Independent of body finish. Options: nickel (default,
`#B9BEC4`), chrome (`#D5D9DD`), gold (`#C9A24B`), black (`#2A2D31`),
aged nickel (`#A5AAB0` with slight tarnish), aged gold (dulled
`#B08838`).

Every metal part (bridge, tailpiece, tuners, pickup covers if metal,
control knobs, switch tip) picks up the `hardware_color` unless
overridden per-part.

## 12. Family switching (the big one)

Changing `GuitarSpec.meta.family` triggers a complete illustration
rebuild. UX and mechanics:

### 12.1 UX

- User selects a new family in the Workshop's parts drawer, first
  category ("Guitar Type" / family selector).
- A one-time confirmation the first time in a session: "Change guitar
  family? This will replace incompatible parts with defaults for the
  new family. [Change] [Cancel]".
- On Change, the whole illustration crossfades (250 ms) to the new
  family with the family's default template.
- Banner lists exactly what changed: "Family changed to Bass.
  Replaced parts: pickups (2), bridge, strings, wiring, nut, tuners."

### 12.2 Mechanics

Family change is a special `SwapPartCommand` that carries a new
family plus a template:

1. Message thread selects the target family's default template (from
   a small library of factory templates per family).
2. For each part slot, if the current part's `compatibility` array
   includes the new family, keep it; else swap to the template's
   default.
3. Build the new `GuitarSpec`.
4. Post `LoadGuitarCommand` (goes through the normal guitar-load
   flow).

Templates ship per family:
- `electric_default_template.luthierguitar` (Vintage Double-Cut).
- `acoustic_default_template.luthierguitar` (Grand Auditorium).
- `classical_default_template.luthierguitar` (Classical).
- `bass_default_template.luthierguitar` (P-Style Bass).
- `resonator_default_template.luthierguitar` (Steel Body Round-Neck).
- `extended_default_template.luthierguitar` (7-String Modern).

### 12.3 What changes with a family switch

- Body outline and depth.
- String count and default gauge.
- Scale length.
- Bridge type.
- Pickups (or the absence thereof for acoustic; or the piezo /
  soundhole magnetic for acoustic-electric).
- Nut width and slot positions.
- Fret count (electric usually 22 or 24, acoustic often 20).
- Tuners (count and headstock layout).
- Circuit / wiring (electric has pots; acoustic-electric may have a
  preamp; classical has none).
- Amp defaults (bass presets recall a bass amp; electric recalls a
  guitar amp; acoustic recalls a clean DI + reverb).

The `family` field is also the trigger for:
- Bass mode in the rhythm engine.
- Slide-friendly hint if the current body has a high nut.
- MIDI output profile default (bass hosts often expect different
  channels).

### 12.4 What is preserved across a family switch

- Preset name and metadata.
- Effects rack states (pedals continue to exist; some may be
  irrelevant for the new family but stay in the rack).
- Amp settings (though the target amp may change if the current amp
  is family-inappropriate).
- Mod matrix.
- Snapshot bank (parameters that no longer exist in the new family
  keep their values in each snapshot; the snapshot recall applies
  what still applies).
- Character seed.

## 13. Workshop interactions on the illustration

Per workshop-ui.md 4, every part is a hit-test region. This section
covers the illustration side.

### 13.1 Hit test

Every static part registers a hit region as a set of polygons. Hit
regions rebuild whenever the static-scene cache invalidates.

Overlapping regions resolve top-of-z-order wins (per the layer order
in section 5). A pickup on top of a pickguard, on top of a body:
click hits the pickup.

Modifier-key overrides:
- `Alt+click` targets the topmost region as usual but with audition
  behaviour (workshop-ui.md 7).
- `Shift+click` targets the region below the topmost (rare use;
  useful for editing the body under a pickup).
- `Ctrl+click` on a string starts a per-string override drag; the
  string highlights as the drop-target for a strings card.

### 13.2 Drag targets

- **Pickup card** onto a pickup: swaps that pickup.
- **Pickup card** onto a bare route: fills that route with the pickup
  (if a route is empty).
- **Bridge card** onto the bridge: swaps.
- **String card** onto a string: overrides that string; drop on a
  non-string region: replaces the whole set.
- **Body card** onto the body: swaps body (triggers a partial rebuild
  since the body drives many other geometries).
- **Pick card** anywhere: swaps the current pick.
- **Slide card** anywhere: swaps the current slide.
- **Capo card** onto a fret: places the capo at that fret. Onto the
  headstock: removes the capo.

### 13.3 Direct-manipulation drags on the illustration itself

Per workshop-ui.md 4:
- Drag a selected pickup along the string axis: changes
  `position_mm`. Ruler snaps.
- Scroll or drag screw handles: changes pickup height per side.
- Drag a bridge saddle: changes per-string intonation.
- Drag a nut slot: changes per-string nut slot depth.
- Drag the capo along the neck: changes capo fret.
- Drag the pick: changes pick position and angle.
- Drag the slide bar: changes slide position and slant.

Every drag has a snap grid (workshop-ui.md 4) and honours theme.md's
fine (default), coarse (Shift), ultra-fine (Ctrl) modifiers.

## 14. Live playing overlays

Per gui-engine-dataflow.md 6.

Additional Workshop-specific overlay:

**Frequency-band highlight** (when a spectrum-delta pending change is
being auditioned): the top of the guitar body shows a subtle warm /
cool tinting where the spectrum is changing most (warm where energy
increases, cool where it decreases). Fades to none within 500 ms of
the audition ending. Under reduced motion, the tinting is a static
label instead.

## 15. Preset-browser thumbnail

Rendered at 128 x 256 px. Cache key: SHA of the `GuitarSpec`
canonical serialization.

Rendered on the worker thread with the same renderer at reduced
detail:
- Grain strokes at half resolution.
- No live overlays.
- Frets drawn as single 1 px lines.
- Small hardware simplified to filled rectangles.

## 16. Accessibility

Every part is an accessible child on the illustration:

- Body: "Body: mahogany, single-cutaway arched, cherry burst."
- Neck: "Neck: mahogany set-neck, medium C profile."
- Fretboard: "Fretboard: rosewood, 24 frets, dot inlays."
- Nut: "Nut: bone, 43 mm width."
- Bridge: "Bridge: ABR-1 Tune-o-matic; string 6 intonation +0.5 mm."
- Pickup (per pickup): "Neck pickup: PAF 57 Alnico 2 7.6 k
  humbucker; position 152 mm from saddle; height 2.8 mm bass / 3.1
  mm treble."
- Tuners: "Tuners: Kluson 15:1, nickel."
- Strings (per string): "String 6: D'Addario NYXL nickel-plated
  steel wound, 46 gauge."
- Pick (when overlaid): "Pick: Celluloid 0.73 mm, standard shape,
  position 40 mm from saddle, angle 10 degrees."
- Slide (when overlaid): "Slide: glass 22 mm; position fret 5,
  slant 0 degrees, pressure normal."

Screen reader announces the current selection on click. Arrow keys
navigate parts in z-order (Tab across parts in visual order); Enter
opens the part in the inspector.

Reduced motion: no crossfades, no vibrating strings on played
notes, no drop shadow updates; parts change instantly with a
static outline highlight.

## 17. Performance

Full static repaint (cache miss): <= 40 ms on mid CPU class for a
typical solid-body electric.
Full static repaint (cache hit, only overlays repaint): <= 2 ms.
Live overlay-only repaint: <= 2 ms.
Family switch (full rebuild): <= 120 ms.
Preset-browser thumbnail render (worker thread): <= 100 ms.
Cache size: 200 static-scene caches at typical 12 KB each = ~2.4 MB.

## 18. Extensibility

New body shapes, new pickup shapes, new bridge shapes: each is a new
`.luthierpart` with drawing metadata. Shipping a new part does not
require code changes.

New families require code changes (the family switch logic is
enumerated, not data-driven; adding "ukulele" or "mandolin" would be
a v2 feature).

## 19. Tests

- Every factory guitar renders end-to-end at 128, 384, 768, 1536,
  3072 px width; no clipping, no missing parts.
- SVG hash changes on every part swap and on every position change >
  0.5 mm.
- Family switch: from every family to every other family produces a
  valid `GuitarSpec` with the target family's defaults.
- Hit testing: 10 000 random clicks over each factory guitar select
  the intended part 100% of the time.
- Drag bounds: pickup cannot leave the body route without advanced
  ranges; height cannot go below 0.8 mm without advanced ranges.
- String colour: for every material listed in section 10, the drawn
  line uses the documented colour within 1 hex step.
- Burst rendering: 3-tone sunburst matches a reference SVG within a
  pixel-level threshold.
- Aging: a guitar rendered with aging 0.7 has visible dinged spots
  in the same positions across runs (seeded by character_seed).
- Live overlays: played-note dot appears within 60 ms of the audio
  note-on, decays over 60 ms.
- Family switch preserves the preset name, effects rack, mod matrix,
  and character seed.
- Reduced motion: no animation frames after toggle-on, all overlays
  render as static states.
- Preset-browser thumbnail cache: cold render < 100 ms; cache hit
  < 1 ms; cache eviction on 200th entry.
- Accessibility: every part reachable via Tab; screen-reader
  announcements match the documented strings for a fixture guitar.
