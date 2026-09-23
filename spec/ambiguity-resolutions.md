# AMBIGUITY RESOLUTIONS

REVIEW.md flagged five items in `spec.md` and `engine.md`: feedback
simulation, freeze / infinite sustain, doubler, chord auto-fingering
rubric, and preset morph. The realism specs surfaced two more: strum
crossing-velocity source when a chord is voiced by the rhythm engine
rather than played live, and `.luthierguitar` compatibility with older
presets. This file resolves all seven with explicit, buildable specs.

Every resolution here supersedes any conflicting text in the original
specs.

## 1. Feedback simulation

**Chosen model**: physical mic-to-speaker loop, simplified to a
per-string feedback path.

Rationale: a heuristic overlay does not respond to pickup selection,
cabinet placement or amp gain in a way a guitarist recognises. A
physical loop does, at modest CPU.

### 1.1 Signal path

For each string, an additional feedback contribution is added at the
excitation point:
```
excitation_feedback[s] = k_couple(s) * H_cab_to_pickup(f) * amp_out
```

Where:
- `k_couple(s)` is the per-string acoustic coupling coefficient from cab
  mic to pickup, computed once per configuration change (guitar move,
  cab distance change, pickup swap in Workshop).
- `H_cab_to_pickup(f)` is a 2-pole peak filter centred at the
  fundamental of the currently ringing note on that string, gain-scaled
  by current amp output level.
- `amp_out` is the amp module output signal, delayed by one block to
  break the same-block loop.

Feeds the post-circuit path: the guitar volume knob attenuates the
feedback loop, matching real behaviour where rolling the guitar volume
down kills feedback.

### 1.2 Parameters

- `feedback_amount` (0-100%, default 0)
- `feedback_distance` (0-3.0 m, default 0.5 m)
- `feedback_angle` (-180 to +180 deg, default 0)
- `feedback_focus` (0-100%, default 60%)
- `feedback_octave_bias` (-2 to +2, default 0)

At `feedback_amount = 0` the path is bypassed and consumes zero CPU. At
`feedback_distance = 0` the loop truncates to a same-block peak filter.

### 1.3 UI

Advanced Column 3, SUSTAIN card, feedback row: five parameters and a
"Feedback" indicator LED that lights when the loop enters a resonant
state.

### 1.4 Tests

- Zero-amount bypass: at 0%, output bitwise identical to feedback module
  disabled.
- Stability: amount 100%, distance 0.5 m, focus 100% for 60 s, output
  remains bounded (never grows past the limiter's ceiling).
- Selectivity: with feedback engaged and a single note ringing, feedback
  contribution's spectrum peaks within 5 cents of the note's fundamental
  (or biased harmonic).
- Circuit interaction: guitar volume knob at 5 (per
  volume-knob-interaction.md) reduces the feedback loop level by the
  circuit's measured attenuation within 0.5 dB.

## 2. Freeze / infinite sustain

**Chosen model**: hybrid. Short freezes use a captured-loop overlay;
feedback-based sustain (E-Bow) uses the section 1 feedback path with
amount clamped to a safe range.

### 2.1 Freeze (captured-loop overlay)

- Grabs a 200-1000 ms window of the current summed string output.
- Crossfades that window into a granular sustain layer that plays
  indefinitely at a user-set level.
- New freeze replaces the layer.
- Envelope: attack 5-500 ms, release 20-2000 ms.

Parameters: `freeze_enable`, `freeze_capture_ms` (200-1000, default
400), `freeze_level` (-inf to 0 dB, default -6), `freeze_attack_ms`,
`freeze_release_ms`, `freeze_lp_cutoff`, `freeze_hp_cutoff`.

### 2.2 E-Bow

Uses the feedback path from section 1 with a fixed narrowband profile.

Parameters: `ebow_enable`, `ebow_string_mask` (default any string with
a held note), `ebow_intensity` (0-100%, default 50%, maps to
`feedback_amount`), `ebow_harmonic` (fundamental, 2nd, 3rd, 4th, 5th).

### 2.3 UI

Column 3, SUSTAIN card, two rows (Freeze, E-Bow), each with enable and
its parameters.

### 2.4 Tests

- Freeze layer: capture then hold 60 s, RMS varies less than 0.5 dB.
- E-Bow: single held note reaches steady state within 500 ms at
  intensity 50%, decays to inaudibility within 200 ms of disable.

## 3. Doubler

**Chosen defaults**: match classic ADT / hardware doubler.

- `doubler_enable` (default off)
- `doubler_delay_ms` (5-40 ms, default 22)
- `doubler_pitch_cents` (-25 to +25, default -8)
- `doubler_pan` (-1 to +1, default -0.7 first voice, +0.7 second when
  stereo)
- `doubler_width` (mono / stereo; default stereo)
- `doubler_mix` (0-100%, default 40%)
- `doubler_hp_cutoff` (20-500 Hz, default 100)
- `doubler_lp_cutoff` (2k-20k Hz, default 8k)

Signal path: post-amp, pre-cab (so cabinet colouration is consistent
across both voices).

### 3.1 UI

Post-effects rack pedal, always available.

### 3.2 Tests

- Mix 0: bypass null within -80 dBFS.
- Mix 100 with delay 22: cross-correlation confirms delayed copy at
  expected offset.

## 4. Chord auto-fingering rubric

Extends rhythm-engine.md 3 with explicit scoring weights.

### 4.1 Constraint set (all must pass)

1. Every fret in `[0, guitar.max_fret]`.
2. Non-muted-string fret span within `hand_span_frets`.
3. Root or bass on lowest sounding non-muted string.
4. No adjacent non-muted strings inverted more than 4 semitones.
5. Barre (if used): >= 3 strings on the barre fret.
6. All notes physically reachable from the barre if barred.

### 4.2 Score

```
score = 0
score += open_bonus            * count(open_strings_used)            // 3
score += style_bias[style]                                            // 4.3
score += transition_bonus                                             // 4.4
score -= abs(barre_fret - hand_position_hint) * hand_move_penalty     // 0.4
score -= barre_penalty          if barre                              // 2
score -= mute_penalty           * count(muted_strings)                // 4
score -= dup_note_penalty       * count(duplicate_pitches_across_octaves) // 1
score -= extension_dropped_penalty * count(dropped_extensions)        // 3
```

### 4.3 Style bias

- `Open`: +6 open string, +3 root on string 5 / 6, -2 barre.
- `Barre`: +5 barre used, +2 root on string 6.
- `Triad`: +6 three notes on top three strings, -3 otherwise.
- `Shell`: +7 root, 3rd, 7th only, -5 otherwise.
- `Drop2` / `Drop3`: +5 if voicing matches canonical drop pattern.
- `Power`: +8 root + 5th (+ octave root) only, -6 otherwise.
- `Rootless`: +6 if root omitted, -6 if present.
- `Wide`: +4 if all N strings sound one note each.
- `Bass` (bass-family instruments): +8 root only or root + fifth,
  +2 walking approach note, -6 anything wider than an octave in the
  voicing.

### 4.4 Transition bonus

When previous chord's voicing is known:
- +2 per common note.
- +1 per common finger position (approximated by fret proximity).
- -2 per position jump > 5 frets on any finger.

### 4.5 Capo handling

Capo raises effective minimum fret to `capo_fret`. Open strings are the
capo'd notes. Partial capos use the string mask from the capo part in
the Workshop.

### 4.6 Determinism

Given identical inputs, voicer returns the same top-ranked voicing.
Ties broken by voicing string count descending, then lowest total fret
sum ascending.

### 4.7 Tests

- All 84 chord templates in every ship key, every style: valid voicing
  or explicit "unplayable".
- Transition: I-IV-V-I in C on standard tuning open style, hand travels
  no more than 3 frets between chords.
- Determinism: fixed inputs produce byte-identical voicing across runs.
- Bass mode: on a J-bass tuned BEAD, a "C7" symbol produces C on E-2nd
  fret only in the Root style, root + fifth in the Root-Fifth style.

## 5. Preset morph

Preset-to-preset morphing (not snapshot morphing).

### 5.1 Scope

- Continuous parameters interpolate between two presets.
- Discrete parameters (amp model, cab model, guitar reference, wiring
  part) do not; they hard-switch at morph position 0.5.
- Structural state (mod matrix routes, patterns, snapshots, `ranges`
  block) does not morph; target's structural state applies at 0.5.
- `GuitarSpec` reference does not morph; morphing between two different
  guitars swaps the spec at 0.5 with the standard crossfade.

### 5.2 UI

Preset browser gains a "Morph" toggle. When on:
- Two slots A and B visible.
- Morph slider below.
- Loading a preset while in morph mode loads into the current slot.
- Master parameters follow the slider live.

Morph slider is automatable (`preset_morph_position`).

### 5.3 Tests

- At morph 0.0, output identical to preset A alone.
- At morph 1.0, output identical to preset B alone.
- At morph 0.5, discrete parameters have switched, continuous parameters
  are the arithmetic midpoint within float tolerance.
- Automation 0->1 over 4 s in a host: click-free, glitch-free
  transition.

## 6. Strum crossing velocity when the rhythm engine voices a chord

`strum-dynamics.md` says crossing velocity can come from MIDI note
spread (mode b), which works when the user plays the chord live. When
the rhythm engine synthesises a chord from a held single-note or from a
progression, there is no burst of MIDI notes with spread; the strum
engine needs a source.

**Chosen resolution**: rhythm engine sets `crossing_velocity_sps` from
the pattern's `crossing_sps` field (rhythm-engine.md 6). Genre kits
carry a default; a step can override. If the pattern does not name a
crossing_sps, the plugin-global default from
`strum-dynamics.md` 4 applies (200 sps guitar, 100 sps bass).

For chord-progression-looper input (chord symbols typed by the user),
the pattern's `crossing_sps` applies.

For MPE / hex-pickup routing (each string arrives on its own channel),
strum synthesis does not apply and per-string events are passed through
in their real timing.

### 6.1 Tests

- Rhythm engine voicing a chord from a held root produces per-string
  events spaced per the active pattern's `crossing_sps` within 1 sample.
- Chord progression looper input at 120 bpm with a pattern
  `crossing_sps = 220` produces the documented 22.7 ms spread.

## 7. `.luthierguitar` compatibility with older presets

Presets pre-dating the parts model (before M49) referenced hard-coded
guitars by index / name. The Workshop parts model replaces them with
`.luthierguitar` files.

**Chosen resolution**: on preset load, if the preset references a
hard-coded guitar name that no longer exists as a live enum, the plugin
consults a migration table that maps every old name to a shipped
`.luthierguitar` file. The migration table lives in
`Resources/Guitars/migration.json` and is versioned.

If the migration table cannot resolve the reference, the plugin loads
the factory default guitar and posts a warning banner: "Guitar 'X' not
found, loaded closest factory match. Open Workshop to save your
customization as a guitar." This is the notification from
gui-integration.md 15.

The preset's own parameter values still apply on top of the loaded
guitar's spec, so tone controls and effects are preserved even if the
guitar identity is not exact.

### 7.1 Tests

- Every hard-coded guitar name in every factory preset from the pre-M49
  build resolves via the migration table.
- A synthetic preset with an unknown guitar name loads without crash,
  triggers the banner, and preserves parameter values.

## 8. Cross-references

- Feedback / freeze / E-Bow parameters are legal modulation destinations
  per modulation-matrix.md 2.
- Doubler is a pedal; MIDI Learn and mod matrix apply as with any pedal.
- Chord voicer changes affect notation output (notation-export.md 4).
- Preset morph does not interact with snapshot bank; a snapshot recall
  during a preset morph cancels the morph and settles on the recalled
  state.
- Circuit interaction from section 1 (feedback attenuated by guitar
  volume) is testable via routing-io.md's Aux 1 pre / post-circuit
  toggle.
- The migration table from section 7 is part of the installer content
  update path (installer.md 11).
