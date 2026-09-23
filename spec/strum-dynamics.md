# STRUM DYNAMICS SPEC

A strum is not six notes at once. It is one object crossing six strings
over 15-60 milliseconds, accelerating as it goes, hitting each string at a
slightly different angle with slightly different force, and sometimes not
hitting one at all.

Luthier's rhythm engine already schedules per-string events with an offset.
This file specifies what that offset actually is, and everything that comes
with a real strum: the acceleration curve, the strike-force profile across
the strings, the striker's own noise, and chucks.

It is what separates "an arpeggiated chord" from "a strum".

## 0. Ground rules

1. **A strum is one gesture, not N events.** Crossing velocity, angle and
   force belong to the gesture; the per-string events are derived.
2. **The hand accelerates.** A constant inter-string delay is the single
   most recognisable synthetic-strum tell.
3. **Down and up are different.** Not mirror images: the hand, the pick
   angle and which strings get emphasis all differ.
4. **Honest magnitudes.** A medium strum crosses six strings in about
   30 ms. Slowing that to 120 ms should sound like a deliberate slow
   strum, not like a default.
5. **A chuck is a strum with the strings muted**, not a separate sample.

## 1. Crossing velocity

The gesture's speed, in **strings per second** (sps).

```
inter-string delay (s) = 1 / crossing_velocity_sps
```

| Feel | sps | Six-string crossing |
|---|---|---|
| Very slow (rake) | 20 | 250 ms |
| Slow | 60 | 83 ms |
| Medium | 200 | 30 ms |
| Fast | 400 | 15 ms |
| Very fast (flick) | 800 | 7.5 ms |

### 1.1 Where it comes from

Four sources, in priority order (`input-routing.md`):

1. **MPE / hex-pickup routing**: per-string events arrive in real timing.
   Strum synthesis does not apply (`ambiguity-resolutions.md` 6).
2. **Live chord played on a keyboard** (mode b): measured from the spread
   of the incoming note-on burst. A player who rolls a chord gets their
   roll.
3. **Rhythm engine**: the active pattern's `crossing_sps` field
   (`rhythm-engine.md` 6); genre kits carry a default and a step may
   override. This is `ambiguity-resolutions.md` 6's chosen resolution.
4. **Plugin-global default**: `strum_crossing_sps`, **200 sps for guitar,
   100 sps for bass**. Section 4 fixes these.

## 2. The acceleration profile

A real strumming hand is not at constant speed. It starts slower, speeds
up through the middle strings and eases at the end.

`strum_acceleration` (0-1, default 0.35) blends between constant velocity
and a strong ease. The crossing time for string *i* of *n*:

```
t(i) = T × ease(i / (n-1), strum_acceleration)
ease(u, a) = (1-a)·u + a·(u² · (3 - 2u))     // smoothstep blend
```

At `a = 0` the strings are evenly spaced. At `a = 1` the first and last
gaps are roughly twice the middle ones, which is what a hand does.

### 2.1 Direction

- **Down** starts at the lowest-pitched string. The hand is descending
  with gravity and the pick meets the strings at a steeper angle: slightly
  harder, slightly brighter.
- **Up** starts at the highest-pitched string, is typically faster (the
  return stroke), and hits at a shallower angle: softer, with more chirp
  (`pick-noise.md` 4) and less click.

`strum_up_velocity_ratio` (default 1.25) makes the up-stroke faster than
the down-stroke by default, because it is.

## 3. Force across the strings

The pick does not strike every string equally.

```
force(i) = base
         × tilt(i, strum_tilt)
         × evenness(i, strum_evenness)
         × accent(i, direction)
```

- **`strum_tilt`** (-1 to +1, default +0.15 down / -0.15 up): a linear
  ramp of force across the strings. Positive favours the strings struck
  later. A down-strum that gets louder as it reaches the treble strings is
  the normal case.
- **`strum_evenness`** (0-1, default 0.75; exists today): random per-string
  force variation. At 1.0 every string gets the same force; at 0 the
  variation is ±40%. Deterministic per instance seed and strum index
  (`character-wear.md` 1).
- **Accent**: on a down-strum the lowest two strings get +1.5 dB; on an
  up-strum the top two do. This is where the hand actually puts weight.

### 3.1 Misses

`strum_miss_probability` (0-1, default 0.04) is the chance a given string
is not struck at all. Real strums miss the low E constantly, and modelling
it is a large part of why a pattern stops sounding like a loop.

Misses are weighted towards the **first string in the direction of
travel** - the one the hand is still arriving at - at three times the base
rate.

The existing `RhythmHumanise::missPercent` is retained and is the rhythm
engine's per-pattern control; this is the physical baseline underneath it.

## 4. Defaults

Fixed here because `ambiguity-resolutions.md` 6 points at this section.

| Parameter | Guitar | Bass |
|---|---|---|
| `strum_crossing_sps` | **200** | **100** |
| `strum_acceleration` | 0.35 | 0.25 |
| `strum_up_velocity_ratio` | 1.25 | 1.15 |
| `strum_tilt` | ±0.15 | ±0.10 |
| `strum_evenness` | 0.75 | 0.85 |
| `strum_miss_probability` | 0.04 | 0.01 |

Bass crosses more slowly because the strings are further apart and the
hand travels further, and misses far less because there are four strings
and they matter more.

## 5. Strikers

The object doing the crossing. Not always a pick.

| Striker | Crossing | Noise | Note |
|---|---|---|---|
| Pick | As set | `pick-noise.md` click + chirp | Default |
| Thumb | ×0.6 | Soft, low click | Fingerstyle down-strums |
| Fingernails | ×1.3 | Bright, short | Up-strums in fingerstyle |
| Flesh (fingers) | ×0.7 | Almost none | Warm, muted |
| Thumbpick | ×0.9 | Between pick and thumb | |
| Brush / hand | ×0.4 | Broadband | Percussive |

`strum_striker` selects. The striker also selects which `pick-noise.md`
generator fires, so a thumb strum has no pick click.

Separate `strum_striker_down` and `strum_striker_up` exist because
fingerstyle players use the thumb down and nails up, which is a real and
audible combination.

## 6. Chucks, mutes and the UI

### 6.1 Chuck

A percussive strum with the fretting hand muting the strings.

- Every string is damped hard (`chuck_damping`, default 0.92) before the
  strum crosses.
- The result is pitched noise with the strum's own timing and the body's
  resonance - which is why a chuck on a dreadnought and a chuck on a
  Telecaster sound completely different.
- `chuck_amount` (0-1) blends towards a full mute; at 0 it is a normal
  strum.
- Triggered by the rhythm pattern's chuck step type
  (`rhythm-engine.md` 6) or by a MIDI note in the chuck key range.

### 6.2 Palm mute interaction

Palm muting already exists as a technique. A palm-muted strum keeps its
pitch and loses its sustain; a chuck loses its pitch. They are different
and both are available.

### 6.3 The STRUM group

`gui-integration.md` 4.4 puts the **STRUM** group on the RHYTHM tab:
crossing velocity, acceleration, tilt, evenness, miss probability, the two
striker dropdowns, chuck amount and damping.

`gui-integration.md` 3.5's Easy-mode **Feel knob** scales crossing velocity
and evenness together: turning Feel up makes strums faster and tighter,
down makes them slower and looser. One knob, because Easy mode should not
ask a user what "sps" means.

Feel at 0.5 is the defaults in section 4. Feel maps crossing velocity
60→400 sps and evenness 0.45→0.95 across its range.

## 7. Parameters

Non-physical, so no `PhysicalRange` family: these describe a gesture, not
an object.

| ID | Range | Default |
|---|---|---|
| `strum_crossing_sps` | 20 – 800 | 200 |
| `strum_acceleration` | 0 – 1 | 0.35 |
| `strum_up_velocity_ratio` | 0.5 – 2.0 | 1.25 |
| `strum_tilt` | -1 – +1 | 0.15 |
| `strum_evenness` | 0 – 1 | 0.75 (exists) |
| `strum_miss_probability` | 0 – 1 | 0.04 |
| `strum_striker_down` | choice | Pick |
| `strum_striker_up` | choice | Pick |
| `chuck_amount` | 0 – 1 | 0.0 |
| `chuck_damping` | 0 – 1 | 0.92 |

Net new parameters: **+9** (`strum_evenness` exists).

## 8. Tests

- **Crossing timing is exact.** At 200 sps with acceleration 0, six
  per-string events are spaced 5 ms ± 1 sample.
- **Acceleration changes spacing, not duration.** At acceleration 1.0 the
  total crossing time is within 2% of the acceleration-0 case, while the
  first gap is at least 1.6× the middle gap.
- **Up is faster than down** by `strum_up_velocity_ratio` within 1%.
- **Rhythm engine supplies velocity.** Per
  `ambiguity-resolutions.md` 6.1: a chord voiced from a held root produces
  events spaced per the active pattern's `crossing_sps` within 1 sample.
- **Live spread wins.** A keyboard chord with 40 ms of note spread produces
  a crossing of 40 ms, not the pattern's.
- **MPE passes through.** Per-string events on separate channels arrive in
  their original timing with no strum synthesis.
- **Misses are weighted and deterministic.** Over 10 000 strums the miss
  rate matches `strum_miss_probability` within 5% relative, the leading
  string misses about 3× as often, and a fixed seed reproduces the exact
  pattern.
- **Evenness bounds the variation.** At evenness 1.0 all per-string forces
  are equal; at 0 the spread is within ±40%.
- **Striker selects the noise.** A thumb strum triggers no pick-click
  generator; a pick strum does.
- **Chuck kills pitch.** A chuck at `chuck_amount` 1.0 produces no
  detectable f0 above the noise floor, while retaining the body's
  resonance.
- **Feel knob maps as specified.** Feel 0, 0.5 and 1.0 produce the three
  documented crossing velocities within 1%.
- **Bass defaults apply** when the loaded guitar's family is bass.
