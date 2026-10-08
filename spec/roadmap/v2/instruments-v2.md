# INSTRUMENTS V2 SPEC

Luthier models guitars and basses as parts: a body, a neck, a bridge, pickups
and strings, with the physics of each (`part-acoustics.md`). The v2 instrument
set covers the neighbours a guitar player reaches for next: lap steel and
resonator, ukulele, mandolin, seven- and eight-string guitars, fretless bass,
and flamenco nylon with its golpe. Several are already close. Others need a new
family, a new body, or a new playing technique.

The seven instruments already specified in `spec/instruments/` (Stick,
guitarron, chitarra sarda, composite-neck bass, tenor guitar, acoustic bass,
extended-range bass) are not repeated here. Where an item below depends on
their multi-scale or touch-family work, it says so.

## 0. Ground rules

1. **Data before DSP.** A new instrument is first a part set (strings, nut,
   bridge, body, tuning presets). New DSP only where the physics differs. Each
   item states which of the two it needs.
2. **Measured beats inferred beats invented.** Numbers carry the tags from
   `spec/instruments/README.md` 0: **M** measured or cited in a research file,
   **I** derived by a stated formula, **D** design default. Every D value is in
   the instrument's data-gap list and is replaced when a research file lands.
   The house rule is that each instrument gets `docs/research/INSTR_<name>.md`
   before its data is final; this spec names those files and does not write
   them.
3. **No new family unless needed.** A family is added only where the existing
   families (`acoustic`, `classical`, `bass`, `resonator`, `electric`) cannot
   express the instrument. Ukulele and mandolin need `acoustic` plus tuning and
   scale data, unless the body or course layout forces a new family.
4. **Append-only IDs.** New parameters go in `// ==== BEGIN V2-INST params ====`.
5. **Tuning and scale are structural.** They change the instrument, so they are
   state, not automation, and they follow the rule in `state-model.md`.

## 1. User stories

- **U1.** I play lap steel with an open E9 tuning and a bar, and the strings
  ring out with no fret-hand damping.
- **U2.** I play a resonator with a dobro bar, and the cone sound is
  unmistakable.
- **U3.** I play a ukulele in reentrant tuning, and the high G sounds like one.
- **U4.** I play mandolin with paired courses, and tremolo picking sustains a
  chord.
- **U5.** I play an eight-string with a low F# and a fanned fretboard that
  feels right under my hand.
- **U6.** I play fretless bass with a glide between notes and no fret buzz.
- **U7.** I play flamenco nylon with a golpe on the top, and rasgueado chords
  at speed.

## 2. UI

Each instrument appears in the guitar picker under its family, with its tuning
preset and scale length shown. Instrument-specific controls are on the card for
that instrument; Easy Mode shows only the instrument name and the tuning
preset. Every control has a tooltip.

| Instrument | Control | Tooltip |
|---|---|---|
| Lap steel | Tuning preset (C6, E9, A6, custom) | "Open tuning the bar is played in" |
| Lap steel | Bar, mode `lap_steel` (exists) | "Bar touches every string; frets are damped" |
| Dobro / resonator | Cone on/off (body) | "The metal cone's extra bite and sustain" |
| Ukulele | Tuning (reentrant GCEA, linear GCEA, baritone DGBE) | "Reentrant: the fourth string is high G" |
| Mandolin | Course spread (0-8 cents) | "Detunes the two strings of each course" |
| Mandolin | Tremolo rate (8-20 Hz) | "Rate of rapid alternate picking on held notes" |
| 7-string | Low B extension (on/off, in tuning) | "Adds a low B below the low E" |
| 8-string | Low F# extension | "Adds a low F# below the low B" |
| Fretless bass | Glide time (20-300 ms) | "Time a note takes to slide to the next pitch" |
| Flamenco nylon | Golpe amount (0-1) | "How hard the hand strikes the top" |
| Flamenco nylon | Rasgueado speed (20-120 ms per string) | "Time from the first string to the last in a rasgueado" |

## 3. Engine and data model

### 3.1 Status

| Instrument | Status | Evidence |
|---|---|---|
| Lap steel (playing) | partial | `DSP/Slide/SlideEngine.h` `SlideMode::lapSteel`; `slide-guitar.md` 1 |
| Lap steel (instrument) | missing | No `lap` part or guitar file; `Resources/Guitars` has none |
| Resonator, dobro (body) | exists | `Resources/Guitars/Resonator/Resonator Steel.luthierguitar`; `BodyShape::Resonator` (`PartAcoustics.cpp` 112-113, 154) |
| Dobro (playing) | exists | `SlideMode::dobro` (`SlideEngine.h`), with `slide-guitar.md` 1 |
| Ukulele | missing | Tuning only, in `Notation/AsciiTabReader.cpp` 276-277; no part or body |
| Mandolin | missing | Tuning only, `AsciiTabReader.cpp` 278; no course model |
| 7-string | exists | `Resources/Guitars/Electric/7-String Modern`; `Parts/Strings/Seven-String NPS` |
| 8-string | exists | `Resources/Guitars/Electric/8-String Modern`; `Parts/Strings/Eight-String NPS` |
| Multi-scale (fanned frets) | spec only | `extended-range-bass.md` 1.1 ("true multi-scale fields") |
| Fretless bass | exists | `Resources/Guitars/Bass/Fretless Bass.luthierguitar`; `Parts/Frets/Fretless (unlined)`; `Parameters.h` `fretless` |
| Fretless glide | partial | `Parameters.h` `slide_*` apply to slide only; no fretless glide |
| Flamenco nylon | exists | `Resources/Guitars/Classical/Flamenca Blanca`; `BodyOutlines.h` (flamenco outline) |
| Golpe (sound) | missing | Golpeador is drawn (`GuitarRenderer.cpp` 1103); the sound is not modelled. `TechniqueKeyswitch::bodyTap` (17) is the nearest voice |
| Rasgueado | partial | `LuthierEventClass::rasgueado` in `Export/LuthierMidiEvents.*`; no playing model |
| Rest stroke | exists | `rest_stroke`, `rest_stroke_damping` (`Parameters.h` 385, 412) |

### 3.2 Lap steel and resonator

Lap steel is a `slide` instrument with the bar on every string and no frets.
The `lapSteel` mode exists; the part set does not. Add a `lap` part family
under `acoustic` with: six or eight strings (`gauges_in` from a lap-steel set,
D), scale length 610-660 mm (D), a flat neck and high nut, and a solid or
resonator body. The resonator body already exists (`BodyShape::Resonator`); a
lap steel uses it, or a solid body with `lapSteel` mode. Tunings are presets
(C6, E9, A6), which are data, not code.

The dobro is the same bar on a resonator body. Its distinct sound (the cone)
is the body already modelled; no new body is needed.

### 3.3 Ukulele

New family `uke` (under `acoustic` for the parts model). Four strings, scale
330 mm for soprano, 381 mm for concert, 432 mm for tenor (D, research file
`docs/research/INSTR_ukulele.md`). Tunings are data from the existing tab
reader table: reentrant GCEA (strings 1-4: A4 = 69, E4 = 64, C4 = 60, G4 = 67; the
fourth string is the G above the C), linear GCEA (low G on the bottom), and baritone DGBE. Body:
a small soundbox with a soundhole and a top-to-back ratio that places the main
air resonance near 250-300 Hz (D). Reentrant tuning is a per-string octave
flag; `StringEngine` already takes pitch per string, so no string model changes.

### 3.4 Mandolin

Mandolin is eight strings in four courses (G3 D4 A4 E5, reentrant with the
octave-up pairs). Each course is two strings at the same pitch, detuned by
`mando_course_spread` cents to make the beat, or set to the octave in the
"octave" courses. The existing 12-string handling already shares a nut slot
between pairs (`Parameters.h` 207, "A 12-string's pairs share"). Reuse that
pairing; add the octave option. Scale 325-355 mm (D). Body: a bowl-back or
flat-back soundbox; the bowl is a new body shape, not a guitar shape (D).

Tremolo picking is a rapid alternate stroke on one or more strings:
`mando_trem_rate` (8-20 Hz) sets the stroke rate, and each stroke is one
`StrumGesture` event at alternate direction. Held notes sustain at the
tremolo rate. The stroke scheduler is the rhythm engine's; the new part is the
tremolo pattern.

### 3.5 Seven- and eight-string

Parts exist (`Seven-String NPS`, `Eight-String NPS`) and the electric bodies
exist. The delta is the multi-scale neck (`extended-range-bass.md` 1.1 defines
the multi-scale fields and is the owner of that work). A seven-string's low B
and an eight-string's low F# are tuning presets. Multi-scale gives the low
string a longer scale than the high string (fanned frets), which changes the
fret positions and the intonation; the change is data, once the multi-scale
fields exist. Until then the instruments play with a single scale (v1
behaviour) and say so in the tooltip.

### 3.6 Fretless bass

The instrument exists. The delta is the glide. Add a `FretlessGlide` in the
bass path: when a note follows another on the same string without a pluck,
pitch glides from the previous note over `fretless_glide_ms` (20-300, default
90) with an ease-in curve. There is no fret buzz (the fret-buzz model does not
apply to a fretless neck, `fret-buzz.md`). The glide is pitch only; the finger
noise (`fingerstyle-attack.md`) and the string's decay are unchanged.

### 3.7 Flamenco nylon: golpe and rasgueado

**Golpe** is the hand striking the top, usually the golpeador (the tap plate).
The sound is a short, broadband thump at the top's low body modes plus a dry
click. Model it as an impulse into the body (`BodyEngine`) with a golpe
excitation: a 5-15 ms lowpassed noise burst at 100-400 Hz plus a 2-4 kHz click,
scaled by `flam_golpe_amount`. The golpe is triggered by the existing
`bodyTap` keyswitch (17) on this instrument, or by a dedicated keyswitch
(24, appended). It is seeded.

**Rasgueado** is a fast strum with the fingers across the strings, in a short
ascending or descending stroke. The MIDI event class exists
(`LuthierMidiEvents`); the playing model is new: each string is one stroke
offset by `flam_rasgueado_speed` (20-120 ms total across the strings), with a
nail-attack excitation (`nail_vs_flesh` already has the contact model). A
rasgueado is one note event, as a strum is.

Rest stroke (`rest_stroke`, existing) is the apoyando stroke and is unchanged.

## 4. Parameters

New, appended inside `// ==== BEGIN V2-INST params ====`:

| ID | Range | Default | Automatable | Control |
|---|---|---|---|---|
| `lap_tuning` | choice: C6, E9, A6, custom | E9 | no | Lap steel card |
| `uke_tuning` | choice: reentrant, linear, baritone | reentrant | no | Ukulele card |
| `mando_course_spread` | 0-8 cents | 3 | yes | Mandolin card |
| `mando_octave_courses` | bool | off | no | Mandolin card |
| `mando_trem_rate` | 8-20 Hz | 12 | yes | Mandolin card |
| `seven_low_b` | bool | off | no | Seven-string tuning |
| `eight_low_fsharp` | bool | off | no | Eight-string tuning |
| `fretless_glide_ms` | 20-300 ms | 90 | yes | Fretless card |
| `flam_golpe_amount` | 0-1 | 0 | yes | Flamenco card |
| `flam_rasgueado_speed` | 20-120 ms | 60 | yes | Flamenco card |

Ten new IDs, five automatable. Structural choices are not automatable. The
keyswitch for golpe (24) is appended to `TechniqueKeyswitch`.

## 5. State, file format and migration

- Instrument identity, tuning preset and octave-course flag live in the part
  set (`Parts/`, `.luthierguitar`), so a saved instrument carries them. New
  instruments are new `.luthierguitar` files; no existing file changes.
- A preset that uses an instrument not yet shipping (for example a ukulele
  preset on a build without it) loads the nearest existing instrument with a
  notice. The notice names the original.
- v1 presets load unchanged. The new parameters default to off or zero, so
  every existing guitar and bass renders as before.
- Tuning presets are stored as data in `Resources/` (like existing tunings).

## 6. Edition

| Instrument | Free | Pro |
|---|---|---|
| Lap steel, dobro (resonator) | Pro | Pro |
| Ukulele, mandolin | Pro | Pro |
| Seven- and eight-string (single scale) | Free | Free |
| Multi-scale 7/8-string | Pro | Pro |
| Fretless bass with glide | Free | Free |
| Flamenco nylon with golpe and rasgueado | Pro | Pro |

Free keeps every instrument it has today. New instruments are Pro, as new
instruments are a Pro feature across the product (`editions.md` 2).

## 7. Performance budget

Units per `performance-budget.md` 0.

- New instruments are data and use the same `StringEngine` and body paths: no
  new per-string cost beyond the instrument's string count.
- Mandolin's paired courses: eight strings at the cost of eight voices (about
  0.2 units at full chord).
- Golpe: 0.02 units (one short excitation).
- Rasgueado: 0.01 units (scheduler only).
- Glide: 0.01 units per gliding note.
- Multi-scale: no DSP cost; it changes positions only.

## 8. Tests

- **IN-01 (lap steel).** Open E9 with a bar: all six strings sound at the bar
  pitch; no fret damping is applied (`slide-guitar.md` 3).
- **IN-02 (dobro).** Resonator body, dobro mode: the body's upper mode is
  present within 3 dB of the factory resonator reference (`BodyShape::Resonator`).
- **IN-03 (ukulele reentrant).** Strings 1-4 sound 69, 64, 60, 67 and the lowest
  pitched string sounds 60, within 5 cents of the tab reader's reference.
- **IN-04 (ukulele body).** Air resonance is between 250 and 300 Hz (D) within 10 Hz.
- **IN-05 (mandolin courses).** A course at spread 3 cents beats at 3 cents, within
  0.2 cent. An octave course sounds one octave apart.
- **IN-06 (tremolo).** At 12 Hz, the stroke interval is 83 ms within 1 ms.
- **IN-07 (seven and eight).** Low B (MIDI 35) and low F# (MIDI 30) tune within 5
  cents. With multi-scale off, the scale is the single v1 value.
- **IN-08 (fretless glide).** A 90 ms glide reaches the target within 5 cents
  at 90 ms +/-5 ms and never overshoots by more than 10 cents.
- **IN-09 (fretless, no fret buzz).** The fret-buzz generator is silent on a
  fretless part at any action setting.
- **IN-10 (golpe).** Energy is concentrated between 100 and 400 Hz plus a
  click in 2-4 kHz; the duration is 5-15 ms at -20 dB.
- **IN-11 (rasgueado).** Six strings across 60 ms: onsets within 2 ms of the
  scheduled offsets; attack is nail-like (`nail_vs_flesh` = 1).
- **IN-12 (compat).** Every existing guitar and bass preset renders
  bit-identically with all new parameters at defaults (shared with PB-M01).
- **IN-13 (visible controls).** `everyAutomatableParameterHasAVisibleControl`
  passes for the five automatable IDs.
- **IN-14 (fallback).** A preset naming an absent instrument loads the nearest
  instrument and shows the notice.

## 9. Effort and dependencies

ED = engineer-days, one engineer with an AI pair. Research files and part data
are counted in the same estimate.

| Work | ED |
|---|---|
| Lap steel part set and presets (3.2) | 8 |
| Dobro card and tests (3.2) | 2 |
| Ukulele part set, body, tunings (3.3) | 12 |
| Mandolin courses, bowl body, tremolo (3.4) | 14 |
| 7/8-string tuning presets (3.5) | 2 |
| Multi-scale (depends on `extended-range-bass.md` 1.1; not counted here) | 0 |
| Fretless glide (3.6) | 4 |
| Golpe and rasgueado (3.7) | 10 |
| Parameters, UI cards, fallback notice | 4 |
| Research files: lap steel, ukulele, mandolin, flamenco | 6 |
| Tests IN-01 to IN-14 | 5 |
| **Total** | **67 ED, about 13 weeks** |

Dependencies: `part-acoustics.md` (parts model); `extended-range-bass.md` 1.1
(multi-scale, for 7/8-string fanning); `slide-guitar.md` (lap steel, dobro);
`fingerstyle-attack.md` (nail contact, rest stroke); `bass-techniques.md`
(fretless bass); `fret-buzz.md` (confirms the fretless exemption).

## 10. Open questions

1. **Mandolin family.** Does a mandolin need its own family, or is `acoustic`
   plus a course flag enough? Recommend the flag, unless the bowl body changes
   the parts model.
2. **Lap steel body.** Resonator, solid or both? Recommend both, selected by
   the body card, since the resonator body already exists.
3. **Ukulele in the guitar picker.** Show it with the guitars, or in a separate
   "small instruments" group? Recommend a group, so the guitar list stays a
   guitar list.
4. **Golpe keyswitch.** Reuse `bodyTap` (17), or add 24? Recommend 24, so a
   slap body tap and a golpe are separate on the same instrument.
5. **Research sources.** Each D value needs a research file under
   `docs/research/`. Who supplies measured values for the uke body and the
   mandolin bowl?
