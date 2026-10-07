# STRING SQUEAK SPEC

The sound of a finger sliding along a wound string. Every acoustic
recording has it, every guitarist stops hearing it after about a year of
playing, and its complete absence is one of the loudest tells that a
guitar track was not played on a guitar.

It is also the detail most often done badly: a sampled "squeak" layer
triggered on position change, always the same, always too loud. This file
specifies a generated squeak whose pitch, level and texture come from how
fast the finger is actually moving over which winding.

## 0. Ground rules

1. **Wound strings only.** A plain steel string has nothing for a
   fingertip to catch on and produces essentially no squeak. Modelling
   that difference correctly is most of the value.
2. **Squeak is a consequence, never an event the user fires.** It happens
   because a note moved. There is no "squeak now" control.
3. **Velocity of the hand, not velocity of the note.** Squeak level and
   pitch follow how fast the finger traverses the string, which is a
   property of the position change, not of how hard the note was struck.
4. **Honest magnitudes.** A normal position shift on a clean acoustic sits
   20-30 dB below the note. On an electric through a clean amp it is
   quieter still, because the pickup is not a microphone.
5. **Zero is silent and free**, as for every noise class.

## 1. The physics, briefly

A fingertip pressed on a wound string and moved along it rides over the
winding wraps. Each wrap the finger crosses is an impulse. Crossing them
at a steady rate produces a tone whose fundamental is

```
f_squeak = slideSpeed (mm/s) × windingPitch (wraps/mm)
```

A 0.046" wound low E has roughly 6.5 wraps/mm. A brisk position shift
moves the hand around 400 mm/s. That gives about 2.6 kHz, which is
precisely where squeak lives.

Because the hand accelerates and decelerates through a shift, `f_squeak`
glides - and that glide is what makes a real squeak sound like a finger
rather than a sine burst. The glide is bell-shaped: it rises from about
half the peak as the hand gets going, reaches the peak mid-shift and falls
back to about three quarters of it as the hand brakes.

The skin can only follow the wraps so fast. Below **600 Hz** the wraps are
felt as separate bumps rather than a tone; above **5 kHz** the fingertip
skates. The peak is clamped to that band, so a thin, finely wound string on
a long shift whistles at the ceiling rather than disappearing above it.

(The wrap pitch is computed from the string part, not the 6.5 wraps/mm
quoted above: a 0.046" phosphor-bronze low E has a 0.34 mm wrap, about 2.9
wraps/mm, and a brisk five-fret shift on it whistles around 2-3 kHz.)

## 2. Trigger detection

A squeak begins when **a fretted note's position changes while the string
is still sounding and the finger has not been lifted**.

Sources of position change, in `input-routing.md` order:

- A legato slide (`TechniqueEngine`'s slide).
- A `PerformanceScore` position change during tab playback.
- A `ChordVoicer` revoicing that moves a held note.
- Explicit MIDI slide events (`midi-export.md`'s `slide` class).

Not a trigger: a new pluck at a new position (the finger was lifted), a
bend (the finger does not travel along the string), vibrato (travel is
sub-wrap).

### 2.2 The hand shift between chords

A chord change lifts the fingers - but a hand that lifts off one chord and
lands on the next, further up the neck, brushes the wound strings on the
way. That is where most of the squeak on an acoustic recording comes from,
and ordinary plucked MIDI (note off, then note on) never produces a legato
shift, so without it the squeak was inaudible in normal playback.

A string's note released and then struck again **on the same string within
300 ms**, at least `squeak_min_travel` away, is a **lifted shift**. It is
the same generator as 2 at **75 % of the fretting pressure** (the finger is
leaving, not pressing), with the hand's speed taken over the gap, and it
rolls against the same probability. A note struck more than 300 ms after
the release is a new pluck and does not squeak.

### 2.1 Minimum travel

Below `squeak_min_travel` (default 1.5 frets) no squeak is generated. A
semitone shift produces a click at most, and generating squeak for every
tiny movement is what makes bad implementations chatter.

## 3. Level and pitch

```
level  = squeak_amount
       × windingDepth(string)          // plain strings → 0
       × pressure^1.3                  // squeak_finger_pressure
       × roughness(moisture)           // squeak_finger_moisture
       × min(1, speed / 300 mm/s)

pitch  = speed × windingPitch(string), glided over the shift
```

- `windingDepth` is 0 for plain strings and rises with winding gauge; it
  comes from the string part (`part-acoustics.md`).
- Speed is derived from the shift distance and the shift duration the
  technique engine already computes, so a fast shift squeaks higher and
  louder than a slow one over the same distance.
- The pressure and moisture terms are **normalised at their defaults**
  (0.5 and 0.35), so `squeak_amount` alone sets where a normal shift sits:
  amount 1 is 15 dB under the note at the generator, the ship default 0.25
  is 27 dB under, and the advanced ceiling of 4 is 3 dB under.
- The squeak's length follows the shift: the whistle holds for the shift
  and tails off over 20 ms after a fast one, 45 ms after a slow one.

### 3.1 No two alike

Every event scatters a little around the formula, deterministically from
the instance seed and the shift index: pitch +/- 9 %, level +/- 1.5 dB,
length +/- 15 %, the glide's starting ratio and the brightness. Two shifts
of the same distance on the same string never render the same twice, and
the same seed and sequence always render the same.

## 4. Per-material spectra

The winding material sets the texture on top of the fundamental.

| Winding | Brightness | Texture | Note |
|---|---|---|---|
| Phosphor bronze | 0.75 | Medium | Acoustic default; the classic squeak |
| 80/20 bronze | 0.85 | Coarse | Brighter, more prominent |
| Nickel-plated steel | 0.55 | Fine | Electric default; noticeably quieter |
| Pure nickel | 0.45 | Fine | Vintage electric; softest |
| Stainless steel | 0.90 | Coarse | Brightest and loudest |
| Flatwound | 0.10 | – | Almost none. This is why jazz players use them. |
| Halfwound / groundwound | 0.35 | Fine | Between flat and round |
| Coated (Elixir-type) | 0.40 | Fine | The coating is the selling point |

Each material has a pre-synthesised noise texture (`pick-noise.md` 1, the
40 MB budget) read at a per-event random offset, band-shaped around
`f_squeak` and its second and third harmonics with the material's
brightness setting their balance.

**Flatwound at 0.10 and coated at 0.40 are the two settings users will
reach for**, and they should hear the difference immediately.

## 5. Injection point

String output, **pre-body** (`pick-noise.md` 1.2). Squeak is surface noise
on the string, not string motion, so it does not go through the string
model - but it does go through body, pickup and circuit, which is why an
acoustic squeaks so much more obviously than a solidbody.

Also summed to Aux 8 (`routing-io.md`).

### 5.1 What the transducer hears

A squeak is a sound from the fingerboard. On an acoustic the mic hears it
**directly through the air** as well as through the top, so the internal
mic path adds 70 % of the raw squeak beside the body's version of it; that
is why a classical, whose body passes little above 2 kHz, still squeaks
audibly. The piezo hears only what reaches the saddle. A **magnetic
pickup** hears only the little squeak that moves the string: on an electric
or a bass the squeak is generated at half the acoustic's level (0.4).

## 6. Probability and moisture

Not every shift squeaks, and a model where every shift squeaks sounds
mechanical.

- `squeak_probability` (default 0.65) is the chance a qualifying shift
  produces an audible squeak at all. The actual chance is
  `p × (1.35 - moisture)^(1 - p)`: moisture pulls it down in the middle of
  the range, but **1 is every qualifying shift and 0 is none**, whatever the
  fingers are like.
- The roll is **deterministic per instance seed and note index**
  (`character-wear.md` 1), so a repeated performance repeats exactly.
- `squeak_finger_moisture` (0-1, default 0.35) is the physical reason:
  dry fingers catch and squeak, damp fingers slide silently. Moisture
  lowers both probability and brightness.

## 7. Pressure

`squeak_finger_pressure` (0-1, default 0.5) is how hard the finger presses
while moving. It raises level and coarsens texture. A player who releases
pressure slightly during a shift - which is what experienced players do -
is modelling `pressure` low, and gets the near-silent shift they get in
real life.

`bass-techniques.md` sets a lower default for bass, where shifts are
longer and rounder.

## 8. Style presets

One dropdown, because the individual controls are a lot to reason about.

| Preset | Amount | Probability | Moisture | Pressure |
|---|---|---|---|---|
| Silent | 0.0 | – | – | – |
| Studio (polished) | 0.15 | 0.35 | 0.60 | 0.35 |
| Natural (default) | 0.35 | 0.65 | 0.35 | 0.50 |
| Folk / close-mic | 0.60 | 0.80 | 0.20 | 0.65 |
| Exaggerated | 1.00 | 1.00 | 0.05 | 0.90 |

`onboarding.md` 1 fixes the ship default at 25% amount; "Natural" is
selected with `squeak_amount` at 0.25 rather than 0.35 on first run, and
the dropdown reads "Natural (modified)" until the user moves it.

## 9. Parameters and UI

All in the `squeak` family (`advanced-ranges.md` 2).

| ID | Stock | Advanced | Default |
|---|---|---|---|
| `squeak_amount` | 0 – 1 | 0 – 4 | 0.25 |
| `squeak_probability` | 0 – 1 | 0 – 1 | 0.65 |
| `squeak_finger_moisture` | 0 – 1 | 0 – 1 | 0.35 |
| `squeak_finger_pressure` | 0 – 1 | 0 – 1 | 0.50 |
| `squeak_min_travel` | 1 – 4 frets | 0.25 – 12 frets | 1.5 |
| `squeak_style` | choice | – | Natural |

Net new parameters: **+6**.

`gui-integration.md` 4.4 puts the **STRING NOISE** group on the CHARACTER
tab, containing exactly: squeak amount, probability, finger moisture,
finger pressure, the per-material profile selector, the style presets
dropdown, and the **noise-event strip**.

### 9.1 The noise-event strip

A 24 px horizontal strip showing the last 8 seconds of noise events as
ticks - squeak, click, chirp, buzz - coloured by class, height by level.
It is the diagnostic that answers "what is that noise?", and it is the
reason a user can tune these controls at all.

Per `gui-engine-dataflow.md`'s rules it drains at 30 Hz and greys out
after 2 s of staleness.

The winding material selector here is a **mirror** of the string part in
the Workshop, which is canonical (`gui-integration.md` 0.1 allows a mirror
where the canonical home is a different mode). It shows the current
material and jumps to the Workshop to change it, until
`guitar-workshop.md` lands, at which point it edits the part directly.

## 10. Interaction with other modules

- **`fret-buzz.md`**: a shift across a buzzing region produces both. They
  are separate generators and both are audible; no ducking.
- **`slide-guitar.md`**: with Slide Mode on, finger squeak is replaced by
  the slide's own clank and friction noise. A bottleneck is not a
  fingertip, and running both is wrong.
- **`character-wear.md`**: fret wear raises squeak slightly (a worn fret
  lets the string sit closer to the winding's catch point).
- **String age** (existing parameter): old strings squeak more, because
  the winding is dirtier. Age multiplies roughness by up to 1.4.

## 11. MIDI export

`midi-export.md`'s `squeak` event class carries, per event: string, start
position, end position (frets, -1 for a scrape's drag), duration, level. In Luthier profile a re-import
reproduces the squeak exactly; in Generic profile squeak events are
dropped, because there is no standard way to say "finger noise" and
inventing one would make the file unreadable elsewhere.

## 12. Performance and the generator pool

16 generators (`performance-budget.md` 1, `NoiseEngine::Squeak`, budget
0.4 units). Sixteen is generous - a six-string guitar cannot physically
produce more than six simultaneous shifts - and the headroom exists because
a fast passage overlaps the tails of previous squeaks.

Degradation step 4 drops the pool to 8, which is still above the physical
maximum, so squeak degrades last among the noise classes.

Textures are synthesised once per material at load, not per event. A
squeak event costs one table read, three biquads and an envelope.

## 13. Tests

- **Plain strings never squeak.** Sweep every factory guitar; assert the
  squeak generator is never triggered on an unwound string.
- **Pitch tracks speed and winding.** A 300 mm/s shift on a 6.5 wraps/mm
  string produces a measured spectral peak within 10% of 1.95 kHz; doubling
  the speed doubles the peak.
- **Glide is present.** The measured peak at the start of a shift differs
  from the peak at its middle by at least 15% for an accelerating shift.
- **Flatwound is near-silent.** Same shift on flatwound versus phosphor
  bronze: at least 15 dB quieter.
- **Minimum travel is respected.** A 1-fret shift with `squeak_min_travel`
  at 1.5 produces no generator; a 2-fret shift does.
- **Probability is deterministic.** The same seed and note sequence
  produces a byte-identical squeak pattern across runs; a different seed
  produces a different one.
- **Bend and vibrato do not squeak.** A full-tone bend and 6 Hz vibrato at
  any depth produce no squeak events.
- **Slide Mode suppresses it.** With Slide Mode on, no squeak generator is
  triggered for any shift.
- **Zero is free.** `squeak_amount` 0 allocates no generator over 10 000
  shifts.
- **No allocation on the audio thread.**
- **Audible at the output.** `StringSqueakRealismTests`: a five-fret chord
  change (lifted and legato) on the Dreadnought with every shift squeaking
  measures 1-6 kHz squeak energy within 18-36 dB of the note at amount 0.25,
  12 dB more at amount 1, nothing at 0; the Classical is quieter than the
  Dreadnought, the electric quieter still, and probability 0 / 1 produce
  no / every squeak.
- **No two alike.** Two identical shifts under one seed differ in pitch and
  length; the same seed repeats them.
- **Material and speed.** Flatwound sits 15 dB under phosphor bronze at the
  output; a shift twice as fast peaks higher and tails off sooner.
