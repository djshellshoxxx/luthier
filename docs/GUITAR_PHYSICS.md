# The physics

Everything Luthier does comes from these equations. This document is the reference
for what the engine computes and why; if a number in the code disagrees with
something here, the code is wrong.

---

## 1. The string

### 1.1 Pitch

A string under tension vibrates at

```
        1     ____
f  =  ----- \/ T/mu
 0     2L
```

where `L` is the vibrating length in metres, `T` the tension in newtons and `mu`
the linear mass density in kilograms per metre.

Luthier runs this backwards. The user chooses a pitch and a string; the engine
solves for the tension that pitch implies:

```
T = mu * (2Lf)^2
```

and then checks that the answer is something a real string could take. That is
identity rule 1, and it is why choosing an absurd tuning shows an amber tension
readout in Advanced mode rather than silently producing a sound no instrument
could make.

### 1.2 Mass per unit length

For a solid wire,

```
mu = rho * pi * r^2
```

A wound string is not solid. Its windings leave gaps, so it weighs less than a
solid wire of the same outside diameter. The engine applies a per-material
`woundMassFactor` of roughly 0.74 to 0.86, which is what brings the computed
tensions into agreement with the figures string manufacturers print on the packet.

Sanity check, a .046 low E at 82.41 Hz on a 648 mm scale:

```
d      = 1.168 mm
r      = 0.584 mm
rho    = 7900 kg/m^3         (nickel-plated steel)
mu_solid = 7900 * pi * (0.000584)^2 = 8.46e-3 kg/m
mu     = 8.46e-3 * 0.78      = 6.60e-3 kg/m
T      = 6.60e-3 * (2 * 0.648 * 82.41)^2 = 75.3 N
```

which is 16.9 lbf, and a packet of .046 low Es says 17 lbf.

### 1.3 Inharmonicity

Real strings are stiff, so their partials are not exact multiples of the
fundamental:

```
            ______________
f  = n f  \/ 1 + B n^2
 n      0
```

The coefficient `B` comes from beam theory:

```
       pi^3  Q  d^4
B  =  -------------
       64  T  L^2
```

`d` here is the **core** diameter, not the outside diameter. Only the core resists
bending; the winding adds mass without adding stiffness. Using the outside diameter
would make a wound low E about seventy times stiffer than it is - the difference
between a guitar and a piano.

For the low E above, with a core of 0.45 x 1.168 = 0.526 mm:

```
B = 31.006 * 2.0e11 * (5.26e-4)^4 / (64 * 75.3 * 0.648^2)
  = 1.2e-4
```

which is in the measured range for a wound guitar string.

The engine realises this as a cascade of first-order all-pass sections in the
feedback loop. **The sign matters.** For an all-pass `H(z) = (a + z^-1)/(1 + a z^-1)`
the group delay is

```
              1 - a^2
tau(w) = ---------------------
          1 + 2a cos(w) + a^2
```

so at DC it is `(1-a)/(1+a)` and at Nyquist `(1+a)/(1-a)`. A **positive** `a`
therefore delays high frequencies *more*, which compresses the partials flat - the
opposite of stiffness. Luthier uses a negative coefficient. There is a unit test
that asserts exactly this, because getting it backwards produces something that
still sounds like a string and is wrong.

### 1.4 Decay

The loop loss is derived from a target T60 rather than dialled in by ear:

```
              -ln(1000) * D
g  =  exp( ------------------- )
              T60 * fs
```

where `D` is the loop length in samples. Solving from T60 rather than picking a
gain is what makes the decay behave sensibly when the pitch changes.

Higher notes must die sooner - identity rule 6 - so the target T60 itself scales
with pitch:

```
T60_effective = T60_base * (110 / f0)^0.40
```

An open low E gets about 5 s; the same string at the twelfth fret gets about 3.5 s;
a high E at the top of the neck gets under 2 s. That matches a real instrument, and
there is a test that asserts the ordering.

`g` is hard-capped at 0.9995. No parameter combination can reach unity.

### 1.5 Frequency-dependent damping

A one-pole lowpass in the feedback loop makes high partials decay faster than low
ones, which is the single most important thing about how a plucked string sounds
over time. Its cutoff is the physical control:

| State | Cutoff |
|---|---|
| Open, fresh steel | ~5.6 kHz |
| Open, old strings | ~2.7 kHz |
| Nylon | ~3.0 kHz |
| Light left-hand touch | ~2.0 kHz |
| Palm mute | ~800 Hz |
| Choked | ~500 Hz |

The filter has unity gain at DC, so it shapes the spectrum without affecting the
fundamental's decay - that is `g`'s job.

### 1.6 Tuning the loop

The loop filter and the dispersion cascade both add delay, so the delay line must
be shortened to compensate or every note plays flat:

```
D_line = fs/f0 - tau_loopfilter(0) - N * tau_allpass(0)
```

with `tau_loopfilter(0) = a/(1-a)` for the one-pole and `tau_allpass(0) = (1-c)/(1+c)`
for each all-pass section.

At high pitches the compensation can exceed the loop itself, so the engine reduces
the number of active dispersion stages until they fit inside 30% of the loop. A
high fretted note therefore has slightly less stiffness modelling than an open low
string - which is also true of the real instrument.

### 1.7 Excitation

Not noise, and not a click. Plucking displaces the string into a triangle with its
apex at the pluck point; releasing launches that shape into the waveguide.

1. Build a triangle of length `round(D * pluckPosition)`, with its apex skewed by
   the pick angle.
2. Subtract a copy delayed by twice the pluck length. This creates the spectral
   notch at the pluck position's node - the partial whose node sits under the pick
   cannot be excited, which is why the same string plucked in different places
   sounds so different.
3. Filter by the contact material: a lowpass at its contact bandwidth plus a
   peaking resonance at its characteristic frequency.
4. Scale by velocity - **and raise the cutoff with it**. A harder pluck deforms the
   string into a sharper corner, so it is genuinely brighter, not just louder. That
   is identity rule 5, and there is a test that measures the spectral shift.

| Contact | Bandwidth | Resonance |
|---|---|---|
| Felt pick | 900 Hz | 600 Hz, +1 dB |
| Thumb | 1.1 kHz | 700 Hz, +1 dB |
| Fingertip | 2.2 kHz | 1.2 kHz, +1.5 dB |
| Wood pick | 4.0 kHz | 2.0 kHz, +2 dB |
| Nylon pick | 5.0 kHz | 2.6 kHz, +3 dB |
| Celluloid pick | 6.0 kHz | 3.0 kHz, +4 dB |
| Fingernail | 7.0 kHz | 4.2 kHz, +5 dB |
| Metal pick | 9.0 kHz | 5.0 kHz, +6 dB |

### 1.8 Fret buzz

When the action is low and the string is driven hard, it hits the frets. The
engine models this where it happens - in the loop, on the signal itself:

```
if |x| > threshold:
    excess  = |x| - threshold
    clipped = threshold + excess / (1 + 9 * excess * buzzAmount)
    x       = sign(x) * clipped + noise * excess * buzzAmount * 0.3
```

The peaks are limited by the fret wire and the contact adds a bright rattle. The
threshold comes from the action height, so lowering the action really does make the
instrument buzz more. Because the nonlinearity only ever *reduces* amplitude, it
cannot destabilise the loop.

---

## 2. Sympathetic coupling

Every string shares one bridge with every other. When one vibrates it moves the
bridge, and the bridge moves the others; each responds most strongly near its own
partials. That ring is a large part of why a guitar sounds like a guitar, which is
why identity rule 7 says it is never switchable off.

Per sample, for each string `i`:

```
raw_i  = sum over j != i of  M[i][j] * bridge_j
recv_i = bandpass(raw_i, centred on f0_i, Q = 1.1)
recv_i = dcblock(recv_i)
recv_i = clamp(recv_i, +/- 0.35)
```

`M` is symmetric with a zero diagonal, because the bridge is a passive mechanical
link. Coupling falls off with string distance - adjacent saddles share more of the
bridge - with a floor so the far strings still ring a little, which is audible on
open chords.

The clamp is what makes runaway impossible. There is a test that feeds the matrix
its own output at four times gain for two seconds and asserts the result stays
bounded.

Running this per sample rather than per block costs at most 144 multiply-adds for
twelve strings, and avoids injecting a step into every delay line at each buffer
boundary.

---

## 3. The body

### 3.1 Air resonance

The soundhole and the enclosed air form a Helmholtz resonator:

```
         c      ____________
f  =  ------- \/  A / (V L)
 H     2 pi
```

with `A` the hole area, `V` the enclosed volume and `L` the hole's effective
length - its plate thickness plus an end correction of `1.7r`.

For a dreadnought: a 102 mm hole, 17.5 litres, a 2.8 mm top.

```
r = 0.051 m,  A = 8.17e-3 m^2
L = 0.0028 + 1.7 * 0.051 = 0.0895 m
f = 54.6 * sqrt(8.17e-3 / (0.0175 * 0.0895)) = 124 Hz
```

Published dreadnought air resonances run 95 to 125 Hz.

Because this is computed rather than stored, changing the body size or the
soundhole diameter in Advanced mode moves the resonance correctly. A solid body has
no cavity and gets no air mode at all.

### 3.2 Plate modes

From thin-plate theory, a clamped circular plate's modes are

```
         lambda        t        ______________________
f     = --------- * ------ * \/ Q / (12 rho (1 - v^2))
 mn       2 pi       a^2
```

with `lambda` the mode's eigenvalue (10.22 for the fundamental, then 21.26, 34.88,
39.77 and so on), `t` the thickness, `a` the plate radius, `Q` Young's modulus and
`v` Poisson's ratio.

A spruce dreadnought top, 2.8 mm thick over a 190 mm radius:

```
sqrt(11e9 / (12 * 430 * 0.91)) = 1531
f = 1.627 * (0.0028 / 0.0361) * 1531 = 193 Hz
```

Measured guitar top resonances sit at 180 to 200 Hz.

Bracing enters as a stiffness multiplier: X-brace 1.00, fan 0.88, ladder 0.82,
solid body 3.20. Mode Q comes from the wood's loss factor, and age raises it,
which is the "opened up" quality of an old instrument.

Above about 1 kHz the response stops being a handful of clean modes and becomes a
dense, irregular thicket. The engine reproduces that with a deterministic scatter
seeded from the body configuration, because a neat harmonic series there sounds
synthetic.

### 3.3 Convolution or modal

Both are built.

**Convolution** is cheaper and reproduces whatever body was captured, but it is
fixed - changing the body size does nothing to an IR.

**Modal** costs more but responds: a bigger virtual body really does ring lower,
because the equations above are evaluated live.

When a preset asks for convolution and no matching IR exists, the engine switches
to modal rather than passing the strings through untouched. Dropping the body
silently would break identity rule 3.

---

## 4. The pickup

A magnetic pickup does two separate things, and conflating them is the usual way
to get this wrong.

### 4.1 It samples one point on the string

A pickup at fraction `p` along the string cannot sense any partial with a node
there. In the waveguide this is a comb:

```
y[n] = x[n] - x[n - 2 p D]
```

with `D` the string's loop length. Partials at `n` where `n p` is an integer are
nulled. A bridge pickup (small `p`) only nulls high harmonics, so it is bright; a
neck pickup nulls lower ones, so it is warm.

**This delay depends on the string's own length**, so the comb has to run per
string, before the sum. Applying it after summing would give every string the same
comb. There is a test that drives a pickup at `p = 1/4` with the 4th harmonic and
asserts it is at least 12 dB down on the 3rd.

### 4.2 It is a coil

An inductor with parasitic resistance and capacitance - a second-order lowpass with
a resonant peak:

```
                1                        1      ____
f    = ----------------- ,      Q  =  ----- \/ L/C
 res    2 pi sqrt(L C)                   R
```

| Pickup | L | C | R | Resonance |
|---|---|---|---|---|
| Single coil | 2.5 H | 200 pF | 6 kohm | 7.1 kHz |
| P90 | 4.0 H | 200 pF | 8 kohm | 5.6 kHz |
| Humbucker | 6.0 H | 180 pF | 8 kohm | 4.8 kHz |

That single number is most of why a humbucker sounds darker than a single coil.
This stage runs **once, on the summed coil signal**, because a real pickup has one
winding for all six strings.

### 4.3 A humbucker is two coils

Two single-coil models at positions `p ± spacing/2`, summed, with the second wound
in reverse. Externally-induced mains hum arrives equally at both and cancels; the
string signal does not, because the coils are at different points along the string.
The comb between them is what makes a humbucker sound like a humbucker - modelling
it as "a single coil with extra bass" gets it wrong.

### 4.4 Magnets and height

Magnet type is a one-band EQ: Alnico 2 lifts around 800 Hz, Alnico 3 is nearly
neutral, Alnico 5 lifts the upper mids, ceramic is a bright high shelf.

Height scales output as roughly `3.2 / h`: closer is louder and slightly brighter,
because the field gradient the string moves through is steeper.

### 4.5 Non-magnetic transducers

A **piezo** senses bridge compression, not the magnetic field. It takes the raw
string sum, highpassed at 40 Hz, with a 3 kHz resonance and a 15 kHz lowpass. It
has no positional comb, because it is not at a position along the string.

An **internal mic** takes the body output with a gentle tilt.

---

## 5. The amplifier

### 5.1 Preamp stages

Each 12AX7 stage is: gain, an asymmetric transfer curve, a cathode-bypass low-mid
lift, a Miller-capacitance roll-off, and an interstage coupling capacitor.

```
y = 1.5 tanh(x) - 0.5 tanh(x - bias)
```

The asymmetry generates even harmonics, which is what makes valve distortion sound
warm rather than buzzy. The coupling capacitor then removes the DC that asymmetry
creates, so the next stage's bias point does not walk away.

Cascading stages is what turns a clean amp into a high-gain one. Each successive
stage is a little tighter in the bass, which is what keeps a five-stage amp from
turning into mud.

### 5.2 The tone stack

Modelled from the circuit, not as cascaded EQ bands. The three-pot passive network
has the transfer function

```
       B1 s + B2 s^2 + B3 s^3
H(s) = -----------------------
       1 + A1 s + A2 s^2 + A3 s^3
```

where each coefficient is a polynomial in the three pot positions and the six
component values - the standard nodal-analysis result for the Fender/Marshall
topology. It is discretised with the bilinear transform into a third-order IIR.

This matters because the controls **interact**. Turning the mids down also moves
where the bass and treble corners land, and the network's insertion loss changes
with the settings. A chain of independent shelving filters cannot do that, and a
guitarist notices immediately - it is why a Marshall with the mids at zero sounds
scooped rather than just quieter in the middle.

Component sets:

| Amp | R1 | R2 | R3 | R4 | C1 | C2 | C3 |
|---|---|---|---|---|---|---|---|
| Fender | 250k | 1M | 25k | 56k | 250 pF | 20 nF | 20 nF |
| Marshall | 220k | 1M | 22k | 33k | 470 pF | 22 nF | 22 nF |
| Vox | 1M | 1M | 10k | 100k | 50 pF | 22 nF | 22 nF |
| Modern | 250k | 1M | 25k | 100k | 500 pF | 22 nF | 47 nF |

A passive stack throws away 15 to 25 dB, and real amps make it back in the
following stage. So does this one - otherwise every amp model would be
mysteriously quiet.

### 5.3 Power amp

A long-tailed-pair phase inverter with a deliberate 4% imbalance - a real inverter
is not perfectly balanced, and that asymmetry is what stops the push-pull pair from
cancelling every even harmonic.

Two power tubes, each a waveshaper whose knee comes from its type:

| Tube | Knee | Asymmetry | Compression |
|---|---|---|---|
| EL84 | 1.05 | 0.22 | 0.85 |
| 6V6 | 1.20 | 0.19 | 0.74 |
| EL34 | 1.35 | 0.16 | 0.62 |
| 6L6 | 1.75 | 0.09 | 0.40 |
| KT88 | 2.20 | 0.06 | 0.26 |

### 5.4 Sag

Under load the supply voltage droops, lowering the headroom and compressing the
dynamics. The droop follows the envelope, not the sample:

```
target = 1 - clamp(envelope * sagAmount * 0.9, 0, 0.55)
supply = target + (supply - target) * coeff
```

with a 20 ms attack and a 350 ms recovery. That time constant is what makes an amp
breathe.

### 5.5 Presence

A high shelf inside the negative-feedback loop. It works by *removing* feedback at
high frequencies, so its effect scales with how much feedback the amp has. A Vox
AC30 has almost none, so its presence control does almost nothing - which is true
of the real amp.

### 5.6 Oversampling

Every nonlinear stage runs at 4x by default through a polyphase half-band filter
with roughly -70 dB stopband. Without it, the harmonics a waveshaper generates fold
back down the spectrum as inharmonic hash, and that is the single biggest thing
that makes an amateur guitar plugin sound digital. There is a test that measures
the aliasing energy with and without.

---

## 6. The cabinet

A speaker in a box in front of a microphone is a linear system, so convolution
reproduces it exactly.

When no IR is available the engine falls back to a procedural model rather than
passing the amp through raw: a highpass at the cone's low corner, a low shelf for
the cabinet's body, the microphone's proximity bump, the cone-breakup peak, and a
steep roll-off above it. That last one is the most recognisable feature of any
guitar cabinet - a 12-inch speaker is effectively deaf above 5 kHz - and it is the
difference between a usable sound and a wasp in a tin.

The synthesised cabinet IRs add two things a pure magnitude design would miss:

- the **cabinet wall reflection**, a delayed, filtered, polarity-inverted copy
  arriving after `2 * depth / c`;
- a **floor bounce**, present in every close-miked capture.

With two microphones, their time-of-flight difference is compensated. Getting that
wrong is what makes a two-mic blend sound thin.

---

## 7. The room

**Early reflections**: sixteen taps whose times come from the room's characteristic
dimension over the speed of sound, with irregular spacing - parallel walls in a
real room are never quite parallel, and evenly-spaced taps ring. Each reflection's
level follows the number of surfaces it has bounced off and the material's
absorption.

**Late reverb**: an eight-line feedback delay network with mutually prime lengths,
per-line damping, and a Householder feedback matrix - which is lossless and mixes
every line into every other, giving a smooth, dense tail.

The late field is fed from the early reflections rather than from the dry signal,
because in a real room the diffuse field is built up by the early bounces.

---

## 8. Things that are approximations

Stated plainly, because a model that hides its approximations is harder to trust.

- **The string is a lumped delay line**, not a bidirectional waveguide with an
  explicit bridge and nut. The two are equivalent for the observable output, but
  this formulation cannot expose the travelling waves separately.
- **Dispersion is bounded.** The all-pass cascade produces the correct *direction*
  and a realistic magnitude for guitar-range `B`, but it cannot reach the extreme
  stretching a piano's bass strings show.
- **The body does not feed back into the string.** On a real acoustic the top loads
  the string, shifting its decay slightly. Here the coupling is one-way.
- **The pickup does not load the string.** Magnetic pull damps a real string
  slightly, which is why a pickup set very high kills sustain. Height changes gain
  and brightness here, not decay.
- **Impulse responses are synthesised, not measured.** See the README.
- **The tone stack ignores the following stage's input impedance**, which in a real
  amp loads the network slightly.
- **Speaker cone breakup is linear.** Real speakers distort at high excursion.
