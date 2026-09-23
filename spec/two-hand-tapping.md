# TWO-HAND TAPPING SPEC

Right hand taps a fret above the left-hand fretting position; the
string sounds without picking. Extends to legato runs, tapping
arpeggios, and eight-finger tapping (a la Stanley Jordan).

## 0. Ground rules

1. **A tap is a fret event, not a pluck.** The right-hand finger
   presses the string against the fret sharply; the string
   segment between the tap fret and the bridge sounds.
2. **Tap-off releases the string.** Lifting the tap without picking
   returns the sounding length to the left-hand fret position, so
   the note pitches down to the fretted note.
3. **Left-hand hammer-ons and pull-offs are the same physics.**
   Both hands can hammer and pull; there's no engine distinction.
4. **User controls per-tap intent.** Fret and string are specified
   per tap; the model does the physics.

## 1. Tap gesture

```
struct TapGesture {
    int string_index;
    int fret;             // tap fret
    double strength_0_to_1;
    TapHand hand;         // left or right (for scheduling and UI)
    bool pull_off_after;  // if true, tap lifts to reveal a lower fret
    int pull_off_target_fret;
    double duration_ms;   // how long the tap holds before lifting
};
```

## 2. Physical model

- **Tap-on**: apply a strong damping+contact event at the tap fret,
  with an impulsive excitation from the finger's kinetic energy.
  Sounding length shrinks to tap-fret-to-bridge.
- **Tap-hold**: the tap remains as a movable capo. String rings
  freely between tap fret and bridge.
- **Tap-off (pull-off)**: release the tap while the left-hand
  fretting position stays. If `pull_off_after` is true, the finger
  laterally flicks the string as it releases, adding a small
  excitation (a real pull-off has a note-on quality); otherwise the
  string just returns to fretted-note pitch with a small transient.

Multi-finger tapping = multiple concurrent tap gestures on the same
or different strings.

## 3. User controls

Techniques tab, Tapping sub-tab:

- **Tap trigger source**: MIDI channel 2 (default; right-hand notes
  come in on a second channel per MPE conventions), keyswitch, or
  the on-screen fretboard tap layer.
- **Tap strength curve**: velocity-to-tap-strength mapping. Default
  linear; user can shape.
- **Auto pull-off**: when a tap releases with a fretted note still
  held, trigger pull-off automatically. Default on.
- **Left-hand hammer-on threshold**: minimum velocity for a legato
  note (played into left-hand channel with a small time gap after
  the previous note) to be treated as hammer-on rather than pluck.
  Default 40.
- **Tap release lateral flick**: 0-1, how strong the pull-off's
  lateral excitation is. Default 0.5.
- **Tap duration default**: fallback duration when duration not
  specified. Default 200 ms.
- **Multi-finger tap max concurrent**: cap on simultaneous taps
  per string. Default 2 (typical two-hand). Advanced range up to 8
  for extreme players.
- **Fret snap**: quantise tap positions to fret centres. Default on.
  Off allows microtonal tap positions.

## 4. Engine integration

New micro-module `TapEngine`. Consumes gestures from
`TechniqueEngine`, produces tap-on and tap-off events into
`StringEngine`'s boundary-condition input.

Interface:
```cpp
class TapEngine {
    void trigger(const TapGesture& g);
    void release(int string, int fret);   // manual tap-off
    void processBlock(int n);
    void reset();
};
```

Insertion: after `TechniqueEngine`, alongside `ScrapeEngine`,
`SlapEngine`, `SlideEngine`, before `StringEngine`.

StringEngine's excitation function already accepts damp-position
inputs (per `harmonic-realism.md` 2); tapping reuses this
mechanism. No StringEngine internal changes needed.

## 5. Left-hand hammer-ons and pull-offs

Left-hand-only legato (no right-hand involvement) is triggered by
consecutive notes on the same string within a short time window and
below a velocity threshold. `TechniqueEngine` promotes them to
hammer-on / pull-off events that route through `TapEngine`.

Sit-detection rule: a note-on on a string that's already ringing,
within 150 ms of the previous note-on, with velocity below the
hammer-on threshold, is a hammer-on. A note-off on a ringing string
that immediately reveals a lower fretted position is a pull-off.

## 6. GUI location

- **Techniques tab, Tapping sub-tab**: all controls.
- **Easy Mode Playing strip**: Tap pill (arms next note as tap
  event on the played fret).
- **Advanced Mode Col 3 CHARACTER Right Hand group**: gains a
  "Tapping" section with strength curve, lateral flick, and
  auto-pull-off toggles.
- **Fretboard illustration**: taps render as small square markers
  (distinct from played-note dots) at tap positions; released taps
  fade over 100 ms.

## 7. Cascade compatibility

- Tapping + muting: fully compatible. Muted tap = percussive click.
- Tapping + microtonal bends: fully compatible. Tap-and-bend is a
  classic technique.
- Tapping + slide: incompatible on the contacted strings (bar
  controls the pitch).
- Tapping + slap: rare, valid in alternation but not on same string
  in same window.
- Tapping + scraping: incompatible on the same string in the same
  window.

See `technique-cascade.md` for full matrix.

## 8. Presets

- **Standard Two-Hand Tap**: MIDI ch 2 right-hand tap, auto
  pull-off on, lateral flick 0.5, fret snap on.
- **Legato Runs**: single channel, hammer-on threshold 60, lateral
  flick 0.3.
- **Eight-Finger Tap**: multi-finger tap max concurrent 8, per
  hand assignment via MPE channels.
- **Microtonal Tap**: fret snap off, tap positions between frets.
- **Percussive Tap**: mute type "extreme", tap strength full.

## 9. MIDI export

Tap events export as SysEx in the Luthier profile with the tap
hand, strength, and pull-off flag preserved. Generic profile
encodes as regular note-ons on channel 2 with velocity mapping.

## 10. Tests

- Tap at fret 12 with A fretted at fret 5: pitch is fret-12 note
  during tap-hold, returns to fret-5 A on release, with a small
  transient at release.
- Pull-off with lateral flick 1.0: released note has audible
  attack transient at release moment.
- Auto pull-off off: released note has no attack transient.
- Hammer-on: two rapid notes on same string, second below threshold:
  second note plays without a picking transient.
- Multi-finger tap: two concurrent taps on the same string
  behave as two capos in series (higher tap wins pitch).
- Fret snap off: tap at fret 12.3 sounds a slightly sharp F.
- CPU: idle < 0.05%; active tap-heavy passage < 0.8%.
- Preset save / restore round-trips every added field.
