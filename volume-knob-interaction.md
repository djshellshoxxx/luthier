# GUITAR CIRCUIT SPEC

*(File name is `volume-knob-interaction.md` because the volume knob is what
sent everyone looking. The module is `GuitarCircuit`.)*

On a real passive guitar, turning the volume knob down does not just make
it quieter. It makes it duller, because the pot, the pickup's inductance,
the cable's capacitance and the amp's input impedance form one network, and
moving the wiper moves the network's corner frequency. Every guitarist
knows this: roll back to 7 for a cleaner rhythm sound and you lose top as
well as level, which is why treble-bleed caps exist and why people argue
about 250 k versus 500 k pots.

Luthier currently models none of it. `CableSim` is a one-pole lowpass whose
cutoff depends on cable length, sitting after a pickup whose output does not
know the pot exists. The volume parameter is a gain multiply.

This file replaces `CableSim` with `GuitarCircuit`. Per
`CLAUDE_CODE_BRIEF.md`'s conflict list item 9, `CableSim` is **removed, not
deprecated**.

## 0. Ground rules

1. **One network, solved once.** Pickup, volume pot, tone pot, tone cap,
   treble bleed, cable and amp input are a single circuit. They are not a
   chain of independent filters, and modelling them as one is the entire
   point of this file.
2. **Passive by default.** The stock guitar is passive, so the loading is
   real. An active guitar buffers the pickup and most of the interaction
   disappears - which is itself a thing players choose, so it is a toggle.
3. **Component values, not tone controls.** The user sets a pot value in
   ohms and a cap in farads, because that is what they buy. The response is
   whatever those parts produce.
4. **Honest magnitudes** (`INDEX.md` global rules). Rolling volume from 10
   to 7 on a 250 k pot with a 3 m cable loses a few dB at 5 kHz, not a
   wah-pedal sweep.
5. **No new audio thread allocation.** Coefficients are recomputed on the
   message thread when a component changes and swapped in; the audio thread
   evaluates a fixed-size filter.

## 1. The circuit

The passive network, from pickup to amp:

```
  pickup                volume pot            cable      amp in
  ┌──────────┐            ┌───┐
  │ Ls   Rs  │───┬────────┤   ├──┬─────────────╥────────┬──────
  │ (coil)   │   │        │ ↕ │  │             ║        │
  └──────────┘   │        └─┬─┘  │            Ccable   Rin
                 │          │    │             ║        │
              Cp (coil)   [gnd] Cbleed/Rbleed  ║        │
                 │          │    │             ║        │
                [gnd]     [gnd] ─┘            [gnd]   [gnd]

  tone pot + cap hang off the same node as the volume wiper input:
                 ├──── Rtone ──── Ctone ──── [gnd]
```

Components:

| Symbol | What | Source |
|---|---|---|
| `Ls`, `Rs` | Pickup coil inductance and DC resistance | `GuitarSpec` pickup part (`part-acoustics.md`) |
| `Cp` | Pickup self-capacitance | pickup part, typ. 100 pF |
| `Rvol` | Volume pot total resistance | `circuit_volume_pot` |
| `α` | Wiper position, 0..1 | `guitar_volume` |
| `Rtone` | Tone pot total resistance | `circuit_tone_pot` |
| `β` | Tone wiper, 0..1 | `guitar_tone` |
| `Ctone` | Tone capacitor | `circuit_tone_cap` |
| `Rbleed`, `Cbleed` | Treble bleed across the volume pot | `circuit_treble_bleed` preset |
| `Ccable` | Cable capacitance = length × pF/m | `cable_length`, `cable_quality` |
| `Rin` | Amp input impedance | `amp_input_impedance` |

### 1.1 Solving it

The network is linear and time-invariant for a given knob position, so it
has a transfer function. Deriving it symbolically and evaluating per sample
would be wasteful; instead:

1. On any component or knob change, compute the s-domain transfer function
   `H(s)` of the whole network at the current `α`, `β`.
2. The result is a **biquad pair**: the pickup resonance (a second-order
   peak set by `Ls` with `Cp + Ccable` loaded by the pots and `Rin`) and the
   tone-control rolloff (first order, promoted to a biquad with a zero when
   the treble bleed is present).
3. Bilinear-transform both at the current sample rate and hand the
   coefficients to the audio thread.

This is the standard approach and it is exact for a fixed knob position,
which is what matters: the interesting behaviour is *where* the resonance
moves as the pot turns, not any nonlinearity.

### 1.2 What the volume knob actually does

Three things happen at once as `α` falls from 1:

- **Level** drops, roughly as the pot's taper (audio taper by default).
- **The pickup sees a heavier load**, because the wiper puts part of the
  pot's resistance in series and part to ground. The resonant peak drops
  in frequency and flattens.
- **The cable capacitance is progressively disconnected** from the pickup
  by the series resistance, which slightly *raises* the corner again at
  very low settings.

The net is the familiar "gets dull as you turn down, then goes quiet". The
model produces this because it solves the network; it is not applied as a
separate "darkening" curve, and `INDEX.md`'s "physical deltas only" rule
forbids doing so.

### 1.3 Treble bleed

A resistor and capacitor across the volume pot's input and wiper. Three
stock configurations plus custom, because these are the three people
actually build:

| Preset | R | C | Character |
|---|---|---|---|
| None | – | – | Full interaction, darkens as you turn down |
| Kinman | 130 kΩ ∥ 1.1 nF | parallel RC | Keeps top, mild level taper change |
| Fender (cap only) | – | 1 nF | Brightens noticeably at low settings |
| Custom | user R, user C | series or parallel | `circuit_treble_bleed_mode` |

### 1.4 Active mode

`circuit_active` on:

- A unity-gain buffer is inserted immediately after the pickup.
- `Ccable` and `Rin` no longer load the pickup: the resonance stays put
  regardless of cable and amp.
- The volume pot becomes a plain attenuator after the buffer; turning down
  loses level and not top.
- The tone control becomes an active first-order lowpass with the same
  nominal corner, because that is what an active guitar's tone control
  does.

This is the correct model and it is also the thing to reach for when a user
complains their long cable is eating their tone.

## 2. Cable

`Ccable = length × capacitance_per_metre`.

| Quality | pF/m | Note |
|---|---|---|
| Studio | 52 | Low-capacitance instrument cable |
| Standard | 98 | The one in most gig bags |
| Cheap | 160 | Visibly duller over a long run |
| Vintage / coiled | 220 | Coiled cables are long and capacitive |

`cable_on` off bypasses cable capacitance entirely (equivalent to a
zero-length studio cable, not to silence).

Cable capacitance interacts with pot value: on a 500 k pot with a cheap
10 m cable the pickup resonance drops around a semitone-and-a-half's worth
of frequency compared with a 3 m studio cable, which is audible and is the
whole reason players care.

## 3. Parameters

All in the `circuit` family (`advanced-ranges.md` 2). Stock and advanced
pairs are in `advanced-ranges.md` 3.2; this table fixes IDs, defaults and
types.

| ID | Type | Default | Notes |
|---|---|---|---|
| `guitar_volume` | float 0–1 | 1.0 | Exists today; semantics change from gain to wiper |
| `guitar_tone` | float 0–1 | 1.0 | Exists today; same |
| `circuit_volume_pot` | float ohm | 500k | Audio taper |
| `circuit_tone_pot` | float ohm | 500k | |
| `circuit_tone_cap` | float F | 22n | |
| `circuit_pot_taper` | choice | Audio | Audio, Linear, 50s-wiring |
| `circuit_treble_bleed` | choice | None | None, Kinman, Fender, Custom |
| `circuit_bleed_r` | float ohm | 130k | Custom only |
| `circuit_bleed_c` | float F | 1.1n | Custom only |
| `circuit_bleed_mode` | choice | Parallel | Parallel, Series |
| `circuit_active` | bool | false | |
| `amp_input_impedance` | float ohm | 1M | |
| `cable_on` | bool | true | Exists today |
| `cable_length` | float m | 3.0 | Exists today; range changes per 3.2 |
| `cable_quality` | choice | Standard | Studio, Standard, Cheap, Vintage |

**Removed:** nothing from the parameter list. `cable_on` and `cable_length`
are kept and re-pointed at `GuitarCircuit`; the `CableSim` *module* goes.

Net parameter count change: **+9**.

### 3.1 50s wiring

`circuit_pot_taper = 50s-wiring` changes where the tone control taps the
volume pot (input side rather than wiper side). The audible result is that
the guitar keeps its top end as volume comes down, and that the tone
control's effect changes with volume. It is a wiring topology rather than a
taper, and it lives in this control because a user looking for it will look
here.

## 4. Interaction with the rest of the engine

- **Pickup**: `GuitarCircuit` needs `Ls`, `Rs`, `Cp` from the pickup part.
  Until `guitar-workshop.md` lands, they come from a table keyed by the
  existing `pickup_type` choice, in `part-acoustics.md`'s units.
- **Feedback** (`ambiguity-resolutions.md` 1): the feedback loop level is
  taken *after* the circuit, so volume at 5 reduces feedback by the
  circuit's measured attenuation within 0.5 dB. That resolution is already
  chosen and this file implements it.
- **Position in the chain**: `StringEngine → PickupEngine → GuitarCircuit →
  PreEffectsChain → Amp → ...`. Exactly where `CableSim` was.
- **Oversampling**: the circuit is linear, so it does not need it.

## 5. UI

Per `gui-integration.md` 4.2, the **CIRCUIT** panel replaces CABLE in
Advanced column 2:

- Guitar volume, guitar tone (large knobs - these are played, not set).
- Pot value dropdown (250k / 500k / 1M / custom), tone cap dropdown
  (22n / 47n / 10n / custom), taper dropdown.
- Treble bleed dropdown with an R/C editor that appears for Custom.
- Active / passive toggle.
- Cable length slider and quality dropdown.
- **Live circuit-response visualiser**: the magnitude response of `H(s)`
  from pickup to amp input, redrawn as any control moves, with the current
  resonant peak marked. Small in column 2; the CHARACTER tab's CIRCUIT
  group (`gui-integration.md` 4.4) shows the same panel at full size.

Easy mode shows the compact card (`gui-integration.md` 3): volume, tone and
the visualiser miniature.

The visualiser is the feature that teaches the interaction. A user turns
the volume knob and watches the peak slide down and flatten, and the thing
every guitarist half-knows becomes visible.

## 6. Tests

- **Volume interaction is real.** With a 500 k pot, 3 m standard cable and
  1 M input, measure the magnitude response at `guitar_volume` 1.0 and 0.7.
  Assert level drops within 0.5 dB of the audio taper's prediction *and*
  that the -3 dB corner moves down by at least 15%.
- **Treble bleed restores top.** Same measurement with the Kinman preset;
  assert the corner at 0.7 is within 5% of its position at 1.0.
- **Active mode removes loading.** With `circuit_active` on, the response
  at volume 1.0 and 0.7 differs by a constant gain within 0.1 dB across
  20 Hz – 20 kHz.
- **Pot value moves the resonance.** 250 k versus 1 M, all else equal:
  assert the resonant peak frequency differs by at least 10% and that the
  1 M case is higher.
- **Cable capacitance moves the resonance.** 1 m studio versus 10 m cheap:
  assert the peak drops by at least 20%.
- **Tone control sweeps.** `guitar_tone` from 1.0 to 0.0 moves the rolloff
  corner monotonically downward; no setting produces a gain above unity.
- **Bypass is neutral.** `cable_on` off and `circuit_active` on with pots
  at 1.0 produces a response flat within 0.1 dB from 20 Hz to 20 kHz.
- **Stability.** Sweep every component across its advanced range at 44.1,
  48, 96 and 192 kHz; assert every biquad is stable (poles inside the unit
  circle) and no output sample is NaN or above +24 dBFS.
- **No allocation on the audio thread** while sweeping every control, per
  `engine.md` 0.
- **Feedback coupling.** Per `ambiguity-resolutions.md` 1: volume at 5
  reduces feedback loop level by the circuit's measured attenuation within
  0.5 dB.
