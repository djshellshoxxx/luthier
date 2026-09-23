# FRET BUZZ SPEC

Every guitar buzzes somewhere. A well set-up one buzzes only when you dig
in hard on a low fret; a badly set-up one buzzes on the open D string. The
buzz is the string striking a fret it is not being held against, and where
it happens is a direct consequence of four measurements a tech takes with
feeler gauges: action, neck relief, nut slot depth and fret height.

Luthier has none of them. This file adds the setup geometry, computes
clearance from it, and produces buzz when the string's actual vibration
exceeds that clearance.

The payoff is not the buzz. It is that **the guitar acquires a setup** -
low and fast with a bit of rattle, or high and clean, and the trade-off
between them becomes something a player can feel.

## 0. Ground rules

1. **Buzz is sensed, not scheduled.** The generator fires when the modelled
   string amplitude at a fret exceeds the clearance there. It is never
   triggered by "a loud note" or a probability roll.
2. **Geometry in real units.** Action in mm at the 12th fret, relief in mm
   at the 7th, nut slot depth in mm, fret height in mm. These are what a
   tech measures and what a spec sheet quotes.
3. **Low action is a feature.** A player who wants a fast neck and accepts
   rattle should be able to have it. The model does not protect them.
4. **Honest magnitudes.** Light buzz on a decent setup is 30-40 dB below
   the note and only on the attack. Continuous audible buzz means the setup
   is genuinely bad.
5. **Sitar mode is separate.** Deliberate permanent buzz is a different
   feature from accidental buzz and gets its own toggle, because a player
   who wants it does not want to fight the setup to get it.

## 1. The geometry

| Field | Range | Default | What a tech means |
|---|---|---|---|
| `setup_action_treble` | 1.0 – 3.0 mm | 1.6 | String-to-fret at fret 12, high E |
| `setup_action_bass` | 1.2 – 3.5 mm | 2.0 | Same, low E. Bass side always higher. |
| `setup_relief` | -0.05 – 0.50 mm | 0.20 | Gap at fret 7 with the string fretted at 1 and 14. Negative is back-bow. |
| `setup_nut_depth[6]` | 0.0 – 1.2 mm | per string | Slot depth; open-string clearance |
| `setup_fret_height` | 0.6 – 1.6 mm | 1.0 | Crown height above the board |
| `setup_buzz_threshold` | 0 – 1 | 0.35 | Sensitivity trim, see 3.2 |
| `setup_sitar_mode` | bool | false | Deliberate buzz, section 5 |

Action at any fret *n* is interpolated from the nut clearance, the relief
curve and the 12th-fret action, with the relief modelled as a parabola
peaking at fret 7 - which is what a truss rod actually produces.

```
clearance(string, fret) =
      lerp(nutClearance, action12, fretFraction(fret))
    + reliefCurve(fret) × setup_relief
    - (playedFret > 0 ? 0 : 0)              // open strings sit on the nut
    - frettedOffset(playedFret, fret)        // frets behind the finger
```

Only frets **between the fretted position and the bridge** can buzz.
Fretting at 5 makes frets 1-4 irrelevant; the string is lifted off them.

## 2. Amplitude at a fret

The string model already knows the string's displacement envelope. Its
amplitude at a point *x* along the string, for the fundamental plus the
first few modes, is

```
amp(x, t) = Σ_k  A_k(t) · sin(k π x / L)
```

The buzz test needs the amplitude at each fret position between the
fretted note and the bridge, which is a handful of sine evaluations per
string per block - not per sample. The envelope is slow; testing at block
rate is sufficient and is what the 0.2-unit budget
(`performance-budget.md` 1, `NoiseEngine::FretBuzz`) pays for.

## 3. When buzz happens

### 3.1 The test

For each string, each block, find the fret where `amp(x_fret) - clearance`
is largest. If it is positive, the string is hitting that fret.

**Excess** = `amp - clearance`, in mm. Excess drives the buzz generator's
level; the fret's position drives its spectrum.

### 3.2 The threshold trim

`setup_buzz_threshold` shifts the test by up to ±0.15 mm. It exists
because the string model's absolute displacement is not calibrated against
a real instrument's millimetres to better than about that, and a user who
finds their setup buzzing more or less than they expect needs a trim that
is not "lie about the action".

It is a trim, not a mute. At 0 the model still buzzes on a genuinely bad
setup.

## 4. The buzz generator

`NoiseEngine::FretBuzz`, 16 generators (`pick-noise.md` 1).

- **Excitation**: a burst per contact, repeating at the string's
  fundamental while excess stays positive. A buzzing low E rattles at
  82 Hz, which is why buzz sounds pitched.
- **Spectrum**: metallic, centred high (3-6 kHz), with the centre rising
  as the contact fret rises. Fret material (`part-acoustics.md`) sets
  brightness: nickel-silver dull, stainless bright, gold-evo between.
- **Level**: `min(1, excess / 0.3 mm)` scaled by fret height - taller
  frets produce a harder, brighter contact.
- **Envelope**: 0.5 ms attack, decay tracking the excess. As the note
  decays the excess goes negative and the buzz stops, which produces the
  correct "buzzes on the attack, cleans up as it rings" behaviour without
  any special-casing.
- **Injection**: string output, pre-body (`pick-noise.md` 1.2). Also Aux 8.

## 5. Sitar mode

`setup_sitar_mode` on:

- A **jawari**-style curved bridge is modelled: the string maintains
  grazing contact through its whole decay rather than only at high
  amplitude.
- Buzz becomes continuous and pitched, with the characteristic long
  shimmering decay.
- The setup geometry still applies, but the threshold is bypassed.
- This is one toggle rather than a tunable bridge profile, because the
  users who want it want the sound, not the metallurgy.

## 6. Setup styles and UI

`gui-integration.md` 4.4 puts the **SETUP** group on the CHARACTER tab:
action treble and bass, relief, nut depth per string, fret height, buzz
threshold, sitar mode, and a **live buzz heatmap on a small fretboard**.

### 6.1 Setup styles

A dropdown, because six numbers is a lot to arrive at by feel.

| Style | Action T/B (mm) | Relief | Character |
|---|---|---|---|
| Factory low | 1.3 / 1.6 | 0.15 | Fast, buzzes when dug in below fret 5 |
| **Player-friendly** | **1.6 / 2.0** | **0.20** | **Ship default. Clean unless attacked hard.** |
| Clean / high | 2.1 / 2.6 | 0.25 | No buzz at any velocity; stiffer feel |
| Slide setup | 2.8 / 3.2 | 0.30 | High enough for a bottleneck; see `slide-guitar.md` |
| Blues / dug-in | 1.5 / 1.9 | 0.10 | Flat neck, rattles on purpose |
| Needs a tech | 1.1 / 1.3 | -0.03 | Back-bowed. Buzzes badly. For demonstration. |

`onboarding.md` 1's ship default is **Player-friendly**.

### 6.2 The buzz heatmap

A small fretboard drawn under the setup controls, each fret shaded by how
close that position is to buzzing right now:

- Warning colour for near-threshold (excess within 0.05 mm of zero).
- Accent for actively buzzing.
- A dot glyph as well as colour, so the map reads in monochrome
  (`gui-integration.md` 21).

It updates at 30 Hz per `gui-engine-dataflow.md` and greys after 2 s stale.

This is the control that makes the setup comprehensible: a user lowers the
action and watches the low frets light up.

## 7. Parameters

All in the `buzz` family (`advanced-ranges.md` 2).

| ID | Stock | Advanced |
|---|---|---|
| `setup_action_treble` | 1.0 – 3.0 mm | 0.2 – 10 mm |
| `setup_action_bass` | 1.2 – 3.5 mm | 0.2 – 12 mm |
| `setup_relief` | -0.05 – 0.50 mm | -0.5 – 2.0 mm |
| `setup_nut_depth` ×6 | 0.0 – 1.2 mm | 0.0 – 4.0 mm |
| `setup_fret_height` | 0.6 – 1.6 mm | 0.1 – 5.0 mm |
| `setup_buzz_threshold` | 0 – 1 | 0 – 1 |
| `setup_sitar_mode` | bool | – |
| `setup_style` | choice | – |

Net new parameters: **+16** (six nut depths).

## 8. Interaction

- **`character-wear.md` fret wear**: a worn fret is lower, so clearance at
  that fret *increases* and buzz there decreases - but the uneven crown
  raises buzz at its neighbours. Wear therefore moves buzz around rather
  than removing it, which is what makes an old neck rattle in new places.
- **`slide-guitar.md`**: slide setups are high by design; the Slide style
  above is what Slide Mode suggests on activation.
- **`bass-techniques.md`**: slap and pop deliberately drive the string into
  the frets. Bass buzz is a feature there, not a fault, and the bass
  defaults are lower-action than guitar.
- **`string-squeak.md`**: both can fire at once; no ducking.
- **Bends** raise the string over the fretboard slightly and reduce buzz
  at the fretted position while increasing it further up.

## 9. Tests

- **Low action buzzes, high action does not.** The "Needs a tech" style at
  velocity 100 produces buzz on the low E; "Clean / high" at velocity 127
  produces none.
- **Buzz stops as the note decays.** Measure buzz generator activity over a
  4 s note on Factory low: active in the first 300 ms, silent by 2 s.
- **Only frets ahead of the finger buzz.** Fret at 7 with a bad setup;
  assert no buzz generator is triggered for frets 1-6.
- **Relief moves where it buzzes.** With relief at 0.0 versus 0.35, assert
  the fret with maximum excess differs by at least 3 frets.
- **Fret height changes level not position.** Doubling fret height raises
  buzz level by at least 3 dB without changing the buzzing fret.
- **Threshold is a trim, not a mute.** At `setup_buzz_threshold` 0 with
  "Needs a tech", buzz is still produced.
- **Sitar mode is continuous.** Buzz generator activity over a 4 s note is
  continuous rather than attack-only.
- **Heatmap matches audio.** The fret the heatmap reports as buzzing is the
  fret the generator was triggered for, over a 30 s random performance.
- **Block-rate sensing costs what it claims.** Profile the buzz test at
  6 voices and assert it stays inside its 0.2-unit budget.
- **No allocation on the audio thread.**
