# MUTING AS RHYTHM SPEC

Muting is not just silence. Palm mute grid on metal chugs, chuka-
chuka funk 16ths on muted strings, reggae skank on off-beats,
percussive nail-flesh muting in classical: muting is a rhythmic
instrument in itself. `string-interaction.md` covered the physics
of palm mute spread and adjacent damping. This file adds:
- Muting patterns and grid triggers.
- User controls for mute style and rhythm mapping.
- Integration with the rhythm engine.

## 0. Ground rules

1. **Muting is a rhythmic voice.** It has patterns, grids, dynamics,
   and swing, just like strums or fingerpicks.
2. **Muting produces sound.** A muted string struck is not silent;
   it produces a percussive thump characterised by pick / hand
   attack noise plus the string's very-short-decay damped ring.
3. **Mute state can flip mid-groove.** Palm-mute on beat 1, open on
   beat 2 + and 3, palm-mute on 4: this is a common groove shape.
4. **Multiple mute types can coexist per groove.** A pattern can
   specify palm mute on some beats, ghost (fully damped percussive)
   on others, open on others.

## 1. Mute types (user-selectable per grid step)

- **Open**: no muting.
- **Palm Mute Light**: palm resting lightly, mostly pitched but
  shorter decay. T60 drops to ~150 ms.
- **Palm Mute Heavy**: palm pressed firmly, tight chug. T60 ~50 ms.
- **Palm Mute Extreme**: palm pressed against strings above bridge,
  minimal pitch, thump-only. T60 ~20 ms.
- **Ghost**: fretting fingers touch strings but do not press. No
  pitch; percussive thump only.
- **Chuka**: hand chops across strings at strum time, damping
  everything on contact. Combined with strum motion.
- **Fret Mute**: single fretted note released to a light touch
  right after the strike, producing a short pitched sound then
  silence (classical / jazz staccato).

Each type has a position and pressure default; the user can
override per pattern step.

## 2. Grid mapping

Muting integrates with the RhythmEngine's step grid. Each step's
existing fields (dynamics, technique) gain a `mute_type` field.

New pattern format extension:
```json
{
  "steps": [
    { "beat": 0.0, "dynamic": 0.9, "mute_type": "palm_mute_heavy" },
    { "beat": 0.5, "dynamic": 0.4, "mute_type": "ghost" },
    { "beat": 1.0, "dynamic": 0.7, "mute_type": "open" },
    { "beat": 1.5, "dynamic": 0.9, "mute_type": "palm_mute_heavy" }
  ]
}
```

Live mode (no rhythm engine active): a "Mute Grid" overlay in
Advanced Mode Col 4 shows a 16-step grid the user can paint on to
define the current muting groove. The grid syncs to host tempo.

## 3. User controls

Techniques tab, Muting sub-tab:

- **Master mute mode**: overrides all pattern mute types with this
  one (useful for consistent metal chug).
- **Mute type per beat** (16-step grid): paint each step.
- **Palm position**: where the palm rests (mm from bridge). Default
  35 mm.
- **Palm pressure**: 0-1. Default 0.5.
- **Fretting-hand mute style**: rock spread vs classical fingertip
  (from `string-interaction.md`).
- **Chuka source**: what triggers a chuka event. Default: any
  strum with dynamics < 0.3.
- **Random mute humanise** (0-1): probability that a step's mute
  type shifts one step (open <-> palm mute) for a less mechanical
  feel. Default 0.
- **Ghost note velocity range**: ghost notes' relative velocity
  vs open notes (default 0.4).

## 4. Engine integration

No new engine module. Two changes:

- **RhythmEngine**: gain a `MuteGrid` field on each pattern and read
  `mute_type` per step. Passes the mute type as a parameter with
  each note-on it generates.
- **StringEngine**: read the mute type from incoming note-on events
  and apply to that note's initial damping and post-strike release.

Both changes are additive; existing patterns without `mute_type`
default to "open" for every step. No migration required.

Live mute grid overlay lives in the RhythmEngine as a runtime-only
pattern that the user paints in real time.

## 5. Cascade compatibility

Muting stacks with almost everything:
- Slap + palm mute: standard funk. Palm rests over the strings
  behind the slap zone.
- Muting + scraping: dulled scrape, thumpy.
- Muting + slide: muted slide gives a soft moaning quality.
- Muting + tapping: percussive taps on muted strings.
- Muting + microtonal bends: fully compatible.

Nothing conflicts with muting; it's an underlay for everything.

## 6. Presets

- **Metal Chug 16ths**: palm mute heavy on every step, dynamics
  full.
- **Funk Chuka**: chuka on every off-beat, open on beats, ghost on
  ands.
- **Reggae Skank**: open on 2 and 4, palm mute light everywhere
  else.
- **Country Boom-Chick**: alternating palm mute (boom on bass) and
  chuka (chick on treble).
- **Metal Gallop**: palm mute heavy in the classic gallop rhythm
  (dotted-8 + 16 + 8).
- **Classical Staccato**: fret mute on every note, no palm.

## 7. GUI location

- **Techniques tab, Muting sub-tab**: the full 16-step editor and
  all controls.
- **Easy Mode Playing strip**: a compact Mute button (4-way:
  Off, Light, Heavy, Extreme) applying master mute mode.
- **Advanced Mode Col 4 RHYTHM tab**: existing pattern editor
  gains a "Mute row" showing mute_type per step.

## 8. MIDI export

Mute types export as SysEx meta events in the Luthier profile per
`midi-export.md` (existing event class `NOTE` gains a `mute_type`
field). Generic profile encodes mute type as text meta events at
each note.

## 9. Tests

- Palm mute heavy on low E: T60 measures 40-60 ms.
- Ghost note: output has < -30 dB pitched content.
- Chuka: strum with dynamics 0.2 produces percussive event with
  no sustained pitch.
- Grid painting: painting palm-mute on step N in the live overlay
  applies within one bar.
- Random humanise 0.5: over 100 loops, ~50% of eligible steps shift.
- Preset save / restore round-trips the mute grid.
- Existing patterns without mute_type play identically to before.
- Cascade with slap: slap event carries its own mute_type; grid
  override propagates correctly.
