# TUNER AND TUNING REFERENCE SPEC

Fills a gap `engine.md` 3 (`TuningEngine`) and `gui-integration.md` 3.1 /
4.1 leave open: the plugin knows exactly what pitch every string should
be, and a player can set that pitch numerically, but there is no visual
tuner to check it against and no way to shift the whole instrument's
concert pitch away from A4 = 440 Hz. Orchestras, some studios and some
older recordings tune to something else, and a guitarist matching a
recording or a pit orchestra needs Luthier to follow.

Two features, one file because they share the same header control and the
same underlying pitch table:

1. A **built-in tuner utility**: a needle/strobe display per string,
   reachable in one interaction, that shows how far the modelled
   instrument's current tuning is from its target, and that can also
   listen to a real guitar plugged into the sidechain.
2. A **global tuning reference**: `tuning_reference_hz`, A4 from 432 to
   446 Hz, default 440, that shifts every note the engine produces without
   touching a single interval.

Test ID prefix: **TUNE-**.

## 0. Ground rules

1. **The reference shifts pitch, not intervals.** `tuning_reference_hz`
   multiplies every target frequency `TuningEngine` computes by
   `tuning_reference_hz / 440.0` (`engine.md` 3's
   `f_open` and the standard-tuning table). Temperament, capo,
   intonation error and detune are computed exactly as `engine.md` 3
   already specifies, then the whole result is scaled once. A chord stays
   in tune with itself at any reference.
2. **The reference is instrument state, not a performance event.** It
   lives beside tuning and capo in the preset (section 4), not in the
   `ranges` block (it is not a `PhysicalRange`; it is a single small
   range with no advanced side, like `mic_tof_mode`) and not in `uiState`.
3. **The tuner never changes pitch.** It is read-only against the engine's
   own state and, in Live mode, against an external signal. Turning it on
   costs CPU; turning it off costs nothing (the same contract
   `practice-tools.md` 0.1 sets for every practice tool).
4. **Honest about what it measures.** Luthier is a modelled instrument
   driven by MIDI: its own strings are always exactly the frequency
   `TuningEngine` computed, so the tuner's Reference mode is a target
   readout and a reference-tone player, not a detector, and says so.
   Detection only happens against a real external signal (Live mode,
   section 2).
5. **No new audio-path module for Reference mode.** It reads values
   `TuningEngine` already holds. Live mode adds one worker/audio-adjacent
   pitch tracker (section 2.3), gated identically to every other practice
   tool.

## 1. Tuning reference

### 1.1 Parameter

`tuning_reference_hz`: 432 to 446 Hz, default 440, unit Hz, non-physical
single range (no stock/advanced split - a concert-pitch choice is not a
"real vs. modelled" question the way a pot value is). Continuous, one
Hz-scale knob; common values (436, 440, 442, 443, 444, 415 baroque - out
of the 432-446 range, so not offered as a snap but the parameter itself is
declared with headroom in `advanced-ranges.md`'s sense would be wrong here,
since this is not a `PhysicalRange` family; the ship range is the
432-446 requirement and nothing wider) are marked as tick labels, not
separate choices.

### 1.2 Where it applies

`TuningEngine::computePitch` (`engine.md` 3) gains one multiplicative term
applied after temperament and detune, before the result reaches the
string engine:

```
f_target = f_temperament_and_detune * (tuning_reference_hz / 440.0)
```

This reaches every note class the standard-tuning table, alternate
tunings, custom tuning, capo, whammy and pitch bend already produce -
none of those are recomputed relative to 440 anywhere else, so scaling the
single output point is exact and touches no other module.

### 1.3 What does not move

- **Cent-based parameters** (detune, intonation slope, bend depth, vibrato
  depth, microtonal offsets) are unaffected: a cent is a ratio, and ratios
  do not change when the reference does.
- **Temperament tables** (`engine.md` 3's per-temperament ratio lookups)
  are unaffected for the same reason.
- **MIDI note numbers** are unaffected; only the Hz a given note number
  resolves to moves.
- **Tone-match, notation and MIDI export** read pitches after the
  reference is applied, so an exported performance is honest to what was
  heard; `notation-export.md`'s `PerformanceScore.meta` gains no new field
  from this spec (a MusicXML/Guitar Pro file has no concert-pitch tag to
  carry it, and the pitches themselves already reflect it).

## 2. Tuner utility

### 2.1 Reference mode (default, always available)

For each of the instrument's strings (however many the loaded `GuitarSpec`
has - 4 to 12), the tuner shows:

- The string's open target frequency at the current tuning, capo and
  reference (section 1.2's output for `fretPosition = 0`).
- A needle-style dial, centred at zero, reading the difference between
  that target and the string's current tuning-engine value (which is
  normally zero, since the engine always plays exactly its target - see
  Ground rule 4). The dial exists so the same widget serves Live mode
  without a second component.
- A **"Play reference"** button per string, and one for "Play all": the
  engine plays that string's open target pitch through the current rig for
  as long as the button is held (or 3 s, whichever is shorter), muted from
  the note-on velocity curve so it sounds like a clean reference tone
  rather than a pick attack. This is the tuner's actual daily use for a
  guitarist: matching a real instrument to Luthier's target pitch by ear,
  the way a tuning fork or a piano note is used.

### 2.2 Live mode

A toggle per tuner instance. When on:

- Reads from the input selected in a small source combo: **Main In**,
  **Sidechain**, or, when the routing layout has one, a specific
  **per-string input** (`routing-io.md` 2). This is the same input
  surface `tone-match.md` 4 already lists for Capture, so no new routing
  concept is added.
- A monophonic pitch tracker (autocorrelation with a parabolic-interpolated
  peak, 20 ms window, 10 ms hop; octave errors resolved against the
  nearest open-string target so a low string does not read an octave
  high) runs on a worker thread reading from the same kind of ring buffer
  `notation-export.md` 6.2 uses for capture - fixed-size, pre-allocated,
  audio thread writes, worker thread drains. No allocation or lock on the
  audio thread.
- The needle for the string nearest the detected pitch moves to show
  cents-off; a "closest string" indicator lights so the player does not
  have to guess which needle is theirs.
- Below a confidence threshold (weak/noisy signal, or silence) the needle
  parks at centre and greys out rather than jittering. Silence for
  2 seconds shows "Play a note."
- Live mode is off by default and does not run its tracker while the
  Live-mode toggle is off - zero cost, per Ground rule 3.

### 2.3 Accuracy and latency

- Reference mode has no detection latency: it reads a value, it does not
  measure one.
- Live mode's needle updates at 15 Hz (matching the noise-event strip
  rate in `ui-wiring.md` 10, an existing precedent for a modest,
  non-critical display rate) and settles to within 1 cent of a steady
  input's true pitch within 200 ms, per string range E2-E6 plus the
  extended-range strings a 7/8-string or bass guitar adds.
- The tracker only ever informs the display; it is never wired to
  `TuningEngine`, MIDI Learn, or any parameter. It cannot retune the
  instrument, by design (Ground rule 3) - closing that gap (auto-tune to
  a detected pitch) is future work, not this spec.

## 3. UI

### 3.1 Canonical location

**Advanced Column 1 GUITAR panel** (`gui-integration.md` 4.1) gains a
"Tuner" button beside the existing tuning fields. It opens the tuner as a
popover anchored to the button - the same interaction class as the Easy
Mode headstock popover (`gui-integration.md` 3.1), so both modes reach the
identical component:

```
+-- TUNER --------------------------------- [Live O] [x] --+
| Ref A4 [ 440 Hz  (o) ]        432 ---- 440 ---- 446       |
+------------------------------------------------------------+
|  E ⟨=====|=====⟩ -3c  [Play]                               |
|  A ⟨===========⟩  0c  [Play]                               |
|  D ⟨=======|===⟩ +5c  [Play]                                |
|  G ⟨=====|=====⟩ -1c  [Play]     [ Play all ]              |
|  B ⟨=====|=====⟩  0c  [Play]                                |
|  e ⟨=====|=====⟩  0c  [Play]                                |
+------------------------------------------------------------+
```

- **Easy Mode**: the headstock click popover (`gui-integration.md` 3.1)
  gains the same needle row and the Live toggle beneath the existing
  per-string tuning fields, so the tuner is reachable in the one
  interaction Easy Mode already promises for tuning
  (`gui-integration.md` 0.2).
- **Practice drawer**: `practice-tools.md` 9's tab strip
  (`METRO | LOOP | TRACK | SCALE | EAR | TAB | PROG | SESSION`) is fixed
  by that spec and is not reopened here. The drawer surfaces the tuner
  instead as a compact status chip next to the bpm readout when a tuner
  popover is open elsewhere in the window ("A 0¢" or "Live: E -3¢"), so a
  player mid-practice sees tuner state without switching views. Clicking
  the chip opens the Column 1 popover.
- **Reference control**: the `tuning_reference_hz` knob is the popover's
  own header field (shown above) and is also mirrored, per
  `gui-integration.md` 0.1's "one canonical control plus mirrors" rule, in
  `Options -> AUDIO` as a read/write field ("Concert pitch (A4): 440 Hz")
  for a player who wants to set it without opening the tuner.

### 3.2 Behaviour

- Opening the popover does not enable Live mode; Live mode is a separate
  toggle inside it (Ground rule 3 - a tuner that is merely open costs
  nothing beyond Reference mode's static readout).
- The popover closes on Escape or an outside click, per the Workshop /
  Options overlay convention (`gui-integration.md` 5, 6).
- A "Play reference" / "Play all" press is exempt from Live Mode's
  suppressed-tooltip rule (`gui-integration.md` 9) - the tuner is a
  utility panel, not a header transport control.
- The needle uses colour and shape together (`accessibility.md` 2): a
  centred green mark plus the numeric cents readout, not colour alone.

### 3.3 Empty and error states

| Condition | Message |
|---|---|
| Live mode on, no signal for 2 s | "Play a note." (needle parked at centre, greyed) |
| Live mode on, selected input has no signal path in the current bus layout | "No signal on \[input name\]. Choose a different input." |
| Reference pitch outside 432-446 in a loaded preset (hand-edited file) | Clamped to range on load; `error-recovery.md`'s standard out-of-range clamp handling, no separate banner |

## 4. Serialization, undo, accessibility

**State.**
- `tuning_reference_hz` is a normal automatable parameter (section 1.1),
  round-tripped in presets, host state, snapshots and morph endpoints
  exactly like any other continuous parameter.
- Live mode's on/off state and selected input live in `uiState`
  (`ui-wiring.md` 0.3) - it is a viewing choice, not instrument state, and
  never affects playback.
- The tuner popover's open/closed state is not persisted (like the
  Options overlay).

**Undo** (`action-and-undo.md`):
- Changing `tuning_reference_hz` is class 3.1 (parameter change),
  grouped within 200 ms like any knob.
- Opening/closing the tuner, toggling Live mode, and pressing "Play
  reference" are all UI-state or transient actions and are not undoable
  (`action-and-undo.md` 3.17 and 7, alongside tap tempo and panic).

**Accessibility.**
- Each needle exposes an accessible value: "E string, 3 cents flat" /
  "A string, in tune."
- The reference knob announces "Concert pitch, A4, 440 hertz."
- "Play reference" and "Play all" are focusable buttons with clear labels;
  they do not require Live mode or a mouse.
- Live mode's confidence-gated needle uses the same value/role contract as
  a meter (`accessibility.md` 1): "no signal detected" is itself an
  announced value, not a silently frozen widget.

## 5. Interactions with other specs

- **Tuning, capo, temperament** (`engine.md` 3, `gui-integration.md` 4.1):
  unaffected in mechanism; the reference is one more multiplicative term
  applied after them.
- **MIDI export / notation export**: pitches already reflect the
  reference (section 1.3); no new event class.
- **Tone Match, Capture**: Live mode's input selector reuses
  `tone-match.md` 4's input surface; it does not compete with an active
  Capture or Cab Match session for the same buffer (each opens its own
  tap; simultaneous use is allowed since neither writes to the audio
  path).
- **CPU quality**: Live mode's tracker is a worker-thread cost, not scaled
  by `cpu-quality-modes.md`'s levels (it is off by default and has no
  audio-path presence); it is exempt from that spec's table for the same
  reason practice tools generally are.
- **Randomize** (`randomize-and-ab.md`): never touches
  `tuning_reference_hz` - concert pitch is a setup choice, not a sound
  parameter a "surprise me" pass should move.
- **Advanced ranges**: not a `PhysicalRange`; does not appear in the
  `ranges` block or any family.

## 6. Tests

1. **TUNE-01** Reference scaling: for A4 in {432, 436, 440, 443, 446} and
   every standard and alternate tuning in `engine.md` 3, every open-string
   frequency equals the 440 Hz value times `A4/440` within 0.01 Hz.
2. **TUNE-02** Intervals preserved: at every reference value, the cents
   between any two strings' open pitches match their 440 Hz values within
   0.1 cent.
3. **TUNE-03** Temperament and capo unaffected in mechanism: switching
   temperament or moving the capo produces the same relative change at
   432 Hz as at 440 Hz.
4. **TUNE-04** Reference-mode readout: with no Live mode running, every
   needle reads exactly 0 cents against `TuningEngine`'s own target for
   the current tuning, capo and reference.
5. **TUNE-05** Play reference: pressing "Play" on a string renders a tone
   at that string's target frequency within 1 cent, through the current
   rig, for up to 3 seconds or release.
6. **TUNE-06** Live pitch tracking accuracy: synthetic sine and sawtooth
   inputs at each open-string frequency plus +/- 50 cents, across
   E2-E6 and a 7-string's low B, settle within 1 cent of true pitch within
   200 ms, with correct octave (no octave-error against the nearest
   open-string target).
7. **TUNE-07** Low-confidence handling: silence and white noise both park
   the needle at centre, grey it out, and never report a false pitch.
8. **TUNE-08** No audio-path effect: 1000 changes to `tuning_reference_hz`
   during playback via automation are click-free (per the smoothing rule
   already governing continuous parameters) and never move a note's pitch
   discontinuously by more than the parameter's own automation step.
9. **TUNE-09** Serialization: `tuning_reference_hz` round-trips in
   presets, host state, snapshots and morph endpoints; Live mode and input
   selection round-trip in `uiState` only.
10. **TUNE-10** Zero cost when closed: with the tuner popover closed and
    Live mode off, no pitch-tracker thread runs and CPU usage is
    unchanged from a build without this feature.
11. **TUNE-11** GUI reachability: the Options -> AUDIO mirror and the
    Column 1 / headstock popover controls both resolve to the same
    parameter and stay in sync within one editor tick of each other.
12. **TUNE-12** Accessibility: each needle's accessible value text matches
    the pattern in section 4; the confidence-gated "no signal" state is
    announced, not silent.
