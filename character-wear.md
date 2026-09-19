# CHARACTER AND WEAR SPEC

Extends spec.md's "String age" and "Humanize" panels. Adds physical
imperfections that real guitars have and physically-modelled synths usually
lack: dead spots, worn frets, loose tuners, aged electronics, uneven pickup
balance. This is what stops a modelled guitar from sounding identical from
note to note.

## 0. Ground rules

1. All character/wear behaviour is **deterministic per instance seed**. A
   given guitar's dead spots are the same every time until the user rerolls.
2. Character parameters do **not** modulate every sample. They influence
   per-note or per-region behaviour with smoothed handoff.
3. Character is **on by default at low intensity**. Zero character is a
   deliberate user choice, not the ship state.
4. All character adds together with humanize; humanize covers timing and
   pitch jitter, character covers acoustic and mechanical imperfection.

## 1. Per-instrument seed

- Each guitar preset carries a 64-bit `character_seed`.
- The seed drives all pseudo-random per-instance values below.
- User can reroll via "New Character" button; the new seed is saved into
  the preset.
- Two presets with the same guitar type but different seeds sound
  measurably different in the same way two physical guitars off the same
  assembly line do.

## 2. Dead spots

Every physical guitar has "dead spots": fret positions where a specific
string sustains noticeably less due to neck resonance interference.

Model:
- For each string, generate 0-3 dead spots at frets drawn from a beta
  distribution centered on frets 6-12 (empirically where they cluster on
  most necks).
- Each dead spot has a depth (0.1-0.7 sustain reduction) and a width
  (2-5 frets).
- When a note plays at a dead-spot fret, the string engine's feedback loop
  gain is reduced by `depth * spot_weight`, where `spot_weight` is a
  Gaussian centered on the spot's fret with the spot's width.
- Dead spots interact with the body model: the frequencies most attenuated
  are those closest to the body's air resonance, matching what physical
  neck-body coupling produces.

## 3. Fret wear

- Each fret has a wear value 0.0 (new) to 1.0 (fully worn).
- Wear is heavier at frets 1-5 and 12-17 on plain strings, matching real
  wear patterns.
- Worn frets produce:
  - Slightly reduced sustain (worn fret contact is imperfect).
  - Increased fret buzz probability at low action.
  - A subtle detune of a few cents on notes played there (worn fret alters
    effective string length).
- User can reset all fret wear ("Refret" button in Options -> Character).

## 4. Tuner drift

- Each string carries a slow-drift LFO in cents, amplitude 0-5 cents,
  period 20-90 seconds, per-string phase random.
- Amplitude scales with `tuner_looseness` parameter (0-100%, default 15%).
- Environmental stability parameter simulates temperature swings: drift
  amplitude can be enveloped over minutes.
- Rerolling character reseeds the drift LFOs.

## 5. Aged electronics

For electric guitars:
- Volume-pot value has a small linearity error curve, sampled from a
  measured aged-pot profile.
- Tone-pot capacitor value drifts +/- 5% from nominal per seed.
- Output-jack contact resistance simulates dodgy jacks: a small chance per
  minute of a brief intermittent drop, 20-100 ms in duration, entirely
  optional (off by default because it will surprise users).

For piezo bridges:
- Per-saddle output balance drifts +/- 2 dB per seed.

## 6. Pickup balance

- Per-string per-pickup output level drifts +/- 1.5 dB from nominal per
  seed.
- Pickup-height model: even a fresh guitar has slight height inconsistency
  across the pole pieces, contributing to string balance character.

## 7. Nut and saddle character

- Nut slot wear: a slight per-string dampening at fret 0 as slots widen.
  Alters open-string sustain.
- Saddle height variation across strings: +/- 0.2 mm per seed, affects
  intonation minutely.
- Bone vs synthetic nut selectable in Advanced, biases the string engine's
  high-frequency damping.

## 8. Body break-in

- The body's modal Qs increase slightly over an "age" parameter (0 = new,
  100 = decades old).
- Air resonance frequency drops 3-8% at full age.
- High-frequency damping decreases 5-10% (the body "opens up").
- Applied globally to the body engine's coefficients on preset load.

## 9. Environmental controls

Options -> Environment:
- Temperature (cold, room, warm): affects string tension slightly and
  drifts tuning.
- Humidity (dry, normal, humid): affects body Qs and top compliance
  (particularly acoustic).
- Elapsed session time: a slow drift that accumulates over minutes of
  simulated playing, until a virtual "retune".
- Retune button: snaps all tuner drift back to zero and starts drifting
  again from there.

## 10. UI

New tab in Advanced mode Column 4: `CHARACTER`.

Contents:
- Character seed field with "New Character" button.
- Dead spots: display map showing each string's dead spots on the
  fretboard, with per-spot depth and width sliders that the user can nudge.
- Fret wear map: fretboard with per-fret wear values, click-drag to alter.
- Tuner looseness slider.
- Aged electronics section: pot linearity strength, cap drift range, jack
  intermittent toggle.
- Body age slider.
- Environment section: temperature, humidity, session time, retune button.

Character panel also has an "All fresh" reset (zero everything) and an
"All old" preset (near-max everything), for A/B auditioning.

## 11. Interaction with humanize

Humanize is per-event randomness. Character is per-instance and per-
position. Both stack. A user who wants a machine-perfect sound sets both
to zero.

## 12. Tests

- Determinism: fixed seed produces byte-identical dead-spot list, fret
  wear map, drift LFO phases, and cap values across runs.
- Dead-spot audibility: for a note played at a dead spot vs. an adjacent
  fret, verify sustain (60 dB down time) is at least 10% shorter at the
  spot for spots at depth >= 0.5.
- Tuner drift: over 10 minutes at 5% looseness, verify RMS drift is within
  1 cent of expected.
- Environmental controls: temperature step of 20 K produces the expected
  tuning offset (measurable in the tuning engine's per-string frequency
  output).
- Zero-character: with all character disabled, output is bitwise identical
  to a synthetic no-wear render.
