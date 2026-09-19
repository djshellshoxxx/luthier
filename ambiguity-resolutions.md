# AMBIGUITY RESOLUTIONS

REVIEW.md flagged five underspecified items in `spec.md` and `engine.md`:
feedback simulation, freeze / infinite sustain, doubler, chord auto-
fingering rubric, and preset morph. This file resolves each with an
explicit, buildable spec.

Every resolution here supersedes any conflicting text in the original
specs.

## 1. Feedback simulation

**Chosen model**: physical mic-to-speaker loop, simplified to a per-string
feedback path.

Rationale: a heuristic overlay ("increase sustain when gain is high")
does not respond to pickup selection, cabinet placement or amp gain in a
way that a guitarist recognises. A physical loop does, at modest CPU.

### 1.1 Signal path

For each string, an additional feedback contribution is added at the
excitation point:
```
excitation_feedback[s] = k_couple(s) * H_cab_to_pickup(f) * amp_out
```

Where:
- `k_couple(s)` is the per-string acoustic coupling coefficient from the
  cabinet mic position to the pickup position, computed once per
  configuration change (guitar move, cab distance change).
- `H_cab_to_pickup(f)` is a simple 2-pole peak filter centred at the
  fundamental of the currently ringing note on that string, gain-scaled
  by the current amp output level.
- `amp_out` is the amp module's output signal, delayed by one block to
  break the same-block feedback loop.

### 1.2 Parameters (all automatable)

- `feedback_amount` (0-100%, default 0): master coupling gain.
- `feedback_distance` (0-3.0 m, default 0.5 m): distance from cab to
  virtual guitar; determines coupling delay and level.
- `feedback_angle` (-180 to +180 deg, default 0): guitar orientation
  relative to cab; drives per-string coupling asymmetry.
- `feedback_focus` (0-100%, default 60%): narrowness of the resonance
  peak (Q of the feedback filter).
- `feedback_octave_bias` (-2 to +2, default 0): biases feedback to
  higher or lower harmonics.

At `feedback_amount = 0` the feedback path is bypassed and consumes zero
CPU. At `feedback_distance = 0` the loop is truncated to a same-block
peak filter (still one-block delayed for stability).

### 1.3 UI

Advanced Column 3, appended below AMP. Compact card with the five
parameters and a "Feedback" indicator LED that lights when the loop
enters a resonant state.

### 1.4 Tests

- Zero-amount bypass: at 0%, output is bitwise identical to feedback
  module disabled.
- Stability: at amount 100%, distance 0.5 m, focus 100% for 60 s of
  sustain, output remains bounded (never grows past 0 dBFS peak beyond
  the limiter's ceiling).
- Selectivity: with feedback engaged and a single note ringing, the
  feedback contribution's spectrum peaks within 5 cents of the note's
  fundamental (or the biased harmonic).

## 2. Freeze / infinite sustain

**Chosen model**: hybrid. Short freezes use a captured-loop overlay for
zero-cost sustain; feedback-based sustain (E-Bow) uses the section 1
feedback path with amount clamped to a safe range.

Rationale: freeze is really two features. "Freeze this chord" wants a
frozen spectrum, which loop capture handles perfectly. "Sing on this
note" wants a growing, harmonic-swelling sustain, which is what the
feedback path does.

### 2.1 Freeze (captured-loop overlay)

- Grabs a 200-1000 ms window of the current summed string output.
- Crossfades that window into a granular sustain layer that plays
  indefinitely at a user-set level.
- The layer plays independently of new notes; when the user triggers
  freeze again, the layer is replaced.
- Envelope: attack 5-500 ms, release 20-2000 ms.

Parameters:
- `freeze_enable` (bool).
- `freeze_capture_ms` (200-1000, default 400).
- `freeze_level` (-inf to 0 dB, default -6).
- `freeze_attack_ms`, `freeze_release_ms`.
- `freeze_lp_cutoff`, `freeze_hp_cutoff` for tonal shaping of the layer.

### 2.2 E-Bow (feedback-based sustain)

- Uses the feedback path from section 1 with a fixed narrowband profile.
- Parameters:
- `ebow_enable` (bool).
- `ebow_string_mask` (which strings the E-Bow drives; default: any
  string with a held note).
- `ebow_intensity` (0-100%, default 50%, maps to feedback_amount).
- `ebow_harmonic` (fundamental, 2nd, 3rd, 4th, 5th; default fundamental).

### 2.3 UI

Column 3, appended below AMP, dedicated "SUSTAIN" card with two rows
(Freeze, E-Bow), each with an enable button and its parameters.

### 2.4 Tests

- Freeze layer: capture then hold for 60 s, verify layer's RMS varies
  less than 0.5 dB over the hold (stable).
- E-Bow: for a single held note, verify E-Bow reaches steady state within
  500 ms at intensity 50%, and decays to inaudibility within 200 ms of
  disable.

## 3. Doubler

**Chosen defaults**: match the classic ADT / hardware doubler pedal.

Parameters:
- `doubler_enable` (bool, default off).
- `doubler_delay_ms` (5-40 ms, default 22 ms): the second voice's delay.
- `doubler_pitch_cents` (-25 to +25, default -8): the second voice's
  pitch offset.
- `doubler_pan` (-1 to +1, default -0.7 for double; +0.7 for the second
  voice if width is set to stereo).
- `doubler_width` (mono, stereo; default stereo).
- `doubler_mix` (0-100%, default 40%).
- `doubler_hp_cutoff` (20-500 Hz, default 100 Hz): high-pass on the
  double voice to keep the bottom tight.
- `doubler_lp_cutoff` (2 k-20 k Hz, default 8 k): low-pass on the double
  voice.

Signal path: doubler sits post-amp, pre-cab (so cabinet colouration is
consistent across both voices).

### 3.1 UI

Post-effects rack pedal, always available in the pedal library. Fits the
standard pedal card.

### 3.2 Tests

- With mix 0, output nulls against bypass at -80 dBFS.
- With mix 100 and delay 22, verify a delayed copy exists at the expected
  offset via cross-correlation.

## 4. Chord auto-fingering rubric

Extends rhythm-engine.md section 3. Adds the explicit scoring weights
and rules that were only sketched there.

### 4.1 Constraint set (hard, must all pass)

1. Every fret in `[0, guitar.max_fret]`.
2. Non-muted-string fret span within `hand_span_frets`.
3. Root or bass on the lowest sounding non-muted string.
4. No adjacent non-muted strings inverted more than 4 semitones.
5. Barre (if used): >= 3 strings on barre fret.
6. All notes physically reachable from the barre if barred (no fret
   above `barre_fret + hand_span_frets`).

### 4.2 Score (higher is better)

```
score = 0
score += open_bonus            * count(open_strings_used)            // default 3
score += style_bias[style]                                            // section 4.3
score += transition_bonus       (see 4.4)
score -= abs(barre_fret - hand_position_hint) * hand_move_penalty     // default 0.4
score -= barre_penalty          if barre                              // default 2
score -= mute_penalty           * count(muted_strings)                // default 4
score -= dup_note_penalty       * count(duplicate_pitches_across_octaves) // default 1
score -= extension_dropped_penalty * count(dropped_extensions)        // default 3
```

### 4.3 Style bias (default numbers, all tunable)

- `Open`: +6 if any open string, +3 if root on string 5/6, -2 if barre.
- `Barre`: +5 if barre used, 0 otherwise; +2 if root on string 6.
- `Triad`: +6 if 3 notes on top three strings, -3 otherwise.
- `Shell`: +7 if root, 3rd, 7th only sounding; -5 otherwise.
- `Drop2` / `Drop3`: +5 if voicing matches canonical drop pattern.
- `Power`: +8 if only root + 5th (+ octave root) sound; -6 otherwise.
- `Rootless`: +6 if root omitted; -6 if root present.
- `Wide`: +4 if all N strings sound one note each.

### 4.4 Transition bonus

When the previous chord's voicing is known:
- +2 per common note (same string, same fret between the two voicings).
- +1 per common finger position (approximated by fret proximity).
- -2 per position jump > 5 frets on any finger.

Transition bonus overrides the base position score for the first chord
after another; the very first chord in a session uses `hand_position_hint`
directly.

### 4.5 Capo handling

- Capo raises the effective minimum fret to `capo_fret`.
- Open strings are the capo'd notes, not the raw open notes.
- Score penalises voicings that require notes below `capo_fret`.

### 4.6 Determinism

Given identical inputs (chord symbol, guitar spec, style, density,
hand_position_hint, previous voicing), the voicer always returns the same
top-ranked voicing. Ties broken by voicing string count descending, then
by lowest total fret sum ascending.

### 4.7 Tests

- All 84 chord templates in every ship key, every style: verify voicer
  returns a valid voicing or explicit "unplayable".
- Transition: for a common progression (I-IV-V-I in C on standard tuning
  open style), verify hand travels no more than 3 frets between chords.
- Determinism: fixed inputs produce byte-identical voicing across runs.

## 5. Preset morph

Preset-to-preset morphing (not snapshot morphing, which
live-performance.md already covers).

### 5.1 Scope

- Continuous parameters interpolate between the two presets.
- Discrete parameters (amp model, cab model, guitar) do not; they
  hard-switch at morph position 0.5.
- Structural state (mod matrix routes, patterns, snapshots) does not
  morph; the target's structural state applies at 0.5.

### 5.2 UI

Preset browser gains a "Morph" mode toggle. When on:
- Preset browser shows two slots: A and B.
- A morph slider appears below.
- Loading a preset while in morph mode loads it into the currently
  selected slot.
- Master parameters follow the morph slider live.

Morph slider is automatable (parameter ID `preset_morph_position`), so
users can automate a preset-to-preset transition in a DAW.

### 5.3 Behaviour on structural change at 0.5

Uses the same crossfade path as snapshot recall for the transition
across 0.5, so the discrete jump is inaudible for typical values.

### 5.4 Tests

- At morph 0.0, output is identical to preset A alone.
- At morph 1.0, output is identical to preset B alone.
- At morph 0.5, discrete parameters have switched, continuous parameters
  are the arithmetic midpoint (within float tolerance).
- Automation: automating morph 0->1 over 4 s in a host produces a click-
  free, glitch-free transition.

## 6. Cross-references

- Feedback / freeze / E-Bow parameters are legal modulation destinations
  per modulation-matrix.md section 2.
- Doubler is a pedal per the effects rack; MIDI Learn and mod matrix
  apply as with any pedal.
- Chord voicer changes affect notation output (notation-export.md
  section 4 chord extraction).
- Preset morph does not interact with snapshot bank; a snapshot recall
  during a preset morph cancels the morph and settles on the recalled
  state.
