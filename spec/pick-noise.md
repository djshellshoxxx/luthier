# PICK NOISE SPEC

The pick is the thing actually touching the string, and Luthier models
everything except it. A plectrum has a material, a thickness, a tip shape,
a bevel and a wear state, it hits the string at an angle, and every one of
those changes the sound - not subtly, either. A 0.6 mm nylon pick and a
1.5 mm Ultex on the same guitar are further apart than two different
pickups.

Three audible events come off a pick and none of them exist in the build:

- **Click**: the attack transient of plectrum-on-string, before the string
  note begins.
- **Chirp**: the short scrape as the pick leaves a wound string.
- **Scrape**: the long rake down the wound strings, deliberately.

This file specifies all three and the pick that produces them. It also
defines the `NoiseEngine` pool that `string-squeak.md` and `fret-buzz.md`
share, because it is the first of the three noise specs in `INDEX.md`
order.

## 0. Ground rules

1. **Noise is generated, not sampled.** A sample library of pick clicks
   would be one click per pick per velocity per string. The synthesis is
   cheap and parameterised by the same physical fields the pick has.
2. **Noise is an event, not a layer.** Every noise generator is triggered
   by something the player did and then runs to completion. Nothing drones.
3. **Noise rides the instrument path.** Click, chirp and scrape are
   produced at the string and go through body, pickup and circuit like any
   other string energy. They are not mixed in at the output.
4. **Honest magnitudes.** A pick click on a clean amp at moderate velocity
   sits 25-35 dB below the note. It is felt more than heard, and turning it
   to where it is obviously audible should sound wrong.
5. **Zero is silent and free.** Amount at 0 means the generator never
   starts and costs nothing.

## 1. The `NoiseEngine` pool (shared by all three noise specs)

`performance-budget.md` 1 fixes the class list and the pool size.

```
NoiseEngine {
    Pool<Generator, 16> squeak;     // string-squeak.md
    Pool<Generator, 16> pickClick;
    Pool<Generator, 16> pickChirp;
    Pool<Generator,  8> pickScrape;
    Pool<Generator, 16> fretBuzz;   // fret-buzz.md
    Pool<Generator,  8> clank;      // slide-guitar.md
}
```

- Pools are **fixed size and pre-allocated** at `prepare()`. Nothing
  allocates in the audio callback (`engine.md` 0).
- A trigger with no free generator **steals the oldest**, because a missing
  recent transient is more audible than a truncated old one.
- `performance-budget.md` 9's degradation step 4 halves each pool to 8.
  Halving is a pool-size change, not a bypass: the quietest events drop
  first because they are the ones stolen.
- **Per-material textures** (40 MB budget, `performance-budget.md` 3) are
  short noise tables synthesised once at load per material, not per event.
  A generator reads a table with a per-event random offset, which is what
  keeps a thousand clicks from being one click repeated.

### 1.1 Generator shape

Every noise generator is the same three things:

```
excitation (table read, or filtered noise burst)
  → resonator (biquad bank, 1-3 poles, set by the event)
  → envelope (attack ms, decay ms, shape)
```

It differs between classes only in how the event sets those. This is why
one pool structure serves six classes.

### 1.2 Where noise enters

| Class | Injection point | Why |
|---|---|---|
| Pick click | String excitation input, summed with the pluck | It is the same physical impact |
| Pick chirp | String output, pre-body | It is surface noise, not string motion |
| Pick scrape | String output, pre-body | As above |
| Squeak | String output, pre-body | `string-squeak.md` 5 |
| Fret buzz | String output, pre-body | `fret-buzz.md` 4 |
| Clank | String output, pre-body | `slide-guitar.md` |

Everything except the click goes through body and pickup, so a noise event
is coloured by the instrument it happened on. The click is part of the
excitation because physically it *is* the excitation's first moment.

### 1.3 Aux 8

`routing-io.md`'s Aux 8 noise bus carries the sum of every generator,
pre-body, so a user can gate or re-balance noise separately. Latency
allowance is 128 samples (`performance-budget.md` 4).

## 2. The pick

| Field | Range | Default | Effect |
|---|---|---|---|
| `pick_material` | choice | Celluloid | Spectrum and damping, table below |
| `pick_thickness` | 0.38 – 3.0 mm | 0.73 | Stiffness; thicker is louder, lower, shorter |
| `pick_tip_radius` | 0.2 – 4.0 mm | 1.0 | Sharp tips click brighter and release faster |
| `pick_bevel` | 0 – 1 | 0.2 | A bevelled edge releases the string more gradually |
| `pick_wear` | 0 – 1 | 0.1 | Wear rounds the tip and roughens the surface |
| `pick_angle` | 0 – 60° | 20 | Attack angle; more angle is less click, more chirp |
| `use_fingers` | bool | false | Exists today; disables pick noise, enables nail/flesh |

### 2.1 Materials

| Material | Density g/cm³ | Damping | Character |
|---|---|---|---|
| Celluloid | 1.40 | 0.35 | The default. Warm click, moderate chirp. |
| Nylon | 1.15 | 0.55 | Softest click, most damped, quietest chirp. |
| Delrin | 1.41 | 0.30 | Slick surface; low chirp, crisp click. |
| Ultex / PEI | 1.27 | 0.15 | Hard and bright, loudest click, sharpest chirp. |
| Tortex | 1.30 | 0.40 | Matte surface; textured chirp. |
| Metal | 8.00 | 0.05 | Very bright, very loud, strong chirp. |
| Stone / horn | 2.60 | 0.10 | Bright with a lower fundamental than metal. |
| Wood | 0.70 | 0.60 | Dull, soft, short. |

Density sets the click's fundamental; damping sets its decay. Surface
roughness (implied by material, modified by wear) sets chirp and scrape
texture.

## 3. Click

Triggered on every picked note.

- **Level**: `pick_click_amount × velocity^0.7 × stiffness(thickness)`,
  where the nominal level at amount 0.5 and velocity 100 sits 30 dB below
  the note's peak.
- **Fundamental**: `f ≈ 1800 · (1.4 / density)^0.5 · (0.73 / thickness)^0.4`
  Hz, i.e. a light nylon pick clicks around 2.2 kHz and a thick metal one
  around 900 Hz.
- **Decay**: 3-15 ms, shorter with higher damping and sharper tip.
- **Angle**: click level falls as `cos(angle)^1.5`. A pick held flat at 0°
  clicks hardest; at 45° it mostly slides.
- **Wear**: adds a second, lower resonance and lengthens decay slightly.
- **Position**: the click is brighter near the bridge, following the same
  `pluck_position` the string excitation already uses.

## 4. Chirp

Triggered on release from a **wound** string only. Plain strings have no
winding for the pick to cross, so they produce no chirp - and modelling
that difference is most of why chirp is worth having.

- **Level**: `pick_chirp_amount × sin(angle) × roughness × windingDepth`.
  Zero at 0° attack angle, maximum around 45°.
- **Spectrum**: a band of noise centred on `windingPitch × pickSpeed`,
  where `windingPitch` is the string's winding wraps per mm (from the
  string part, `part-acoustics.md`) and `pickSpeed` derives from velocity.
  This produces the characteristic upward chirp as the pick accelerates
  across the winding.
- **Duration**: 8-40 ms.
- Chirp is what makes a wound string sound wound on attack, and it is the
  single most missed detail in modelled guitars.

## 5. Scrape (rake)

A deliberate drag along the wound strings, not a side effect.

- Triggered by the `pick_scrape` MIDI event class (`midi-export.md`) or by
  the Easy-mode rake gesture (`gui-integration.md`).
- Duration is the gesture's, 100 ms – 3 s.
- Spectrum sweeps as the pick crosses successive strings, each crossing
  producing a chirp-like band plus a broadband component.
- **Level**: `pick_scrape_amount`, default low, because a scrape at full
  level is a special effect.
- Scrape uses the 8-generator pool: more than a couple of simultaneous
  rakes is not a thing.

## 6. Fingers

`use_fingers` on: click, chirp and scrape are all silent and replaced by

- **Nail-vs-flesh** (`nail_vs_flesh`, exists today) crossfading two
  excitation shapes: nail is brighter and shorter, flesh is duller and
  longer.
- **Fingertip release noise**, a much softer equivalent of chirp, present
  on wound strings and roughly 12 dB below the pick's.

## 7. Parameters

All in the `pick` family (`advanced-ranges.md` 2).

| ID | Stock | Advanced | Default |
|---|---|---|---|
| `pick_material` | choice | – | Celluloid |
| `pick_thickness` | 0.38 – 3.0 mm | 0.1 – 10 mm | 0.73 |
| `pick_tip_radius` | 0.2 – 4.0 mm | 0.05 – 20 mm | 1.0 |
| `pick_bevel` | 0 – 1 | 0 – 1 | 0.2 |
| `pick_wear` | 0 – 1 | 0 – 1 | 0.1 |
| `pick_angle` | 0 – 60° | 0 – 89° | 20 |
| `pick_click_amount` | 0 – 1 | 0 – 4 | 0.5 |
| `pick_chirp_amount` | 0 – 1 | 0 – 4 | 0.4 |
| `pick_scrape_amount` | 0 – 1 | 0 – 4 | 0.25 |

Existing `pickMaterial`, `pickThickness`, `pickAngle` parameters are
re-pointed at this module rather than duplicated. Net new: **+6**.

## 8. UI

`gui-integration.md` 4.4 puts the **PICK** group on the CHARACTER tab:
pick and strum striker dropdowns, material, thickness, tip, bevel, wear,
angle, click and chirp amounts, pick-scrape amount.

The illustration draws the pick at true relative size, rotated by attack
angle, with 8 px accent drag handles (`gui-integration.md` 21). Dragging
the pick in the illustration sets angle; the handles resize thickness.

## 9. Tests

- **Click scales with velocity** as `velocity^0.7` within 1 dB across
  velocities 20-127.
- **Click fundamental tracks material and thickness.** Nylon 0.6 mm clicks
  at least an octave above metal 3 mm; measured by spectral centroid of the
  first 10 ms.
- **Angle reduces click and raises chirp.** At 0° click is maximal and
  chirp is silent; at 45° chirp is within 3 dB of its maximum.
- **Plain strings never chirp.** Assert the chirp generator is never
  triggered on an unwound string, for every factory guitar.
- **Noise rides the instrument.** The same click through two different
  bodies produces measurably different spectra; through the same body with
  amount 0 produces bit-identical output to noise disabled.
- **Zero is free.** With all three amounts at 0, no generator is allocated
  from the pool over 10 000 notes.
- **Pool exhaustion steals the oldest.** Trigger 20 simultaneous clicks
  into a 16-pool; assert 16 sound, the 4 oldest were stolen, and nothing
  allocated.
- **No allocation on the audio thread** for any noise event.
- **Determinism.** A fixed seed produces byte-identical noise for the same
  event sequence (`character-wear.md` 0.1's rule applies here too).
