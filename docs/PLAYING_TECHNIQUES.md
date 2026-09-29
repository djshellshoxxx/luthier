# Playing techniques

How each technique is detected, and what it actually changes in the model.

The short version: nothing here is a sample or a special case. Every technique is a
change to the delay length, the loop filter, the loss gain or the excitation shape.
That is why techniques compose - a bend into a slide into a vibrato into a harmonic
works without anyone having written that combination.

---

## How a technique is chosen

Detection runs in a strict priority order. An explicitly triggered technique always
beats an inferred one, so a held controller is never overruled by what you happened
to play.

```
1.  Palm mute            CC 67 above threshold
2.  Pinch harmonic       CC 72 held
3.  Natural harmonic     CC 73 held
4.  Tap                  CC 74 held (when MPE is off)
5.  Slide guitar         CC 75, or the Advanced toggle
6.  Muted picking        CC 71 above threshold
7.  Slide                same string still ringing, and either CC 65 held
                         or less than the legato window since the last note
8.  Hammer-on            same string ringing, higher pitch, velocity below the
                         legato threshold
9.  Pull-off             same string ringing, lower pitch, velocity below the
                         legato threshold
10. Pluck                everything else
```

Two extra rules:

- On a **fretless** instrument, a hammer-on or pull-off becomes a slide. There are
  no frets to land on, so the pitch has to travel.
- Two notes at the **same pitch** with a light touch are a re-articulation, not a
  hammer-on that goes nowhere.

---

## The techniques

### Pluck

The default. A fresh excitation is generated and injected into the delay line.

If the string is already ringing, it is muted first over 5 ms - which is what the
pick does when it lands on a vibrating string. Skipping that crossfade is what
causes clicks on repeated notes.

**Changes:** new excitation, voice steal if needed.

---

### Hammer-on and pull-off

The string does **not** get re-picked. Its vibration carries through; only the
delay length changes, plus a small re-excitation for the finger landing or snapping
off.

| | Excitation length | Amplitude |
|---|---|---|
| Pluck | full | 100% |
| Hammer-on | 55% | 28% |
| Pull-off | 45% | 24% |
| Tap | 50% | 42% |

A pull-off is slightly shorter and brighter than a hammer-on: a finger snapping off
sideways is a sharper contact than one landing flat.

**Changes:** delay length, small re-excitation, no voice steal.

There is a test that asserts a hammer-on disturbs a ringing string less than a
re-pluck does.

---

### Slide

The delay length glides from the old pitch to the new one. Nothing is re-picked.

The duration comes from the distance, because a hand moves at a roughly constant
speed:

```
duration = clamp(0.018 + 0.025 * |semitones|, 0.020, 0.400) seconds
```

A two-fret slide takes about 70 ms; a twelve-fret slide about 320 ms. Both are
floored and capped so a short slide is not instant and a long one does not outlast
the note.

While the delay is moving, filtered noise is injected in proportion to the slide
speed. Wound strings squeak; plain strings barely do - the noise is scaled by the
material's `squeakFactor` and by roughly 0.22 for a plain string.

**Changes:** delay length ramps, slide noise, tiny re-excitation.

---

### Bend

Pitch-bend messages move the target frequency, smoothed over about 2 ms. Physically
this is the string's tension rising, which is exactly what the delay length
represents.

Range is configurable from 1 to 48 semitones. MPE controllers expect 48.

In **Guitar Controller** mode each channel bends its own string, so per-string bends
work the way they do on a hex pickup.

**Changes:** delay length, continuously.

There is a test that bends a note up a whole tone and asserts both that the pitch
arrives and that no sample-to-sample jump exceeds the click threshold.

---

### Vibrato

Cyclic modulation of the delay length. Depth comes from the mod wheel or
aftertouch; rate and maximum depth are set in Advanced.

The default shape is **finger vibrato**, which is asymmetric: a real finger pulls
the string sharp faster than it lets it return. Sine, triangle, square, saw and
random are also available.

The depth ramps in rather than appearing instantly, because a player's hand takes a
moment to start shaking the string. Each string's LFO has a different phase offset,
because no two fingers wobble in lockstep.

**Changes:** delay length, cyclically.

---

### Palm mute

The heel of the picking hand on the bridge does two things: it damps the high
frequencies hard, and it shortens the decay.

| | Loop cutoff | T60 scale |
|---|---|---|
| Open | ~5.6 kHz | 1.00 |
| Muted picking | 2.0 kHz | 0.35 |
| Palm mute | 800 Hz | 0.11 |
| Released | 1.2 kHz | 0.13 |
| Choked | 500 Hz | 0.035 |

The amount is continuous, so CC 67 sweeps from open to fully muted rather than
switching.

A muted string still **receives** sympathetic energy - it just dissipates it
quickly, which is what a damped string does.

**Changes:** loop filter cutoff, loss gain.

---

### Natural harmonic

Touching the string at a node kills every partial that does not have a node there.

| Fret | Partial | Sounds |
|---|---|---|
| 12 | 2 | one octave up |
| 7, 19 | 3 | an octave and a fifth |
| 5, 24 | 4 | two octaves |
| 4, 9, 16 | 5 | two octaves and a third |
| 3.2 | 6 | |
| 2.7 | 7 | |

The excitation is band-passed twice around the target partial, so neighbouring
partials stay silent, then gain-compensated so the harmonic is actually audible.
Because the waveguide is linear, it sustains only what was excited - no extra
filtering is needed in the loop.

Harmonics decay noticeably faster than stopped notes (T60 x 0.55) and accept less
sympathetic energy, both of which are true of the real thing.

If you trigger a harmonic somewhere that is **not** a node, the engine plays an
artificial harmonic an octave up rather than producing a dead note.

**Changes:** band-limited excitation, shorter T60, reduced coupling.

---

### Pinch harmonic

The thumb grazes the string just after the pick, exciting one upper partial. The
partial chosen rises with velocity:

```
partial = 2 + floor(velocity * 3)
```

so a light pinch gives the 2nd and a hard one the 5th. The excitation is shorter
and steeper than a natural harmonic's, and it is band-passed less tightly - a pinch
is a dirtier thing than a node touch.

**Changes:** short band-limited excitation on an upper partial.

---

### Tap

A hammer-on with more force. Same mechanism, higher amplitude.

For two-handed playing, use Mono mode with a long legato window so successive taps
on one string stay legato instead of re-picking.

**Changes:** delay length, medium re-excitation.

---

### Slide guitar (bottleneck)

A mode rather than an event. While it is on:

- fret quantisation is disabled, so every pitch is continuous, whatever instrument
  is loaded;
- the excitation uses the Slide contact spectrum - glass or metal on wound strings;
- every note glides from the last one rather than starting fresh;
- slide noise is present throughout.

**Changes:** continuous pitch, slide excitation, permanent glide.

---

### Whammy bar

The bar moves the bridge, changing every string's tension at once. The important
difference between bridge types is *how* that shared movement maps onto individual
strings.

| Bridge | Typical range | Behaviour |
|---|---|---|
| Fixed / Hardtail | none | the bar does nothing |
| Vintage Tremolo | -2 / +1 st | chords detune as you bend |
| Locking Tremolo | -24 / +12 st | same, far wider, plus spring resonance |
| Transposing Tremolo | -12 / +5 st | chords stay in tune |
| Vintage Vibrato | -1 / +0.5 st | gentle |

The ranges are each bridge's usual travel; the **Down Range** and **Up Range**
knobs set the actual range.

A **Transposing Tremolo** applies the same frequency *ratio* to every string, so the
intervals inside a chord are preserved. That is the whole point of the design, and
there is a test that asserts every string receives an identical cent offset.

A **Vintage Tremolo** applies the same bridge *movement*, which is a bigger pitch
change on the slacker strings. Modelling that unevenness is what makes it detune a
chord the way the real thing does - and there is a test for that too.

On a Locking Tremolo, snapping the bar back sets the springs behind the bridge ringing:
a short filtered noise burst around 240 and 430 Hz. It is very audible on records
and is often what people notice missing from cheap plugins.

**Changes:** a global cent offset added to every string's tuning.

---

### Strumming

In Poly mode a chord is not triggered simultaneously. The pick crosses the strings
over time, at the **Strum Crossing** speed in strings per second (20 to 800; 200 is
a medium strum): each string sounds when the pick reaches it, so a faster crossing
packs the chord tighter, and the pick accelerates a little through the stroke
rather than moving at one fixed step per string.

A downstroke crosses the low strings first; an upstroke reverses. Up-strokes are
lighter (about 85% of the force), and the pick loses a little energy as it crosses, so
each successive string is slightly quieter.

The speed varies slightly every time, scaled by the Humanize macro. A
machine-even strum is instantly recognisable.

---

### Freeze

**Freeze** captures a window of what is sounding and loops it underneath whatever
you play next, crossfaded at the seam so the held layer does not pulse. A second
Freeze replaces the first.

---

### E-Bow

Infinite sustain, implemented the way an E-Bow actually works: the string is driven
electromagnetically at its own resonance.

The engine feeds a small amount of each ringing string's own output back into it,
but **only while its level is below a target**. Above that, driving stops. The loop
gain never reaches unity, so this sustains indefinitely without any possibility of
runaway.

### Freeze

Captures a window of the audio (typically 200-1000 ms) and loops it indefinitely.
Switch it on again to capture a new window. Use it to sustain a riff or chord while
you play over the top.

---

### Freeze

Freeze captures a short window of whatever is sounding and loops it underneath
what you play next, so a chord can hang while you play over it. Switch it on again
to capture a new window. The capture length, level, attack and release, and a
low-/high-cut on the frozen sound are in the Advanced FREEZE group.

---

### Amp feedback

With the amp loud and a note sustaining, the speaker drives the string. The engine
models that as a physical loop: the amp's output travels back through the air to
every ringing string, delayed by the distance, weakened by distance and by which
way the guitar faces the speaker, and heard by each string only near its own note
(or an octave of it). It goes in where the string is excited, so the pickups, the
guitar's circuit and the volume knob are all inside the loop - rolling the volume
back kills feedback, as it does on a real guitar. The loop saturates softly rather
than running away.

The controls, in Advanced column 3 under Sustain: **Feedback** (how much of the
amp reaches the strings), **Distance**, **Angle**, **Focus** (how narrowly each
string hears its own note) and **Octave** (which octave of the note the feedback
settles on). A light beside them shows when feedback has taken hold.

---

## Controller map

Defaults, all remappable in the preset.

| CC | Target |
|---|---|
| 1 | Vibrato depth |
| 2 | Whammy bar |
| 4 | Expression |
| 11 | Master level |
| 64 | Sustain - let all strings ring |
| 65 | Slide mode |
| 66 | Sostenuto - hold only what is already down |
| 67 | Palm mute amount |
| 70 | Pick position |
| 71 | Muted picking |
| 72 | Pinch harmonic |
| 73 | Natural harmonic |
| 74 | Tap (with MPE on, CC 74 is per-note timbre instead) |
| 75 | Slide guitar mode |
| 76 | Strum speed |
| 77 | Strum direction |
| 78 | Vibrato rate |
| 79 | Humanize amount |

Aftertouch drives vibrato depth by default; it can be switched to bend (Options >
Controllers, "Aftertouch bends the note").

MIDI Learn on any other control is per-control, through its right-click menu, and
is stored with the plugin state rather than with the preset - so your controller
setup survives changing sounds.

---

## Playing modes

### Mono / lead

Every note goes to one string, chosen to keep the hand near where it already is.
Overlapping notes become hammer-ons, pull-offs or slides. This is the mode for
solos, and the one to use for tapping.

### Poly / chord

Chords are voiced across the strings by a search that only returns fingerings a
hand could make:

- one note per string;
- a fret span within reach, four frets by default;
- pitch order following string order - a voicing that puts a low note on a high
  string is possible but almost never what a guitarist plays, so it is heavily
  penalised;
- gaps penalised, since a muted inner string has to be damped by the fretting hand;
- positions near the last chord preferred, so a progression does not jump around
  the neck.

If a chord is genuinely impossible - six notes a semitone apart, say - the voicer
returns the best playable subset rather than nothing, or nonsense. That is identity
rule 9.

Poly mode carries a small latency (the chord window, 2 ms by default) so that a
chord split across a buffer boundary still voices as one chord. It is reported to
the host.

### Guitar controller

MIDI channel 1 is the high E, channel 2 the B, and so on - the convention hex
pickups use. Per-string bend and pressure work natively.

Turn **MPE** on for expressive controllers (or pick an MPE profile in Options >
Controllers). Each note then gets its own channel, with per-note bend and
pressure; notes on the zone's master channel are ignored, and a member channel
keeps to the string it last played while that string can reach the note. CC 74,
MPE's timbre dimension, is read per note but does not yet change the sound.

---

## Humanisation

Every parameter is independent, so you can have loose timing with perfect tuning,
or the reverse.

| Control | What varies |
|---|---|
| Timing | note onsets, in ms |
| Velocity | how hard each note lands |
| Detune | pitch, refreshed every note |
| Attack | contact time, 0.5 to 2 ms |
| Noise | how often incidental string noise occurs |
| Strum | strum speed, stroke to stroke |

The **Humanize** macro scales all of them at once. At zero the output is
machine-perfect - which is a specific stylistic choice, not a neutral default.

Separately, **Realism Detune** puts each string slightly out of tune and keeps it
there, stored in the preset. A real guitar is never exactly in tune, and this is
what stops repeated chords from sounding like a sampler.
