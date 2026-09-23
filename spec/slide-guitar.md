# SLIDE GUITAR SPEC

A slide is a hard tube pressed against the strings instead of a fingertip.
It removes the frets from the equation entirely: pitch becomes continuous,
the string is damped on both sides of the contact, intonation is the
player's responsibility, and the bar itself rings and clanks.

Nothing about it is a variation on fretted playing, which is why it needs
its own module rather than a "glide" parameter. `SlideEngine` is a mode
the instrument enters.

Covers bottleneck (the tube on a finger), lap steel (the bar on a flat
instrument), dobro (bar on a resonator) and hybrid (slide on one finger,
frets with the others) - which is how most electric slide is actually
played and the one most implementations forget.

## 0. Ground rules

1. **Pitch is continuous.** In Slide Mode there are no fret positions.
   Quantising to semitones would remove the only thing that matters.
2. **The bar damps both sides.** A slide contacts the string at a point;
   the segment behind it towards the nut is damped, not free. This is why
   slide has its particular short, vocal sustain.
3. **The bar is an object.** Material, mass and length are physical fields
   with audible consequences, and they live in the Workshop as a part.
4. **Hybrid is the default for electric.** Pure slide is a lap-steel
   technique; a bottleneck player frets with three fingers.
5. **Setup matters.** A guitar set up for fretting buzzes under a slide.
   The plugin says so rather than silently sounding bad.

## 1. Modes

| Mode | Contact | Damping behind | Typical setup |
|---|---|---|---|
| `bottleneck` | One string or several, angled | Partial (finger mutes) | High action, round neck |
| `lap_steel` | All strings, square to the neck | Full | Very high action, flat or hi-nut |
| `dobro` | All strings, square | Full | Resonator body, hi-nut |
| `hybrid` | Slide on one string, frets elsewhere | Partial | Normal-to-high action |

`slide_mode` selects. Hybrid routes any note not currently under the bar
through the normal fretted path, which is what makes it hybrid.

## 2. The bar

A Workshop part (`guitar-workshop.md`), mirrored read-only into the
CHARACTER tab's SLIDE group.

| Field | Range | Default | Effect |
|---|---|---|---|
| `slide_material` | choice | Glass | Damping, brightness, friction, clank spectrum |
| `slide_mass` | 20 – 220 g | 65 | Contact stiffness; heavier sustains longer and clanks lower |
| `slide_length` | 40 – 100 mm | 70 | How many strings it can cover |
| `slide_diameter` | 15 – 30 mm | 22 | Contact patch curvature |

### 2.1 Materials

| Material | Damping | Brightness | Friction | Character |
|---|---|---|---|---|
| Glass (borosilicate) | 0.30 | 0.70 | 0.25 | Smooth, sweet, the default |
| Glass (thick wall) | 0.25 | 0.65 | 0.25 | Fatter, more sustain |
| Brass | 0.12 | 0.85 | 0.40 | Loud, bright, heavy, clanky |
| Steel | 0.10 | 0.95 | 0.45 | Brightest; lap-steel standard |
| Ceramic / porcelain | 0.22 | 0.75 | 0.30 | Between glass and brass |
| Bone | 0.40 | 0.55 | 0.50 | Warm, quiet, grabby |
| Brass-plated steel bar | 0.11 | 0.90 | 0.42 | Dobro bar |

Friction drives the continuous surface noise in section 5; damping and
brightness drive the string model's contact.

## 3. The contact model

At the contact point *x* along the string:

- The **sounding length** is bridge-to-contact. Pitch is
  `f = f_open × L / (L - x)`, continuous in `x`.
- The **segment behind** (contact-to-nut) is damped by
  `slide_damping_behind`, which is 1.0 for lap steel and dobro (the bar
  covers everything and the player mutes with the palm) and 0.55 for
  bottleneck and hybrid (a finger behind the slide damps partially).
- The **contact itself** is lossy: the bar absorbs energy at a rate set by
  its material damping and its mass. Heavier bars couple less and sustain
  more; softer materials absorb more.
- **Pressure** (`slide_pressure`, 0-1) sets how firmly the bar sits. Too
  light and the string buzzes against the bar (see 5.2); too heavy and the
  string is choked against the frets underneath.

### 3.1 Slant

`slide_slant` (-30° to +30°, default 0) tilts the bar across the strings,
so the contact point differs per string. This is how a slide player gets
two different intervals at once, and it is the reason the parameter exists
rather than being a fixed perpendicular contact.

Contact for string *s* is `x + slant_offset(s)`, where the offset is
`tan(slant) × stringSpacing × (s - centreString)`.

### 3.2 Vibrato

Slide vibrato is a movement of the bar, not a bend. It modulates `x`
directly, which means it modulates pitch **and** the damped-segment length,
which is why slide vibrato sounds different from finger vibrato. The
existing `vibrato_rate` and `vibrato_depth` drive it; depth is interpreted
in millimetres of bar travel rather than cents.

## 4. Intonation

There are no frets, so pitch is whatever the player's hand does.

- `slide_intonation_assist` (0-1, default 0.15) pulls the sounding pitch
  toward the nearest equal-tempered note, with a time constant of 120 ms.
- At 0 the pitch is exactly where the bar is - correct, and hard to play
  from a keyboard.
- At 1 it snaps to semitones, which is not slide any more but is useful for
  sketching.
- The default of 0.15 is a light assist that a real player's ear would
  supply.

This is a playability aid and it is marked as such in the UI, not hidden.

## 5. Noise

### 5.1 Friction

A continuous, quiet band of noise while the bar is moving, proportional to
`slide_noise_amount × friction(material) × barSpeed`. It replaces finger
squeak, which is suppressed on contacted strings
(`gui-integration.md` 7, `string-squeak.md` 10).

### 5.2 Clank

`NoiseEngine::Clank`, 8 generators (`performance-budget.md` 1).

Triggered when the bar lands on the strings, and when insufficient
pressure lets a string rattle against the bar.

- **Spectrum**: set by material and mass - a brass bar clanks around
  1.2 kHz, glass around 2.5 kHz, and mass lowers both.
- **Level**: `slide_clank_amount × landingVelocity`, plus the rattle term
  when `slide_pressure` is below 0.3.
- Clank is the sound that makes slide playing sound like an object being
  put down on strings, and leaving it out is why most slide emulations
  sound synthetic.

## 6. Setup requirements

A slide needs height. With Slide Mode on and the current setup's action
below 2.2 mm bass, the plugin shows the empty-state message
`gui-integration.md` 14 fixes:

> This guitar was not built for slide. Open Workshop to fit a hi-nut or a
> different bridge.

It does **not** change the setup automatically. `fret-buzz.md` 6.1's
"Slide setup" style is one click away and the user chooses it.

With too-low action, the string is driven into the frets under the bar and
the buzz model produces exactly the ugly rattle a real guitar would. That
is correct behaviour and the message explains it.

## 7. Parameters and UI

All in the `slide` family (`advanced-ranges.md` 2).

| ID | Stock | Advanced | Default |
|---|---|---|---|
| `slide_enabled` | bool | – | false |
| `slide_mode` | choice | – | Hybrid |
| `slide_pressure` | 0 – 1 | 0 – 1 | 0.55 |
| `slide_slant` | -30 – +30° | -60 – +60° | 0 |
| `slide_damping_behind` | 0 – 1 | 0 – 1 | per mode |
| `slide_noise_amount` | 0 – 1 | 0 – 4 | 0.4 |
| `slide_clank_amount` | 0 – 1 | 0 – 4 | 0.45 |
| `slide_intonation_assist` | 0 – 1 | 0 – 1 | 0.15 |

Bar material, mass, length and diameter are **Workshop part fields**, not
parameters, and are mirrored read-only here.

Net new parameters: **+8**.

`gui-integration.md` 4.4 puts the **SLIDE** group on CHARACTER, shown only
when Slide Mode is on, containing: pressure state, slant, the material and
mass mirror, noise amount, clank amount.

Slide Mode itself is a header toggle with shortcut `S`
(`gui-integration.md` 7, 17). When on:

- The SLIDE group appears.
- The Workshop's Slide part category is enabled.
- The fretboard overlay draws the bar at the current position and slant -
  a 6 px rounded bar in the material's colour at 80% opacity, 80 ms ease
  (`gui-integration.md` 21).
- The tuning popover shows continuous pitch rather than a fret.
- Squeak on contacted strings is suppressed.

## 8. Interaction

- **`string-squeak.md`**: suppressed on contacted strings. In hybrid mode,
  strings *not* under the bar still squeak normally, because they are
  still being fretted with fingers.
- **`fret-buzz.md`**: still active and deliberately so, per section 6.
- **`character-wear.md`**: fret wear is irrelevant under the bar, which is
  physically true and worth not modelling.
- **`bass-techniques.md`**: slide on bass is legitimate; no interaction
  beyond both being available.
- **`midi-export.md`**: the `slide` event class carries bar position over
  time, slant and pressure. Luthier profile round-trips it; Generic profile
  renders it as pitch bend, which is the honest lossy mapping.

## 9. Tests

- **Pitch is continuous.** Sweep the bar from fret 3 to fret 5 over 1 s;
  assert the measured f0 is monotonic and never quantises to a semitone
  grid, with `slide_intonation_assist` at 0.
- **Assist pulls to pitch.** At assist 1.0, a bar 40 cents sharp settles
  within 5 cents of equal temperament inside 400 ms.
- **The segment behind is damped.** Compare sustain of a note at fret 12
  fretted versus under the bar in lap-steel mode; assert the slide note's
  60 dB decay is at least 25% shorter.
- **Slant produces different pitches per string.** At 20° slant, assert the
  interval between strings 1 and 6 differs from the open-tuning interval by
  at least 40 cents.
- **Mass changes sustain.** A 200 g bar sustains at least 15% longer than a
  30 g bar, all else equal.
- **Clank fires on landing.** Assert a clank generator is triggered within
  5 ms of bar contact, and that its spectral centroid is lower for brass
  than for glass.
- **Squeak is suppressed under the bar**, and in hybrid mode still fires on
  non-contacted strings.
- **The low-action warning fires.** With action below 2.2 mm bass and Slide
  Mode on, assert the empty-state message is shown and the setup is *not*
  modified.
- **Mode switch is clean.** Toggling Slide Mode during a sounding note
  produces no discontinuity above -60 dBFS.
- **No allocation on the audio thread.**
