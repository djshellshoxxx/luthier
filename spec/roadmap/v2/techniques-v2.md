# TECHNIQUES V2 SPEC

Luthier already plays a great deal: plucks, hammers, slides, bends, vibrato,
palm mutes, natural and pinch harmonics, whammy dives, scrapes, rakes,
slaps and two-hand taps. This file is the v2 to-do list for the playing
techniques that a working guitarist uses and Luthier does not yet model, plus
the partial ones that need finishing. Each entry says what exists, with file
evidence, and then specifies only what is missing or partial.

Players hear these techniques in the first two bars of a recording. A missing
volume swell or a sweep that is just legato at speed is the first thing a
guitarist will notice, so the list is ordered by how often the technique
appears in real playing, not by how easy it is to build.

## 0. Ground rules

1. **Evidence before spec.** An item is marked exists, partial or missing from
   a search of `Source/` and `spec/`. Exists items are not re-specified. Partial
   items specify only the gap. A claim of "exists" needs a file and symbol.
2. **Playing, not sampling.** Each technique is a gesture the engine performs
   from the note stream, not a sample that fires on a trigger. Gestures are
   scheduled from the same events as the note they belong to.
3. **Keyswitch first, then gesture.** A technique is triggered by the existing
   keyswitch and CC routes (`TechniqueKeyswitch`, `TriggerSource`) before it
   gets its own UI.
4. **Append-only IDs.** New parameters are appended inside a `V2-TECH` block
   (`docs/HANDOFF.md`). Every automatable parameter has a visible control.
5. **Seeded.** Any randomness uses the character seed (`character-wear.md` 1)
   and a fixed salt per technique, so offline renders repeat.
6. **RT-safe.** Gestures are precomputed per note on the message thread where
   possible; the audio thread reads a fixed-size envelope.

## 1. User stories

- **U1.** Swell the volume on a held chord before the pick, to get the
  pre-echo of a swell (3.6).
- **U2.** Sweep an arpeggio from a chord shape, with the pick strokes
  implied by the sweep direction (3.7).
- **U3.** Play a thumb-over bass line where the thumb takes the lower string
  while the fingers stay on the treble (3.8).
- **U4.** Make the rhythm lay back or push on a groove, without a random feel
  (3.10).
- **U5.** Hit a tapped harmonic and have the engine know it is a harmonic
  (3.1).
- **U6.** Let the bar be pulled before the pick on a whammy-equipped guitar
  (3.2).
- **U7.** Hear a chicken-pick snap that is part of the strum, not a separate
  sound (3.4).
- **U8.** Have a player's mistakes sometimes land late or wrong, optionally,
  for realism (3.11).

## 2. UI

Each technique adds controls only where the player needs them. Every control
has a tooltip and is reachable by keyboard.

| Technique | Control | Where | Tooltip |
|---|---|---|---|
| Tapped harmonic | Tapped harmonic on/off per note | Note inspector | "Play this note as a tapped harmonic at the twelfth fret above" |
| Whammy bar pre-bend | Pre-bend depth, trigger | Whammy card | "How far the bar is pulled before the pick, and what triggers it" |
| Volume swell | Swell on, depth, rise time | Technique card | "Fade the note in from silence before the pick, as a volume swell" |
| Hybrid picking | Snap amount (exists as `hybrid_snap`) | Right-hand card | "How much the fingers snap the treble strings" |
| Chicken pickin' | Snap length, mute amount | Right-hand card | "The short muted snap on the treble strings" |
| Sweep | Sweep on, direction, speed | Technique card | "Sweep the chord shape in one stroke" |
| Thumb-over bass | Thumb string, thumb weight | Right-hand card | "Which string the thumb plays and how hard it hits" |
| Arm vs wrist | Stroke source: arm, wrist, mixed | Strum card | "Arm strokes are broad and slow; wrist strokes are small and quick" |
| Push and pull | Push/pull in milliseconds, per part | Rhythm card | "Ahead of the beat (push) or behind it (pull)" |
| Between-phrase damping | Release mode: off, fret hand, strings | Mute card | "Stop the strings the fret hand is not holding between phrases" |
| Mistakes | Mistake amount (0 off), late, wrong | Humanize card | "Occasionally late or wrong notes, as real playing has" |

Easy Mode shows only the on/off switches for these, with the summary
"Techniques: swell, sweep, pre-bend". (`gui-integration.md` rule 3.)

## 3. Engine and data model

### 3.0 Status of the list

| Technique | Status | Evidence |
|---|---|---|
| Natural harmonic | exists | `PlayingEvents.h` `Technique::NaturalHarmonic`; `harmonic-realism.md` |
| Pinch harmonic | exists | `Technique::PinchHarmonic`; `docs/PLAYING_TECHNIQUES.md` |
| Tapped harmonic | partial | `Riffs/RiffCompiler.cpp:298` (`T::tapHarmonic`), `Export/MidiPerformance.cpp:43`; no `Technique` value and no engine voice |
| Whammy dive, flutter | exists | `DSP/Whammy/WhammyEngine.h` (`FloydRose`, dive and pull ranges); `WhammyEngine.cpp` flutter, 5 ms smoother |
| Whammy bar pre-bend | partial | `DSP/Techniques/BendEngine.h` pre-bend is for strings (`preBendCents`, keyswitch 20); no bar path |
| Volume swell | missing | `VolumePedal` exists as an effect; no swell gesture (grep: no hits in `Model/Playing`, `DSP/Techniques`) |
| Pick scrape | exists | `DSP/Noise/ScrapeEngine`; `TechniqueKeyswitch::scrape`; `string-scraping.md` |
| Rake | exists | `TechniqueKeyswitch::rakeDown/rakeUp`; `pick-noise.md` 5; `GenreKit` "Country Rake" |
| Hybrid picking | partial | `Model/Playing/RightHand.h` `RhStyle::hybrid`; `fingerstyle-attack.md` Hybrid row; no pick-and-finger split |
| Chicken pickin' | partial | `LuthierEngineRealismB.cpp:649` snap; `TuneMelody` `countryChicken`; no muted-snap model |
| Sweep picking | missing | No `sweep` gesture in `Model/Playing` or `DSP` (the word hits are unrelated SPEC-SWEEP tags) |
| Legato runs | exists | `two-hand-tapping.md` "Legato Runs"; `Technique::HammerOn/PullOff` |
| Thumb-over bass | missing | `bass-techniques.md` and `SlapEngine` have thumb slap, not thumb-over |
| Nail vs flesh | exists | `nail_vs_flesh` parameter; `fingerstyle-attack.md` |
| Whole-strum dynamics | partial | `Rhythm/StrumGesture.h`; `strum-dynamics.md` 1-3 (crossing, acceleration, string force) |
| Arm vs wrist | missing | No hit for arm or wrist stroke in `Source/` or spec |
| Swing, ghost strums | exists | `rhythm-engine.md` swing; `humanize.ghost_pct` |
| Push and pull | missing | Only timing jitter (`rhythm-engine.md` 135); no bias |
| Fret-hand damping between phrases | partial | `DSP/Techniques/MuteEngine.h` chuck damping; `string-interaction.md` 3 adjacent damping; no release between phrases |
| Mistakes (late, wrong) | partial | `humanize.miss_pct` (`rhythm-engine.md` 137); no late or wrong notes |

Only the rows marked partial or missing are specified below.

### 3.1 Tapped harmonic (partial)

Add `Technique::TappedHarmonic` (appended after `SlideGuitar`, `Strum`), with
keyswitch `tapHarmonic` = 22 in `TechniqueKeyswitch`. The engine voice is a
string excitation at the twelfth-fret node: the fundamental at half the
sounding length, mixed with the tap's finger-noise burst (`pick-noise.md`
style, seeded). A tap at the twelfth fret above a fretted note sounds that note's octave
(pitch = 2 x fretted pitch); over an open string, `harmonic-realism.md` sets
the node. Reuse `ArtificialHarmonic` voicing for the
node and add the tap's fast attack. Output tests in 8.

### 3.2 Whammy bar pre-bend (partial)

`WhammyEngine` gains a pre-bend state: the bar is pulled `bar_prebend_depth`
semitones (0-4, default 1) before the pick, over `bar_prebend_time` ms (10-300,
default 80), and released on the note. The trigger is a keyswitch (23) or a
CC, as `BendEngine` does for strings. The bar's pitch uses the existing
`whammy_up_range` and `whammy_down_range`; the pre-bend never exceeds the up
range. Spring behaviour (`whammy_springs`) decides how far the bar returns.

### 3.3 Volume swell (missing)

A `SwellGesture` per note: gain envelope from 0 to 1 over `vswell_rise` ms
(40-2000, default 400) with an exponential-ease curve, then optional hold
until the pick (`vswell_hold` on/off). The swell is applied to the note's
output gain before the amp, so the amp responds to the swell as it does to
the volume knob. Chords swell together. Swells are not applied to notes that
start with a pick attack unless `vswell_hold` is on, which keeps the attack
from being double-counted.

### 3.4 Hybrid picking and chicken pickin' (partial)

Hybrid: the pick takes the bass strings, the middle and ring fingers take the
treble (`fingerstyle-attack.md` Hybrid). Keep `hybrid_snap` (existing). Add a
per-string assignment: strings 1-3 to fingers, 4-6 to pick, selectable in the
right-hand card.

Chicken pickin': the fingers pull up and snap the treble strings (`hybrid_snap`
at 0.3 x velocity, per `fingerstyle-attack.md` 188). Add `chicken_snap_ms`
(15-60, default 30) for the snap length and `chicken_mute` (0-1, default 0.7)
for how fast the string is damped after the snap. The snap is a short
excitation through the existing `Excitation` path, not a new sample, and
belongs to the same strum event.

### 3.5 Sweep picking (missing)

A `SweepGesture` takes a chord shape (an ordered list of strings and frets)
and plays it as a single stroke across the strings in `sweep_speed_ms` per
string (3-40, default 12), up or down (`sweep_direction`). The stroke sets
each string's onset from the sweep time and damps the previously sounding
string as the pick moves on (`Muting::dampingFor`). It is driven by the
existing chord voicer: the chord's voicing is the shape, and the sweep order is
the string order. Sweeps are not a tap; they sound like a pick across.

### 3.6 Thumb-over bass (missing)

On bass and on guitar, the thumb plays a fixed string (`thumbover_string`,
4-6 for bass, default 4 on guitar) while the fingers play above it. The thumb's
strokes are scheduled as a separate voice: its excitation uses the Thumb
contact spec already in `Excitation::Material` (`fingerstyle-attack.md` 14),
with `thumbover_weight` (0-1, default 0.8) for how hard the thumb hits relative
to the fingers. A thumb note rings through, not muted by the fingers, because
the thumb is on a different string.

### 3.7 Arm versus wrist strokes (missing)

Extend `StrumGesture` with a stroke source: `arm` (broad, slow, large
acceleration, 60-120 ms per down-stroke), `wrist` (small, quick, 25-50 ms),
or `mixed`. Arm strokes give more crossing velocity and more string spread; wrist
strokes give tighter timing and less string-to-string variation. The
per-string force model (`strum-dynamics.md` 3) is kept; the stroke source only
changes the acceleration profile (2) and the timing spread. `strum_stroke_source`
is a choice (0 mixed, 1 arm, 2 wrist) with `mixed` as default.

### 3.8 Push and pull (missing)

Per part: `push_pull_ms` in -30 to +30 (negative pulls behind the beat, positive
pushes ahead). Applied as a constant bias before the existing timing jitter
(`humanize.timing_ms`), so the feel is steady rather than random. Push and pull
are per part (rhythm track), not global.

### 3.9 Fret-hand damping between phrases (partial)

`MuteEngine` damps the string under the pick and the chuck. Add release
mode: `ftdamp_mode` (0 off, 1 fret hand, 2 all unplayed strings). In mode 1,
between phrases (a gap longer than `ftdamp_gap_ms`, default 250), strings not
in the next chord are damped with the existing `Muting::dampingFor`, so they
stop ringing as the fret hand lets go.

### 3.10 Whole-strum dynamics (partial)

Crossing velocity, acceleration and string force exist in `strum-dynamics.md`
1-3. Missing: the stroke source in 3.7. That is the only addition.

### 3.11 Mistakes (partial)

Extend `humanize` with `mistake_late_pct` (0-5 %, default 0) and
`mistake_wrong_pct` (0-2 %, default 0). Late notes shift by 40-120 ms; wrong
notes replace the note with a neighbour in the same scale (one or two semitones,
chosen by the seed). Both are off by default and never apply to the first note
of a phrase. The existing miss probability (`miss_pct`) is unchanged.

## 4. Parameters

New, appended inside `// ==== BEGIN V2-TECH params ====`:

| ID | Range | Default | Automatable | Control |
|---|---|---|---|---|
| `bar_prebend_depth` | 0-4 st | 1 | yes | Whammy card |
| `bar_prebend_time` | 10-300 ms | 80 | yes | Whammy card |
| `vswell_rise` | 40-2000 ms | 400 | yes | Technique card |
| `vswell_hold` | bool | off | no (structural) | Technique card |
| `chicken_snap_ms` | 15-60 ms | 30 | yes | Right-hand card |
| `chicken_mute` | 0-1 | 0.7 | yes | Right-hand card |
| `sweep_speed_ms` | 3-40 ms | 12 | yes | Technique card |
| `sweep_direction` | choice: up, down | up | no | Technique card |
| `thumbover_string` | 1-6 | 4 | no | Right-hand card |
| `thumbover_weight` | 0-1 | 0.8 | yes | Right-hand card |
| `strum_stroke_source` | choice: mixed, arm, wrist | mixed | no | Strum card |
| `push_pull_ms` | -30 to +30 ms | 0 | yes | Rhythm card (per part) |
| `ftdamp_mode` | choice: off, fret hand, all unplayed | off | no | Mute card |
| `ftdamp_gap_ms` | 50-1000 ms | 250 | yes | Mute card |
| `mistake_late_pct` | 0-5 % | 0 | yes | Humanize card |
| `mistake_wrong_pct` | 0-2 % | 0 | yes | Humanize card |

Existing and unchanged: `hybrid_snap`, `thumb_palm_mute`, `thumb_position_offset`,
`nail_vs_flesh`, `whammy_*`, `strum_*`. Choices and booleans are not automatable
where they change structure; the table marks them. Count: 11 automatable, 5
structural. Keyswitches 22 (tapped harmonic) and 23 (bar pre-bend) are appended
to `TechniqueKeyswitch` after `slideGesture` = 21.

## 5. State, file format and migration

- Per-note flags (tapped harmonic, swell on a note) are stored in the riff and
  performance note records (`riff-library.md`), appended with defaults. v1
  riffs load with all flags off.
- The sweep shape is a chord voicing plus the order of strings. It is saved
  with the riff, not as a new file format.
- Presets gain a `techniques` block only when a v2 technique is used; a preset
  without it renders as v1.
- Migration: every new parameter defaults to the v1 behaviour (off, zero, mixed
  stroke source, neutral push/pull). `TechniqueKeyswitch` IDs 22 and 23 are
  unused in v1, so no existing mapping moves.

## 6. Edition

| Feature | Free | Pro |
|---|---|---|
| Tapped harmonic, whammy pre-bend, swell | yes | yes |
| Sweep, thumb-over, arm/wrist, push/pull | no | yes |
| Hybrid and chicken snap (existing `hybrid_snap`) | yes | yes |
| Fret-hand damping, mistakes | no | yes |

Free presets using a Pro technique load with the technique off and a notice.
Sound is not altered otherwise (`editions.md` 2).

## 7. Performance budget

Per `performance-budget.md` 0 units:

- Tapped harmonic: 0.02 (one excitation, shares StringEngine).
- Whammy pre-bend: 0.01 (a smoother).
- Swell: 0.01 per swelling note; eight notes at once is 0.08.
- Sweep: 0.03 per sweep (scheduler only; strings already exist).
- Chicken snap: 0.02 (short excitation).
- Thumb-over: 0.02 (one extra voice).
- Arm/wrist, push/pull, mistakes, damping: under 0.01 each (scheduling only).
- Total new worst case, all techniques on a 6-voice heavy preset: about 0.3
  units, against the 22-unit Heavy budget.

## 8. Tests

- **TQ-01 (tapped harmonic).** A tapped note over an open string sounds one
  octave above the fretted pitch within 5 cents; the fundamental is at
  half the sounding length.
- **TQ-02 (pre-bend).** Depth 1 st, time 80 ms: the pitch reaches the target
  within 5 cents at 80 ms +/-5 ms, then returns on the note.
- **TQ-03 (pre-bend range).** A depth above the up range is clamped; no
  pitch leaves the whammy range.
- **TQ-04 (swell).** Rise 400 ms: gain at 200 ms is within 0.5 dB of the
  exponential curve; at 400 ms gain is 1.
- **TQ-05 (swell attack).** With `vswell_hold` off, a picked note's attack
  transient is within 1 dB of the no-swell render.
- **TQ-06 (chicken snap).** Snap length 30 ms: energy is inside 25-35 ms and
  the string is damped by `chicken_mute` within 10 ms.
- **TQ-07 (sweep).** A three-string sweep at 12 ms per string has onsets within
  1 ms of 0, 12, 24 ms; each earlier string is damped as the next sounds.
- **TQ-08 (thumb-over).** The thumb string keeps ringing while the fingers play
  above it; the fingers' notes do not mute the thumb string.
- **TQ-09 (arm vs wrist).** Arm down-stroke duration is 60-120 ms; wrist 25-50 ms;
  wrist string spread is at most half the arm spread.
- **TQ-10 (push/pull).** Push +10 ms moves every onset 10 ms earlier, within
  0.1 ms, with timing jitter at 0 %.
- **TQ-11 (damping).** In mode 1, a gap of 300 ms with two unplayed strings
  silences both within 50 ms; in mode 0 they keep ringing.
- **TQ-12 (mistakes).** At late 5 %, over 10,000 notes the late fraction is
  4-6 %; no late note in the first note of a phrase.
- **TQ-13 (compat).** A v1 riff and preset render bit-identically with all
  new parameters at defaults (shared with PB-M01).
- **TQ-14 (seed).** Two renders with the same seed are identical; different
  seeds change only the mistake and snap-noise streams.
- **TQ-15 (visible controls).** `everyAutomatableParameterHasAVisibleControl`
  passes for the 11 automatable IDs.

## 9. Effort and dependencies

ED = engineer-days, one engineer with an AI pair.

| Work | ED |
|---|---|
| Tapped harmonic (3.1) | 5 |
| Whammy pre-bend (3.2) | 4 |
| Volume swell (3.3) | 4 |
| Hybrid and chicken snap (3.4) | 5 |
| Sweep (3.5) | 6 |
| Thumb-over (3.6) | 4 |
| Arm vs wrist (3.7) | 3 |
| Push and pull (3.8) | 2 |
| Fret-hand damping (3.9) | 3 |
| Mistakes (3.11) | 3 |
| Parameters, UI cards, Easy summaries | 5 |
| Tests TQ-01 to TQ-15 | 6 |
| **Total** | **50 ED, about 10 weeks** |

Dependencies: `harmonic-realism.md` (harmonic node model, tapped harmonic);
`fingerstyle-attack.md` (Thumb contact, hybrid snap); `strum-dynamics.md`;
`rhythm-engine.md` (humanize, per-part timing); `string-interaction.md` and
`muting-rhythm.md` (damping); `riff-library.md` (note flags).

## 10. Open questions

1. **Swell and velocity.** Should a swelled note's velocity map to swell depth,
   or stay independent? Recommend independent in v2.0.
2. **Sweep on guitar vs bass.** Is sweep limited to chord shapes of four or
   more strings? Recommend yes; smaller shapes are arpeggios.
3. **Mistakes default.** Off by default is the only honest choice for a
   production tool; confirm with the rhythm owner.
4. **Tapped harmonic keyswitch.** Keyswitch 22 is free, but the MIDI export
   profile (`MIDI_EXPORT_LUTHIER_PROFILE.md`) must also carry the new
   technique name `tapharm`. Confirm.
