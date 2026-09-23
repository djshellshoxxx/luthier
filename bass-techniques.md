# BASS TECHNIQUES SPEC

Luthier ships five bass guitars and plays them like guitars with long
strings. A bass is not that. The right hand does completely different
things - slap, pop, double thump, ghost notes, fingerstyle alternation,
palm-muted plucking with a pick - and each of them is a distinct physical
event, not a velocity layer.

This file adds those techniques and the bass-family defaults that should
have been different everywhere else.

It is last in the realism phase because it depends on the noise engine
(`pick-noise.md`), the buzz model (`fret-buzz.md`), the parts model
(`guitar-workshop.md`) and strum dynamics.

## 0. Ground rules

1. **Bass techniques only exist on a bass.** When the loaded guitar's
   family is not `bass`, the SLAP group is hidden and the techniques are
   inert (`gui-integration.md` 0.7). The empty-state message is fixed:
   "Bass techniques are inactive. Load a bass to use them."
2. **Slap is a collision, not a pluck.** The thumb strikes the string
   against the fretboard. That is a different excitation and a different
   spectrum, and modelling it as a bright pluck is why most bass
   emulations fail.
3. **Ghost notes are the groove.** A slap line is mostly ghosts. They must
   be cheap, plentiful and convincing.
4. **Bass defaults differ everywhere.** Not just here: string count,
   scale length, action, squeak, strum crossing, compression.
5. **Honest magnitudes.** A slap is loud, and the model's headroom has to
   survive it without a limiter doing the work.

## 1. Family detection

`GuitarSpec.meta.family == "bass"` enables everything here. Detection is
on the spec, not on string count, because a baritone guitar has six
strings and is not a bass, and a six-string bass exists.

String count comes from the spec (4, 5 or 6 typical). Nothing here
assumes four.

## 2. Slap

The thumb strikes the string near the end of the fretboard, driving it
against the frets.

### 2.1 The collision model

1. The thumb imparts an initial displacement at the strike point, larger
   than a pluck's and with a much faster rise (0.3 ms versus 2 ms).
2. The string is driven **into the frets**, which is a hard nonlinear
   contact, not the soft release of a pluck.
3. That contact is handed to `fret-buzz.md`'s generator with a large
   excess, producing the characteristic metallic click - so slap's clack
   comes from the same code that models a bad setup, because physically it
   is the same event.
4. The string rebounds and rings with a strongly inharmonic attack that
   settles into the fundamental over 30-80 ms.

### 2.2 Parameters

| ID | Range | Default | Effect |
|---|---|---|---|
| `slap_strength` | 0 – 1 | 0.7 | Initial displacement |
| `slap_position_mm` | 20 – 200 | 60 | Distance from the last fret |
| `slap_thumb_hardness` | 0 – 1 | 0.55 | Contact stiffness; harder is clackier |
| `slap_fret_contact` | 0 – 1 | 0.8 | How much the string is driven into the frets |

`slap_fret_contact` at 0 gives a thumb-thump with no clack, which is a
real technique (Motown-style thumb playing) and worth having.

## 3. Pop

The finger hooks under the string and pulls it up, releasing it to snap
back onto the fretboard.

- **Release is the event.** The string is displaced *away* from the board
  and released; the snap-back collision is the sound.
- Spectrum is brighter and shorter than slap, with a strong initial
  transient around 2-4 kHz.
- `pop_strength` (0-1, default 0.75), `pop_position_mm` (default 40 -
  pops happen closer to the neck end than slaps).
- Pops are usually on the top two strings; nothing enforces that.

## 4. Double thump

The thumb strikes downward and then upward on the return, giving two
events per hand movement.

- The up-stroke is quieter (×0.65 by default) and brighter, because the
  thumb's nail side contacts.
- `double_thump_enabled` (bool, default off) plus
  `double_thump_up_ratio` (0-1, default 0.65).
- When on, a slap event schedules its return stroke at
  `1 / (2 × strum_crossing_sps)` later, reusing
  `strum-dynamics.md`'s gesture timing rather than inventing a second
  clock.

## 5. Ghost notes

A muted, pitchless percussive note. The fretting hand rests on the string
without pressing.

- Implemented as a heavily damped excitation: `ghost_damping` (default
  0.94) applied before the strike, which is the same mechanism as
  `strum-dynamics.md` 6.1's chuck and shares its code.
- `ghost_level` (0-1, default 0.45) relative to a full note.
- Triggered by velocity below `ghost_velocity_threshold` (default 32) when
  `ghost_auto` is on, or explicitly by the `BASS_TECH` MIDI event class.
- **Auto-ghosting is on by default for bass.** A bass line played with
  dynamics gets its ghosts for free, which is the single change that makes
  a MIDI bass line stop sounding typed in.

## 6. Fingerstyle

The default bass technique, and currently modelled as a guitar's finger
pluck.

- **Alternation**: index and middle fingers alternate. They are not
  identical - `finger_alternation_variation` (0-1, default 0.25) applies a
  small timing and tone difference between them, which is what gives
  fingerstyle bass its pulse.
- **Rest stroke**: the finger comes to rest on the adjacent string,
  damping it. `rest_stroke` (default on) damps the next-lower string on
  each pluck, which is why real fingerstyle bass is cleaner than the MIDI
  version.
- **Plucking position** follows the existing `pluck_position`, with a bass
  default nearer the bridge.

## 7. Pick bass and palm muting

- Pick on bass uses `pick-noise.md` unchanged, with a heavier default
  (1.14 mm) and more click.
- **Palm muting is the bass pick technique.** The existing palm-mute
  damping gets a bass-specific profile: shorter decay, more fundamental
  retained, because the palm sits on the strings at the bridge and a bass
  string has more mass to carry through it.

## 8. Bass defaults elsewhere

The changes this file makes outside itself, all keyed on family `bass`:

| Spec | Guitar | Bass | Why |
|---|---|---|---|
| `strum-dynamics.md` 4 | 200 sps | **100 sps** | Wider spacing, longer travel |
| `strum-dynamics.md` 4 | miss 0.04 | **0.01** | Four strings, each matters |
| `string-squeak.md` 7 | pressure 0.50 | **0.35** | Longer, rounder shifts |
| `fret-buzz.md` 6.1 | Player-friendly | **Factory low** | Bass is set up lower; buzz is part of the sound |
| `part-acoustics.md` 3 | 628-648 mm | **864 mm** (34") | Long scale default |
| `pick-noise.md` 2 | 0.73 mm | **1.14 mm** | Heavier picks |
| Compressor default | off | **on, 2:1** | Bass is nearly always compressed |

These are defaults, not constraints. Every one is a control the user can
move.

## 9. UI

`gui-integration.md` 4.4 puts the **SLAP** group on the CHARACTER tab and
the **bass step grid** on the RHYTHM tab, both shown only when the loaded
guitar's family is bass (`gui-integration.md` 0.7).

**SLAP group**: slap strength, position, thumb hardness, fret contact; pop
strength and position; double thump toggle and ratio; ghost level,
damping, auto-ghost toggle and threshold.

**Bass step grid** (RHYTHM tab): a step sequencer whose per-step type is a
bass technique rather than a strum direction - thumb, pop, ghost,
fingerstyle, dead. This is how bass patterns are actually written, and it
is a different grid from the strum one rather than a relabelled version.

`factory-content.md`'s genre kits gain bass kits that use it.

## 10. MIDI export

`midi-export.md` 9 fixes it: bass events export as **`BASS_TECH`**.

- Luthier profile: technique, strength, position and fret-contact per
  event; round-trips exactly.
- Generic profile: slap and pop become high-velocity notes, ghosts become
  low-velocity notes, and the technique is lost. That is the honest lossy
  mapping and it still plays back as a recognisable bass line.

## 11. Parameters

| ID | Range | Default |
|---|---|---|
| `slap_strength` | 0 – 1 | 0.70 |
| `slap_position_mm` | 20 – 200 | 60 |
| `slap_thumb_hardness` | 0 – 1 | 0.55 |
| `slap_fret_contact` | 0 – 1 | 0.80 |
| `pop_strength` | 0 – 1 | 0.75 |
| `pop_position_mm` | 10 – 150 | 40 |
| `double_thump_enabled` | bool | false |
| `double_thump_up_ratio` | 0 – 1 | 0.65 |
| `ghost_level` | 0 – 1 | 0.45 |
| `ghost_damping` | 0 – 1 | 0.94 |
| `ghost_auto` | bool | true |
| `ghost_velocity_threshold` | 1 – 127 | 32 |
| `finger_alternation_variation` | 0 – 1 | 0.25 |
| `rest_stroke` | bool | true |

Net new parameters: **+14**.

These are gesture parameters rather than object properties, so they are
not in a `PhysicalRange` family - with the exception of
`slap_position_mm` and `pop_position_mm`, which are positions on a real
instrument and join the `buzz` family, since that is where the fret
geometry they interact with lives.

## 12. Tests

- **Inert on a guitar.** With a non-bass guitar loaded, assert no bass
  technique produces any audible difference and the SLAP group is hidden.
- **Slap uses the buzz generator.** Assert a slap at `slap_fret_contact`
  0.8 triggers `NoiseEngine::FretBuzz`, and at 0.0 does not.
- **Slap is not a loud pluck.** Compare a slap and a velocity-127 pluck at
  equal peak level; assert the slap's spectral centroid in the first 20 ms
  is at least 1.5× higher and its attack rise time is at least 3× faster.
- **Pop is brighter and shorter than slap** by at least 20% on both
  measures.
- **Double thump produces two events** at the specified spacing within
  1 sample, with the second at the ratio's level within 0.5 dB.
- **Ghosts are pitchless.** A ghost at default damping produces no
  detectable f0 above the noise floor.
- **Auto-ghosting fires below threshold** and not above it.
- **Rest stroke damps the neighbour.** With `rest_stroke` on, plucking
  string 2 reduces string 3's ringing amplitude by at least 12 dB within
  10 ms.
- **Finger alternation varies.** Over 100 consecutive fingerstyle notes,
  odd and even notes differ measurably in timing and centroid; with
  variation 0 they do not.
- **Bass defaults apply on load.** Loading each factory bass produces the
  section 8 values.
- **Headroom.** A full-strength slap on every string simultaneously peaks
  below 0 dBFS with the limiter bypassed.
- **BASS_TECH round-trips.** Luthier-profile export and re-import
  reproduces every technique event exactly; Generic profile produces notes
  whose velocities preserve the slap/ghost distinction.
