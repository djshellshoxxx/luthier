# STRING INTERACTION SPEC

Six strings on one guitar are not six instruments. They share a bridge,
the air in front of the top, one pickup's magnetic field and two hands,
and every one of those links makes a string behave differently because
its neighbours exist. A chugged power chord is tight because the palm
lies across four strings, not one. A rock player's open B stays quiet
while the G is fretted because the finger's underside rests on it. A
chord never stops on one sample. A strum across an `x` string still
hits it.

The bridge link already exists (`CouplingMatrix`, `engine.md` 5.6), and
`body-coupling.md` owns the bridge-to-body-to-string path. This file
adds the six other interactions:

1. **Air-path sympathetic ring**: strings driven by sound pressure, not
   by the bridge.
2. **Palm mute spread**: the palm damps every string it covers.
3. **Adjacent finger damping**: fretting fingers touch neighbours.
4. **Chord release stagger**: fingers leave a chord one by one.
5. **Pickup crosstalk**: a pole piece senses more than its own string.
6. **Muted-string thump**: a strum across a muted string sounds it.

`muting-rhythm.md` 3 points here for the physics of palm spread and the
fretting-hand mute style; this file is that physics.

## 0. Ground rules

1. **Interactions are physical deltas.** Each one changes a string's
   damping, excitation, timing or sensed level; none is an output EQ.
2. **Honest magnitudes.** The air path is 15-35 dB under the bridge
   path on an acoustic and inaudible on a solid body. Crosstalk moves a
   string's level by about a decibel. Stagger is milliseconds. These
   effects are felt, and they are sized so.
3. **Deterministic.** Stagger and thump use seeded randomness, so a
   render repeats bit-for-bit and Luthier-profile MIDI round-trips.
4. **Off is free and bit-identical.** Every amount at 0 skips its code
   path entirely.
5. **Per-string outputs keep their contract.** `routing-io.md` 10 says
   the per-string outputs sum to the main pre-body signal within
   -80 dBFS. Nothing here is applied after the per-string tap.

## 1. Air-path sympathetic ring

**Physics.** An acoustic top radiates; the pressure field in front of
it drives every string over its whole length. Unlike the bridge path
it does not fall off with saddle distance (the field is roughly uniform
across a 55 mm string band), it arrives about 0.3 ms later (10 cm of
air), and radiation efficiency rises with frequency below the top's
coincidence, roughly +6 dB/octave. On a solid body the only strong air
path is from the amplifier, which is `ambiguity-resolutions.md` 1's
feedback loop, not this.

**Model.** A rank-1 term in `CouplingMatrix::process`:

```
airSum[t]   = sum_j bridgeOut_j[t]                        (one sum, O(N))
air_i[t]    = a_air * HP_120Hz( airSum[t - tau] - bridgeOut_i[t - tau] )
shaped_i    = receiveFilter_i( bridgeSum_i + air_i )      (existing filter)
tau         = 0.29 ms  (computed from 0.1 m / 343 m/s at prepare)
a_air       = coupling_air_amount * a_cat
a_cat       = 0.0030 acoustic / classical, 0.0010 archtop / semi-hollow,
              0.0001 solid body, 0.0004 bass
```

Against the default bridge coefficients (`buildDefault(0.020)`: 0.020
adjacent, 0.005 floor at distance 5), the acoustic air term is
**0.15x** the bridge term for adjacent strings and **0.6x** for the
outermost pair: the air path is what lets the high E sing along with a
low-E note on an acoustic, and it is negligible everywhere else. It
passes through the existing receive filter and the existing
`kEnergyCap`, so it adds no new runaway path. The 14-sample delay line
(`tau` at 48 kHz, sized for 192 kHz at prepare) is the only new state.

## 2. Palm mute spread

**Physics.** The palm edge lies across the strings near the saddle with
a contact band 20-60 mm wide. Every string under it is damped, struck or
not, which is why a palm-muted chug has no open-string ring behind it.

**Model.** `palm_mute_spread` (mm) over the bridge string spacing
`s_b` (10.5 mm electric, 11.0 acoustic, 11.5 classical, 19.0 bass, from
the guitar family until `guitar-workshop.md` parts carry it) gives a
width `W = spread / s_b` strings. Palm centre `c` is the mean string
index of the most recent palm-muted strike group (strikes within
30 ms). Weight for a string at `d = |s - c|`:

```
w(d) = 1                                  d <= W/2
     = cos^2( (pi/2) (d - W/2) )          W/2 < d < W/2 + 1
     = 0                                  otherwise
```

At block rate in `LuthierEngine::updatePerBlockModulation`, while
`TechniqueEngine::getPalmMuteAmount() = P > 0.05`, every string not
struck this block whose damping is `Open` or `Released` is set to
`Damping::PalmMute` with amount `P w(d)` and flagged `spreadMuted`.
When `P` falls below 0.05, flagged strings return to the state they had
(`Open` rings on with whatever energy survives). `Choked`, `Silenced`
and `Chuck` are never overridden. Default 35 mm: `W` = 3.3 on an
electric, so a chord on strings 3-5 fully damps 3, 4 and 5, damps
string 2 at 0.75 and leaves 0 and 1 alone.

When `muting-rhythm.md` supplies a per-step palm position and pressure,
they set `P` and `c`; this section only defines coverage.

## 3. Adjacent finger damping

**Physics.** A fretting finger that is not arched touches the next
thinner string with its underside and the next thicker one with its
tip. Rock and blues players rely on it; classical technique arches the
fingers precisely to avoid it.

**Model.** In `LuthierEngine::triggerNote`, for a fretted note
(`fret > 0`, not a harmonic, tap or slide-guitar note) on string `s`,
each neighbour `s - 1` (underside, weight 1.0) and `s + 1` (tip, weight
0.5) that has no held note (`stringMidiNote == -1`) gets
`Damping::Chuck` with amount `a = adjacent_mute_amount x weight x
style`. Chuck is the right model: strum-dynamics 6.1 defines it as the
fretting hand lying on the string, and its T60 is geometric between the
note's own and 10 ms, `T60 = T60_note^(1-a) x 0.01^a`. At the default
(`a` = 0.6) an open B's 3.2 s becomes about 0.1 s. `style` is 1.0
for rock spread and 0.1 for classical fingertip (`muting-rhythm.md` 3's fretting-hand mute style,
`MuteSettings::frettingStyle`). The neighbour records the source string
in a bitmask. A note later struck on the neighbour clears it (its own
`triggerNote` sets its damping); the source's note-off lifts every mute
it caused. A chord's notes therefore never mute each other: a
neighbour that is itself fretted in the chord overrides the mute when
its note-on arrives, within the strum.

## 4. Chord release stagger

**Physics.** Lifting a chord is several fingers leaving at slightly
different times, 5-30 ms apart; the resulting overlapping stops are
part of why a released chord sounds played rather than gated.

**Model.** In `LuthierEngine::scheduleEvents`, note-offs for two or more
strings whose timestamps fall within 3 ms form a release group (letRing
and sustained strings are excluded). Each gets a delay:

```
u_s     = (1 - |bias|) * rng_s + |bias| * rank_s        in [0, 1]
delay_s = release_stagger_ms * u_s
```

`rank_s` is 0..1 by string order, treble first when `bias > 0`, bass
first when `bias < 0`. `rng_s` is drawn from an `RtRandom` seeded from
the character seed (`character-wear.md` 0.1) XOR the group's absolute
sample, so it is reproducible. Delays are only ever added: a note-off is
never moved earlier. Delayed offs go on the existing schedule
(`kMaxScheduledEvents` 192), and on overflow fire immediately, like
every other scheduled event.

## 5. Pickup crosstalk

**Physics.** A pole piece senses a string through a field with a
lateral aperture of a few millimetres. Neighbouring strings sit
10.5 mm away and are sensed about 30 dB down. In the mono coil sum that
is only a per-string level, and a small one. It becomes audible when a
string moves sideways: a bend drags the string off its own pole and
toward the next, and a neck pickup loses about a decibel on a whole-step
bend. That is the effect modelled.

**Model.** Gaussian aperture per pole: `A(y) = exp(-y^2 / (2 sigma^2))`,
`sigma` = 4.0 mm single coil, 5.0 humbucker, 5.5 P-90, times
`pickup_aperture_scale`; piezo and internal mic are skipped. A string
displaced laterally by `delta` at the pickup is sensed with

```
g_s = [ A(delta) + A(s_b - delta) + A(s_b + delta) ] / [ A(0) + 2 A(s_b) ]
```

(edge strings drop their missing neighbour term). Lateral displacement
from the bend: `delta_fret = 7.0 mm * sqrt(|bendCents| / 200)` (a
whole-step bend at mid-neck moves the string 6-8 mm), scaled to the
pickup linearly from the saddle: `delta = delta_fret x x_pu / x_fret`.
Strings 0-3 push toward the bass side, 4 and above pull toward the
treble side. `g_s` is computed at block rate in
`updatePerBlockModulation` and applied inside
`PickupEngine::processStrings` as `coilSum += apertureGain[slot][s] *
combSample (...)`, via a new
`PickupEngine::setStringLateralOffsets (const double* mm, int n)`.
Unbent, `g_s = 1` exactly; the per-string outputs are untouched
(ground rule 5).

Magnitudes at default: whole-step bend, G string, fret 7, neck single
coil (about 160 mm from the saddle on a Strat): `delta` = 2.6 mm,
**-0.9 dB**; bridge pickup (40 mm): -0.07 dB.

## 6. Muted-string thump

**Physics.** A strum crosses every string between its first and last,
including those the fretting hand mutes (`x` in a voicing). The muted
string is struck: a short, pitchless thump from a heavily damped
string, 10-20 dB under the sounding strings.

**Model.** Where a strum is built (`MidiInterpreter`'s strum for live
chords, `RhythmEngine`'s strum for patterns), each string with fret -1
in the voicing that lies inside the strum's span (between the first and
last struck string, or inside the pattern's STRUM string mask) gets a
`NoteOnEvent` at its crossing time with:

- `chuck = 0.92` (`Damping::Chuck`, strum-dynamics 6.1: T60 about 15 ms),
- `velocity = strike x muted_thump_level x 0.3`,
- pitch of the fretting hand's position (`RubricVoicer` hand fret) on
  that string,
- new flag `deadStrike = true`: `triggerNote` skips `stringActivity`,
  `stringMidiNote` and MIDI out, so a thump is never reported as a note.

`midi-export.md` STRUM already carries `mute` and the string mask; the
thumps are regenerated from them on import, not exported as notes.

## 7. Parameters

| ID | Name | Stock | Advanced | Default | Unit | Family |
|---|---|---|---|---|---|---|
| `coupling_air_amount` | Air Coupling | 0 – 1 | – | 1.0 | x physical | none (trim, like `coupling_amount`) |
| `palm_mute_spread` | Palm Width | 20 – 60 | 5 – 120 | 35 | mm | `pick` |
| `adjacent_mute_amount` | Neighbour Mute | 0 – 1 | 0 – 1 | 0.6 | ratio | `squeak` |
| `release_stagger_ms` | Release Stagger | 0 – 40 | – | 12 | ms | none (gesture) |
| `release_stagger_bias` | Stagger Order | -1 – 1 | – | 0 | bass / treble first | none |
| `pickup_aperture_scale` | Pole Aperture | 0.5 – 2.0 | 0.1 – 5.0 | 1.0 | x sigma | `circuit` |
| `muted_thump_level` | Muted-String Thump | 0 – 1 | – | 0.5 | ratio | none (amount) |

Families: `file-formats.md` fixes seven keys. The palm is right-hand
contact (`pick`); fretting-hand contact already lives in `squeak`
(finger moisture and pressure); pole aperture is transducer physics
and joins `circuit`, the family the Workshop's pickup electrics already
report under. Gesture timings and amounts are non-physical
(`advanced-ranges.md` 0.5, the `bass-techniques.md` 11 precedent).
`adjacent_mute_amount` has no wider physical meaning past 1, so it has
the same pair twice. Net new: **+7**.

## 8. Triggering

No new MIDI. Palm spread follows the existing palm-mute CC (CC 67) and
muting-rhythm grid; adjacent damping follows note-ons; stagger follows
note-offs; thump follows strums; crosstalk follows bends (pitch bend,
MPE, `microtonal-bends.md`).

## 9. UI and state

- `gui-integration.md` 4.4 CHARACTER tab, new **STRING INTERACTION**
  group: air coupling, palm width, neighbour mute (with the fretting
  style mirrored from MUTE), release stagger and order, pole aperture,
  muted-string thump.
- `gui-techniques-updates.md` 4's mute-zone shading draws the palm
  width and weights (band opacity = `w`), so a user sees which strings
  the palm covers.
- Section 19 rows for each control, primary location CHARACTER ->
  STRING INTERACTION; palm width also mirrored in TECHNIQUES -> MUTE.
- Preset: plain APVTS parameters. Runtime flags (`spreadMuted`,
  neighbour bitmasks) are cleared by `reset()`, preset load and panic.

## 10. Performance and realtime safety

- Air: one sum, N subtractions and one one-pole per sample; palm,
  neighbour and crosstalk: block rate; stagger and thump: per event.
  Total budget **0.1 units** at 12 strings; everything is skipped at 0.
- No allocation: the air delay is sized in `prepare`, flags are fixed
  arrays, staggered offs use the existing schedule.
- The air term goes through the existing receive filter, DC blocker and
  `kEnergyCap`; the NaN guard is unchanged.

## 11. Tests

- **SI-01 Air magnitudes.** `CouplingMatrix` alone, noise on string 5:
  output energy on string 0 from the air term is 0.4-0.8x that from the
  bridge term on an acoustic, at most 0.05x on a solid body; for the
  adjacent pair 4-5 it is at most 0.25x.
- **SI-02 Air off is bit-identical.** `coupling_air_amount` 0 produces
  output bit-identical to the matrix without the air term.
- **SI-03 Air delay.** Impulse on string 5: the air contribution on
  string 0 starts 0.29 ms (+/- 1 sample) after the bridge contribution.
- **SI-04 Palm spread coverage.** Open strings ringing; palm-muted
  strike on strings 3-5 at CC 67 = 127, spread 35 mm: string 2's T60
  falls at least 2.5x; strings 0 and 1 keep their T60 within 10 %. At
  20 mm string 2 is unaffected (within 10 %).
- **SI-05 Palm lift restores.** Setting CC 67 to 0 returns every
  spread-muted string to its prior damping within one block; its loop
  gain equals the pre-mute value within 1e-9.
- **SI-06 Neighbour mute, rock.** Open B ringing, fret G at 5 with
  rock spread and amount 0.6: B drops at least 10 dB within 150 ms;
  high E changes by less than 0.5 dB.
- **SI-07 Neighbour mute, classical and chords.** Same with classical
  style: B drops at most 2 dB. A chord including the B, strummed:
  the B's level is within 0.5 dB of the level with amount 0.
- **SI-08 Stagger spread.** Six-string chord, one note-off sample,
  12 ms, default character seed: the damping onsets span 4-12 ms, none precedes the note-off;
  two renders are bit-identical; at 0 ms every onset is on the
  note-off sample.
- **SI-09 Stagger order.** Bias +1: release order strictly treble first;
  -1: strictly bass first.
- **SI-10 Crosstalk honesty.** Unbent, every `g_s` is exactly 1.
  Whole-step bend, G fret 7: neck single coil drops 0.5-2 dB, bridge
  single coil less than 0.4 dB, neck humbucker less than the neck
  single coil.
- **SI-11 Thump.** Strum `x32010` downward, thump level 0.5: string 5
  produces an event whose peak is 8-20 dB under the loudest sounding
  string, whose normalised autocorrelation has no peak above 0.3 at any
  lag in 2-25 ms, and which is 40 dB down within 60 ms; it appears in
  neither `stringActivity` nor MIDI out. Level 0: bit-identical to
  without.
- **SI-12 Per-string contract.** With every feature at default, the
  `routing-io.md` 10 per-string sum test passes at -80 dBFS.
- **SI-13 Realtime.** No allocation across SI-01 to SI-11; measured
  cost at 12 strings within 0.1 units; zero amounts cost nothing
  measurable.
- **SI-14 Ranges and presets.** Physical rows valid in `RangeRegistry`;
  declaration mismatches empty; a preset with every field changed
  round-trips.
