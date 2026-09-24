# FINGERSTYLE ATTACK SPEC

A classical guitarist, a Travis picker, a flamenco player and a funk
bassist all play with their fingers, and none of them sound alike. The
difference is not which string or how hard. It is **what touches the
string and how it lets go**: a nail edge releases in a few hundredths of
a millisecond, a fingertip pad rolls off in a tenth; a rest stroke
drives the string toward the top and lands on the next string, a free
stroke pulls it across and clears it; a thumb is wider and slower than
a finger; a thumbpick is a pick on a thumb.

Luthier has the pieces and not the player. What exists today:

- `Excitation::Material` has `Fingernail`, `Fingertip`, `Thumb` and
  `Thumbpick` contact specs, and `nail_vs_flesh` blends the flesh and
  nail **cutoff only**. The resonator peak, contact length and noise come
  from whichever material `LuthierEngine::setUseFingers` picked on its
  side of 0.5, so the blend has a step in it at 0.5.
- `use_fingers` and `pick_material` choose one tool for all strings.
- `pick-noise.md` 6 silences click, chirp and scrape with fingers and
  adds a fingertip release noise 12 dB under the pick's.
- `rhythm-engine.md` 5 fingerpick patterns name a finger (`p i m a e`)
  per step, and `RhythmEngine::scheduleFingerpick` drops it: every
  finger plays as the global tool.
- `bass-techniques.md` 6 specifies rest stroke and finger alternation
  for bass, not yet built; `string-slap-technique.md` and the WIP
  `SlapEngine` own slap and pop.

This file turns those into **contact profiles**, assigns a tool per
string, carries the finger through from the rhythm engine, and adds the
stroke (free / rest). It extends pick-noise 6, bass-techniques 6 and
string-slap 6; it contradicts none of them.

## 0. Ground rules

1. **The tool is an excitation, not an EQ.** Every profile is a change
   to `Excitation::Params` (release time, contact length, kind, noise),
   a change to the note's damping or bridge drive, or a neighbour
   damping. Nothing is filtered afterward.
2. **The pick path does not move.** A note resolved to a pick renders
   bit-identically to today, including every pick-noise event.
3. **Release time is the physics.** A contact that releases the string
   over `tau` seconds cannot put much energy above `1 / (2 pi tau)`.
   The finger profiles are specified by `tau`; the shipped flesh (2.2
   kHz), nail (7.0 kHz) and thumb (1.1 kHz) cutoffs are what their
   default `tau` produce.
4. **Honest magnitudes.** Rest stroke is 1.5-3 dB louder than free
   stroke at the same effort, with a slightly darker, fuller attack and
   a slightly shorter ring. A nail click is 35-40 dB under the note.
   Alternation between two fingers is a few percent of tone and a
   millisecond or two of time.
5. **One owner per mechanism.** Slap and pop are `SlapEngine`'s. Pick
   click, chirp and fingertip noise are `PlayingNoise`'s. Rest-stroke
   damping and alternation are built here once and used by the bass
   family too.

## 1. Contact profiles

A profile fills `Excitation::Params` for one note. New field
`Params::releaseSeconds` (-1 keeps the material table): when set, the
excitation lowpass base is `1 / (2 pi tau)` instead of the table's
`lowpassHz`. Velocity, brightness, thickness and angle trims apply on
top as today.

| Tool | Material basis | `tau` | Position | Kind | Noise |
|---|---|---|---|---|---|
| Pick | `pick_material` | table | `pluck_position` | Pluck | pick-noise click / chirp |
| Finger | Fingertip <-> Fingernail by `nail_vs_flesh` | `lerp(flesh, nail, b)` | `pluck_position` | Pluck | fingertip noise + nail click x `b` |
| Thumb | Thumb, nail side by `0.5 b` | `lerp(2 x flesh, nail, 0.5 b)` | `+ thumb_position_offset` | Pluck | fingertip noise |
| Thumbpick | Thumbpick | table | `+ thumb_position_offset` | Pluck | pick-noise click |
| Slap | - | - | slap position | `SlapEngine` thumb | `SlapEngine` |
| Pop | - | - | pop position | `SlapEngine` pop | `SlapEngine` |

`b` is `nail_vs_flesh`. For Finger and Thumb, **every** field of
`MaterialSpec` (peak Hz, peak dB, noise scale, length scale) is blended
linearly between the two specs by the same `b`, not only the cutoff.
That removes today's step at 0.5; test FA-02 pins it.

Defaults reproduce the shipped table: `finger_flesh_release_ms` 0.0723
gives 2.20 kHz, `finger_nail_release_ms` 0.0227 gives 7.01 kHz, and the
thumb's `2 x flesh` = 0.1447 ms gives 1.10 kHz.

**Nail click.** A nail meeting a steel string clicks. When `b > 0` the
Finger tool fires `PlayingNoise`'s click generator with material
Fingernail at `pick_click_amount x 0.35 x b`, which puts it 35-40 dB
under the note (pick-noise 3's click sits 30 dB under at amount 0.5).
The fingertip release noise of pick-noise 6 is unchanged.

**Thumb position.** The thumb strikes nearer the neck than the fingers
(it hangs further out of the hand). `thumb_position_offset` is added to
`pluck_position` for thumb-class tools, clamped to 0.02-0.5.

## 2. Stroke: free (tirando) and rest (apoyando)

**Physics.** A free stroke pulls the string roughly parallel to the top
and the finger clears the next string. A rest stroke pushes it toward
the top and the finger comes to rest on the adjacent string. A guitar's
bridge admittance is several times higher perpendicular to the top, so
the rest stroke's motion drives the top harder: more level, a fuller
fundamental, and a shorter ring because energy leaves faster.

**Model**, for finger-class tools with the stroke resolved to Rest:

| Term | Free | Rest | Where |
|---|---|---|---|
| Level | x1.0 | x1.35 (+2.6 dB) | `Excitation` `kindGain` |
| Contact length | x1.0 | x1.10 | `pluckLen` |
| Brightness | x1.0 | x0.90 | `brightTrim` |
| Bridge drive | x1.0 | x1.30 | `StringEngine::Physical::couplingSend`, this note |
| Sustain | x1.0 | x0.85 | `StringEngine::setSustainScale`, this note |
| Neighbour | - | damped | below |

The engine has one polarisation, so "toward the top" is carried by the
bridge drive and sustain terms, which are the audible consequences.

**Neighbour damping.** The finger lands on string `s + 1` (the next
lower-pitched string); a thumb rest stroke lands on `s - 1`, because the
thumb moves toward the treble. The neighbour gets `Damping::Chuck` with
amount `rest_stroke_damping` (strum-dynamics 6.1's hand-on-string
damping: `T60 = T60_note^(1-a) x 0.01^a`, about 30 ms at 0.8) until the
next note-on on either string. This is the mechanism
`bass-techniques.md` 6 asks for: on a bass, its `rest_stroke` bool, when
on, forces Rest for finger tools and uses this same damping.

**Auto** (`rh_stroke` = Auto) is the classical rule: a finger-class note
with no other note-on within 30 ms (a melody note, not a chord) and
velocity at least 0.7 is a rest stroke; everything else is free.

## 3. Per-string tools

`rh_string_tool_1` ... `_6` (String 1 is the high E, as in
`routing-io.md` 3) choose each string's tool; `Global` means the
existing `pick_material` / `use_fingers` / `nail_vs_flesh` path,
unchanged. Strings 7-12 follow their course (12-string) or the lowest
assigned string (7- and 8-string), as `setup_nut_depth` does.

A note's tool resolves in this order, in `LuthierEngine::triggerNote`
before the `Excitation::Params` are built:

1. **CC 102 live override** (section 5), if held.
2. **The pattern's finger** (`NoteOnEvent::finger`, new, below): `p`
   forces thumb class (the string's tool if it is Thumb, Thumbpick or
   Slap, else Thumb); `i m a e` force finger class (Finger or Pop, else
   Finger).
3. **The string's tool** (`rh_string_tool_N`).
4. **Global.**

`strikerMaterial` (strum-dynamics 5) still wins over all four for
strums: a strum's striker is what hit the string.

**Carrying the finger.** `NoteOnEvent` gains `int finger = -1` (a
`Finger` from `Rhythm/Patterns.h`). `RhythmEngine::scheduleFingerpick`
passes `step.finger` through `emitNote`, so a p-i-m-a pattern finally
plays with a thumb and three fingers, as `rhythm-engine.md` 5 intended.

**Alternation.** Consecutive finger-class notes alternate `i` and `m`
(or follow the pattern's letters). The `m` stroke gets `tau x (1 + 0.15
v)`, `pluck_position + 0.01 v` and a timing offset of `+1.5 v` ms, where
`v` = `finger_alternation_variation` (bass-techniques 6's parameter,
applied to every family). Deterministic: no randomness, so it round-trips.

## 4. Styles

`rh_style` writes the per-string tools and related controls, as
`setup_style` does for the setup (`fret-buzz.md` 6.1): on a user change
only, on the message thread, as one undo entry. Loading a preset never
re-applies it. Custom is the default and writes nothing.

| Style | Strings 1-3 | Strings 4-6 | Also sets |
|---|---|---|---|
| Custom | - | - | nothing |
| Pick | Global | Global | `use_fingers` off |
| Fingerstyle | Finger | Thumb | stroke Free, `nail_vs_flesh` 0.4 |
| Classical | Finger | Thumb | stroke Auto, `nail_vs_flesh` 0.7 |
| Travis | Finger | Thumbpick | `thumb_palm_mute` 0.35, `nail_vs_flesh` 0.3 |
| Hybrid | Finger | Pick | `hybrid_snap` 0.3, `pick_material` Celluloid |
| Slap & Pop | Pop | Slap | - |

- **Travis.** Thumb-class notes get `Damping::PalmMute` at
  `thumb_palm_mute`: the heel of the hand lightly muting the
  alternating bass while the fingers ring. It is per note, so it does
  not engage `string-interaction.md` 2's palm spread (that follows
  CC 67).
- **Hybrid.** The pick takes the bass strings; middle and ring fingers
  take the treble and pull up and snap. `hybrid_snap` sends each
  finger-class note on a guitar that has any Pick string through
  `SlapEngine`'s pop collision path (the fret-buzz event with large
  excess) at strength `0.3 x hybrid_snap x velocity`: chicken-picking's
  snap is a small pop.
- **Slap & Pop.** The Slap and Pop tools classify the note as a
  `SlapEngine` thumb slap or pop whether or not the SLAP technique is
  armed: choosing the tool is the intent. Strength and position come
  from `SlapSettings`. This is the "Slap" and "Pop" entries
  `string-slap-technique.md` 6 asks the tool selector for, and works on
  any family, as that spec generalises.

## 5. Triggering

| Trigger | Default | Effect |
|---|---|---|
| CC 102, new `MidiTarget::RightHandTool` | value bands of 128/7: Off, Pick, Finger, Thumb, Thumbpick, Slap, Pop | Live override of every string's tool while not Off |
| CC 105, new `MidiTarget::RestStroke` | >= 64 | Forces Rest while held |
| Velocity | Auto stroke only | >= 0.7 single notes rest |
| Pattern finger | rhythm engine | Section 3, step 2 |

CC 102-119 are undefined in MIDI 1.0; `harmonic-realism.md` takes 103
and 104. No keyswitch is used: 12-18 are taken and the range stops
below MIDI 21 (a drop-A bass). For `technique-cascade.md` 1, every tool
is an excitation source; Slap and Pop inherit Slap's row of the matrix.

## 6. Parameters

| ID | Name | Stock | Advanced | Default | Unit | Family |
|---|---|---|---|---|---|---|
| `finger_flesh_release_ms` | Flesh Release | 0.04 – 0.20 | 0.01 – 1.0 | 0.0723 | ms | `pick` |
| `finger_nail_release_ms` | Nail Release | 0.015 – 0.06 | 0.005 – 0.2 | 0.0227 | ms | `pick` |
| `thumb_position_offset` | Thumb Position | -0.05 – 0.10 | -0.20 – 0.30 | 0.04 | fraction of string | `pick` |
| `rest_stroke_damping` | Rest Damping | 0 – 1 | 0 – 1 | 0.8 | ratio | `pick` |
| `rh_stroke` | Stroke | choice Free / Rest / Auto | – | Free | – | none |
| `rh_style` | Right-Hand Style | choice (section 4) | – | Custom | – | none |
| `rh_string_tool_1` ... `_6` | String N Tool | choice Global / Pick / Finger / Thumb / Thumbpick / Slap / Pop | – | Global | – | none |
| `thumb_palm_mute` | Thumb Palm Mute | 0 – 1 | – | 0 | amount | none |
| `hybrid_snap` | Hybrid Snap | 0 – 1 | – | 0.3 | amount | none |

`rest_stroke_damping` keeps the same pair twice (`advanced-ranges.md`
3.3): past 1 the chuck formula has no meaning. Choice lists are append
only. `finger_alternation_variation` and `rest_stroke` are
bass-techniques 11's IDs and are reused, not duplicated. Net new:
**+14**.

## 7. UI and state

- `gui-integration.md` 4.4 CHARACTER tab, new **RIGHT HAND** group
  (the one `gui-techniques-updates.md` 5 extends with Tapping): style
  dropdown; a six-cell string row with a tool glyph per string (click
  cycles, right-click lists); stroke; flesh and nail release (shown as
  their resulting kHz as well as ms); thumb position; rest damping;
  Travis mute; hybrid snap; alternation variation. The PICK group's
  `use_fingers` and `nail_vs_flesh` stay where they are and are mirrored
  here.
- Easy Mode Playing strip (`gui-integration.md` 3.3): the **Tool
  selector** that `gui-techniques-updates.md` 2 places the technique
  pills under. It is `rh_style` as a segmented control; Custom shows as
  "Mixed".
- Illustration: the picking hand's contact point per string shows the
  tool glyph at `pluck_position` (thumb-class strings offset toward the
  neck).
- Section 19 row: "Right-hand tools, stroke, finger release | Excitation
  profiles | Col 4 CHARACTER -> RIGHT HAND | Playing strip Tool selector
  | TECHNIQUES -> SLAP (slap / pop)".
- Preset: plain APVTS parameters. `midi-export.md` PICK class gains
  `tool`, `finger` and `stroke` per note; Generic profile carries none
  of them, which is its honest loss.

## 8. Performance and realtime safety

- All work is at note-on: one tool resolution, a few extra multiplies
  in `Excitation::trigger`, and at most one neighbour damping change.
  No per-sample cost. Budget: **0.02 units** at 16 notes/s; idle 0.
- No allocation: `Params` is a POD, the neighbour-rest state is a fixed
  per-string array, the nail click uses the existing generator pool.
- `reset()` clears rest-stroke neighbours and the alternation phase.
- The style write is message-thread only; the audio thread only reads
  parameters.

## 9. Tests

- **FA-01 Nail is brighter.** Same note and velocity 0.8, Finger tool:
  `nail_vs_flesh` 1 versus 0 gives a contact (excitation) power
  centroid at least 1.6x higher.
- **FA-02 No step at 0.5.** Sweeping `nail_vs_flesh` 0.45 to 0.55 in
  0.01 steps, adjacent contacts' centroids never differ by more than
  4 %.
- **FA-03 Release time sets the cutoff.** `Excitation::trigger` with
  `releaseSeconds` = `tau`, velocity 0.478 and brightness 0.364 (both
  trims at unity), Fingertip: the excitation buffer's -3 dB point is
  `1 / (2 pi tau)` within 15 % for `tau` = 0.04, 0.0723 and 0.2 ms.
- **FA-04 Defaults reproduce the table.** With default release times,
  a Finger note at `b` = 0 and `b` = 1 renders within -60 dB null of
  the same note through the old Fingertip and Fingernail material
  cutoffs.
- **FA-05 Pick path untouched.** Every factory pick preset renders
  bit-identically before and after this change.
- **FA-06 Rest stroke level and tone.** Same note and velocity: Rest is
  1.5-3 dB louder at peak than Free, its first-50 ms centroid is 5-20 %
  lower, and its T60 is 0.8-0.9x.
- **FA-07 Rest damps the neighbour.** String index 3 ringing, finger
  rest stroke on index 2 with default damping, bridge coupling off:
  index 3's partials are at least 12 dB down in the 10 ms ending three
  of its periods plus 10 ms after the stroke (the lumped loop damps once
  per round trip). Free stroke: less than 1 dB. A thumb rest stroke
  on index 4 damps index 3, not index 5.
- **FA-08 Auto stroke.** Auto: a single note at velocity 0.8 is Rest; a
  three-note chord at 0.8 is Free; a single note at 0.5 is Free.
- **FA-09 Pattern fingers reach the string.** A p-i-m-a pattern: every
  `p` note is rendered with a thumb-class material and every `i m a`
  note with a finger-class one (asserted on the recorded `Params`).
- **FA-10 Per-string tools.** Hybrid style: strings 4-6 pick (pick-noise
  click fires), strings 1-3 finger (no pick click, nail click at the
  expected level: 0.35 b of a pick click, 47 dB under the note at `b`
  0.4 and 39 dB at `b` 1).
- **FA-11 Travis mute.** Travis style: the thumb string's T60 is at most
  0.75x its T60 under Fingerstyle (0.35 on the palm-mute curve is 0.69x).
- **FA-12 Alternation.** 100 consecutive treble finger notes at
  variation 0.25: odd and even notes' contacts differ by at least 0.3 %
  in centroid and by 0.3-0.5 ms in mean onset; at 0 they are
  bit-identical.
- **FA-13 Slap and Pop tools.** On a non-bass guitar, a Slap-tool note
  renders `Excitation::Kind::Slap` and triggers `NoiseEngine::FretBuzz`;
  a Pop-tool note triggers the pop path. `hybrid_snap` 0 triggers no
  buzz on hybrid finger notes; 1.0 does.
- **FA-14 Thumb position.** A thumb-class note's `pluckPosition` equals
  `pluck_position + thumb_position_offset`, clamped to 0.02-0.5.
- **FA-15 CC triggers.** CC 102 at each band selects that tool for every
  string; back to Off restores the per-string tools on the next note;
  CC 105 at 64 forces Rest.
- **FA-16 Style writes once.** Changing `rh_style` writes its table in
  one undo entry; loading a preset with a style stored does not
  overwrite that preset's per-string tools.
- **FA-17 Ranges, realtime, round trip.** Physical rows valid in
  `RangeRegistry` with no declaration mismatch; no allocation on the
  audio thread across FA-01 to FA-15; measured cost within 0.02 units;
  a Luthier-profile export of a p-i-m-a passage with rest strokes
  re-imports to a -60 dBFS RMS null.

## Build notes (REALISM-B, 2026-09-24)

1. **Rest-stroke terms** are level 1.35 and contact 1.10 (was 1.26 and
   1.15): a longer contact spreads the same peak over more samples, so
   the draft's terms raised the note's peak only 1.2 dB and lowered its
   centroid 21 %; these land FA-06's 1.5-3 dB and 5-20 %.
2. **Where tone is measured.** The string's first 50 ms is dominated by
   its own partials: its centroid moved 1 % between nail and flesh while
   the contact's moved 87 %. FA-01, FA-02 and FA-12 measure the contact
   (the note's recorded `Params` rendered again).
3. **Alternation** at `v` = 0.25 moves the contact's centroid 0.5 % with
   section 3's own terms, so FA-12's floor is 0.3 %; the `m` stroke's
   +1.5 v ms is an excitation start delay.
4. **Global stays Global.** The contact profiles (release time, whole
   `MaterialSpec` blend) apply to the Finger and Thumb tools; a string on
   Global keeps the old path exactly, step at 0.5 included (FA-05).
5. **Pattern fingers win over Global.** As section 3 says, a fingerpick
   pattern's `p` plays a thumb and `i m a` fingers even on Global strings,
   so presets that run fingerpick patterns now sound picked by fingers.
6. **Bass parameters.** `finger_alternation_variation` and `rest_stroke`
   are read when bass-techniques.md declares them; until then alternation
   variation is 0 and the RIGHT HAND group says so.
7. **Styles** are written only by the RIGHT HAND style box and the Easy
   Tool selector (`RightHandGroup::applyStyle`, one undo entry); nothing
   in preset loading calls it.
8. **Deferred**: `midi-export.md` PICK fields `tool`, `finger` and
   `stroke` are capture-path metadata owned by midi-export; the audio
   round trip holds without them (the tools are parameters, CC 102/105
   are in the stream).
